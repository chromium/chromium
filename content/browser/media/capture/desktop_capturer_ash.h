// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MEDIA_CAPTURE_DESKTOP_CAPTURER_ASH_H_
#define CONTENT_BROWSER_MEDIA_CAPTURE_DESKTOP_CAPTURER_ASH_H_

#include <optional>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_capturer.h"

namespace gfx {
class Image;
}

namespace content {

// This class provides snapshots of the desktop for ash-chrome.
// It will capture the specified display, or the root window for new windows if
// display has not been set by SelectSource(SourceId).
//
// `SourceId` as used in GetSourceList(SourceList) and SelectSource(SourceId) is
// defined in `//third_party/webrtc` as an opaque `int64` and is expected to
// match the native identifier for each platform. This class will always use
// display::Display::id().
class DesktopCapturerAsh : public webrtc::DesktopCapturer {
 public:
  DesktopCapturerAsh();
  DesktopCapturerAsh(const DesktopCapturerAsh&) = delete;
  DesktopCapturerAsh& operator=(const DesktopCapturerAsh&) = delete;
  ~DesktopCapturerAsh() override;

  // DesktopCapturer:
  void Start(Callback* callback) override;
  void CaptureFrame() override;
  bool GetSourceList(SourceList* sources) override;
  bool SelectSource(SourceId id) override;
  bool FocusOnSelectedSource() override;
  bool IsOccluded(const webrtc::DesktopVector& pos) override;
  void SetSharedMemoryFactory(std::unique_ptr<webrtc::SharedMemoryFactory>
                                  shared_memory_factory) override;
  void SetExcludedWindow(webrtc::WindowId window) override;

 private:
  void OnGrabWindowSnapsot(gfx::Image snapshot);

  // Display to capture.
  std::optional<SourceId> display_id_;
  raw_ptr<Callback> callback_ = nullptr;
  base::WeakPtrFactory<DesktopCapturerAsh> weak_ptr_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_MEDIA_CAPTURE_DESKTOP_CAPTURER_ASH_H_
