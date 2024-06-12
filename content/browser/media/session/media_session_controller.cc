// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/session/media_session_controller.h"

#include "content/browser/media/media_devices_util.h"
#include "content/browser/media/media_web_contents_observer.h"
#include "content/browser/media/session/media_session_impl.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/media_device_id.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "media/base/media_content_type.h"

namespace content {

int MediaSessionController::player_count_ = 0;

MediaSessionController::MediaSessionController(const MediaPlayerId& id,
                                               WebContentsImpl* web_contents)
    : id_(id),
      web_contents_(web_contents),
      media_session_(MediaSessionImpl::Get(web_contents)) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

MediaSessionController::~MediaSessionController() {
  media_session_->RemovePlayer(this, player_id_);
}

void MediaSessionController::SetMetadata(
    bool has_audio,
    bool has_video,
    media::MediaContentType media_content_type) {
  has_audio_ = has_audio;
  has_video_ = has_video;
  media_content_type_ = media_content_type;
  AddOrRemovePlayer();
}

bool MediaSessionController::OnPlaybackStarted() {
  is_paused_ = false;
  is_playback_in_progress_ = true;
  return AddOrRemovePlayer();
}

void MediaSessionController::OnSuspend(int player_id) {
  DCHECK_EQ(player_id_, player_id);
  // TODO(crbug.com/40623496): Set triggered_by_user to true ONLY if that action
  // was actually triggered by user as this will activate the frame.
  web_contents_->media_web_contents_observer()
      ->GetMediaPlayerRemote(id_)
      ->RequestPause(/*triggered_by_user=*/true);
}

void MediaSessionController::OnResume(int player_id) {
  DCHECK_EQ(player_id_, player_id);
  web_contents_->media_web_contents_observer()
      ->GetMediaPlayerRemote(id_)
      ->RequestPlay();
}

void MediaSessionController::OnSeekForward(int player_id,
                                           base::TimeDelta seek_time) {
  DCHECK_EQ(player_id_, player_id);
  web_contents_->media_web_contents_observer()
      ->GetMediaPlayerRemote(id_)
      ->RequestSeekForward(seek_time);
}

void MediaSessionController::OnSeekBackward(int player_id,
                                            base::TimeDelta seek_time) {
  DCHECK_EQ(player_id_, player_id);
  web_contents_->media_web_contents_observer()
      ->GetMediaPlayerRemote(id_)
      ->RequestSeekBackward(seek_time);
}

void MediaSessionController::OnSeekTo(int player_id,
                                      base::TimeDelta seek_time) {
  DCHECK_EQ(player_id_, player_id);
  web_contents_->media_web_contents_observer()
      ->GetMediaPlayerRemote(id_)
      ->RequestSeekTo(seek_time);
}

void MediaSessionController::OnSetVolumeMultiplier(int player_id,
                                                   double volume_multiplier) {
  DCHECK_EQ(player_id_, player_id);

  auto* observer = web_contents_->media_web_contents_observer();
  // The MediaPlayer mojo interface may not be available in tests.
  if (!observer->IsMediaPlayerRemoteAvailable(id_))
    return;
  observer->GetMediaPlayerRemote(id_)->SetVolumeMultiplier(volume_multiplier);
}

void MediaSessionController::OnEnterPictureInPicture(int player_id) {
  DCHECK_EQ(player_id_, player_id);

  web_contents_->media_web_contents_observer()
      ->GetMediaPlayerRemote(id_)
      ->RequestEnterPictureInPicture();
}

void MediaSessionController::OnSetAudioSinkId(
    int player_id,
    const std::string& raw_device_id) {
  DCHECK_EQ(player_id_, player_id);

  auto* render_frame_host = RenderFrameHost::FromID(id_.frame_routing_id);
  if (!render_frame_host)
    return;

  GetHMACFromRawDeviceId(
      render_frame_host->GetGlobalId(), raw_device_id,
      base::BindOnce(&MediaSessionController::OnHashedSinkIdReceived,
                     weak_factory_.GetWeakPtr()));
}

void MediaSessionController::OnHashedSinkIdReceived(
    const std::string& hashed_sink_id) {
  // Grant the renderer the permission to use this audio output device.
  auto* render_frame_host_impl =
      RenderFrameHostImpl::FromID(id_.frame_routing_id);
  if (!render_frame_host_impl) {
    return;
  }
  render_frame_host_impl->SetAudioOutputDeviceIdForGlobalMediaControls(
      hashed_sink_id);

  web_contents_->media_web_contents_observer()
      ->GetMediaPlayerRemote(id_)
      ->SetAudioSinkId(hashed_sink_id);
}

void MediaSessionController::OnSetMute(int player_id, bool mute) {
  DCHECK_EQ(player_id_, player_id);

  web_contents_->media_web_contents_observer()
      ->GetMediaPlayerRemote(id_)
      ->RequestMute(mute);
}

void MediaSessionController::OnRequestMediaRemoting(int player_id) {
  DCHECK_EQ(player_id_, player_id);

  // Media Remoting can't start if the media is paused. So we should start
  // playing before requesting Media Remoting.
  if (is_paused_) {
    web_contents_->media_web_contents_observer()
        ->GetMediaPlayerRemote(id_)
        ->RequestPlay();
  }
  web_contents_->media_web_contents_observer()
      ->GetMediaPlayerRemote(id_)
      ->RequestMediaRemoting();
}

void MediaSessionController::OnRequestVisibility(
    int player_id,
    RequestVisibilityCallback request_visibility_callback) {
  DCHECK_EQ(player_id_, player_id);
  web_contents_->media_web_contents_observer()
      ->GetMediaPlayerRemote(id_)
      ->RequestVisibility(std::move(request_visibility_callback));
}

RenderFrameHost* MediaSessionController::render_frame_host() const {
  return RenderFrameHost::FromID(id_.frame_routing_id);
}

std::optional<media_session::MediaPosition> MediaSessionController::GetPosition(
    int player_id) const {
  DCHECK_EQ(player_id_, player_id);
  return position_;
}

bool MediaSessionController::IsPictureInPictureAvailable(int player_id) const {
  DCHECK_EQ(player_id_, player_id);
  return is_picture_in_picture_available_;
}

bool MediaSessionController::HasSufficientlyVisibleVideo(int player_id) const {
  DCHECK_EQ(player_id_, player_id);
  return has_sufficiently_visible_video_;
}

void MediaSessionController::OnPlaybackPaused(bool reached_end_of_stream) {
  is_paused_ = true;

  if (reached_end_of_stream) {
    is_playback_in_progress_ = false;
    AddOrRemovePlayer();
  }

  // We check for suspension here since the renderer may issue its own pause
  // in response to or while a pause from the browser is in flight.
  if (media_session_->IsActive())
    media_session_->OnPlayerPaused(this, player_id_);
}

void MediaSessionController::PictureInPictureStateChanged(
    bool is_picture_in_picture) {
  AddOrRemovePlayer();
}

void MediaSessionController::WebContentsMutedStateChanged(bool muted) {
  AddOrRemovePlayer();
}

void MediaSessionController::OnMediaPositionStateChanged(
    const media_session::MediaPosition& position) {
  position_ = position;
  media_session_->RebuildAndNotifyMediaPositionChanged();
}

void MediaSessionController::OnMediaMutedStatusChanged(bool mute) {
  media_session_->OnMediaMutedStatusChanged(mute);
}

void MediaSessionController::OnPictureInPictureAvailabilityChanged(
    bool available) {
  is_picture_in_picture_available_ = available;
  media_session_->OnPictureInPictureAvailabilityChanged();
}

void MediaSessionController::OnAudioOutputSinkChanged(
    const std::string& raw_device_id) {
  audio_output_sink_id_ = raw_device_id;
  media_session_->OnAudioOutputSinkIdChanged();
}

void MediaSessionController::OnAudioOutputSinkChangingDisabled() {
  supports_audio_output_device_switching_ = false;
  media_session_->OnAudioOutputSinkChangingDisabled();
}

void MediaSessionController::OnRemotePlaybackMetadataChanged(
    media_session::mojom::RemotePlaybackMetadataPtr metadata) {
  media_session_->SetRemotePlaybackMetadata(std::move(metadata));
  AddOrRemovePlayer();
}

void MediaSessionController::OnVideoVisibilityChanged(
    bool meets_visibility_threshold) {
  has_sufficiently_visible_video_ = meets_visibility_threshold;
  media_session_->OnVideoVisibilityChanged();
}

bool MediaSessionController::IsMediaSessionNeeded() const {
  if (web_contents_->HasPictureInPictureVideo())
    return true;

  if (!is_playback_in_progress_)
    return false;

  // If the media content has an associated Remote Playback session started, we
  // should request audio focus regardless of whether the tab is muted.
  media_session::mojom::MediaSessionInfoPtr session_info =
      media_session_->GetMediaSessionInfoSync();
  if (session_info && session_info->remote_playback_metadata &&
      session_info->remote_playback_metadata->remote_playback_started) {
    return true;
  }

  // We want to make sure we do not request audio focus on a muted tab as it
  // would break user expectations by pausing/ducking other playbacks.
  return has_audio_ && !web_contents_->IsAudioMuted();
}

bool MediaSessionController::AddOrRemovePlayer() {
  const bool needs_session = IsMediaSessionNeeded();

  if (needs_session) {
    // Attempt to add a session even if we already have one.  MediaSession
    // expects AddPlayer() to be called after OnPlaybackPaused() to reactivate
    // the session.
    if (!media_session_->AddPlayer(this, player_id_)) {
      // If a session can't be created, force a pause immediately.
      OnSuspend(player_id_);
      return false;
    }

    // Need to synchronise paused/playing state in case we're adding the player
    // because of entering Picture-In-Picture.
    if (is_paused_)
      media_session_->OnPlayerPaused(this, player_id_);

    return true;
  }

  media_session_->RemovePlayer(this, player_id_);
  return true;
}

bool MediaSessionController::HasAudio(int player_id) const {
  DCHECK_EQ(player_id_, player_id);
  return has_audio_;
}

bool MediaSessionController::HasVideo(int player_id) const {
  DCHECK_EQ(player_id_, player_id);
  return has_video_;
}

bool MediaSessionController::IsPaused(int player_id) const {
  DCHECK_EQ(player_id_, player_id);
  return is_paused_;
}

std::string MediaSessionController::GetAudioOutputSinkId(int player_id) const {
  DCHECK_EQ(player_id_, player_id);
  return audio_output_sink_id_;
}

bool MediaSessionController::SupportsAudioOutputDeviceSwitching(
    int player_id) const {
  DCHECK_EQ(player_id_, player_id);
  return supports_audio_output_device_switching_;
}

media::MediaContentType MediaSessionController::GetMediaContentType() const {
  return media_content_type_;
}

}  // namespace content
