// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_MEDIA_IN_PROCESS_VIDEO_CAPTURE_PROVIDER_H_
#define CONTENT_BROWSER_RENDERER_HOST_MEDIA_IN_PROCESS_VIDEO_CAPTURE_PROVIDER_H_

#include "base/memory/ref_counted.h"
#include "base/task/single_thread_task_runner.h"
#include "content/browser/renderer_host/media/video_capture_provider.h"
#include "content/common/content_export.h"
#include "media/capture/video/video_capture_system.h"

namespace content {

class CONTENT_EXPORT InProcessVideoCaptureProvider
    : public VideoCaptureProvider {
 public:
  InProcessVideoCaptureProvider(
      std::unique_ptr<media::VideoCaptureSystem> video_capture_system,
      scoped_refptr<base::SingleThreadTaskRunner> device_task_runner,
      base::RepeatingCallback<void(const std::string&)> emit_log_message_cb);

  ~InProcessVideoCaptureProvider() override;

  static std::unique_ptr<VideoCaptureProvider>
  CreateInstanceForNonDeviceCapture(
      scoped_refptr<base::SingleThreadTaskRunner> device_task_runner,
      base::RepeatingCallback<void(const std::string&)> emit_log_message_cb);

  static std::unique_ptr<VideoCaptureProvider> CreateInstance(
      std::unique_ptr<media::VideoCaptureSystem> video_capture_system,
      scoped_refptr<base::SingleThreadTaskRunner> device_task_runner,
      base::RepeatingCallback<void(const std::string&)> emit_log_message_cb);

  void GetDeviceInfosAsync(GetDeviceInfosCallback result_callback) override;

  std::unique_ptr<VideoCaptureDeviceLauncher> CreateDeviceLauncher() override;

 private:
  // Can be nullptr.
  std::unique_ptr<media::VideoCaptureSystem> video_capture_system_;
  // The message loop of media stream device thread, where VCD's live.
  const scoped_refptr<base::SingleThreadTaskRunner> device_task_runner_;
  base::RepeatingCallback<void(const std::string&)> emit_log_message_cb_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_MEDIA_IN_PROCESS_VIDEO_CAPTURE_PROVIDER_H_
