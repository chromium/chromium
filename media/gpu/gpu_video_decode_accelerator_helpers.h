// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_GPU_VIDEO_DECODE_ACCELERATOR_HELPERS_H_
#define MEDIA_GPU_GPU_VIDEO_DECODE_ACCELERATOR_HELPERS_H_

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "media/base/supported_video_decoder_config.h"
#include "media/gpu/media_gpu_export.h"
#include "media/video/video_decode_accelerator.h"

namespace gl {
class GLContext;
}

namespace gpu {
class GLImageNativePixmap;

namespace gles2 {
class AbstractTexture;
class ContextGroup;
}
}

namespace media {

// Helpers/defines for specific VideoDecodeAccelerator implementations in GPU
// process. Which callbacks are required depends on the implementation.
//
// Note that these callbacks may be called more than once, and so must own/share
// ownership of any objects bound to them.
//
// Unless specified otherwise, these callbacks must be executed on the GPU Child
// thread (i.e. the thread which the VDAs are initialized on).

// Return current GLContext.
using GetGLContextCallback = base::RepeatingCallback<gl::GLContext*(void)>;

// Make the applicable GL context current. To be called by VDAs before
// executing any GL calls. Return true on success, false otherwise.
using MakeGLContextCurrentCallback = base::RepeatingCallback<bool(void)>;

#if BUILDFLAG(IS_OZONE)
// Bind |image| to |client_texture_id| given |texture_target|, marking the
// texture as not needing binding by the decoder.
// Return true on success, false otherwise.
using BindGLImageCallback = base::RepeatingCallback<bool(
    uint32_t client_texture_id,
    uint32_t texture_target,
    const scoped_refptr<gpu::GLImageNativePixmap>& image)>;
#endif

// Return a ContextGroup*, if one is available.
using GetContextGroupCallback =
    base::RepeatingCallback<gpu::gles2::ContextGroup*(void)>;

// Create and return an AbstractTexture, if possible.
using CreateAbstractTextureCallback =
    base::RepeatingCallback<std::unique_ptr<gpu::gles2::AbstractTexture>(
        unsigned /* GLenum */ target,
        unsigned /* GLenum */ internal_format,
        int /* GLsizei */ width,
        int /* GLsizei */ height,
        int /* GLsizei */ depth,
        int /* GLint */ border,
        unsigned /* GLenum */ format,
        unsigned /* GLenum */ type)>;

// OpenGL callbacks made by VideoDecodeAccelerator sub-classes.
struct MEDIA_GPU_EXPORT GpuVideoDecodeGLClient {
  GpuVideoDecodeGLClient();
  ~GpuVideoDecodeGLClient();
  GpuVideoDecodeGLClient(const GpuVideoDecodeGLClient&);
  GpuVideoDecodeGLClient& operator=(const GpuVideoDecodeGLClient&);

  // Return current GLContext.
  using GetGLContextCallback = base::RepeatingCallback<gl::GLContext*(void)>;

  // Make the applicable GL context current. To be called by VDAs before
  // executing any GL calls. Return true on success, false otherwise.
  using MakeGLContextCurrentCallback = base::RepeatingCallback<bool(void)>;

#if BUILDFLAG(IS_OZONE)
  // Bind |image| to |client_texture_id| given |texture_target|, marking the
  // texture as not needing binding by the decoder.
  // Return true on success, false otherwise.
  using BindGLImageCallback = base::RepeatingCallback<bool(
      uint32_t client_texture_id,
      uint32_t texture_target,
      const scoped_refptr<gpu::GLImageNativePixmap>& image)>;
#endif

  // Return a ContextGroup*, if one is available.
  using GetContextGroupCallback =
      base::RepeatingCallback<gpu::gles2::ContextGroup*(void)>;

  // Callback to return current GLContext, if available.
  GetGLContextCallback get_context;

  // Callback for making the relevant context current for GL calls.
  MakeGLContextCurrentCallback make_context_current;

#if BUILDFLAG(IS_OZONE)
  // Callback to bind a GLImage to a given texture id and target.
  BindGLImageCallback bind_image;
#endif

  // Callback to return a ContextGroup*.
  GetContextGroupCallback get_context_group;

  // Callback to return a DecoderContext*.
  CreateAbstractTextureCallback create_abstract_texture;

  // Whether or not the command buffer is passthrough.
  bool is_passthrough = false;

  // Whether or not ARB_texture_rectangle is present.
  bool supports_arb_texture_rectangle = false;
};

// Convert vector of VDA::SupportedProfile to vector of
// SupportedVideoDecoderConfig.
MEDIA_GPU_EXPORT SupportedVideoDecoderConfigs ConvertFromSupportedProfiles(
    const VideoDecodeAccelerator::SupportedProfiles& profiles,
    bool allow_encrypted);

}  // namespace media

#endif  // MEDIA_GPU_GPU_VIDEO_DECODE_ACCELERATOR_HELPERS_H_
