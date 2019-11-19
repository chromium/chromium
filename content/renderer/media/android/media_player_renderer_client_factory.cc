// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/android/media_player_renderer_client_factory.h"

#include "content/renderer/media/android/media_player_renderer_client.h"
#include "media/mojo/clients/mojo_renderer.h"
#include "media/mojo/clients/mojo_renderer_factory.h"
#include "media/mojo/mojom/renderer_extensions.mojom.h"

namespace content {

MediaPlayerRendererClientFactory::MediaPlayerRendererClientFactory(
    scoped_refptr<base::SingleThreadTaskRunner> compositor_task_runner,
    std::unique_ptr<media::MojoRendererFactory> mojo_renderer_factory,
    const GetStreamTextureWrapperCB& get_stream_texture_wrapper_cb)
    : get_stream_texture_wrapper_cb_(get_stream_texture_wrapper_cb),
      compositor_task_runner_(std::move(compositor_task_runner)),
      mojo_renderer_factory_(std::move(mojo_renderer_factory)) {}

MediaPlayerRendererClientFactory::~MediaPlayerRendererClientFactory() {}

std::unique_ptr<media::Renderer>
MediaPlayerRendererClientFactory::CreateRenderer(
    const scoped_refptr<base::SingleThreadTaskRunner>& media_task_runner,
    const scoped_refptr<base::TaskRunner>& /* worker_task_runner */,
    media::AudioRendererSink* /* audio_renderer_sink */,
    media::VideoRendererSink* video_renderer_sink,
    const media::RequestOverlayInfoCB& /* request_overlay_info_cb */,
    const gfx::ColorSpace& /* target_color_space */) {
  // Used to send messages from the MPRC (Renderer process), to the MPR (Browser
  // process). The |renderer_extension_request| will be bound in
  // MediaPlayerRenderer.
  mojo::PendingRemote<media::mojom::MediaPlayerRendererExtension>
      renderer_extension_remote;
  auto renderer_extension_receiver =
      renderer_extension_remote.InitWithNewPipeAndPassReceiver();

  // Used to send messages from the MPR (Browser process), to the MPRC (Renderer
  // process). The |client_extension_request| will be bound in
  // MediaPlayerRendererClient.
  mojo::PendingRemote<media::mojom::MediaPlayerRendererClientExtension>
      client_extension_remote;
  auto client_extension_receiver =
      client_extension_remote.InitWithNewPipeAndPassReceiver();

  std::unique_ptr<media::MojoRenderer> mojo_renderer =
      mojo_renderer_factory_->CreateMediaPlayerRenderer(
          std::move(renderer_extension_receiver),
          std::move(client_extension_remote), media_task_runner,
          video_renderer_sink);

  media::ScopedStreamTextureWrapper stream_texture_wrapper =
      get_stream_texture_wrapper_cb_.Run();

  return std::make_unique<MediaPlayerRendererClient>(
      std::move(renderer_extension_remote),
      std::move(client_extension_receiver), media_task_runner,
      compositor_task_runner_, std::move(mojo_renderer),
      std::move(stream_texture_wrapper), video_renderer_sink);
}

media::MediaResource::Type
MediaPlayerRendererClientFactory::GetRequiredMediaResourceType() {
  return media::MediaResource::Type::URL;
}

}  // namespace content
