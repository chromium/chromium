// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_ANDROID_VIDEO_ACCELERATOR_UTIL_H_
#define MEDIA_GPU_ANDROID_VIDEO_ACCELERATOR_UTIL_H_

#include <vector>

#include "media/base/video_codecs.h"
#include "media/gpu/media_gpu_export.h"
#include "media/video/video_encode_accelerator.h"
#include "ui/gfx/geometry/size.h"

namespace media {

struct MediaCodecEncoderInfo {
  VideoEncodeAccelerator::SupportedProfile profile;
  std::string name;
};

// Returns information on encoders from the Java MediaCodecList.getCodecInfos()
// function. Initialized once per process. Must not be called from the renderer.
MEDIA_GPU_EXPORT const std::vector<MediaCodecEncoderInfo>&
GetEncoderInfoCache();

struct MediaCodecDecoderInfo {
  VideoCodecProfile profile;
  gfx::Size coded_size_min;
  gfx::Size coded_size_max;
  bool is_software_codec;
};

// Returns information on decoder from the Java MediaCodecList.getCodecInfos()
// function. Initialized once per process. Must not be called from the renderer.
//
// Some devices may not fill out MediaCodecInfo.CodecCapabilities.profileLevels
// correctly. In such cases, the Android platform defaults are assumed:
// https://developer.android.com/guide/topics/media/media-formats
MEDIA_GPU_EXPORT const std::vector<MediaCodecDecoderInfo>&
GetDecoderInfoCache();

}  // namespace media

#endif  // MEDIA_GPU_ANDROID_VIDEO_ACCELERATOR_UTIL_H_
