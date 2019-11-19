// Copyright 2014 The Chromium Authors. All rights reserved.
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

namespace media {
namespace cast {

double I420PSNR(const media::VideoFrame& frame1,
                const media::VideoFrame& frame2) {
  if (frame1.visible_rect().width() != frame2.visible_rect().width() ||
      frame1.visible_rect().height() != frame2.visible_rect().height())
    return -1;

  return libyuv::I420Psnr(frame1.visible_data(VideoFrame::kYPlane),
                          frame1.stride(VideoFrame::kYPlane),
                          frame1.visible_data(VideoFrame::kUPlane),
                          frame1.stride(VideoFrame::kUPlane),
                          frame1.visible_data(VideoFrame::kVPlane),
                          frame1.stride(VideoFrame::kVPlane),
                          frame2.visible_data(VideoFrame::kYPlane),
                          frame2.stride(VideoFrame::kYPlane),
                          frame2.visible_data(VideoFrame::kUPlane),
                          frame2.stride(VideoFrame::kUPlane),
                          frame2.visible_data(VideoFrame::kVPlane),
                          frame2.stride(VideoFrame::kVPlane),
                          frame1.visible_rect().width(),
                          frame1.visible_rect().height());
}

double I420SSIM(const media::VideoFrame& frame1,
                const media::VideoFrame& frame2) {
  if (frame1.visible_rect().width() != frame2.visible_rect().width() ||
      frame1.visible_rect().height() != frame2.visible_rect().height())
    return -1;

  return libyuv::I420Ssim(frame1.visible_data(VideoFrame::kYPlane),
                          frame1.stride(VideoFrame::kYPlane),
                          frame1.visible_data(VideoFrame::kUPlane),
                          frame1.stride(VideoFrame::kUPlane),
                          frame1.visible_data(VideoFrame::kVPlane),
                          frame1.stride(VideoFrame::kVPlane),
                          frame2.visible_data(VideoFrame::kYPlane),
                          frame2.stride(VideoFrame::kYPlane),
                          frame2.visible_data(VideoFrame::kUPlane),
                          frame2.stride(VideoFrame::kUPlane),
                          frame2.visible_data(VideoFrame::kVPlane),
                          frame2.stride(VideoFrame::kVPlane),
                          frame1.visible_rect().width(),
                          frame1.visible_rect().height());
}

void PopulateVideoFrame(VideoFrame* frame, int start_value) {
  const gfx::Size frame_size = frame->coded_size();
  const int stripe_size =
      std::max(32, std::min(frame_size.width(), frame_size.height()) / 8) & -2;

  // Set Y.
  const int height = frame_size.height();
  const int stride_y = frame->stride(VideoFrame::kYPlane);
  uint8_t* y_plane = frame->data(VideoFrame::kYPlane);
  for (int j = 0; j < height; ++j) {
    const int stripe_j = (j / stripe_size) * stripe_size;
    for (int i = 0; i < stride_y; ++i) {
      const int stripe_i = (i / stripe_size) * stripe_size;
      *y_plane = static_cast<uint8_t>(start_value + stripe_i + stripe_j);
      ++y_plane;
    }
  }

  const int half_height = (height + 1) / 2;
  if (frame->format() == PIXEL_FORMAT_NV12) {
    const int stride_uv = frame->stride(VideoFrame::kUVPlane);
    uint8_t* uv_plane = frame->data(VideoFrame::kUVPlane);

    // Set U and V.
    for (int j = 0; j < half_height; ++j) {
      const int stripe_j = (j / stripe_size) * stripe_size;
      for (int i = 0; i < stride_uv; i += 2) {
        const int stripe_i = (i / stripe_size) * stripe_size;
        *uv_plane = *(uv_plane + 1) =
            static_cast<uint8_t>(start_value + stripe_i + stripe_j);
        uv_plane += 2;
      }
    }
  } else {
    DCHECK(frame->format() == PIXEL_FORMAT_I420 ||
           frame->format() == PIXEL_FORMAT_YV12);
    const int stride_u = frame->stride(VideoFrame::kUPlane);
    const int stride_v = frame->stride(VideoFrame::kVPlane);
    uint8_t* u_plane = frame->data(VideoFrame::kUPlane);
    uint8_t* v_plane = frame->data(VideoFrame::kVPlane);

    // Set U.
    for (int j = 0; j < half_height; ++j) {
      const int stripe_j = (j / stripe_size) * stripe_size;
      for (int i = 0; i < stride_u; ++i) {
        const int stripe_i = (i / stripe_size) * stripe_size;
        *u_plane = static_cast<uint8_t>(start_value + stripe_i + stripe_j);
        ++u_plane;
      }
    }

    // Set V.
    for (int j = 0; j < half_height; ++j) {
      const int stripe_j = (j / stripe_size) * stripe_size;
      for (int i = 0; i < stride_v; ++i) {
        const int stripe_i = (i / stripe_size) * stripe_size;
        *v_plane = static_cast<uint8_t>(start_value + stripe_i + stripe_j);
        ++v_plane;
      }
    }
  }
}

void PopulateVideoFrameWithNoise(VideoFrame* frame) {
  const int height = frame->coded_size().height();
  const int stride_y = frame->stride(VideoFrame::kYPlane);
  const int stride_u = frame->stride(VideoFrame::kUPlane);
  const int stride_v = frame->stride(VideoFrame::kVPlane);
  const int half_height = (height + 1) / 2;
  uint8_t* const y_plane = frame->data(VideoFrame::kYPlane);
  uint8_t* const u_plane = frame->data(VideoFrame::kUPlane);
  uint8_t* const v_plane = frame->data(VideoFrame::kVPlane);

  base::RandBytes(y_plane, height * stride_y);
  base::RandBytes(u_plane, half_height * stride_u);
  base::RandBytes(v_plane, half_height * stride_v);
}

bool PopulateVideoFrameFromFile(VideoFrame* frame, FILE* video_file) {
  const int width = frame->coded_size().width();
  const int height = frame->coded_size().height();
  const int half_width = (width + 1) / 2;
  const int half_height = (height + 1) / 2;
  const size_t frame_size = width * height + 2 * half_width * half_height;
  uint8_t* const y_plane = frame->data(VideoFrame::kYPlane);
  uint8_t* const u_plane = frame->data(VideoFrame::kUPlane);
  uint8_t* const v_plane = frame->data(VideoFrame::kVPlane);

  uint8_t* const raw_data = new uint8_t[frame_size];
  const size_t count = fread(raw_data, 1, frame_size, video_file);
  if (count != frame_size)
    return false;

  memcpy(y_plane, raw_data, width * height);
  memcpy(u_plane, raw_data + width * height, half_width * half_height);
  memcpy(v_plane,
         raw_data + width * height + half_width * half_height,
         half_width * half_height);
  delete[] raw_data;
  return true;
}

}  // namespace cast
}  // namespace media
