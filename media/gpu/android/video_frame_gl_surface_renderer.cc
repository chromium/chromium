// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/android/video_frame_gl_surface_renderer.h"

#include <array>
#include <string_view>

#include "base/android/requires_api.h"
#include "base/strings/stringprintf.h"
#include "third_party/skia/include/effects/SkColorMatrix.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_surface.h"
#include "ui/gl/gl_surface_egl.h"
#include "ui/gl/init/gl_factory.h"
#include "ui/gl/scoped_make_current.h"

namespace media {

namespace {

// Create shader program for YUV to RGB conversion.
constexpr std::string_view kVertexShaderSource =
    R"*(
    attribute vec4 a_position;
    attribute vec2 a_texCoord;
    varying vec2 v_texCoord;
    void main() {
      gl_Position = a_position;
      v_texCoord = a_texCoord;
    }
    )*";

constexpr auto kVertexShaderSourcePtr =
    std::to_array<const char*>({kVertexShaderSource.data()});
constexpr std::string_view kFragmentShaderYUVSource =
    R"*(
    precision mediump float;
    varying vec2 v_texCoord;
    uniform sampler2D s_texture_y;
    uniform sampler2D s_texture_u_or_uv;
    uniform sampler2D s_texture_v;
    uniform bool u_is_nv12;
    uniform mat3 yuv_to_rgb_matrix;
    uniform vec3 yuv_to_rgb_translation;
    void main() {
      float y = texture2D(s_texture_y, v_texCoord).r;
      float u, v;
      if (u_is_nv12) {
        vec2 uv = texture2D(s_texture_u_or_uv, v_texCoord).rg;
        u = uv.x;
        v = uv.y;
      } else {
        u = texture2D(s_texture_u_or_uv, v_texCoord).r;
        v = texture2D(s_texture_v, v_texCoord).r;
      }
      vec3 rgb = yuv_to_rgb_matrix * vec3(y, u, v);
      gl_FragColor = vec4(rgb + yuv_to_rgb_translation, 1.0);
    }
    )*";
constexpr auto kFragmentShaderSourcePtr =
    std::to_array<const char*>({kFragmentShaderYUVSource.data()});

// The vertices of a quad, and the texture coordinates corresponding to each
// vertex. The texture coordinates are chosen to render the frame upright when
// the surface origin is at the top-left.
constexpr GLfloat kVerticesTopLeftOrigin[] = {
    -1.0f, 1.0f,
    0.0f,  1.0f,  // Top-left vertex, bottom-left texture coordinate.
    -1.0f, -1.0f,
    0.0f,  0.0f,  // Bottom-left vertex, top-left texture coord.
    1.0f,  1.0f,
    1.0f,  1.0f,  // Top-right vertex, bottom-right texture coordinate.
    1.0f,  -1.0f,
    1.0f,  0.0f,  // Bottom-right vertex, top-right texture coord.
};

// Vertices with flipped texture coordinates for rendering upright when the
// surface origin is at the bottom-left.
constexpr GLfloat kVerticesBottomLeftOrigin[] = {
    -1.0f, 1.0f,
    0.0f,  0.0f,  // Top-left vertex, top-left texture coordinate.
    -1.0f, -1.0f,
    0.0f,  1.0f,  // Bottom-left vertex, bottom-left texture coord.
    1.0f,  1.0f,
    1.0f,  0.0f,  // Top-right vertex, top-right texture coordinate.
    1.0f,  -1.0f,
    1.0f,  1.0f,  // Bottom-right vertex, bottom-right texture coord.
};

}  // namespace

VideoFrameGLSurfaceRenderer::VideoFrameGLSurfaceRenderer(
    gl::ScopedANativeWindow window)
    : window_(std::move(window)) {}

VideoFrameGLSurfaceRenderer::~VideoFrameGLSurfaceRenderer() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DestroyGL();
}

EncoderStatus VideoFrameGLSurfaceRenderer::Initialize() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  gl::GLDisplayEGL* display = gl::GLSurfaceEGL::GetGLDisplayEGL();
  if (!display) {
    return {EncoderStatus::Codes::kEncoderInitializationError,
            "gl::GetDefaultDisplayEGL failed"};
  }

  gl_surface_ = base::MakeRefCounted<gl::NativeViewGLSurfaceEGL>(
      display->GetAs<gl::GLDisplayEGL>(), std::move(window_), nullptr, true);
  if (!gl_surface_->Initialize(gl::GLSurfaceFormat())) {
    return {EncoderStatus::Codes::kEncoderInitializationError,
            "GLSurface Initialize failed"};
  }

  gl_context_ = gl::init::CreateGLContext(nullptr, gl_surface_.get(),
                                          gl::GLContextAttribs());
  if (!gl_context_) {
    return {EncoderStatus::Codes::kEncoderInitializationError,
            "gl::init::CreateGLContext failed"};
  }

  ui::ScopedMakeCurrent smc(gl_context_.get(), gl_surface_.get());
  if (!smc.IsContextCurrent()) {
    return {EncoderStatus::Codes::kEncoderInitializationError,
            "gl::GLContext::MakeCurrent() failed"};
  }

  InitializeGL();

  const GLenum error = glGetError();
  if (error != GL_NO_ERROR) {
    DLOG(ERROR) << "GL initialization error: " << error;
    return {EncoderStatus::Codes::kEncoderInitializationError,
            base::StringPrintf("GL initialization error: 0x%x", error)};
  }

  return EncoderStatus::Codes::kOk;
}

EncoderStatus VideoFrameGLSurfaceRenderer::RenderVideoFrame(
    scoped_refptr<VideoFrame> frame,
    base::TimeTicks presentation_timestamp) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  ui::ScopedMakeCurrent smc(gl_context_.get(), gl_surface_.get());
  if (!smc.IsContextCurrent()) {
    return {EncoderStatus::Codes::kSystemAPICallError,
            "gl::GLContext::MakeCurrent() failed"};
  }

  gl::GLApi* api = gl::g_current_gl_context;
  api->glClearFn(GL_COLOR_BUFFER_BIT);

  if (!frame->IsMappable()) {
    return {EncoderStatus::Codes::kUnsupportedFrameFormat,
            "Frame is not mappable"};
  }

  switch (frame->format()) {
    case PIXEL_FORMAT_I420:
    case PIXEL_FORMAT_NV12:
      if (auto status = RenderYUVVideoFrame(frame); !status.is_ok()) {
        return status;
      }
      break;
    default:
      return {EncoderStatus::Codes::kUnsupportedFrameFormat,
              "Unsupported frame format: " +
                  VideoPixelFormatToString(frame->format())};
  }

  const GLenum error = api->glGetErrorFn();
  if (error != GL_NO_ERROR) {
    DLOG(ERROR) << "GL error: " << error;
    return {EncoderStatus::Codes::kSystemAPICallError,
            base::StringPrintf("GL error: 0x%x", error)};
  }

  gl_surface_->SetPresentationTimestamp(presentation_timestamp);

  // SwapBuffers submits the rendered frame to the surface.
  if (gl_surface_->SwapBuffers(base::DoNothing(), gfx::FrameData()) ==
      gfx::SwapResult::SWAP_FAILED) {
    return {EncoderStatus::Codes::kSystemAPICallError,
            "GL surface SwapBuffers failed"};
  }
  return EncoderStatus::Codes::kOk;
}

EncoderStatus VideoFrameGLSurfaceRenderer::RenderYUVVideoFrame(
    scoped_refptr<VideoFrame> frame) {
  gl::GLApi* api = gl::g_current_gl_context;
  const bool is_nv12 = frame->format() == PIXEL_FORMAT_NV12;
  const size_t num_textures = is_nv12 ? 2 : 3;
  const gfx::Size frame_size = frame->visible_rect().size();

  if (cached_yuv_textures_.empty() || cached_frame_format_ != frame->format() ||
      cached_frame_size_ != frame_size) {
    cached_yuv_textures_.clear();

    cached_frame_format_ = frame->format();
    cached_frame_size_ = frame_size;

    std::vector<GLuint> texture_handles(num_textures);
    api->glGenTexturesFn(num_textures, texture_handles.data());
    for (GLuint handle : texture_handles) {
      cached_yuv_textures_.emplace_back(handle);
    }

    for (size_t i = 0; i < num_textures; ++i) {
      api->glActiveTextureFn(GL_TEXTURE0 + i);
      api->glBindTextureFn(GL_TEXTURE_2D, cached_yuv_textures_[i].get());

      api->glTexParameteriFn(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
      api->glTexParameteriFn(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
      api->glTexParameteriFn(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S,
                             GL_CLAMP_TO_EDGE);
      api->glTexParameteriFn(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T,
                             GL_CLAMP_TO_EDGE);

      auto plane = static_cast<VideoFrame::Plane>(i);
      gfx::Size plane_size = VideoFrame::PlaneSizeInSamples(
          frame->format(), plane, frame->visible_rect().size());
      GLenum gl_format = is_nv12 && i == 1 ? GL_RG : GL_RED;
      api->glTexImage2DFn(GL_TEXTURE_2D, 0, gl_format, plane_size.width(),
                          plane_size.height(), 0, gl_format, GL_UNSIGNED_BYTE,
                          nullptr);
    }
  }

  for (size_t i = 0; i < num_textures; ++i) {
    api->glActiveTextureFn(GL_TEXTURE0 + i);
    api->glBindTextureFn(GL_TEXTURE_2D, cached_yuv_textures_[i].get());

    auto plane = static_cast<VideoFrame::Plane>(i);
    gfx::Size plane_size = VideoFrame::PlaneSizeInSamples(
        frame->format(), plane, frame->visible_rect().size());
    const void* pixels = frame->visible_data(plane);

    // For NV12, the second plane contains interleaved U and V data. We
    // upload this as a two-channel GL_RG texture. The shader can then
    // access U as the 'r' component and V as the 'g' component. For all
    // other planes (Y, and U/V in I420), we use a single-channel
    // GL_RED texture.
    GLenum gl_format = is_nv12 && i == 1 ? GL_RG : GL_RED;

    // GL_UNPACK_ROW_LENGTH is specified in pixels, not bytes.
    const size_t stride_bytes = frame->stride(plane);
    GLint unpack_row_length_pixels;
    if (gl_format == GL_RG) {
      if (stride_bytes % 2 != 0) {
        return {EncoderStatus::Codes::kUnsupportedFrameFormat,
                base::StringPrintf("Unsupported stride: %d", stride_bytes)};
      }
      // Each pixel is 2 bytes (GL_RG).
      unpack_row_length_pixels = stride_bytes / 2;
    } else {
      // Each pixel is 1 byte (GL_RED).
      unpack_row_length_pixels = stride_bytes;
    }
    api->glPixelStoreiFn(GL_UNPACK_ROW_LENGTH, unpack_row_length_pixels);
    api->glPixelStoreiFn(GL_UNPACK_ALIGNMENT, 1);

    api->glTexSubImage2DFn(GL_TEXTURE_2D, 0, 0, 0, plane_size.width(),
                           plane_size.height(), gl_format, GL_UNSIGNED_BYTE,
                           pixels);
  }

  // Start the rendering process.
  api->glUseProgramFn(gl_program_);

  // Inform the shader whether we are processing an NV12 frame. This allows
  // the shader to use the correct sampling logic.
  api->glUniform1iFn(gl_is_nv12_location_, is_nv12);

  // Get the color space from the VideoFrame to ensure color-correct
  // conversion.
  SkYUVColorSpace yuv_color_space;
  gfx::ColorSpace frame_color_space = frame->ColorSpace();
  if (!frame_color_space.IsValid()) {
    frame_color_space = gfx::ColorSpace::CreateREC601();
  }
  frame_color_space.ToSkYUVColorSpace(&yuv_color_space);
  const auto yuv_to_rgb_matrix = SkColorMatrix::YUVtoRGB(yuv_color_space);

  std::array<float, 20> yuv_to_rgb_matrix_array;
  yuv_to_rgb_matrix.getRowMajor(yuv_to_rgb_matrix_array.data());

  // The Skia matrix includes a translation vector in the 4th column, which
  // we extract and pass to the shader separately.
  const std::array<float, 3> yuv_to_rgb_translation = {
      yuv_to_rgb_matrix_array[4], yuv_to_rgb_matrix_array[9],
      yuv_to_rgb_matrix_array[14]};

  // The matrix passed to the shader is 3x3, but SkColorMatrix is a 4x5
  // matrix. We need to extract the 3x3 part.
  const std::array<float, 9> yuv_to_rgb_matrix_3x3 = {
      yuv_to_rgb_matrix_array[0],  yuv_to_rgb_matrix_array[1],
      yuv_to_rgb_matrix_array[2],  yuv_to_rgb_matrix_array[5],
      yuv_to_rgb_matrix_array[6],  yuv_to_rgb_matrix_array[7],
      yuv_to_rgb_matrix_array[10], yuv_to_rgb_matrix_array[11],
      yuv_to_rgb_matrix_array[12]};

  // Pass the conversion matrix and translation vector to the shader.
  // We must transpose the matrix because Skia matrices are row-major, while
  // OpenGL expects column-major matrices by default.
  api->glUniformMatrix3fvFn(gl_yuv_to_rgb_matrix_location_, 1, GL_TRUE,
                            yuv_to_rgb_matrix_3x3.data());
  api->glUniform3fvFn(gl_yuv_to_rgb_translation_location_, 1,
                      yuv_to_rgb_translation.data());

  // Bind the texture samplers to their respective texture units.
  api->glUniform1iFn(gl_tex_locations_[0], 0);
  api->glUniform1iFn(gl_tex_locations_[1], 1);
  if (!is_nv12) {
    api->glUniform1iFn(gl_tex_locations_[2], 2);
  }

  // Set up the vertex buffer and attribute pointers for the quad.
  api->glBindBufferFn(GL_ARRAY_BUFFER, gl_vbo_);
  api->glVertexAttribPointerFn(gl_pos_location_, 2, GL_FLOAT, GL_FALSE,
                               4 * sizeof(GLfloat), nullptr);
  api->glVertexAttribPointerFn(
      gl_tc_location_, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat),
      reinterpret_cast<const void*>(2 * sizeof(GLfloat)));
  api->glEnableVertexAttribArrayFn(gl_pos_location_);
  api->glEnableVertexAttribArrayFn(gl_tc_location_);

  // Draw the quad, which triggers the fragment shader to run for each pixel,
  // performing the YUV to RGB conversion.
  api->glDrawArraysFn(GL_TRIANGLE_STRIP, 0, 4);

  // Cleanup per-frame objects.
  api->glDisableVertexAttribArrayFn(gl_pos_location_);
  api->glDisableVertexAttribArrayFn(gl_tc_location_);
  return EncoderStatus::Codes::kOk;
}

void VideoFrameGLSurfaceRenderer::InitializeGL() {
  if (gl_program_) {
    return;
  }

  gl::GLApi* api = gl::g_current_gl_context;
  gl_vertex_shader_ = api->glCreateShaderFn(GL_VERTEX_SHADER);
  api->glShaderSourceFn(gl_vertex_shader_, 1, kVertexShaderSourcePtr.data(),
                        nullptr);
  api->glCompileShaderFn(gl_vertex_shader_);

  gl_fragment_shader_ = api->glCreateShaderFn(GL_FRAGMENT_SHADER);
  api->glShaderSourceFn(gl_fragment_shader_, 1, kFragmentShaderSourcePtr.data(),
                        nullptr);
  api->glCompileShaderFn(gl_fragment_shader_);

  gl_program_ = api->glCreateProgramFn();
  api->glAttachShaderFn(gl_program_, gl_vertex_shader_);
  api->glAttachShaderFn(gl_program_, gl_fragment_shader_);
  api->glLinkProgramFn(gl_program_);

  gl_pos_location_ = api->glGetAttribLocationFn(gl_program_, "a_position");
  gl_tc_location_ = api->glGetAttribLocationFn(gl_program_, "a_texCoord");
  gl_tex_locations_[0] =
      api->glGetUniformLocationFn(gl_program_, "s_texture_y");
  gl_tex_locations_[1] =
      api->glGetUniformLocationFn(gl_program_, "s_texture_u_or_uv");
  gl_tex_locations_[2] =
      api->glGetUniformLocationFn(gl_program_, "s_texture_v");
  gl_yuv_to_rgb_matrix_location_ =
      api->glGetUniformLocationFn(gl_program_, "yuv_to_rgb_matrix");
  gl_yuv_to_rgb_translation_location_ =
      api->glGetUniformLocationFn(gl_program_, "yuv_to_rgb_translation");
  gl_is_nv12_location_ = api->glGetUniformLocationFn(gl_program_, "u_is_nv12");

  // Set up vertices for a quad.
  api->glGenBuffersARBFn(1, &gl_vbo_);
  api->glBindBufferFn(GL_ARRAY_BUFFER, gl_vbo_);

  if (gl_surface_->GetOrigin() == gfx::SurfaceOrigin::kTopLeft) {
    api->glBufferDataFn(GL_ARRAY_BUFFER, sizeof(kVerticesTopLeftOrigin),
                        kVerticesTopLeftOrigin, GL_STATIC_DRAW);
  } else {
    api->glBufferDataFn(GL_ARRAY_BUFFER, sizeof(kVerticesBottomLeftOrigin),
                        kVerticesBottomLeftOrigin, GL_STATIC_DRAW);
  }
}

void VideoFrameGLSurfaceRenderer::DestroyGL() {
  if (!gl_program_) {
    return;
  }

  ui::ScopedMakeCurrent smc(gl_context_.get(), gl_surface_.get());
  gl::GLApi* api = gl::g_current_gl_context;
  api->glDeleteProgramFn(gl_program_);
  api->glDeleteShaderFn(gl_vertex_shader_);
  api->glDeleteShaderFn(gl_fragment_shader_);
  api->glDeleteBuffersARBFn(1, &gl_vbo_);

  cached_yuv_textures_.clear();

  gl_program_ = 0;
  gl_vertex_shader_ = 0;
  gl_fragment_shader_ = 0;
  gl_vbo_ = 0;
  gl_pos_location_ = GL_INVALID_INDEX;
  gl_tc_location_ = GL_INVALID_INDEX;
  gl_tex_locations_.fill(GL_INVALID_INDEX);
}

}  // namespace media
