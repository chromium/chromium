// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVTOOLS_PROTOCOL_DISPLAY_CAPTURE_LAUNCHER_H_
#define CONTENT_BROWSER_DEVTOOLS_PROTOCOL_DISPLAY_CAPTURE_LAUNCHER_H_

#include <memory>
#include <string>

#include "base/memory/weak_ptr.h"
#include "content/common/content_export.h"
#include "content/public/browser/browser_thread.h"
#include "media/capture/video/video_frame_receiver.h"
#include "media/capture/video_capture_types.h"

namespace content::protocol {

// Captures a display surface (for example a browser window) with the video
// capture device launcher that getDisplayMedia() uses, and forwards the
// frames to a receiver on the UI thread.
//
// The launcher and the launched device live on the IO thread. This class is
// created, used and destroyed on the UI thread. The device stops when this
// object is destroyed.
class CONTENT_EXPORT DisplayCaptureLauncher {
 public:
  // Launches the device for `device_id` (a DesktopMediaID string). Frames go
  // to `receiver` on the UI thread.
  DisplayCaptureLauncher(const std::string& device_id,
                         const media::VideoCaptureParams& params,
                         base::WeakPtr<media::VideoFrameReceiver> receiver);
  DisplayCaptureLauncher(const DisplayCaptureLauncher&) = delete;
  DisplayCaptureLauncher& operator=(const DisplayCaptureLauncher&) = delete;
  ~DisplayCaptureLauncher();

  // Asks the device for a new frame, for example when the content does not
  // change.
  void RequestRefreshFrame();

  // Suspends or resumes the device, for example when a data leak prevention
  // policy pauses the capture. Works also before the device has launched.
  void Suspend();
  void Resume();

 private:
  class Core;

  std::unique_ptr<Core, BrowserThread::DeleteOnIOThread> core_;
};

}  // namespace content::protocol

#endif  // CONTENT_BROWSER_DEVTOOLS_PROTOCOL_DISPLAY_CAPTURE_LAUNCHER_H_
