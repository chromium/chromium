// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/clients/mojo_renderer_wrapper.h"

#include <utility>

namespace media {

MojoRendererWrapper::MojoRendererWrapper(
    std::unique_ptr<MojoRenderer> mojo_renderer)
    : mojo_renderer_(std::move(mojo_renderer)) {}

MojoRendererWrapper::~MojoRendererWrapper() = default;

void MojoRendererWrapper::Initialize(MediaResource* media_resource,
                                     RendererClient* client,
                                     PipelineStatusCallback init_cb) {
  mojo_renderer_->Initialize(media_resource, client, std::move(init_cb));
}

void MojoRendererWrapper::Flush(base::OnceClosure flush_cb) {
  mojo_renderer_->Flush(std::move(flush_cb));
}

void MojoRendererWrapper::StartPlayingFrom(base::TimeDelta time) {
  mojo_renderer_->StartPlayingFrom(time);
}

void MojoRendererWrapper::SetPlaybackRate(double playback_rate) {
  mojo_renderer_->SetPlaybackRate(playback_rate);
}

void MojoRendererWrapper::SetVolume(float volume) {
  mojo_renderer_->SetVolume(volume);
}

void MojoRendererWrapper::SetCdm(CdmContext* cdm_context,
                                 CdmAttachedCB cdm_attached_cb) {
  mojo_renderer_->SetCdm(cdm_context, std::move(cdm_attached_cb));
}

void MojoRendererWrapper::SetLatencyHint(
    std::optional<base::TimeDelta> latency_hint) {
  mojo_renderer_->SetLatencyHint(latency_hint);
}

base::TimeDelta MojoRendererWrapper::GetMediaTime() {
  return mojo_renderer_->GetMediaTime();
}

}  // namespace media
