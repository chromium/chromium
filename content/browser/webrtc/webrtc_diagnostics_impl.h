// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEBRTC_WEBRTC_DIAGNOSTICS_IMPL_H_
#define CONTENT_BROWSER_WEBRTC_WEBRTC_DIAGNOSTICS_IMPL_H_

#include <map>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/no_destructor.h"
#include "base/observer_list.h"
#include "base/supports_user_data.h"
#include "base/timer/timer.h"
#include "content/browser/webrtc/webrtc_internals_ui_observer.h"
#include "content/common/content_export.h"
#include "content/public/browser/child_process_host.h"
#include "content/public/browser/webrtc_diagnostics.h"
#include "content/public/common/child_process_id.h"

namespace content {

class WebRTCInternals;

// Observes WebRTCInternals process-wide and partitions everything it sees by
// BrowserContext, so that a capture session, its cached diagnostic data and
// its observers never cross a profile boundary.
//
// The singleton itself holds no capture state. Each profile's state lives in a
// PerContext object owned by the BrowserContext through SupportsUserData, so it
// is destroyed with the profile and there is no process-global map keyed on a
// BrowserContext pointer that could outlive it.
//
// Attribution: every WebRTCInternals event carries a `rid` (the renderer's
// ChildProcessId), which resolves to a RenderProcessHost and from there to a
// BrowserContext. An entry whose `rid` no longer resolves cannot be attributed
// and is dropped rather than being shown to an arbitrary profile.
class CONTENT_EXPORT WebRtcDiagnosticsImpl : public WebRtcDiagnostics,
                                             public WebRTCInternalsUIObserver {
 public:
  static WebRtcDiagnosticsImpl* GetInstance();

  ~WebRtcDiagnosticsImpl() override;

  WebRtcDiagnosticsImpl(const WebRtcDiagnosticsImpl&) = delete;
  WebRtcDiagnosticsImpl& operator=(const WebRtcDiagnosticsImpl&) = delete;

  // WebRtcDiagnostics implementation.
  StartCaptureResult StartCaptureForClient(
      BrowserContext* context,
      std::string_view client_id,
      std::vector<url::Origin> origins) override;
  StopCaptureResult StopCaptureForClient(BrowserContext* context,
                                         std::string_view client_id) override;
  bool GetSnapshot(BrowserContext* context,
                   std::string_view client_id,
                   const std::vector<url::Origin>& origins,
                   base::OnceCallback<void(base::Value)> callback) override;
  bool IsCapturingForClient(BrowserContext* context,
                            std::string_view client_id) override;
  std::vector<std::string> GetCapturingClients(
      BrowserContext* context) override;
  std::optional<std::vector<url::Origin>> GetFilterOriginsForClient(
      BrowserContext* context,
      std::string_view client_id) override;
  std::optional<url::Origin> GetOriginForPeerConnection(
      BrowserContext* context,
      std::string_view pc_id) override;
  std::optional<ChildProcessId> GetRenderProcessIdForPeerConnection(
      BrowserContext* context,
      std::string_view pc_id) override;

  void AddObserver(BrowserContext* context,
                   WebRtcDiagnostics::Observer* observer) override;
  void RemoveObserver(BrowserContext* context,
                      WebRtcDiagnostics::Observer* observer) override;

  void ResetForTesting();

  // WebRTCInternalsUIObserver implementation.
  void OnUpdate(const std::string& event_name,
                const base::Value* event_data) override;

 private:
  friend class base::NoDestructor<WebRtcDiagnosticsImpl>;
  WebRtcDiagnosticsImpl();

  struct PeerConnectionInfo {
    PeerConnectionInfo();
    PeerConnectionInfo(std::optional<url::Origin> origin,
                       ChildProcessId render_process_id);
    PeerConnectionInfo(const PeerConnectionInfo&);
    PeerConnectionInfo& operator=(const PeerConnectionInfo&);
    ~PeerConnectionInfo();

    std::optional<url::Origin> origin;
    ChildProcessId render_process_id =
        ChildProcessId::FromUnsafeValue(ChildProcessHost::kInvalidUniqueID);
  };

  // All capture state for one profile. Owned by the BrowserContext via
  // SupportsUserData, so it dies with the profile; the destructor removes the
  // entry from `contexts_`, which is what keeps that index free of stale
  // pointers.
  class PerContext : public base::SupportsUserData::Data {
   public:
    PerContext(WebRtcDiagnosticsImpl* owner, BrowserContext* context);
    ~PerContext() override;

    PerContext(const PerContext&) = delete;
    PerContext& operator=(const PerContext&) = delete;

    // Returns the state for `context`, creating it if necessary. Returns null
    // if `context` has already begun shutting down, so that a late event
    // cannot resurrect state on a dying profile.
    static PerContext* GetOrCreate(WebRtcDiagnosticsImpl* owner,
                                   BrowserContext* context);
    // Returns the state for `context`, or null if it has none.
    static PerContext* GetIfExists(BrowserContext* context);

    bool has_clients() const { return !clients.empty(); }

    // Client ID to the origin filter registered with StartCaptureForClient. An
    // empty vector means unfiltered.
    std::map<std::string, std::vector<url::Origin>, std::less<>> clients;

    // Peer connection ID to metadata, used to filter removal events.
    std::map<std::string, PeerConnectionInfo, std::less<>> pc_metadata;

    // Local copies of the WebRTCInternals data belonging to this profile, so
    // that WebRTCInternals itself is never modified.
    base::ListValue get_user_media_requests;
    base::ListValue peer_connection_data;

    base::ObserverList<WebRtcDiagnostics::Observer> observers;

    // Truncation bookkeeping for this profile. The counters are cumulative
    // totals since the profile last went idle, and are what
    // OnSnapshotTruncated reports. `last_truncation_notification` rate
    // limits that notification to one every 5 seconds.
    base::TimeTicks last_truncation_notification;
    int dropped_log_entries = 0;
    int dropped_stats_entries = 0;
    int dropped_media_entries = 0;

   private:
    static const void* UserDataKey();

    raw_ptr<WebRtcDiagnosticsImpl> owner_;
    raw_ptr<BrowserContext> context_;
  };

  void OnPerContextDestroyed(PerContext* state);

  // Adds or removes this object as a WebRTCInternals observer based on whether
  // any context currently has a capturing client, and replays the existing
  // WebRTCInternals state into `newly_capturing` when it just became the first
  // client of its own context.
  void UpdateInternalsRegistration(PerContext* newly_capturing);

  void RequestStats();
  void UpdateStatsTimer();

  // Returns the state for the profile that `entry` was captured in, resolved
  // through the renderer id the entry carries. Null if the renderer is gone
  // or that profile has no state for this feature.
  PerContext* FindPerContextState(const base::DictValue& entry);

  // Records the origin and renderer of the peer connection described by
  // `pc_dict` in `state`, so that later events for it can be filtered by
  // origin. Entries without a renderer id or a local id are ignored.
  void RebuildMetadataFor(PerContext* state, const base::DictValue& pc_dict);

  // Reports `state`'s cumulative dropped-entry counts to its observers, at
  // most once every 5 seconds.
  void NotifySnapshotTruncated(PerContext* state);

  // Every context that currently has a PerContext alive. Entries are removed
  // by ~PerContext.
  std::set<raw_ptr<PerContext>> contexts_;

  // Timer for requesting stats. Process-wide, because WebRTCInternals polls
  // process-wide; it runs while any context has a peer connection cached.
  base::RepeatingTimer stats_timer_;

  bool registered_with_internals_ = false;
};

}  // namespace content

#endif  // CONTENT_BROWSER_WEBRTC_WEBRTC_DIAGNOSTICS_IMPL_H_
