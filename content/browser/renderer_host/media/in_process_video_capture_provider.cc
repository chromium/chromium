// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/media/in_process_video_capture_provider.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/task/single_thread_task_runner.h"
#include "content/browser/renderer_host/media/in_process_video_capture_device_launcher.h"

namespace content {

InProcessVideoCaptureProvider::InProcessVideoCaptureProvider(
    scoped_refptr<base::SingleThreadTaskRunner> device_task_runner)
    : native_screen_capture_picker_(MaybeCreateNativeScreenCapturePicker()),
      device_task_runner_(std::move(device_task_runner)) {}

InProcessVideoCaptureProvider::~InProcessVideoCaptureProvider() {
  // Destruct the `native_screen_capture_picker_` on the `device_task_runner_`
  // as all its functions are run on `device_task_runner_` as well.
  device_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce([](std::unique_ptr<NativeScreenCapturePicker>) {},
                     std::move(native_screen_capture_picker_)));
}

// static
std::unique_ptr<VideoCaptureProvider>
InProcessVideoCaptureProvider::CreateInstanceForScreenCapture(
    scoped_refptr<base::SingleThreadTaskRunner> device_task_runner) {
  // Using base::WrapUnique<>(new ...) to access private constructor.
  return base::WrapUnique<InProcessVideoCaptureProvider>(
      new InProcessVideoCaptureProvider(std::move(device_task_runner)));
}

void InProcessVideoCaptureProvider::GetDeviceInfosAsync(
    GetDeviceInfosCallback result_callback) {
  NOTREACHED_IN_MIGRATION();
}

std::unique_ptr<VideoCaptureDeviceLauncher>
InProcessVideoCaptureProvider::CreateDeviceLauncher() {
  return std::make_unique<InProcessVideoCaptureDeviceLauncher>(
      device_task_runner_, native_screen_capture_picker_.get());
}

void InProcessVideoCaptureProvider::OpenNativeScreenCapturePicker(
    DesktopMediaID::Type type,
    base::OnceCallback<void(DesktopMediaID::Id)> created_callback,
    base::OnceCallback<void(webrtc::DesktopCapturer::Source)> picker_callback,
    base::OnceCallback<void()> cancel_callback,
    base::OnceCallback<void()> error_callback) {
  CHECK(native_screen_capture_picker_);

  device_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&NativeScreenCapturePicker::Open,
                     native_screen_capture_picker_->GetWeakPtr(), type,
                     std::move(created_callback), std::move(picker_callback),
                     std::move(cancel_callback), std::move(error_callback)));
}

void InProcessVideoCaptureProvider::CloseNativeScreenCapturePicker(
    DesktopMediaID device_id) {
  if (!native_screen_capture_picker_) {
    return;
  }

  device_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&NativeScreenCapturePicker::Close,
                     native_screen_capture_picker_->GetWeakPtr(), device_id));
}

}  // namespace content
