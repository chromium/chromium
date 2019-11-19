// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "build/build_config.h"
#include "media/base/video_frame.h"
#include "media/mojo/common/media_type_converters.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "services/video_capture/broadcasting_receiver.h"
#include "services/video_capture/device_media_to_mojo_adapter.h"
#include "services/video_capture/public/cpp/mock_video_frame_handler.h"
#include "services/video_capture/public/mojom/constants.mojom.h"
#include "services/video_capture/public/mojom/device_factory.mojom.h"
#include "services/video_capture/public/mojom/video_frame_handler.mojom.h"
#include "services/video_capture/test/fake_device_test.h"

using testing::_;
using testing::AtLeast;
using testing::Invoke;
using testing::InvokeWithoutArgs;

namespace video_capture {

// This alias ensures test output is easily attributed to this service's tests.
// TODO(rockot/chfremer): Consider just renaming the type.
using FakeVideoCaptureDeviceTest = FakeDeviceTest;

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
  EXPECT_CALL(video_frame_handler, DoOnFrameReadyInBuffer(_, _, _, _))
      .WillRepeatedly(InvokeWithoutArgs([&wait_loop, &num_frames_arrived]() {
        num_frames_arrived += 1;
        if (num_frames_arrived >= kNumFramesToWaitFor) {
          wait_loop.Quit();
        }
      }));

  i420_fake_device_remote_->Start(requestable_settings_,
                                  std::move(handler_remote));
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
  EXPECT_CALL(video_frame_handler, DoOnFrameReadyInBuffer(_, _, _, _))
      .WillRepeatedly(InvokeWithoutArgs([&wait_loop, &num_frames_arrived]() {
        num_frames_arrived += 1;
        if (num_frames_arrived >= kNumFramesToWaitFor) {
          wait_loop.Quit();
        }
      }));
  EXPECT_CALL(video_frame_handler, OnStartedUsingGpuDecode()).Times(0);

  mjpeg_fake_device_remote_->Start(requestable_settings_,
                                   std::move(handler_remote));
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
      .WillRepeatedly(InvokeWithoutArgs(
          [&num_buffers_created]() { num_buffers_created++; }));
  EXPECT_CALL(video_frame_handler, DoOnFrameReadyInBuffer(_, _, _, _))
      .WillRepeatedly(InvokeWithoutArgs([&wait_loop, &num_frames_arrived]() {
        if (++num_frames_arrived >= kNumFramesToWaitFor) {
          wait_loop.Quit();
        }
      }));

  i420_fake_device_remote_->Start(requestable_settings_,
                                  std::move(handler_remote));
  wait_loop.Run();

  ASSERT_LT(num_buffers_created, num_frames_arrived);
  ASSERT_LE(num_buffers_created, kMaxBufferPoolBuffers);
}

// Tests that when the device is stopped OnBufferRetired() events get sent out
// to the receiver followed by OnStopped().
TEST_F(FakeVideoCaptureDeviceTest, BuffersGetRetiredWhenDeviceIsStopped) {
  base::RunLoop wait_for_frames_loop;
  static const int kNumFramesToWaitFor = 2;
  std::vector<int32_t> known_buffer_ids;
  int num_frames_arrived = 0;
  mojo::PendingRemote<mojom::VideoFrameHandler> handler_remote;
  MockVideoFrameHandler video_frame_handler(
      handler_remote.InitWithNewPipeAndPassReceiver());
  EXPECT_CALL(video_frame_handler, DoOnNewBuffer(_, _))
      .WillRepeatedly(
          Invoke([&known_buffer_ids](int32_t buffer_id,
                                     media::mojom::VideoBufferHandlePtr*) {
            known_buffer_ids.push_back(buffer_id);
          }));
  EXPECT_CALL(video_frame_handler, DoOnFrameReadyInBuffer(_, _, _, _))
      .WillRepeatedly(
          InvokeWithoutArgs([&wait_for_frames_loop, &num_frames_arrived]() {
            if (++num_frames_arrived >= kNumFramesToWaitFor) {
              wait_for_frames_loop.Quit();
            }
          }));

  i420_fake_device_remote_->Start(requestable_settings_,
                                  std::move(handler_remote));
  wait_for_frames_loop.Run();

  base::RunLoop wait_for_on_stopped_loop;
  EXPECT_CALL(video_frame_handler, DoOnBufferRetired(_))
      .WillRepeatedly(Invoke([&known_buffer_ids](int32_t buffer_id) {
        auto iter = std::find(known_buffer_ids.begin(), known_buffer_ids.end(),
                              buffer_id);
        ASSERT_TRUE(iter != known_buffer_ids.end());
        known_buffer_ids.erase(iter);
      }));
  EXPECT_CALL(video_frame_handler, OnStopped())
      .WillOnce(Invoke(
          [&wait_for_on_stopped_loop]() { wait_for_on_stopped_loop.Quit(); }));

  // Stop the device
  i420_fake_device_remote_.reset();
  wait_for_on_stopped_loop.Run();
  ASSERT_TRUE(known_buffer_ids.empty());
}

// This requires the linux platform, where shared regions are backed by a file
// descriptor.
#if defined(OS_LINUX)
TEST_F(FakeVideoCaptureDeviceTest,
       ReceiveFramesViaFileDescriptorHandlesForSharedMemory) {
  base::RunLoop wait_loop;
  static const int kNumFramesToWaitFor = 3;
  int num_frames_arrived = 0;
  std::map<int32_t, media::mojom::VideoBufferHandlePtr> buffers_by_id;
  mojo::PendingRemote<mojom::VideoFrameHandler> handler_remote;
  MockVideoFrameHandler video_frame_handler(
      handler_remote.InitWithNewPipeAndPassReceiver());
  EXPECT_CALL(video_frame_handler, DoOnNewBuffer(_, _))
      .Times(AtLeast(1))
      .WillRepeatedly(Invoke(
          [&buffers_by_id](int32_t buffer_id,
                           media::mojom::VideoBufferHandlePtr* buffer_handle) {
            ASSERT_TRUE(
                (*buffer_handle)->is_shared_memory_via_raw_file_descriptor());
            // |buffer_handle| is a |VideoBufferHandlePtr*| only because gmock
            // doesn't handle move-only types. Because |buffer_handle| is not
            // used in the MockVideoFrameHandler implementation of
            // |OnNewBuffer|, it is safe to move the reference.
            BroadcastingReceiver::BufferContext context(
                buffer_id, std::move(*buffer_handle));
            // Use |context| to convert the raw file descriptor handle to a
            // shared memory type that can be easily mapped in
            // DoOnFrameReadyInBuffer, below.
            buffers_by_id.insert(std::make_pair(
                buffer_id, context.CloneBufferHandle(
                               media::VideoCaptureBufferType::kSharedMemory)));
          }));
  bool found_unexpected_all_zero_frame = false;
  EXPECT_CALL(video_frame_handler, DoOnFrameReadyInBuffer(_, _, _, _))
      .WillRepeatedly(
          Invoke([&wait_loop, &num_frames_arrived, &buffers_by_id,
                  &found_unexpected_all_zero_frame](
                     int32_t buffer_id, int32_t frame_feedback_id,
                     const mojo::PendingRemote<mojom::ScopedAccessPermission>&,
                     media::mojom::VideoFrameInfoPtr*) {
            const mojo::ScopedSharedBufferHandle& handle =
                buffers_by_id[buffer_id]->get_shared_buffer_handle();
            mojo::ScopedSharedBufferMapping mapping =
                handle->Map(handle->GetSize());
            const uint8_t* data = static_cast<uint8_t*>(mapping.get());
            // Check that there is at least one non-zero byte in the frame data.
            bool found_non_zero_byte = false;
            for (uint32_t i = 0; i < handle->GetSize(); i++) {
              if (data[i] != 0u) {
                found_non_zero_byte = true;
                break;
              }
            }
            if (!found_non_zero_byte) {
              found_unexpected_all_zero_frame = true;
              wait_loop.Quit();
              return;
            }
            num_frames_arrived += 1;
            if (num_frames_arrived >= kNumFramesToWaitFor) {
              wait_loop.Quit();
            }
          }));

  // Make a copy of |requestable_settings_| and change it to ask for
  // |kSharedMemoryViaRawFileDescriptor|.
  media::VideoCaptureParams settings_to_request = requestable_settings_;
  settings_to_request.buffer_type =
      media::VideoCaptureBufferType::kSharedMemoryViaRawFileDescriptor;
  i420_fake_device_remote_->Start(settings_to_request,
                                  std::move(handler_remote));
  wait_loop.Run();
  EXPECT_FALSE(found_unexpected_all_zero_frame);
}
#endif  // defined(OS_LINUX)

}  // namespace video_capture
