// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_GPU_VIDEO_DECODE_ACCELERATOR_HELPERS_H_
#define MEDIA_GPU_GPU_VIDEO_DECODE_ACCELERATOR_HELPERS_H_

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "media/gpu/media_gpu_export.h"
#include "media/video/supported_video_decoder_config.h"
#include "media/video/video_decode_accelerator.h"

namespace gl {
class GLContext;
class GLImage;
}

namespace gpu {
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

// Bind |image| to |client_texture_id| given |texture_target|. If
// |can_bind_to_sampler| is true, then the image may be used as a sampler
// directly, otherwise a copy to a staging buffer is required.
// Return true on success, false otherwise.
using BindGLImageCallback =
    base::RepeatingCallback<bool(uint32_t client_texture_id,
                                 uint32_t texture_target,
                                 const scoped_refptr<gl::GLImage>& image,
                                 bool can_bind_to_sampler)>;

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

// Convert vector of VDA::SupportedProfile to vector of
// SupportedVideoDecoderConfig.
MEDIA_GPU_EXPORT SupportedVideoDecoderConfigs ConvertFromSupportedProfiles(
    const VideoDecodeAccelerator::SupportedProfiles& profiles,
    bool allow_encrypted);

}  // namespace media

#endif  // MEDIA_GPU_GPU_VIDEO_DECODE_ACCELERATOR_HELPERS_H_
