// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/gpu_video_encode_accelerator_helpers.h"

#include <algorithm>

#include "base/check_op.h"
#include "base/notreached.h"

namespace media {
namespace {
// The maximum size for output buffer, which is chosen empirically for
// 1080p video.
constexpr size_t kMaxBitstreamBufferSizeInBytes = 2 * 1024 * 1024;  // 2MB

// The frame size for 1080p (FHD) video in pixels.
constexpr int k1080PSizeInPixels = 1920 * 1080;
// The frame size for 1440p (QHD) video in pixels.
constexpr int k1440PSizeInPixels = 2560 * 1440;

// The mapping from resolution, bitrate, framerate to the bitstream buffer size.
struct BitstreamBufferSizeInfo {
  int coded_size_area;
  uint32_t bitrate_in_bps;
  uint32_t framerate;
  uint32_t buffer_size_in_bytes;
};

// The bitstream buffer size for each resolution. The table must be sorted in
// increasing order by the resolution. The value is decided by measuring the
// biggest buffer size, and then double the size as margin. (crbug.com/889739)
constexpr BitstreamBufferSizeInfo kBitstreamBufferSizeTable[] = {
    {320 * 180, 100000, 30, 15000},
    {640 * 360, 500000, 30, 52000},
    {1280 * 720, 1200000, 30, 110000},
    {1920 * 1080, 4000000, 30, 380000},
    {3840 * 2160, 20000000, 30, 970000},
};

// Use quadruple size of kMaxBitstreamBufferSizeInBytes when the input frame
// size is larger than 1440p, double if larger than 1080p. This is chosen
// empirically for some 4k encoding use cases and Android CTS VideoEncoderTest
// (crbug.com/927284).
size_t GetMaxEncodeBitstreamBufferSize(const gfx::Size& size) {
  if (size.GetArea() > k1440PSizeInPixels)
    return kMaxBitstreamBufferSizeInBytes * 4;
  if (size.GetArea() > k1080PSizeInPixels)
    return kMaxBitstreamBufferSizeInBytes * 2;
  return kMaxBitstreamBufferSizeInBytes;
}

}  // namespace

size_t GetEncodeBitstreamBufferSize(const gfx::Size& size,
                                    uint32_t bitrate,
                                    uint32_t framerate) {
  DCHECK_NE(framerate, 0u);
  for (auto& data : kBitstreamBufferSizeTable) {
    if (size.GetArea() <= data.coded_size_area) {
      // The buffer size is proportional to (bitrate / framerate), but linear
      // interpolation for smaller ratio is not enough. Therefore we only use
      // linear extrapolation for larger ratio.
      double ratio = std::max(
          1.0f * (bitrate / framerate) / (data.bitrate_in_bps / data.framerate),
          1.0f);
      return std::min(static_cast<size_t>(data.buffer_size_in_bytes * ratio),
                      GetMaxEncodeBitstreamBufferSize(size));
    }
  }
  return GetMaxEncodeBitstreamBufferSize(size);
}

// Get the maximum output bitstream buffer size. Since we don't change the
// buffer size when we update bitrate and framerate, we have to calculate the
// buffer size for the maximum bitrate.
// However, the maximum bitrate for intel chipset is 40Mbps. The buffer size
// calculated with this bitrate is always larger than 2MB. Therefore we just
// return the value.
// TODO(crbug.com/889739): Deprecate this function after we can update the
// buffer size while requesting new bitrate and framerate.
size_t GetEncodeBitstreamBufferSize(const gfx::Size& size) {
  return GetMaxEncodeBitstreamBufferSize(size);
}

std::vector<uint8_t> GetFpsAllocation(size_t num_temporal_layers) {
  DCHECK_LT(num_temporal_layers, 4u);
  constexpr uint8_t kFullAllocation = 255;
  // The frame rate fraction is given as an 8 bit unsigned integer where 0 = 0%
  // and 255 = 100%. Each layer's allocated fps refers to the previous one, so
  // e.g. your camera is opened at 30fps, and you want to have decode targets at
  // 15fps and 7.5fps as well:
  // TL0 then gets an allocation of 7.5/30 = 1/4. TL1 adds another 7.5fps to end
  // up at (7.5 + 7.5)/30 = 15/30 = 1/2 of the total allocation. TL2 adds the
  // final 15fps to end up at (15 + 15)/30, which is the full allocation.
  // Therefor, fps_allocation values are as follows,
  // fps_allocation[0][0] = kFullAllocation / 4;
  // fps_allocation[0][1] = kFullAllocation / 2;
  // fps_allocation[0][2] = kFullAllocation;
  //  For more information, see webrtc::VideoEncoderInfo::fps_allocation.
  switch (num_temporal_layers) {
    case 1:
      // In this case, the number of spatial layers must great than 1.
      return {kFullAllocation};
    case 2:
      return {kFullAllocation / 2, kFullAllocation};
    case 3:
      return {kFullAllocation / 4, kFullAllocation / 2, kFullAllocation};
    default:
      NOTREACHED() << "Unsupported temporal layers";
      return {};
  }
}

}  // namespace media
