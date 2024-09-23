// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/picture_in_picture/video_picture_in_picture_window_controller_impl.h"

#include <set>
#include <utility>

#include "components/viz/common/surfaces/surface_id.h"
#include "content/browser/media/media_web_contents_observer.h"
#include "content/browser/media/session/media_session_impl.h"
#include "content/browser/picture_in_picture/document_picture_in_picture_window_controller_impl.h"
#include "content/browser/picture_in_picture/picture_in_picture_session.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/overlay_window.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"  // for PictureInPictureResult
#include "content/public/browser/web_contents_observer.h"
#include "content/public/common/content_client.h"

namespace content {

// static
VideoPictureInPictureWindowController*
PictureInPictureWindowController::GetOrCreateVideoPictureInPictureController(
    WebContents* web_contents) {
  return VideoPictureInPictureWindowControllerImpl::GetOrCreateForWebContents(
      web_contents);
}

// static
VideoPictureInPictureWindowControllerImpl*
VideoPictureInPictureWindowControllerImpl::GetOrCreateForWebContents(
    WebContents* web_contents) {
  DCHECK(web_contents);

  // This is a no-op if the controller already exists.
  CreateForWebContents(web_contents);
  return FromWebContents(web_contents);
}

VideoPictureInPictureWindowControllerImpl::
    ~VideoPictureInPictureWindowControllerImpl() = default;

VideoPictureInPictureWindowControllerImpl::
    VideoPictureInPictureWindowControllerImpl(WebContents* web_contents)
    : WebContentsUserData<VideoPictureInPictureWindowControllerImpl>(
          *web_contents),
      WebContentsObserver(web_contents) {}

void VideoPictureInPictureWindowControllerImpl::Show() {
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
  media_session_action_toggle_microphone_handled_ =
      media_session->ShouldRouteAction(
          media_session::mojom::MediaSessionAction::kToggleMicrophone);
  media_session_action_toggle_camera_handled_ =
      media_session->ShouldRouteAction(
          media_session::mojom::MediaSessionAction::kToggleCamera);
  media_session_action_hang_up_handled_ = media_session->ShouldRouteAction(
      media_session::mojom::MediaSessionAction::kHangUp);
  media_session_action_previous_slide_handled_ =
      media_session->ShouldRouteAction(
          media_session::mojom::MediaSessionAction::kPreviousSlide);
  media_session_action_next_slide_handled_ = media_session->ShouldRouteAction(
      media_session::mojom::MediaSessionAction::kNextSlide);

  UpdatePlayPauseButtonVisibility();
  window_->SetSkipAdButtonVisibility(media_session_action_skip_ad_handled_);
  window_->SetNextTrackButtonVisibility(
      media_session_action_next_track_handled_);
  window_->SetPreviousTrackButtonVisibility(
      media_session_action_previous_track_handled_);
  window_->SetMicrophoneMuted(microphone_muted_);
  window_->SetToggleMicrophoneButtonVisibility(
      media_session_action_toggle_microphone_handled_);
  window_->SetCameraState(camera_turned_on_);
  window_->SetToggleCameraButtonVisibility(
      media_session_action_toggle_camera_handled_);
  window_->SetHangUpButtonVisibility(media_session_action_hang_up_handled_);
  window_->SetNextSlideButtonVisibility(
      media_session_action_next_slide_handled_);
  window_->SetPreviousSlideButtonVisibility(
      media_session_action_previous_slide_handled_);
  window_->ShowInactive();
  GetWebContentsImpl()->SetHasPictureInPictureVideo(true);
}

void VideoPictureInPictureWindowControllerImpl::FocusInitiator() {
  GetWebContentsImpl()->Activate();
}

void VideoPictureInPictureWindowControllerImpl::Close(bool should_pause_video) {
  if (!window_ || !window_->IsVisible())
    return;

  window_->Hide();
  // The call to `Hide()` may cause `window_` to be cleared.
  CloseInternal(should_pause_video);
}

void VideoPictureInPictureWindowControllerImpl::CloseAndFocusInitiator() {
  Close(false /* should_pause_video */);
  FocusInitiator();
}

void VideoPictureInPictureWindowControllerImpl::OnWindowDestroyed(
    bool should_pause_video) {
  window_ = nullptr;
  CloseInternal(should_pause_video);
}

void VideoPictureInPictureWindowControllerImpl::EmbedSurface(
    const viz::SurfaceId& surface_id,
    const gfx::Size& natural_size) {
  // If there is no window, then it's already been closed. A new call to
  // StartSession is required, which will replace `surface_id_` anyway. For
  // example, if the user closes the pip window, we will end up clearing
  // `window_` as part of that. If the renderer is in the process up updating
  // the SurfaceId, then we can get here without a window.
  if (!window_)
    return;

  DCHECK(active_session_);
  DCHECK(surface_id.is_valid());

  surface_id_ = surface_id;

  // Update the playback state in step with the video surface id. If the surface
  // id was updated for the same video, this is a no-op. This could be updated
  // for a different video if another media player on the same WebContents
  // enters Picture-in-Picture mode.
  UpdatePlaybackState();

  window_->UpdateNaturalSize(natural_size);
  window_->SetSurfaceId(surface_id_);
}

VideoOverlayWindow*
VideoPictureInPictureWindowControllerImpl::GetWindowForTesting() {
  return window_.get();
}

void VideoPictureInPictureWindowControllerImpl::UpdateLayerBounds() {
  if (active_session_ && window_ && window_->IsVisible())
    active_session_->NotifyWindowResized(window_->GetBounds().size());
}

bool VideoPictureInPictureWindowControllerImpl::IsPlayerActive() {
  if (!active_session_ || !active_session_->player_id().has_value())
    return false;

  return GetWebContentsImpl()->media_web_contents_observer()->IsPlayerActive(
      active_session_->player_id().value());
}

WebContents* VideoPictureInPictureWindowControllerImpl::GetWebContents() {
  return web_contents();
}

WebContents* VideoPictureInPictureWindowControllerImpl::GetChildWebContents() {
  return nullptr;
}

std::optional<url::Origin>
VideoPictureInPictureWindowControllerImpl::GetOrigin() {
  return origin_;
}

void VideoPictureInPictureWindowControllerImpl::SetOrigin(
    std::optional<url::Origin> origin) {
  origin_ = origin;
}

void VideoPictureInPictureWindowControllerImpl::UpdatePlaybackState() {
  if (!window_)
    return;

  auto playback_state = VideoOverlayWindow::PlaybackState::kPaused;
  if (IsPlayerActive()) {
    playback_state = VideoOverlayWindow::PlaybackState::kPlaying;
  } else if (media_position_ && media_position_->end_of_media()) {
    playback_state = VideoOverlayWindow::PlaybackState::kEndOfVideo;
  }

  window_->SetPlaybackState(playback_state);
}

bool VideoPictureInPictureWindowControllerImpl::TogglePlayPause() {
  // This comes from the window, rather than the renderer, so we must actually
  // have a window at this point.
  DCHECK(window_);
  DCHECK(active_session_);

  if (IsPlayerActive()) {
    if (media_session_action_pause_handled_) {
      MediaSessionImpl::Get(web_contents())
          ->Suspend(MediaSession::SuspendType::kUI);
      return true /* still playing */;
    }

    active_session_->GetMediaPlayerRemote()->RequestPause(
        /*triggered_by_user=*/false);
    return false /* paused */;
  }

  if (media_session_action_play_handled_) {
    MediaSessionImpl::Get(web_contents())
        ->Resume(MediaSession::SuspendType::kUI);
    return false /* still paused */;
  }

  active_session_->GetMediaPlayerRemote()->RequestPlay();
  return true /* playing */;
}

PictureInPictureResult VideoPictureInPictureWindowControllerImpl::StartSession(
    PictureInPictureServiceImpl* service,
    const MediaPlayerId& player_id,
    mojo::PendingAssociatedRemote<media::mojom::MediaPlayer> player_remote,
    const viz::SurfaceId& surface_id,
    const gfx::Size& natural_size,
    bool show_play_pause_button,
    mojo::PendingRemote<blink::mojom::PictureInPictureSessionObserver> observer,
    const gfx::Rect& source_bounds,
    mojo::PendingRemote<blink::mojom::PictureInPictureSession>* session_remote,
    gfx::Size* window_size) {
  auto result = GetWebContentsImpl()->EnterPictureInPicture();

  // Picture-in-Picture may not be supported by all embedders, so we should only
  // create the session if the EnterPictureInPicture request was successful.
  if (result != PictureInPictureResult::kSuccess)
    return result;

  if (active_session_)
    active_session_->Disconnect();

  source_bounds_ = source_bounds;

  active_session_ = std::make_unique<PictureInPictureSession>(
      service, player_id, std::move(player_remote),
      session_remote->InitWithNewPipeAndPassReceiver(), std::move(observer));

  // There can be a window already if this session is replacing an old one,
  // without the old one being closed first.
  if (!window_) {
    window_ =
        GetContentClient()->browser()->CreateWindowForVideoPictureInPicture(
            this);
  }
  DCHECK(window_) << "Picture in Picture requires a valid window.";

  // If the window is closed by the system, then the picture in picture session
  // will end. The renderer must call `StartSession()` again.
  EmbedSurface(surface_id, natural_size);
  SetShowPlayPauseButton(show_play_pause_button);
  Show();

  if (on_window_created_notify_observers_callback_) {
    std::move(on_window_created_notify_observers_callback_).Run();
  }

  // TODO(crbug.com/40227464): Rather than set this synchronously, we should
  // call back with the bounds once the window provides them.
  *window_size = GetSize();
  return result;
}

void VideoPictureInPictureWindowControllerImpl::OnServiceDeleted(
    PictureInPictureServiceImpl* service) {
  if (!active_session_ || active_session_->service() != service)
    return;

  active_session_->Shutdown();
  active_session_ = nullptr;
}

void VideoPictureInPictureWindowControllerImpl::SetShowPlayPauseButton(
    bool show_play_pause_button) {
  always_show_play_pause_button_ = show_play_pause_button;
  UpdatePlayPauseButtonVisibility();
}

void VideoPictureInPictureWindowControllerImpl::SkipAd() {
  if (media_session_action_skip_ad_handled_)
    MediaSession::Get(web_contents())->SkipAd();
}

void VideoPictureInPictureWindowControllerImpl::PreviousSlide() {
  if (media_session_action_previous_slide_handled_)
    MediaSession::Get(web_contents())->PreviousSlide();
}

void VideoPictureInPictureWindowControllerImpl::NextSlide() {
  if (media_session_action_next_slide_handled_)
    MediaSession::Get(web_contents())->NextSlide();
}

void VideoPictureInPictureWindowControllerImpl::NextTrack() {
  if (media_session_action_next_track_handled_)
    MediaSession::Get(web_contents())->NextTrack();
}

void VideoPictureInPictureWindowControllerImpl::PreviousTrack() {
  if (media_session_action_previous_track_handled_)
    MediaSession::Get(web_contents())->PreviousTrack();
}

void VideoPictureInPictureWindowControllerImpl::ToggleMicrophone() {
  if (!media_session_action_toggle_microphone_handled_)
    return;

  MediaSession::Get(web_contents())->ToggleMicrophone();
}

void VideoPictureInPictureWindowControllerImpl::ToggleCamera() {
  if (!media_session_action_toggle_camera_handled_)
    return;

  MediaSession::Get(web_contents())->ToggleCamera();
}

void VideoPictureInPictureWindowControllerImpl::HangUp() {
  if (media_session_action_hang_up_handled_)
    MediaSession::Get(web_contents())->HangUp();
}

void VideoPictureInPictureWindowControllerImpl::MediaSessionInfoChanged(
    const media_session::mojom::MediaSessionInfoPtr& info) {
  if (!info)
    return;

  microphone_muted_ =
      info->microphone_state == media_session::mojom::MicrophoneState::kMuted;
  camera_turned_on_ =
      info->camera_state == media_session::mojom::CameraState::kTurnedOn;

  if (!window_)
    return;

  window_->SetMicrophoneMuted(microphone_muted_);
  window_->SetCameraState(camera_turned_on_);
}

void VideoPictureInPictureWindowControllerImpl::MediaSessionActionsChanged(
    const std::set<media_session::mojom::MediaSessionAction>& actions) {
  // TODO(crbug.com/40608570): Currently, the first Media Session to be created
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
  media_session_action_toggle_microphone_handled_ =
      actions.find(
          media_session::mojom::MediaSessionAction::kToggleMicrophone) !=
      actions.end();
  media_session_action_toggle_camera_handled_ =
      actions.find(media_session::mojom::MediaSessionAction::kToggleCamera) !=
      actions.end();
  media_session_action_hang_up_handled_ =
      actions.find(media_session::mojom::MediaSessionAction::kHangUp) !=
      actions.end();
  media_session_action_previous_slide_handled_ =
      actions.find(media_session::mojom::MediaSessionAction::kPreviousSlide) !=
      actions.end();
  media_session_action_next_slide_handled_ =
      actions.find(media_session::mojom::MediaSessionAction::kNextSlide) !=
      actions.end();

  if (!window_)
    return;

  UpdatePlayPauseButtonVisibility();
  window_->SetSkipAdButtonVisibility(media_session_action_skip_ad_handled_);
  window_->SetNextTrackButtonVisibility(
      media_session_action_next_track_handled_);
  window_->SetPreviousTrackButtonVisibility(
      media_session_action_previous_track_handled_);
  window_->SetToggleMicrophoneButtonVisibility(
      media_session_action_toggle_microphone_handled_);
  window_->SetToggleCameraButtonVisibility(
      media_session_action_toggle_camera_handled_);
  window_->SetHangUpButtonVisibility(media_session_action_hang_up_handled_);
  window_->SetNextSlideButtonVisibility(
      media_session_action_next_slide_handled_);
  window_->SetPreviousSlideButtonVisibility(
      media_session_action_previous_slide_handled_);
}

void VideoPictureInPictureWindowControllerImpl::MediaSessionPositionChanged(
    const std::optional<media_session::MediaPosition>& media_position) {
  media_position_ = media_position;
  UpdatePlaybackState();
}

gfx::Size VideoPictureInPictureWindowControllerImpl::GetSize() {
  return window_->GetBounds().size();
}

void VideoPictureInPictureWindowControllerImpl::MediaStartedPlaying(
    const MediaPlayerInfo&,
    const MediaPlayerId& media_player_id) {
  if (web_contents()->IsBeingDestroyed())
    return;

  if (!active_session_ || active_session_->player_id() != media_player_id)
    return;

  UpdatePlaybackState();
}

void VideoPictureInPictureWindowControllerImpl::MediaStoppedPlaying(
    const MediaPlayerInfo&,
    const MediaPlayerId& media_player_id,
    WebContentsObserver::MediaStoppedReason) {
  if (web_contents()->IsBeingDestroyed())
    return;

  if (!active_session_ || active_session_->player_id() != media_player_id)
    return;

  UpdatePlaybackState();
}

void VideoPictureInPictureWindowControllerImpl::WebContentsDestroyed() {
  if (window_)
    window_->Close();
}

void VideoPictureInPictureWindowControllerImpl::OnLeavingPictureInPicture(
    bool should_pause_video) {
  DCHECK(active_session_);

  if (IsPlayerActive() && should_pause_video) {
    // Pause the current video so there is only one video playing at a time.
    active_session_->GetMediaPlayerRemote()->RequestPause(
        /*triggered_by_user=*/false);
  }

  active_session_->Shutdown();
  active_session_ = nullptr;
}

void VideoPictureInPictureWindowControllerImpl::CloseInternal(
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

const gfx::Rect& VideoPictureInPictureWindowControllerImpl::GetSourceBounds()
    const {
  return source_bounds_;
}

std::optional<gfx::Rect>
VideoPictureInPictureWindowControllerImpl::GetWindowBounds() {
  if (!window_)
    return std::nullopt;
  return window_->GetBounds();
}

void VideoPictureInPictureWindowControllerImpl::
    UpdatePlayPauseButtonVisibility() {
  if (!window_)
    return;

  window_->SetPlayPauseButtonVisibility((media_session_action_pause_handled_ &&
                                         media_session_action_play_handled_) ||
                                        always_show_play_pause_button_);
}

void VideoPictureInPictureWindowControllerImpl::
    SetOnWindowCreatedNotifyObserversCallback(
        base::OnceClosure on_window_created_notify_observers_callback) {
  on_window_created_notify_observers_callback_ =
      std::move(on_window_created_notify_observers_callback);
}

WebContentsImpl*
VideoPictureInPictureWindowControllerImpl::GetWebContentsImpl() {
  return static_cast<WebContentsImpl*>(web_contents());
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(VideoPictureInPictureWindowControllerImpl);

}  // namespace content
