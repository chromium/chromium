// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/video_capture/broadcasting_receiver.h"

#include "base/memory/unsafe_shared_memory_region.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "mojo/public/cpp/system/platform_handle.h"
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
        media::mojom::VideoBufferHandle::New();
    buffer_handle->set_shared_buffer_handle(
        mojo::WrapUnsafeSharedMemoryRegion(std::move(shm_region_)));
    broadcaster_.OnNewBuffer(kArbitraryBufferId, std::move(buffer_handle));
  }

  FakeVideoFrameAccessHandler* CreateAndSendFakeVideoFrameAccessHandler() {
    mojo::PendingRemote<mojom::VideoFrameAccessHandler> pending_remote;
    auto frame_access_handler =
        mojo::MakeSelfOwnedReceiver<mojom::VideoFrameAccessHandler>(
            std::make_unique<FakeVideoFrameAccessHandler>(),
            pending_remote.InitWithNewPipeAndPassReceiver());
    broadcaster_.OnFrameAccessHandlerReady(std::move(pending_remote));
    return static_cast<FakeVideoFrameAccessHandler*>(
        frame_access_handler->impl());
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
  FakeVideoFrameAccessHandler* frame_access_handler =
      CreateAndSendFakeVideoFrameAccessHandler();

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

  media::mojom::VideoFrameInfoPtr frame_info =
      media::mojom::VideoFrameInfo::New();
  broadcaster_.OnFrameReadyInBuffer(
      mojom::ReadyFrameInBuffer::New(
          kArbitraryBufferId, kArbitraryFrameFeedbackId, std::move(frame_info)),
      {});

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
  EXPECT_TRUE(frame_access_handler->released_buffer_ids().empty());

  // mock_video_frame_handler_2_ finishes consuming. Access is still not
  // released because of mock_video_frame_handler_1_.
  mock_video_frame_handler_2_->ReleaseAccessedFrames();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(frame_access_handler->released_buffer_ids().empty());

  // mock_video_frame_handler_1_ finishes consuming. Now the buffer is finally
  // released.
  mock_video_frame_handler_1_->ReleaseAccessedFrames();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(frame_access_handler->released_buffer_ids().empty());
}

TEST_F(BroadcastingReceiverTest,
       DoesNotHoldOnToAccessPermissionWhenAllClientsAreSuspended) {
  FakeVideoFrameAccessHandler* frame_access_handler =
      CreateAndSendFakeVideoFrameAccessHandler();

  EXPECT_CALL(*mock_video_frame_handler_1_, DoOnFrameReadyInBuffer(_, _, _))
      .Times(0);
  EXPECT_CALL(*mock_video_frame_handler_2_, DoOnFrameReadyInBuffer(_, _, _))
      .Times(0);
  mock_video_frame_handler_1_->HoldAccessPermissions();
  mock_video_frame_handler_2_->HoldAccessPermissions();

  broadcaster_.SuspendClient(client_id_1_);
  broadcaster_.SuspendClient(client_id_2_);

  broadcaster_.OnFrameReadyInBuffer(
      mojom::ReadyFrameInBuffer::New(kArbitraryBufferId,
                                     kArbitraryFrameFeedbackId,
                                     media::mojom::VideoFrameInfo::New()),
      {});

  // Because the clients are suspended, frames are automatically released.
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(frame_access_handler->released_buffer_ids().empty());
  frame_access_handler->ClearReleasedBufferIds();

  // Resume one of the clients and pass another frame.
  broadcaster_.ResumeClient(client_id_2_);
  EXPECT_CALL(*mock_video_frame_handler_2_, DoOnFrameReadyInBuffer(_, _, _))
      .Times(1);
  broadcaster_.OnFrameReadyInBuffer(
      mojom::ReadyFrameInBuffer::New(kArbitraryBufferId,
                                     kArbitraryFrameFeedbackId,
                                     media::mojom::VideoFrameInfo::New()),
      {});

  // This time the buffer is not released automatically.
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(frame_access_handler->released_buffer_ids().empty());

  // Releasing mock_video_frame_handler_2_'s frame is sufficient for the buffer
  // to be released since the frame was never delivered to
  // mock_video_frame_handler_1_.
  mock_video_frame_handler_2_->ReleaseAccessedFrames();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(frame_access_handler->released_buffer_ids().empty());
}

TEST_F(BroadcastingReceiverTest, ForwardsScaledFrames) {
  const int kBufferId = 10;
  const int kScaledBufferId = 11;

  FakeVideoFrameAccessHandler* frame_access_handler =
      CreateAndSendFakeVideoFrameAccessHandler();
  mock_video_frame_handler_1_->HoldAccessPermissions();

  media::mojom::VideoBufferHandlePtr buffer_handle =
      media::mojom::VideoBufferHandle::New();
  buffer_handle->set_shared_buffer_handle(mojo::WrapUnsafeSharedMemoryRegion(
      base::UnsafeSharedMemoryRegion::Create(kArbitraryDummyBufferSize)));
  broadcaster_.OnNewBuffer(kBufferId, std::move(buffer_handle));

  media::mojom::VideoBufferHandlePtr scaled_buffer_handle =
      media::mojom::VideoBufferHandle::New();
  scaled_buffer_handle->set_shared_buffer_handle(
      mojo::WrapUnsafeSharedMemoryRegion(
          base::UnsafeSharedMemoryRegion::Create(kArbitraryDummyBufferSize)));
  broadcaster_.OnNewBuffer(kScaledBufferId, std::move(scaled_buffer_handle));

  // Suspend the second client so that the first client alone controls buffer
  // access.
  broadcaster_.SuspendClient(client_id_2_);

  base::RunLoop on_buffer_ready;
  EXPECT_CALL(*mock_video_frame_handler_1_, DoOnFrameReadyInBuffer(_, _, _))
      .WillOnce(
          InvokeWithoutArgs([&on_buffer_ready]() { on_buffer_ready.Quit(); }));

  mojom::ReadyFrameInBufferPtr ready_buffer =
      mojom::ReadyFrameInBuffer::New(kBufferId, kArbitraryFrameFeedbackId,
                                     media::mojom::VideoFrameInfo::New());

  std::vector<mojom::ReadyFrameInBufferPtr> scaled_ready_buffers;
  scaled_ready_buffers.push_back(
      mojom::ReadyFrameInBuffer::New(kScaledBufferId, kArbitraryFrameFeedbackId,
                                     media::mojom::VideoFrameInfo::New()));

  broadcaster_.OnFrameReadyInBuffer(std::move(ready_buffer),
                                    std::move(scaled_ready_buffers));
  on_buffer_ready.Run();
  EXPECT_TRUE(frame_access_handler->released_buffer_ids().empty());

  // Releasing the handler's buffers releases both frame and scaled frame.
  mock_video_frame_handler_1_->ReleaseAccessedFrames();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(frame_access_handler->released_buffer_ids().size(), 2u);

  // Scaled buffers also get retired.
  base::RunLoop on_both_buffers_retired;
  size_t num_buffers_retired = 0u;
  EXPECT_CALL(*mock_video_frame_handler_1_, DoOnBufferRetired(_))
      .WillRepeatedly(
          InvokeWithoutArgs([&on_both_buffers_retired, &num_buffers_retired]() {
            ++num_buffers_retired;
            if (num_buffers_retired == 2u)
              on_both_buffers_retired.Quit();
          }));
  broadcaster_.OnBufferRetired(kBufferId);
  broadcaster_.OnBufferRetired(kScaledBufferId);
  on_both_buffers_retired.Run();
}

TEST_F(BroadcastingReceiverTest, AccessPermissionsSurviveStop) {
  // For simplicitly, we only care about the first client in this test.
  broadcaster_.SuspendClient(client_id_2_);
  broadcaster_.OnStarted();

  // In this test, two frame handlers are used. In order to inspect all frame
  // IDs that have released after the first handler is destroyed, released
  // buffer IDs are stored in |released_buffer_ids|.
  std::vector<int32_t> released_buffer_ids;
  base::RepeatingCallback<void(int32_t)> buffer_released_callback =
      base::BindRepeating(
          [](std::vector<int32_t>* released_buffer_ids, int32_t buffer_id) {
            released_buffer_ids->push_back(buffer_id);
          },
          base::Unretained(&released_buffer_ids));
  mock_video_frame_handler_1_->HoldAccessPermissions();

  {
    mojo::PendingRemote<mojom::VideoFrameAccessHandler>
        frame_access_handler_proxy;
    auto frame_access_handler =
        mojo::MakeSelfOwnedReceiver<mojom::VideoFrameAccessHandler>(
            std::make_unique<FakeVideoFrameAccessHandler>(
                buffer_released_callback),
            frame_access_handler_proxy.InitWithNewPipeAndPassReceiver());
    broadcaster_.OnFrameAccessHandlerReady(
        std::move(frame_access_handler_proxy));
  }

  EXPECT_CALL(*mock_video_frame_handler_1_, DoOnFrameReadyInBuffer(_, _, _))
      .Times(1);
  broadcaster_.OnFrameReadyInBuffer(
      mojom::ReadyFrameInBuffer::New(kArbitraryBufferId,
                                     kArbitraryFrameFeedbackId,
                                     media::mojom::VideoFrameInfo::New()),
      {});
  base::RunLoop().RunUntilIdle();

  // The first frame has not been released yet.
  EXPECT_TRUE(released_buffer_ids.empty());

  // Simulate a device restart.
  broadcaster_.OnStopped();
  broadcaster_.OnStarted();
  FakeVideoFrameAccessHandler* second_device_frame_access_handler;
  {
    mojo::PendingRemote<mojom::VideoFrameAccessHandler>
        frame_access_handler_proxy;
    auto frame_access_handler =
        mojo::MakeSelfOwnedReceiver<mojom::VideoFrameAccessHandler>(
            std::make_unique<FakeVideoFrameAccessHandler>(
                buffer_released_callback),
            frame_access_handler_proxy.InitWithNewPipeAndPassReceiver());
    broadcaster_.OnFrameAccessHandlerReady(
        std::move(frame_access_handler_proxy));
    second_device_frame_access_handler =
        static_cast<FakeVideoFrameAccessHandler*>(frame_access_handler->impl());
  }
  base::RunLoop().RunUntilIdle();

  // The first frame has still not been released yet.
  EXPECT_TRUE(released_buffer_ids.empty());

  // Receive another frame after device restart.
  base::UnsafeSharedMemoryRegion shm_region2 =
      base::UnsafeSharedMemoryRegion::Create(kArbitraryDummyBufferSize);
  ASSERT_TRUE(shm_region2.IsValid());
  media::mojom::VideoBufferHandlePtr buffer_handle2 =
      media::mojom::VideoBufferHandle::New();
  buffer_handle2->set_shared_buffer_handle(
      mojo::WrapUnsafeSharedMemoryRegion(std::move(shm_region2)));
  broadcaster_.OnNewBuffer(kArbitraryBufferId + 1, std::move(buffer_handle2));
  EXPECT_CALL(*mock_video_frame_handler_1_, DoOnFrameReadyInBuffer(_, _, _))
      .Times(1);
  broadcaster_.OnFrameReadyInBuffer(
      mojom::ReadyFrameInBuffer::New(kArbitraryBufferId + 1,
                                     kArbitraryFrameFeedbackId + 1,
                                     media::mojom::VideoFrameInfo::New()),
      {});
  base::RunLoop().RunUntilIdle();

  // Neither frame has been released.
  EXPECT_TRUE(released_buffer_ids.empty());

  // Release all frames. This should inform both the old and the new handler.
  mock_video_frame_handler_1_->ReleaseAccessedFrames();
  base::RunLoop().RunUntilIdle();

  // Both buffers should now be released.
  EXPECT_EQ(released_buffer_ids.size(), 2u);
  // The first buffer was released by the first handler, which now no longer
  // exist. The second buffer was released by the second handler so we can
  // EXPECT it to only have observed a single frame release event.
  EXPECT_EQ(second_device_frame_access_handler->released_buffer_ids().size(),
            1u);
}

}  // namespace video_capture
