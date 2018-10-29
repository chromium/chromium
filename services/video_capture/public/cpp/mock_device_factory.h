// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_VIDEO_CAPTURE_PUBLIC_CPP_MOCK_DEVICE_FACTORY_H_
#define SERVICES_VIDEO_CAPTURE_PUBLIC_CPP_MOCK_DEVICE_FACTORY_H_

#include "services/video_capture/public/mojom/device_factory.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace video_capture {

class MockDeviceFactory : public video_capture::mojom::DeviceFactory {
 public:
  MockDeviceFactory();
  ~MockDeviceFactory() override;

  void GetDeviceInfos(GetDeviceInfosCallback callback) override;
  void CreateDevice(const std::string& device_id,
                    video_capture::mojom::DeviceRequest device_request,
                    CreateDeviceCallback callback) override;
  void AddSharedMemoryVirtualDevice(
      const media::VideoCaptureDeviceInfo& device_info,
      video_capture::mojom::ProducerPtr producer,
      bool send_buffer_handles_to_producer_as_raw_file_descriptors,
      video_capture::mojom::SharedMemoryVirtualDeviceRequest virtual_device)
      override;
  void AddTextureVirtualDevice(const media::VideoCaptureDeviceInfo& device_info,
                               video_capture::mojom::TextureVirtualDeviceRequest
                                   virtual_device) override;
  void RegisterVirtualDevicesChangedObserver(
      video_capture::mojom::DevicesChangedObserverPtr observer) override {
    NOTIMPLEMENTED();
  }

  MOCK_METHOD1(DoGetDeviceInfos, void(GetDeviceInfosCallback& callback));
  MOCK_METHOD3(DoCreateDevice,
               void(const std::string& device_id,
                    video_capture::mojom::DeviceRequest* device_request,
                    CreateDeviceCallback& callback));
  MOCK_METHOD3(DoAddVirtualDevice,
               void(const media::VideoCaptureDeviceInfo& device_info,
                    video_capture::mojom::ProducerProxy* producer,
                    video_capture::mojom::SharedMemoryVirtualDeviceRequest*
                        virtual_device_request));
  MOCK_METHOD2(
      DoAddTextureVirtualDevice,
      void(const media::VideoCaptureDeviceInfo& device_info,
           video_capture::mojom::TextureVirtualDeviceRequest* virtual_device));
};

}  // namespace video_capture

#endif  // SERVICES_VIDEO_CAPTURE_PUBLIC_CPP_MOCK_DEVICE_FACTORY_H_
