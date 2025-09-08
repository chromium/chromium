// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_ANDROID_VIDEO_FRAME_GL_SURFACE_RENDERER_H_
#define MEDIA_GPU_ANDROID_VIDEO_FRAME_GL_SURFACE_RENDERER_H_

#include <android/native_window.h>

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "gpu/command_buffer/service/memory_tracking.h"
#include "media/base/encoder_status.h"
#include "media/base/video_frame.h"
#include "media/gpu/media_gpu_export.h"
#include "ui/gl/android/scoped_a_native_window.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_surface_egl.h"
#include "ui/gl/scoped_gl_texture.h"

namespace gpu {
class SharedImageManager;
}  // namespace gpu

namespace media {

// This class handles rendering `VideoFrame`s to `ANativeWindow` (Surface) via
// GL API. It sets up the GL context and surface.
class MEDIA_GPU_EXPORT VideoFrameGLSurfaceRenderer {
 public:
  // The constructor takes ownership of the ANativeWindow.
  explicit VideoFrameGLSurfaceRenderer(gl::ScopedANativeWindow window);
  ~VideoFrameGLSurfaceRenderer();

  // Sets up the GLSurface and GLContext. Must be called before any other
  // methods.
  EncoderStatus Initialize();

  // Renders a VideoFrame to the surface. This will fill the surface with the
  // rendered frame, and then call SwapBuffers().
  EncoderStatus RenderVideoFrame(scoped_refptr<VideoFrame> frame,
                                 base::TimeTicks presentation_timestamp);

  void SetSharedImageManager(gpu::SharedImageManager* shared_image_manager);

 private:
  // Renders a YUV VideoFrame to the surface.
  EncoderStatus RenderYUVVideoFrame(scoped_refptr<VideoFrame> frame);

  // Renders an RGB VideoFrame to the surface.
  EncoderStatus RenderRGBVideoFrame(scoped_refptr<VideoFrame> frame);

  // Renders a SharedImage-backed VideoFrame to the surface.
  EncoderStatus RenderSharedImageVideoFrame(scoped_refptr<VideoFrame> frame);

  // Checks if the cached textures are valid for the given frame and recreates
  // them if necessary.
  void UpdateTextures(const VideoFrame& frame);

  // Draws a quad with the currently bound texture and program.
  void DrawQuad();

  void InitializeGL();
  void DestroyGL();

  SEQUENCE_CHECKER(sequence_checker_);

  gl::ScopedANativeWindow window_;
  scoped_refptr<gl::NativeViewGLSurfaceEGL> gl_surface_;
  scoped_refptr<gl::GLContext> gl_context_;

  // Cached GL objects for surface rendering, used to avoid creating them
  // repeatedly for each frame.
  // The GL program for converting YUV to RGB and the shaders that it uses.
  GLuint gl_program_yuv_ = 0;
  // The vertex shader for the program.
  GLuint gl_vertex_shader_ = 0;
  // The fragment shader for the YUV program.
  GLuint gl_fragment_shader_yuv_ = 0;

  // The GL program for rendering RGB and the fragment shader that it uses.
  GLuint gl_program_rgb_ = 0;
  GLuint gl_fragment_shader_rgb_ = 0;

  // The GL program for rendering `AHardwareBuffer`s
  // and the fragment shader that it uses.
  GLuint gl_program_ahb_ = 0;
  GLuint gl_fragment_shader_ahb_ = 0;

  // The vertex buffer object for the quad.
  GLuint gl_vbo_ = 0;
  // The location of the position attribute in the shader.
  GLint gl_pos_location_ = GL_INVALID_INDEX;
  // The location of the texture coordinate attribute in the shader.
  GLint gl_tc_location_ = GL_INVALID_INDEX;
  // The locations of the Y, U, and V texture samplers in the shader.
  std::array<GLint, 3> gl_yuv_tex_locations_{{-1, -1, -1}};
  // The location of the RGB texture sampler in the shader.
  GLint gl_rgb_tex_location_ = GL_INVALID_INDEX;
  // The location of the AHardwareBuffer texture sampler in the shader.
  GLint gl_ahb_tex_location_ = GL_INVALID_INDEX;
  // The location of the YUV to RGB conversion matrix uniform in the shader.
  GLint gl_yuv_to_rgb_matrix_location_ = GL_INVALID_INDEX;
  // shader.
  GLint gl_yuv_to_rgb_translation_location_ = GL_INVALID_INDEX;
  // The location of the boolean uniform that indicates if the input is NV12.
  GLint gl_is_nv12_location_ = GL_INVALID_INDEX;

  // Cached textures to avoid reallocation on every frame.
  std::vector<gl::ScopedGLTexture> cached_textures_;
  // Whether the cached textures are for a SharedImage-backed frame.
  bool cached_textures_for_shared_image_ = false;
  VideoPixelFormat cached_frame_format_ = PIXEL_FORMAT_UNKNOWN;
  gfx::Size cached_frame_size_;

  gpu::MemoryTypeTracker memory_type_tracker_{nullptr};
  raw_ptr<gpu::SharedImageManager> shared_image_manager_;
};

}  // namespace media

#endif  // MEDIA_GPU_ANDROID_VIDEO_FRAME_GL_SURFACE_RENDERER_H_
