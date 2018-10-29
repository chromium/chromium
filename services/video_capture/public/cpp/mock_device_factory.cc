// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/video_capture/public/cpp/mock_device_factory.h"

namespace video_capture {

MockDeviceFactory::MockDeviceFactory() = default;

MockDeviceFactory::~MockDeviceFactory() = default;

void MockDeviceFactory::GetDeviceInfos(GetDeviceInfosCallback callback) {
  DoGetDeviceInfos(callback);
}

void MockDeviceFactory::CreateDevice(
    const std::string& device_id,
    video_capture::mojom::DeviceRequest device_request,
    CreateDeviceCallback callback) {
  DoCreateDevice(device_id, &device_request, callback);
}

void MockDeviceFactory::AddSharedMemoryVirtualDevice(
    const media::VideoCaptureDeviceInfo& device_info,
    video_capture::mojom::ProducerPtr producer,
    bool send_buffer_handles_to_producer_as_raw_file_descriptors,
    video_capture::mojom::SharedMemoryVirtualDeviceRequest virtual_device) {
  DoAddVirtualDevice(device_info, producer.get(), &virtual_device);
}

void MockDeviceFactory::AddTextureVirtualDevice(
    const media::VideoCaptureDeviceInfo& device_info,
    video_capture::mojom::TextureVirtualDeviceRequest virtual_device) {
  DoAddTextureVirtualDevice(device_info, &virtual_device);
}

}  // namespace video_capture
