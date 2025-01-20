// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cast/test/utility/video_utility.h"

#include <math.h>
#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <cstdio>

#include "base/rand_util.h"
#include "third_party/libyuv/include/libyuv/compare.h"
#include "ui/gfx/geometry/size.h"

namespace media::cast {

void FillPlane(VideoFrame& frame,
               VideoFrame::Plane plane,
               int height,
               int stripe_size,
               int start_value) {
  const int stride = frame.stride(plane);
  base::span<uint8_t> plane_to_fill = frame.GetWritableVisiblePlaneData(plane);

  for (int j = 0; j < height; ++j) {
    const int stripe_j = (j / stripe_size) * stripe_size;
    for (int i = 0; i < stride; ++i) {
      const int stripe_i = (i / stripe_size) * stripe_size;
      // Special case:: if this is the UV plane, odd indices should be set to
      // the same value as the even index before them.
      if (plane == VideoFrame::Plane::kUV && i % 2 == 1) {
        plane_to_fill[j * stride + i] = plane_to_fill[j * stride + i - 1];
      } else {
        plane_to_fill[j * stride + i] =
            static_cast<uint8_t>(start_value + stripe_i + stripe_j);
      }
    }
  }
}

void PopulateVideoFrame(VideoFrame* frame, int start_value) {
  const gfx::Size frame_size = frame->coded_size();
  const int stripe_size =
      std::max(32, std::min(frame_size.width(), frame_size.height()) / 8) & -2;

  // Set Y.
  const int height = frame_size.height();
  FillPlane(*frame, VideoFrame::Plane::kY, height, stripe_size, start_value);

  const int half_height = (height + 1) / 2;
  if (frame->format() == PIXEL_FORMAT_NV12) {
    FillPlane(*frame, VideoFrame::Plane::kUV, half_height, stripe_size,
              start_value);
  } else {
    CHECK(frame->format() == PIXEL_FORMAT_I420 ||
          frame->format() == PIXEL_FORMAT_YV12);
    FillPlane(*frame, VideoFrame::Plane::kU, half_height, stripe_size,
              start_value);
    FillPlane(*frame, VideoFrame::Plane::kV, half_height, stripe_size,
              start_value);
  }
}

void PopulateVideoFrameWithNoise(VideoFrame* frame) {
  for (auto plane :
       {VideoFrame::Plane::kY, VideoFrame::Plane::kU, VideoFrame::Plane::kV}) {
    base::RandBytes(frame->GetWritableVisiblePlaneData(plane));
  }
}
}  // namespace media::cast
