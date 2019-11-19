// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_MEDIA_PEER_CONNECTION_TRACKER_HOST_H_
#define CONTENT_BROWSER_RENDERER_HOST_MEDIA_PEER_CONNECTION_TRACKER_HOST_H_

#include <string>

#include "base/macros.h"
#include "base/power_monitor/power_observer.h"
#include "base/process/process_handle.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/mojom/peerconnection/peer_connection_tracker.mojom.h"

namespace base {
class Value;
}  // namespace base

namespace content {

class RenderProcessHost;

// This class is the host for PeerConnectionTracker in the browser process
// managed by RenderProcessHostImpl. It receives PeerConnection events from
// PeerConnectionTracker as IPC messages that it forwards to WebRTCInternals.
// It also forwards browser process events to PeerConnectionTracker via IPC.
class PeerConnectionTrackerHost
    : public base::PowerObserver,
      public blink::mojom::PeerConnectionTrackerHost {
 public:
  explicit PeerConnectionTrackerHost(RenderProcessHost* rph);
  ~PeerConnectionTrackerHost() override;

  // base::PowerObserver override.
  void OnSuspend() override;

  // These methods call out to blink::mojom::PeerConnectionManager on renderer
  // side.
  void StartEventLog(int peer_connection_local_id, int output_period_ms);
  void StopEventLog(int lid);
  void GetStandardStats();
  void GetLegacyStats();

  void BindReceiver(
      mojo::PendingReceiver<blink::mojom::PeerConnectionTrackerHost>
          pending_receiver);

 private:
  // blink::mojom::PeerConnectionTrackerHost implementation.
  void AddPeerConnection(blink::mojom::PeerConnectionInfoPtr info) override;
  void RemovePeerConnection(int lid) override;
  void UpdatePeerConnection(int lid,
                            const std::string& type,
                            const std::string& value) override;
  void OnPeerConnectionSessionIdSet(int lid,
                                    const std::string& session_id) override;
  void GetUserMedia(const std::string& origin,
                    bool audio,
                    bool video,
                    const std::string& audio_constraints,
                    const std::string& video_constraints) override;
  void WebRtcEventLogWrite(int lid, const std::string& output) override;
  void AddStandardStats(int lid, base::Value value) override;
  void AddLegacyStats(int lid, base::Value value) override;

  int render_process_id_;
  base::ProcessId peer_pid_;
  mojo::Receiver<blink::mojom::PeerConnectionTrackerHost> receiver_{this};
  mojo::Remote<blink::mojom::PeerConnectionManager> tracker_;

  DISALLOW_COPY_AND_ASSIGN(PeerConnectionTrackerHost);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_MEDIA_PEER_CONNECTION_TRACKER_HOST_H_
