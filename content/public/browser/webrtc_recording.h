// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_WEBRTC_RECORDING_H_
#define CONTENT_PUBLIC_BROWSER_WEBRTC_RECORDING_H_

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "base/functional/callback.h"
#include "base/observer_list_types.h"
#include "base/values.h"
#include "content/common/content_export.h"
#include "content/public/common/child_process_id.h"
#include "url/origin.h"

namespace content {

class BrowserContext;

// Provides programmatic access to WebRTC diagnostic data equivalent to what
// chrome://webrtc-internals displays. The implementation observes
// WebRTCInternals from the side; it never modifies it.
//
// GetInstance() returns a single process-wide object, but it owns no capture
// state of its own. All capture state - sessions, cached diagnostic data and
// observers - is scoped to a BrowserContext, which is why every method below
// takes one. Two profiles running the same extension therefore have
// independent sessions, independent origin filters and independent event
// streams, and a profile's state is destroyed with the profile.
//
// All methods must be called on the UI thread.
class CONTENT_EXPORT WebRtcRecording {
 public:
  virtual ~WebRtcRecording() = default;

  // Maximum number of filter origins accepted by StartRecording and
  // GetRecordingData.
  static constexpr size_t kMaxFilterOrigins = 128;

  enum class StartRecordingResult {
    kSuccess,
    kAlreadyCapturing,
    kInvalidOrigin,
    kTooManyOrigins,
    // The BrowserContext cannot host a session, because it is null or has
    // already begun shutting down.
    kUnavailable,
  };

  enum class StopRecordingResult { kSuccess, kNotCapturing };

  // Observes capture events for a single BrowserContext. An observer is
  // registered against one context and only ever receives events belonging to
  // that context, so implementations do not need to re-check the profile.
  // Callbacks are invoked on the UI thread.
  class CONTENT_EXPORT Observer : public base::CheckedObserver {
   public:
    virtual void OnPeerConnectionAdded(std::string_view id,
                                       const base::Value& data) {}
    virtual void OnPeerConnectionRemoved(std::string_view id) {}
    virtual void OnSnapshotTruncated(int dropped_log,
                                     int dropped_stats,
                                     int dropped_media) {}
    virtual void OnCaptureStopped(std::string_view stopped_client_id) {}
  };

  static WebRtcRecording* GetInstance();

  // Begins a capture session for `client_id` within `context`, optionally
  // filtered to `origins`. A session in one BrowserContext neither blocks nor
  // observes a session for the same `client_id` in another.
  //
  // `origins` is taken by value because the session stores it; callers that no
  // longer need their copy should std::move() into this call.
  virtual StartRecordingResult StartRecording(
      BrowserContext* context,
      std::string_view client_id,
      std::vector<url::Origin> origins) = 0;

  // Terminates the capture session for a single client within `context`. Only
  // call with client IDs obtained from a trusted source.
  virtual StopRecordingResult StopRecordingForClient(
      BrowserContext* context,
      std::string_view client_id) = 0;

  // Delivers a snapshot of the data recorded for `context`, optionally further
  // filtered to `origins`, by running `callback` before returning. Returns
  // false if the request is rejected (invalid origin, too many origins, or no
  // active session for `client_id`), in which case `callback` is not run.
  //
  // The result is always scoped to the caller's own session: `origins` can
  // narrow what the session already covers but can never widen it.
  virtual bool GetRecordingData(
      BrowserContext* context,
      std::string_view client_id,
      const std::vector<url::Origin>& origins,
      base::OnceCallback<void(base::Value)> callback) = 0;

  // Whether `client_id` has an active capture session in `context`. This
  // reports the caller's own session, not whether any capture is running
  // anywhere in the process.
  virtual bool IsRecordingForClient(BrowserContext* context,
                                    std::string_view client_id) = 0;

  // The IDs of every client currently capturing in `context`. Returned by
  // value so that callers may start or stop sessions while iterating.
  virtual std::vector<std::string> GetCapturingClients(
      BrowserContext* context) = 0;

  // The following methods expose capture metadata for trusted browser-process
  // callers only. Do not expose their results to renderers or untrusted IPC.

  // The origin filter `client_id` registered in `context`, or nullopt if it has
  // no session there. An empty (but present) vector means the session is
  // unfiltered and matches every origin; nullopt means there is no session and
  // nothing should be dispatched.
  virtual std::optional<std::vector<url::Origin>> GetFilterOriginsForClient(
      BrowserContext* context,
      std::string_view client_id) = 0;
  virtual std::optional<url::Origin> GetOriginForPeerConnection(
      BrowserContext* context,
      std::string_view id) = 0;
  virtual std::optional<ChildProcessId> GetRenderProcessIdForPeerConnection(
      BrowserContext* context,
      std::string_view id) = 0;

  virtual void AddObserver(BrowserContext* context, Observer* observer) = 0;
  virtual void RemoveObserver(BrowserContext* context, Observer* observer) = 0;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_WEBRTC_RECORDING_H_
