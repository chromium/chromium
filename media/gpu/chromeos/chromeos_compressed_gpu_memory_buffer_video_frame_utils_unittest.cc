// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/chromeos/chromeos_compressed_gpu_memory_buffer_video_frame_utils.h"

#include <drm_fourcc.h>

#include "base/time/time.h"
#include "media/base/format_utils.h"
#include "media/base/video_frame.h"
#include "media/gpu/chromeos/fake_chromeos_intel_compressed_gpu_memory_buffer.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

class ChromeOSCompressedGpuMemoryBufferTest
    : public ::testing::TestWithParam<VideoPixelFormat> {};

INSTANTIATE_TEST_SUITE_P(
    All,
    ChromeOSCompressedGpuMemoryBufferTest,
    testing::Values(PIXEL_FORMAT_NV12, PIXEL_FORMAT_P016LE),
    [](const ::testing::TestParamInfo<VideoPixelFormat>& info) {
      return VideoPixelFormatToString(info.param);
    });

TEST_P(ChromeOSCompressedGpuMemoryBufferTest,
       WrapChromeOSCompressedGpuMemoryBufferAsVideoFrame) {
  const VideoPixelFormat pixel_format = GetParam();
  constexpr gfx::Size kCodedSize(256, 256);
  constexpr gfx::Size kNaturalSize(240, 240);
  constexpr gfx::Rect kVisibleRect(kCodedSize);
  constexpr auto kTimestamp = base::Milliseconds(1);
  constexpr uint64_t kModifier = I915_FORMAT_MOD_Y_TILED_GEN12_MC_CCS;
  auto gmb = std::make_unique<FakeChromeOSIntelCompressedGpuMemoryBuffer>(
      kCodedSize, *VideoPixelFormatToGfxBufferFormat(pixel_format));
  FakeChromeOSIntelCompressedGpuMemoryBuffer* gmb_raw_ptr = gmb.get();

  auto frame = WrapChromeOSCompressedGpuMemoryBufferAsVideoFrame(
      kVisibleRect, kNaturalSize, std::move(gmb), kTimestamp);
  ASSERT_TRUE(!!frame);

  constexpr size_t kExpectedNumberOfPlanes = 4u;
  EXPECT_EQ(frame->layout().format(), pixel_format);
  EXPECT_EQ(frame->layout().coded_size(), kCodedSize);
  ASSERT_EQ(frame->layout().num_planes(), kExpectedNumberOfPlanes);
  EXPECT_EQ(frame->layout().is_multi_planar(), false);
  for (size_t i = 0; i < kExpectedNumberOfPlanes; ++i) {
    EXPECT_EQ(frame->layout().planes()[i].stride, gmb_raw_ptr->stride(i));
    EXPECT_EQ(frame->layout().planes()[i].offset, gmb_raw_ptr->plane_offset(i));
    EXPECT_EQ(frame->layout().planes()[i].size, gmb_raw_ptr->plane_size(i));
  }
  EXPECT_EQ(frame->layout().modifier(), kModifier);
  EXPECT_EQ(frame->storage_type(), VideoFrame::STORAGE_GPU_MEMORY_BUFFER);
  ASSERT_TRUE(frame->HasGpuMemoryBuffer());
  EXPECT_EQ(frame->GetGpuMemoryBuffer(), gmb_raw_ptr);
  EXPECT_EQ(frame->coded_size(), kCodedSize);
  EXPECT_EQ(frame->visible_rect(), kVisibleRect);
  EXPECT_EQ(frame->natural_size(), kNaturalSize);
  EXPECT_EQ(frame->timestamp(), kTimestamp);
}

}  // namespace media
