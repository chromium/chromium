// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/v4l2/test/video_decoder.h"

namespace media {
namespace v4l2_test {

VideoDecoder::VideoDecoder(std::unique_ptr<IvfParser> ivf_parser,
                           std::unique_ptr<V4L2IoctlShim> v4l2_ioctl,
                           std::unique_ptr<V4L2Queue> OUTPUT_queue,
                           std::unique_ptr<V4L2Queue> CAPTURE_queue)
    : ivf_parser_(std::move(ivf_parser)),
      v4l2_ioctl_(std::move(v4l2_ioctl)),
      OUTPUT_queue_(std::move(OUTPUT_queue)),
      CAPTURE_queue_(std::move(CAPTURE_queue)) {}

VideoDecoder::~VideoDecoder() = default;

}  // namespace v4l2_test
}  // namespace media
