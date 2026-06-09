// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/picture_in_picture/picture_in_picture_service_impl.h"

#include <utility>

#include "base/functional/bind.h"
#include "content/browser/picture_in_picture/picture_in_picture_session.h"
#include "content/browser/picture_in_picture/video_picture_in_picture_window_controller_impl.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"

namespace content {

// static
void PictureInPictureServiceImpl::Create(
    RenderFrameHost* render_frame_host,
    mojo::PendingReceiver<blink::mojom::PictureInPictureService> receiver) {
  CHECK(render_frame_host);
  new PictureInPictureServiceImpl(*render_frame_host, std::move(receiver));
}

// static
PictureInPictureServiceImpl* PictureInPictureServiceImpl::CreateForTesting(
    RenderFrameHost* render_frame_host,
    mojo::PendingReceiver<blink::mojom::PictureInPictureService> receiver) {
  CHECK(render_frame_host);
  return new PictureInPictureServiceImpl(*render_frame_host,
                                         std::move(receiver));
}

void PictureInPictureServiceImpl::StartSession(
    uint32_t player_id,
    mojo::PendingAssociatedRemote<media::mojom::MediaPlayer> player_remote,
    const viz::SurfaceId& surface_id,
    const gfx::Size& natural_size,
    bool show_play_pause_button,
    mojo::PendingRemote<blink::mojom::PictureInPictureSessionObserver> observer,
    const gfx::Rect& source_bounds,
    bool request_immersive,
    StartSessionCallback callback) {
  // Invalidate any pending immersive confirmation flows. Note that this does
  // not immediately destroy the `PendingSession` stored in the pending
  // confirmation callback, as the callback object is owned by the controller
  // and will be destroyed when the controller releases or runs the callback.
  immersive_confirmation_weak_factory_.InvalidateWeakPtrs();

  auto pending_session = std::make_unique<PendingSession>(
      player_id, std::move(player_remote), surface_id, natural_size,
      show_play_pause_button, std::move(observer), source_bounds,
      mojo::WrapCallbackWithDefaultInvokeIfNotRun(
          std::move(callback), mojo::NullRemote(), gfx::Size()));

  if (!request_immersive) {
    StartSessionInternal(std::move(pending_session),
                         /*immersive_options=*/std::nullopt);
  } else {
    StartSessionImmersive(std::move(pending_session));
  }
}

void PictureInPictureServiceImpl::StartSessionInternal(
    std::unique_ptr<PictureInPictureServiceImpl::PendingSession>
        pending_session,
    std::optional<ImmersiveOptions> immersive_options) {
  gfx::Size window_size;
  mojo::PendingRemote<blink::mojom::PictureInPictureSession> session_remote;

  auto result = GetController().StartSession(
      this,
      MediaPlayerId(render_frame_host().GetGlobalId(),
                    pending_session->player_id),
      std::move(pending_session->player_remote), pending_session->surface_id,
      pending_session->natural_size, pending_session->show_play_pause_button,
      std::move(pending_session->observer), pending_session->source_bounds,
      std::move(immersive_options), &session_remote, &window_size);

  if (result == PictureInPictureResult::kSuccess) {
    // Frames are to be blocklisted from the back-forward cache because the
    // picture-in-picture continues to be displayed while the page is in the
    // cache instead of closing.
    static_cast<RenderFrameHostImpl&>(render_frame_host())
        .OnBackForwardCacheDisablingStickyFeatureUsed(
            blink::scheduler::WebSchedulerTrackedFeature::kPictureInPicture);
  }

  std::move(pending_session->callback)
      .Run(std::move(session_remote), window_size);
}

void PictureInPictureServiceImpl::StartSessionImmersive(
    std::unique_ptr<PictureInPictureServiceImpl::PendingSession>
        pending_session) {
  // Immersive playback confirmation flow can only be requested in a
  // browser-native fullscreen state.
  auto* web_contents = WebContents::FromRenderFrameHost(&render_frame_host());
  if (!web_contents || !web_contents->IsFullscreen()) {
    return;
  }

  GetController().RequestImmersivePlaybackConfirmation(base::BindOnce(
      &PictureInPictureServiceImpl::OnImmersivePlaybackConfirmation,
      immersive_confirmation_weak_factory_.GetWeakPtr(),
      std::move(pending_session)));
}

void PictureInPictureServiceImpl::OnImmersivePlaybackConfirmation(
    std::unique_ptr<PendingSession> pending_session,
    ImmersivePlaybackConfirmationResult result) {
  if (result.status != ImmersivePlaybackConfirmationStatus::kConfirmed ||
      !result.options) {
    return;
  }

  StartSessionInternal(std::move(pending_session), std::move(result.options));
}

PictureInPictureServiceImpl::PendingSession::PendingSession(
    uint32_t player_id,
    mojo::PendingAssociatedRemote<media::mojom::MediaPlayer> player_remote,
    const viz::SurfaceId& surface_id,
    const gfx::Size& natural_size,
    bool show_play_pause_button,
    mojo::PendingRemote<blink::mojom::PictureInPictureSessionObserver> observer,
    const gfx::Rect& source_bounds,
    PictureInPictureServiceImpl::StartSessionCallback callback)
    : player_id(player_id),
      player_remote(std::move(player_remote)),
      surface_id(surface_id),
      natural_size(natural_size),
      show_play_pause_button(show_play_pause_button),
      observer(std::move(observer)),
      source_bounds(source_bounds),
      callback(std::move(callback)) {}

PictureInPictureServiceImpl::PendingSession::~PendingSession() = default;

PictureInPictureServiceImpl::PictureInPictureServiceImpl(
    RenderFrameHost& render_frame_host,
    mojo::PendingReceiver<blink::mojom::PictureInPictureService> receiver)
    : DocumentService(render_frame_host, std::move(receiver)) {}

PictureInPictureServiceImpl::~PictureInPictureServiceImpl() {
  // If the service is destroyed because the frame was destroyed, the session
  // may still be active and it has to be shutdown before its dtor runs.
  if (auto* controller =
          VideoPictureInPictureWindowControllerImpl::FromWebContents(
              WebContents::FromRenderFrameHost(&render_frame_host()))) {
    controller->OnServiceDeleted(this);
  }
}

VideoPictureInPictureWindowControllerImpl&
PictureInPictureServiceImpl::GetController() {
  auto& controller =
      *VideoPictureInPictureWindowControllerImpl::GetOrCreateForWebContents(
          WebContents::FromRenderFrameHost(&render_frame_host()));
  return controller;
}

}  // namespace content
