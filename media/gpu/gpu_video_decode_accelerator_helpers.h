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

namespace media {

// Convert vector of VDA::SupportedProfile to vector of
// SupportedVideoDecoderConfig.
MEDIA_GPU_EXPORT SupportedVideoDecoderConfigs ConvertFromSupportedProfiles(
    const VideoDecodeAccelerator::SupportedProfiles& profiles,
    bool allow_encrypted);

}  // namespace media

#endif  // MEDIA_GPU_GPU_VIDEO_DECODE_ACCELERATOR_HELPERS_H_
