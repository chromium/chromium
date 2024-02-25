// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/video_capture/broadcasting_receiver.h"

#include "base/memory/unsafe_shared_memory_region.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "services/video_capture/public/cpp/mock_video_frame_handler.h"
#include "services/video_capture/public/mojom/video_frame_handler.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::InvokeWithoutArgs;

namespace video_capture {

static const size_t kArbitraryDummyBufferSize = 8u;
static const int kArbitraryBufferId = 123;
static const int kArbitraryFrameFeedbackId = 456;

class StubReadWritePermission final
    : public media::VideoCaptureDevice::Client::Buffer::ScopedAccessPermission {
 public:
  StubReadWritePermission() = default;

  StubReadWritePermission(const StubReadWritePermission&) = delete;
  StubReadWritePermission& operator=(const StubReadWritePermission&) = delete;

  ~StubReadWritePermission() override = default;
};

class BroadcastingReceiverTest : public ::testing::Test {
 public:
  void SetUp() override {
    mojo::PendingRemote<mojom::VideoFrameHandler> video_frame_handler_1;
    mojo::PendingRemote<mojom::VideoFrameHandler> video_frame_handler_2;
    mock_video_frame_handler_1_ = std::make_unique<MockVideoFrameHandler>(
        video_frame_handler_1.InitWithNewPipeAndPassReceiver());
    mock_video_frame_handler_2_ = std::make_unique<MockVideoFrameHandler>(
        video_frame_handler_2.InitWithNewPipeAndPassReceiver());
    client_id_1_ =
        broadcaster_.AddClient(std::move(video_frame_handler_1),
                               media::VideoCaptureBufferType::kSharedMemory);
    client_id_2_ =
        broadcaster_.AddClient(std::move(video_frame_handler_2),
                               media::VideoCaptureBufferType::kSharedMemory);

    shm_region_ =
        base::UnsafeSharedMemoryRegion::Create(kArbitraryDummyBufferSize);
    ASSERT_TRUE(shm_region_.IsValid());
    media::mojom::VideoBufferHandlePtr buffer_handle =
        media::mojom::VideoBufferHandle::NewUnsafeShmemRegion(
            std::move(shm_region_));
    broadcaster_.OnNewBuffer(kArbitraryBufferId, std::move(buffer_handle));
  }

  size_t HoldBufferContextSize() {
    return broadcaster_.scoped_access_permissions_by_buffer_context_id_.size();
  }

 protected:
  BroadcastingReceiver broadcaster_;
  std::unique_ptr<MockVideoFrameHandler> mock_video_frame_handler_1_;
  std::unique_ptr<MockVideoFrameHandler> mock_video_frame_handler_2_;
  int32_t client_id_1_;
  int32_t client_id_2_;
  base::UnsafeSharedMemoryRegion shm_region_;
  base::test::TaskEnvironment task_environment_;
};

TEST_F(
    BroadcastingReceiverTest,
    HoldsOnToAccessPermissionForRetiredBufferUntilLastClientFinishedConsuming) {
  base::RunLoop frame_arrived_at_video_frame_handler_1;
  base::RunLoop frame_arrived_at_video_frame_handler_2;
  EXPECT_CALL(*mock_video_frame_handler_1_, DoOnFrameReadyInBuffer(_, _, _))
      .WillOnce(InvokeWithoutArgs([&frame_arrived_at_video_frame_handler_1]() {
        frame_arrived_at_video_frame_handler_1.Quit();
      }));
  EXPECT_CALL(*mock_video_frame_handler_2_, DoOnFrameReadyInBuffer(_, _, _))
      .WillOnce(InvokeWithoutArgs([&frame_arrived_at_video_frame_handler_2]() {
        frame_arrived_at_video_frame_handler_2.Quit();
      }));
  mock_video_frame_handler_1_->HoldAccessPermissions();
  mock_video_frame_handler_2_->HoldAccessPermissions();

  broadcaster_.OnFrameReadyInBuffer(
      media::ReadyFrameInBuffer(kArbitraryBufferId, kArbitraryFrameFeedbackId,
                                std::make_unique<StubReadWritePermission>(),
                                media::mojom::VideoFrameInfo::New()));

  // mock_video_frame_handler_1_ finishes consuming immediately.
  // mock_video_frame_handler_2_ continues consuming.
  frame_arrived_at_video_frame_handler_1.Run();
  frame_arrived_at_video_frame_handler_2.Run();

  base::RunLoop buffer_retired_arrived_at_video_frame_handler_1;
  base::RunLoop buffer_retired_arrived_at_video_frame_handler_2;
  EXPECT_CALL(*mock_video_frame_handler_1_, DoOnBufferRetired(_))
      .WillOnce(InvokeWithoutArgs(
          [&buffer_retired_arrived_at_video_frame_handler_1]() {
            buffer_retired_arrived_at_video_frame_handler_1.Quit();
          }));
  EXPECT_CALL(*mock_video_frame_handler_2_, DoOnBufferRetired(_))
      .WillOnce(InvokeWithoutArgs(
          [&buffer_retired_arrived_at_video_frame_handler_2]() {
            buffer_retired_arrived_at_video_frame_handler_2.Quit();
          }));

  // Retiring the buffer results in both receivers getting the retired event.
  broadcaster_.OnBufferRetired(kArbitraryBufferId);
  buffer_retired_arrived_at_video_frame_handler_1.Run();
  buffer_retired_arrived_at_video_frame_handler_2.Run();
  base::RunLoop().RunUntilIdle();

  // Despite retiring, the access to the buffer is not released because it is
  // still in use by both handlers.
  DCHECK_EQ(HoldBufferContextSize(), 1u);

  // mock_video_frame_handler_2_ finishes consuming. Access is still not
  // released because of mock_video_frame_handler_1_.
  mock_video_frame_handler_2_->ReleaseAccessedFrames();
  base::RunLoop().RunUntilIdle();
  DCHECK_EQ(HoldBufferContextSize(), 1u);

  // mock_video_frame_handler_1_ finishes consuming. Now the buffer is finally
  // released.
  mock_video_frame_handler_1_->ReleaseAccessedFrames();
  base::RunLoop().RunUntilIdle();
  DCHECK_EQ(HoldBufferContextSize(), 0u);
}

TEST_F(BroadcastingReceiverTest,
       DoesNotHoldOnToAccessPermissionWhenAllClientsAreSuspended) {
  EXPECT_CALL(*mock_video_frame_handler_1_, DoOnFrameReadyInBuffer(_, _, _))
      .Times(0);
  EXPECT_CALL(*mock_video_frame_handler_2_, DoOnFrameReadyInBuffer(_, _, _))
      .Times(0);
  mock_video_frame_handler_1_->HoldAccessPermissions();
  mock_video_frame_handler_2_->HoldAccessPermissions();

  broadcaster_.SuspendClient(client_id_1_);
  broadcaster_.SuspendClient(client_id_2_);

  broadcaster_.OnFrameReadyInBuffer(
      media::ReadyFrameInBuffer(kArbitraryBufferId, kArbitraryFrameFeedbackId,
                                std::make_unique<StubReadWritePermission>(),
                                media::mojom::VideoFrameInfo::New()));

  // Because the clients are suspended, frames are automatically released.
  base::RunLoop().RunUntilIdle();
  DCHECK_EQ(HoldBufferContextSize(), 0u);

  // Resume one of the clients and pass another frame.
  broadcaster_.ResumeClient(client_id_2_);
  EXPECT_CALL(*mock_video_frame_handler_2_, DoOnFrameReadyInBuffer(_, _, _))
      .Times(1);
  broadcaster_.OnFrameReadyInBuffer(
      media::ReadyFrameInBuffer(kArbitraryBufferId, kArbitraryFrameFeedbackId,
                                std::make_unique<StubReadWritePermission>(),
                                media::mojom::VideoFrameInfo::New()));

  // This time the buffer is not released automatically.
  base::RunLoop().RunUntilIdle();
  DCHECK_EQ(HoldBufferContextSize(), 1u);

  // Releasing mock_video_frame_handler_2_'s frame is sufficient for the buffer
  // to be released since the frame was never delivered to
  // mock_video_frame_handler_1_.
  mock_video_frame_handler_2_->ReleaseAccessedFrames();
  base::RunLoop().RunUntilIdle();
  DCHECK_EQ(HoldBufferContextSize(), 0u);
}

TEST_F(BroadcastingReceiverTest, AccessPermissionsSurviveStop) {
  // For simplicitly, we only care about the first client in this test.
  broadcaster_.SuspendClient(client_id_2_);
  broadcaster_.OnStarted();

  // In this test, two frame handlers are used. In order to inspect all frame
  // IDs that have released after the first handler is destroyed.
  mock_video_frame_handler_1_->HoldAccessPermissions();

  EXPECT_CALL(*mock_video_frame_handler_1_, DoOnFrameReadyInBuffer(_, _, _))
      .Times(1);
  broadcaster_.OnFrameReadyInBuffer(
      media::ReadyFrameInBuffer(kArbitraryBufferId, kArbitraryFrameFeedbackId,
                                std::make_unique<StubReadWritePermission>(),
                                media::mojom::VideoFrameInfo::New()));
  base::RunLoop().RunUntilIdle();

  // The first frame has not been released yet.
  DCHECK_EQ(HoldBufferContextSize(), 1u);

  // Simulate a device restart.
  broadcaster_.OnStopped();
  broadcaster_.OnStarted();
  base::RunLoop().RunUntilIdle();

  // The first frame has still not been released yet.
  DCHECK_EQ(HoldBufferContextSize(), 1u);

  // Receive another frame after device restart.
  base::UnsafeSharedMemoryRegion shm_region2 =
      base::UnsafeSharedMemoryRegion::Create(kArbitraryDummyBufferSize);
  ASSERT_TRUE(shm_region2.IsValid());
  media::mojom::VideoBufferHandlePtr buffer_handle2 =
      media::mojom::VideoBufferHandle::NewUnsafeShmemRegion(
          std::move(shm_region2));
  broadcaster_.OnNewBuffer(kArbitraryBufferId + 1, std::move(buffer_handle2));
  EXPECT_CALL(*mock_video_frame_handler_1_, DoOnFrameReadyInBuffer(_, _, _))
      .Times(1);
  broadcaster_.OnFrameReadyInBuffer(media::ReadyFrameInBuffer(
      kArbitraryBufferId + 1, kArbitraryFrameFeedbackId + 1,
      std::make_unique<StubReadWritePermission>(),
      media::mojom::VideoFrameInfo::New()));
  base::RunLoop().RunUntilIdle();

  // Neither frame has been released.
  DCHECK_EQ(HoldBufferContextSize(), 2u);

  // Release all frames. This should inform both the old and the new handler.
  mock_video_frame_handler_1_->ReleaseAccessedFrames();
  base::RunLoop().RunUntilIdle();

  // Both buffers should now be released.
  DCHECK_EQ(HoldBufferContextSize(), 0u);
}

}  // namespace video_capture
