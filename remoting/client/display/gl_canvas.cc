// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/client/display/gl_canvas.h"

#include "base/check.h"
#include "remoting/client/display/gl_helpers.h"
#include "remoting/client/display/gl_math.h"

namespace {

const int kVertexSize = 2;
const int kVertexCount = 4;

const char kTexCoordToViewVert[] =
    // Region of the texture to be used (normally the whole texture).
    "varying vec2 v_texCoord;\n"
    "attribute vec2 a_texCoord;\n"

    // Position to draw the texture on the canvas.
    "attribute vec2 a_position;\n"

    // Defines the zoom and pan configurations of the canvas.
    "uniform mat3 u_transform;\n"

    // Size of the view in pixel.
    "uniform vec2 u_viewSize;\n"

    // This matrix translates normalized texture coordinates
    // ([0, 1] starting at upper-left corner) to the view coordinates
    // ([-1, 1] starting at the center of the screen).
    // Note that the matrix is defined in column-major order.
    "const mat3 tex_to_view = mat3(2, 0, 0,\n"
    "                              0, -2, 0,\n"
    "                              -1, 1, 0);\n"
    "void main() {\n"
    "  v_texCoord = a_texCoord;\n"

    // Transforms coordinates related to the canvas to coordinates
    // related to the view.
    "  vec3 trans_position = u_transform * vec3(a_position, 1.0);\n"

    // Normalize the position by the size of the view.
    "  trans_position.xy /= u_viewSize;\n"

    // Transforms texture coordinates to view coordinates and adds
    // projection component 1.
    "  gl_Position = vec4(tex_to_view * trans_position, 1.0);\n"
    "}";

const char kDrawTexFrag[] =
    "precision mediump float;\n"

    // Region on the texture to be used (normally the whole texture).
    "varying vec2 v_texCoord;\n"
    "uniform sampler2D u_texture;\n"
    "uniform float u_alpha_multiplier;\n"
    "void main() {\n"
    "  gl_FragColor = texture2D(u_texture, v_texCoord);\n"
    "  gl_FragColor.a *= u_alpha_multiplier;\n"
    "}";

}  // namespace

namespace remoting {

GlCanvas::GlCanvas(int gl_version) : gl_version_(gl_version) {
  glGetIntegerv(GL_MAX_TEXTURE_SIZE, &max_texture_size_);

  vertex_shader_ = CompileShader(GL_VERTEX_SHADER, kTexCoordToViewVert);
  fragment_shader_ = CompileShader(GL_FRAGMENT_SHADER, kDrawTexFrag);
  program_ = CreateProgram(vertex_shader_, fragment_shader_);
  glUseProgram(program_);

  transform_location_ = glGetUniformLocation(program_, "u_transform");
  view_size_location_ = glGetUniformLocation(program_, "u_viewSize");
  texture_location_ = glGetUniformLocation(program_, "u_texture");
  alpha_multiplier_location_ =
      glGetUniformLocation(program_, "u_alpha_multiplier");
  position_location_ = glGetAttribLocation(program_, "a_position");
  tex_cord_location_ = glGetAttribLocation(program_, "a_texCoord");
  glEnableVertexAttribArray(position_location_);
  glEnableVertexAttribArray(tex_cord_location_);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

GlCanvas::~GlCanvas() {
  DCHECK(thread_checker_.CalledOnValidThread());
  glDisable(GL_BLEND);
  glDisableVertexAttribArray(tex_cord_location_);
  glDisableVertexAttribArray(position_location_);
  glDeleteProgram(program_);
  glDeleteShader(vertex_shader_);
  glDeleteShader(fragment_shader_);
}

void GlCanvas::Clear() {
#ifndef NDEBUG
  // Set the background clear color to bright green for debugging purposes.
  glClearColor(0.0f, 1.0f, 0.0f, 1.0f);
#else
  // Set the background clear color to black.
  glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
#endif
  glClear(GL_COLOR_BUFFER_BIT);
}

void GlCanvas::SetTransformationMatrix(const std::array<float, 9>& matrix) {
  DCHECK(thread_checker_.CalledOnValidThread());
  std::array<float, 9> transposed_matrix = matrix;
  TransposeTransformationMatrix(&transposed_matrix);
  glUniformMatrix3fv(transform_location_, 1, GL_FALSE,
                     transposed_matrix.data());
  transformation_set_ = true;
}

void GlCanvas::SetViewSize(int width, int height) {
  DCHECK(width > 0 && height > 0);
  float view_size[2]{static_cast<float>(width), static_cast<float>(height)};
  glUniform2fv(view_size_location_, 1, view_size);
  view_size_set_ = true;
}

void GlCanvas::DrawTexture(int texture_id,
                           int texture_handle,
                           int vertex_buffer,
                           float alpha_multiplier) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!view_size_set_ || !transformation_set_) {
    return;
  }
  glActiveTexture(GL_TEXTURE0 + texture_id);
  glBindTexture(GL_TEXTURE_2D, texture_handle);
  glUniform1i(texture_location_, texture_id);
  glUniform1f(alpha_multiplier_location_, alpha_multiplier);
  glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer);

  glVertexAttribPointer(position_location_, kVertexSize, GL_FLOAT, GL_FALSE, 0,
                        0);
  glVertexAttribPointer(
      tex_cord_location_, kVertexSize, GL_FLOAT, GL_FALSE, 0,
      reinterpret_cast<void*>(kVertexSize * kVertexCount * sizeof(GLfloat)));

  glDrawArrays(GL_TRIANGLE_STRIP, 0, kVertexCount);
  glBindBuffer(GL_ARRAY_BUFFER, 0);
  glBindTexture(GL_TEXTURE_2D, 0);
}

int GlCanvas::GetVersion() const {
  return gl_version_;
}

int GlCanvas::GetMaxTextureSize() const {
  return max_texture_size_;
}

base::WeakPtr<Canvas> GlCanvas::GetWeakPtr() {
  DCHECK(thread_checker_.CalledOnValidThread());
  return weak_factory_.GetWeakPtr();
}

}  // namespace remoting
