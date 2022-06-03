// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/services/media_foundation_renderer_wrapper.h"

#include "base/callback_helpers.h"
#include "media/base/win/mf_helpers.h"
#include "media/mojo/mojom/renderer_extensions.mojom.h"
#include "media/mojo/services/mojo_media_log.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"
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

MediaFoundationRendererWrapper::MediaFoundationRendererWrapper(
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    mojom::FrameInterfaceFactory* frame_interfaces,
    mojo::PendingRemote<mojom::MediaLog> media_log_remote,
    mojo::PendingReceiver<RendererExtension> renderer_extension_receiver)
    : frame_interfaces_(frame_interfaces),
      renderer_(std::make_unique<MediaFoundationRenderer>(
          task_runner,
          std::make_unique<MojoMediaLog>(std::move(media_log_remote),
                                         task_runner))),
      renderer_extension_receiver_(this,
                                   std::move(renderer_extension_receiver)),
      site_mute_observer_(this) {
  DVLOG_FUNC(1);
  DCHECK(frame_interfaces_);
}

MediaFoundationRendererWrapper::~MediaFoundationRendererWrapper() {
  DVLOG_FUNC(1);
  if (!dcomp_surface_token_.is_empty())
    dcomp_surface_registry_->UnregisterDCOMPSurfaceHandle(dcomp_surface_token_);
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

void MediaFoundationRendererWrapper::GetDCOMPSurface(
    GetDCOMPSurfaceCallback callback) {
  if (has_get_dcomp_surface_called_) {
    renderer_extension_receiver_.ReportBadMessage(
        "GetDCOMPSurface should only be called once!");
    return;
  }

  has_get_dcomp_surface_called_ = true;
  renderer_->GetDCompSurface(
      base::BindOnce(&MediaFoundationRendererWrapper::OnReceiveDCOMPSurface,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void MediaFoundationRendererWrapper::SetVideoStreamEnabled(bool enabled) {
  renderer_->SetVideoStreamEnabled(enabled);
}

void MediaFoundationRendererWrapper::SetOutputRect(
    const gfx::Rect& output_rect,
    SetOutputRectCallback callback) {
  renderer_->SetOutputRect(output_rect, std::move(callback));
}

void MediaFoundationRendererWrapper::OnMuteStateChange(bool muted) {
  DVLOG_FUNC(2) << ": muted=" << muted;

  if (muted == muted_)
    return;

  muted_ = muted;
  renderer_->SetVolume(muted_ ? 0 : volume_);
}

void MediaFoundationRendererWrapper::OnReceiveDCOMPSurface(
    GetDCOMPSurfaceCallback callback,
    base::win::ScopedHandle handle) {
  if (!handle.IsValid()) {
    std::move(callback).Run(absl::nullopt);
    return;
  }

  if (!dcomp_surface_registry_) {
    frame_interfaces_->CreateDCOMPSurfaceRegistry(
        dcomp_surface_registry_.BindNewPipeAndPassReceiver());
  }

  auto register_cb = base::BindOnce(
      &MediaFoundationRendererWrapper::OnDCOMPSurfaceHandleRegistered,
      weak_factory_.GetWeakPtr(), std::move(callback));

  dcomp_surface_registry_->RegisterDCOMPSurfaceHandle(
      mojo::PlatformHandle(std::move(handle)),
      mojo::WrapCallbackWithDefaultInvokeIfNotRun(std::move(register_cb),
                                                  absl::nullopt));
}

void MediaFoundationRendererWrapper::OnDCOMPSurfaceHandleRegistered(
    GetDCOMPSurfaceCallback callback,
    const absl::optional<base::UnguessableToken>& token) {
  if (token) {
    DCHECK(dcomp_surface_token_.is_empty());
    dcomp_surface_token_ = token.value();
  }

  std::move(callback).Run(token);
}

}  // namespace media
