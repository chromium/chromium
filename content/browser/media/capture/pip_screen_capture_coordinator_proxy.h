// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MEDIA_CAPTURE_PIP_SCREEN_CAPTURE_COORDINATOR_PROXY_H_
#define CONTENT_BROWSER_MEDIA_CAPTURE_PIP_SCREEN_CAPTURE_COORDINATOR_PROXY_H_

#include <optional>

#include "base/observer_list_types.h"
#include "base/unguessable_token.h"
#include "content/public/browser/desktop_media_id.h"
#include "content/public/browser/global_routing_id.h"

namespace content {

// PipScreenCaptureCoordinatorProxy is designed to tracking the state
// of the Picture-in-Picture window from components that run on a
// different sequence (e.g., the device thread) from the
// PipScreenCaptureCoordinator (which lives on the UI thread). This
// allows for safe, cross-thread observation of the PiP window ID.
class PipScreenCaptureCoordinatorProxy {
 public:
  // Information about a capture session.
  struct CaptureInfo {
    base::UnguessableToken session_id;
    content::GlobalRenderFrameHostId render_frame_host_id;
    DesktopMediaID desktop_media_id;

    bool operator==(const CaptureInfo& other) const = default;
  };

  class Observer : public base::CheckedObserver {
   public:
    virtual void OnStateChanged(
        const std::optional<DesktopMediaID::Id>& new_pip_window_id,
        const GlobalRenderFrameHostId& new_pip_owner_render_frame_host_id,
        const std::vector<CaptureInfo>& captures) = 0;
  };

  virtual ~PipScreenCaptureCoordinatorProxy() = default;

  // Returns the tracked PiP window ID.
  virtual std::optional<DesktopMediaID::Id> PipWindowId() const = 0;
  virtual GlobalRenderFrameHostId GetPipOwnerRenderFrameHostId() const = 0;
  virtual const std::vector<CaptureInfo>& Captures() const = 0;

  virtual std::optional<DesktopMediaID::Id> WindowToExclude(
      const DesktopMediaID& media_id) const = 0;

  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;
};

}  // namespace content

#endif  // CONTENT_BROWSER_MEDIA_CAPTURE_PIP_SCREEN_CAPTURE_COORDINATOR_PROXY_H_
