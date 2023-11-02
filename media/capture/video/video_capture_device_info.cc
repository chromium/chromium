// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/capture/video/video_capture_device_info.h"

namespace media {

VideoCaptureDeviceInfo::VideoCaptureDeviceInfo() = default;

VideoCaptureDeviceInfo::VideoCaptureDeviceInfo(
    VideoCaptureDeviceDescriptor descriptor)
    : descriptor(descriptor) {}

VideoCaptureDeviceInfo::VideoCaptureDeviceInfo(
    const VideoCaptureDeviceInfo& other) = default;

VideoCaptureDeviceInfo::~VideoCaptureDeviceInfo() = default;

VideoCaptureDeviceInfo& VideoCaptureDeviceInfo::operator=(
    const VideoCaptureDeviceInfo& other) = default;

}  // namespace media
