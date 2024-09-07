// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_MEDIA_IN_PROCESS_VIDEO_CAPTURE_PROVIDER_H_
#define CONTENT_BROWSER_RENDERER_HOST_MEDIA_IN_PROCESS_VIDEO_CAPTURE_PROVIDER_H_

#include "base/memory/ref_counted.h"
#include "base/task/single_thread_task_runner.h"
#include "content/browser/media/capture/native_screen_capture_picker.h"
#include "content/browser/renderer_host/media/video_capture_provider.h"
#include "content/common/content_export.h"

namespace content {

class CONTENT_EXPORT InProcessVideoCaptureProvider
    : public VideoCaptureProvider {
 public:
  ~InProcessVideoCaptureProvider() override;

  static std::unique_ptr<VideoCaptureProvider> CreateInstanceForScreenCapture(
      scoped_refptr<base::SingleThreadTaskRunner> device_task_runner);

  std::unique_ptr<VideoCaptureDeviceLauncher> CreateDeviceLauncher() override;

  void OpenNativeScreenCapturePicker(
      DesktopMediaID::Type type,
      base::OnceCallback<void(DesktopMediaID::Id)> created_callback,
      base::OnceCallback<void(webrtc::DesktopCapturer::Source)> picker_callback,
      base::OnceCallback<void()> cancel_callback,
      base::OnceCallback<void()> error_callback) override;

  void CloseNativeScreenCapturePicker(DesktopMediaID device_id) override;

 private:
  explicit InProcessVideoCaptureProvider(
      scoped_refptr<base::SingleThreadTaskRunner> device_task_runner);

  void GetDeviceInfosAsync(GetDeviceInfosCallback result_callback) override;

  std::unique_ptr<NativeScreenCapturePicker> native_screen_capture_picker_;
  // The message loop of media stream device thread, where VCD's live.
  const scoped_refptr<base::SingleThreadTaskRunner> device_task_runner_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_MEDIA_IN_PROCESS_VIDEO_CAPTURE_PROVIDER_H_
