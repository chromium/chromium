// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/services/media_foundation_renderer_wrapper.h"

#include "base/callback_helpers.h"
#include "media/base/win/mf_helpers.h"
#include "media/mojo/mojom/renderer_extensions.mojom.h"
#include "mojo/public/cpp/system/platform_handle.h"

namespace media {

namespace {

bool HasAudio(MediaResource* media_resource) {
  DCHECK(media_resource->GetType() == MediaResource::Type::STREAM);

  const auto media_streams = media_resource->GetAllStreams();
  for (const media::DemuxerStream* stream : media_streams) {
    if (stream->type() == media::DemuxerStream::Type::AUDIO)
      return true;
  }

  return false;
}

}  // namespace

// TODO(xhwang): Remove `force_dcomp_mode_for_testing=true` after composition is
// working by default.
MediaFoundationRendererWrapper::MediaFoundationRendererWrapper(
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    mojom::FrameInterfaceFactory* frame_interfaces,
    mojo::PendingReceiver<RendererExtension> renderer_extension_receiver)
    : frame_interfaces_(frame_interfaces),
      renderer_(std::make_unique<MediaFoundationRenderer>(
          std::move(task_runner),
          /*force_dcomp_mode_for_testing=*/true)),
      renderer_extension_receiver_(this,
                                   std::move(renderer_extension_receiver)),
      site_mute_observer_(this) {
  DVLOG_FUNC(1);
  DCHECK(frame_interfaces_);
}

MediaFoundationRendererWrapper::~MediaFoundationRendererWrapper() {
  DVLOG_FUNC(1);
}

void MediaFoundationRendererWrapper::Initialize(
    MediaResource* media_resource,
    RendererClient* client,
    PipelineStatusCallback init_cb) {
  if (HasAudio(media_resource)) {
    frame_interfaces_->RegisterMuteStateObserver(
        site_mute_observer_.BindNewPipeAndPassRemote());
  }

  renderer_->Initialize(media_resource, client, std::move(init_cb));
}

void MediaFoundationRendererWrapper::SetCdm(CdmContext* cdm_context,
                                            CdmAttachedCB cdm_attached_cb) {
  renderer_->SetCdm(cdm_context, std::move(cdm_attached_cb));
}

void MediaFoundationRendererWrapper::SetLatencyHint(
    absl::optional<base::TimeDelta> latency_hint) {
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
  volume_ = volume;
  renderer_->SetVolume(muted_ ? 0 : volume_);
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

void MediaFoundationRendererWrapper::OnMuteStateChange(bool muted) {
  DVLOG_FUNC(2) << ": muted=" << muted;

  if (muted == muted_)
    return;

  muted_ = muted;
  renderer_->SetVolume(muted_ ? 0 : volume_);
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
