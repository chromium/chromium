// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/clients/win/media_foundation_renderer_client_factory.h"

#include "base/task/sequenced_task_runner.h"
#include "media/base/win/dcomp_texture_wrapper.h"
#include "media/base/win/mf_feature_checks.h"
#include "media/base/win/mf_helpers.h"
#include "media/mojo/clients/mojo_media_log_service.h"
#include "media/mojo/clients/mojo_renderer.h"
#include "media/mojo/clients/mojo_renderer_factory.h"
#include "media/mojo/clients/win/media_foundation_renderer_client.h"
#include "media/mojo/mojom/renderer_extensions.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"

namespace media {

MediaFoundationRendererClientFactory::MediaFoundationRendererClientFactory(
    MediaLog* media_log,
    GetDCOMPTextureWrapperCB get_dcomp_texture_wrapper_cb,
    ObserveOverlayStateCB observe_overlay_state_cb,
    std::unique_ptr<media::MojoRendererFactory> mojo_renderer_factory,
    mojo::Remote<media::mojom::MediaFoundationRendererNotifier>
        media_foundation_renderer_notifier)
    : media_log_(media_log),
      get_dcomp_texture_wrapper_cb_(std::move(get_dcomp_texture_wrapper_cb)),
      observe_overlay_state_cb_(std::move(observe_overlay_state_cb)),
      mojo_renderer_factory_(std::move(mojo_renderer_factory)),
      media_foundation_renderer_notifier_(
          std::move(media_foundation_renderer_notifier)) {
  DVLOG_FUNC(1);
}

MediaFoundationRendererClientFactory::~MediaFoundationRendererClientFactory() {
  DVLOG_FUNC(1);
}

std::unique_ptr<media::Renderer>
MediaFoundationRendererClientFactory::CreateRenderer(
    const scoped_refptr<base::SequencedTaskRunner>& media_task_runner,
    const scoped_refptr<base::TaskRunner>& /*worker_task_runner*/,
    media::AudioRendererSink* /*audio_renderer_sink*/,
    media::VideoRendererSink* video_renderer_sink,
    media::RequestOverlayInfoCB /*request_overlay_info_cb*/,
    const gfx::ColorSpace& /*target_color_space*/) {
  DVLOG_FUNC(1);

  // Use `mojo::MakeSelfOwnedReceiver` for MediaLog so logs may go through even
  // after `this` is destructed. `Clone()` is necessary since the remote could
  // live longer than `media_log_`.
  mojo::PendingReceiver<mojom::MediaLog> media_log_pending_receiver;
  auto media_log_pending_remote =
      media_log_pending_receiver.InitWithNewPipeAndPassRemote();
  mojo::MakeSelfOwnedReceiver(
      std::make_unique<MojoMediaLogService>(media_log_->Clone()),
      std::move(media_log_pending_receiver));

  // Used to send messages from the MediaFoundationRendererClient (Renderer
  // process), to the MediaFoundationRenderer (MF_CDM LPAC Utility process).
  // The |renderer_extension_receiver| will be bound in MediaFoundationRenderer.
  mojo::PendingRemote<media::mojom::MediaFoundationRendererExtension>
      renderer_extension_remote;
  auto renderer_extension_receiver =
      renderer_extension_remote.InitWithNewPipeAndPassReceiver();

  // Used to send messages from the MediaFoundationRenderer (MF_CDM LPAC Utility
  // process), to the MediaFoundationRendererClient (Renderer process).
  // The |client_extension_receiver| will be bound in
  // MediaFoundationRendererClient.
  mojo::PendingRemote<media::mojom::MediaFoundationRendererClientExtension>
      client_extension_remote;
  auto client_extension_receiver =
      client_extension_remote.InitWithNewPipeAndPassReceiver();

  // `dcomp_texture_wrapper` could be null, which will be handled in
  // MediaFoundationRendererClient::Initialize() for a more consistent error
  // handling.
  auto dcomp_texture_wrapper = get_dcomp_texture_wrapper_cb_.Run();

  std::unique_ptr<media::MojoRenderer> mojo_renderer =
      mojo_renderer_factory_->CreateMediaFoundationRenderer(
          std::move(media_log_pending_remote),
          std::move(renderer_extension_receiver),
          std::move(client_extension_remote), media_task_runner,
          video_renderer_sink);

  // Notify the browser that a Media Foundation Renderer has been created. Live
  // Caption supports muted media so this is run regardless of whether the media
  // is audible.
  mojo::PendingRemote<media::mojom::MediaFoundationRendererObserver>
      media_foundation_renderer_observer_remote;
  media_foundation_renderer_notifier_->MediaFoundationRendererCreated(
      media_foundation_renderer_observer_remote
          .InitWithNewPipeAndPassReceiver());

  // mojo_renderer's ownership is passed to MediaFoundationRendererClient.
  return std::make_unique<MediaFoundationRendererClient>(
      media_task_runner, media_log_->Clone(), std::move(mojo_renderer),
      std::move(renderer_extension_remote),
      std::move(client_extension_receiver), std::move(dcomp_texture_wrapper),
      observe_overlay_state_cb_, video_renderer_sink,
      std::move(media_foundation_renderer_observer_remote));
}

media::MediaResource::Type
MediaFoundationRendererClientFactory::GetRequiredMediaResourceType() {
  return media::MediaResource::Type::kStream;
}

}  // namespace media
