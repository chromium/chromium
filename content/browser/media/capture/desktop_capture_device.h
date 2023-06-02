// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MEDIA_CAPTURE_DESKTOP_CAPTURE_DEVICE_H_
#define CONTENT_BROWSER_MEDIA_CAPTURE_DESKTOP_CAPTURE_DEVICE_H_

#include <memory>

#include "base/memory/ref_counted.h"
#include "base/task/single_thread_task_runner.h"
#include "base/threading/thread.h"
#include "base/time/time.h"
#include "content/common/content_export.h"
#include "content/public/browser/desktop_media_id.h"
#include "media/capture/video/video_capture_device.h"
#include "ui/gfx/native_widget_types.h"

namespace base {
class Thread;
class TickClock;
}  // namespace base

namespace webrtc {
class DesktopCapturer;
}  // namespace webrtc

namespace content {

// DesktopCaptureDevice implements VideoCaptureDevice for screens and windows.
// It's essentially an adapter between webrtc::DesktopCapturer and
// VideoCaptureDevice, i.e. it employs the third-party WebRTC code to use native
// OS library to capture the screen.
//
// This class is used to perform desktop capture on Windows, Mac, and Linux but
// not on Chrome OS (which uses AuraWindowCaptureMachine instead).
class CONTENT_EXPORT DesktopCaptureDevice : public media::VideoCaptureDevice {
 public:
  // Creates capturer for the specified |source| and then creates
  // DesktopCaptureDevice for it. May return NULL in case of a failure (e.g. if
  // requested window was destroyed).
  static std::unique_ptr<media::VideoCaptureDevice> Create(
      const DesktopMediaID& source);

  DesktopCaptureDevice(const DesktopCaptureDevice&) = delete;
  DesktopCaptureDevice& operator=(const DesktopCaptureDevice&) = delete;

  ~DesktopCaptureDevice() override;

  // VideoCaptureDevice interface.
  void AllocateAndStart(const media::VideoCaptureParams& params,
                        std::unique_ptr<Client> client) override;
  void StopAndDeAllocate() override;
  // If currently stopped, starts the refresh frame timer to guarantee a frame
  // representing the most up-to-date content will be sent to the consumer in
  // the near future. This refresh operation will be canceled if a default
  // capture event triggers a frame capture in the meantime, and will result in
  // a frame sent to the consumer with a delay of up to one second.
  void RequestRefreshFrame() override;

  // Set the platform-dependent window id for the notification window.
  void SetNotificationWindowId(gfx::NativeViewId window_id);

 private:
  FRIEND_TEST_ALL_PREFIXES(DesktopCaptureDeviceTest,
                           RequestRefreshFrameBeforeStart);
  FRIEND_TEST_ALL_PREFIXES(DesktopCaptureDeviceTest,
                           RequestRefreshFrameAfterStop);
  friend class DesktopCaptureDeviceTest;
  friend class DesktopCaptureDeviceThrottledTest;
  friend class DesktopCaptureDeviceRequestRefreshFrameTest;
  class Core;

  DesktopCaptureDevice(
      std::unique_ptr<webrtc::DesktopCapturer> desktop_capturer,
      DesktopMediaID::Type type);

  // Useful to pass the TestMockTimeTaskRunner and the MockTickClock or any
  // other testing entities inheriting the common runner and tick interfaces.
  void SetMockTimeForTesting(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner,
      const base::TickClock* tick_clock);

  std::unique_ptr<Core> core_;

  // Ensure that the thread is the first object destroyed, as that will ensure
  // it is stopped. This helps to guarantee that the thread is stopped before
  // any of our objects (which it may be depending on), are destroyed. While the
  // thread *should* be stopped by consumers with StopAndDeAllocate, some edge
  // cases may mean that there is either not a chance for it to be called, or it
  // may have been called but not yet scheduled to run.
  base::Thread thread_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_MEDIA_CAPTURE_DESKTOP_CAPTURE_DEVICE_H_
