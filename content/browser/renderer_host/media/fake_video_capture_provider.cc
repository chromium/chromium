// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/media/fake_video_capture_provider.h"

#include <memory>

#include "content/browser/renderer_host/media/fake_video_capture_device_launcher.h"
#include "media/capture/video/fake_video_capture_device_factory.h"

namespace content {

FakeVideoCaptureProvider::FakeVideoCaptureProvider(
    std::unique_ptr<::media::VideoCaptureDeviceFactory> device_factory)
    : system_(std::move(device_factory)) {}

FakeVideoCaptureProvider::FakeVideoCaptureProvider()
    : FakeVideoCaptureProvider(
          std::make_unique<media::FakeVideoCaptureDeviceFactory>()) {}

FakeVideoCaptureProvider::~FakeVideoCaptureProvider() = default;

void FakeVideoCaptureProvider::GetDeviceInfosAsync(
    GetDeviceInfosCallback result_callback) {
  system_.GetDeviceInfosAsync(
      base::BindOnce(std::move(result_callback),
                     media::mojom::DeviceEnumerationResult::kSuccess));
}

std::unique_ptr<VideoCaptureDeviceLauncher>
FakeVideoCaptureProvider::CreateDeviceLauncher() {
  return std::make_unique<FakeVideoCaptureDeviceLauncher>(&system_);
}

void FakeVideoCaptureProvider::OpenNativeScreenCapturePicker(
    DesktopMediaID::Type type,
    base::OnceCallback<void(DesktopMediaID::Id)> created_callback,
    base::OnceCallback<void(webrtc::DesktopCapturer::Source)> picker_callback,
    base::OnceCallback<void()> cancel_callback,
    base::OnceCallback<void()> error_callback) {}

void FakeVideoCaptureProvider::CloseNativeScreenCapturePicker(
    DesktopMediaID device_id) {}
}  // namespace content
