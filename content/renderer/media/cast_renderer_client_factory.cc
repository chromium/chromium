// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/cast_renderer_client_factory.h"
#include "media/base/media_log.h"
#include "media/mojo/clients/mojo_renderer.h"
#include "media/mojo/clients/mojo_renderer_factory.h"
#include "media/renderers/decrypting_renderer.h"

namespace content {

CastRendererClientFactory::CastRendererClientFactory(
    media::MediaLog* media_log,
    std::unique_ptr<media::MojoRendererFactory> mojo_renderer_factory)
    : media_log_(media_log),
      mojo_renderer_factory_(std::move(mojo_renderer_factory)) {}

CastRendererClientFactory::~CastRendererClientFactory() = default;

std::unique_ptr<media::Renderer> CastRendererClientFactory::CreateRenderer(
    const scoped_refptr<base::SingleThreadTaskRunner>& media_task_runner,
    const scoped_refptr<base::TaskRunner>& worker_task_runner,
    media::AudioRendererSink* audio_renderer_sink,
    media::VideoRendererSink* video_renderer_sink,
    media::RequestOverlayInfoCB request_overlay_info_cb,
    const gfx::ColorSpace& target_color_space) {
  std::unique_ptr<media::Renderer> renderer =
      mojo_renderer_factory_->CreateCastRenderer(media_task_runner,
                                                 video_renderer_sink);

  return std::make_unique<media::DecryptingRenderer>(
      std::move(renderer), media_log_, media_task_runner);
}

}  // namespace content
