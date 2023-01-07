// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/client/display/gl_helpers.h"

#include "base/logging.h"

namespace remoting {

GLuint CompileShader(GLenum shader_type, const char* shader_source) {
  GLuint shader = glCreateShader(shader_type);

  if (shader != 0) {
    int shader_source_length = strlen(shader_source);
    // Pass in the shader source.
    glShaderSource(shader, 1, &shader_source, &shader_source_length);

    // Compile the shader.
    glCompileShader(shader);

    // Get the compilation status.
    GLint compile_status;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &compile_status);

    // If the compilation failed, delete the shader.
    if (compile_status == GL_FALSE) {
      LOG(ERROR) << "Error compiling shader: \n" << shader_source;
      glDeleteShader(shader);
      shader = 0;
    }
  }

  if (shader == 0) {
    LOG(FATAL) << "Error creating shader.";
  }

  return shader;
}

GLuint CreateProgram(GLuint vertex_shader, GLuint fragment_shader) {
  GLuint program = glCreateProgram();

  if (program != 0) {
    glAttachShader(program, vertex_shader);
    glAttachShader(program, fragment_shader);
    glLinkProgram(program);

    // Get the link status.
    GLint link_status;
    glGetProgramiv(program, GL_LINK_STATUS, &link_status);
    // If the link failed, delete the program.
    if (link_status == GL_FALSE) {
      LOG(ERROR) << "Error compiling program.";
      glDeleteProgram(program);
      program = 0;
    }
  }

  if (program == 0) {
    LOG(FATAL) << "Error creating program.";
  }

  return program;
}

GLuint CreateTexture() {
  GLuint texture;
  glGenTextures(1, &texture);
  if (texture == 0) {
    LOG(FATAL) << "Error creating texture.";
  }
  return texture;
}

GLuint CreateBuffer(const void* data, int size) {
  GLuint buffer;
  glGenBuffers(1, &buffer);
  glBindBuffer(GL_ARRAY_BUFFER, buffer);
  glBufferData(GL_ARRAY_BUFFER, size, data, GL_STATIC_DRAW);
  glBindBuffer(GL_ARRAY_BUFFER, 0);
  return buffer;
}

}  // namespace remoting
