// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/v4l2/v4l2_video_decoder_backend.h"

#include "base/logging.h"
#include "media/gpu/macros.h"
#include "media/gpu/v4l2/v4l2_device.h"

namespace media {

V4L2VideoDecoderBackend::V4L2VideoDecoderBackend(
    Client* const client,
    scoped_refptr<V4L2Device> device)
    : client_(client), device_(std::move(device)) {
  input_queue_ = device_->GetQueue(V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE);
  output_queue_ = device_->GetQueue(V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE);
  if (!input_queue_ || !output_queue_) {
    VLOGF(1) << "Failed to get V4L2 queue. This should not happen since the "
             << "queues are supposed to be initialized when we are called.";
    NOTREACHED_IN_MIGRATION();
  }
}

V4L2VideoDecoderBackend::~V4L2VideoDecoderBackend() = default;

}  // namespace media
