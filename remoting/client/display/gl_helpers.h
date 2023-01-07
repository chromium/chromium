// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_CLIENT_DISPLAY_GL_HELPERS_H_
#define REMOTING_CLIENT_DISPLAY_GL_HELPERS_H_

#include "remoting/client/display/sys_opengl.h"

namespace remoting {

// Compiles a shader and returns the reference to the shader if it succeeds.
GLuint CompileShader(GLenum shader_type, const char* shader_source);

// Creates a program with the given reference to the vertex shader and fragment
// shader. returns the reference of the program if it succeeds.
GLuint CreateProgram(GLuint vertex_shader, GLuint fragment_shader);

// Creates and returns the texture names if it succeeds.
GLuint CreateTexture();

// Creates a GL_ARRAY_BUFFER and fills it with |data|. Returns the reference to
// the buffer.
GLuint CreateBuffer(const void* data, int size);

}  // namespace remoting

#endif  // REMOTING_CLIENT_DISPLAY_GL_HELPERS_H_
