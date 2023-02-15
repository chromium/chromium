// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/capture/aura_window_to_mojo_device_adapter.h"

#include <memory>
#include <utility>

#include "base/task/bind_post_task.h"
#include "content/browser/media/capture/aura_window_video_capture_device.h"
#include "content/public/browser/desktop_media_id.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "services/video_capture/public/cpp/receiver_mojo_to_media_adapter.h"
#include "services/video_capture/public/mojom/device.mojom.h"

namespace content {
AuraWindowToMojoDeviceAdapter::AuraWindowToMojoDeviceAdapter(
    const content::DesktopMediaID& device_id)
    : device_(
          std::make_unique<content::AuraWindowVideoCaptureDevice>(device_id)) {}

AuraWindowToMojoDeviceAdapter::~AuraWindowToMojoDeviceAdapter() {
  // The AuraWindowVideoCaptureDevice expects to be stopped before being
  // destroyed. The video_capture::mojom::Device interface doesn't expose any
  // stop method; but we expect to be started as a self-owned receiver, so being
  // destroyed is the same as being told to stop.
  device_->StopAndDeAllocate();
}

void AuraWindowToMojoDeviceAdapter::Start(
    const media::VideoCaptureParams& requested_settings,
    mojo::PendingRemote<video_capture::mojom::VideoFrameHandler>
        handler_pending_remote) {
  auto receiver = std::make_unique<video_capture::ReceiverMojoToMediaAdapter>(
      mojo::Remote(std::move(handler_pending_remote)));
  device_->AllocateAndStartWithReceiver(requested_settings,
                                        std::move(receiver));
}

void AuraWindowToMojoDeviceAdapter::MaybeSuspend() {
  device_->MaybeSuspend();
}

void AuraWindowToMojoDeviceAdapter::Resume() {
  device_->Resume();
}

void AuraWindowToMojoDeviceAdapter::GetPhotoState(
    GetPhotoStateCallback callback) {
  media::VideoCaptureDevice::GetPhotoStateCallback scoped_callback =
      mojo::WrapCallbackWithDefaultInvokeIfNotRun(
          base::BindPostTaskToCurrentDefault(std::move(callback)), nullptr);
  device_->GetPhotoState(std::move(scoped_callback));
}

void AuraWindowToMojoDeviceAdapter::SetPhotoOptions(
    media::mojom::PhotoSettingsPtr settings,
    SetPhotoOptionsCallback callback) {
  media::mojom::ImageCapture::SetPhotoOptionsCallback scoped_callback =
      mojo::WrapCallbackWithDefaultInvokeIfNotRun(
          base::BindPostTaskToCurrentDefault(std::move(callback)), false);
  device_->SetPhotoOptions(std::move(settings), std::move(scoped_callback));
}

void AuraWindowToMojoDeviceAdapter::TakePhoto(TakePhotoCallback callback) {
  media::mojom::ImageCapture::TakePhotoCallback scoped_callback =
      mojo::WrapCallbackWithDefaultInvokeIfNotRun(
          base::BindPostTaskToCurrentDefault(std::move(callback)), nullptr);
  device_->TakePhoto(std::move(scoped_callback));
}

void AuraWindowToMojoDeviceAdapter::ProcessFeedback(
    const media::VideoCaptureFeedback& feedback) {
  device_->OnUtilizationReport(feedback);
}

void AuraWindowToMojoDeviceAdapter::RequestRefreshFrame() {
  device_->RequestRefreshFrame();
}

}  // namespace content
