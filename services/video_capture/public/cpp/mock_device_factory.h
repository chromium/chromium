// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_VIDEO_CAPTURE_PUBLIC_CPP_MOCK_DEVICE_FACTORY_H_
#define SERVICES_VIDEO_CAPTURE_PUBLIC_CPP_MOCK_DEVICE_FACTORY_H_

#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "services/video_capture/public/mojom/device_factory.mojom.h"
#include "services/video_capture/public/mojom/devices_changed_observer.mojom.h"
#include "services/video_capture/public/mojom/producer.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace video_capture {

class MockDeviceFactory : public video_capture::mojom::DeviceFactory {
 public:
  MockDeviceFactory();
  ~MockDeviceFactory() override;

  void GetDeviceInfos(GetDeviceInfosCallback callback) override;
  void CreateDevice(
      const std::string& device_id,
      mojo::PendingReceiver<video_capture::mojom::Device> device_receiver,
      CreateDeviceCallback callback) override;
  void AddSharedMemoryVirtualDevice(
      const media::VideoCaptureDeviceInfo& device_info,
      mojo::PendingRemote<video_capture::mojom::Producer> producer,
      bool send_buffer_handles_to_producer_as_raw_file_descriptors,
      mojo::PendingReceiver<video_capture::mojom::SharedMemoryVirtualDevice>
          virtual_device_receiver) override;
  void AddTextureVirtualDevice(
      const media::VideoCaptureDeviceInfo& device_info,
      mojo::PendingReceiver<video_capture::mojom::TextureVirtualDevice>
          virtual_device_receiver) override;
  void RegisterVirtualDevicesChangedObserver(
      mojo::PendingRemote<video_capture::mojom::DevicesChangedObserver>
          observer,
      bool raise_event_if_virtual_devices_already_present) override {
    NOTIMPLEMENTED();
  }

  MOCK_METHOD1(DoGetDeviceInfos, void(GetDeviceInfosCallback& callback));
  MOCK_METHOD3(
      DoCreateDevice,
      void(const std::string& device_id,
           mojo::PendingReceiver<video_capture::mojom::Device>* device_receiver,
           CreateDeviceCallback& callback));
  MOCK_METHOD3(
      DoAddVirtualDevice,
      void(
          const media::VideoCaptureDeviceInfo& device_info,
          mojo::PendingRemote<video_capture::mojom::Producer> producer,
          mojo::PendingReceiver<video_capture::mojom::SharedMemoryVirtualDevice>
              virtual_device_receiver));
  MOCK_METHOD2(
      DoAddTextureVirtualDevice,
      void(const media::VideoCaptureDeviceInfo& device_info,
           mojo::PendingReceiver<video_capture::mojom::TextureVirtualDevice>
               virtual_device_receiver));
};

}  // namespace video_capture

#endif  // SERVICES_VIDEO_CAPTURE_PUBLIC_CPP_MOCK_DEVICE_FACTORY_H_
