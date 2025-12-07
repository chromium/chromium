// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MEDIA_CAPTURE_PIP_SCREEN_CAPTURE_COORDINATOR_H_
#define CONTENT_BROWSER_MEDIA_CAPTURE_PIP_SCREEN_CAPTURE_COORDINATOR_H_

#include <cstdint>

#include "content/browser/media/capture/pip_screen_capture_coordinator_proxy.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents_user_data.h"

namespace content {

class WebContents;

// Manages information about the visibility and identity of
// Picture-in-Picture (PiP) windows associated with a
// WebContents. This is used for excluding the PiP window from screen
// captures of the originating tab.
class CONTENT_EXPORT PipScreenCaptureCoordinator
    : public WebContentsUserData<PipScreenCaptureCoordinator> {
 public:
  // Retrieves the PipScreenCaptureCoordinator for |web_contents| if
  // enabled, creating one if it does not already exist. The returned
  // pointer is owned by |web_contents|. |web_contents| must not be
  // null.
  //
  // Returns nullptr if ExcludePipFromScreenCapture is disabled.
  static PipScreenCaptureCoordinator* GetOrCreateForWebContents(
      WebContents* web_contents);
  // Retrieves the PipScreenCaptureCoordinator for |render_frame_host|
  // if enabled, creating one if it does not already exist. The
  // returned pointer is owned by the associated
  // web_contents. |render_frame_host| must not be null.
  //
  // Returns nullptr if ExcludePipFromScreenCapture is disabled.
  static PipScreenCaptureCoordinator* GetOrCreateForRenderFrameHost(
      RenderFrameHost* render_frame_host);

  ~PipScreenCaptureCoordinator() override;

  PipScreenCaptureCoordinator(const PipScreenCaptureCoordinator&) = delete;
  PipScreenCaptureCoordinator& operator=(const PipScreenCaptureCoordinator&) =
      delete;

  // Called when a document PiP window is shown from the WebContents
  // which this coordinator belongs to.
  virtual void OnPipShown(WebContents& pip_web_contents);
  // Called when the PiP window is closed.
  virtual void OnPipClosed();

  std::unique_ptr<PipScreenCaptureCoordinatorProxy> CreateProxy();

 private:
  explicit PipScreenCaptureCoordinator(WebContents* web_contents);

  friend class MockPipScreenCaptureCoordinator;
  friend class WebContentsUserData<PipScreenCaptureCoordinator>;
  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace content

#endif  // CONTENT_BROWSER_MEDIA_CAPTURE_PIP_SCREEN_CAPTURE_COORDINATOR_H_
