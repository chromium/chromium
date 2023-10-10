// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_V4L2_STATELESS_UTILS_H_
#define MEDIA_GPU_V4L2_STATELESS_UTILS_H_

#include "media/gpu/v4l2/stateless/device.h"
#include "media/video/video_decode_accelerator.h"

namespace media {
VideoDecodeAccelerator::SupportedProfiles GetSupportedDecodeProfiles(
    Device* device);
}  // namespace media
#endif  // MEDIA_GPU_V4L2_STATELESS_UTILS_H_
