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

  // Callback to return current GLContext, if available.
  GetGLContextCallback get_context;

  // Callback for making the relevant context current for GL calls.
  MakeGLContextCurrentCallback make_context_current;

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
