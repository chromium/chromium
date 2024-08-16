// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/capture/video/linux/v4l2_gpu_memory_buffer_tracker.h"

#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "components/viz/test/test_context_provider.h"
#include "media/capture/video/mock_gpu_memory_buffer_manager.h"
#include "media/capture/video/video_capture_gpu_channel_host.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::A;
using testing::AtLeast;
using testing::AtMost;
using testing::Invoke;
using testing::InvokeWithoutArgs;

namespace media {

class V4l2GpuMemoryBufferTrackerTest : public ::testing::Test {
 public:
  V4l2GpuMemoryBufferTrackerTest() = default;

  V4l2GpuMemoryBufferTrackerTest(const V4l2GpuMemoryBufferTrackerTest&) =
      delete;
  V4l2GpuMemoryBufferTrackerTest& operator=(
      const V4l2GpuMemoryBufferTrackerTest&) = delete;

  void SetUp() override {
    test_sii_ = base::MakeRefCounted<gpu::TestSharedImageInterface>();
  }

  void TearDown() override {
    VideoCaptureGpuChannelHost::GetInstance().SetSharedImageInterface(nullptr);
  }

  void SetSharedImageInterface() {
    VideoCaptureGpuChannelHost::GetInstance().SetSharedImageInterface(
        test_sii_);
  }

 protected:
  scoped_refptr<gpu::TestSharedImageInterface> test_sii_;
};

TEST_F(V4l2GpuMemoryBufferTrackerTest, CreateSuccess) {
  SetSharedImageInterface();
  constexpr gfx::Size dimensions = {1280, 720};

  auto tracker = std::make_unique<V4L2GpuMemoryBufferTracker>();
  EXPECT_TRUE(
      tracker->Init(dimensions, VideoPixelFormat::PIXEL_FORMAT_NV12, nullptr));
}

TEST_F(V4l2GpuMemoryBufferTrackerTest, GetMemorySizeInBytes) {
  SetSharedImageInterface();
  constexpr gfx::Size dimensions = {1280, 720};
  constexpr VideoPixelFormat format = VideoPixelFormat::PIXEL_FORMAT_NV12;
  constexpr size_t byte_size = dimensions.width() * dimensions.height() * 3 / 2;

  auto tracker = std::make_unique<V4L2GpuMemoryBufferTracker>();
  EXPECT_TRUE(tracker->Init(dimensions, format, nullptr));
  EXPECT_EQ(tracker->GetMemorySizeInBytes(), byte_size);
}

TEST_F(V4l2GpuMemoryBufferTrackerTest, InitFailedAsInvalidFormat) {
  constexpr gfx::Size dimensions = {1280, 720};
  constexpr VideoPixelFormat invalid_format =
      VideoPixelFormat::PIXEL_FORMAT_I420;

  auto tracker = std::make_unique<V4L2GpuMemoryBufferTracker>();
  EXPECT_FALSE(tracker->Init(dimensions, invalid_format, nullptr));
}

TEST_F(V4l2GpuMemoryBufferTrackerTest,
       InitFailedAsEmptyTestSharedImageInterface) {
  constexpr gfx::Size dimensions = {1280, 720};
  constexpr VideoPixelFormat format = VideoPixelFormat::PIXEL_FORMAT_NV12;

  auto tracker = std::make_unique<V4L2GpuMemoryBufferTracker>();
  EXPECT_FALSE(tracker->Init(dimensions, format, nullptr));
}

TEST_F(V4l2GpuMemoryBufferTrackerTest, ReusableFormat) {
  SetSharedImageInterface();
  constexpr gfx::Size dimensions = {1280, 720};
  constexpr VideoPixelFormat format = VideoPixelFormat::PIXEL_FORMAT_NV12;

  auto tracker = std::make_unique<V4L2GpuMemoryBufferTracker>();
  EXPECT_TRUE(tracker->Init(dimensions, format, nullptr));
  EXPECT_TRUE(tracker->IsReusableForFormat(dimensions, format, nullptr));
}

TEST_F(V4l2GpuMemoryBufferTrackerTest, NotReusableAsGpuContextLost) {
  SetSharedImageInterface();
  constexpr gfx::Size dimensions = {1280, 720};
  constexpr VideoPixelFormat format = VideoPixelFormat::PIXEL_FORMAT_NV12;

  auto tracker = std::make_unique<V4L2GpuMemoryBufferTracker>();
  EXPECT_TRUE(tracker->Init(dimensions, format, nullptr));

  tracker->OnContextLost();
  EXPECT_FALSE(tracker->IsReusableForFormat(dimensions, format, nullptr));
}

TEST_F(V4l2GpuMemoryBufferTrackerTest, NotReusableAsDiffFormat) {
  SetSharedImageInterface();
  constexpr gfx::Size dimensions = {1280, 720};
  constexpr VideoPixelFormat format = VideoPixelFormat::PIXEL_FORMAT_NV12;
  constexpr VideoPixelFormat diff_format = VideoPixelFormat::PIXEL_FORMAT_I420;

  auto tracker = std::make_unique<V4L2GpuMemoryBufferTracker>();
  EXPECT_TRUE(tracker->Init(dimensions, format, nullptr));
  EXPECT_FALSE(tracker->IsReusableForFormat(dimensions, diff_format, nullptr));
}

}  // namespace media
