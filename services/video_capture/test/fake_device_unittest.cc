// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "build/build_config.h"
#include "media/base/video_frame.h"
#include "media/capture/video/shared_memory_handle_provider.h"
#include "media/mojo/common/media_type_converters.h"
#include "services/video_capture/device_media_to_mojo_adapter.h"
#include "services/video_capture/public/cpp/mock_receiver.h"
#include "services/video_capture/public/mojom/constants.mojom.h"
#include "services/video_capture/public/mojom/device_factory.mojom.h"
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
  mojom::ReceiverPtr receiver_proxy;
  MockReceiver receiver(mojo::MakeRequest(&receiver_proxy));
  EXPECT_CALL(receiver, DoOnNewBuffer(_, _)).Times(AtLeast(1));
  EXPECT_CALL(receiver, DoOnFrameReadyInBuffer(_, _, _, _))
      .WillRepeatedly(InvokeWithoutArgs([&wait_loop, &num_frames_arrived]() {
        num_frames_arrived += 1;
        if (num_frames_arrived >= kNumFramesToWaitFor) {
          wait_loop.Quit();
        }
      }));

  i420_fake_device_proxy_->Start(requestable_settings_,
                                 std::move(receiver_proxy));
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
  mojom::ReceiverPtr receiver_proxy;
  MockReceiver receiver(mojo::MakeRequest(&receiver_proxy));
  EXPECT_CALL(receiver, DoOnNewBuffer(_, _)).Times(AtLeast(1));
  EXPECT_CALL(receiver, DoOnFrameReadyInBuffer(_, _, _, _))
      .WillRepeatedly(InvokeWithoutArgs([&wait_loop, &num_frames_arrived]() {
        num_frames_arrived += 1;
        if (num_frames_arrived >= kNumFramesToWaitFor) {
          wait_loop.Quit();
        }
      }));
  EXPECT_CALL(receiver, OnStartedUsingGpuDecode()).Times(0);

  mjpeg_fake_device_proxy_->Start(requestable_settings_,
                                  std::move(receiver_proxy));
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
  mojom::ReceiverPtr receiver_proxy;
  MockReceiver receiver(mojo::MakeRequest(&receiver_proxy));
  EXPECT_CALL(receiver, DoOnNewBuffer(_, _))
      .WillRepeatedly(InvokeWithoutArgs(
          [&num_buffers_created]() { num_buffers_created++; }));
  EXPECT_CALL(receiver, DoOnFrameReadyInBuffer(_, _, _, _))
      .WillRepeatedly(InvokeWithoutArgs([&wait_loop, &num_frames_arrived]() {
        if (++num_frames_arrived >= kNumFramesToWaitFor) {
          wait_loop.Quit();
        }
      }));

  i420_fake_device_proxy_->Start(requestable_settings_,
                                 std::move(receiver_proxy));
  wait_loop.Run();

  ASSERT_LT(num_buffers_created, num_frames_arrived);
  ASSERT_LE(num_buffers_created, kMaxBufferPoolBuffers);
}

// Tests that OnBufferRetired() events get sent out to the receiver when the
// device is stopped.
TEST_F(FakeVideoCaptureDeviceTest, BuffersGetRetiredWhenDeviceIsStopped) {
  base::RunLoop wait_for_frames_loop;
  static const int kNumFramesToWaitFor = 2;
  std::vector<int32_t> received_buffer_ids;
  int num_frames_arrived = 0;
  mojom::ReceiverPtr receiver_proxy;
  MockReceiver receiver(mojo::MakeRequest(&receiver_proxy));
  EXPECT_CALL(receiver, DoOnNewBuffer(_, _))
      .WillRepeatedly(
          Invoke([&received_buffer_ids](int32_t buffer_id,
                                        media::mojom::VideoBufferHandlePtr*) {
            received_buffer_ids.push_back(buffer_id);
          }));
  EXPECT_CALL(receiver, DoOnFrameReadyInBuffer(_, _, _, _))
      .WillRepeatedly(
          InvokeWithoutArgs([&wait_for_frames_loop, &num_frames_arrived]() {
            if (++num_frames_arrived >= kNumFramesToWaitFor) {
              wait_for_frames_loop.Quit();
            }
          }));

  i420_fake_device_proxy_->Start(requestable_settings_,
                                 std::move(receiver_proxy));
  wait_for_frames_loop.Run();

  base::RunLoop wait_for_buffers_retired_loop;
  EXPECT_CALL(receiver, OnBufferRetired(_))
      .WillRepeatedly(
          Invoke([&received_buffer_ids,
                  &wait_for_buffers_retired_loop](int32_t buffer_id) {
            auto iter = std::find(received_buffer_ids.begin(),
                                  received_buffer_ids.end(), buffer_id);
            ASSERT_TRUE(iter != received_buffer_ids.end());
            received_buffer_ids.erase(iter);
            if (received_buffer_ids.empty()) {
              wait_for_buffers_retired_loop.Quit();
            }
          }));

  // Stop the device
  i420_fake_device_proxy_.reset();
  wait_for_buffers_retired_loop.Run();
}

// This requires platforms where base::SharedMemoryHandle is backed by a
// file descriptor.
#if defined(OS_LINUX)
TEST_F(FakeVideoCaptureDeviceTest,
       ReceiveFramesViaFileDescriptorHandlesForSharedMemory) {
  base::RunLoop wait_loop;
  static const int kNumFramesToWaitFor = 3;
  int num_frames_arrived = 0;
  std::map<int32_t, std::unique_ptr<media::SharedMemoryHandleProvider>>
      buffers_by_id;
  mojom::ReceiverPtr receiver_proxy;
  MockReceiver receiver(mojo::MakeRequest(&receiver_proxy));
  EXPECT_CALL(receiver, DoOnNewBuffer(_, _))
      .Times(AtLeast(1))
      .WillRepeatedly(Invoke(
          [&buffers_by_id](int32_t buffer_id,
                           media::mojom::VideoBufferHandlePtr* buffer_handle) {
            ASSERT_TRUE(
                (*buffer_handle)->is_shared_memory_via_raw_file_descriptor());
            auto provider =
                std::make_unique<media::SharedMemoryHandleProvider>();
            provider->InitAsReadOnlyFromRawFileDescriptor(
                std::move((*buffer_handle)
                              ->get_shared_memory_via_raw_file_descriptor()
                              ->file_descriptor_handle),
                (*buffer_handle)
                    ->get_shared_memory_via_raw_file_descriptor()
                    ->shared_memory_size_in_bytes);
            buffers_by_id.insert(
                std::make_pair(buffer_id, std::move(provider)));
          }));
  bool found_unexpected_all_zero_frame = false;
  EXPECT_CALL(receiver, DoOnFrameReadyInBuffer(_, _, _, _))
      .WillRepeatedly(Invoke([&wait_loop, &num_frames_arrived, &buffers_by_id,
                              &found_unexpected_all_zero_frame](
                                 int32_t buffer_id, int32_t frame_feedback_id,
                                 mojom::ScopedAccessPermissionPtr*,
                                 media::mojom::VideoFrameInfoPtr*) {
        auto buffer_access =
            buffers_by_id[buffer_id]->GetHandleForInProcessAccess();
        // Check that there is at least one non-zero byte in the frame data.
        bool found_non_zero_byte = false;
        for (uint32_t i = 0; i < buffer_access->mapped_size(); i++) {
          if (buffer_access->const_data()[i] != 0u) {
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
  i420_fake_device_proxy_->Start(settings_to_request,
                                 std::move(receiver_proxy));
  wait_loop.Run();
  EXPECT_FALSE(found_unexpected_all_zero_frame);
}
#endif  // defined(OS_LINUX)

}  // namespace video_capture
