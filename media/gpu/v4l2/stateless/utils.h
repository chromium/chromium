// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_V4L2_STATELESS_UTILS_H_
#define MEDIA_GPU_V4L2_STATELESS_UTILS_H_

#include <poll.h>

#include "base/functional/callback_helpers.h"
#include "media/gpu/v4l2/stateless/device.h"
#include "media/video/video_decode_accelerator.h"

namespace media {
VideoDecodeAccelerator::SupportedProfiles GetSupportedDecodeProfiles(
    Device* device);
void WaitOnceForEvents(struct pollfd event,
                       base::OnceClosure dequeue_callback,
                       base::OnceClosure error_callback);
std::string IoctlToString(uint64_t request);
}  // namespace media
#endif  // MEDIA_GPU_V4L2_STATELESS_UTILS_H_
