// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/capture/video/chromeos/video_capture_device_chromeos_halv3.h"

#include "base/strings/string_util.h"
#include "media/capture/video/chromeos/video_capture_device_chromeos_delegate.h"

namespace media {

constexpr char kVirtualPrefix[] = "VIRTUAL_";

VideoCaptureDeviceChromeOSHalv3::VideoCaptureDeviceChromeOSHalv3(
    VideoCaptureDeviceChromeOSDelegate* delegate,
    const VideoCaptureDeviceDescriptor& vcd_descriptor)
    : vcd_delegate_(delegate) {
  client_type_ = base::StartsWith(vcd_descriptor.device_id, kVirtualPrefix)
                     ? ClientType::kVideoClient
                     : ClientType::kPreviewClient;
}

VideoCaptureDeviceChromeOSHalv3::~VideoCaptureDeviceChromeOSHalv3() {
  // TODO(b/335574894) : Self deleting object is kind of a dangerous pattern.
  // Refine the pattern when refactoring.
  vcd_delegate_.ExtractAsDangling()->Shutdown();
}

// VideoCaptureDevice implementation.
void VideoCaptureDeviceChromeOSHalv3::AllocateAndStart(
    const VideoCaptureParams& params,
    std::unique_ptr<Client> client) {
  vcd_delegate_->AllocateAndStart(params, std::move(client), client_type_);
}

void VideoCaptureDeviceChromeOSHalv3::StopAndDeAllocate() {
  vcd_delegate_->StopAndDeAllocate(client_type_);
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
