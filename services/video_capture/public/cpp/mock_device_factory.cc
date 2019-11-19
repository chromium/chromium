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
    mojo::PendingReceiver<video_capture::mojom::Device> device_receiver,
    CreateDeviceCallback callback) {
  DoCreateDevice(device_id, &device_receiver, callback);
}

void MockDeviceFactory::AddSharedMemoryVirtualDevice(
    const media::VideoCaptureDeviceInfo& device_info,
    mojo::PendingRemote<video_capture::mojom::Producer> producer,
    bool send_buffer_handles_to_producer_as_raw_file_descriptors,
    mojo::PendingReceiver<video_capture::mojom::SharedMemoryVirtualDevice>
        virtual_device_receiver) {
  DoAddVirtualDevice(device_info, std::move(producer),
                     std::move(virtual_device_receiver));
}

void MockDeviceFactory::AddTextureVirtualDevice(
    const media::VideoCaptureDeviceInfo& device_info,
    mojo::PendingReceiver<video_capture::mojom::TextureVirtualDevice>
        virtual_device_receiver) {
  DoAddTextureVirtualDevice(device_info, std::move(virtual_device_receiver));
}

}  // namespace video_capture
