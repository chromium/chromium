// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/gles2_cmd_clear_framebuffer.h"

#include "gpu/command_buffer/service/gl_utils.h"
#include "gpu/command_buffer/service/gles2_cmd_decoder.h"
#include "gpu/command_buffer/service/shader_manager.h"
#include "ui/gfx/geometry/size.h"

namespace {

#define SHADER(src)            \
  "#ifdef GL_ES\n"             \
  "precision mediump float;\n" \
  "#endif\n" #src

const char* g_vertex_shader_source = {
  SHADER(
    uniform float u_clear_depth;
    attribute vec4 a_position;
    void main(void) {
      gl_Position = vec4(a_position.x, a_position.y, u_clear_depth, 1.0);
    }
  ),
};

const char* g_fragment_shader_source = {
  SHADER(
    uniform vec4 u_clear_color;
    void main(void) {
      gl_FragColor = u_clear_color;
    }
  ),
};
#undef SHADER

}  // namespace

namespace gpu {
namespace gles2 {

ClearFramebufferResourceManager::ClearFramebufferResourceManager(
    const gles2::GLES2Decoder* decoder)
    : initialized_(false), program_(0u), buffer_id_(0u) {
  Initialize(decoder);
}

ClearFramebufferResourceManager::~ClearFramebufferResourceManager() = default;

void ClearFramebufferResourceManager::Initialize(
    const gles2::GLES2Decoder* decoder) {
  static_assert(
      kVertexPositionAttrib == 0u,
      "kVertexPositionAttrib must be 0");
  DCHECK(!buffer_id_);

  glGenBuffersARB(1, &buffer_id_);
  glBindBuffer(GL_ARRAY_BUFFER, buffer_id_);
  const GLfloat kQuadVertices[] = {-1.0f, -1.0f,
                                    1.0f, -1.0f,
                                    1.0f,  1.0f,
                                   -1.0f,  1.0f};
  glBufferData(
      GL_ARRAY_BUFFER, sizeof(kQuadVertices), kQuadVertices, GL_STATIC_DRAW);
  decoder->RestoreBufferBindings();
  initialized_ = true;
}

void ClearFramebufferResourceManager::Destroy() {
  if (!initialized_)
    return;

  glDeleteProgram(program_);
  glDeleteBuffersARB(1, &buffer_id_);
}

void ClearFramebufferResourceManager::ClearFramebuffer(
    const gles2::GLES2Decoder* decoder,
    const gfx::Size& max_viewport_size,
    GLbitfield mask,
    GLfloat clear_color_red,
    GLfloat clear_color_green,
    GLfloat clear_color_blue,
    GLfloat clear_color_alpha,
    GLfloat clear_depth_value,
    GLint clear_stencil_value) {
  if (!initialized_) {
    DLOG(ERROR) << "Uninitialized manager.";
    return;
  }

  if (!program_) {
    program_ = glCreateProgram();
    GLuint vertex_shader = glCreateShader(GL_VERTEX_SHADER);
    CompileShaderWithLog(vertex_shader, g_vertex_shader_source);
    glAttachShader(program_, vertex_shader);
    GLuint fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);
    CompileShaderWithLog(fragment_shader, g_fragment_shader_source);
    glAttachShader(program_, fragment_shader);
    glBindAttribLocation(program_, kVertexPositionAttrib, "a_position");
    glLinkProgram(program_);
#if DCHECK_IS_ON()
    GLint linked = GL_FALSE;
    glGetProgramiv(program_, GL_LINK_STATUS, &linked);
    if (GL_TRUE != linked)
      DLOG(ERROR) << "Program link failure.";
#endif
    depth_handle_ = glGetUniformLocation(program_, "u_clear_depth");
    color_handle_ = glGetUniformLocation(program_, "u_clear_color");
    glDeleteShader(fragment_shader);
    glDeleteShader(vertex_shader);
  }
  glUseProgram(program_);

#if DCHECK_IS_ON()
  glValidateProgram(program_);
  GLint validation_status = GL_FALSE;
  glGetProgramiv(program_, GL_VALIDATE_STATUS, &validation_status);
  if (GL_TRUE != validation_status)
    DLOG(ERROR) << "Invalid shader.";
#endif

  decoder->ClearAllAttributes();
  glEnableVertexAttribArray(kVertexPositionAttrib);

  glBindBuffer(GL_ARRAY_BUFFER, buffer_id_);
  glVertexAttribPointer(kVertexPositionAttrib, 2, GL_FLOAT, GL_FALSE, 0, 0);

  glUniform1f(depth_handle_, clear_depth_value);
  glUniform4f(color_handle_, clear_color_red, clear_color_green,
              clear_color_blue, clear_color_alpha);

  if (!(mask & GL_COLOR_BUFFER_BIT)) {
    glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
  }

  if (mask & GL_DEPTH_BUFFER_BIT) {
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_ALWAYS);
  } else {
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
  }

  if (mask & GL_STENCIL_BUFFER_BIT) {
    glEnable(GL_STENCIL_TEST);
    glStencilFunc(GL_ALWAYS, clear_stencil_value, 0xFF);
    glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
  } else {
    glDisable(GL_STENCIL_TEST);
    glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
    glStencilMask(0);
  }

  glDisable(GL_CULL_FACE);
  glDisable(GL_BLEND);
  glDisable(GL_POLYGON_OFFSET_FILL);

  glViewport(0, 0, max_viewport_size.width(), max_viewport_size.height());
  glDrawArrays(GL_TRIANGLE_FAN, 0, 4);

  decoder->RestoreAllAttributes();
  decoder->RestoreProgramBindings();
  decoder->RestoreBufferBindings();
  decoder->RestoreGlobalState();
}

}  // namespace gles2
}  // namespace gpu
