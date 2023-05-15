// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/video_capture/ash/video_capture_device_ash.h"

#include <memory>
#include <utility>

#include "media/capture/mojom/image_capture.mojom.h"
#include "services/video_capture/ash/video_frame_handler_ash.h"

namespace crosapi {

VideoCaptureDeviceAsh::VideoCaptureDeviceAsh(
    mojo::PendingRemote<video_capture::mojom::Device> device_remote)
    : device_(std::move(device_remote)) {}

VideoCaptureDeviceAsh::VideoCaptureDeviceAsh(
    mojo::PendingReceiver<crosapi::mojom::VideoCaptureDevice> proxy_receiver,
    mojo::PendingRemote<video_capture::mojom::Device> device_remote,
    base::OnceClosure cleanup_callback)
    : VideoCaptureDeviceAsh(std::move(device_remote)) {
  receiver_.Bind(std::move(proxy_receiver));
  receiver_.set_disconnect_handler(std::move(cleanup_callback));
}

VideoCaptureDeviceAsh::VideoCaptureDeviceAsh(
    mojo::PendingReceiver<crosapi::mojom::VideoCaptureDevice> proxy_receiver,
    video_capture::mojom::Device* device,
    base::OnceClosure cleanup_callback) {
  receiver_.Bind(std::move(proxy_receiver));
  receiver_.set_disconnect_handler(std::move(cleanup_callback));
  device_proxy_ =
      std::make_unique<mojo::Receiver<video_capture::mojom::Device>>(device);
  device_proxy_->Bind(device_.BindNewPipeAndPassReceiver());
}

VideoCaptureDeviceAsh::~VideoCaptureDeviceAsh() = default;

void VideoCaptureDeviceAsh::Start(
    const media::VideoCaptureParams& requested_settings,
    mojo::PendingRemote<crosapi::mojom::VideoFrameHandler> proxy_handler) {
  mojo::PendingRemote<video_capture::mojom::VideoFrameHandler> handler_remote;
  handler_ = std::make_unique<VideoFrameHandlerAsh>(
      handler_remote.InitWithNewPipeAndPassReceiver(),
      std::move(proxy_handler));
  device_->Start(std::move(requested_settings), std::move(handler_remote));
}

void VideoCaptureDeviceAsh::MaybeSuspend() {
  device_->MaybeSuspend();
}

void VideoCaptureDeviceAsh::Resume() {
  device_->Resume();
}

void VideoCaptureDeviceAsh::GetPhotoState(GetPhotoStateCallback callback) {
  device_->GetPhotoState(std::move(callback));
}

void VideoCaptureDeviceAsh::SetPhotoOptions(
    media::mojom::PhotoSettingsPtr settings,
    SetPhotoOptionsCallback callback) {
  device_->SetPhotoOptions(std::move(settings), std::move(callback));
}

void VideoCaptureDeviceAsh::TakePhoto(TakePhotoCallback callback) {
  device_->TakePhoto(std::move(callback));
}

void VideoCaptureDeviceAsh::ProcessFeedback(
    const media::VideoCaptureFeedback& feedback) {
  device_->ProcessFeedback(std::move(feedback));
}

void VideoCaptureDeviceAsh::RequestRefreshFrame() {
  device_->RequestRefreshFrame();
}

}  // namespace crosapi
