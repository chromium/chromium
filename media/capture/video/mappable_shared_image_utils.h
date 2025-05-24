// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAPTURE_VIDEO_MAPPABLE_SHARED_IMAGE_UTILS_H_
#define MEDIA_CAPTURE_VIDEO_MAPPABLE_SHARED_IMAGE_UTILS_H_

#include "media/capture/video/video_capture_device.h"

namespace media {

VideoCaptureDevice::Client::ReserveResult AllocateNV12SharedImage(
    VideoCaptureDevice::Client* capture_client,
    const gfx::Size& buffer_size,
    scoped_refptr<gpu::ClientSharedImage>* out_shared_image,
    VideoCaptureDevice::Client::Buffer* out_capture_buffer);

}  // namespace media

#endif  // MEDIA_CAPTURE_VIDEO_MAPPABLE_SHARED_IMAGE_UTILS_H_
