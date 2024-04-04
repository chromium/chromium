// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/video_capture/lacros/device_proxy_lacros.h"

#include <memory>
#include <utility>

#include "media/capture/mojom/image_capture.mojom.h"
#include "media/capture/video/video_capture_device_client.h"
#include "services/video_capture/lacros/video_frame_handler_proxy_lacros.h"

namespace video_capture {

DeviceProxyLacros::DeviceProxyLacros(
    std::optional<mojo::PendingReceiver<mojom::Device>> device_receiver,
    mojo::PendingRemote<crosapi::mojom::VideoCaptureDevice> proxy_remote,
    base::OnceClosure cleanup_callback)
    : device_(std::move(proxy_remote)) {
  if (device_receiver) {
    receiver_.Bind(std::move(*device_receiver));
    receiver_.set_disconnect_handler(std::move(cleanup_callback));
  }

  // Note that currently all versioned calls that we need to make are
  // best effort, and can just be dropped if we haven't gotten an updated
  // version yet. If that changes, we'll need to track that we have an
  // outstanding query and respond accordingly.
  device_.QueryVersion(base::DoNothing());
}

DeviceProxyLacros::~DeviceProxyLacros() = default;

void DeviceProxyLacros::Start(
    const media::VideoCaptureParams& requested_settings,
    mojo::PendingRemote<mojom::VideoFrameHandler> handler) {
  mojo::PendingRemote<crosapi::mojom::VideoFrameHandler> proxy_handler_remote;
  handler_ = std::make_unique<VideoFrameHandlerProxyLacros>(
      proxy_handler_remote.InitWithNewPipeAndPassReceiver(), std::move(handler),
      /*handler_remote_in_process=*/nullptr);
  device_->Start(std::move(requested_settings),
                 std::move(proxy_handler_remote));
}

void DeviceProxyLacros::StartInProcess(
    const media::VideoCaptureParams& requested_settings,
    const base::WeakPtr<media::VideoFrameReceiver>& frame_handler,
    media::VideoEffectsContext context) {
  mojo::PendingRemote<crosapi::mojom::VideoFrameHandler> proxy_handler_remote;
  handler_ = std::make_unique<VideoFrameHandlerProxyLacros>(
      proxy_handler_remote.InitWithNewPipeAndPassReceiver(),
      /*handler_remote=*/std::nullopt, frame_handler);
  device_->Start(std::move(requested_settings),
                 std::move(proxy_handler_remote));
}

void DeviceProxyLacros::MaybeSuspend() {
  device_->MaybeSuspend();
}

void DeviceProxyLacros::Resume() {
  device_->Resume();
}

void DeviceProxyLacros::GetPhotoState(GetPhotoStateCallback callback) {
  device_->GetPhotoState(std::move(callback));
}

void DeviceProxyLacros::SetPhotoOptions(media::mojom::PhotoSettingsPtr settings,
                                        SetPhotoOptionsCallback callback) {
  device_->SetPhotoOptions(std::move(settings), std::move(callback));
}

void DeviceProxyLacros::TakePhoto(TakePhotoCallback callback) {
  device_->TakePhoto(std::move(callback));
}

void DeviceProxyLacros::ProcessFeedback(
    const media::VideoCaptureFeedback& feedback) {
  device_->ProcessFeedback(std::move(feedback));
}

void DeviceProxyLacros::RequestRefreshFrame() {
  if (device_.version() >=
      int{crosapi::mojom::VideoCaptureDevice::MethodMinVersions::
              kRequestRefreshFrameMinVersion}) {
    device_->RequestRefreshFrame();
  }
}

}  // namespace video_capture
