// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/run_loop.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/video_capture/public/cpp/mock_video_frame_handler.h"
#include "services/video_capture/public/mojom/device.mojom.h"
#include "services/video_capture/public/mojom/video_frame_handler.mojom.h"
#include "services/video_capture/test/fake_device_descriptor_test.h"

using testing::_;
using testing::AtLeast;
using testing::InvokeWithoutArgs;

namespace video_capture {

class MockCreateDeviceRemoteCallback {
 public:
  MOCK_METHOD1(Run, void(mojom::DeviceAccessResultCode result_code));
};

// This alias ensures test output is easily attributed to this service's tests.
// TODO(rockot/chfremer): Consider just renaming the type.
using FakeVideoCaptureDeviceDescriptorTest = FakeDeviceDescriptorTest;

// Tests that when a client calls CreateDevice() but releases the message pipe
// passed in as |device_receiver| before the corresponding callback arrives,
// nothing bad happens and the callback still does arrive at some point after.
TEST_F(FakeVideoCaptureDeviceDescriptorTest,
       ClientReleasesDeviceHandleBeforeCreateCallbackHasArrived) {
  mojo::Remote<mojom::Device> device_remote;
  MockCreateDeviceRemoteCallback create_device_remote_callback;
  factory_->CreateDevice(
      i420_fake_device_info_.descriptor.device_id,
      device_remote.BindNewPipeAndPassReceiver(),
      base::BindOnce(&MockCreateDeviceRemoteCallback::Run,
                     base::Unretained(&create_device_remote_callback)));

  base::RunLoop wait_loop;
  EXPECT_CALL(create_device_remote_callback,
              Run(mojom::DeviceAccessResultCode::SUCCESS))
      .WillOnce(InvokeWithoutArgs([&wait_loop]() { wait_loop.Quit(); }));

  device_remote.reset();
  wait_loop.Run();
}

// Tests that when requesting a second remote for a device without closing the
// first one, the service revokes access to the first one by closing the
// connection.
TEST_F(FakeVideoCaptureDeviceDescriptorTest, AccessIsRevokedOnSecondAccess) {
  base::RunLoop wait_loop_1;
  mojo::Remote<mojom::Device> device_remote_1;
  bool device_access_1_revoked = false;
  MockCreateDeviceRemoteCallback create_device_remote_callback_1;
  EXPECT_CALL(create_device_remote_callback_1,
              Run(mojom::DeviceAccessResultCode::SUCCESS))
      .Times(1);
  factory_->CreateDevice(
      i420_fake_device_info_.descriptor.device_id,
      device_remote_1.BindNewPipeAndPassReceiver(),
      base::BindOnce(&MockCreateDeviceRemoteCallback::Run,
                     base::Unretained(&create_device_remote_callback_1)));
  device_remote_1.set_disconnect_handler(base::BindOnce(
      [](bool* access_revoked, base::RunLoop* wait_loop_1) {
        *access_revoked = true;
        wait_loop_1->Quit();
      },
      &device_access_1_revoked, &wait_loop_1));

  base::RunLoop wait_loop_2;
  mojo::Remote<mojom::Device> device_remote_2;
  bool device_access_2_revoked = false;
  MockCreateDeviceRemoteCallback create_device_remote_callback_2;
  EXPECT_CALL(create_device_remote_callback_2,
              Run(mojom::DeviceAccessResultCode::SUCCESS))
      .Times(1)
      .WillOnce(InvokeWithoutArgs([&wait_loop_2]() { wait_loop_2.Quit(); }));
  factory_->CreateDevice(
      i420_fake_device_info_.descriptor.device_id,
      device_remote_2.BindNewPipeAndPassReceiver(),
      base::BindOnce(&MockCreateDeviceRemoteCallback::Run,
                     base::Unretained(&create_device_remote_callback_2)));
  device_remote_2.set_disconnect_handler(
      base::BindOnce([](bool* access_revoked) { *access_revoked = true; },
                     &device_access_2_revoked));
  wait_loop_1.Run();
  wait_loop_2.Run();
  ASSERT_TRUE(device_access_1_revoked);
  ASSERT_FALSE(device_access_2_revoked);
}

// Tests that a second remote requested for a device can be used successfully.
TEST_F(FakeVideoCaptureDeviceDescriptorTest, CanUseSecondRequestedProxy) {
  mojo::Remote<mojom::Device> device_remote_1;
  factory_->CreateDevice(i420_fake_device_info_.descriptor.device_id,
                         device_remote_1.BindNewPipeAndPassReceiver(),
                         base::DoNothing());

  base::RunLoop wait_loop;
  mojo::Remote<mojom::Device> device_remote_2;
  factory_->CreateDevice(
      i420_fake_device_info_.descriptor.device_id,
      device_remote_2.BindNewPipeAndPassReceiver(),
      base::BindOnce(
          [](base::RunLoop* wait_loop,
             mojom::DeviceAccessResultCode result_code) { wait_loop->Quit(); },
          &wait_loop));
  wait_loop.Run();

  media::VideoCaptureParams arbitrary_requested_settings;
  arbitrary_requested_settings.requested_format.frame_size.SetSize(640, 480);
  arbitrary_requested_settings.requested_format.frame_rate = 15;
  arbitrary_requested_settings.resolution_change_policy =
      media::ResolutionChangePolicy::FIXED_RESOLUTION;
  arbitrary_requested_settings.power_line_frequency =
      media::PowerLineFrequency::FREQUENCY_DEFAULT;

  base::RunLoop wait_loop_2;
  mojo::PendingRemote<mojom::VideoFrameHandler> subscriber;
  MockVideoFrameHandler video_frame_handler(
      subscriber.InitWithNewPipeAndPassReceiver());
  EXPECT_CALL(video_frame_handler, DoOnNewBuffer(_, _)).Times(AtLeast(1));
  EXPECT_CALL(video_frame_handler, DoOnFrameReadyInBuffer(_, _, _, _))
      .WillRepeatedly(
          InvokeWithoutArgs([&wait_loop_2]() { wait_loop_2.Quit(); }));

  device_remote_2->Start(arbitrary_requested_settings, std::move(subscriber));
  wait_loop_2.Run();
}

}  // namespace video_capture
