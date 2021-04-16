// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/capture/video/linux/fake_device_provider.h"

#include <string>
#include <vector>

#include "media/capture/video/video_capture_device_descriptor.h"
#include "media/capture/video_capture_types.h"

namespace media {

FakeDeviceProvider::FakeDeviceProvider() = default;
FakeDeviceProvider::~FakeDeviceProvider() = default;

void FakeDeviceProvider::AddDevice(
    const VideoCaptureDeviceDescriptor& descriptor) {
  descriptors_.emplace_back(descriptor);
}

void FakeDeviceProvider::GetDeviceIds(
    std::vector<std::string>* target_container) {
  for (const auto& entry : descriptors_) {
    target_container->emplace_back(entry.device_id);
  }
}

std::string FakeDeviceProvider::GetDeviceModelId(const std::string& device_id) {
  auto iter =
      std::find_if(descriptors_.begin(), descriptors_.end(),
                   [&device_id](const VideoCaptureDeviceDescriptor& val) {
                     return val.device_id == device_id;
                   });
  if (iter == descriptors_.end())
    CHECK(false) << "Unknown device_id " << device_id;

  return iter->model_id;
}

std::string FakeDeviceProvider::GetDeviceDisplayName(
    const std::string& device_id) {
  auto iter =
      std::find_if(descriptors_.begin(), descriptors_.end(),
                   [&device_id](const VideoCaptureDeviceDescriptor& val) {
                     return val.device_id == device_id;
                   });
  if (iter == descriptors_.end())
    CHECK(false) << "Unknown device_id " << device_id;

  return iter->display_name();
}

VideoFacingMode FakeDeviceProvider::GetCameraFacing(
    const std::string& device_id,
    const std::string& model_id) {
  return MEDIA_VIDEO_FACING_NONE;
}

int FakeDeviceProvider::GetOrientation(const std::string& device_id,
                                       const std::string& model_id) {
  return 0;
}

}  // namespace media
