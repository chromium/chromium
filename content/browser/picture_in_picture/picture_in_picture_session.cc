// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/picture_in_picture/picture_in_picture_session.h"

#include <utility>

#include "content/browser/picture_in_picture/picture_in_picture_service_impl.h"
#include "content/browser/picture_in_picture/picture_in_picture_window_controller_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"

namespace content {

PictureInPictureSession::PictureInPictureSession(
    PictureInPictureServiceImpl* service,
    const MediaPlayerId& player_id,
    const base::Optional<viz::SurfaceId>& surface_id,
    const gfx::Size& natural_size,
    bool show_play_pause_button,
    mojo::PendingReceiver<blink::mojom::PictureInPictureSession> receiver,
    mojo::PendingRemote<blink::mojom::PictureInPictureSessionObserver> observer,
    gfx::Size* window_size)
    : service_(service),
      receiver_(this, std::move(receiver)),
      player_id_(player_id),
      observer_(std::move(observer)) {
  receiver_.set_disconnect_handler(base::BindOnce(
      &PictureInPictureSession::OnConnectionError, base::Unretained(this)));

  GetController().SetActiveSession(this);
  GetController().EmbedSurface(surface_id.value(), natural_size);
  GetController().SetAlwaysHidePlayPauseButton(show_play_pause_button);
  GetController().Show();

  *window_size = GetController().GetSize();
}

PictureInPictureSession::~PictureInPictureSession() {
  DCHECK(is_stopping_);
}

void PictureInPictureSession::Stop(StopCallback callback) {
  StopInternal(std::move(callback));
}

void PictureInPictureSession::Update(
    uint32_t player_id,
    const base::Optional<viz::SurfaceId>& surface_id,
    const gfx::Size& natural_size,
    bool show_play_pause_button) {
  player_id_ = MediaPlayerId(service_->render_frame_host_, player_id);

  GetController().EmbedSurface(surface_id.value(), natural_size);
  GetController().SetAlwaysHidePlayPauseButton(show_play_pause_button);
  GetController().SetActiveSession(this);
}

void PictureInPictureSession::NotifyWindowResized(const gfx::Size& size) {
  observer_->OnWindowSizeChanged(size);
}

void PictureInPictureSession::Shutdown() {
  if (is_stopping_)
    return;

  StopInternal(base::NullCallback());
}

void PictureInPictureSession::StopInternal(StopCallback callback) {
  DCHECK(!is_stopping_);

  is_stopping_ = true;

  GetWebContentsImpl()->ExitPictureInPicture();

  // `OnStopped()` should only be called if there is no callback to run, as a
  // contract in the API.
  if (callback)
    std::move(callback).Run();
  else
    observer_->OnStopped();

  GetController().SetActiveSession(nullptr);

  // Reset must happen after everything is done as it will destroy |this|.
  service_->active_session_.reset();
}

void PictureInPictureSession::OnConnectionError() {
  // StopInternal() will self destruct which will close the bindings.
  StopInternal(base::NullCallback());
}

WebContentsImpl* PictureInPictureSession::GetWebContentsImpl() {
  return static_cast<WebContentsImpl*>(service_->web_contents());
}

PictureInPictureWindowControllerImpl& PictureInPictureSession::GetController() {
  return *PictureInPictureWindowControllerImpl::GetOrCreateForWebContents(
      GetWebContentsImpl());
}

}  // namespace content
