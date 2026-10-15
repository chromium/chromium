// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/android/xr_renderer.h"

#include <algorithm>
#include <iterator>

#include "base/check_op.h"
#include "device/vr/vr_gl_util.h"

namespace device {

namespace {

constexpr float kStereoEyeUvWidth = 0.5f;

void BindAndConfigureTexture(GLenum texture_unit,
                             const LocalTexture& texture,
                             GLint uniform_location,
                             GLint unit_index) {
  glActiveTexture(texture_unit);
  glBindTexture(texture.target, texture.id);
  glTexParameteri(texture.target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(texture.target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glTexParameteri(texture.target, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(texture.target, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  if (texture.target != GL_TEXTURE_EXTERNAL_OES) {
    glTexParameteri(texture.target, GL_TEXTURE_BASE_LEVEL, 0);
    glTexParameteri(texture.target, GL_TEXTURE_MAX_LEVEL, 0);
  }
  glUniform1i(uniform_location, unit_index);
}

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

// Same as kVertexShader, but we need ES 3.0 for the fragment shader.
static constexpr char const* kVertexShaderES3 = SHADER_ES3(
  precision mediump float;
  in vec4 a_Position;
  out vec2 v_TexCoordinate;
  uniform mat4 u_UvTransform;

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

static constexpr char const* kFragmentShaderExternal = OEIE_SHADER(
  precision highp float;
  uniform samplerExternalOES u_Texture;
  varying vec2 v_TexCoordinate;
  uniform float u_Opacity;

  void main() {
    // Keep the pixels premultiplied.
    gl_FragColor = texture2D(u_Texture, v_TexCoordinate) * u_Opacity;
  }
);

static constexpr char const* kFragmentShader2D = OEIE_SHADER(
  precision highp float;
  uniform sampler2D u_Texture;
  varying vec2 v_TexCoordinate;
  uniform float u_Opacity;

  void main() {
    // Keep the pixels premultiplied.
    gl_FragColor = texture2D(u_Texture, v_TexCoordinate) * u_Opacity;
  }
);

static constexpr char const* kFragmentShaderCubemap = OEIE_SHADER(
  precision highp float;
  uniform sampler2D u_Texture;
  varying vec2 v_TexCoordinate;
  uniform float u_Opacity;
  uniform float u_ColumnIndex;
  uniform float u_RowIndex;

  void main() {
    // We have 3 tiles per row, and total 2 rows.
    // Scale the 0..1 UV to the size of a single tile (1/3 width, 1/2 height)
    float x = (v_TexCoordinate.x + u_ColumnIndex) / 3.0;
    float y = (v_TexCoordinate.y + u_RowIndex) / 2.0;

    // Keep the pixels premultiplied.
    gl_FragColor = texture2D(u_Texture, vec2(x, y)) * u_Opacity;
  }
);

// We need ES 3.0 for 2D arrays.
static constexpr char const* kFragmentShaderArray = SHADER_ES3(
  precision highp float;
  in vec2 v_TexCoordinate;
  out vec4 outColor;
  // "highp" is needed because some compilers require the texture
  // precision to be explicitly defined.
  uniform highp sampler2DArray u_Texture;
  uniform float u_Opacity;
  uniform int u_LayerIndex;

  void main() {
    vec3 coord = vec3(v_TexCoordinate, u_LayerIndex);
    // Keep the pixels premultiplied.
    outColor = texture(u_Texture, coord) * u_Opacity;
  }
);

// `u_OverlayBounds` holds (x_min, y_min, x_max, y_max) in [0, 1] UV space.
// `u_OverlayUvTransform` holds (scale_x, scale_y, offset_x, offset_y).
static constexpr const char* kFragmentShaderArrayOverlay = SHADER_ES3(
  precision highp float;
  in vec2 v_TexCoordinate;
  out vec4 outColor;
  uniform highp sampler2DArray u_Texture;
  uniform highp sampler2D u_OverlayTexture;
  uniform float u_Opacity;
  uniform int u_LayerIndex;
  uniform vec4 u_OverlayBounds;
  uniform vec4 u_OverlayUvTransform;

  void main() {
    vec3 coord = vec3(v_TexCoordinate, u_LayerIndex);
    vec4 base_color = texture(u_Texture, coord) * u_Opacity;
    if (v_TexCoordinate.x >= u_OverlayBounds.x &&
        v_TexCoordinate.x <= u_OverlayBounds.z &&
        v_TexCoordinate.y >= u_OverlayBounds.y &&
        v_TexCoordinate.y <= u_OverlayBounds.w) {
      vec2 overlay_uv =
          v_TexCoordinate * u_OverlayUvTransform.xy + u_OverlayUvTransform.zw;
      vec4 overlay_color = texture(u_OverlayTexture, overlay_uv);
      outColor = mix(base_color, overlay_color, overlay_color.a);
    } else {
      outColor = base_color;
    }
  }
);
// clang-format on

}  // namespace

XrRenderer::Program XrRenderer::CreateProgram(const std::string& vertex,
                                              const std::string& fragment) {
  std::string error;
  GLuint vertex_shader_handle =
      vr::CompileShader(GL_VERTEX_SHADER, vertex, error);

  CHECK(vertex_shader_handle) << error << "\nvertex_src\n" << vertex;

  GLuint fragment_shader_handle =
      vr::CompileShader(GL_FRAGMENT_SHADER, fragment, error);
  CHECK(fragment_shader_handle) << error << "\nfragment_src\n" << fragment;

  Program result;
  result.program_handle_ = vr::CreateAndLinkProgram(
      vertex_shader_handle, fragment_shader_handle, error);
  CHECK(result.program_handle_) << error;

  // Once the program is linked the shader objects are no longer needed
  glDeleteShader(vertex_shader_handle);
  glDeleteShader(fragment_shader_handle);

  result.position_handle_ =
      glGetAttribLocation(result.program_handle_, "a_Position");
  result.texture_handle_ =
      glGetUniformLocation(result.program_handle_, "u_Texture");
  result.overlay_texture_handle_ =
      glGetUniformLocation(result.program_handle_, "u_OverlayTexture");
  result.uv_transform_ =
      glGetUniformLocation(result.program_handle_, "u_UvTransform");
  result.opacity_ = glGetUniformLocation(result.program_handle_, "u_Opacity");
  result.column_index_ =
      glGetUniformLocation(result.program_handle_, "u_ColumnIndex");
  result.row_index_ =
      glGetUniformLocation(result.program_handle_, "u_RowIndex");
  result.layer_index_ =
      glGetUniformLocation(result.program_handle_, "u_LayerIndex");
  result.overlay_bounds_ =
      glGetUniformLocation(result.program_handle_, "u_OverlayBounds");
  result.overlay_uv_transform_ =
      glGetUniformLocation(result.program_handle_, "u_OverlayUvTransform");

  return result;
}

XrRenderer::XrRenderer() {
  program_external_ = CreateProgram(kVertexShader, kFragmentShaderExternal);
  program_2d_ = CreateProgram(kVertexShader, kFragmentShader2D);
  program_cubemap_ = CreateProgram(kVertexShader, kFragmentShaderCubemap);
  program_array_ = CreateProgram(kVertexShaderES3, kFragmentShaderArray);
  program_array_overlay_ =
      CreateProgram(kVertexShaderES3, kFragmentShaderArrayOverlay);
}

void XrRenderer::EnsureVertexBuffers() {
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
}

void XrRenderer::Draw(const Program& program,
                      const LocalTexture& texture,
                      const float (&uv_transform)[16],
                      float opacity,
                      int face_index,
                      int layer_index) {
  EnsureVertexBuffers();

  CHECK(texture.target == GL_TEXTURE_EXTERNAL_OES ||
        texture.target == GL_TEXTURE_2D ||
        (texture.target == GL_TEXTURE_2D_ARRAY && layer_index >= 0));

  glUseProgram(program.program_handle_);

  // Bind vertex attributes
  glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer_);

  // Set up position attribute.
  glVertexAttribPointer(program.position_handle_, 2, GL_FLOAT, false, 0,
                        nullptr);
  glEnableVertexAttribArray(program.position_handle_);

  // Bind texture. This is not necessarily a 1:1 pixel copy since the
  // size is modified by framebufferScaleFactor and requestViewportScale,
  // so use GL_LINEAR.
  BindAndConfigureTexture(GL_TEXTURE0, texture, program.texture_handle_, 0);

  glUniformMatrix4fv(program.uv_transform_, 1, GL_FALSE, &uv_transform[0]);
  glUniform1f(program.opacity_, std::clamp(opacity, 0.0f, 1.0f));

  if (face_index >= 0) {
    CHECK(face_index < 6);
    // 6 faces are placed as 3 tiles per row.
    glUniform1f(program.column_index_, static_cast<float>(face_index % 3));
    glUniform1f(program.row_index_, static_cast<float>(face_index / 3));
  } else if (layer_index >= 0) {
    glUniform1i(program.layer_index_, layer_index);
  }

  // Blit texture to buffer
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, index_buffer_);
  glDrawElements(GL_TRIANGLES, std::size(kQuadIndices), GL_UNSIGNED_SHORT,
                 nullptr);

  glDisableVertexAttribArray(program.position_handle_);
}

void XrRenderer::Draw(const LocalTexture& texture,
                      const float (&uv_transform)[16],
                      float opacity) {
  const Program& program = texture.target == GL_TEXTURE_EXTERNAL_OES
                               ? program_external_
                               : program_2d_;
  Draw(program, texture, uv_transform, opacity);
}

void XrRenderer::DrawCubemap(const LocalTexture& texture,
                             uint32_t target_texture,
                             const float (&uv_transform)[16],
                             float opacity) {
  for (int i = 0; i < 6; ++i) {
    glFramebufferTexture2DEXT(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                              GL_TEXTURE_CUBE_MAP_POSITIVE_X + i,
                              target_texture, 0);
    Draw(program_cubemap_, texture, uv_transform, opacity, i);
  }
}

void XrRenderer::DrawArray(const LocalTexture& texture,
                           uint32_t target_texture,
                           uint16_t layers,
                           const float (&uv_transform)[16],
                           float opacity) {
  for (uint16_t i = 0; i < layers; ++i) {
    glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                              target_texture, 0, i /*layer index*/);
    DrawArrayLayer(texture, i, uv_transform, opacity);
  }
}

void XrRenderer::DrawArrayLayer(const LocalTexture& texture,
                                uint16_t layer_index,
                                const float (&uv_transform)[16],
                                float opacity) {
  Draw(program_array_, texture, uv_transform, opacity, -1, layer_index);
}

void XrRenderer::DrawArrayLayerWithOverlay(const LocalTexture& texture,
                                           uint16_t layer_index,
                                           const float (&uv_transform)[16],
                                           float opacity,
                                           const OverlayLayerParams& overlay) {
  if (overlay.bounds.IsEmpty()) {
    DrawArrayLayer(texture, layer_index, uv_transform, opacity);
    return;
  }

  EnsureVertexBuffers();
  CHECK_EQ(texture.target, static_cast<GLenum>(GL_TEXTURE_2D_ARRAY));
  CHECK_EQ(overlay.texture.target, static_cast<GLenum>(GL_TEXTURE_2D));

  const Program& program = program_array_overlay_;
  glUseProgram(program.program_handle_);

  glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer_);
  glVertexAttribPointer(program.position_handle_, 2, GL_FLOAT, GL_FALSE, 0,
                        nullptr);
  glEnableVertexAttribArray(program.position_handle_);

  BindAndConfigureTexture(GL_TEXTURE0, texture, program.texture_handle_, 0);
  BindAndConfigureTexture(GL_TEXTURE1, overlay.texture,
                          program.overlay_texture_handle_, 1);

  glUniformMatrix4fv(program.uv_transform_, 1, GL_FALSE, &uv_transform[0]);
  glUniform1f(program.opacity_, std::clamp(opacity, 0.0f, 1.0f));
  glUniform1i(program.layer_index_, layer_index);

  const float y_min =
      overlay.flip_y ? (1.0f - overlay.bounds.bottom()) : overlay.bounds.y();
  const float y_max = y_min + overlay.bounds.height();
  glUniform4f(program.overlay_bounds_, overlay.bounds.x(), y_min,
              overlay.bounds.right(), y_max);
  glUniform4f(program.overlay_uv_transform_, kStereoEyeUvWidth,
              overlay.flip_y ? -1.0f : 1.0f, overlay.eye_uv_offset,
              overlay.flip_y ? 1.0f : 0.0f);

  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, index_buffer_);
  glDrawElements(GL_TRIANGLES, std::size(kQuadIndices), GL_UNSIGNED_SHORT,
                 nullptr);

  glDisableVertexAttribArray(program.position_handle_);
  glActiveTexture(GL_TEXTURE0);
}

// Note that we don't explicitly delete gl objects here, they're deleted
// automatically when we call ShutdownGL, and deleting them here leads to
// segfaults.
XrRenderer::~XrRenderer() = default;

}  // namespace device
