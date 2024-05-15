#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <string>
#include <iostream>
#include <sstream>
#include <fstream>

using namespace std;

// Settings
// Functions to allow us to setup OpenGL to be run as a library
unsigned int scrWidth = 800;
unsigned int scrHeight = 600;
const char* title = "MoEngine - Pong";

// Drawing Variables
const float paddleSpeed = 300.0f;
const float paddleHeight = 100.0f;
const float halfPaddleHeight = paddleHeight / 2.0f;
const float paddleWidth = 10.0f;
const float halfPaddleWidth = paddleWidth / 2.0f;
const float pongDiameter = 16.0f;
const float pongRadius = pongDiameter / 2.0f;
const float offset = pongRadius;
const float paddleBoundary = halfPaddleHeight + offset;

// 2D Vector Struct
// In memory, these are stored consecutively
struct vec2d {
	float x;
	float y;
};

// Public offsets to change with screen size changes
vec2d paddleOffsets[2];
vec2d pongOffset;

// Public array for moving the pong ball and paddles
float paddleVelocity[2]; // we only care about the y axis
vec2d pongVelocityInitial = { 200.0f, 200.0f };
vec2d pongVelocity = { 200.0f, 200.0f };

// UI Values
unsigned int leftScore = 0;
unsigned int rightScore = 0;

// Pause game vvariable
bool pauseMe = false;
bool pausePressed = false;
float gameSpeed = 1.0f;

// I know this isn't the best but I just wanted to simplify it for my brain so I can
// Acces this in some callbacks (such as reshaping the orth projection)
GLuint shaderProgram;

// Initialize the GLFW
void initGLFW(unsigned int versionMajor, unsigned int versionMinor) {

	glfwInit();

	// Window Params
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, versionMajor);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, versionMinor);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

	// Mac stuff thx u apple
#ifdef __APPLE__
	glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif
}

// Create the window
void createWindow(GLFWwindow*& window,
	const char* title, unsigned int width, unsigned int height,
	GLFWframebuffersizefun framebufferSizeCallback) {

	window = glfwCreateWindow(width, height, title, NULL, NULL);
	if (!window) {
		return;
	}

	glfwMakeContextCurrent(window);
	glfwSetFramebufferSizeCallback(window, framebufferSizeCallback);
}

// We want GLAD to help us link all the OpenGL functions to this program
bool loadGlad() {

	return gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);

}

// Shader Funcs

// Read the file
string readFile(const char* filename) {

	ifstream file;
	stringstream buf;

	string returnMe = "";

	file.open(filename);

	if (file.is_open()) {
		buf << file.rdbuf();
		returnMe = buf.str();
	}
	else {
		cout << "File could not be opened " << filename << endl;
	}

	file.close();

	return returnMe;
		
}

// Gen the shader
int genShader(const char* filepath, GLenum type) {

	string shaderSrc = readFile(filepath);
	const GLchar* shader = shaderSrc.c_str();

	int shaderObj = glCreateShader(type);
	glShaderSource(shaderObj, 1, &shader, NULL);
	glCompileShader(shaderObj);

	//Error Check
	int success;
	char logMe[512];
	glGetShaderiv(shaderObj, GL_COMPILE_STATUS, &success);
	if (!success) {
		glGetShaderInfoLog(shaderObj, 512, NULL, logMe);
		cout << "Compiling Shader causes an error: " << logMe << endl;
		return -1;
	}

	return shaderObj;
		
}

// Generate the shader program that will link the vertex and the fragment
// shaders together here
int genShaderProgram(const char* vertexShaderPath, const char* fragmentShaderPath) {

	int shaderProgram = glCreateProgram();

	int vertexShader = genShader(vertexShaderPath, GL_VERTEX_SHADER);
	int fragmentShader = genShader(fragmentShaderPath, GL_FRAGMENT_SHADER);

	if (vertexShader == -1 || fragmentShader == -1) {
		return -1;
	}

	// Link the shaders
	glAttachShader(shaderProgram, vertexShader);
	glAttachShader(shaderProgram, fragmentShader);
	glLinkProgram(shaderProgram);


	//Error Check
	int success;
	char logMe[512];
	glGetShaderiv(shaderProgram, GL_LINK_STATUS, &success);
	if (!success) {
		glGetShaderInfoLog(shaderProgram, 512, NULL, logMe);
		cout << "Compiling Linking Shaders causes an error: " << logMe << endl;
		return -1;
	}

	glDeleteShader(vertexShader);
	glDeleteShader(fragmentShader);

	return shaderProgram;
}

// Bind the shader
void bindShader(int shaderProgram) {
	glUseProgram(shaderProgram);
}

// Set the projection
// This allows us to translate pixel coords into normalized coords for the shaders to easily use
void setOrthographicProjection(int shaderProgram,
	float left, float right,
	float bottom, float top,
	float near, float far) {
	float matrix[4][4] = {
		{ 2.0f / (right - left), 0.0f, 0.0f, 0.0f },
		{ 0.0f, 2.0f / (top - bottom), 0.0f, 0.0f},
		{ 0.0f, 0.0f, -2.0f / (far - near), 0.0f},
		{ -(right + left) / (right - left), -(top + bottom) / (top - bottom), -(far + near) / (far - near), 1.0f}
	};

	bindShader(shaderProgram);
	glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "projection"), 1, GL_FALSE, &matrix[0][0]);
}

// Delete the shader
void deleteShader(int shaderProgram) {
	glDeleteProgram(shaderProgram);
}

//
// Vertex Array Object (VAO) & Vertex Buffer Object (VBO)
//

// A container that stores all of the state needed to supply vertex data 
// (including which VBOs to use). It remembers the configurations of vertex 
// attributes and which VBO is bound to which attribute.
struct VAO {
	GLuint val; // Stores location of the VAO
	GLuint posVBO;
	GLuint offsetVBO;
	GLuint sizeVBO;
	GLuint EBO;
};

// Genereate VAO
void genVAO(VAO* vao){
	glGenVertexArrays(1, &vao->val);
	glBindVertexArray(vao -> val);
}

// Generate Buffer of specific type and data
template<typename T>
void genBufferObject(GLuint& bo, GLenum type, GLuint noElements, T* data, GLenum usage) {
	glGenBuffers(1, &bo);
	glBindBuffer(type, bo);
	glBufferData(type, noElements * sizeof(T), data, usage);
}

// Update the data contained in the VBO
template<typename T>
void updateData(GLuint& bo, GLintptr offset, GLuint noElements, T* data) {
	glBindBuffer(GL_ARRAY_BUFFER, bo);
	glBufferSubData(GL_ARRAY_BUFFER, offset, noElements * sizeof(T), data);
}

// Set atrcibute pointers which tell the GPU how to actually read the VBO
template<typename T>
void setAttPointer(GLuint& bo, GLuint idx, GLint size, GLenum type, GLuint stride, GLuint offset, GLuint divisor = 0) {
	glBindBuffer(GL_ARRAY_BUFFER, bo);
	glVertexAttribPointer(idx, size, type, GL_FALSE, stride * sizeof(T), (void*)(offset * sizeof(T)));
	glEnableVertexAttribArray(idx);
	if (divisor > 0) {
		// Reset idx every divisor occurrence
		glVertexAttribDivisor(idx, divisor);
	}
}

// Draw VAO
void draw(VAO vao, GLenum mode, GLuint count, GLenum type, GLint indices, GLuint instanceCount = 1) {
	glBindVertexArray(vao.val);
	glDrawElementsInstanced(mode, count, type, (void*)indices, instanceCount);
}

// Unbind the buffer
void unbindBuffer(GLenum type) {
	glBindBuffer(type, 0);
}

// Unbind VAO
void unbindVAO() {
	glBindVertexArray(0);
}

// Deallocate VAO/VBO memory
void cleanup(VAO vao) {
	glDeleteBuffers(1, &vao.posVBO);
	glDeleteBuffers(1, &vao.offsetVBO);
	glDeleteBuffers(1, &vao.sizeVBO);
	glDeleteBuffers(1, &vao.EBO);
	glDeleteVertexArrays(1, &vao.val);
}

// Method to gen things for the ball (circle VBO)
// Circles are made up of a bunch of triangles from the center
// The more triangles, the more it's "high res"
void gen2DCircleArray(float*& vertices, unsigned int*& indices, unsigned int numTriangles, float radius = 0.5f) {
	
	// We are adding 1 for the origin, then doubling cuz we are storing x AND y
	vertices = new float[(numTriangles + 1) * 2]; 
	//
	// x	y	index
	// 0.0	0.0	0
	// x1	y1	1
	// x2	y2	1
	//

	// Origin
	vertices[0] = 0.0f;
	vertices[1] = 0.0f;

	indices = new unsigned int[numTriangles * 3];

	float pi = 4 * atanf(1.0f);

	float numTrianglesF = (float)numTriangles;
	
	float theta = 0.0f; // theta here acts as our step to draw the next triangle and helps us measure the distance

	// The step to draw the triangles is [diameter of circle (that's 2pi) / num of triangles]
	// We increase the theta to draw the next triangle from the origin
	// therefore each step is i * [2pi / num of triangles]
	// x = rcos(theta) = vertices [(i+1) * 2]
	// y = rsin(theta) = vertices[(i+1) * 2 + 1]

	for (unsigned int i = 0; i < numTriangles; i++) {

		vertices[(i + 1) * 2] = radius * cosf(theta);
		vertices[(i + 1) * 2 + 1] = radius * sinf(theta);

		indices[i * 3] = 0;
		indices[i * 3 + 1] = i + 1;
		indices[i * 3 + 2] = i + 2;
		
		theta += (2 * pi) / numTriangles;
	}

	indices[(numTriangles - 1) * 3 + 2] = 1; // Go back to first index


}

//
// Main Loops
//

// Window Size changer
void framebufferSizeCallback(GLFWwindow* window, int width, int height) {

	glViewport(0, 0, width, height);
	scrWidth = width;
	scrHeight = height;

	//Update Projection Matrix
	setOrthographicProjection(shaderProgram, 0, width, 0, height, 0.0f, 1.0f);

	// Update right padel pos
	paddleOffsets[1].x = width - 35.0f; 
}

// Input Processor
void processInput(GLFWwindow* window, double dt) {

	paddleVelocity[0] = 0.0f;
	paddleVelocity[1] = 0.0f;

	if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
		glfwSetWindowShouldClose(window, true);
	}

	// Left Paddle
	if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) {
		if (paddleOffsets[0].y < scrHeight - paddleBoundary) {
			paddleVelocity[0] = paddleSpeed;
		}
		else {
			paddleOffsets[0].y = scrHeight - paddleBoundary;
		}
	}

	if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) {
		if (paddleOffsets[0].y > paddleBoundary) {
			paddleVelocity[0] = -paddleSpeed;
		}
		else {
			paddleOffsets[0].y = paddleBoundary;
		}
	}

	// Right Paddle
	if (glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS) {
		if (paddleOffsets[1].y > paddleBoundary) {
			paddleVelocity[1] = -paddleSpeed;
		}
		else {
			paddleOffsets[1].y = paddleBoundary;
		}
	}

	if (glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS) {
		if (paddleOffsets[1].y < scrHeight - paddleBoundary) {
			paddleVelocity[1] = paddleSpeed;
		}
		else {
			paddleOffsets[1].y = scrHeight - paddleBoundary;
		}
	}

	// pause key
	if (glfwGetKey(window, GLFW_KEY_P) == GLFW_RELEASE) {
		pausePressed = false;
	}
	else if (glfwGetKey(window, GLFW_KEY_P) == GLFW_PRESS && !pausePressed) {
		// key just pressed
		pauseMe = !pauseMe;
		gameSpeed = pauseMe ? 0.0f : 1.0f;
		pausePressed = true;
	}
}


// Screen clearer
void clearScreen() {
	glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT);
}

// New Frame
void newFrame(GLFWwindow* window) {
	glfwSwapBuffers(window);
	glfwPollEvents();
}

// Display the Score
void displayScore() {
	cout << leftScore << " - " << rightScore << endl;
}

//
// Cleanupers
//
void cleanup() {
	glfwTerminate();
}

int main() {
	cout << "Hello World!" << endl;

	// Timing
	double dt = 0.0;
	double lastFrame = 0.0;

	// Init (I am using OpenGL version 3.3
	initGLFW(3, 3);

	// Create the window
	GLFWwindow* window = nullptr;
	createWindow(window, title, scrWidth, scrHeight, framebufferSizeCallback);
	if (!window) {
		cout << "Window could not be created" << endl;
		cleanup();
		return -1;
	}

	// Load Glad
	if (!loadGlad()) {
		cout << "Glad could ot be loaded" << endl;
		cleanup();
		return -1;
	}

	glViewport(0, 0, scrWidth, scrHeight);

	// Shaders
	shaderProgram = genShaderProgram("main.vs", "main.fs");
	setOrthographicProjection(shaderProgram, 0, scrWidth, 0, scrHeight, 0.0f, 1.0f);

	//////
	//
	// Paddle Stuff!
	//
	//////



	// Everything is drawn in the form of triangles
	// So we setup a vertex array to hold the "endpoints" of a triangle
	// Setup vertex data
	float paddleVertices[] = {
	//		x		y
			0.5f, 0.5f, // Index 0
			-0.5f, 0.5f, // Index 1
			-0.5f, -0.5f, // Index 2
			0.5f, -0.5f // Index 3
	};

	// Then this index array holds the order of vertices which tells the order of drawing
	// Index data
	unsigned int paddleIndices[] = {
		0, 1, 2, 
		2, 3, 0
	};

	// Paddle Offsets
	paddleOffsets[0] = { 35.0f, scrHeight / 2.0f };
	paddleOffsets[1] = { scrWidth - 35.0f, scrHeight / 2.0f };

	// Paddle Sizes
	vec2d paddleSizes[] = {
		paddleWidth, paddleHeight
	};

	paddleVelocity[0] = 0.0f;
	paddleVelocity[1] = 0.0f;

	// Setup Paddles VAO/VBOs
	VAO paddleVAO;
	genVAO(&paddleVAO);

	// pos VBO
	genBufferObject<float>(paddleVAO.posVBO, GL_ARRAY_BUFFER, 2 * 4, paddleVertices, GL_STATIC_DRAW);
	setAttPointer<float>(paddleVAO.posVBO, 0, 2, GL_FLOAT, 2, 0);

	// offset VBO
	genBufferObject<vec2d>(paddleVAO.offsetVBO, GL_ARRAY_BUFFER, 2, paddleOffsets, GL_DYNAMIC_DRAW);
	setAttPointer<float>(paddleVAO.offsetVBO, 1, 2, GL_FLOAT, 2, 0, 1);

	// size VBO
	genBufferObject<vec2d>(paddleVAO.sizeVBO, GL_ARRAY_BUFFER, 1, paddleSizes, GL_STATIC_DRAW);
	setAttPointer<float>(paddleVAO.sizeVBO, 2, 2, GL_FLOAT, 2, 0, 2);

	// EBO
	genBufferObject<GLuint>(paddleVAO.EBO, GL_ELEMENT_ARRAY_BUFFER, 2 * 4, paddleIndices, GL_STATIC_DRAW);

	// unbind VBO and VAO
	unbindBuffer(GL_ARRAY_BUFFER);
	unbindVAO();

	
	//////
	//
	// Pong Ball Stuff!
	//
	//////

	float* pongVertices;
	unsigned int* pongIndices;
	unsigned int numOfTtriangles = 20;
	gen2DCircleArray(pongVertices, pongIndices, numOfTtriangles, 0.5f);

	// These two arrays will allow the shader to scale the size of the generic vertices to anything we want
	// Offsets
	pongOffset = { scrWidth / 2.0f, scrHeight / 2.0f };

	// Sizes
	vec2d pongSizes[] = {
		pongDiameter, pongDiameter
	};

	// Setup Pong Ball VAO/VBOs
	VAO pongVAO;
	genVAO(&pongVAO);

	// Pos VBO
	// The vertices are 2 per incides with 4 total indices, and we use static draw since the data wont likely change
	// to help ease the GPU's troubles :)
	genBufferObject<float>(pongVAO.posVBO, GL_ARRAY_BUFFER, 2 * (numOfTtriangles + 1), pongVertices, GL_STATIC_DRAW);
	setAttPointer<float>(pongVAO.posVBO, 0, 2, GL_FLOAT, 2, 0);

	// Offset VBO
	// The offset array is 1 by 2, and we use dyanmic draw to tell the GPU that this will likely change every frame
	genBufferObject<vec2d>(pongVAO.offsetVBO, GL_ARRAY_BUFFER, 1, &pongOffset, GL_DYNAMIC_DRAW);
	setAttPointer<float>(pongVAO.offsetVBO, 1, 2, GL_FLOAT, 2, 0, 1);

	// Size VBO
	genBufferObject<vec2d>(pongVAO.sizeVBO, GL_ARRAY_BUFFER, 1, pongSizes, GL_DYNAMIC_DRAW);
	setAttPointer<float>(pongVAO.sizeVBO, 2, 2, GL_FLOAT, 2, 0, 1);

	// EBO
	genBufferObject<unsigned int>(pongVAO.EBO, GL_ELEMENT_ARRAY_BUFFER, 3 * (numOfTtriangles), pongIndices, GL_STATIC_DRAW);

	// Unbind VBO and VAO
	unbindBuffer(GL_ARRAY_BUFFER);
	unbindVAO();

	// Resets ball to center when a player scores
	unsigned char pongReset = 0;

	// Var to check distance of Pong to paddle
	vec2d pongToPaddle;

	// Bool for collision checking
	bool collided = false;

	// Time since last collision
	unsigned int framesSinceCollided = -1;
	unsigned int framesToAllowCollision = 7;

	displayScore(); //Initial score -> 0 - 0

	// Render Loop
	while (!glfwWindowShouldClose(window)) {
		//Update the time
		dt = glfwGetTime() - lastFrame;
		lastFrame += dt;

		////
		// Physiccs
		////

		// Input
		processInput(window, dt);

		//Update Paddle Position
		paddleOffsets[0].y += paddleVelocity[0] * dt * gameSpeed;
		paddleOffsets[1].y += paddleVelocity[1] * dt * gameSpeed;

		// Update Pong Position
		pongOffset.x += pongVelocity.x * dt * gameSpeed;
		pongOffset.y += pongVelocity.y * dt * gameSpeed;

		////
		// Check collision
		////

		// Pong Ball Collision
		//Collision with window 
		// Collided with top and bottom of window
		if (pongOffset.y - pongRadius <= 0 || pongOffset.y + pongRadius >= scrHeight) {
			pongVelocity.y *= -1;
		}

		// Collided with left and right window
		if (pongOffset.x - pongRadius <= 0) {
			//cout << "Right Player Scored a Point!" << endl;
			rightScore++;
			pongReset = 1;
		}

		else if (pongOffset.x + pongRadius >= scrWidth) {
			//cout << "Left Player Scored a Point!" << endl;
			leftScore++;
			pongReset = 2;
		}

		// Resets pong's pos and velocity
		if (pongReset) {
			pongOffset.x = scrWidth / 2.0f;
			pongOffset.y = scrHeight / 2.0f;

			pongVelocity.x = pongReset == 1 ? pongVelocityInitial.x : -pongVelocityInitial.x;
			pongVelocity.y = pongVelocityInitial.y;
			
			displayScore();

			pongReset = false;
		}


		if (framesSinceCollided != -1) {
			framesSinceCollided++;
		}

		if (framesSinceCollided >= framesToAllowCollision || framesSinceCollided == -1) {
			// Paddle Collision with Pong ball
			// Chceck which paddle it is
			int paddleIndex = 0;
			if (pongOffset.x > scrHeight / 2.0f) {
				paddleIndex++;
			}

			// We are usig the paddle index from above to check which paddle it is likely to collide with
			// Then over here we are checking the distance of the Pong ball to the paddle
			pongToPaddle = { abs(pongOffset.x - paddleOffsets[paddleIndex].x), abs(pongOffset.y - paddleOffsets[paddleIndex].y) };

			if ((pongToPaddle.x <= halfPaddleWidth + pongRadius) && (pongToPaddle.y <= halfPaddleHeight + pongRadius)) {
				
				collided = false;
				
				// Collided along the LENGTH of the Paddle
				if (pongToPaddle.x <= halfPaddleWidth && pongToPaddle.x >= (halfPaddleWidth - pongRadius)) {
					collided = true;
					pongVelocity.x *= -1; // Flipping the x only
				}

				// Collided along the WIDTH of the Paddle
				else if (pongToPaddle.y <= halfPaddleHeight && pongToPaddle.y >= (halfPaddleHeight - pongRadius)) {
					collided = true;
					pongVelocity.y *= -1; // Flipping the y only
				}

				// Collided on an edge case (like literally the edge of the paddle is an edge case lol)
				if ((pongToPaddle.x - halfPaddleWidth) * (pongToPaddle.x - halfPaddleWidth)
					+ (pongToPaddle.y - halfPaddleHeight) * (pongToPaddle.y - halfPaddleHeight)
					<= (pongRadius * pongRadius) && (!collided)) {
					// Pythagorean theorm
					// Squared distance is < radius^2
					// therefore distance is less than radius -> We'll treat as length collision since I can't be bothered lol

					collided = true;

					float signedDifference = paddleOffsets[paddleIndex].x - pongOffset.x;
					if (paddleIndex == 0) {
						// Reversing the difference if right paddle, lefts needs to be +ve
						signedDifference *= -1;
					}
					if ((pongToPaddle.y - halfPaddleHeight) <= (signedDifference - halfPaddleWidth)) {
						pongVelocity.x *= -1; // More of a length collision
					}
					else {
						pongVelocity.y *= -1; // Otherwise more of a width collision
					}

				}

				if (collided) {
					// Increase velocity of pong ball upong collision
					float k = 0.5f;
					pongVelocity.x *= 1.1;
					pongVelocity.y += k * 1 * paddleVelocity[paddleIndex];

					framesSinceCollided = 0;
				}
			}
		}

		



		////
		// Graphics
		////

		// Clear screen for the next frame
		clearScreen();

		// update
		updateData<vec2d>(paddleVAO.offsetVBO, 0, 2, paddleOffsets);
		updateData<vec2d>(pongVAO.offsetVBO, 0, 1, &pongOffset);

		// Render Objects
		bindShader(shaderProgram);
		draw(paddleVAO, GL_TRIANGLES, 3 * 2, GL_UNSIGNED_INT, 0, 2);
		draw(pongVAO, GL_TRIANGLES, 3 * numOfTtriangles, GL_UNSIGNED_INT, 0);

		// Swap Frames
		newFrame(window);
	}

	// Cleanup Memory
	cleanup(paddleVAO);
	cleanup(pongVAO);
	deleteShader(shaderProgram);
	cleanup();

	return 0;
}