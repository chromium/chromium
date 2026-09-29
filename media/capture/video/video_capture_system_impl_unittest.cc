// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/capture/video/video_capture_system_impl.h"

#include <vector>

#include "media/capture/video_capture_types.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/size.h"

namespace media {

TEST(VideoCaptureSystemImplTest,
     ConsolidateCaptureFormatsDeduplicatesIdenticalFormats) {
  VideoCaptureFormats formats = {
      VideoCaptureFormat(gfx::Size(1920, 1080), 30.0f, PIXEL_FORMAT_I420),
      VideoCaptureFormat(gfx::Size(1920, 1080), 30.0f, PIXEL_FORMAT_I420),
  };
  VideoCaptureSystemImpl::ConsolidateCaptureFormatsForTesting(&formats);
  EXPECT_EQ(formats.size(), 1u);
}

TEST(VideoCaptureSystemImplTest,
     ConsolidateCaptureFormatsDeduplicatesNormalizedFormatsSeparatedByNV12) {
  // A device reporting MJPEG, NV12, and YUY2 at the same resolution and frame
  // rate. MJPEG and YUY2 will both be normalized to I420.
  VideoCaptureFormats formats = {
      VideoCaptureFormat(gfx::Size(1920, 1080), 30.0f, PIXEL_FORMAT_MJPEG),
      VideoCaptureFormat(gfx::Size(1920, 1080), 30.0f, PIXEL_FORMAT_NV12),
      VideoCaptureFormat(gfx::Size(1920, 1080), 30.0f, PIXEL_FORMAT_YUY2),
  };

  VideoCaptureSystemImpl::ConsolidateCaptureFormatsForTesting(&formats);

  // We expect exactly 2 formats: one NV12 and one I420.
  EXPECT_THAT(
      formats,
      ::testing::UnorderedElementsAre(
          VideoCaptureFormat(gfx::Size(1920, 1080), 30.0f, PIXEL_FORMAT_I420),
          VideoCaptureFormat(gfx::Size(1920, 1080), 30.0f, PIXEL_FORMAT_NV12)));
}

TEST(VideoCaptureSystemImplTest,
     ConsolidateCaptureFormatsDeduplicatesIdenticalNV12SeparatedByI420) {
  VideoCaptureFormats formats = {
      VideoCaptureFormat(gfx::Size(1920, 1080), 30.0f, PIXEL_FORMAT_NV12),
      VideoCaptureFormat(gfx::Size(1920, 1080), 30.0f, PIXEL_FORMAT_I420),
      VideoCaptureFormat(gfx::Size(1920, 1080), 30.0f, PIXEL_FORMAT_NV12),
  };

  VideoCaptureSystemImpl::ConsolidateCaptureFormatsForTesting(&formats);

  // We expect exactly 2 formats: one NV12 and one I420.
  EXPECT_THAT(
      formats,
      ::testing::UnorderedElementsAre(
          VideoCaptureFormat(gfx::Size(1920, 1080), 30.0f, PIXEL_FORMAT_I420),
          VideoCaptureFormat(gfx::Size(1920, 1080), 30.0f, PIXEL_FORMAT_NV12)));
}

}  // namespace media
