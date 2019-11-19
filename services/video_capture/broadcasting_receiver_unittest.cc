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
static const int kArbiraryBufferId = 123;
static const int kArbiraryFrameFeedbackId = 456;

class FakeAccessPermission : public mojom::ScopedAccessPermission {
 public:
  FakeAccessPermission(base::OnceClosure destruction_cb)
      : destruction_cb_(std::move(destruction_cb)) {}
  ~FakeAccessPermission() override { std::move(destruction_cb_).Run(); }

 private:
  base::OnceClosure destruction_cb_;
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
        media::mojom::VideoBufferHandle::New();
    buffer_handle->set_shared_buffer_handle(
        mojo::WrapUnsafeSharedMemoryRegion(std::move(shm_region_)));
    broadcaster_.OnNewBuffer(kArbiraryBufferId, std::move(buffer_handle));
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
  EXPECT_CALL(*mock_video_frame_handler_1_, DoOnFrameReadyInBuffer(_, _, _, _))
      .WillOnce(InvokeWithoutArgs([&frame_arrived_at_video_frame_handler_1]() {
        frame_arrived_at_video_frame_handler_1.Quit();
      }));
  EXPECT_CALL(*mock_video_frame_handler_2_, DoOnFrameReadyInBuffer(_, _, _, _))
      .WillOnce(InvokeWithoutArgs([&frame_arrived_at_video_frame_handler_2]() {
        frame_arrived_at_video_frame_handler_2.Quit();
      }));
  mock_video_frame_handler_2_->HoldAccessPermissions();

  mojo::PendingRemote<mojom::ScopedAccessPermission> access_permission;
  bool access_permission_has_been_released = false;
  mojo::MakeSelfOwnedReceiver(
      std::make_unique<FakeAccessPermission>(base::BindOnce(
          [](bool* access_permission_has_been_released) {
            *access_permission_has_been_released = true;
          },
          &access_permission_has_been_released)),
      access_permission.InitWithNewPipeAndPassReceiver());
  media::mojom::VideoFrameInfoPtr frame_info =
      media::mojom::VideoFrameInfo::New();
  media::VideoFrameMetadata frame_metadata;
  frame_info->metadata = frame_metadata.GetInternalValues().Clone();
  broadcaster_.OnFrameReadyInBuffer(kArbiraryBufferId, kArbiraryFrameFeedbackId,
                                    std::move(access_permission),
                                    std::move(frame_info));

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

  // retire the buffer
  broadcaster_.OnBufferRetired(kArbiraryBufferId);

  // expect that both receivers get the retired event
  buffer_retired_arrived_at_video_frame_handler_1.Run();
  buffer_retired_arrived_at_video_frame_handler_2.Run();

  // expect that |access_permission| is still being held
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(access_permission_has_been_released);

  // mock_video_frame_handler_2_ finishes consuming
  mock_video_frame_handler_2_->ReleaseAccessPermissions();

  // expect that |access_permission| is released
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(access_permission_has_been_released);
}

TEST_F(BroadcastingReceiverTest,
       DoesNotHoldOnToAccessPermissionWhenAllClientsAreSuspended) {
  EXPECT_CALL(*mock_video_frame_handler_1_, DoOnFrameReadyInBuffer(_, _, _, _))
      .Times(0);
  EXPECT_CALL(*mock_video_frame_handler_2_, DoOnFrameReadyInBuffer(_, _, _, _))
      .Times(0);

  broadcaster_.SuspendClient(client_id_1_);
  broadcaster_.SuspendClient(client_id_2_);

  mojo::PendingRemote<mojom::ScopedAccessPermission> access_permission;
  bool access_permission_has_been_released = false;
  mojo::MakeSelfOwnedReceiver(
      std::make_unique<FakeAccessPermission>(base::BindOnce(
          [](bool* access_permission_has_been_released) {
            *access_permission_has_been_released = true;
          },
          &access_permission_has_been_released)),
      access_permission.InitWithNewPipeAndPassReceiver());
  media::mojom::VideoFrameInfoPtr frame_info =
      media::mojom::VideoFrameInfo::New();
  media::VideoFrameMetadata frame_metadata;
  frame_info->metadata = frame_metadata.GetInternalValues().Clone();
  broadcaster_.OnFrameReadyInBuffer(kArbiraryBufferId, kArbiraryFrameFeedbackId,
                                    std::move(access_permission),
                                    std::move(frame_info));

  // expect that |access_permission| is released
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(access_permission_has_been_released);
}

}  // namespace video_capture
