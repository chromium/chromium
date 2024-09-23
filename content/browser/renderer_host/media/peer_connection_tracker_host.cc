// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/media/peer_connection_tracker_host.h"

#include <set>
#include <utility>

#include "base/functional/bind.h"
#include "base/no_destructor.h"
#include "base/observer_list.h"
#include "base/power_monitor/power_monitor.h"
#include "base/ranges/algorithm.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "services/service_manager/public/cpp/interface_provider.h"

namespace content {

namespace {

using ObserverListType = base::ObserverList<PeerConnectionTrackerHostObserver,
                                            /*check_empty=*/true,
                                            /*allow_reentrancy=*/false>;
ObserverListType& GetObserverList() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  static base::NoDestructor<ObserverListType> observer_list{};
  return *observer_list;
}

std::set<PeerConnectionTrackerHost*>& AllHosts() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  static base::NoDestructor<std::set<PeerConnectionTrackerHost*>> all_hosts{};
  return *all_hosts;
}

void RegisterHost(PeerConnectionTrackerHost* host) {
  AllHosts().insert(host);
}
void RemoveHost(PeerConnectionTrackerHost* host) {
  AllHosts().erase(host);
}

}  // namespace

// static
void PeerConnectionTrackerHost::AddObserver(
    base::PassKey<PeerConnectionTrackerHostObserver>,
    PeerConnectionTrackerHostObserver* observer) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  GetObserverList().AddObserver(observer);
}

// static
void PeerConnectionTrackerHost::RemoveObserver(
    base::PassKey<PeerConnectionTrackerHostObserver>,
    PeerConnectionTrackerHostObserver* observer) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  GetObserverList().RemoveObserver(observer);
}

// static
const std::set<PeerConnectionTrackerHost*>&
PeerConnectionTrackerHost::GetAllHosts() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return AllHosts();
}

PeerConnectionTrackerHost::PeerConnectionTrackerHost(RenderFrameHost* frame)
    : DocumentUserData<PeerConnectionTrackerHost>(frame),
      frame_id_(frame->GetGlobalId()),
      peer_pid_(frame->GetProcess()->GetProcess().Pid()) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  RegisterHost(this);
  auto* power_monitor = base::PowerMonitor::GetInstance();
  power_monitor->AddPowerSuspendObserver(this);
  // Ensure that the initial thermal state is known by the |tracker_|.
  base::PowerThermalObserver::DeviceThermalState initial_thermal_state =
      power_monitor->AddPowerStateObserverAndReturnPowerThermalState(this);

  frame->GetRemoteInterfaces()->GetInterface(
      tracker_.BindNewPipeAndPassReceiver());
  if (initial_thermal_state !=
      base::PowerThermalObserver::DeviceThermalState::kUnknown) {
    OnThermalStateChange(initial_thermal_state);
  }
}

PeerConnectionTrackerHost::~PeerConnectionTrackerHost() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  RemoveHost(this);
  auto* power_monitor = base::PowerMonitor::GetInstance();
  power_monitor->RemovePowerSuspendObserver(this);
  power_monitor->RemovePowerThermalObserver(this);
}

void PeerConnectionTrackerHost::AddPeerConnection(
    blink::mojom::PeerConnectionInfoPtr info) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  const std::string& url =
      (info->url == std::nullopt) ? std::string() : *info->url;

  for (auto& observer : GetObserverList()) {
    observer.OnPeerConnectionAdded(frame_id_, info->lid, peer_pid_, url,
                                   info->rtc_configuration);
  }
}

void PeerConnectionTrackerHost::RemovePeerConnection(int lid) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  for (auto& observer : GetObserverList()) {
    observer.OnPeerConnectionRemoved(frame_id_, lid);
  }
}

void PeerConnectionTrackerHost::UpdatePeerConnection(int lid,
                                                     const std::string& type,
                                                     const std::string& value) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  for (auto& observer : GetObserverList()) {
    observer.OnPeerConnectionUpdated(frame_id_, lid, type, value);
  }
}

void PeerConnectionTrackerHost::OnPeerConnectionSessionIdSet(
    int lid,
    const std::string& session_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  for (auto& observer : GetObserverList()) {
    observer.OnPeerConnectionSessionIdSet(frame_id_, lid, session_id);
  }
}

void PeerConnectionTrackerHost::AddStandardStats(int lid,
                                                 base::Value::List value) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  for (auto& observer : GetObserverList()) {
    observer.OnAddStandardStats(frame_id_, lid, value.Clone());
  }
}

void PeerConnectionTrackerHost::AddLegacyStats(int lid,
                                               base::Value::List value) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  for (auto& observer : GetObserverList()) {
    observer.OnAddLegacyStats(frame_id_, lid, value.Clone());
  }
}

void PeerConnectionTrackerHost::GetUserMedia(
    int request_id,
    bool audio,
    bool video,
    const std::string& audio_constraints,
    const std::string& video_constraints) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  for (auto& observer : GetObserverList()) {
    observer.OnGetUserMedia(frame_id_, peer_pid_, request_id, audio, video,
                            audio_constraints, video_constraints);
  }
}

void PeerConnectionTrackerHost::GetUserMediaSuccess(
    int request_id,
    const std::string& stream_id,
    const std::string& audio_track_info,
    const std::string& video_track_info) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  for (auto& observer : GetObserverList()) {
    observer.OnGetUserMediaSuccess(frame_id_, peer_pid_, request_id, stream_id,
                                   audio_track_info, video_track_info);
  }
}

void PeerConnectionTrackerHost::GetUserMediaFailure(
    int request_id,
    const std::string& error,
    const std::string& error_message) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  for (auto& observer : GetObserverList()) {
    observer.OnGetUserMediaFailure(frame_id_, peer_pid_, request_id, error,
                                   error_message);
  }
}

void PeerConnectionTrackerHost::GetDisplayMedia(
    int request_id,
    bool audio,
    bool video,
    const std::string& audio_constraints,
    const std::string& video_constraints) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  for (auto& observer : GetObserverList()) {
    observer.OnGetDisplayMedia(frame_id_, peer_pid_, request_id, audio, video,
                               audio_constraints, video_constraints);
  }
}

void PeerConnectionTrackerHost::GetDisplayMediaSuccess(
    int request_id,
    const std::string& stream_id,
    const std::string& audio_track_info,
    const std::string& video_track_info) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  for (auto& observer : GetObserverList()) {
    observer.OnGetDisplayMediaSuccess(frame_id_, peer_pid_, request_id,
                                      stream_id, audio_track_info,
                                      video_track_info);
  }
}

void PeerConnectionTrackerHost::GetDisplayMediaFailure(
    int request_id,
    const std::string& error,
    const std::string& error_message) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  for (auto& observer : GetObserverList()) {
    observer.OnGetDisplayMediaFailure(frame_id_, peer_pid_, request_id, error,
                                      error_message);
  }
}

void PeerConnectionTrackerHost::WebRtcEventLogWrite(
    int lid,
    const std::vector<uint8_t>& output) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  std::string message(output.begin(), output.end());
  for (auto& observer : GetObserverList()) {
    observer.OnWebRtcEventLogWrite(frame_id_, lid, message);
  }
}

void PeerConnectionTrackerHost::OnSuspend() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  tracker_->OnSuspend();
}

void PeerConnectionTrackerHost::OnThermalStateChange(
    base::PowerThermalObserver::DeviceThermalState new_state) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  tracker_->OnThermalStateChange(
      static_cast<blink::mojom::DeviceThermalState>(new_state));
}

void PeerConnectionTrackerHost::OnSpeedLimitChange(int new_limit) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  tracker_->OnSpeedLimitChange(new_limit);
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

void PeerConnectionTrackerHost::GetCurrentState() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  tracker_->GetCurrentState();
}

void PeerConnectionTrackerHost::BindReceiver(
    mojo::PendingReceiver<blink::mojom::PeerConnectionTrackerHost>
        pending_receiver) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  receiver_.reset();
  receiver_.Bind(std::move(pending_receiver));
}

DOCUMENT_USER_DATA_KEY_IMPL(PeerConnectionTrackerHost);
}  // namespace content
