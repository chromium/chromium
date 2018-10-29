// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/picture_in_picture/picture_in_picture_window_controller_impl.h"

#include "components/viz/common/surfaces/surface_id.h"
#include "content/browser/media/media_web_contents_observer.h"
#include "content/browser/picture_in_picture/overlay_surface_embedder.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/common/media/media_player_delegate_messages.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/overlay_window.h"
#include "content/public/browser/web_contents.h"
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
  OnLeavingPictureInPicture(true /* should_pause_video */,
                            true /* should_reset_pip_player */);
}

PictureInPictureWindowControllerImpl::PictureInPictureWindowControllerImpl(
    WebContents* initiator)
    : initiator_(static_cast<WebContentsImpl* const>(initiator)) {
  DCHECK(initiator_);

  media_web_contents_observer_ = initiator_->media_web_contents_observer();

  EnsureWindow();
  DCHECK(window_) << "Picture in Picture requires a valid window.";
}

gfx::Size PictureInPictureWindowControllerImpl::Show() {
  DCHECK(window_);
  DCHECK(surface_id_.is_valid());

  window_->Show();
  initiator_->SetHasPictureInPictureVideo(true);

  return window_->GetBounds().size();
}

void PictureInPictureWindowControllerImpl::SetPictureInPictureCustomControls(
    const std::vector<blink::PictureInPictureControlInfo>& controls) {
  DCHECK(window_);
  window_->SetPictureInPictureCustomControls(controls);
}

void PictureInPictureWindowControllerImpl::Close(bool should_pause_video,
                                                 bool should_reset_pip_player) {
  if (!window_ || !window_->IsVisible())
    return;

  window_->Hide();
  CloseInternal(should_pause_video, should_reset_pip_player);
}

void PictureInPictureWindowControllerImpl::OnWindowDestroyed() {
  window_ = nullptr;
  embedder_ = nullptr;
  CloseInternal(true /* should_pause_video */,
                true /* should_reset_pip_player */);
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
  media_player_id_ =
      media_web_contents_observer_->GetPictureInPictureVideoMediaPlayerId();
  UpdatePlaybackState(IsPlayerActive(), !media_player_id_.has_value());

  window_->UpdateVideoSize(natural_size);

  if (!embedder_)
    embedder_.reset(new OverlaySurfaceEmbedder(window_.get()));
  embedder_->SetSurfaceId(surface_id_);
}

OverlayWindow* PictureInPictureWindowControllerImpl::GetWindowForTesting() {
  return window_.get();
}

void PictureInPictureWindowControllerImpl::UpdateLayerBounds() {
  if (media_player_id_.has_value() && window_ && window_->IsVisible()) {
    media_web_contents_observer_->OnPictureInPictureWindowResize(
        window_->GetBounds().size());
  }

  if (embedder_)
    embedder_->UpdateLayerBounds();
}

bool PictureInPictureWindowControllerImpl::IsPlayerActive() {
  if (!media_player_id_.has_value()) {
    media_player_id_ =
        media_web_contents_observer_->GetPictureInPictureVideoMediaPlayerId();
  }

  return media_player_id_.has_value() &&
         media_web_contents_observer_->IsPlayerActive(*media_player_id_);
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
    media_player_id_.reset();
    window_->SetPlaybackState(OverlayWindow::PlaybackState::kNoVideo);
    return;
  }

  DCHECK(media_player_id_.has_value());

  window_->SetPlaybackState(is_playing ? OverlayWindow::PlaybackState::kPlaying
                                       : OverlayWindow::PlaybackState::kPaused);
}

bool PictureInPictureWindowControllerImpl::TogglePlayPause() {
  DCHECK(window_ && window_->IsActive());

  if (IsPlayerActive()) {
    media_player_id_->render_frame_host->Send(new MediaPlayerDelegateMsg_Pause(
        media_player_id_->render_frame_host->GetRoutingID(),
        media_player_id_->delegate_id));
    return false;
  }

  media_player_id_->render_frame_host->Send(new MediaPlayerDelegateMsg_Play(
      media_player_id_->render_frame_host->GetRoutingID(),
      media_player_id_->delegate_id));
  return true;
}

void PictureInPictureWindowControllerImpl::CustomControlPressed(
    const std::string& control_id) {
  DCHECK(window_);

  media_player_id_->render_frame_host->Send(
      new MediaPlayerDelegateMsg_ClickPictureInPictureControl(
          media_player_id_->render_frame_host->GetRoutingID(),
          media_player_id_->delegate_id, control_id));
}

void PictureInPictureWindowControllerImpl::SetAlwaysHidePlayPauseButton(
    bool is_visible) {
  if (!window_)
    return;

  window_->SetAlwaysHidePlayPauseButton(is_visible);
}

void PictureInPictureWindowControllerImpl::OnLeavingPictureInPicture(
    bool should_pause_video,
    bool should_reset_pip_player) {
  if (IsPlayerActive() && should_pause_video) {
    // Pause the current video so there is only one video playing at a time.
    media_player_id_->render_frame_host->Send(new MediaPlayerDelegateMsg_Pause(
        media_player_id_->render_frame_host->GetRoutingID(),
        media_player_id_->delegate_id));
  }

  if (media_player_id_.has_value()) {
    media_player_id_->render_frame_host->Send(
        new MediaPlayerDelegateMsg_EndPictureInPictureMode(
            media_player_id_->render_frame_host->GetRoutingID(),
            media_player_id_->delegate_id));
    if (should_reset_pip_player)
      media_web_contents_observer_->ResetPictureInPictureVideoMediaPlayerId();
  }
}

void PictureInPictureWindowControllerImpl::CloseInternal(
    bool should_pause_video,
    bool should_reset_pip_player) {
  if (initiator_->IsBeingDestroyed())
    return;

  surface_id_ = viz::SurfaceId();

  initiator_->SetHasPictureInPictureVideo(false);
  OnLeavingPictureInPicture(should_pause_video, should_reset_pip_player);
}

void PictureInPictureWindowControllerImpl::EnsureWindow() {
  if (window_)
    return;

  window_ =
      GetContentClient()->browser()->CreateWindowForPictureInPicture(this);
}

}  // namespace content
