// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/media/peer_connection_tracker_host.h"

#include <utility>

#include "base/bind.h"
#include "base/power_monitor/power_monitor.h"
#include "base/task/post_task.h"
#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/browser/webrtc/webrtc_internals.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/webrtc_event_logger.h"

namespace content {

PeerConnectionTrackerHost::PeerConnectionTrackerHost(RenderProcessHost* rph)
    : render_process_id_(rph->GetID()), peer_pid_(rph->GetProcess().Pid()) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  base::PowerMonitor::AddObserver(this);
  rph->BindReceiver(tracker_.BindNewPipeAndPassReceiver());
  // Ensure that the initial thermal state is known by the |tracker_|.
  base::PowerObserver::DeviceThermalState initial_thermal_state =
      base::PowerMonitor::GetCurrentThermalState();
  if (initial_thermal_state !=
      base::PowerObserver::DeviceThermalState::kUnknown) {
    OnThermalStateChange(initial_thermal_state);
  }
}

PeerConnectionTrackerHost::~PeerConnectionTrackerHost() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  base::PowerMonitor::RemoveObserver(this);
}

void PeerConnectionTrackerHost::AddPeerConnection(
    blink::mojom::PeerConnectionInfoPtr info) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  WebRTCInternals* webrtc_internals = WebRTCInternals::GetInstance();
  if (webrtc_internals) {
    webrtc_internals->OnAddPeerConnection(
        render_process_id_, peer_pid_, info->lid, info->url,
        info->rtc_configuration, info->constraints);
  }

  WebRtcEventLogger* logger = WebRtcEventLogger::Get();
  if (logger) {
    logger->PeerConnectionAdded(render_process_id_, info->lid,
                                base::OnceCallback<void(bool)>());
  }
}

void PeerConnectionTrackerHost::RemovePeerConnection(int lid) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  WebRTCInternals* webrtc_internals = WebRTCInternals::GetInstance();
  if (webrtc_internals) {
    webrtc_internals->OnRemovePeerConnection(peer_pid_, lid);
  }
  WebRtcEventLogger* logger = WebRtcEventLogger::Get();
  if (logger) {
    logger->PeerConnectionRemoved(render_process_id_, lid,
                                  base::OnceCallback<void(bool)>());
  }
}

void PeerConnectionTrackerHost::UpdatePeerConnection(int lid,
                                                     const std::string& type,
                                                     const std::string& value) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // TODO(eladalon): Get rid of magic value. https://crbug.com/810383
  if (type == "stop") {
    WebRtcEventLogger* logger = WebRtcEventLogger::Get();
    if (logger) {
      logger->PeerConnectionStopped(render_process_id_, lid,
                                    base::OnceCallback<void(bool)>());
    }
  }

  WebRTCInternals* webrtc_internals = WebRTCInternals::GetInstance();
  if (webrtc_internals) {
    webrtc_internals->OnUpdatePeerConnection(peer_pid_, lid, type, value);
  }
}

void PeerConnectionTrackerHost::OnPeerConnectionSessionIdSet(
    int lid,
    const std::string& session_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  WebRtcEventLogger* logger = WebRtcEventLogger::Get();
  if (logger) {
    logger->PeerConnectionSessionIdSet(render_process_id_, lid, session_id,
                                       base::OnceCallback<void(bool)>());
  }
}

void PeerConnectionTrackerHost::AddStandardStats(int lid, base::Value value) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  WebRTCInternals* webrtc_internals = WebRTCInternals::GetInstance();
  if (webrtc_internals)
    webrtc_internals->OnAddStandardStats(peer_pid_, lid, std::move(value));
}

void PeerConnectionTrackerHost::AddLegacyStats(int lid, base::Value value) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  WebRTCInternals* webrtc_internals = WebRTCInternals::GetInstance();
  if (webrtc_internals)
    webrtc_internals->OnAddLegacyStats(peer_pid_, lid, std::move(value));
}

void PeerConnectionTrackerHost::GetUserMedia(
    const std::string& origin,
    bool audio,
    bool video,
    const std::string& audio_constraints,
    const std::string& video_constraints) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  WebRTCInternals* webrtc_internals = WebRTCInternals::GetInstance();
  if (webrtc_internals) {
    webrtc_internals->OnGetUserMedia(render_process_id_, peer_pid_, origin,
                                     audio, video, audio_constraints,
                                     video_constraints);
  }
}

void PeerConnectionTrackerHost::WebRtcEventLogWrite(
    int lid,
    const std::vector<uint8_t>& output) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  std::string converted_output(output.begin(), output.end());
  WebRtcEventLogger* logger = WebRtcEventLogger::Get();
  if (logger) {
    logger->OnWebRtcEventLogWrite(
        render_process_id_, lid, converted_output,
        base::OnceCallback<void(std::pair<bool, bool>)>());
  }
}

void PeerConnectionTrackerHost::OnSuspend() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  tracker_->OnSuspend();
}

void PeerConnectionTrackerHost::OnThermalStateChange(
    base::PowerObserver::DeviceThermalState new_state) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  tracker_->OnThermalStateChange(
      static_cast<blink::mojom::DeviceThermalState>(new_state));
}

void PeerConnectionTrackerHost::StartEventLog(int lid, int output_period_ms) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  tracker_->StartEventLog(lid, output_period_ms);
}

void PeerConnectionTrackerHost::StopEventLog(int lid) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  tracker_->StopEventLog(lid);
}

void PeerConnectionTrackerHost::GetStandardStats() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  tracker_->GetStandardStats();
}

void PeerConnectionTrackerHost::GetLegacyStats() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  tracker_->GetLegacyStats();
}

void PeerConnectionTrackerHost::BindReceiver(
    mojo::PendingReceiver<blink::mojom::PeerConnectionTrackerHost>
        pending_receiver) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  receiver_.reset();
  receiver_.Bind(std::move(pending_receiver));
}

}  // namespace content
