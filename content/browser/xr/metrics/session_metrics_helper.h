// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_XR_METRICS_SESSION_METRICS_HELPER_H_
#define CONTENT_BROWSER_XR_METRICS_SESSION_METRICS_HELPER_H_

#include <memory>
#include <unordered_map>
#include <unordered_set>

#include "content/common/content_export.h"
#include "content/public/browser/web_contents_observer.h"
#include "device/vr/public/mojom/vr_service.mojom-forward.h"
#include "device/vr/public/mojom/xr_device.mojom-forward.h"
#include "device/vr/public/mojom/xr_session.mojom-forward.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_source_id.h"

namespace content {
class SessionTimer;
class WebXRSessionTracker;

// This class is not thread-safe and must only be used from the main thread.
// This class tracks metrics for various kinds of sessions, including VR
// browsing sessions, WebXR presentation sessions, and others. It mainly tracks
// metrics that require state monitoring, such as durations, but also tracks
// data we want attached to that, such as number of videos watched and how the
// session was started.
class CONTENT_EXPORT SessionMetricsHelper
    : public content::WebContentsObserver {
 public:
  // Returns the SessionMetricsHelper singleton if it has been created for the
  // WebContents.
  static SessionMetricsHelper* FromWebContents(content::WebContents* contents);
  static SessionMetricsHelper* CreateForWebContents(
      content::WebContents* contents);

  ~SessionMetricsHelper() override;

  // Records that an inline session was started and returns the |PendingRemote|
  // for the created session recorder.
  mojo::PendingRemote<device::mojom::XRSessionMetricsRecorder>
  StartInlineSession(const device::mojom::XRSessionOptions& session_options,
                     const std::unordered_set<device::mojom::XRSessionFeature>&
                         enabled_features,
                     size_t session_id);

  // Records that inline session was stopped. Will record an UKM entry.
  void StopAndRecordInlineSession(size_t session_id);

  // Records that an immersive session was started and returns the
  // |PendingRemote| for the created session recorder. Two immersive sessions
  // may not exist simultaneously.
  mojo::PendingRemote<device::mojom::XRSessionMetricsRecorder>
  StartImmersiveSession(
      const device::mojom::XRDeviceId& runtime_id,
      const device::mojom::XRSessionOptions& session_options,
      const std::unordered_set<device::mojom::XRSessionFeature>&
          enabled_features);

  // Records that an immersive session was stopped. Will record a UKM entry.
  void StopAndRecordImmersiveSession();

 private:
  explicit SessionMetricsHelper(content::WebContents* contents);

  // WebContentsObserver
  void PrimaryPageChanged(content::Page& page) override;

  std::unique_ptr<SessionTimer> session_timer_;

  std::unique_ptr<WebXRSessionTracker> webxr_immersive_session_tracker_;

  // Map associating active inline session Ids to their trackers. The contents
  // of the map are managed by |StartInlineSession| and
  // |StopAndRecordInlineSession|.
  std::unordered_map<size_t, std::unique_ptr<WebXRSessionTracker>>
      webxr_inline_session_trackers_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_XR_METRICS_SESSION_METRICS_HELPER_H_
