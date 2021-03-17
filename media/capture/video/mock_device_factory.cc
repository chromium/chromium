// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/capture/video/mock_device_factory.h"

#include <utility>

namespace {

// Report a single hard-coded supported format to clients.
media::VideoCaptureFormat kSupportedFormat(gfx::Size(640, 480),
                                           25.0f,
                                           media::PIXEL_FORMAT_I420);

// Wraps a raw pointer to a media::VideoCaptureDevice and allows us to
// create a std::unique_ptr<media::VideoCaptureDevice> that delegates to it.
class RawPointerVideoCaptureDevice : public media::VideoCaptureDevice {
 public:
  explicit RawPointerVideoCaptureDevice(media::VideoCaptureDevice* device)
      : device_(device) {}

  // media::VideoCaptureDevice:
  void AllocateAndStart(const media::VideoCaptureParams& params,
                        std::unique_ptr<Client> client) override {
    device_->AllocateAndStart(params, std::move(client));
  }
  void RequestRefreshFrame() override { device_->RequestRefreshFrame(); }
  void StopAndDeAllocate() override { device_->StopAndDeAllocate(); }
  void GetPhotoState(GetPhotoStateCallback callback) override {
    device_->GetPhotoState(std::move(callback));
  }
  void SetPhotoOptions(media::mojom::PhotoSettingsPtr settings,
                       SetPhotoOptionsCallback callback) override {
    device_->SetPhotoOptions(std::move(settings), std::move(callback));
  }
  void TakePhoto(TakePhotoCallback callback) override {
    device_->TakePhoto(std::move(callback));
  }
  void OnUtilizationReport(int frame_feedback_id,
                           media::VideoFrameFeedback feedback) override {
    device_->OnUtilizationReport(frame_feedback_id, feedback);
  }

 private:
  media::VideoCaptureDevice* device_;
};

}  // anonymous namespace

namespace media {

MockDeviceFactory::MockDeviceFactory() = default;

MockDeviceFactory::~MockDeviceFactory() = default;

void MockDeviceFactory::AddMockDevice(
    media::VideoCaptureDevice* device,
    const media::VideoCaptureDeviceDescriptor& descriptor) {
  devices_[descriptor] = device;
}

void MockDeviceFactory::RemoveAllDevices() {
  devices_.clear();
}

std::unique_ptr<media::VideoCaptureDevice> MockDeviceFactory::CreateDevice(
    const media::VideoCaptureDeviceDescriptor& device_descriptor) {
  if (devices_.find(device_descriptor) == devices_.end())
    return nullptr;
  return std::make_unique<RawPointerVideoCaptureDevice>(
      devices_[device_descriptor]);
}

void MockDeviceFactory::GetDevicesInfo(GetDevicesInfoCallback callback) {
  std::vector<media::VideoCaptureDeviceInfo> result;
  for (const auto& entry : devices_) {
    result.emplace_back(entry.first);
    result.back().supported_formats.push_back(kSupportedFormat);
  }

  std::move(callback).Run(std::move(result));
}

}  // namespace media
