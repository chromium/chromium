// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/video_capture/public/cpp/mock_video_source_provider.h"

namespace video_capture {

MockVideoSourceProvider::MockVideoSourceProvider() = default;

MockVideoSourceProvider::~MockVideoSourceProvider() = default;

void MockVideoSourceProvider::GetVideoSource(
    const std::string& device_id,
    mojo::PendingReceiver<video_capture::mojom::VideoSource> source_receiver) {
  DoGetVideoSource(device_id, &source_receiver);
}

void MockVideoSourceProvider::GetSourceInfos(GetSourceInfosCallback callback) {
  DoGetSourceInfos(callback);
}

void MockVideoSourceProvider::AddSharedMemoryVirtualDevice(
    const media::VideoCaptureDeviceInfo& device_info,
    mojo::PendingRemote<video_capture::mojom::Producer> producer,
    mojo::PendingReceiver<video_capture::mojom::SharedMemoryVirtualDevice>
        virtual_device_receiver) {
  DoAddVirtualDevice(device_info, std::move(producer),
                     std::move(virtual_device_receiver));
}

void MockVideoSourceProvider::AddTextureVirtualDevice(
    const media::VideoCaptureDeviceInfo& device_info,
    mojo::PendingReceiver<video_capture::mojom::TextureVirtualDevice>
        virtual_device_receiver) {
  DoAddTextureVirtualDevice(device_info, std::move(virtual_device_receiver));
}

void MockVideoSourceProvider::Close(CloseCallback callback) {
  DoClose(callback);
}

}  // namespace video_capture
