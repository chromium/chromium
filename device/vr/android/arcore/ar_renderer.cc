// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/android/arcore/ar_renderer.h"

#include "device/vr/vr_gl_util.h"

namespace device {

namespace {

static constexpr float kQuadVertices[8] = {
    -0.5f, 0.5f, -0.5f, -0.5f, 0.5f, 0.5f, 0.5f, -0.5f,
};
static constexpr GLushort kQuadIndices[6] = {0, 1, 2, 1, 3, 2};

// clang-format off
static constexpr char const* kVertexShader = SHADER(
  precision mediump float;
  uniform mat4 u_UvTransform;
  attribute vec4 a_Position;
  varying vec2 v_TexCoordinate;

  void main() {
    // The quad vertex coordinate range is [-0.5, 0.5]. Transform to [0, 1],
    // then apply the supplied affine transform matrix to get the final UV.
    float xposition = a_Position[0] + 0.5;
    float yposition = a_Position[1] + 0.5;
    vec4 uv_in = vec4(xposition, yposition, 0.0, 1.0);
    vec4 uv_out = u_UvTransform * uv_in;
    v_TexCoordinate = vec2(uv_out.x, uv_out.y);
    gl_Position = vec4(a_Position.xyz * 2.0, 1.0);
  }
);

static constexpr char const* kFragmentShader = OEIE_SHADER(
  precision highp float;
  uniform samplerExternalOES u_Texture;
  varying vec2 v_TexCoordinate;

  void main() {
    gl_FragColor = texture2D(u_Texture, v_TexCoordinate);
  }
);
// clang-format on

}  // namespace

ArRenderer::ArRenderer() {
  std::string error;
  GLuint vertex_shader_handle =
      vr::CompileShader(GL_VERTEX_SHADER, kVertexShader, error);
  // TODO(crbug.com/866593): fail gracefully if shaders don't compile.
  CHECK(vertex_shader_handle) << error << "\nvertex_src\n" << kVertexShader;

  GLuint fragment_shader_handle =
      vr::CompileShader(GL_FRAGMENT_SHADER, kFragmentShader, error);
  CHECK(fragment_shader_handle) << error << "\nfragment_src\n"
                                << kFragmentShader;

  program_handle_ = vr::CreateAndLinkProgram(vertex_shader_handle,
                                             fragment_shader_handle, error);
  CHECK(program_handle_) << error;

  // Once the program is linked the shader objects are no longer needed
  glDeleteShader(vertex_shader_handle);
  glDeleteShader(fragment_shader_handle);

  position_handle_ = glGetAttribLocation(program_handle_, "a_Position");
  clip_rect_handle_ = glGetUniformLocation(program_handle_, "u_ClipRect");
  texture_handle_ = glGetUniformLocation(program_handle_, "u_Texture");
  uv_transform_ = glGetUniformLocation(program_handle_, "u_UvTransform");
}

void ArRenderer::Draw(int texture_handle, const float (&uv_transform)[16]) {
  if (!vertex_buffer_ || !index_buffer_) {
    GLuint buffers[2];
    glGenBuffersARB(2, buffers);
    vertex_buffer_ = buffers[0];
    index_buffer_ = buffers[1];

    glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer_);
    glBufferData(GL_ARRAY_BUFFER, std::size(kQuadVertices) * sizeof(float),
                 kQuadVertices, GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, index_buffer_);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                 std::size(kQuadIndices) * sizeof(GLushort), kQuadIndices,
                 GL_STATIC_DRAW);
  }

  glUseProgram(program_handle_);

  // Bind vertex attributes
  glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer_);

  // Set up position attribute.
  glVertexAttribPointer(position_handle_, 2, GL_FLOAT, false, 0, 0);
  glEnableVertexAttribArray(position_handle_);

  // Bind texture. This is not necessarily a 1:1 pixel copy since the
  // size is modified by framebufferScaleFactor and requestViewportScale,
  // so use GL_LINEAR.
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_EXTERNAL_OES, texture_handle);
  glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glUniform1i(texture_handle_, 0);

  glUniformMatrix4fv(uv_transform_, 1, GL_FALSE, &uv_transform[0]);

  // Blit texture to buffer
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, index_buffer_);
  glDrawElements(GL_TRIANGLES, std::size(kQuadIndices), GL_UNSIGNED_SHORT, 0);

  glDisableVertexAttribArray(position_handle_);
}

// Note that we don't explicitly delete gl objects here, they're deleted
// automatically when we call ShutdownGL, and deleting them here leads to
// segfaults.
ArRenderer::~ArRenderer() = default;

}  // namespace device
