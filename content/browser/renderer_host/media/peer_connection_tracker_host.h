// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_MEDIA_PEER_CONNECTION_TRACKER_HOST_H_
#define CONTENT_BROWSER_RENDERER_HOST_MEDIA_PEER_CONNECTION_TRACKER_HOST_H_

#include <set>
#include <string>

#include "base/power_monitor/power_observer.h"
#include "base/process/process_handle.h"
#include "base/types/pass_key.h"
#include "content/public/browser/document_user_data.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/peer_connection_tracker_host_observer.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/mojom/peerconnection/peer_connection_tracker.mojom.h"

namespace base {
class Value;
}  // namespace base

namespace content {

class RenderFrameHost;

// This class is the host for PeerConnectionTracker in the browser process
// managed by RenderFrameHostImpl. It receives PeerConnection events from
// PeerConnectionTracker as IPC messages that it forwards to WebRTCInternals.
// It also forwards browser process events to PeerConnectionTracker via IPC.
//
// Note: This class and all of its methods are meant to only be used on the UI
//       thread.
class PeerConnectionTrackerHost
    : public DocumentUserData<PeerConnectionTrackerHost>,
      public base::PowerSuspendObserver,
      public base::PowerThermalObserver,
      public blink::mojom::PeerConnectionTrackerHost {
 public:
  PeerConnectionTrackerHost(const PeerConnectionTrackerHost&) = delete;
  PeerConnectionTrackerHost& operator=(const PeerConnectionTrackerHost&) =
      delete;

  ~PeerConnectionTrackerHost() override;

  // Adds/removes a PeerConnectionTrackerHostObserver.
  static void AddObserver(base::PassKey<PeerConnectionTrackerHostObserver>,
                          PeerConnectionTrackerHostObserver* observer);
  static void RemoveObserver(base::PassKey<PeerConnectionTrackerHostObserver>,
                             PeerConnectionTrackerHostObserver* observer);

  static const std::set<PeerConnectionTrackerHost*>& GetAllHosts();

  // base::PowerSuspendObserver override.
  void OnSuspend() override;
  // base::PowerThermalObserver override.
  void OnThermalStateChange(
      base::PowerThermalObserver::DeviceThermalState new_state) override;
  void OnSpeedLimitChange(int) override;

  // These methods call out to blink::mojom::PeerConnectionManager on renderer
  // side.
  void StartEventLog(int peer_connection_local_id, int output_period_ms);
  void StopEventLog(int lid);
  void GetStandardStats();
  void GetCurrentState();

  void BindReceiver(
      mojo::PendingReceiver<blink::mojom::PeerConnectionTrackerHost>
          pending_receiver);

 private:
  friend class DocumentUserData<PeerConnectionTrackerHost>;
  explicit PeerConnectionTrackerHost(RenderFrameHost* rfh);
  DOCUMENT_USER_DATA_KEY_DECL();

  // blink::mojom::PeerConnectionTrackerHost implementation.
  void AddPeerConnection(blink::mojom::PeerConnectionInfoPtr info) override;
  void RemovePeerConnection(int lid) override;
  void UpdatePeerConnection(int lid,
                            const std::string& type,
                            const std::string& value) override;
  void OnPeerConnectionSessionIdSet(int lid,
                                    const std::string& session_id) override;
  void GetUserMedia(int request_id,
                    bool audio,
                    bool video,
                    const std::string& audio_constraints,
                    const std::string& video_constraints) override;
  void GetUserMediaSuccess(int request_id,
                           const std::string& stream_id,
                           const std::string& audio_track_info,
                           const std::string& video_track_info) override;
  void GetUserMediaFailure(int request_id,
                           const std::string& error,
                           const std::string& error_message) override;
  void GetDisplayMedia(int request_id,
                       bool audio,
                       bool video,
                       const std::string& audio_constraints,
                       const std::string& video_constraints) override;
  void GetDisplayMediaSuccess(int request_id,
                              const std::string& stream_id,
                              const std::string& audio_track_info,
                              const std::string& video_track_info) override;
  void GetDisplayMediaFailure(int request_id,
                              const std::string& error,
                              const std::string& error_message) override;
  void WebRtcEventLogWrite(int lid,
                           const std::vector<uint8_t>& output) override;
  void AddStandardStats(int lid, base::Value::List value) override;
  void AddLegacyStats(int lid, base::Value::List value) override;

  GlobalRenderFrameHostId frame_id_;
  base::ProcessId peer_pid_;
  mojo::Receiver<blink::mojom::PeerConnectionTrackerHost> receiver_{this};
  mojo::Remote<blink::mojom::PeerConnectionManager> tracker_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_MEDIA_PEER_CONNECTION_TRACKER_HOST_H_
