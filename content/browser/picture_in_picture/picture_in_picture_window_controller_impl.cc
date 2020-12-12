// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/picture_in_picture/picture_in_picture_window_controller_impl.h"

#include <set>
#include <utility>

#include "components/viz/common/surfaces/surface_id.h"
#include "content/browser/media/media_web_contents_observer.h"
#include "content/browser/media/session/media_session_impl.h"
#include "content/browser/picture_in_picture/picture_in_picture_session.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/common/media/media_player_delegate_messages.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/overlay_window.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"  // for PictureInPictureResult
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

PictureInPictureWindowControllerImpl::~PictureInPictureWindowControllerImpl() =
    default;

PictureInPictureWindowControllerImpl::PictureInPictureWindowControllerImpl(
    WebContents* web_contents)
    : WebContentsObserver(web_contents) {
  DCHECK(web_contents);

  EnsureWindow();
  DCHECK(window_) << "Picture in Picture requires a valid window.";
}

void PictureInPictureWindowControllerImpl::Show() {
  DCHECK(window_);
  DCHECK(surface_id_.is_valid());

  MediaSessionImpl* media_session = MediaSessionImpl::Get(web_contents());
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
  GetWebContentsImpl()->SetHasPictureInPictureVideo(true);
}

void PictureInPictureWindowControllerImpl::Close(bool should_pause_video) {
  if (!window_ || !window_->IsVisible())
    return;

  window_->Hide();
  CloseInternal(should_pause_video);
}

void PictureInPictureWindowControllerImpl::CloseAndFocusInitiator() {
  Close(false /* should_pause_video */);
  GetWebContentsImpl()->Activate();
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
  DCHECK(active_session_);
  DCHECK(surface_id.is_valid());

  surface_id_ = surface_id;

  // Update the playback state in step with the video surface id. If the surface
  // id was updated for the same video, this is a no-op. This could be updated
  // for a different video if another media player on the same WebContents
  // enters Picture-in-Picture mode.
  UpdatePlaybackState(IsPlayerActive(), false);

  window_->UpdateVideoSize(natural_size);
  window_->SetSurfaceId(surface_id_);
}

OverlayWindow* PictureInPictureWindowControllerImpl::GetWindowForTesting() {
  return window_.get();
}

void PictureInPictureWindowControllerImpl::UpdateLayerBounds() {
  if (active_session_ && window_ && window_->IsVisible())
    active_session_->NotifyWindowResized(window_->GetBounds().size());
}

bool PictureInPictureWindowControllerImpl::IsPlayerActive() {
  if (!active_session_)
    return false;

  return GetWebContentsImpl()->media_web_contents_observer()->IsPlayerActive(
      active_session_->player_id());
}

WebContents* PictureInPictureWindowControllerImpl::GetWebContents() {
  return web_contents();
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

  DCHECK(active_session_);

  window_->SetPlaybackState(is_playing ? OverlayWindow::PlaybackState::kPlaying
                                       : OverlayWindow::PlaybackState::kPaused);
}

bool PictureInPictureWindowControllerImpl::TogglePlayPause() {
  DCHECK(window_);
  DCHECK(active_session_);

  MediaPlayerId player_id = active_session_->player_id();

  if (IsPlayerActive()) {
    if (media_session_action_pause_handled_) {
      MediaSessionImpl::Get(web_contents())
          ->Suspend(MediaSession::SuspendType::kUI);
      return true /* still playing */;
    }

    player_id.render_frame_host->Send(new MediaPlayerDelegateMsg_Pause(
        player_id.render_frame_host->GetRoutingID(), player_id.delegate_id,
        false /* triggered_by_user */));
    return false /* paused */;
  }

  if (media_session_action_play_handled_) {
    MediaSessionImpl::Get(web_contents())
        ->Resume(MediaSession::SuspendType::kUI);
    return false /* still paused */;
  }

  player_id.render_frame_host->Send(new MediaPlayerDelegateMsg_Play(
      player_id.render_frame_host->GetRoutingID(), player_id.delegate_id));
  return true /* playing */;
}

PictureInPictureResult PictureInPictureWindowControllerImpl::StartSession(
    PictureInPictureServiceImpl* service,
    const MediaPlayerId& player_id,
    const viz::SurfaceId& surface_id,
    const gfx::Size& natural_size,
    bool show_play_pause_button,
    mojo::PendingRemote<blink::mojom::PictureInPictureSessionObserver> observer,
    mojo::PendingRemote<blink::mojom::PictureInPictureSession>* session_remote,
    gfx::Size* window_size) {
  auto result =
      GetWebContentsImpl()->EnterPictureInPicture(surface_id, natural_size);

  // Picture-in-Picture may not be supported by all embedders, so we should only
  // create the session if the EnterPictureInPicture request was successful.
  if (result != PictureInPictureResult::kSuccess)
    return result;

  if (active_session_)
    active_session_->Disconnect();

  active_session_ = std::make_unique<PictureInPictureSession>(
      service, player_id, session_remote->InitWithNewPipeAndPassReceiver(),
      std::move(observer));

  EmbedSurface(surface_id, natural_size);
  SetShowPlayPauseButton(show_play_pause_button);
  Show();

  *window_size = GetSize();
  return result;
}

void PictureInPictureWindowControllerImpl::OnServiceDeleted(
    PictureInPictureServiceImpl* service) {
  if (!active_session_ || active_session_->service() != service)
    return;

  active_session_->Shutdown();
  active_session_ = nullptr;
}

void PictureInPictureWindowControllerImpl::SetShowPlayPauseButton(
    bool show_play_pause_button) {
  always_show_play_pause_button_ = show_play_pause_button;
  UpdatePlayPauseButtonVisibility();
}

void PictureInPictureWindowControllerImpl::SkipAd() {
  if (media_session_action_skip_ad_handled_)
    MediaSession::Get(web_contents())->SkipAd();
}

void PictureInPictureWindowControllerImpl::NextTrack() {
  if (media_session_action_next_track_handled_)
    MediaSession::Get(web_contents())->NextTrack();
}

void PictureInPictureWindowControllerImpl::PreviousTrack() {
  if (media_session_action_previous_track_handled_)
    MediaSession::Get(web_contents())->PreviousTrack();
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
  if (web_contents()->IsBeingDestroyed())
    return;

  if (!active_session_ || active_session_->player_id() != media_player_id)
    return;

  UpdatePlaybackState(true /* is_playing */, false /* reached_end_of_stream */);
}

void PictureInPictureWindowControllerImpl::MediaStoppedPlaying(
    const MediaPlayerInfo&,
    const MediaPlayerId& media_player_id,
    WebContentsObserver::MediaStoppedReason reason) {
  if (web_contents()->IsBeingDestroyed())
    return;

  if (!active_session_ || active_session_->player_id() != media_player_id)
    return;

  UpdatePlaybackState(
      false /* is_playing */,
      reason == WebContentsObserver::MediaStoppedReason::kReachedEndOfStream);
}

void PictureInPictureWindowControllerImpl::WebContentsDestroyed() {
  if (window_)
    window_->Close();
}

void PictureInPictureWindowControllerImpl::OnLeavingPictureInPicture(
    bool should_pause_video) {
  DCHECK(active_session_);

  MediaPlayerId player_id = active_session_->player_id();

  if (IsPlayerActive() && should_pause_video) {
    // Pause the current video so there is only one video playing at a time.
    player_id.render_frame_host->Send(new MediaPlayerDelegateMsg_Pause(
        player_id.render_frame_host->GetRoutingID(), player_id.delegate_id,
        false /* triggered_by_user */));
  }

  active_session_->Shutdown();
  active_session_ = nullptr;
}

void PictureInPictureWindowControllerImpl::CloseInternal(
    bool should_pause_video) {
  // We shouldn't have an empty active_session_ in this case but (at least for
  // there tests), extensions seem to be closing the window before the
  // WebContents is marked as being destroyed. It leads to `CloseInternal()`
  // being called twice. This early check avoids the rest of the code having to
  // be aware of this oddity.
  if (web_contents()->IsBeingDestroyed() || !active_session_)
    return;

  GetWebContentsImpl()->SetHasPictureInPictureVideo(false);
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

  window_->SetPlayPauseButtonVisibility((media_session_action_pause_handled_ &&
                                         media_session_action_play_handled_) ||
                                        always_show_play_pause_button_);
}

WebContentsImpl* PictureInPictureWindowControllerImpl::GetWebContentsImpl() {
  return static_cast<WebContentsImpl*>(web_contents());
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(PictureInPictureWindowControllerImpl)

}  // namespace content
