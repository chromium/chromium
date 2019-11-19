// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/media_web_contents_observer.h"

#include <memory>

#include "build/build_config.h"
#include "content/browser/media/audible_metrics.h"
#include "content/browser/media/audio_stream_monitor.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/common/media/media_player_delegate_messages.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "ipc/ipc_message_macros.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "services/device/public/mojom/wake_lock_context.mojom.h"
#include "services/media_session/public/cpp/media_position.h"
#include "third_party/blink/public/platform/web_fullscreen_video_status.h"
#include "ui/gfx/geometry/size.h"

namespace content {

namespace {

AudibleMetrics* GetAudibleMetrics() {
  static AudibleMetrics* metrics = new AudibleMetrics();
  return metrics;
}

void CheckFullscreenDetectionEnabled(WebContents* web_contents) {
#if defined(OS_ANDROID)
  DCHECK(web_contents->GetRenderViewHost()
             ->GetWebkitPreferences()
             .video_fullscreen_detection_enabled)
      << "Attempt to use method relying on fullscreen detection while "
      << "fullscreen detection is disabled.";
#else   // defined(OS_ANDROID)
  NOTREACHED() << "Attempt to use method relying on fullscreen detection, "
               << "which is only enabled on Android.";
#endif  // defined(OS_ANDROID)
}

// Returns true if |player_id| exists in |player_map|.
bool MediaPlayerEntryExists(
    const MediaPlayerId& player_id,
    const MediaWebContentsObserver::ActiveMediaPlayerMap& player_map) {
  const auto& players = player_map.find(player_id.render_frame_host);
  if (players == player_map.end())
    return false;

  return players->second.find(player_id.delegate_id) != players->second.end();
}

#if defined(OS_ANDROID)
static void SuspendAllMediaPlayersInRenderFrame(
    RenderFrameHost* render_frame_host) {
  render_frame_host->Send(new MediaPlayerDelegateMsg_SuspendAllMediaPlayers(
      render_frame_host->GetRoutingID()));
}
#endif  // defined(OS_ANDROID)

}  // anonymous namespace

MediaWebContentsObserver::MediaWebContentsObserver(WebContents* web_contents)
    : WebContentsObserver(web_contents),
      audible_metrics_(GetAudibleMetrics()),
      session_controllers_manager_(this),
      power_experiment_manager_(MediaPowerExperimentManager::Instance()) {}

MediaWebContentsObserver::~MediaWebContentsObserver() {
  // Remove all players so that the experiment manager is notified.
  RemoveAllPlayers();
}

void MediaWebContentsObserver::WebContentsDestroyed() {
  AudioStreamMonitor* audio_stream_monitor =
      web_contents_impl()->audio_stream_monitor();

  audible_metrics_->WebContentsDestroyed(
      web_contents(), audio_stream_monitor->WasRecentlyAudible() &&
                          !web_contents()->IsAudioMuted());

  // Remove all players so that the experiment manager is notified.
  RemoveAllPlayers();
}

void MediaWebContentsObserver::RenderFrameDeleted(
    RenderFrameHost* render_frame_host) {
  ClearWakeLocks(render_frame_host);
  session_controllers_manager_.RenderFrameDeleted(render_frame_host);

  if (fullscreen_player_ &&
      fullscreen_player_->render_frame_host == render_frame_host) {
    picture_in_picture_allowed_in_fullscreen_.reset();
    fullscreen_player_.reset();
  }

  // Cancel any pending callbacks for players from this frame.
  per_frame_factory_.erase(render_frame_host);
}

void MediaWebContentsObserver::MaybeUpdateAudibleState() {
  AudioStreamMonitor* audio_stream_monitor =
      web_contents_impl()->audio_stream_monitor();

  if (audio_stream_monitor->WasRecentlyAudible())
    LockAudio();
  else
    CancelAudioLock();

  audible_metrics_->UpdateAudibleWebContentsState(
      web_contents(), audio_stream_monitor->IsCurrentlyAudible() &&
                          !web_contents()->IsAudioMuted());
}

bool MediaWebContentsObserver::HasActiveEffectivelyFullscreenVideo() const {
  CheckFullscreenDetectionEnabled(web_contents_impl());
  if (!web_contents()->IsFullscreen() || !fullscreen_player_)
    return false;

  // Check that the player is active.
  return MediaPlayerEntryExists(*fullscreen_player_, active_video_players_);
}

bool MediaWebContentsObserver::IsPictureInPictureAllowedForFullscreenVideo()
    const {
  DCHECK(picture_in_picture_allowed_in_fullscreen_.has_value());

  return *picture_in_picture_allowed_in_fullscreen_;
}

const base::Optional<MediaPlayerId>&
MediaWebContentsObserver::GetFullscreenVideoMediaPlayerId() const {
  CheckFullscreenDetectionEnabled(web_contents_impl());
  return fullscreen_player_;
}

bool MediaWebContentsObserver::OnMessageReceived(
    const IPC::Message& msg,
    RenderFrameHost* render_frame_host) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP_WITH_PARAM(MediaWebContentsObserver, msg,
                                   render_frame_host)
    IPC_MESSAGE_HANDLER(MediaPlayerDelegateHostMsg_OnMediaDestroyed,
                        OnMediaDestroyed)
    IPC_MESSAGE_HANDLER(MediaPlayerDelegateHostMsg_OnMediaPaused, OnMediaPaused)
    IPC_MESSAGE_HANDLER(MediaPlayerDelegateHostMsg_OnMediaPlaying,
                        OnMediaPlaying)
    IPC_MESSAGE_HANDLER(MediaPlayerDelegateHostMsg_OnMutedStatusChanged,
                        OnMediaMutedStatusChanged)
    IPC_MESSAGE_HANDLER(MediaPlayerDelegateHostMsg_OnMediaPositionStateChanged,
                        OnMediaPositionStateChanged);
    IPC_MESSAGE_HANDLER(
        MediaPlayerDelegateHostMsg_OnMediaEffectivelyFullscreenChanged,
        OnMediaEffectivelyFullscreenChanged)
    IPC_MESSAGE_HANDLER(MediaPlayerDelegateHostMsg_OnMediaSizeChanged,
                        OnMediaSizeChanged)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void MediaWebContentsObserver::DidUpdateAudioMutingState(bool muted) {
  session_controllers_manager_.WebContentsMutedStateChanged(muted);
}

void MediaWebContentsObserver::RequestPersistentVideo(bool value) {
  if (!fullscreen_player_)
    return;

  // The message is sent to the renderer even though the video is already the
  // fullscreen element itself. It will eventually be handled by Blink.
  fullscreen_player_->render_frame_host->Send(
      new MediaPlayerDelegateMsg_BecamePersistentVideo(
          fullscreen_player_->render_frame_host->GetRoutingID(),
          fullscreen_player_->delegate_id, value));
}

bool MediaWebContentsObserver::IsPlayerActive(
    const MediaPlayerId& player_id) const {
  if (MediaPlayerEntryExists(player_id, active_video_players_))
    return true;

  return MediaPlayerEntryExists(player_id, active_audio_players_);
}

void MediaWebContentsObserver::OnMediaDestroyed(
    RenderFrameHost* render_frame_host,
    int delegate_id) {
  // TODO(liberato): Should we skip power manager notifications in this case?
  OnMediaPaused(render_frame_host, delegate_id, true);
}

void MediaWebContentsObserver::OnMediaPaused(RenderFrameHost* render_frame_host,
                                             int delegate_id,
                                             bool reached_end_of_stream) {
  const MediaPlayerId player_id(render_frame_host, delegate_id);
  const bool removed_audio =
      RemoveMediaPlayerEntry(player_id, &active_audio_players_);
  const bool removed_video =
      RemoveMediaPlayerEntry(player_id, &active_video_players_);

  if (removed_audio || removed_video) {
    // Notify observers the player has been "paused".
    web_contents_impl()->MediaStoppedPlaying(
        WebContentsObserver::MediaPlayerInfo(removed_video, removed_audio),
        player_id,
        reached_end_of_stream
            ? WebContentsObserver::MediaStoppedReason::kReachedEndOfStream
            : WebContentsObserver::MediaStoppedReason::kUnspecified);
  }

  if (reached_end_of_stream)
    session_controllers_manager_.OnEnd(player_id);
  else
    session_controllers_manager_.OnPause(player_id);
}

void MediaWebContentsObserver::OnMediaPlaying(
    RenderFrameHost* render_frame_host,
    int delegate_id,
    bool has_video,
    bool has_audio,
    bool is_remote,
    media::MediaContentType media_content_type) {
  // TODO(mlamouri): this used to be done to avoid video wake lock. However, it
  // was doing much more. Removing will be done in a follow-up CL to avoid
  // regressions to be pinpoint to the wake lock refactor.
  if (is_remote)
    return;

  BackForwardCache::DisableForRenderFrameHost(
      render_frame_host, "MediaWebContentsObserver::OnMediaPlaying");

  const MediaPlayerId id(render_frame_host, delegate_id);
  if (has_audio)
    AddMediaPlayerEntry(id, &active_audio_players_);

  if (has_video)
    AddMediaPlayerEntry(id, &active_video_players_);

  if (!session_controllers_manager_.RequestPlay(
          id, has_audio, is_remote, media_content_type)) {
    return;
  }

  // Notify observers of the new player.
  web_contents_impl()->MediaStartedPlaying(
      WebContentsObserver::MediaPlayerInfo(has_video, has_audio), id);
}

void MediaWebContentsObserver::OnMediaEffectivelyFullscreenChanged(
    RenderFrameHost* render_frame_host,
    int delegate_id,
    blink::WebFullscreenVideoStatus fullscreen_status) {
  const MediaPlayerId id(render_frame_host, delegate_id);

  switch (fullscreen_status) {
    case blink::WebFullscreenVideoStatus::kFullscreenAndPictureInPictureEnabled:
      fullscreen_player_ = id;
      picture_in_picture_allowed_in_fullscreen_ = true;
      break;
    case blink::WebFullscreenVideoStatus::
        kFullscreenAndPictureInPictureDisabled:
      fullscreen_player_ = id;
      picture_in_picture_allowed_in_fullscreen_ = false;
      break;
    case blink::WebFullscreenVideoStatus::kNotEffectivelyFullscreen:
      if (!fullscreen_player_ || *fullscreen_player_ != id)
        return;

      picture_in_picture_allowed_in_fullscreen_.reset();
      fullscreen_player_.reset();
      break;
  }

  bool is_fullscreen =
      (fullscreen_status !=
       blink::WebFullscreenVideoStatus::kNotEffectivelyFullscreen);
  web_contents_impl()->MediaEffectivelyFullscreenChanged(is_fullscreen);
}

void MediaWebContentsObserver::OnMediaSizeChanged(
    RenderFrameHost* render_frame_host,
    int delegate_id,
    const gfx::Size& size) {
  const MediaPlayerId id(render_frame_host, delegate_id);
  web_contents_impl()->MediaResized(size, id);
}

void MediaWebContentsObserver::ClearWakeLocks(
    RenderFrameHost* render_frame_host) {
  std::set<MediaPlayerId> video_players;
  RemoveAllMediaPlayerEntries(render_frame_host, &active_video_players_,
                              &video_players);
  std::set<MediaPlayerId> audio_players;
  RemoveAllMediaPlayerEntries(render_frame_host, &active_audio_players_,
                              &audio_players);

  std::set<MediaPlayerId> removed_players;
  std::set_union(video_players.begin(), video_players.end(),
                 audio_players.begin(), audio_players.end(),
                 std::inserter(removed_players, removed_players.end()));

  // Notify all observers the player has been "paused".
  for (const auto& id : removed_players) {
    auto it = video_players.find(id);
    bool was_video = (it != video_players.end());
    bool was_audio = (audio_players.find(id) != audio_players.end());
    web_contents_impl()->MediaStoppedPlaying(
        WebContentsObserver::MediaPlayerInfo(was_video, was_audio), id,
        WebContentsObserver::MediaStoppedReason::kUnspecified);
  }
}

device::mojom::WakeLock* MediaWebContentsObserver::GetAudioWakeLock() {
  // Here is a lazy binding, and will not reconnect after connection error.
  if (!audio_wake_lock_) {
    mojo::PendingReceiver<device::mojom::WakeLock> receiver =
        audio_wake_lock_.BindNewPipeAndPassReceiver();
    device::mojom::WakeLockContext* wake_lock_context =
        web_contents()->GetWakeLockContext();
    if (wake_lock_context) {
      wake_lock_context->GetWakeLock(
          device::mojom::WakeLockType::kPreventAppSuspension,
          device::mojom::WakeLockReason::kAudioPlayback, "Playing audio",
          std::move(receiver));
    }
  }
  return audio_wake_lock_.get();
}

void MediaWebContentsObserver::LockAudio() {
  GetAudioWakeLock()->RequestWakeLock();
  has_audio_wake_lock_for_testing_ = true;
}

void MediaWebContentsObserver::CancelAudioLock() {
  GetAudioWakeLock()->CancelWakeLock();
  has_audio_wake_lock_for_testing_ = false;
}

void MediaWebContentsObserver::OnMediaMutedStatusChanged(
    RenderFrameHost* render_frame_host,
    int delegate_id,
    bool muted) {
  const MediaPlayerId id(render_frame_host, delegate_id);
  web_contents_impl()->MediaMutedStatusChanged(id, muted);
}

void MediaWebContentsObserver::OnMediaPositionStateChanged(
    RenderFrameHost* render_frame_host,
    int delegate_id,
    const media_session::MediaPosition& position) {
  const MediaPlayerId id(render_frame_host, delegate_id);
  session_controllers_manager_.OnMediaPositionStateChanged(id, position);
}

void MediaWebContentsObserver::AddMediaPlayerEntry(
    const MediaPlayerId& id,
    ActiveMediaPlayerMap* player_map) {
  (*player_map)[id.render_frame_host].insert(id.delegate_id);
  if (power_experiment_manager_) {
    power_experiment_manager_->PlayerStarted(
        id,
        base::BindRepeating(&MediaWebContentsObserver::OnExperimentStateChanged,
                            GetWeakPtrForFrame(id.render_frame_host), id));
  }
}

bool MediaWebContentsObserver::RemoveMediaPlayerEntry(
    const MediaPlayerId& id,
    ActiveMediaPlayerMap* player_map) {
  // If the power experiment is running, then notify it.
  if (power_experiment_manager_)
    power_experiment_manager_->PlayerStopped(id);

  auto it = player_map->find(id.render_frame_host);
  if (it == player_map->end())
    return false;

  // Remove the player.
  bool did_remove = it->second.erase(id.delegate_id) == 1;
  if (!did_remove)
    return false;

  // If there are no players left, remove the map entry.
  if (it->second.empty())
    player_map->erase(it);

  return true;
}

void MediaWebContentsObserver::RemoveAllMediaPlayerEntries(
    RenderFrameHost* render_frame_host,
    ActiveMediaPlayerMap* player_map,
    std::set<MediaPlayerId>* removed_players) {
  auto it = player_map->find(render_frame_host);
  if (it == player_map->end())
    return;

  for (int delegate_id : it->second) {
    MediaPlayerId id(render_frame_host, delegate_id);
    removed_players->insert(id);

    // Since the player is being destroyed, don't bother to notify it if it's
    // no longer the active experiment.
    if (power_experiment_manager_) {
      power_experiment_manager_->PlayerStopped(
          id, MediaPowerExperimentManager::NotificationMode::kSkip);
    }
  }

  player_map->erase(it);
}

WebContentsImpl* MediaWebContentsObserver::web_contents_impl() const {
  return static_cast<WebContentsImpl*>(web_contents());
}

#if defined(OS_ANDROID)
void MediaWebContentsObserver::SuspendAllMediaPlayers() {
  web_contents()->ForEachFrame(
      base::BindRepeating(&SuspendAllMediaPlayersInRenderFrame));
}
#endif  // defined(OS_ANDROID)

void MediaWebContentsObserver::OnExperimentStateChanged(MediaPlayerId id,
                                                        bool is_starting) {
  // TODO(liberato): Notify the player.
}

void MediaWebContentsObserver::RemoveAllPlayers(
    ActiveMediaPlayerMap* player_map) {
  if (power_experiment_manager_) {
    for (auto& iter : *player_map) {
      for (auto delegate_id : iter.second) {
        MediaPlayerId id(iter.first, delegate_id);
        power_experiment_manager_->PlayerStopped(
            id, MediaPowerExperimentManager::NotificationMode::kSkip);
      }
    }
  }

  player_map->clear();
}

void MediaWebContentsObserver::RemoveAllPlayers() {
  RemoveAllPlayers(&active_audio_players_);
  RemoveAllPlayers(&active_video_players_);
}

base::WeakPtr<MediaWebContentsObserver>
MediaWebContentsObserver::GetWeakPtrForFrame(
    RenderFrameHost* render_frame_host) {
  auto iter = per_frame_factory_.find(render_frame_host);
  if (iter != per_frame_factory_.end())
    return iter->second->GetWeakPtr();

  auto result = per_frame_factory_.emplace(std::make_pair(
      render_frame_host,
      std::make_unique<base::WeakPtrFactory<MediaWebContentsObserver>>(this)));
  return result.first->second->GetWeakPtr();
}

}  // namespace content
