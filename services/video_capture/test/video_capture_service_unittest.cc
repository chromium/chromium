// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/test/mock_callback.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/video_capture/public/cpp/mock_producer.h"
#include "services/video_capture/public/mojom/constants.mojom.h"
#include "services/video_capture/public/mojom/device.mojom.h"
#include "services/video_capture/public/mojom/device_factory.mojom.h"
#include "services/video_capture/public/mojom/virtual_device.mojom.h"
#include "services/video_capture/test/mock_devices_changed_observer.h"
#include "services/video_capture/test/video_capture_service_test.h"

using testing::_;
using testing::Exactly;
using testing::Invoke;
using testing::InvokeWithoutArgs;

namespace video_capture {

// Tests that an answer arrives from the service when calling
// GetDeviceInfos().
TEST_F(VideoCaptureServiceTest, GetDeviceInfosCallbackArrives) {
  base::RunLoop wait_loop;
  EXPECT_CALL(device_info_receiver_, Run(_))
      .Times(Exactly(1))
      .WillOnce(InvokeWithoutArgs([&wait_loop]() { wait_loop.Quit(); }));

  factory_->GetDeviceInfos(device_info_receiver_.Get());
  wait_loop.Run();
}

TEST_F(VideoCaptureServiceTest, FakeDeviceFactoryEnumeratesThreeDevices) {
  base::RunLoop wait_loop;
  size_t num_devices_enumerated = 0;
  EXPECT_CALL(device_info_receiver_, Run(_))
      .Times(Exactly(1))
      .WillOnce(
          Invoke([&wait_loop, &num_devices_enumerated](
                     const std::vector<media::VideoCaptureDeviceInfo>& infos) {
            num_devices_enumerated = infos.size();
            wait_loop.Quit();
          }));

  factory_->GetDeviceInfos(device_info_receiver_.Get());
  wait_loop.Run();
  ASSERT_EQ(3u, num_devices_enumerated);
}

// Tests that an added virtual device will be returned in the callback
// when calling GetDeviceInfos.
TEST_F(VideoCaptureServiceTest, VirtualDeviceEnumeratedAfterAdd) {
  const std::string virtual_device_id = "/virtual/device";
  auto device_context = AddSharedMemoryVirtualDevice(virtual_device_id);

  base::RunLoop wait_loop;
  EXPECT_CALL(device_info_receiver_, Run(_))
      .Times(Exactly(1))
      .WillOnce(
          Invoke([&wait_loop, virtual_device_id](
                     const std::vector<media::VideoCaptureDeviceInfo>& infos) {
            bool virtual_device_enumerated = false;
            for (const auto& info : infos) {
              if (info.descriptor.device_id == virtual_device_id) {
                virtual_device_enumerated = true;
                break;
              }
            }
            EXPECT_TRUE(virtual_device_enumerated);
            wait_loop.Quit();
          }));
  factory_->GetDeviceInfos(device_info_receiver_.Get());
  wait_loop.Run();
}

TEST_F(VideoCaptureServiceTest,
       AddingAndRemovingVirtualDevicesRaisesDevicesChangedEvent) {
  mojo::PendingRemote<mojom::DevicesChangedObserver> observer;
  MockDevicesChangedObserver mock_observer;
  mojo::Receiver<mojom::DevicesChangedObserver> observer_receiver(
      &mock_observer, observer.InitWithNewPipeAndPassReceiver());
  factory_->RegisterVirtualDevicesChangedObserver(
      std::move(observer),
      false /*raise_event_if_virtual_devices_already_present*/);

  std::unique_ptr<SharedMemoryVirtualDeviceContext> device_context_1;
  {
    base::RunLoop run_loop;
    EXPECT_CALL(mock_observer, OnDevicesChanged())
        .WillOnce(Invoke([&run_loop]() { run_loop.Quit(); }));
    device_context_1 = AddSharedMemoryVirtualDevice("TestDevice1");
    run_loop.Run();
  }

  mojo::PendingRemote<mojom::TextureVirtualDevice> device_context_2;
  {
    base::RunLoop run_loop;
    EXPECT_CALL(mock_observer, OnDevicesChanged())
        .WillOnce(Invoke([&run_loop]() { run_loop.Quit(); }));
    device_context_2 = AddTextureVirtualDevice("TestDevice2");
    run_loop.Run();
  }

  {
    base::RunLoop run_loop;
    EXPECT_CALL(mock_observer, OnDevicesChanged())
        .WillOnce(Invoke([&run_loop]() { run_loop.Quit(); }));
    device_context_1.reset();
    run_loop.Run();
  }

  {
    base::RunLoop run_loop;
    EXPECT_CALL(mock_observer, OnDevicesChanged())
        .WillOnce(Invoke([&run_loop]() { run_loop.Quit(); }));
    device_context_2.reset();
    run_loop.Run();
  }
}

// Tests that disconnecting a devices changed observer does not lead to any
// crash or bad state.
TEST_F(VideoCaptureServiceTest,
       AddAndRemoveVirtualDeviceAfterObserverHasDisconnected) {
  mojo::PendingRemote<mojom::DevicesChangedObserver> observer;
  MockDevicesChangedObserver mock_observer;
  mojo::Receiver<mojom::DevicesChangedObserver> observer_receiver(
      &mock_observer, observer.InitWithNewPipeAndPassReceiver());
  factory_->RegisterVirtualDevicesChangedObserver(
      std::move(observer),
      false /*raise_event_if_virtual_devices_already_present*/);

  // Disconnect observer
  observer_receiver.reset();

  auto device_context = AddTextureVirtualDevice("TestDevice");
  device_context.reset();
}

// Tests that VideoCaptureDeviceFactory::CreateDevice() returns an error
// code when trying to create a device for an invalid descriptor.
TEST_F(VideoCaptureServiceTest, ErrorCodeOnCreateDeviceForInvalidDescriptor) {
  const std::string invalid_device_id = "invalid";
  base::RunLoop wait_loop;
  mojo::Remote<mojom::Device> fake_device_remote;
  base::MockCallback<mojom::DeviceFactory::CreateDeviceCallback>
      create_device_remote_callback;
  EXPECT_CALL(create_device_remote_callback,
              Run(mojom::DeviceAccessResultCode::ERROR_DEVICE_NOT_FOUND))
      .Times(1)
      .WillOnce(InvokeWithoutArgs([&wait_loop]() { wait_loop.Quit(); }));
  factory_->GetDeviceInfos(device_info_receiver_.Get());
  factory_->CreateDevice(invalid_device_id,
                         fake_device_remote.BindNewPipeAndPassReceiver(),
                         create_device_remote_callback.Get());
  wait_loop.Run();
}

// Test that CreateDevice() will succeed when trying to create a device
// for an added virtual device.
TEST_F(VideoCaptureServiceTest, CreateDeviceSuccessForVirtualDevice) {
  base::RunLoop wait_loop;
  const std::string virtual_device_id = "/virtual/device";
  auto device_context = AddSharedMemoryVirtualDevice(virtual_device_id);

  base::MockCallback<mojom::DeviceFactory::CreateDeviceCallback>
      create_device_remote_callback;
  EXPECT_CALL(create_device_remote_callback,
              Run(mojom::DeviceAccessResultCode::SUCCESS))
      .Times(1)
      .WillOnce(InvokeWithoutArgs([&wait_loop]() { wait_loop.Quit(); }));
  mojo::Remote<mojom::Device> device_remote;
  factory_->CreateDevice(virtual_device_id,
                         device_remote.BindNewPipeAndPassReceiver(),
                         create_device_remote_callback.Get());
  wait_loop.Run();
}

}  // namespace video_capture
