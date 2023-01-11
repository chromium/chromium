// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/picture_in_picture/picture_in_picture_session.h"

#include <utility>

#include "base/functional/callback_helpers.h"
#include "content/browser/picture_in_picture/picture_in_picture_service_impl.h"
#include "content/browser/picture_in_picture/video_picture_in_picture_window_controller_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"

namespace content {

PictureInPictureSession::PictureInPictureSession(
    PictureInPictureServiceImpl* service,
    const MediaPlayerId& player_id,
    mojo::PendingAssociatedRemote<media::mojom::MediaPlayer> player_remote,
    mojo::PendingReceiver<blink::mojom::PictureInPictureSession> receiver,
    mojo::PendingRemote<blink::mojom::PictureInPictureSessionObserver> observer)
    : service_(service),
      receiver_(this, std::move(receiver)),
      player_id_(player_id),
      media_player_remote_(std::move(player_remote)),
      observer_(std::move(observer)) {
  receiver_.set_disconnect_handler(base::BindOnce(
      &PictureInPictureSession::OnConnectionError, base::Unretained(this)));
  media_player_remote_.set_disconnect_handler(base::BindOnce(
      &PictureInPictureSession::OnPlayerGone, base::Unretained(this)));
}

PictureInPictureSession::~PictureInPictureSession() {
  DCHECK(is_stopping_);
}

void PictureInPictureSession::Stop(StopCallback callback) {
  StopInternal(std::move(callback));
}

void PictureInPictureSession::Update(
    uint32_t player_id,
    mojo::PendingAssociatedRemote<media::mojom::MediaPlayer> player_remote,
    const viz::SurfaceId& surface_id,
    const gfx::Size& natural_size,
    bool show_play_pause_button) {
  player_id_ =
      MediaPlayerId(service_->render_frame_host().GetGlobalId(), player_id);

  media_player_remote_.reset();
  media_player_remote_.Bind(std::move(player_remote));
  media_player_remote_.set_disconnect_handler(base::BindOnce(
      &PictureInPictureSession::OnPlayerGone, base::Unretained(this)));

  GetController().EmbedSurface(surface_id, natural_size);
  GetController().SetShowPlayPauseButton(show_play_pause_button);
}

void PictureInPictureSession::OnPlayerGone() {
  player_id_.reset();
  GetController().SetShowPlayPauseButton(false);
}

void PictureInPictureSession::NotifyWindowResized(const gfx::Size& size) {
  observer_->OnWindowSizeChanged(size);
}

mojo::AssociatedRemote<media::mojom::MediaPlayer>&
PictureInPictureSession::GetMediaPlayerRemote() {
  DCHECK(media_player_remote_.is_bound());
  return media_player_remote_;
}

void PictureInPictureSession::Disconnect() {
  // |is_stopping_| shouldn't be true for the implementation in //chrome but if
  // the WebContentsDelegate's Picture-in-Picture calls are empty, it's possible
  // for `Disconnect()` to be called even after `StopInternal()` as the
  // expectation of self-destruction will no longer be true.
  if (is_stopping_)
    return;

  is_stopping_ = true;
  observer_->OnStopped();
}

void PictureInPictureSession::Shutdown() {
  if (is_stopping_)
    return;

  StopInternal(base::NullCallback());
}

void PictureInPictureSession::StopInternal(StopCallback callback) {
  DCHECK(!is_stopping_);

  is_stopping_ = true;

  // `OnStopped()` should only be called if there is no callback to run, as a
  // contract in the API.
  if (callback)
    std::move(callback).Run();
  else
    observer_->OnStopped();

  // |this| will be deleted after this call.
  GetWebContentsImpl()->ExitPictureInPicture();
}

void PictureInPictureSession::OnConnectionError() {
  // There is possibility that OnConnectionError arrives between StopInternal()
  // is called and |this| is deleted. As a result, DCHECK in StopInternal()
  // will fail.
  if (is_stopping_)
    return;

  // StopInternal() will self destruct which will close the bindings.
  StopInternal(base::NullCallback());
}

WebContentsImpl* PictureInPictureSession::GetWebContentsImpl() {
  return static_cast<WebContentsImpl*>(
      WebContents::FromRenderFrameHost(&service_->render_frame_host()));
}

VideoPictureInPictureWindowControllerImpl&
PictureInPictureSession::GetController() {
  return *VideoPictureInPictureWindowControllerImpl::GetOrCreateForWebContents(
      GetWebContentsImpl());
}

}  // namespace content
