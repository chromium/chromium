// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/media_web_contents_observer.h"

#include <memory>
#include <tuple>

#include "base/bind.h"
#include "base/debug/crash_logging.h"
#include "base/debug/dump_without_crashing.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "build/build_config.h"
#include "content/browser/media/audible_metrics.h"
#include "content/browser/media/audio_stream_monitor.h"
#include "content/browser/media/media_devices_util.h"
#include "content/browser/renderer_host/media/media_stream_manager.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/common/media/media_player_delegate_messages.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "ipc/ipc_message_macros.h"
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

#if defined(OS_ANDROID)
static void SuspendAllMediaPlayersInRenderFrame(
    RenderFrameHost* render_frame_host) {
  render_frame_host->Send(new MediaPlayerDelegateMsg_SuspendAllMediaPlayers(
      render_frame_host->GetRoutingID()));
}
#endif  // defined(OS_ANDROID)

static void OnAudioOutputDeviceIdTranslated(
    base::WeakPtr<MediaWebContentsObserver> observer,
    const MediaPlayerId& player_id,
    const base::Optional<std::string>& raw_device_id) {
  if (!raw_device_id)
    return;

  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&MediaWebContentsObserver::OnReceivedTranslatedDeviceId,
                     std::move(observer), player_id, raw_device_id.value()));
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

 private:
  void NotifyPlayerStarted() {
    observer_->web_contents_impl()->MediaStartedPlaying(
        WebContentsObserver::MediaPlayerInfo(has_video_, has_audio_), id_);

    if (observer_->power_experiment_manager_) {
      // Bind the callback to a WeakPtr for the frame, so that we won't try to
      // notify the frame after it's been destroyed.
      observer_->power_experiment_manager_->PlayerStarted(
          id_, base::BindRepeating(
                   &MediaWebContentsObserver::OnExperimentStateChanged,
                   observer_->GetWeakPtrForFrame(id_.render_frame_host), id_));
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
  MediaWebContentsObserver* const observer_;

  bool has_audio_ = false;
  bool has_video_ = false;
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
}

void MediaWebContentsObserver::RenderFrameDeleted(
    RenderFrameHost* render_frame_host) {
  use_after_free_checker_.check();
  base::EraseIf(
      player_info_map_,
      [render_frame_host](const PlayerInfoMap::value_type& id_and_player_info) {
        return render_frame_host == id_and_player_info.first.render_frame_host;
      });

  base::EraseIf(media_player_hosts_,
                [render_frame_host](const MediaPlayerHostImplMap::value_type&
                                        media_player_hosts_value_type) {
                  return render_frame_host ==
                         media_player_hosts_value_type.first;
                });

  base::EraseIf(
      media_player_observer_hosts_,
      [render_frame_host](const MediaPlayerObserverHostImplMap::value_type&
                              media_player_observer_hosts_value_type) {
        return render_frame_host ==
               media_player_observer_hosts_value_type.first.render_frame_host;
      });

  base::EraseIf(
      media_player_remotes_,
      [render_frame_host](const MediaPlayerRemotesMap::value_type&
                              media_player_remotes_value_type) {
        return render_frame_host ==
               media_player_remotes_value_type.first.render_frame_host;
      });

  session_controllers_manager_->RenderFrameDeleted(render_frame_host);

  if (fullscreen_player_ &&
      fullscreen_player_->render_frame_host == render_frame_host) {
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
  if (!web_contents()->IsFullscreen() || !fullscreen_player_)
    return false;

  // Check that the player is active.
  if (const PlayerInfo* player_info = GetPlayerInfo(*fullscreen_player_))
    return player_info->is_playing() && player_info->has_video();

  return false;
}

bool MediaWebContentsObserver::IsPictureInPictureAllowedForFullscreenVideo()
    const {
  DCHECK(picture_in_picture_allowed_in_fullscreen_.has_value());

  return *picture_in_picture_allowed_in_fullscreen_;
}

const base::Optional<MediaPlayerId>&
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
  if (const PlayerInfo* player_info = GetPlayerInfo(player_id))
    return player_info->is_playing();

  return false;
}

MediaWebContentsObserver::MediaPlayerHostImpl::MediaPlayerHostImpl(
    RenderFrameHost* render_frame_host,
    MediaWebContentsObserver* media_web_contents_observer)
    : render_frame_host_(render_frame_host),
      media_web_contents_observer_(media_web_contents_observer) {}

MediaWebContentsObserver::MediaPlayerHostImpl::~MediaPlayerHostImpl() = default;

void MediaWebContentsObserver::MediaPlayerHostImpl::BindMediaPlayerHostReceiver(
    mojo::PendingAssociatedReceiver<media::mojom::MediaPlayerHost> receiver) {
  receivers_.Add(this, std::move(receiver));
}

void MediaWebContentsObserver::MediaPlayerHostImpl::OnMediaPlayerAdded(
    mojo::PendingAssociatedRemote<media::mojom::MediaPlayer> media_player,
    int32_t player_id) {
  media_web_contents_observer_->OnMediaPlayerAdded(
      std::move(media_player), MediaPlayerId(render_frame_host_, player_id));
}

MediaWebContentsObserver::MediaPlayerObserverHostImpl::
    MediaPlayerObserverHostImpl(
        const MediaPlayerId& media_player_id,
        MediaWebContentsObserver* media_web_contents_observer)
    : media_player_id_(media_player_id),
      media_web_contents_observer_(media_web_contents_observer) {}

MediaWebContentsObserver::MediaPlayerObserverHostImpl::
    ~MediaPlayerObserverHostImpl() = default;

mojo::PendingAssociatedRemote<media::mojom::MediaPlayerObserver>
MediaWebContentsObserver::MediaPlayerObserverHostImpl::
    BindMediaPlayerObserverReceiverAndPassRemote() {
  media_player_observer_receiver_.reset();
  mojo::PendingAssociatedRemote<media::mojom::MediaPlayerObserver>
      pending_remote =
          media_player_observer_receiver_.BindNewEndpointAndPassRemote();

  // |media_web_contents_observer_| outlives MediaPlayerHostImpl, so it's safe
  // to use base::Unretained().
  media_player_observer_receiver_.set_disconnect_handler(base::BindOnce(
      &MediaWebContentsObserver::OnMediaPlayerObserverDisconnected,
      base::Unretained(media_web_contents_observer_), media_player_id_));

  return pending_remote;
}

void MediaWebContentsObserver::MediaPlayerObserverHostImpl::
    OnMutedStatusChanged(bool muted) {
  media_web_contents_observer_->web_contents_impl()->MediaMutedStatusChanged(
      media_player_id_, muted);
}

void MediaWebContentsObserver::MediaPlayerObserverHostImpl::
    OnMediaMetadataChanged(bool has_audio,
                           bool has_video,
                           media::MediaContentType media_content_type) {
  media_web_contents_observer_->OnMediaMetadataChanged(
      media_player_id_, has_audio, has_video, media_content_type);
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
  media_web_contents_observer_->session_controllers_manager()
      ->OnAudioOutputSinkChanged(media_player_id_, hashed_device_id);
}

void MediaWebContentsObserver::MediaPlayerObserverHostImpl::
    OnAudioOutputSinkChangingDisabled() {
  media_web_contents_observer_->session_controllers_manager()
      ->OnAudioOutputSinkChangingDisabled(media_player_id_);
}

void MediaWebContentsObserver::MediaPlayerObserverHostImpl::
    OnBufferUnderflow() {
  media_web_contents_observer_->web_contents_impl()->MediaBufferUnderflow(
      media_player_id_);
}

void MediaWebContentsObserver::MediaPlayerObserverHostImpl::OnSeek() {
  media_web_contents_observer_->web_contents_impl()->MediaPlayerSeek(
      media_player_id_);
}

void MediaWebContentsObserver::MediaPlayerObserverHostImpl::OnMediaPlaying() {
  PlayerInfo* player_info =
      media_web_contents_observer_->GetPlayerInfo(media_player_id_);
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
}

void MediaWebContentsObserver::MediaPlayerObserverHostImpl::OnMediaPaused(
    bool stream_ended) {
  PlayerInfo* player_info =
      media_web_contents_observer_->GetPlayerInfo(media_player_id_);
  if (!player_info || !player_info->is_playing())
    return;

  player_info->SetIsStopped(stream_ended);

  media_web_contents_observer_->session_controllers_manager()->OnPause(
      media_player_id_, stream_ended);
}

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

void MediaWebContentsObserver::OnAudioOutputSinkChanged(
    const MediaPlayerId& player_id,
    std::string hashed_device_id) {
  auto salt_and_origin = content::GetMediaDeviceSaltAndOrigin(
      player_id.render_frame_host->GetProcess()->GetID(),
      player_id.render_frame_host->GetRoutingID());

  auto callback_on_io_thread = base::BindOnce(
      [](const std::string& salt, const url::Origin& origin,
         const std::string& hashed_device_id,
         base::OnceCallback<void(const base::Optional<std::string>&)>
             callback) {
        MediaStreamManager::GetMediaDeviceIDForHMAC(
            blink::mojom::MediaDeviceType::MEDIA_AUDIO_OUTPUT, salt,
            std::move(origin), hashed_device_id,
            base::SequencedTaskRunnerHandle::Get(), std::move(callback));
      },
      salt_and_origin.device_id_salt, std::move(salt_and_origin.origin),
      hashed_device_id,
      base::BindOnce(&OnAudioOutputDeviceIdTranslated,
                     weak_ptr_factory_.GetWeakPtr(), player_id));

  content::GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE, std::move(callback_on_io_thread));
}

void MediaWebContentsObserver::OnReceivedTranslatedDeviceId(
    const MediaPlayerId& player_id,
    const std::string& raw_device_id) {
  session_controllers_manager_->OnAudioOutputSinkChanged(player_id,
                                                         raw_device_id);
}

media::mojom::MediaPlayer* MediaWebContentsObserver::GetMediaPlayerRemote(
    const MediaPlayerId& player_id) {
  if (media_player_remotes_.contains(player_id)) {
    DCHECK(media_player_remotes_[player_id].is_bound());
    return media_player_remotes_.at(player_id).get();
  }

  return nullptr;
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
  GetAudioWakeLock()->CancelWakeLock();
  has_audio_wake_lock_for_testing_ = false;
}

WebContentsImpl* MediaWebContentsObserver::web_contents_impl() const {
  return static_cast<WebContentsImpl*>(web_contents());
}

void MediaWebContentsObserver::BindMediaPlayerHost(
    RenderFrameHost* host,
    mojo::PendingAssociatedReceiver<media::mojom::MediaPlayerHost>
        player_receiver) {
  if (!media_player_hosts_.contains(host)) {
    media_player_hosts_[host] =
        std::make_unique<MediaPlayerHostImpl>(host, this);
  }

  media_player_hosts_[host]->BindMediaPlayerHostReceiver(
      std::move(player_receiver));
}

void MediaWebContentsObserver::OnMediaPlayerAdded(
    mojo::PendingAssociatedRemote<media::mojom::MediaPlayer> player_remote,
    MediaPlayerId player_id) {
  if (media_player_remotes_.contains(player_id)) {
    // Original remote associated with |player_id| will be overridden. If the
    // original player is still alive, this will break our ability to control
    // it from the browser process. We don't know that the original player is
    // actually still alive.
    // TODO(https://crbug.com/1172882): Determine the root cause of duplication
    // and/or refactor to make ID purely a browser-side concept.
    LOG(ERROR) << __func__ << " Duplicate media player id ("
               << player_id.delegate_id << ")";
  }

  media_player_remotes_[player_id].Bind(std::move(player_remote));
  media_player_remotes_[player_id].set_disconnect_handler(base::BindOnce(
      [](MediaWebContentsObserver* observer, const MediaPlayerId& player_id) {
        observer->player_info_map_.erase(player_id);
        observer->media_player_remotes_.erase(player_id);
        observer->session_controllers_manager_->OnEnd(player_id);
        observer->web_contents_impl()->MediaDestroyed(player_id);
      },
      base::Unretained(this), player_id));

  // Create a new MediaPlayerObserverHostImpl for |player_id|, implementing the
  // media::mojom::MediaPlayerObserver mojo interface, to handle messages sent
  // from the MediaPlayer element in the renderer process.
  if (!media_player_observer_hosts_.contains(player_id)) {
    media_player_observer_hosts_[player_id] =
        std::make_unique<MediaPlayerObserverHostImpl>(player_id, this);
  }
  media_player_remotes_[player_id]->AddMediaPlayerObserver(
      media_player_observer_hosts_[player_id]
          ->BindMediaPlayerObserverReceiverAndPassRemote());
}

#if defined(OS_ANDROID)
void MediaWebContentsObserver::SuspendAllMediaPlayers() {
  web_contents()->ForEachFrame(
      base::BindRepeating(&SuspendAllMediaPlayersInRenderFrame));
}
#endif  // defined(OS_ANDROID)

void MediaWebContentsObserver::OnExperimentStateChanged(MediaPlayerId id,
                                                        bool is_starting) {
  use_after_free_checker_.check();
  id.render_frame_host->Send(
      new MediaPlayerDelegateMsg_NotifyPowerExperimentState(
          id.render_frame_host->GetRoutingID(), id.delegate_id, is_starting));
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
