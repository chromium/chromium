// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <vector>

#include "base/barrier_closure.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "build/build_config.h"
#include "media/base/video_frame.h"
#include "media/mojo/common/media_type_converters.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "services/video_capture/broadcasting_receiver.h"
#include "services/video_capture/device_media_to_mojo_adapter.h"
#include "services/video_capture/public/cpp/mock_video_frame_handler.h"
#include "services/video_capture/public/mojom/constants.mojom.h"
#include "services/video_capture/public/mojom/video_frame_handler.mojom.h"
#include "services/video_capture/test/fake_video_capture_device_test.h"

using testing::_;
using testing::AtLeast;

namespace video_capture {

TEST_F(FakeVideoCaptureDeviceTest, FrameCallbacksArriveFromI420Device) {
  base::RunLoop wait_loop;
  // Constants must be static as a workaround
  // for a MSVC++ bug about lambda captures, see the discussion at
  // https://social.msdn.microsoft.com/Forums/SqlServer/4abf18bd-4ae4-4c72-ba3e-3b13e7909d5f
  static const int kNumFramesToWaitFor = 3;
  int num_frames_arrived = 0;
  mojo::PendingRemote<mojom::VideoFrameHandler> handler_remote;
  MockVideoFrameHandler video_frame_handler(
      handler_remote.InitWithNewPipeAndPassReceiver());
  EXPECT_CALL(video_frame_handler, DoOnNewBuffer(_, _)).Times(AtLeast(1));
  EXPECT_CALL(video_frame_handler, DoOnFrameReadyInBuffer(_, _, _))
      .WillRepeatedly([&wait_loop, &num_frames_arrived]() {
        num_frames_arrived += 1;
        if (num_frames_arrived >= kNumFramesToWaitFor) {
          wait_loop.Quit();
        }
      });

  mojo::Remote<video_capture::mojom::PushVideoStreamSubscription> subscription;

  i420_fake_source_remote_->CreatePushSubscription(
      std::move(handler_remote), requestable_settings_,
      false /*force_reopen_with_new_settings*/,
      subscription.BindNewPipeAndPassReceiver(),
      base::BindLambdaForTesting(
          [&subscription](
              video_capture::mojom::CreatePushSubscriptionResultCodePtr
                  result_code,
              const media::VideoCaptureParams& params) {
            EXPECT_TRUE(result_code->is_success_code());
            subscription->Activate();
          }));

  wait_loop.Run();
}

// Tests that the service successfully decodes a MJPEG frames even if
// DeviceFactoryProvider.InjectGpuDependencies() has not been called.
TEST_F(FakeVideoCaptureDeviceTest, FrameCallbacksArriveFromMjpegDevice) {
  base::RunLoop wait_loop;
  // Constants must be static as a workaround
  // for a MSVC++ bug about lambda captures, see the discussion at
  // https://social.msdn.microsoft.com/Forums/SqlServer/4abf18bd-4ae4-4c72-ba3e-3b13e7909d5f
  static const int kNumFramesToWaitFor = 3;
  int num_frames_arrived = 0;
  mojo::PendingRemote<mojom::VideoFrameHandler> handler_remote;
  MockVideoFrameHandler video_frame_handler(
      handler_remote.InitWithNewPipeAndPassReceiver());
  EXPECT_CALL(video_frame_handler, DoOnNewBuffer(_, _)).Times(AtLeast(1));
  EXPECT_CALL(video_frame_handler, DoOnFrameReadyInBuffer(_, _, _))
      .WillRepeatedly([&wait_loop, &num_frames_arrived]() {
        num_frames_arrived += 1;
        if (num_frames_arrived >= kNumFramesToWaitFor) {
          wait_loop.Quit();
        }
      });
  EXPECT_CALL(video_frame_handler, OnStartedUsingGpuDecode()).Times(0);

  mojo::Remote<video_capture::mojom::PushVideoStreamSubscription> subscription;

  mjpeg_fake_source_remote_->CreatePushSubscription(
      std::move(handler_remote), requestable_settings_,
      false /*force_reopen_with_new_settings*/,
      subscription.BindNewPipeAndPassReceiver(),
      base::BindLambdaForTesting(
          [&subscription](
              video_capture::mojom::CreatePushSubscriptionResultCodePtr
                  result_code,
              const media::VideoCaptureParams& params) {
            EXPECT_TRUE(result_code->is_success_code());
            subscription->Activate();
          }));

  wait_loop.Run();
}

// Tests that buffers get reused when receiving more frames than the maximum
// number of buffers in the pool.
TEST_F(FakeVideoCaptureDeviceTest, BuffersGetReused) {
  base::RunLoop wait_loop;
  const int kMaxBufferPoolBuffers =
      DeviceMediaToMojoAdapter::max_buffer_pool_buffer_count();
  // Constants must be static as a workaround
  // for a MSVC++ bug about lambda captures, see the discussion at
  // https://social.msdn.microsoft.com/Forums/SqlServer/4abf18bd-4ae4-4c72-ba3e-3b13e7909d5f
  static const int kNumFramesToWaitFor = kMaxBufferPoolBuffers + 3;
  int num_buffers_created = 0;
  int num_frames_arrived = 0;
  mojo::PendingRemote<mojom::VideoFrameHandler> handler_remote;
  MockVideoFrameHandler video_frame_handler(
      handler_remote.InitWithNewPipeAndPassReceiver());
  EXPECT_CALL(video_frame_handler, DoOnNewBuffer(_, _))
      .WillRepeatedly([&num_buffers_created]() { num_buffers_created++; });
  EXPECT_CALL(video_frame_handler, DoOnFrameReadyInBuffer(_, _, _))
      .WillRepeatedly([&wait_loop, &num_frames_arrived]() {
        if (++num_frames_arrived >= kNumFramesToWaitFor) {
          wait_loop.Quit();
        }
      });

  mojo::Remote<video_capture::mojom::PushVideoStreamSubscription> subscription;

  i420_fake_source_remote_->CreatePushSubscription(
      std::move(handler_remote), requestable_settings_,
      false /*force_reopen_with_new_settings*/,
      subscription.BindNewPipeAndPassReceiver(),
      base::BindLambdaForTesting(
          [&subscription](
              video_capture::mojom::CreatePushSubscriptionResultCodePtr
                  result_code,
              const media::VideoCaptureParams& params) {
            EXPECT_TRUE(result_code->is_success_code());
            subscription->Activate();
          }));

  wait_loop.Run();

  ASSERT_LT(num_buffers_created, num_frames_arrived);
  ASSERT_LE(num_buffers_created, kMaxBufferPoolBuffers);
}

// Tests that when the device is stopped (triggered by a second subscriber
// forcing a reopen with different settings), all previously allocated buffers
// are retired via OnBufferRetired().
TEST_F(FakeVideoCaptureDeviceTest, BuffersGetRetiredWhenDeviceIsStopped) {
  base::RunLoop wait_for_frames_loop;
  static const int kNumFramesToWaitFor = 2;
  std::vector<int32_t> buffers_to_retire;
  bool collecting_buffers = true;
  int num_frames_arrived = 0;
  mojo::PendingRemote<mojom::VideoFrameHandler> handler_remote;
  MockVideoFrameHandler video_frame_handler(
      handler_remote.InitWithNewPipeAndPassReceiver());
  EXPECT_CALL(video_frame_handler, DoOnNewBuffer(_, _))
      .WillRepeatedly(
          [&buffers_to_retire, &collecting_buffers](
              int32_t buffer_id, media::mojom::VideoBufferHandlePtr*) {
            if (collecting_buffers) {
              buffers_to_retire.push_back(buffer_id);
            }
          });
  EXPECT_CALL(video_frame_handler, DoOnFrameReadyInBuffer(_, _, _))
      .WillRepeatedly([&wait_for_frames_loop, &num_frames_arrived]() {
        if (++num_frames_arrived >= kNumFramesToWaitFor) {
          wait_for_frames_loop.Quit();
        }
      });

  mojo::Remote<video_capture::mojom::PushVideoStreamSubscription> subscription;

  i420_fake_source_remote_->CreatePushSubscription(
      std::move(handler_remote), requestable_settings_,
      false /*force_reopen_with_new_settings*/,
      subscription.BindNewPipeAndPassReceiver(),
      base::BindLambdaForTesting(
          [&subscription](
              video_capture::mojom::CreatePushSubscriptionResultCodePtr
                  result_code,
              const media::VideoCaptureParams& params) {
            EXPECT_TRUE(result_code->is_success_code());
            subscription->Activate();
          }));

  wait_for_frames_loop.Run();
  collecting_buffers = false;
  ASSERT_FALSE(buffers_to_retire.empty());

  base::RunLoop wait_for_reopen_and_retire_loop;
  auto barrier =
      base::BarrierClosure(2, wait_for_reopen_and_retire_loop.QuitClosure());
  bool all_retired = false;
  EXPECT_CALL(video_frame_handler, DoOnBufferRetired(_))
      .WillRepeatedly(
          [&buffers_to_retire, &all_retired, &barrier](int32_t buffer_id) {
            std::erase(buffers_to_retire, buffer_id);
            if (!all_retired && buffers_to_retire.empty()) {
              all_retired = true;
              barrier.Run();
            }
          });

  // Stop (and reopen) the device by creating a second subscription with
  // different settings and `force_reopen_with_new_settings` = true.
  media::VideoCaptureParams different_settings = requestable_settings_;
  different_settings.requested_format.frame_size =
      gfx::Size(requestable_settings_.requested_format.frame_size.width() / 2,
                requestable_settings_.requested_format.frame_size.height() / 2);
  ASSERT_FALSE(different_settings == requestable_settings_);

  mojo::PendingRemote<mojom::VideoFrameHandler> handler_remote_2;
  MockVideoFrameHandler video_frame_handler_2(
      handler_remote_2.InitWithNewPipeAndPassReceiver());
  mojo::Remote<video_capture::mojom::PushVideoStreamSubscription>
      subscription_2;
  i420_fake_source_remote_->CreatePushSubscription(
      std::move(handler_remote_2), different_settings,
      /*force_reopen_with_new_settings=*/true,
      subscription_2.BindNewPipeAndPassReceiver(),
      base::BindLambdaForTesting(
          [&barrier](video_capture::mojom::CreatePushSubscriptionResultCodePtr
                         result_code,
                     const media::VideoCaptureParams&) {
            EXPECT_TRUE(result_code->is_success_code());
            barrier.Run();
          }));

  wait_for_reopen_and_retire_loop.Run();
}

}  // namespace video_capture
