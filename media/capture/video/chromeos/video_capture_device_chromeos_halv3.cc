// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/capture/video/chromeos/video_capture_device_chromeos_halv3.h"

#include "media/capture/video/chromeos/video_capture_device_chromeos_delegate.h"

namespace media {

VideoCaptureDeviceChromeOSHalv3::VideoCaptureDeviceChromeOSHalv3(
    std::unique_ptr<VideoCaptureDeviceChromeOSDelegate> delegate)
    : vcd_delegate_(std::move(delegate)) {}

VideoCaptureDeviceChromeOSHalv3::~VideoCaptureDeviceChromeOSHalv3() {
}

// VideoCaptureDevice implementation.
void VideoCaptureDeviceChromeOSHalv3::AllocateAndStart(
    const VideoCaptureParams& params,
    std::unique_ptr<Client> client) {
  vcd_delegate_->AllocateAndStart(params, std::move(client));
}

void VideoCaptureDeviceChromeOSHalv3::StopAndDeAllocate() {
  vcd_delegate_->StopAndDeAllocate();
}

void VideoCaptureDeviceChromeOSHalv3::TakePhoto(TakePhotoCallback callback) {
  vcd_delegate_->TakePhoto(std::move(callback));
}

void VideoCaptureDeviceChromeOSHalv3::GetPhotoState(
    GetPhotoStateCallback callback) {
  vcd_delegate_->GetPhotoState(std::move(callback));
}

void VideoCaptureDeviceChromeOSHalv3::SetPhotoOptions(
    mojom::PhotoSettingsPtr settings,
    SetPhotoOptionsCallback callback) {
  vcd_delegate_->SetPhotoOptions(std::move(settings), std::move(callback));
}

}  // namespace media
