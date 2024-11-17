// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/media_web_contents_observer.h"

#include <memory>
#include <tuple>

#include "base/debug/crash_logging.h"
#include "base/debug/dump_without_crashing.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "build/build_config.h"
#include "content/browser/media/audible_metrics.h"
#include "content/browser/media/media_devices_util.h"
#include "content/browser/renderer_host/media/media_stream_manager.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "mojo/public/cpp/bindings/message.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "services/device/public/mojom/wake_lock_context.mojom.h"
#include "services/media_session/public/cpp/media_position.h"
#include "third_party/blink/public/common/mediastream/media_devices.h"
#include "third_party/blink/public/platform/web_fullscreen_video_status.h"
#include "ui/gfx/geometry/size.h"

namespace content {

namespace {

AudibleMetrics* GetAudibleMetrics() {
  static AudibleMetrics* metrics = new AudibleMetrics();
  return metrics;
}

}  // anonymous namespace

// Maintains state for a single player.  Issues WebContents and power-related
// notifications appropriate for state changes.
class MediaWebContentsObserver::PlayerInfo {
 public:
  PlayerInfo(const MediaPlayerId& id, MediaWebContentsObserver* observer)
      : id_(id), observer_(observer) {}

  ~PlayerInfo() {
    if (is_playing_) {
      NotifyPlayerStopped(WebContentsObserver::MediaStoppedReason::kUnspecified,
                          MediaPowerExperimentManager::NotificationMode::kSkip);
    }
  }

  PlayerInfo(const PlayerInfo&) = delete;
  PlayerInfo& operator=(const PlayerInfo&) = delete;

  void set_has_audio(bool has_audio) { has_audio_ = has_audio; }

  bool has_video() const { return has_video_; }
  void set_has_video(bool has_video) { has_video_ = has_video; }

  void set_muted(bool muted) { muted_ = muted; }

  bool is_playing() const { return is_playing_; }

  void SetIsPlaying() {
    DCHECK(!is_playing_);
    is_playing_ = true;

    NotifyPlayerStarted();
  }

  void SetIsStopped(bool reached_end_of_stream) {
    DCHECK(is_playing_);
    is_playing_ = false;

    NotifyPlayerStopped(
        reached_end_of_stream
            ? WebContentsObserver::MediaStoppedReason::kReachedEndOfStream
            : WebContentsObserver::MediaStoppedReason::kUnspecified,
        MediaPowerExperimentManager::NotificationMode::kNotify);
  }

  bool IsAudible() const { return has_audio_ && is_playing_ && !muted_; }

  GlobalRenderFrameHostId GetHostId() { return id_.frame_routing_id; }

 private:
  void NotifyPlayerStarted() {
    observer_->web_contents_impl()->MediaStartedPlaying(
        WebContentsObserver::MediaPlayerInfo(has_video_, has_audio_), id_);

    if (observer_->power_experiment_manager_) {
      auto* render_frame_host = RenderFrameHost::FromID(id_.frame_routing_id);
      DCHECK(render_frame_host);

      // Bind the callback to a WeakPtr for the frame, so that we won't try to
      // notify the frame after it's been destroyed.
      observer_->power_experiment_manager_->PlayerStarted(
          id_, base::BindRepeating(
                   &MediaWebContentsObserver::OnExperimentStateChanged,
                   observer_->GetWeakPtrForFrame(render_frame_host), id_));
    }
  }

  void NotifyPlayerStopped(
      WebContentsObserver::MediaStoppedReason stopped_reason,
      MediaPowerExperimentManager::NotificationMode notification_mode) {
    observer_->web_contents_impl()->MediaStoppedPlaying(
        WebContentsObserver::MediaPlayerInfo(has_video_, has_audio_), id_,
        stopped_reason);

    if (observer_->power_experiment_manager_) {
      observer_->power_experiment_manager_->PlayerStopped(id_,
                                                          notification_mode);
    }
  }

  const MediaPlayerId id_;
  const raw_ptr<MediaWebContentsObserver> observer_;

  bool has_audio_ = false;
  bool has_video_ = false;
  bool muted_ = false;
  bool is_playing_ = false;
};

MediaWebContentsObserver::MediaWebContentsObserver(
    WebContentsImpl* web_contents)
    : WebContentsObserver(web_contents),
      audible_metrics_(GetAudibleMetrics()),
      session_controllers_manager_(
          std::make_unique<MediaSessionControllersManager>(web_contents)),
      power_experiment_manager_(MediaPowerExperimentManager::Instance()) {}

MediaWebContentsObserver::~MediaWebContentsObserver() = default;

void MediaWebContentsObserver::WebContentsDestroyed() {
  CHECK_CURRENTLY_ON(BrowserThread::UI);
  use_after_free_checker_.check();
  AudioStreamMonitor* audio_stream_monitor =
      web_contents_impl()->audio_stream_monitor();

  audible_metrics_->WebContentsDestroyed(
      web_contents(), audio_stream_monitor->WasRecentlyAudible() &&
                          !web_contents()->IsAudioMuted());

  // Remove all players so that the experiment manager is notified.
  player_info_map_.clear();

  // Remove all the mojo receivers and remotes associated to the media players
  // handled by this WebContents to prevent from handling/sending any more
  // messages after this point, plus properly cleaning things up.
  media_player_hosts_.clear();
  media_player_observer_hosts_.clear();
  media_player_remotes_.clear();

  session_controllers_manager_.reset();
  fullscreen_player_.reset();
}

void MediaWebContentsObserver::RenderFrameDeleted(
    RenderFrameHost* render_frame_host) {
  CHECK_CURRENTLY_ON(BrowserThread::UI);
  use_after_free_checker_.check();

  GlobalRenderFrameHostId frame_routing_id = render_frame_host->GetGlobalId();

  // This cannot just use `base::EraseIf()`, because some observers call back
  // into `this` and query player info when a PlayerInfo is destroyed (!!).
  // Re-entering a container while erasing an entry is generally not very safe
  // or robust.
  for (auto it = player_info_map_.begin(); it != player_info_map_.end();) {
    if (it->first.frame_routing_id != frame_routing_id) {
      ++it;
      continue;
    }
    // Instead, remove entries in a multi-step process:
    // 1. Move ownership of the PlayerInfo out of the map.
    // 2. Erase the entry from the map.
    // 3. Destroy the PlayerInfo (by letting it go out of scope).
    //    Because the entry is already gone from the map, GetPlayerInfo() will
    //    return null instead of trying to compare keys that are potentially
    //    destroyed.
    auto player_info = std::move(it->second);
    it = player_info_map_.erase(it);
  }

  base::EraseIf(media_player_hosts_,
                [frame_routing_id](const MediaPlayerHostImplMap::value_type&
                                       media_player_hosts_value_type) {
                  return frame_routing_id ==
                         media_player_hosts_value_type.first;
                });

  base::EraseIf(
      media_player_observer_hosts_,
      [frame_routing_id](const MediaPlayerObserverHostImplMap::value_type&
                             media_player_observer_hosts_value_type) {
        return frame_routing_id ==
               media_player_observer_hosts_value_type.first.frame_routing_id;
      });

  std::vector<MediaPlayerId> removed_media_players;
  base::EraseIf(
      media_player_remotes_, [&](const MediaPlayerRemotesMap::value_type&
                                     media_player_remotes_value_type) {
        const MediaPlayerId& player_id = media_player_remotes_value_type.first;
        if (frame_routing_id == player_id.frame_routing_id) {
          removed_media_players.push_back(player_id);
          return true;
        }
        return false;
      });

  for (const MediaPlayerId& player_id : removed_media_players) {
    // Call MediaDestroyed() after all state associated with the media player is
    // deleted, to ensure that observers see up-to-date state. For example,
    // HasActiveEffectivelyFullscreenVideo() should return false if `player_id`
    // was the only fullscreen media.
    web_contents_impl()->MediaDestroyed(player_id);
  }

  session_controllers_manager_->RenderFrameDeleted(render_frame_host);

  if (fullscreen_player_ &&
      fullscreen_player_->frame_routing_id == frame_routing_id) {
    picture_in_picture_allowed_in_fullscreen_.reset();
    fullscreen_player_.reset();
  }

  // Cancel any pending callbacks for players from this frame.
  use_after_free_checker_.check();
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
  CHECK_CURRENTLY_ON(BrowserThread::UI);
  use_after_free_checker_.check();
  CHECK(!fullscreen_player_.has_value() ||
        picture_in_picture_allowed_in_fullscreen_.has_value());
  if (!web_contents()->IsFullscreen() || !fullscreen_player_)
    return false;

  // Check that the player is active.
  if (const PlayerInfo* player_info = GetPlayerInfo(*fullscreen_player_))
    return player_info->is_playing() && player_info->has_video();

  return false;
}

bool MediaWebContentsObserver::IsPictureInPictureAllowedForFullscreenVideo()
    const {
  CHECK_CURRENTLY_ON(BrowserThread::UI);
  use_after_free_checker_.check();
  CHECK(fullscreen_player_.has_value());
  CHECK(picture_in_picture_allowed_in_fullscreen_.has_value());

  return *picture_in_picture_allowed_in_fullscreen_;
}

const std::optional<MediaPlayerId>&
MediaWebContentsObserver::GetFullscreenVideoMediaPlayerId() const {
  return fullscreen_player_;
}

void MediaWebContentsObserver::MediaPictureInPictureChanged(
    bool is_picture_in_picture) {
  session_controllers_manager_->PictureInPictureStateChanged(
      is_picture_in_picture);
}

void MediaWebContentsObserver::DidUpdateAudioMutingState(bool muted) {
  session_controllers_manager_->WebContentsMutedStateChanged(muted);
}

void MediaWebContentsObserver::GetHasPlayedBefore(
    GetHasPlayedBeforeCallback callback) {
  std::move(callback).Run(has_played_before_);
}

void MediaWebContentsObserver::BindMediaPlayerObserverClient(
    mojo::PendingReceiver<media::mojom::MediaPlayerObserverClient>
        pending_receiver) {
  receivers_.Add(this, std::move(pending_receiver));
}

void MediaWebContentsObserver::RequestPersistentVideo(bool value) {
  if (!fullscreen_player_)
    return;

  // The message is sent to the renderer even though the video is already the
  // fullscreen element itself. It will eventually be handled by Blink.
  GetMediaPlayerRemote(*fullscreen_player_)->SetPersistentState(value);
}

bool MediaWebContentsObserver::IsPlayerActive(
    const MediaPlayerId& player_id) const {
  if (const PlayerInfo* player_info = GetPlayerInfo(player_id))
    return player_info->is_playing();

  return false;
}

// MediaWebContentsObserver::MediaPlayerHostImpl

MediaWebContentsObserver::MediaPlayerHostImpl::MediaPlayerHostImpl(
    GlobalRenderFrameHostId frame_routing_id,
    MediaWebContentsObserver* media_web_contents_observer)
    : frame_routing_id_(frame_routing_id),
      media_web_contents_observer_(media_web_contents_observer) {}

MediaWebContentsObserver::MediaPlayerHostImpl::~MediaPlayerHostImpl() = default;

void MediaWebContentsObserver::MediaPlayerHostImpl::AddMediaPlayerHostReceiver(
    mojo::PendingAssociatedReceiver<media::mojom::MediaPlayerHost> receiver) {
  receivers_.Add(this, std::move(receiver));
}

void MediaWebContentsObserver::MediaPlayerHostImpl::OnMediaPlayerAdded(
    mojo::PendingAssociatedRemote<media::mojom::MediaPlayer> media_player,
    mojo::PendingAssociatedReceiver<media::mojom::MediaPlayerObserver>
        media_player_observer,
    int32_t player_id) {
  media_web_contents_observer_->OnMediaPlayerAdded(
      std::move(media_player), std::move(media_player_observer),
      MediaPlayerId(frame_routing_id_, player_id));
}

// MediaWebContentsObserver::MediaPlayerObserverHostImpl

MediaWebContentsObserver::MediaPlayerObserverHostImpl::
    MediaPlayerObserverHostImpl(
        const MediaPlayerId& media_player_id,
        MediaWebContentsObserver* media_web_contents_observer)
    : media_player_id_(media_player_id),
      media_web_contents_observer_(media_web_contents_observer) {}

MediaWebContentsObserver::MediaPlayerObserverHostImpl::
    ~MediaPlayerObserverHostImpl() = default;

void MediaWebContentsObserver::MediaPlayerObserverHostImpl::
    BindMediaPlayerObserverReceiver(
        mojo::PendingAssociatedReceiver<media::mojom::MediaPlayerObserver>
            media_player_observer) {
  media_player_observer_receiver_.Bind(std::move(media_player_observer));

  // |media_web_contents_observer_| outlives MediaPlayerHostImpl, so it's safe
  // to use base::Unretained().
  media_player_observer_receiver_.set_disconnect_handler(base::BindOnce(
      &MediaWebContentsObserver::OnMediaPlayerObserverDisconnected,
      base::Unretained(media_web_contents_observer_), media_player_id_));
}

void MediaWebContentsObserver::MediaPlayerObserverHostImpl::
    OnMutedStatusChanged(bool muted) {
  media_web_contents_observer_->web_contents_impl()->MediaMutedStatusChanged(
      media_player_id_, muted);

  media_web_contents_observer_->session_controllers_manager()
      ->OnMediaMutedStatusChanged(media_player_id_, muted);

  PlayerInfo* player_info = GetPlayerInfo();
  if (!player_info)
    return;

  player_info->set_muted(muted);
  NotifyAudioStreamMonitorIfNeeded();
}

void MediaWebContentsObserver::MediaPlayerObserverHostImpl::
    OnMediaMetadataChanged(bool has_audio,
                           bool has_video,
                           media::MediaContentType media_content_type) {
  media_web_contents_observer_->OnMediaMetadataChanged(
      media_player_id_, has_audio, has_video, media_content_type);

  NotifyAudioStreamMonitorIfNeeded();
}

void MediaWebContentsObserver::MediaPlayerObserverHostImpl::
    OnMediaPositionStateChanged(
        const media_session::MediaPosition& media_position) {
  media_web_contents_observer_->session_controllers_manager()
      ->OnMediaPositionStateChanged(media_player_id_, media_position);
}

void MediaWebContentsObserver::MediaPlayerObserverHostImpl::
    OnMediaEffectivelyFullscreenChanged(
        blink::WebFullscreenVideoStatus status) {
  media_web_contents_observer_->OnMediaEffectivelyFullscreenChanged(
      media_player_id_, status);
}

void MediaWebContentsObserver::MediaPlayerObserverHostImpl::OnMediaSizeChanged(
    const ::gfx::Size& size) {
  media_web_contents_observer_->web_contents_impl()->MediaResized(
      size, media_player_id_);
}

void MediaWebContentsObserver::MediaPlayerObserverHostImpl::
    OnPictureInPictureAvailabilityChanged(bool available) {
  media_web_contents_observer_->session_controllers_manager()
      ->OnPictureInPictureAvailabilityChanged(media_player_id_, available);
}

void MediaWebContentsObserver::MediaPlayerObserverHostImpl::
    OnAudioOutputSinkChanged(const std::string& hashed_device_id) {
  auto* render_frame_host =
      RenderFrameHost::FromID(media_player_id_.frame_routing_id);
  DCHECK(render_frame_host);

  content::GetRawDeviceIdFromHMAC(
      render_frame_host->GetGlobalId(), hashed_device_id,
      blink::mojom::MediaDeviceType::kMediaAudioOutput,
      base::BindOnce(&MediaPlayerObserverHostImpl::OnReceivedTranslatedDeviceId,
                     weak_factory_.GetWeakPtr()));
}

void MediaWebContentsObserver::MediaPlayerObserverHostImpl::
    OnReceivedTranslatedDeviceId(
        const std::optional<std::string>& translated_id) {
  if (!translated_id)
    return;

  media_web_contents_observer_->OnAudioOutputSinkChangedWithRawDeviceId(
      media_player_id_, *translated_id);
}

void MediaWebContentsObserver::MediaPlayerObserverHostImpl::
    OnUseAudioServiceChanged(bool uses_audio_service) {
  uses_audio_service_ = uses_audio_service;
  NotifyAudioStreamMonitorIfNeeded();
}

void MediaWebContentsObserver::MediaPlayerObserverHostImpl::
    OnAudioOutputSinkChangingDisabled() {
  media_web_contents_observer_->session_controllers_manager()
      ->OnAudioOutputSinkChangingDisabled(media_player_id_);
}

void MediaWebContentsObserver::MediaPlayerObserverHostImpl::
    OnRemotePlaybackMetadataChange(
        media_session::mojom::RemotePlaybackMetadataPtr
            remote_playback_metadata) {
  media_web_contents_observer_->OnRemotePlaybackMetadataChange(
      media_player_id_, std::move(remote_playback_metadata));
}

void MediaWebContentsObserver::MediaPlayerObserverHostImpl::OnMediaPlaying() {
  PlayerInfo* player_info = GetPlayerInfo();
  if (!player_info)
    return;

  if (!media_web_contents_observer_->session_controllers_manager()->RequestPlay(
          media_player_id_)) {
    // Return early to avoid spamming WebContents with playing/stopped
    // notifications.  If RequestPlay() fails, media session will send a pause
    // signal right away.
    return;
  }

  if (!player_info->is_playing())
    player_info->SetIsPlaying();

  media_web_contents_observer_->OnMediaPlaying();
  NotifyAudioStreamMonitorIfNeeded();
}

void MediaWebContentsObserver::MediaPlayerObserverHostImpl::OnMediaPaused(
    bool stream_ended) {
  PlayerInfo* player_info = GetPlayerInfo();
  if (!player_info || !player_info->is_playing())
    return;

  player_info->SetIsStopped(stream_ended);

  media_web_contents_observer_->session_controllers_manager()->OnPause(
      media_player_id_, stream_ended);

  NotifyAudioStreamMonitorIfNeeded();
}

void MediaWebContentsObserver::MediaPlayerObserverHostImpl::
    NotifyAudioStreamMonitorIfNeeded() {
  PlayerInfo* player_info = GetPlayerInfo();
  if (!player_info)
    return;

  bool should_add_client = player_info->IsAudible() && !uses_audio_service_;
  auto* audio_stream_monitor =
      media_web_contents_observer_->web_contents_impl()->audio_stream_monitor();

  if (should_add_client && !audio_client_registration_) {
    audio_client_registration_ =
        audio_stream_monitor->RegisterAudibleClient(player_info->GetHostId());
  } else if (!should_add_client && audio_client_registration_) {
    audio_client_registration_.reset();
  }
}

MediaWebContentsObserver::PlayerInfo*
MediaWebContentsObserver::MediaPlayerObserverHostImpl::GetPlayerInfo() {
  return media_web_contents_observer_->GetPlayerInfo(media_player_id_);
}

// MediaWebContentsObserver

MediaWebContentsObserver::PlayerInfo* MediaWebContentsObserver::GetPlayerInfo(
    const MediaPlayerId& id) const {
  const auto it = player_info_map_.find(id);
  return it != player_info_map_.end() ? it->second.get() : nullptr;
}

void MediaWebContentsObserver::OnMediaMetadataChanged(
    const MediaPlayerId& player_id,
    bool has_audio,
    bool has_video,
    media::MediaContentType media_content_type) {
  PlayerInfo* player_info = GetPlayerInfo(player_id);
  if (!player_info) {
    PlayerInfoMap::iterator it;
    std::tie(it, std::ignore) = player_info_map_.emplace(
        player_id, std::make_unique<PlayerInfo>(player_id, this));
    player_info = it->second.get();
  }

  player_info->set_has_audio(has_audio);
  player_info->set_has_video(has_video);

  session_controllers_manager_->OnMetadata(player_id, has_audio, has_video,
                                           media_content_type);
}

void MediaWebContentsObserver::OnMediaEffectivelyFullscreenChanged(
    const MediaPlayerId& player_id,
    blink::WebFullscreenVideoStatus fullscreen_status) {
  CHECK_CURRENTLY_ON(BrowserThread::UI);
  use_after_free_checker_.check();
  CHECK(!fullscreen_player_.has_value() ||
        picture_in_picture_allowed_in_fullscreen_.has_value());
  switch (fullscreen_status) {
    case blink::WebFullscreenVideoStatus::kFullscreenAndPictureInPictureEnabled:
      fullscreen_player_ = player_id;
      picture_in_picture_allowed_in_fullscreen_ = true;
      break;
    case blink::WebFullscreenVideoStatus::
        kFullscreenAndPictureInPictureDisabled:
      fullscreen_player_ = player_id;
      picture_in_picture_allowed_in_fullscreen_ = false;
      break;
    case blink::WebFullscreenVideoStatus::kNotEffectivelyFullscreen:
      if (!fullscreen_player_ || *fullscreen_player_ != player_id)
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

void MediaWebContentsObserver::OnMediaPlaying() {
  has_played_before_ = true;
}

void MediaWebContentsObserver::OnAudioOutputSinkChangedWithRawDeviceId(
    const MediaPlayerId& player_id,
    const std::string& raw_device_id) {
  session_controllers_manager_->OnAudioOutputSinkChanged(player_id,
                                                         raw_device_id);
}

void MediaWebContentsObserver::OnRemotePlaybackMetadataChange(
    const MediaPlayerId& player_id,
    media_session::mojom::RemotePlaybackMetadataPtr remote_playback_metadata) {
  session_controllers_manager_->OnRemotePlaybackMetadataChange(
      player_id, std::move(remote_playback_metadata));
}

void MediaWebContentsObserver::MediaPlayerObserverHostImpl::
    OnVideoVisibilityChanged(bool meets_visibility_threshold) {
  media_web_contents_observer_->session_controllers_manager()
      ->OnVideoVisibilityChanged(media_player_id_, meets_visibility_threshold);
}

bool MediaWebContentsObserver::IsMediaPlayerRemoteAvailable(
    const MediaPlayerId& player_id) {
  return media_player_remotes_.contains(player_id);
}

mojo::AssociatedRemote<media::mojom::MediaPlayer>&
MediaWebContentsObserver::GetMediaPlayerRemote(const MediaPlayerId& player_id) {
  return media_player_remotes_.at(player_id);
}

void MediaWebContentsObserver::OnMediaPlayerObserverDisconnected(
    const MediaPlayerId& player_id) {
  DCHECK(media_player_observer_hosts_.contains(player_id));
  media_player_observer_hosts_.erase(player_id);
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
  if (audio_wake_lock_)
    GetAudioWakeLock()->CancelWakeLock();
  has_audio_wake_lock_for_testing_ = false;
}

WebContentsImpl* MediaWebContentsObserver::web_contents_impl() const {
  return static_cast<WebContentsImpl*>(web_contents());
}

void MediaWebContentsObserver::BindMediaPlayerHost(
    GlobalRenderFrameHostId frame_routing_id,
    mojo::PendingAssociatedReceiver<media::mojom::MediaPlayerHost>
        player_receiver) {
  auto it = media_player_hosts_.find(frame_routing_id);
  if (it == media_player_hosts_.end()) {
    it = media_player_hosts_
             .try_emplace(
                 frame_routing_id,
                 std::make_unique<MediaPlayerHostImpl>(frame_routing_id, this))
             .first;
  }
  it->second->AddMediaPlayerHostReceiver(std::move(player_receiver));
}

void MediaWebContentsObserver::OnMediaPlayerAdded(
    mojo::PendingAssociatedRemote<media::mojom::MediaPlayer> player_remote,
    mojo::PendingAssociatedReceiver<media::mojom::MediaPlayerObserver>
        media_player_observer,
    MediaPlayerId player_id) {
  if (!RenderFrameHost::FromID(player_id.frame_routing_id) ||
      media_player_remotes_.contains(player_id) ||
      media_player_observer_hosts_.contains(player_id)) {
    // If you see this, it's likely due to https://crbug.com/1392441
    mojo::ReportBadMessage("Bad MediaPlayer request.");
    return;
  }

  auto remote_it = media_player_remotes_.try_emplace(player_id);
  remote_it.first->second.Bind(std::move(player_remote));
  remote_it.first->second.set_disconnect_handler(base::BindOnce(
      [](MediaWebContentsObserver* observer, const MediaPlayerId& player_id) {
        // This cannot just use `erase()`, because some observers call back
        // into `this` and query player info when a PlayerInfo is destroyed
        // (!!).  Re-entering a container while erasing an entry is generally
        // not very safe or robust.
        if (auto it = observer->player_info_map_.find(player_id);
            it != observer->player_info_map_.end()) {
          // Instead, remove entries in a multi-step process:
          // 1. Move ownership of the PlayerInfo out of the map.
          // 2. Erase the entry from the map.
          // 3. Destroy the PlayerInfo (by letting it go out of scope).
          //    Because the entry is already gone from the map, GetPlayerInfo()
          //    will return null instead of trying to compare keys that are
          //    potentially destroyed.
          auto player_info = std::move(it->second);
          observer->player_info_map_.erase(it);
        }
        observer->media_player_remotes_.erase(player_id);
        observer->session_controllers_manager_->OnEnd(player_id);
        if (observer->fullscreen_player_ &&
            *observer->fullscreen_player_ == player_id) {
          observer->fullscreen_player_.reset();
        }
        observer->web_contents_impl()->MediaDestroyed(player_id);
      },
      base::Unretained(this), player_id));

  auto observer_it = media_player_observer_hosts_.try_emplace(
      player_id,
      std::make_unique<MediaPlayerObserverHostImpl>(player_id, this));
  observer_it.first->second->BindMediaPlayerObserverReceiver(
      std::move(media_player_observer));
}

void MediaWebContentsObserver::SuspendAllMediaPlayers() {
  for (auto& media_player_remote : media_player_remotes_) {
    media_player_remote.second->SuspendForFrameClosed();
  }
}

void MediaWebContentsObserver::OnExperimentStateChanged(MediaPlayerId id,
                                                        bool is_starting) {
  use_after_free_checker_.check();

  GetMediaPlayerRemote(id)->SetPowerExperimentState(is_starting);
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
