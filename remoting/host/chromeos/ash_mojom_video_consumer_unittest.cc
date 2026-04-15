// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/chromeos/ash_mojom_video_consumer.h"

#include <memory>

#include "base/memory/read_only_shared_memory_region.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "media/base/video_frame.h"
#include "media/capture/mojom/video_capture_buffer.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_frame.h"

namespace remoting {

namespace {

class MockFrameCallbacks
    : public viz::mojom::FrameSinkVideoConsumerFrameCallbacks {
 public:
  MockFrameCallbacks() = default;
  ~MockFrameCallbacks() override = default;

  MOCK_METHOD(void, Done, (), (override));
  MOCK_METHOD(void,
              ProvideFeedback,
              (const media::VideoCaptureFeedback&),
              (override));

  mojo::PendingRemote<viz::mojom::FrameSinkVideoConsumerFrameCallbacks>
  BindNewPipeAndPassRemote() {
    return receiver_.BindNewPipeAndPassRemote();
  }

 private:
  mojo::Receiver<viz::mojom::FrameSinkVideoConsumerFrameCallbacks> receiver_{
      this};
};

}  // namespace

class AshMojomVideoConsumerTest : public testing::Test {
 public:
  AshMojomVideoConsumerTest() = default;
  ~AshMojomVideoConsumerTest() override = default;

 protected:
  base::test::TaskEnvironment task_environment_;
};

TEST_F(AshMojomVideoConsumerTest, CreateSkBitmapWithUnsupportedPixelFormat) {
  AshMojomVideoConsumer consumer;

  // Create malicious parameters:
  // Use I420 format which has 1.5 bytes per pixel.
  // coded_size is 100x100, so allocation size is ~15000 bytes.
  auto info = media::mojom::VideoFrameInfo::New();
  info->pixel_format = media::VideoPixelFormat::PIXEL_FORMAT_I420;
  info->coded_size = gfx::Size(100, 100);
  info->visible_rect = gfx::Rect(0, 0, 100, 100);

  size_t shared_memory_size =
      media::VideoFrame::AllocationSize(info->pixel_format, info->coded_size);
  auto region = base::ReadOnlySharedMemoryRegion::Create(shared_memory_size);
  ASSERT_TRUE(region.IsValid());

  media::mojom::VideoBufferHandlePtr data =
      media::mojom::VideoBufferHandle::NewReadOnlyShmemRegion(
          std::move(region.region));

  // content_rect is 100x100.
  // CreateSkBitmap would allocate N32 bitmap (4 bytes per pixel) for 100x100,
  // which is 40000 bytes.
  // Then it would memcpy 40000 bytes from the 15000 bytes shared memory.
  gfx::Rect content_rect(0, 0, 100, 100);

  base::test::TestFuture<void> future;
  MockFrameCallbacks callbacks;
  EXPECT_CALL(callbacks, Done()).WillOnce(base::test::InvokeFuture(future));

  static_cast<viz::mojom::FrameSinkVideoConsumer&>(consumer).OnFrameCaptured(
      std::move(data), std::move(info), content_rect,
      callbacks.BindNewPipeAndPassRemote());
  EXPECT_TRUE(future.Wait());

  // This should have been rejected by IsValidFrame() in OnFrameCaptured.
  auto frame = consumer.GetLatestFrame(gfx::Point(0, 0));
  EXPECT_EQ(frame, nullptr);
}

TEST_F(AshMojomVideoConsumerTest,
       CreateSkBitmapWithUnsupportedSinglePlanePixelFormat) {
  AshMojomVideoConsumer consumer;

  // Create malicious parameters:
  // RGB24 is single-plane but has 3 bytes per pixel, not 4.
  auto info = media::mojom::VideoFrameInfo::New();
  info->pixel_format = media::VideoPixelFormat::PIXEL_FORMAT_RGB24;
  info->coded_size = gfx::Size(100, 100);
  info->visible_rect = gfx::Rect(0, 0, 100, 100);

  size_t shared_memory_size =
      media::VideoFrame::AllocationSize(info->pixel_format, info->coded_size);
  auto region = base::ReadOnlySharedMemoryRegion::Create(shared_memory_size);
  ASSERT_TRUE(region.IsValid());

  media::mojom::VideoBufferHandlePtr data =
      media::mojom::VideoBufferHandle::NewReadOnlyShmemRegion(
          std::move(region.region));

  gfx::Rect content_rect(0, 0, 100, 100);

  base::test::TestFuture<void> future;
  MockFrameCallbacks callbacks;
  EXPECT_CALL(callbacks, Done()).WillOnce(base::test::InvokeFuture(future));

  static_cast<viz::mojom::FrameSinkVideoConsumer&>(consumer).OnFrameCaptured(
      std::move(data), std::move(info), content_rect,
      callbacks.BindNewPipeAndPassRemote());
  EXPECT_TRUE(future.Wait());

  // This should have been rejected by IsValidFrame() in OnFrameCaptured.
  auto frame = consumer.GetLatestFrame(gfx::Point(0, 0));
  EXPECT_EQ(frame, nullptr);
}

TEST_F(AshMojomVideoConsumerTest, CreateSkBitmapWithOversizedContentRect) {
  AshMojomVideoConsumer consumer;

  // Create malicious parameters:
  // Use ARGB format (4 bytes per pixel).
  // coded_size is 100x100, so allocation size is 40000 bytes.
  auto info = media::mojom::VideoFrameInfo::New();
  info->pixel_format = media::VideoPixelFormat::PIXEL_FORMAT_ARGB;
  info->coded_size = gfx::Size(100, 100);
  info->visible_rect = gfx::Rect(0, 0, 100, 100);

  size_t shared_memory_size =
      media::VideoFrame::AllocationSize(info->pixel_format, info->coded_size);
  auto region = base::ReadOnlySharedMemoryRegion::Create(shared_memory_size);
  ASSERT_TRUE(region.IsValid());

  media::mojom::VideoBufferHandlePtr data =
      media::mojom::VideoBufferHandle::NewReadOnlyShmemRegion(
          std::move(region.region));

  // Malicious content_rect is 200x200, much larger than coded_size.
  // CreateSkBitmap would allocate N32 bitmap for 200x200, which is 160000
  // bytes. Then it would memcpy 160000 bytes from the 40000 bytes shared
  // memory.
  gfx::Rect content_rect(0, 0, 200, 200);

  base::test::TestFuture<void> future;
  MockFrameCallbacks callbacks;
  EXPECT_CALL(callbacks, Done()).WillOnce(base::test::InvokeFuture(future));

  static_cast<viz::mojom::FrameSinkVideoConsumer&>(consumer).OnFrameCaptured(
      std::move(data), std::move(info), content_rect,
      callbacks.BindNewPipeAndPassRemote());
  EXPECT_TRUE(future.Wait());

  // This should have been rejected by IsValidFrame() in OnFrameCaptured.
  auto frame = consumer.GetLatestFrame(gfx::Point(0, 0));
  EXPECT_EQ(frame, nullptr);
}

TEST_F(AshMojomVideoConsumerTest, CreateSkBitmapWithOutOfBoundsContentRect) {
  AshMojomVideoConsumer consumer;

  // Create malicious parameters:
  auto info = media::mojom::VideoFrameInfo::New();
  info->pixel_format = media::VideoPixelFormat::PIXEL_FORMAT_ARGB;
  info->coded_size = gfx::Size(100, 100);
  info->visible_rect = gfx::Rect(0, 0, 100, 100);

  size_t shared_memory_size =
      media::VideoFrame::AllocationSize(info->pixel_format, info->coded_size);
  auto region = base::ReadOnlySharedMemoryRegion::Create(shared_memory_size);
  ASSERT_TRUE(region.IsValid());

  media::mojom::VideoBufferHandlePtr data =
      media::mojom::VideoBufferHandle::NewReadOnlyShmemRegion(
          std::move(region.region));

  // content_rect that is out of bounds.
  gfx::Rect content_rect(-1, -1, 10, 10);

  base::test::TestFuture<void> future;
  MockFrameCallbacks callbacks;
  EXPECT_CALL(callbacks, Done()).WillOnce(base::test::InvokeFuture(future));

  static_cast<viz::mojom::FrameSinkVideoConsumer&>(consumer).OnFrameCaptured(
      std::move(data), std::move(info), content_rect,
      callbacks.BindNewPipeAndPassRemote());
  EXPECT_TRUE(future.Wait());

  // This should have been rejected by IsValidFrame() in OnFrameCaptured.
  auto frame = consumer.GetLatestFrame(gfx::Point(0, 0));
  EXPECT_EQ(frame, nullptr);
}

TEST_F(AshMojomVideoConsumerTest,
       CreateSkBitmapWithOversizedCaptureUpdateRect) {
  AshMojomVideoConsumer consumer;

  // Create malicious parameters:
  auto info = media::mojom::VideoFrameInfo::New();
  info->pixel_format = media::VideoPixelFormat::PIXEL_FORMAT_ARGB;
  info->coded_size = gfx::Size(100, 100);
  info->visible_rect = gfx::Rect(0, 0, 100, 100);

  // malicious capture_update_rect outside content_rect.
  info->metadata.capture_update_rect = gfx::Rect(0, 0, 200, 200);

  size_t shared_memory_size =
      media::VideoFrame::AllocationSize(info->pixel_format, info->coded_size);
  auto region = base::ReadOnlySharedMemoryRegion::Create(shared_memory_size);
  ASSERT_TRUE(region.IsValid());

  media::mojom::VideoBufferHandlePtr data =
      media::mojom::VideoBufferHandle::NewReadOnlyShmemRegion(
          std::move(region.region));

  gfx::Rect content_rect(0, 0, 100, 100);

  base::test::TestFuture<void> future;
  MockFrameCallbacks callbacks;
  EXPECT_CALL(callbacks, Done()).WillOnce(base::test::InvokeFuture(future));

  static_cast<viz::mojom::FrameSinkVideoConsumer&>(consumer).OnFrameCaptured(
      std::move(data), std::move(info), content_rect,
      callbacks.BindNewPipeAndPassRemote());
  EXPECT_TRUE(future.Wait());

  // This should have been rejected in OnFrameCaptured.
  auto frame = consumer.GetLatestFrame(gfx::Point(0, 0));
  EXPECT_EQ(frame, nullptr);
}

TEST_F(AshMojomVideoConsumerTest, CreateSkBitmapWithInvalidSizeCalculation) {
  AshMojomVideoConsumer consumer;

  // Create malicious parameters:
  auto info = media::mojom::VideoFrameInfo::New();
  info->pixel_format = media::VideoPixelFormat::PIXEL_FORMAT_ARGB;
  // Use a very large coded_size to cause an overflow in the size calculation.
  // Assuming size_t is at least 32 bits, something like 100000x100000
  // will overflow when multiplied by 4 (kBytesPerPixel).
  info->coded_size = gfx::Size(1000000, 1000000);
  info->visible_rect = gfx::Rect(0, 0, 1000000, 1000000);

  // Allocate a small valid region just to pass the IsValid() check.
  auto region = base::ReadOnlySharedMemoryRegion::Create(100);
  ASSERT_TRUE(region.IsValid());

  media::mojom::VideoBufferHandlePtr data =
      media::mojom::VideoBufferHandle::NewReadOnlyShmemRegion(
          std::move(region.region));

  gfx::Rect content_rect(0, 0, 100, 100);

  base::test::TestFuture<void> future;
  MockFrameCallbacks callbacks;
  EXPECT_CALL(callbacks, Done()).WillOnce(base::test::InvokeFuture(future));

  static_cast<viz::mojom::FrameSinkVideoConsumer&>(consumer).OnFrameCaptured(
      std::move(data), std::move(info), content_rect,
      callbacks.BindNewPipeAndPassRemote());
  EXPECT_TRUE(future.Wait());

  // This should have been rejected by IsValidFrame() in OnFrameCaptured.
  auto frame = consumer.GetLatestFrame(gfx::Point(0, 0));
  EXPECT_EQ(frame, nullptr);
}

TEST_F(AshMojomVideoConsumerTest, CreateSkBitmapWithValidFrame) {
  AshMojomVideoConsumer consumer;

  // Create valid parameters:
  auto info = media::mojom::VideoFrameInfo::New();
  info->pixel_format = media::VideoPixelFormat::PIXEL_FORMAT_ARGB;
  info->coded_size = gfx::Size(100, 100);
  info->visible_rect = gfx::Rect(0, 0, 100, 100);

  size_t shared_memory_size =
      media::VideoFrame::AllocationSize(info->pixel_format, info->coded_size);
  auto region = base::ReadOnlySharedMemoryRegion::Create(shared_memory_size);
  ASSERT_TRUE(region.IsValid());

  media::mojom::VideoBufferHandlePtr data =
      media::mojom::VideoBufferHandle::NewReadOnlyShmemRegion(
          std::move(region.region));

  gfx::Rect content_rect(0, 0, 100, 100);

  base::test::TestFuture<void> future;
  MockFrameCallbacks callbacks;
  // Done() should be called when Frame is destroyed (e.g. when consumer is
  // destroyed).
  EXPECT_CALL(callbacks, Done()).WillOnce(base::test::InvokeFuture(future));

  static_cast<viz::mojom::FrameSinkVideoConsumer&>(consumer).OnFrameCaptured(
      std::move(data), std::move(info), content_rect,
      callbacks.BindNewPipeAndPassRemote());

  auto frame = consumer.GetLatestFrame(gfx::Point(0, 0));
  EXPECT_NE(frame, nullptr);
  EXPECT_EQ(frame->size().width(), 100);
  EXPECT_EQ(frame->size().height(), 100);

  // Trigger Frame destruction to satisfy the Done() expectation.
  static_cast<viz::mojom::FrameSinkVideoConsumer&>(consumer).OnStopped();
  EXPECT_TRUE(future.Wait());
}

}  // namespace remoting
