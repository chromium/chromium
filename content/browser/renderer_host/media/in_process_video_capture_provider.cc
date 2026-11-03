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
  // Destroy `native_screen_capture_picker_` on `device_task_runner_`, where
  // all its methods run. The tasks bound with `base::Unretained()` in this
  // file rely on this: they are posted to the same runner before this task,
  // so they run while the picker is still alive.
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
    media::mojom::VideoCaptureDeviceEnumerationMode enumeration_mode,
    GetDeviceInfosCallback result_callback) {
  NOTREACHED();
}

std::unique_ptr<VideoCaptureDeviceLauncher>
InProcessVideoCaptureProvider::CreateDeviceLauncher() {
  return std::make_unique<InProcessVideoCaptureDeviceLauncher>(
      device_task_runner_, native_screen_capture_picker_.get());
}

std::unique_ptr<VideoCaptureDeviceLauncher>
InProcessVideoCaptureProvider::CreateDeviceLauncherWithSharedMemoryBuffers() {
  auto launcher = std::make_unique<InProcessVideoCaptureDeviceLauncher>(
      device_task_runner_, native_screen_capture_picker_.get());
  launcher->SetPreferSharedMemoryBuffers();
  return launcher;
}

void InProcessVideoCaptureProvider::OpenNativeScreenCapturePicker(
    DesktopMediaID::Type type,
    base::OnceCallback<void(DesktopMediaID::Id)> created_callback,
    base::OnceCallback<void(webrtc::DesktopCapturer::Source)> picker_callback,
    base::OnceCallback<void()> cancel_callback,
    base::OnceCallback<void()> error_callback,
    base::OnceCallback<void(DesktopMediaID::Id)> stop_audio_callback) {
  CHECK(native_screen_capture_picker_);

  // `base::Unretained` is safe, see ~InProcessVideoCaptureProvider().
  device_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&NativeScreenCapturePicker::Open,
                     base::Unretained(native_screen_capture_picker_.get()),
                     type, std::move(created_callback),
                     std::move(picker_callback), std::move(cancel_callback),
                     std::move(error_callback),
                     std::move(stop_audio_callback)));
}

void InProcessVideoCaptureProvider::CloseNativeScreenCapturePicker(
    DesktopMediaID device_id) {
  if (!native_screen_capture_picker_) {
    return;
  }

  // `base::Unretained` is safe, see ~InProcessVideoCaptureProvider().
  device_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&NativeScreenCapturePicker::Close,
                     base::Unretained(native_screen_capture_picker_.get()),
                     device_id));
}

#if BUILDFLAG(IS_MAC)
void InProcessVideoCaptureProvider::GetApplicationAudioCaptureId(
    DesktopMediaID::Id session_id,
    base::OnceCallback<
        void(const std::optional<desktop_capture::ApplicationAudioCaptureId>&)>
        callback) {
  if (!native_screen_capture_picker_) {
    std::move(callback).Run(std::nullopt);
    return;
  }

  // `base::Unretained` is safe, see ~InProcessVideoCaptureProvider().
  device_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&NativeScreenCapturePicker::GetApplicationAudioCaptureId,
                     base::Unretained(native_screen_capture_picker_.get()),
                     session_id, std::move(callback)));
}
#endif

}  // namespace content
