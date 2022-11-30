// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/vr_gl_util.h"

#include "ui/gfx/geometry/transform.h"

namespace vr {

// This code is adapted from the GVR Treasure Hunt demo source.
std::array<float, 16> MatrixToGLArray(const gfx::Transform& transform) {
  std::array<float, 16> result;
  transform.GetColMajorF(result.data());
  return result;
}

GLuint CompileShader(GLenum shader_type,
                     const std::string& shader_source,
                     std::string& error) {
  GLuint shader_handle = glCreateShader(shader_type);
  if (shader_handle != 0) {
    // Pass in the shader source. No need to pass in a length for
    // null-terminated input.
    const char* source = shader_source.c_str();
    glShaderSource(shader_handle, 1, &source, nullptr);
    // Compile the shader.
    glCompileShader(shader_handle);
    // Get the compilation status.
    GLint status = GL_FALSE;
    glGetShaderiv(shader_handle, GL_COMPILE_STATUS, &status);
    if (status == GL_FALSE) {
      GLint info_log_length = 0;
      glGetShaderiv(shader_handle, GL_INFO_LOG_LENGTH, &info_log_length);
      GLchar* str_info_log = new GLchar[info_log_length + 1];
      glGetShaderInfoLog(shader_handle, info_log_length, nullptr, str_info_log);
      error = "Error compiling shader: ";
      error += str_info_log;
      delete[] str_info_log;
      glDeleteShader(shader_handle);
      shader_handle = 0;
    }
  } else {
    error = "Could not create a shader handle (did not attempt compilation).";
  }

  return shader_handle;
}

GLuint CreateAndLinkProgram(GLuint vertext_shader_handle,
                            GLuint fragment_shader_handle,
                            std::string& error) {
  GLuint program_handle = glCreateProgram();

  if (program_handle != 0) {
    // Bind the vertex shader to the program.
    glAttachShader(program_handle, vertext_shader_handle);

    // Bind the fragment shader to the program.
    glAttachShader(program_handle, fragment_shader_handle);

    // Link the two shaders together into a program.
    glLinkProgram(program_handle);

    // Get the link status.
    GLint link_status = GL_FALSE;
    glGetProgramiv(program_handle, GL_LINK_STATUS, &link_status);

    // If the link failed, delete the program.
    if (link_status == GL_FALSE) {
      GLint info_log_length = 0;
      glGetProgramiv(program_handle, GL_INFO_LOG_LENGTH, &info_log_length);

      GLchar* str_info_log = new GLchar[info_log_length + 1];
      glGetProgramInfoLog(program_handle, info_log_length, nullptr,
                          str_info_log);
      error = "Error compiling program: ";
      error += str_info_log;
      delete[] str_info_log;
      glDeleteProgram(program_handle);
      program_handle = 0;
    }
  }

  return program_handle;
}

void SetTexParameters(GLenum texture_type) {
  glTexParameteri(texture_type, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(texture_type, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glTexParameteri(texture_type, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(texture_type, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
}

void SetColorUniform(GLuint handle, SkColor c) {
  glUniform4f(handle, SkColorGetR(c) / 255.0, SkColorGetG(c) / 255.0,
              SkColorGetB(c) / 255.0, SkColorGetA(c) / 255.0);
}

void SetOpaqueColorUniform(GLuint handle, SkColor c) {
  glUniform3f(handle, SkColorGetR(c) / 255.0, SkColorGetG(c) / 255.0,
              SkColorGetB(c) / 255.0);
}

}  // namespace vr
