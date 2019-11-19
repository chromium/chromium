// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/picture_in_picture/picture_in_picture_window_controller_impl.h"

#include <set>

#include "components/viz/common/surfaces/surface_id.h"
#include "content/browser/media/media_web_contents_observer.h"
#include "content/browser/media/session/media_session_impl.h"
#include "content/browser/picture_in_picture/picture_in_picture_session.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/common/media/media_player_delegate_messages.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/overlay_window.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/common/content_client.h"

namespace content {

// static
PictureInPictureWindowController*
PictureInPictureWindowController::GetOrCreateForWebContents(
    WebContents* web_contents) {
  return PictureInPictureWindowControllerImpl::GetOrCreateForWebContents(
      web_contents);
}

// static
PictureInPictureWindowControllerImpl*
PictureInPictureWindowControllerImpl::GetOrCreateForWebContents(
    WebContents* web_contents) {
  DCHECK(web_contents);

  // This is a no-op if the controller already exists.
  CreateForWebContents(web_contents);
  return FromWebContents(web_contents);
}

PictureInPictureWindowControllerImpl::~PictureInPictureWindowControllerImpl() {
  if (window_)
    window_->Close();

  // If the initiator WebContents is being destroyed, there is no need to put
  // the video's media player in a post-Picture-in-Picture mode. In fact, some
  // things, such as the MediaWebContentsObserver, may already been torn down.
  if (initiator_->IsBeingDestroyed())
    return;

  initiator_->SetHasPictureInPictureVideo(false);
  OnLeavingPictureInPicture(true /* should_pause_video */);
}

PictureInPictureWindowControllerImpl::PictureInPictureWindowControllerImpl(
    WebContents* initiator)
    : WebContentsObserver(initiator),
      initiator_(static_cast<WebContentsImpl* const>(initiator)) {
  DCHECK(initiator_);

  media_web_contents_observer_ = initiator_->media_web_contents_observer();

  EnsureWindow();
  DCHECK(window_) << "Picture in Picture requires a valid window.";
}

void PictureInPictureWindowControllerImpl::Show() {
  DCHECK(window_);
  DCHECK(surface_id_.is_valid());

  MediaSessionImpl* media_session = MediaSessionImpl::Get(initiator_);
  media_session_action_play_handled_ = media_session->ShouldRouteAction(
      media_session::mojom::MediaSessionAction::kPlay);
  media_session_action_pause_handled_ = media_session->ShouldRouteAction(
      media_session::mojom::MediaSessionAction::kPause);
  media_session_action_skip_ad_handled_ = media_session->ShouldRouteAction(
      media_session::mojom::MediaSessionAction::kSkipAd);
  media_session_action_next_track_handled_ = media_session->ShouldRouteAction(
      media_session::mojom::MediaSessionAction::kNextTrack);
  media_session_action_previous_track_handled_ =
      media_session->ShouldRouteAction(
          media_session::mojom::MediaSessionAction::kPreviousTrack);

  UpdatePlayPauseButtonVisibility();
  window_->SetSkipAdButtonVisibility(media_session_action_skip_ad_handled_);
  window_->SetNextTrackButtonVisibility(
      media_session_action_next_track_handled_);
  window_->SetPreviousTrackButtonVisibility(
      media_session_action_previous_track_handled_);
  window_->ShowInactive();
  initiator_->SetHasPictureInPictureVideo(true);
}

void PictureInPictureWindowControllerImpl::Close(bool should_pause_video) {
  if (!window_ || !window_->IsVisible())
    return;

  window_->Hide();
  CloseInternal(should_pause_video);
}

void PictureInPictureWindowControllerImpl::CloseAndFocusInitiator() {
  Close(false /* should_pause_video */);
  initiator_->Activate();
}

void PictureInPictureWindowControllerImpl::OnWindowDestroyed() {
  window_ = nullptr;
  CloseInternal(true /* should_pause_video */);
}

void PictureInPictureWindowControllerImpl::EmbedSurface(
    const viz::SurfaceId& surface_id,
    const gfx::Size& natural_size) {
  EnsureWindow();
  DCHECK(window_);

  DCHECK(surface_id.is_valid());

  surface_id_ = surface_id;

  // Update the media player id in step with the video surface id. If the
  // surface id was updated for the same video, this is a no-op. This could
  // be updated for a different video if another media player on the same
  // |initiator_| enters Picture-in-Picture mode.
  UpdateMediaPlayerId();

  window_->UpdateVideoSize(natural_size);
  window_->SetSurfaceId(surface_id_);
}

OverlayWindow* PictureInPictureWindowControllerImpl::GetWindowForTesting() {
  return window_.get();
}

void PictureInPictureWindowControllerImpl::UpdateLayerBounds() {
  if (media_player_id_.has_value() && active_session_ && window_ &&
      window_->IsVisible()) {
    active_session_->NotifyWindowResized(window_->GetBounds().size());
  }
}

bool PictureInPictureWindowControllerImpl::IsPlayerActive() {
  if (!media_player_id_.has_value())
    media_player_id_ =
        active_session_ ? active_session_->player_id() : base::nullopt;

  // At creation time, the player id may not be set.
  if (!media_player_id_.has_value())
    return false;

  return media_web_contents_observer_->IsPlayerActive(*media_player_id_);
}

WebContents* PictureInPictureWindowControllerImpl::GetInitiatorWebContents() {
  return initiator_;
}

void PictureInPictureWindowControllerImpl::UpdatePlaybackState(
    bool is_playing,
    bool reached_end_of_stream) {
  if (!window_)
    return;

  if (reached_end_of_stream) {
    window_->SetPlaybackState(OverlayWindow::PlaybackState::kEndOfVideo);
    return;
  }

  DCHECK(media_player_id_.has_value());

  window_->SetPlaybackState(is_playing ? OverlayWindow::PlaybackState::kPlaying
                                       : OverlayWindow::PlaybackState::kPaused);
}

bool PictureInPictureWindowControllerImpl::TogglePlayPause() {
  DCHECK(window_);

  if (IsPlayerActive()) {
    if (media_session_action_pause_handled_) {
      MediaSessionImpl::Get(initiator_)
          ->Suspend(MediaSession::SuspendType::kUI);
      return true /* still playing */;
    }

    media_player_id_->render_frame_host->Send(new MediaPlayerDelegateMsg_Pause(
        media_player_id_->render_frame_host->GetRoutingID(),
        media_player_id_->delegate_id, false /* triggered_by_user */));
    return false /* paused */;
  }

  if (media_session_action_play_handled_) {
    MediaSessionImpl::Get(initiator_)->Resume(MediaSession::SuspendType::kUI);
    return false /* still paused */;
  }

  media_player_id_->render_frame_host->Send(new MediaPlayerDelegateMsg_Play(
      media_player_id_->render_frame_host->GetRoutingID(),
      media_player_id_->delegate_id));
  return true /* playing */;
}

void PictureInPictureWindowControllerImpl::UpdateMediaPlayerId() {
  media_player_id_ =
      active_session_ ? active_session_->player_id() : base::nullopt;
  UpdatePlaybackState(IsPlayerActive(), !media_player_id_.has_value());
}

void PictureInPictureWindowControllerImpl::SetActiveSession(
    PictureInPictureSession* session) {
  if (active_session_ == session)
    return;

  if (active_session_)
    active_session_->Shutdown();

  active_session_ = session;
}

void PictureInPictureWindowControllerImpl::SetAlwaysHidePlayPauseButton(
    bool is_visible) {
  always_hide_play_pause_button_ = is_visible;
  UpdatePlayPauseButtonVisibility();
}

void PictureInPictureWindowControllerImpl::SkipAd() {
  if (media_session_action_skip_ad_handled_)
    MediaSession::Get(initiator_)->SkipAd();
}

void PictureInPictureWindowControllerImpl::NextTrack() {
  if (media_session_action_next_track_handled_)
    MediaSession::Get(initiator_)->NextTrack();
}

void PictureInPictureWindowControllerImpl::PreviousTrack() {
  if (media_session_action_previous_track_handled_)
    MediaSession::Get(initiator_)->PreviousTrack();
}

void PictureInPictureWindowControllerImpl::MediaSessionActionsChanged(
    const std::set<media_session::mojom::MediaSessionAction>& actions) {
  // TODO(crbug.com/919842): Currently, the first Media Session to be created
  // (independently of the frame) will be used. This means, we could show a
  // Skip Ad button for a PiP video from another frame. Ideally, we should have
  // a Media Session per frame, not per tab. This is not implemented yet.

  media_session_action_pause_handled_ =
      actions.find(media_session::mojom::MediaSessionAction::kPause) !=
      actions.end();
  media_session_action_play_handled_ =
      actions.find(media_session::mojom::MediaSessionAction::kPlay) !=
      actions.end();
  media_session_action_skip_ad_handled_ =
      actions.find(media_session::mojom::MediaSessionAction::kSkipAd) !=
      actions.end();
  media_session_action_next_track_handled_ =
      actions.find(media_session::mojom::MediaSessionAction::kNextTrack) !=
      actions.end();
  media_session_action_previous_track_handled_ =
      actions.find(media_session::mojom::MediaSessionAction::kPreviousTrack) !=
      actions.end();

  if (!window_)
    return;

  UpdatePlayPauseButtonVisibility();
  window_->SetSkipAdButtonVisibility(media_session_action_skip_ad_handled_);
  window_->SetNextTrackButtonVisibility(
      media_session_action_next_track_handled_);
  window_->SetPreviousTrackButtonVisibility(
      media_session_action_previous_track_handled_);
}

gfx::Size PictureInPictureWindowControllerImpl::GetSize() {
  return window_->GetBounds().size();
}

void PictureInPictureWindowControllerImpl::MediaStartedPlaying(
    const MediaPlayerInfo&,
    const MediaPlayerId& media_player_id) {
  if (initiator_->IsBeingDestroyed())
    return;

  if (media_player_id_ != media_player_id)
    return;

  UpdatePlaybackState(true /* is_playing */, false /* reached_end_of_stream */);
}

void PictureInPictureWindowControllerImpl::MediaStoppedPlaying(
    const MediaPlayerInfo&,
    const MediaPlayerId& media_player_id,
    WebContentsObserver::MediaStoppedReason reason) {
  if (initiator_->IsBeingDestroyed())
    return;

  if (media_player_id_ != media_player_id)
    return;

  UpdatePlaybackState(
      false /* is_playing */,
      reason == WebContentsObserver::MediaStoppedReason::kReachedEndOfStream);
}

void PictureInPictureWindowControllerImpl::OnLeavingPictureInPicture(
    bool should_pause_video) {
  if (IsPlayerActive() && should_pause_video) {
    // Pause the current video so there is only one video playing at a time.
    media_player_id_->render_frame_host->Send(new MediaPlayerDelegateMsg_Pause(
        media_player_id_->render_frame_host->GetRoutingID(),
        media_player_id_->delegate_id, false /* triggered_by_user */));
  }

  if (media_player_id_.has_value()) {
    active_session_->Shutdown();
    active_session_ = nullptr;
    media_player_id_.reset();
  }
}

void PictureInPictureWindowControllerImpl::CloseInternal(
    bool should_pause_video) {
  if (initiator_->IsBeingDestroyed())
    return;

  initiator_->SetHasPictureInPictureVideo(false);
  OnLeavingPictureInPicture(should_pause_video);
  surface_id_ = viz::SurfaceId();
}

void PictureInPictureWindowControllerImpl::EnsureWindow() {
  if (window_)
    return;

  window_ =
      GetContentClient()->browser()->CreateWindowForPictureInPicture(this);
}

void PictureInPictureWindowControllerImpl::UpdatePlayPauseButtonVisibility() {
  if (!window_)
    return;

  window_->SetAlwaysHidePlayPauseButton((media_session_action_pause_handled_ &&
                                         media_session_action_play_handled_) ||
                                        always_hide_play_pause_button_);
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(PictureInPictureWindowControllerImpl)

}  // namespace content
