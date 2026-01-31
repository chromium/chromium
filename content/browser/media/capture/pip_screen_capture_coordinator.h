// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MEDIA_CAPTURE_PIP_SCREEN_CAPTURE_COORDINATOR_H_
#define CONTENT_BROWSER_MEDIA_CAPTURE_PIP_SCREEN_CAPTURE_COORDINATOR_H_

#include <memory>

#include "content/common/content_export.h"
#include "content/public/browser/desktop_media_id.h"
#include "content/public/browser/global_routing_id.h"

namespace content {

class PipScreenCaptureCoordinatorProxy;
class WebContents;

// Manages information about the visibility and identity of
// Picture-in-Picture (PiP) windows associated with a
// WebContents. This is used for excluding the PiP window from screen
// captures of the originating tab.
class CONTENT_EXPORT PipScreenCaptureCoordinator {
 public:
  // Gets a pointer to the singleton that implements the interface.
  static PipScreenCaptureCoordinator* GetInstance();

  // Allows overriding the singleton instance for testing.
  static void SetInstanceForTesting(PipScreenCaptureCoordinator* instance);

  virtual ~PipScreenCaptureCoordinator() = default;

  PipScreenCaptureCoordinator(const PipScreenCaptureCoordinator&) = delete;
  PipScreenCaptureCoordinator& operator=(const PipScreenCaptureCoordinator&) =
      delete;

  // Called when a document PiP window is shown from the WebContents
  // which this coordinator belongs to.
  virtual void OnPipShown(
      WebContents& pip_web_contents,
      const GlobalRenderFrameHostId& pip_owner_render_frame_host_id) = 0;
  // Called when the PiP window is closed.
  virtual void OnPipClosed() = 0;
  // Returns the ID of the PiP window if it exists and should be excluded from
  // the capture of the specified `desktop_id`.
  virtual std::optional<DesktopMediaID::Id>
  GetPipWindowToExcludeFromScreenCapture(DesktopMediaID::Id desktop_id) = 0;

  virtual std::unique_ptr<PipScreenCaptureCoordinatorProxy> CreateProxy() = 0;

 protected:
  PipScreenCaptureCoordinator() = default;
};

}  // namespace content

#endif  // CONTENT_BROWSER_MEDIA_CAPTURE_PIP_SCREEN_CAPTURE_COORDINATOR_H_
