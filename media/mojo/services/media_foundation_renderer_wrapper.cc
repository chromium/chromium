// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/services/media_foundation_renderer_wrapper.h"

#include "base/callback_helpers.h"
#include "media/base/win/mf_helpers.h"
#include "media/mojo/mojom/renderer_extensions.mojom.h"
#include "mojo/public/cpp/system/platform_handle.h"

namespace media {

MediaFoundationRendererWrapper::MediaFoundationRendererWrapper(
    bool web_contents_muted,
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    mojo::PendingReceiver<RendererExtension> renderer_extension_receiver)
    : renderer_(std::make_unique<media::MediaFoundationRenderer>(
          web_contents_muted,
          std::move(task_runner))),
      renderer_extension_receiver_(this,
                                   std::move(renderer_extension_receiver)) {
  DVLOG_FUNC(1);
}

MediaFoundationRendererWrapper::~MediaFoundationRendererWrapper() {
  DVLOG_FUNC(1);
}

void MediaFoundationRendererWrapper::Initialize(
    media::MediaResource* media_resource,
    media::RendererClient* client,
    media::PipelineStatusCallback init_cb) {
  renderer_->Initialize(media_resource, client, std::move(init_cb));
}

void MediaFoundationRendererWrapper::SetCdm(CdmContext* cdm_context,
                                            CdmAttachedCB cdm_attached_cb) {
  renderer_->SetCdm(cdm_context, std::move(cdm_attached_cb));
}

void MediaFoundationRendererWrapper::SetLatencyHint(
    base::Optional<base::TimeDelta> latency_hint) {
  renderer_->SetLatencyHint(latency_hint);
}

void MediaFoundationRendererWrapper::Flush(base::OnceClosure flush_cb) {
  renderer_->Flush(std::move(flush_cb));
}

void MediaFoundationRendererWrapper::StartPlayingFrom(base::TimeDelta time) {
  renderer_->StartPlayingFrom(time);
}

void MediaFoundationRendererWrapper::SetPlaybackRate(double playback_rate) {
  renderer_->SetPlaybackRate(playback_rate);
}

void MediaFoundationRendererWrapper::SetVolume(float volume) {
  return renderer_->SetVolume(volume);
}

base::TimeDelta MediaFoundationRendererWrapper::GetMediaTime() {
  return renderer_->GetMediaTime();
}

void MediaFoundationRendererWrapper::SetDCOMPMode(
    bool enabled,
    SetDCOMPModeCallback callback) {
  renderer_->SetDCompMode(enabled, std::move(callback));
}

void MediaFoundationRendererWrapper::GetDCOMPSurface(
    GetDCOMPSurfaceCallback callback) {
  get_decomp_surface_cb_ = std::move(callback);
  renderer_->GetDCompSurface(
      base::BindOnce(&MediaFoundationRendererWrapper::OnReceiveDCOMPSurface,
                     weak_factory_.GetWeakPtr()));
}

void MediaFoundationRendererWrapper::SetVideoStreamEnabled(bool enabled) {
  renderer_->SetVideoStreamEnabled(enabled);
}

void MediaFoundationRendererWrapper::SetOutputParams(
    const gfx::Rect& output_rect) {
  renderer_->SetOutputParams(output_rect);
}

void MediaFoundationRendererWrapper::OnReceiveDCOMPSurface(HANDLE handle) {
  base::win::ScopedHandle local_surface_handle;
  local_surface_handle.Set(handle);
  if (get_decomp_surface_cb_) {
    mojo::ScopedHandle surface_handle;
    surface_handle = mojo::WrapPlatformHandle(
        mojo::PlatformHandle(std::move(local_surface_handle)));
    std::move(get_decomp_surface_cb_).Run(std::move(surface_handle));
  }
}

}  // namespace media
