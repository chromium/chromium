// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MEDIA_CAPTURE_AURA_WINDOW_VIDEO_CAPTURE_DEVICE_H_
#define CONTENT_BROWSER_MEDIA_CAPTURE_AURA_WINDOW_VIDEO_CAPTURE_DEVICE_H_

#include <memory>

#include "base/memory/weak_ptr.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "content/browser/media/capture/frame_sink_video_capture_device.h"
#include "content/common/content_export.h"
#include "content/public/browser/browser_thread.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"

namespace aura {
class Window;
}  // namespace aura

namespace content {

struct DesktopMediaID;

// Captures the displayed contents of an aura::Window, producing a stream of
// video frames.
class CONTENT_EXPORT AuraWindowVideoCaptureDevice final
    : public FrameSinkVideoCaptureDevice {
 public:
  explicit AuraWindowVideoCaptureDevice(const DesktopMediaID& source_id);

  AuraWindowVideoCaptureDevice(const AuraWindowVideoCaptureDevice&) = delete;
  AuraWindowVideoCaptureDevice& operator=(const AuraWindowVideoCaptureDevice&) =
      delete;

  ~AuraWindowVideoCaptureDevice() final;

 private:
  // Monitors the target Window and notifies the base class if it is destroyed.
  class WindowTracker;

  // A helper that runs on the UI thread to monitor the target aura::Window, and
  // post a notification if it is destroyed.
  std::unique_ptr<WindowTracker, BrowserThread::DeleteOnUIThread> tracker_;
  base::WeakPtrFactory<AuraWindowVideoCaptureDevice> weak_ptr_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_MEDIA_CAPTURE_AURA_WINDOW_VIDEO_CAPTURE_DEVICE_H_
