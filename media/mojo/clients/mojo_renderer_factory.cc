// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/clients/mojo_renderer_factory.h"

#include <utility>

#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "build/build_config.h"
#include "media/mojo/clients/mojo_renderer.h"
#include "media/mojo/mojom/renderer_extensions.mojom.h"
#include "media/renderers/decrypting_renderer.h"
#include "media/renderers/video_overlay_factory.h"
#include "mojo/public/cpp/bindings/pending_remote.h"

namespace media {

MojoRendererFactory::MojoRendererFactory(
    media::mojom::InterfaceFactory* interface_factory)
    : interface_factory_(interface_factory) {
  DCHECK(interface_factory_);
}

MojoRendererFactory::~MojoRendererFactory() = default;

std::unique_ptr<Renderer> MojoRendererFactory::CreateRenderer(
    const scoped_refptr<base::SequencedTaskRunner>& media_task_runner,
    const scoped_refptr<base::TaskRunner>& worker_task_runner,
    AudioRendererSink* audio_renderer_sink,
    VideoRendererSink* video_renderer_sink,
    RequestOverlayInfoCB request_overlay_info_cb,
    const gfx::ColorSpace& target_color_space) {
  DCHECK(interface_factory_);

  auto overlay_factory = std::make_unique<VideoOverlayFactory>();

  mojo::PendingRemote<mojom::Renderer> renderer_remote;
  interface_factory_->CreateDefaultRenderer(
      std::string(), renderer_remote.InitWithNewPipeAndPassReceiver());

  return std::make_unique<MojoRenderer>(
      media_task_runner, std::move(overlay_factory), video_renderer_sink,
      std::move(renderer_remote));
}

#if BUILDFLAG(IS_WIN)
std::unique_ptr<MojoRenderer>
MojoRendererFactory::CreateMediaFoundationRenderer(
    mojo::PendingRemote<mojom::MediaLog> media_log_remote,
    mojo::PendingReceiver<mojom::MediaFoundationRendererExtension>
        renderer_extension_receiver,
    mojo::PendingRemote<mojom::MediaFoundationRendererClientExtension>
        client_extension_remote,
    const scoped_refptr<base::SequencedTaskRunner>& media_task_runner,
    VideoRendererSink* video_renderer_sink) {
  DCHECK(interface_factory_);
  mojo::PendingRemote<mojom::Renderer> renderer_remote;
  interface_factory_->CreateMediaFoundationRenderer(
      std::move(media_log_remote),
      renderer_remote.InitWithNewPipeAndPassReceiver(),
      std::move(renderer_extension_receiver),
      std::move(client_extension_remote));

  return std::make_unique<MojoRenderer>(
      media_task_runner, /*video_overlay_factory=*/nullptr, video_renderer_sink,
      std::move(renderer_remote));
}
#endif  // BUILDFLAG(IS_WIN)

#if BUILDFLAG(ENABLE_CAST_RENDERER)
std::unique_ptr<MojoRenderer> MojoRendererFactory::CreateCastRenderer(
    const scoped_refptr<base::SequencedTaskRunner>& media_task_runner,
    VideoRendererSink* video_renderer_sink) {
  DCHECK(interface_factory_);

  auto overlay_factory = std::make_unique<VideoOverlayFactory>();

  mojo::PendingRemote<mojom::Renderer> renderer_remote;
  interface_factory_->CreateCastRenderer(
      overlay_factory->overlay_plane_id(),
      renderer_remote.InitWithNewPipeAndPassReceiver());

  return std::make_unique<MojoRenderer>(
      media_task_runner, std::move(overlay_factory), video_renderer_sink,
      std::move(renderer_remote));
}
#endif  // BUILDFLAG(ENABLE_CAST_RENDERER)

#if BUILDFLAG(IS_ANDROID)
std::unique_ptr<MojoRenderer> MojoRendererFactory::CreateFlingingRenderer(
    const std::string& presentation_id,
    mojo::PendingRemote<mojom::FlingingRendererClientExtension>
        client_extension_remote,
    const scoped_refptr<base::SequencedTaskRunner>& media_task_runner,
    VideoRendererSink* video_renderer_sink) {
  DCHECK(interface_factory_);
  mojo::PendingRemote<mojom::Renderer> renderer_remote;

  interface_factory_->CreateFlingingRenderer(
      presentation_id, std::move(client_extension_remote),
      renderer_remote.InitWithNewPipeAndPassReceiver());

  return std::make_unique<MojoRenderer>(media_task_runner, nullptr,
                                        video_renderer_sink,
                                        std::move(renderer_remote));
}

std::unique_ptr<MojoRenderer> MojoRendererFactory::CreateMediaPlayerRenderer(
    mojo::PendingReceiver<mojom::MediaPlayerRendererExtension>
        renderer_extension_receiver,
    mojo::PendingRemote<mojom::MediaPlayerRendererClientExtension>
        client_extension_remote,
    const scoped_refptr<base::SequencedTaskRunner>& media_task_runner,
    VideoRendererSink* video_renderer_sink) {
  DCHECK(interface_factory_);
  mojo::PendingRemote<mojom::Renderer> renderer_remote;

  interface_factory_->CreateMediaPlayerRenderer(
      std::move(client_extension_remote),
      renderer_remote.InitWithNewPipeAndPassReceiver(),
      std::move(renderer_extension_receiver));

  return std::make_unique<MojoRenderer>(media_task_runner, nullptr,
                                        video_renderer_sink,
                                        std::move(renderer_remote));
}
#endif  // BUILDFLAG(IS_ANDROID)

}  // namespace media
