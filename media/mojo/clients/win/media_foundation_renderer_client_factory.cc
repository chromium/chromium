// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/clients/win/media_foundation_renderer_client_factory.h"

#include "media/base/win/mf_helpers.h"
#include "media/mojo/clients/mojo_renderer.h"
#include "media/mojo/clients/mojo_renderer_factory.h"
#include "media/mojo/clients/win/media_foundation_renderer_client.h"
#include "media/mojo/mojom/renderer_extensions.mojom.h"

namespace media {

MediaFoundationRendererClientFactory::MediaFoundationRendererClientFactory(
    scoped_refptr<base::SingleThreadTaskRunner> compositor_task_runner,
    std::unique_ptr<media::MojoRendererFactory> mojo_renderer_factory)
    : compositor_task_runner_(std::move(compositor_task_runner)),
      mojo_renderer_factory_(std::move(mojo_renderer_factory)) {
  DVLOG_FUNC(1);
}

MediaFoundationRendererClientFactory::~MediaFoundationRendererClientFactory() {
  DVLOG_FUNC(1);
}

std::unique_ptr<media::Renderer>
MediaFoundationRendererClientFactory::CreateRenderer(
    const scoped_refptr<base::SingleThreadTaskRunner>& media_task_runner,
    const scoped_refptr<base::TaskRunner>& /*worker_task_runner*/,
    media::AudioRendererSink* /*audio_renderer_sink*/,
    media::VideoRendererSink* video_renderer_sink,
    media::RequestOverlayInfoCB /*request_overlay_info_cb*/,
    const gfx::ColorSpace& /*target_color_space*/) {
  DVLOG_FUNC(1);

  // Used to send messages from the MediaFoundationRendererClient (Renderer
  // process), to the MediaFoundationRenderer (MF_CDM LPAC Utility process).
  // The |renderer_extension_receiver| will be bound in MediaFoundationRenderer.
  mojo::PendingRemote<media::mojom::MediaFoundationRendererExtension>
      renderer_extension_remote;
  auto renderer_extension_receiver =
      renderer_extension_remote.InitWithNewPipeAndPassReceiver();

  std::unique_ptr<media::MojoRenderer> mojo_renderer =
      mojo_renderer_factory_->CreateMediaFoundationRenderer(
          std::move(renderer_extension_receiver), media_task_runner,
          video_renderer_sink);

  // mojo_renderer's ownership is passed to MediaFoundationRendererClient.
  return std::make_unique<MediaFoundationRendererClient>(
      std::move(renderer_extension_remote), media_task_runner,
      compositor_task_runner_, std::move(mojo_renderer), video_renderer_sink);
}

media::MediaResource::Type
MediaFoundationRendererClientFactory::GetRequiredMediaResourceType() {
  return media::MediaResource::Type::STREAM;
}

}  // namespace media