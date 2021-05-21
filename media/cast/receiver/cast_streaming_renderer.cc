// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cast/receiver/cast_streaming_renderer.h"

namespace media {
namespace cast {

CastStreamingRenderer::CastStreamingRenderer(std::unique_ptr<Renderer> renderer)
    : real_renderer_(std::move(renderer)) {
  DCHECK(real_renderer_);
}

CastStreamingRenderer::~CastStreamingRenderer() = default;

void CastStreamingRenderer::Initialize(MediaResource* media_resource,
                                       RendererClient* client,
                                       PipelineStatusCallback init_cb) {
  real_renderer_->Initialize(media_resource, client, std::move(init_cb));
}

void CastStreamingRenderer::SetCdm(CdmContext* cdm_context,
                                   CdmAttachedCB cdm_attached_cb) {
  NOTREACHED();
}

void CastStreamingRenderer::SetLatencyHint(
    absl::optional<base::TimeDelta> latency_hint) {}

void CastStreamingRenderer::Flush(base::OnceClosure flush_cb) {}

void CastStreamingRenderer::StartPlayingFrom(base::TimeDelta time) {}

void CastStreamingRenderer::SetPlaybackRate(double playback_rate) {}

void CastStreamingRenderer::SetVolume(float volume) {}

base::TimeDelta CastStreamingRenderer::GetMediaTime() {
  // TODO(b/187763759): Implement this method.
  return {};
}

}  // namespace cast
}  // namespace media
