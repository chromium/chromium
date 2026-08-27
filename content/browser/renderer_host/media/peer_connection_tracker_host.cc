// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/media/peer_connection_tracker_host.h"

#include <algorithm>
#include <iterator>
#include <set>
#include <utility>

#include "base/barrier_closure.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/no_destructor.h"
#include "base/observer_list.h"
#include "base/power_monitor/power_monitor.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "media/base/media_switches.h"
#include "services/service_manager/public/cpp/interface_provider.h"

namespace content {

namespace {

using ObserverListType =
    base::ObserverList<PeerConnectionTrackerHostObserver,
                       /*check_empty=*/true,
                       base::ObserverListReentrancyPolicy::kDisallowReentrancy>;
ObserverListType& GetObserverList() {
  CHECK_CURRENTLY_ON(BrowserThread::UI, base::NotFatalUntil::M158);
  static base::NoDestructor<ObserverListType> observer_list{};
  return *observer_list;
}

std::set<PeerConnectionTrackerHost*>& AllHosts() {
  CHECK_CURRENTLY_ON(BrowserThread::UI, base::NotFatalUntil::M158);
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
  CHECK_CURRENTLY_ON(BrowserThread::UI, base::NotFatalUntil::M158);
  GetObserverList().AddObserver(observer);
}

// static
void PeerConnectionTrackerHost::RemoveObserver(
    base::PassKey<PeerConnectionTrackerHostObserver>,
    PeerConnectionTrackerHostObserver* observer) {
  CHECK_CURRENTLY_ON(BrowserThread::UI, base::NotFatalUntil::M158);
  GetObserverList().RemoveObserver(observer);
}

// static
const std::set<PeerConnectionTrackerHost*>&
PeerConnectionTrackerHost::GetAllHosts() {
  CHECK_CURRENTLY_ON(BrowserThread::UI, base::NotFatalUntil::M158);
  return AllHosts();
}

PeerConnectionTrackerHost::PeerConnectionTrackerHost(RenderFrameHost* frame)
    : DocumentUserData<PeerConnectionTrackerHost>(frame),
      frame_id_(frame->GetGlobalId()),
      peer_pid_(frame->GetProcess()->GetProcess().Pid()) {
  CHECK_CURRENTLY_ON(BrowserThread::UI, base::NotFatalUntil::M158);
  RegisterHost(this);
#if BUILDFLAG(IS_ANDROID)
  if (base::FeatureList::IsEnabled(media::kAndroidSuspendWebRtcOnScreenOff)) {
    base::android::ScreenStateReceiver::AddObserver(this);
  }
#endif
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
  CHECK_CURRENTLY_ON(BrowserThread::UI, base::NotFatalUntil::M158);
  for (int lid : peer_connection_lids_) {
    for (auto& observer : GetObserverList()) {
      observer.OnPeerConnectionRemoved(frame_id_, lid);
    }
  }
#if BUILDFLAG(IS_ANDROID)
  if (base::FeatureList::IsEnabled(media::kAndroidSuspendWebRtcOnScreenOff)) {
    base::android::ScreenStateReceiver::RemoveObserver(this);
  }
#endif
  RemoveHost(this);
  auto* power_monitor = base::PowerMonitor::GetInstance();
  power_monitor->RemovePowerSuspendObserver(this);
  power_monitor->RemovePowerThermalObserver(this);
}

void PeerConnectionTrackerHost::AddPeerConnection(
    blink::mojom::PeerConnectionInfoPtr info) {
  CHECK_CURRENTLY_ON(BrowserThread::UI, base::NotFatalUntil::M158);

  const std::string& url =
      (info->url == std::nullopt) ? std::string() : *info->url;

  peer_connection_lids_.insert(info->lid);
  for (auto& observer : GetObserverList()) {
    observer.OnPeerConnectionAdded(frame_id_, info->lid, peer_pid_, url,
                                   info->rtc_configuration);
  }
}

void PeerConnectionTrackerHost::RemovePeerConnection(int lid) {
  CHECK_CURRENTLY_ON(BrowserThread::UI, base::NotFatalUntil::M158);

  peer_connection_lids_.erase(lid);
  for (auto& observer : GetObserverList()) {
    observer.OnPeerConnectionRemoved(frame_id_, lid);
  }
}

void PeerConnectionTrackerHost::UpdatePeerConnection(int lid,
                                                     const std::string& type,
                                                     const std::string& value) {
  CHECK_CURRENTLY_ON(BrowserThread::UI, base::NotFatalUntil::M158);

  for (auto& observer : GetObserverList()) {
    observer.OnPeerConnectionUpdated(frame_id_, lid, type, value);
  }
}

void PeerConnectionTrackerHost::OnPeerConnectionSessionIdSet(
    int lid,
    const std::string& session_id,
    base::OnceClosure callback) {
  CHECK_CURRENTLY_ON(BrowserThread::UI, base::NotFatalUntil::M158);

  // The observer list does not have a method to query the number of observers.
  // The correctness of `count` relies on OnPeerConnectionSessionIdSet not
  // removing or adding any observers.
  const size_t count =
      std::distance(GetObserverList().begin(), GetObserverList().end());

  base::RepeatingClosure barrier =
      base::BarrierClosure(count, std::move(callback));
  for (auto& observer : GetObserverList()) {
    observer.OnPeerConnectionSessionIdSet(frame_id_, lid, session_id, barrier);
  }
}

void PeerConnectionTrackerHost::AddStandardStats(int lid,
                                                 base::ListValue value) {
  CHECK_CURRENTLY_ON(BrowserThread::UI, base::NotFatalUntil::M158);

  for (auto& observer : GetObserverList()) {
    observer.OnAddStandardStats(frame_id_, lid, value.Clone());
  }
}

void PeerConnectionTrackerHost::GetUserMedia(
    int request_id,
    bool audio,
    bool video,
    const std::string& audio_constraints,
    const std::string& video_constraints) {
  CHECK_CURRENTLY_ON(BrowserThread::UI, base::NotFatalUntil::M158);

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
  CHECK_CURRENTLY_ON(BrowserThread::UI, base::NotFatalUntil::M158);

  for (auto& observer : GetObserverList()) {
    observer.OnGetUserMediaSuccess(frame_id_, peer_pid_, request_id, stream_id,
                                   audio_track_info, video_track_info);
  }
}

void PeerConnectionTrackerHost::GetUserMediaFailure(
    int request_id,
    const std::string& error,
    const std::string& error_message) {
  CHECK_CURRENTLY_ON(BrowserThread::UI, base::NotFatalUntil::M158);

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
  CHECK_CURRENTLY_ON(BrowserThread::UI, base::NotFatalUntil::M158);

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
  CHECK_CURRENTLY_ON(BrowserThread::UI, base::NotFatalUntil::M158);

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
  CHECK_CURRENTLY_ON(BrowserThread::UI, base::NotFatalUntil::M158);

  for (auto& observer : GetObserverList()) {
    observer.OnGetDisplayMediaFailure(frame_id_, peer_pid_, request_id, error,
                                      error_message);
  }
}

void PeerConnectionTrackerHost::WebRtcEventLogWrite(
    int lid,
    const std::vector<uint8_t>& output) {
  CHECK_CURRENTLY_ON(BrowserThread::UI, base::NotFatalUntil::M158);

  std::string message(output.begin(), output.end());
  for (auto& observer : GetObserverList()) {
    observer.OnWebRtcEventLogWrite(frame_id_, lid, message);
  }
}

void PeerConnectionTrackerHost::OnSuspend() {
  CHECK_CURRENTLY_ON(BrowserThread::UI, base::NotFatalUntil::M158);
  tracker_->OnSuspend();
}

void PeerConnectionTrackerHost::OnThermalStateChange(
    base::PowerThermalObserver::DeviceThermalState new_state) {
  CHECK_CURRENTLY_ON(BrowserThread::UI, base::NotFatalUntil::M158);
  tracker_->OnThermalStateChange(
      static_cast<blink::mojom::DeviceThermalState>(new_state));
}

void PeerConnectionTrackerHost::StartEventLog(int lid, int output_period_ms) {
  CHECK_CURRENTLY_ON(BrowserThread::UI, base::NotFatalUntil::M158);
  tracker_->StartEventLog(lid, output_period_ms);
}

void PeerConnectionTrackerHost::StopEventLog(int lid) {
  CHECK_CURRENTLY_ON(BrowserThread::UI, base::NotFatalUntil::M158);
  tracker_->StopEventLog(lid);
}

void PeerConnectionTrackerHost::StartDataChannelLog(int lid) {
  CHECK_CURRENTLY_ON(BrowserThread::UI, base::NotFatalUntil::M158);
  tracker_->StartDataChannelLog(lid);
}

void PeerConnectionTrackerHost::StopDataChannelLog(int lid) {
  CHECK_CURRENTLY_ON(BrowserThread::UI, base::NotFatalUntil::M158);
  tracker_->StopDataChannelLog(lid);
}

void PeerConnectionTrackerHost::WebRtcDataChannelLogWrite(
    int lid,
    const std::vector<uint8_t>& output) {
  CHECK_CURRENTLY_ON(BrowserThread::UI, base::NotFatalUntil::M158);

  std::string message(output.begin(), output.end());
  for (auto& observer : GetObserverList()) {
    observer.OnWebRtcDataChannelLogWrite(frame_id_, lid, message);
  }
}

void PeerConnectionTrackerHost::GetStandardStats() {
  CHECK_CURRENTLY_ON(BrowserThread::UI, base::NotFatalUntil::M158);
  tracker_->GetStandardStats();
}

void PeerConnectionTrackerHost::GetCurrentState() {
  CHECK_CURRENTLY_ON(BrowserThread::UI, base::NotFatalUntil::M158);
  tracker_->GetCurrentState();
}

void PeerConnectionTrackerHost::BindReceiver(
    mojo::PendingReceiver<blink::mojom::PeerConnectionTrackerHost>
        pending_receiver) {
  CHECK_CURRENTLY_ON(BrowserThread::UI, base::NotFatalUntil::M158);
  receiver_.reset();
  receiver_.Bind(std::move(pending_receiver));
}

DOCUMENT_USER_DATA_KEY_IMPL(PeerConnectionTrackerHost);

#if BUILDFLAG(IS_ANDROID)
// Android does not provide an API for apps to be notified of system suspend.
// Therefore, base::PowerSuspendObserver::OnSuspend is never triggered on
// Android. As a workaround, we use the SCREEN_OFF event as a proxy to trigger
// WebRTC suspend, ensuring hardware resources are released.
void PeerConnectionTrackerHost::OnScreenOff() {
  CHECK_CURRENTLY_ON(BrowserThread::UI, base::NotFatalUntil::M158);
  OnSuspend();
}
#endif
}  // namespace content
