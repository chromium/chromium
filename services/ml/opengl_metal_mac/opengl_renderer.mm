/*
 Copyright © 2018 Apple Inc.

 Permission is hereby granted, free of charge, to any person obtaining a copy of
 this software and associated documentation files (the "Software"), to deal in
 the Software without restriction, including without limitation the rights to
 use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
 of the Software, and to permit persons to whom the Software is furnished to do
 so, subject to the following conditions:

 The above copyright notice and this permission notice shall be included in all
 copies or substantial portions of the Software.

 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 SOFTWARE.
*/

#import "opengl_renderer.h"

// #import <OpenGL/OpenGL.h>
// #import <OpenGL/gl.h>
#import <OpenGL/gl3.h>
#import <simd/simd.h>

#include "base/logging.h"
#include "fragment_shader.h"
#include "vertex_shader.h"

#define BUFFER_OFFSET(i) ((char*)NULL + (i))

@implementation OpenGLRenderer {
  GLuint _programName;
  GLuint _vertexArrayName;
}

// Indicies to which we will set vertex array attibutes
// See buildVAO and buildProgram
enum { POS_ATTRIB_IDX, TEXCOORD_ATTRIB_IDX };

- (instancetype)initWithDefault {
  self = [super init];

  _vertexArrayName = [self buildVAO];

  _programName = [self buildProgram];

  return self;
}

- (GLuint)buildVAO {
  typedef struct {
    vector_float4 position;
    packed_float2 texCoord;
  } AAPLVertex;

  static const AAPLVertex QuadVertices[] = {
      // x, y, z, w
      {{-1.0, -1.0, 0.0, 1.0}, {0.0, 0.0}}, {{1.0, -1.0, 0.0, 1.0}, {1.0, 0.0}},
      {{-1.0, 1.0, 0.0, 1.0}, {0.0, 1.0}},

      {{1.0, -1.0, 0.0, 1.0}, {1.0, 0.0}},  {{-1.0, 1.0, 0.0, 1.0}, {0.0, 1.0}},
      {{1.0, 1.0, 0.0, 1.0}, {1.0, 1.0}}};

  GLuint vaoName;

  glGenVertexArrays(1, &vaoName);
  glBindVertexArray(vaoName);

  GLuint bufferName;

  glGenBuffers(1, &bufferName);
  glBindBuffer(GL_ARRAY_BUFFER, bufferName);

  glBufferData(GL_ARRAY_BUFFER, sizeof(QuadVertices), QuadVertices,
               GL_STATIC_DRAW);

  glEnableVertexAttribArray(POS_ATTRIB_IDX);

  GLuint stride = sizeof(AAPLVertex);
  GLuint positionOffset = offsetof(AAPLVertex, position);

  glVertexAttribPointer(POS_ATTRIB_IDX, 2, GL_FLOAT, GL_FALSE, stride,
                        BUFFER_OFFSET(positionOffset));

  // Enable the position attribute for this VAO
  glEnableVertexAttribArray(TEXCOORD_ATTRIB_IDX);

  GLuint texCoordOffset = offsetof(AAPLVertex, texCoord);

  glVertexAttribPointer(TEXCOORD_ATTRIB_IDX, 2, GL_FLOAT, GL_FALSE, stride,
                        BUFFER_OFFSET(texCoordOffset));

  glGetError();

  return vaoName;
}

- (void)destroyVAO:(GLuint)vaoName {
  // Bind the VAO so we can get data from it
  glBindVertexArray(vaoName);

  // For every possible attribute set in the VAO, delete the attached buffer
  for (GLuint index = 0; index < 16; index++) {
    GLuint bufName;
    glGetVertexAttribiv(index, GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING,
                        (GLint*)&bufName);
    if (bufName) {
      glDeleteBuffers(1, &bufName);
    }
  }

  glDeleteVertexArrays(1, &vaoName);

  glGetError();
}

- (GLuint)buildProgram {
  // Determine if GLSL version 140 is supported by this context.
  //  We'll use this info to generate a GLSL shader source string
  //  with the proper version preprocessor string prepended
  float glLanguageVersion;
  sscanf((char*)glGetString(GL_SHADING_LANGUAGE_VERSION), "%f",
         &glLanguageVersion);
  // GL_SHADING_LANGUAGE_VERSION returns the version standard version form
  //  with decimals, but the GLSL version preprocessor directive simply
  //  uses integers (thus 1.10 should 110 and 1.40 should be 140, etc.)
  //  We multiply the floating point number by 100 to get a proper
  //  number for the GLSL preprocessor directive
  GLuint version = 100 * glLanguageVersion;
  // Get the size of the version preprocessor string info so we know
  //  how much memory to allocate for our sourceString
  const GLsizei versionStringSize = sizeof("#version 123\n");

  // Specify and compile VertexShader.
  // Allocate memory for the source string including the version preprocessor
  // information
  NSString* vertSourceString = [NSString stringWithUTF8String:VERTEX_SHADER];
  GLchar* sourceString =
      (GLchar*)malloc(vertSourceString.length + versionStringSize);

  // Prepend our vertex shader source string with the supported GLSL version so
  // the shader will work on ES, Legacy, and OpenGL 3.2 Core Profile contexts
  sprintf(sourceString, "#version %d\n%s", version,
          vertSourceString.UTF8String);

  GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
  glShaderSource(vertexShader, 1, (const GLchar**)&(sourceString), NULL);
  glCompileShader(vertexShader);

  GLint logLength, status;
  glGetShaderiv(vertexShader, GL_INFO_LOG_LENGTH, &logLength);
  if (logLength > 0) {
    GLchar* log = (GLchar*)malloc(logLength);
    glGetShaderInfoLog(vertexShader, logLength, &logLength, log);
    LOG(ERROR) << "Vtx Shader compile log:" << log;
    free(log);
  }
  glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &status);
  if (status == 0) {
    LOG(ERROR) << "Failed to compile vtx shader:" << sourceString;
    return 0;
  }

  free(sourceString);
  sourceString = NULL;

  // Create a program object
  GLuint prgName = glCreateProgram();

  glBindAttribLocation(prgName, POS_ATTRIB_IDX, "inPosition");
  glBindAttribLocation(prgName, TEXCOORD_ATTRIB_IDX, "inTexcoord");

  // Attach the vertex shader to our program
  glAttachShader(prgName, vertexShader);

  // Delete the vertex shader since it is now attached
  // to the program, which will retain a reference to it
  glDeleteShader(vertexShader);

  // Specify and compile Fragment Shader.
  // Allocate memory for the source string including the version preprocessor
  // information
  NSString* fragSourceString = [NSString stringWithUTF8String:FRAGMENT_SHADER];
  sourceString = (GLchar*)malloc(fragSourceString.length + versionStringSize);

  // Prepend our fragment shader source string with the supported GLSL version
  // so the shader will work on ES, Legacy, and OpenGL 3.2 Core Profile contexts
  sprintf(sourceString, "#version %d\n%s", version,
          fragSourceString.UTF8String);

  GLuint fragShader = glCreateShader(GL_FRAGMENT_SHADER);
  glShaderSource(fragShader, 1, (const GLchar**)&(sourceString), NULL);
  glCompileShader(fragShader);
  glGetShaderiv(fragShader, GL_INFO_LOG_LENGTH, &logLength);
  if (logLength > 0) {
    GLchar* log = (GLchar*)malloc(logLength);
    glGetShaderInfoLog(fragShader, logLength, &logLength, log);
    LOG(ERROR) << "Frag Shader compile log:" << log;
    free(log);
  }

  glGetShaderiv(fragShader, GL_COMPILE_STATUS, &status);
  if (status == 0) {
    LOG(ERROR) << "Failed to compile frag shader:" << sourceString;
    return 0;
  }

  free(sourceString);
  sourceString = NULL;

  // Attach the fragment shader to our program
  glAttachShader(prgName, fragShader);

  // Delete the fragment shader since it is now attached
  // to the program, which will retain a reference to it
  glDeleteShader(fragShader);

  // Link the program.
  glLinkProgram(prgName);
  glGetProgramiv(prgName, GL_INFO_LOG_LENGTH, &logLength);
  if (logLength > 0) {
    GLchar* log = (GLchar*)malloc(logLength);
    glGetProgramInfoLog(prgName, logLength, &logLength, log);
    LOG(ERROR) << "Program link log:" << log;
    free(log);
  }

  glGetProgramiv(prgName, GL_LINK_STATUS, &status);
  if (status == 0) {
    LOG(ERROR) << "Failed to link program";
    return 0;
  }

  glGetProgramiv(prgName, GL_INFO_LOG_LENGTH, &logLength);
  if (logLength > 0) {
    GLchar* log = (GLchar*)malloc(logLength);
    glGetProgramInfoLog(prgName, logLength, &logLength, log);
    LOG(ERROR) << "Program validate log:" << log;
    free(log);
  }

  glUseProgram(prgName);

  GLint location = glGetUniformLocation(prgName, "texture1");
  if (location < 0) {
    LOG(ERROR) << "Could not get sampler Uniform Index";
    return 0;
  }
  // Indicate that the diffuse texture will be bound to texture unit 1
  GLint unit = 1;
  glUniform1i(location, unit);

  glGetError();

  return prgName;
}

- (void)draw:(GLuint)frameBufferName
    texTarget:(GLenum)texTarget
      texName:(GLuint)texName {
  glBindFramebuffer(GL_FRAMEBUFFER, frameBufferName);

  //    glClearColor(1, 0, 0, 1);
  //    glClear(GL_COLOR_BUFFER_BIT);

  glUseProgram(_programName);

  glActiveTexture(GL_TEXTURE1);
  glBindTexture(texTarget, texName);

  glBindVertexArray(_vertexArrayName);

  glDrawArrays(GL_TRIANGLES, 0, 6);

  glGetError();
}

@end
