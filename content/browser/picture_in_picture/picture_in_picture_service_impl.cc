// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/picture_in_picture/picture_in_picture_service_impl.h"

#include <utility>

#include "content/browser/picture_in_picture/picture_in_picture_session.h"
#include "content/browser/picture_in_picture/picture_in_picture_window_controller_impl.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/public/browser/web_contents_delegate.h"

namespace content {

// static
void PictureInPictureServiceImpl::Create(
    RenderFrameHost* render_frame_host,
    mojo::PendingReceiver<blink::mojom::PictureInPictureService> receiver) {
  DCHECK(render_frame_host);
  new PictureInPictureServiceImpl(render_frame_host, std::move(receiver));
}

// static
PictureInPictureServiceImpl* PictureInPictureServiceImpl::CreateForTesting(
    RenderFrameHost* render_frame_host,
    mojo::PendingReceiver<blink::mojom::PictureInPictureService> receiver) {
  return new PictureInPictureServiceImpl(render_frame_host,
                                         std::move(receiver));
}

void PictureInPictureServiceImpl::StartSession(
    uint32_t player_id,
    const base::Optional<viz::SurfaceId>& surface_id,
    const gfx::Size& natural_size,
    bool show_play_pause_button,
    mojo::PendingRemote<blink::mojom::PictureInPictureSessionObserver> observer,
    StartSessionCallback callback) {
  gfx::Size window_size;
  mojo::PendingRemote<blink::mojom::PictureInPictureSession> session_remote;

  if (surface_id.has_value()) {
    auto result = GetController().StartSession(
        this, MediaPlayerId(render_frame_host(), player_id), surface_id.value(),
        natural_size, show_play_pause_button, std::move(observer),
        &session_remote, &window_size);

    if (result == PictureInPictureResult::kSuccess) {
      // Frames are to be blocklisted from the back-forward cache because the
      // picture-in-picture continues to be displayed while the page is in the
      // cache instead of closing.
      static_cast<RenderFrameHostImpl*>(render_frame_host())
          ->OnSchedulerTrackedFeatureUsed(
              blink::scheduler::WebSchedulerTrackedFeature::kPictureInPicture);
    }
  }

  std::move(callback).Run(std::move(session_remote), window_size);
}

PictureInPictureServiceImpl::PictureInPictureServiceImpl(
    RenderFrameHost* render_frame_host,
    mojo::PendingReceiver<blink::mojom::PictureInPictureService> receiver)
    : FrameServiceBase(render_frame_host, std::move(receiver)) {}

PictureInPictureServiceImpl::~PictureInPictureServiceImpl() {
  // If the service is destroyed because the frame was destroyed, the session
  // may still be active and it has to be shutdown before its dtor runs.
  GetController().OnServiceDeleted(this);
}

PictureInPictureWindowControllerImpl&
PictureInPictureServiceImpl::GetController() {
  return *PictureInPictureWindowControllerImpl::GetOrCreateForWebContents(
      WebContents::FromRenderFrameHost(render_frame_host()));
}

}  // namespace content
