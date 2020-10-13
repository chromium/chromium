// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/media_web_contents_observer.h"

#include <memory>
#include <tuple>

#include "base/bind.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "build/build_config.h"
#include "content/browser/media/audible_metrics.h"
#include "content/browser/media/audio_stream_monitor.h"
#include "content/browser/media/media_devices_util.h"
#include "content/browser/renderer_host/media/media_stream_manager.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/common/media/media_player_delegate_messages.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "ipc/ipc_message_macros.h"
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
    RenderFrameHost* render_frame_host,
    int delegate_id,
    const base::Optional<std::string>& raw_device_id) {
  if (!raw_device_id)
    return;

  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&MediaWebContentsObserver::OnReceivedTranslatedDeviceId,
                     std::move(observer), render_frame_host, delegate_id,
                     raw_device_id.value()));
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

MediaWebContentsObserver::MediaWebContentsObserver(WebContents* web_contents)
    : WebContentsObserver(web_contents),
      audible_metrics_(GetAudibleMetrics()),
      session_controllers_manager_(web_contents),
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
}

void MediaWebContentsObserver::RenderFrameDeleted(
    RenderFrameHost* render_frame_host) {
  use_after_free_checker_.check();
  base::EraseIf(
      player_info_map_,
      [render_frame_host](const PlayerInfoMap::value_type& id_and_player_info) {
        return render_frame_host == id_and_player_info.first.render_frame_host;
      });

  session_controllers_manager_.RenderFrameDeleted(render_frame_host);

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

bool MediaWebContentsObserver::OnMessageReceived(
    const IPC::Message& msg,
    RenderFrameHost* render_frame_host) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP_WITH_PARAM(MediaWebContentsObserver, msg,
                                   render_frame_host)
    IPC_MESSAGE_HANDLER(MediaPlayerDelegateHostMsg_OnMediaDestroyed,
                        OnMediaDestroyed)
    IPC_MESSAGE_HANDLER(MediaPlayerDelegateHostMsg_OnMediaPaused, OnMediaPaused)
    IPC_MESSAGE_HANDLER(MediaPlayerDelegateHostMsg_OnMediaMetadataChanged,
                        OnMediaMetadataChanged)
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
    IPC_MESSAGE_HANDLER(
        MediaPlayerDelegateHostMsg_OnPictureInPictureAvailabilityChanged,
        OnPictureInPictureAvailabilityChanged)
    IPC_MESSAGE_HANDLER(MediaPlayerDelegateHostMsg_OnAudioOutputSinkChanged,
                        OnAudioOutputSinkChanged);
    IPC_MESSAGE_HANDLER(
        MediaPlayerDelegateHostMsg_OnAudioOutputSinkChangingDisabled,
        OnAudioOutputSinkChangingDisabled)
    IPC_MESSAGE_HANDLER(MediaPlayerDelegateHostMsg_OnBufferUnderflow,
                        OnBufferUnderflow)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void MediaWebContentsObserver::MediaPictureInPictureChanged(
    bool is_picture_in_picture) {
  session_controllers_manager_.PictureInPictureStateChanged(
      is_picture_in_picture);
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
  if (const PlayerInfo* player_info = GetPlayerInfo(player_id))
    return player_info->is_playing();

  return false;
}

MediaWebContentsObserver::PlayerInfo* MediaWebContentsObserver::GetPlayerInfo(
    const MediaPlayerId& id) const {
  const auto it = player_info_map_.find(id);
  return it != player_info_map_.end() ? it->second.get() : nullptr;
}

void MediaWebContentsObserver::OnMediaDestroyed(
    RenderFrameHost* render_frame_host,
    int delegate_id) {
  // TODO(liberato): Should we skip power manager notifications in this case?
  const MediaPlayerId player_id(render_frame_host, delegate_id);
  player_info_map_.erase(player_id);
  session_controllers_manager_.OnEnd(player_id);
}

void MediaWebContentsObserver::OnMediaPaused(RenderFrameHost* render_frame_host,
                                             int delegate_id,
                                             bool reached_end_of_stream) {
  const MediaPlayerId player_id(render_frame_host, delegate_id);
  PlayerInfo* player_info = GetPlayerInfo(player_id);
  if (!player_info || !player_info->is_playing())
    return;

  player_info->SetIsStopped(reached_end_of_stream);

  session_controllers_manager_.OnPause(player_id, reached_end_of_stream);
}

void MediaWebContentsObserver::OnMediaMetadataChanged(
    RenderFrameHost* render_frame_host,
    int delegate_id,
    bool has_audio,
    bool has_video,
    media::MediaContentType media_content_type) {
  const MediaPlayerId player_id(render_frame_host, delegate_id);

  PlayerInfo* player_info = GetPlayerInfo(player_id);
  if (!player_info) {
    PlayerInfoMap::iterator it;
    std::tie(it, std::ignore) = player_info_map_.emplace(
        player_id, std::make_unique<PlayerInfo>(player_id, this));
    player_info = it->second.get();
  }

  player_info->set_has_audio(has_audio);
  player_info->set_has_video(has_video);

  session_controllers_manager_.OnMetadata(player_id, has_audio, has_video,
                                          media_content_type);
}

void MediaWebContentsObserver::OnMediaPlaying(
    RenderFrameHost* render_frame_host,
    int delegate_id) {
  const MediaPlayerId player_id(render_frame_host, delegate_id);

  PlayerInfo* player_info = GetPlayerInfo(player_id);
  if (!player_info)
    return;

  if (!session_controllers_manager_.RequestPlay(player_id)) {
    // Return early to avoid spamming WebContents with playing/stopped
    // notifications.  If RequestPlay() fails, media session will send a pause
    // signal right away.
    return;
  }

  if (!player_info->is_playing())
    player_info->SetIsPlaying();
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

void MediaWebContentsObserver::OnPictureInPictureAvailabilityChanged(
    RenderFrameHost* render_frame_host,
    int delegate_id,
    bool available) {
  session_controllers_manager_.OnPictureInPictureAvailabilityChanged(
      MediaPlayerId(render_frame_host, delegate_id), available);
}

void MediaWebContentsObserver::OnAudioOutputSinkChanged(
    RenderFrameHost* render_frame_host,
    int delegate_id,
    std::string hashed_device_id) {
  auto salt_and_origin = content::GetMediaDeviceSaltAndOrigin(
      render_frame_host->GetProcess()->GetID(),
      render_frame_host->GetRoutingID());

  auto callback_on_io_thread = base::BindOnce(
      [](const std::string& salt, const url::Origin& origin,
         const std::string& hashed_device_id,
         base::OnceCallback<void(const base::Optional<std::string>&)>
             callback) {
        MediaStreamManager::GetMediaDeviceIDForHMAC(
            blink::MediaDeviceType::MEDIA_DEVICE_TYPE_AUDIO_OUTPUT, salt,
            std::move(origin), hashed_device_id,
            base::SequencedTaskRunnerHandle::Get(), std::move(callback));
      },
      salt_and_origin.device_id_salt, std::move(salt_and_origin.origin),
      hashed_device_id,
      base::BindOnce(&OnAudioOutputDeviceIdTranslated,
                     weak_ptr_factory_.GetWeakPtr(), render_frame_host,
                     delegate_id));

  content::GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE, std::move(callback_on_io_thread));
}

void MediaWebContentsObserver::OnReceivedTranslatedDeviceId(
    RenderFrameHost* render_frame_host,
    int delegate_id,
    const std::string& raw_device_id) {
  session_controllers_manager_.OnAudioOutputSinkChanged(
      MediaPlayerId(render_frame_host, delegate_id), raw_device_id);
}

void MediaWebContentsObserver::OnAudioOutputSinkChangingDisabled(
    RenderFrameHost* render_frame_host,
    int delegate_id) {
  session_controllers_manager_.OnAudioOutputSinkChangingDisabled(
      MediaPlayerId(render_frame_host, delegate_id));
}

void MediaWebContentsObserver::OnBufferUnderflow(
    RenderFrameHost* render_frame_host,
    int delegate_id) {
  const MediaPlayerId id(render_frame_host, delegate_id);
  web_contents_impl()->MediaBufferUnderflow(id);
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
