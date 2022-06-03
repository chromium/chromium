// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/renderers/decrypting_renderer_factory.h"

#include "media/base/media_log.h"
#include "media/renderers/decrypting_renderer.h"

namespace media {

DecryptingRendererFactory::DecryptingRendererFactory(
    media::MediaLog* media_log,
    std::unique_ptr<media::RendererFactory> renderer_factory)
    : media_log_(media_log), renderer_factory_(std::move(renderer_factory)) {}

DecryptingRendererFactory::~DecryptingRendererFactory() = default;

std::unique_ptr<Renderer> DecryptingRendererFactory::CreateRenderer(
    const scoped_refptr<base::SingleThreadTaskRunner>& media_task_runner,
    const scoped_refptr<base::TaskRunner>& worker_task_runner,
    AudioRendererSink* audio_renderer_sink,
    VideoRendererSink* video_renderer_sink,
    RequestOverlayInfoCB request_overlay_info_cb,
    const gfx::ColorSpace& target_color_space) {
  std::unique_ptr<media::Renderer> renderer = renderer_factory_->CreateRenderer(
      media_task_runner, worker_task_runner, audio_renderer_sink,
      video_renderer_sink, std::move(request_overlay_info_cb),
      target_color_space);

  return std::make_unique<DecryptingRenderer>(std::move(renderer), media_log_,
                                              media_task_runner);
}

}  // namespace media
