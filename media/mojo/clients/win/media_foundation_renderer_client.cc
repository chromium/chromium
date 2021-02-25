// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/clients/win/media_foundation_renderer_client.h"

#include <utility>

#include "base/callback_helpers.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "media/base/win/mf_helpers.h"

namespace media {

MediaFoundationRendererClient::MediaFoundationRendererClient(
    mojo::PendingRemote<RendererExtension> renderer_extension_remote,
    scoped_refptr<base::SingleThreadTaskRunner> media_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> compositor_task_runner,
    std::unique_ptr<media::MojoRenderer> mojo_renderer,
    media::VideoRendererSink* sink)
    : mojo_renderer_(std::move(mojo_renderer)),
      sink_(sink),
      media_task_runner_(std::move(media_task_runner)),
      compositor_task_runner_(std::move(compositor_task_runner)),
      delayed_bind_renderer_extension_remote_(
          std::move(renderer_extension_remote)) {
  DVLOG_FUNC(1);
}

MediaFoundationRendererClient::~MediaFoundationRendererClient() {
  DVLOG_FUNC(1);
  if (video_rendering_started_) {
    sink_->Stop();
  }
}

void MediaFoundationRendererClient::Initialize(MediaResource* media_resource,
                                               RendererClient* client,
                                               PipelineStatusCallback init_cb) {
  DVLOG_FUNC(1);
  DCHECK(media_task_runner_->BelongsToCurrentThread());
  DCHECK(!init_cb_);

  // Consume and bind the delayed PendingRemote now that we
  // are on |media_task_runner_|.
  renderer_extension_remote_.Bind(
      std::move(delayed_bind_renderer_extension_remote_), media_task_runner_);

  // Handle unexpected mojo pipe disconnection such as "mf_cdm" utility process
  // crashed or killed in Browser task manager.
  renderer_extension_remote_.set_disconnect_handler(
      base::BindOnce(&MediaFoundationRendererClient::OnConnectionError,
                     base::Unretained(this)));

  client_ = client;
  init_cb_ = std::move(init_cb);

  const std::vector<media::DemuxerStream*> media_streams =
      media_resource->GetAllStreams();
  for (const media::DemuxerStream* stream : media_streams) {
    if (stream->type() == media::DemuxerStream::Type::VIDEO) {
      has_video_ = true;
      break;
    }
  }

  mojo_renderer_->Initialize(
      media_resource, this,
      base::BindOnce(
          &MediaFoundationRendererClient::OnRemoteRendererInitialized,
          weak_factory_.GetWeakPtr()));
}

void MediaFoundationRendererClient::OnConnectionError() {
  DVLOG_FUNC(1);
  DCHECK(media_task_runner_->BelongsToCurrentThread());

  if (waiting_for_dcomp_surface_handle_) {
    OnReceivedRemoteDCOMPSurface(mojo::ScopedHandle());
  }
}

void MediaFoundationRendererClient::OnRemoteRendererInitialized(
    PipelineStatus status) {
  DVLOG_FUNC(1) << "status=" << status;

  DCHECK(media_task_runner_->BelongsToCurrentThread());
  if (status != media::PipelineStatus::PIPELINE_OK) {
    DCHECK(!init_cb_.is_null());
    std::move(init_cb_).Run(status);
    return;
  }

  if (has_video_) {
    // TODO(frankli): Add code to init DCOMPTextureWrapper.
  } else {
    std::move(init_cb_).Run(status);
  }
}

void MediaFoundationRendererClient::OnDCOMPSurfaceHandleCreated(bool success) {
  if (!media_task_runner_->BelongsToCurrentThread()) {
    media_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(
            &MediaFoundationRendererClient::OnDCOMPSurfaceHandleCreated,
            weak_factory_.GetWeakPtr(), success));
    return;
  }

  DVLOG_FUNC(1);
  DCHECK(has_video_);

  dcomp_surface_handle_bound_ = true;
  return;
}

void MediaFoundationRendererClient::OnReceivedRemoteDCOMPSurface(
    mojo::ScopedHandle surface_handle) {
  DVLOG_FUNC(1);
  DCHECK(has_video_);
  DCHECK(surface_handle.is_valid());
  DCHECK(media_task_runner_->BelongsToCurrentThread());

  waiting_for_dcomp_surface_handle_ = false;
  base::win::ScopedHandle local_handle =
      mojo::UnwrapPlatformHandle(std::move(surface_handle)).TakeHandle();
  RegisterDCOMPSurfaceHandleInGPUProcess(std::move(local_handle));
}

void MediaFoundationRendererClient::RegisterDCOMPSurfaceHandleInGPUProcess(
    base::win::ScopedHandle surface_handle) {
  if (!media_task_runner_->BelongsToCurrentThread()) {
    media_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&MediaFoundationRendererClient::
                           RegisterDCOMPSurfaceHandleInGPUProcess,
                       weak_factory_.GetWeakPtr(), std::move(surface_handle)));
    return;
  }

  DVLOG_FUNC(1) << "surface_handle=" << surface_handle.Get();
  DCHECK(has_video_);

  mojo::ScopedHandle mojo_surface_handle =
      mojo::WrapPlatformHandle(mojo::PlatformHandle(std::move(surface_handle)));

  // TODO(frankli): Pass the |mojo_surface_handle| to Gpu process.
}

void MediaFoundationRendererClient::OnDCOMPSurfaceRegisteredInGPUProcess(
    const base::UnguessableToken& token) {
  DVLOG_FUNC(1);
  DCHECK(has_video_);

  return;
}

void MediaFoundationRendererClient::OnDCOMPSurfaceTextureReleased() {
  DCHECK(has_video_);
  return;
}

void MediaFoundationRendererClient::OnDCOMPStreamTextureInitialized(
    bool success) {
  DVLOG_FUNC(1) << "success=" << success;
  DCHECK(media_task_runner_->BelongsToCurrentThread());
  DCHECK(!init_cb_.is_null());
  DCHECK(has_video_);

  media::PipelineStatus status = media::PipelineStatus::PIPELINE_OK;
  if (!success) {
    status = media::PipelineStatus::PIPELINE_ERROR_INITIALIZATION_FAILED;
  }
  if (natural_size_.width() != 0 || natural_size_.height() != 0) {
    InitializeDCOMPRendering();
  }
  std::move(init_cb_).Run(status);
}

void MediaFoundationRendererClient::OnVideoFrameCreated(
    scoped_refptr<media::VideoFrame> video_frame) {
  if (!media_task_runner_->BelongsToCurrentThread()) {
    media_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&MediaFoundationRendererClient::OnVideoFrameCreated,
                       weak_factory_.GetWeakPtr(), video_frame));
    return;
  }

  DVLOG_FUNC(1);
  DCHECK(has_video_);

  video_frame->metadata().protected_video = true;
  video_frame->metadata().allow_overlay = true;

  dcomp_frame_ = video_frame;

  sink_->PaintSingleFrame(dcomp_frame_, true);
}

void MediaFoundationRendererClient::OnCompositionParamsReceived(
    gfx::Rect output_rect) {
  DVLOG_FUNC(1) << "output_rect=" << output_rect.ToString();
  DCHECK(media_task_runner_->BelongsToCurrentThread());
  DCHECK(has_video_);

  renderer_extension_remote_->SetOutputParams(output_rect);
  return;
}

bool MediaFoundationRendererClient::MojoSetDCOMPMode(bool enabled) {
  DVLOG_FUNC(1) << "enabled=" << enabled;
  DCHECK(media_task_runner_->BelongsToCurrentThread());
  DCHECK(renderer_extension_remote_.is_bound());

  bool success = false;
  if (!renderer_extension_remote_->SetDCOMPMode(enabled, &success)) {
    return false;
  }
  return success;
}

void MediaFoundationRendererClient::MojoGetDCOMPSurface() {
  DVLOG_FUNC(1);
  DCHECK(media_task_runner_->BelongsToCurrentThread());
  DCHECK(renderer_extension_remote_.is_bound());

  if (!renderer_extension_remote_.is_connected()) {
    media_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(
            &MediaFoundationRendererClient::OnReceivedRemoteDCOMPSurface,
            weak_factory_.GetWeakPtr(), mojo::ScopedHandle()));
    return;
  }
  waiting_for_dcomp_surface_handle_ = true;
  renderer_extension_remote_->GetDCOMPSurface(base::BindOnce(
      &MediaFoundationRendererClient::OnReceivedRemoteDCOMPSurface,
      weak_factory_.GetWeakPtr()));
}

void MediaFoundationRendererClient::InitializeDCOMPRendering() {
  DVLOG_FUNC(1);
  DCHECK(has_video_);

  if (dcomp_rendering_initialized_) {
    return;
  }

  if (!MojoSetDCOMPMode(true)) {
    DLOG(ERROR) << "Failed to initialize DCOMP mode on remote renderer. this="
                << this;
    return;
  }
  MojoGetDCOMPSurface();

  dcomp_rendering_initialized_ = true;
  return;
}

void MediaFoundationRendererClient::SetCdm(CdmContext* cdm_context,
                                           CdmAttachedCB cdm_attached_cb) {
  DVLOG_FUNC(1) << "cdm_context=" << cdm_context;
  DCHECK(cdm_context);

  if (cdm_context_) {
    DLOG(ERROR) << "Switching CDM not supported. this=" << this;
    std::move(cdm_attached_cb).Run(false);
    return;
  }

  cdm_context_ = cdm_context;
  DCHECK(cdm_attached_cb_.is_null());
  cdm_attached_cb_ = std::move(cdm_attached_cb);
  mojo_renderer_->SetCdm(
      cdm_context_,
      base::BindOnce(&MediaFoundationRendererClient::OnCdmAttached,
                     weak_factory_.GetWeakPtr()));
}

void MediaFoundationRendererClient::SetLatencyHint(
    base::Optional<base::TimeDelta> /*latency_hint*/) {
  // We do not use the latency hint today
}

void MediaFoundationRendererClient::OnCdmAttached(bool success) {
  DCHECK(cdm_attached_cb_);
  std::move(cdm_attached_cb_).Run(success);
}

void MediaFoundationRendererClient::Flush(base::OnceClosure flush_cb) {
  mojo_renderer_->Flush(std::move(flush_cb));
}

void MediaFoundationRendererClient::StartPlayingFrom(base::TimeDelta time) {
  mojo_renderer_->StartPlayingFrom(time);
}

void MediaFoundationRendererClient::SetPlaybackRate(double playback_rate) {
  mojo_renderer_->SetPlaybackRate(playback_rate);
}

void MediaFoundationRendererClient::SetVolume(float volume) {
  mojo_renderer_->SetVolume(volume);
}

base::TimeDelta MediaFoundationRendererClient::GetMediaTime() {
  return mojo_renderer_->GetMediaTime();
}

void MediaFoundationRendererClient::OnSelectedVideoTracksChanged(
    const std::vector<media::DemuxerStream*>& enabled_tracks,
    base::OnceClosure change_completed_cb) {
  bool video_track_selected = (enabled_tracks.size() > 0);
  DVLOG_FUNC(1) << "video_track_selected=" << video_track_selected;
  renderer_extension_remote_->SetVideoStreamEnabled(video_track_selected);
  std::move(change_completed_cb).Run();
}

void MediaFoundationRendererClient::OnError(PipelineStatus status) {
  DVLOG_FUNC(1) << "status=" << status;
  client_->OnError(status);
}

void MediaFoundationRendererClient::OnEnded() {
  client_->OnEnded();
}

void MediaFoundationRendererClient::OnStatisticsUpdate(
    const media::PipelineStatistics& stats) {
  client_->OnStatisticsUpdate(stats);
}

void MediaFoundationRendererClient::OnBufferingStateChange(
    media::BufferingState state,
    media::BufferingStateChangeReason reason) {
  client_->OnBufferingStateChange(state, reason);
}

void MediaFoundationRendererClient::OnWaiting(WaitingReason reason) {
  client_->OnWaiting(reason);
}

void MediaFoundationRendererClient::OnAudioConfigChange(
    const media::AudioDecoderConfig& config) {
  client_->OnAudioConfigChange(config);
}
void MediaFoundationRendererClient::OnVideoConfigChange(
    const media::VideoDecoderConfig& config) {
  client_->OnVideoConfigChange(config);
}

void MediaFoundationRendererClient::OnVideoNaturalSizeChange(
    const gfx::Size& size) {
  DVLOG_FUNC(1) << "size=" << size.ToString();
  DCHECK(media_task_runner_->BelongsToCurrentThread());
  DCHECK(has_video_);

  natural_size_ = size;
  // Skip creation of a new video frame if the DCOMP surface has not yet been
  // bound to the DCOMP texture as we will create a new frame after binding has
  // completed.
  if (dcomp_surface_handle_bound_) {
    // TODO(frankli): Add code to call DCOMPTextureWrapper::CreateVideoFrame().
  }
  InitializeDCOMPRendering();
  client_->OnVideoNaturalSizeChange(natural_size_);
}

void MediaFoundationRendererClient::OnVideoOpacityChange(bool opaque) {
  DVLOG_FUNC(1) << "opaque=" << opaque;
  DCHECK(has_video_);
  client_->OnVideoOpacityChange(opaque);
}

void MediaFoundationRendererClient::OnVideoFrameRateChange(
    base::Optional<int> fps) {
  DVLOG_FUNC(1) << "fps=" << (fps ? *fps : -1);
  DCHECK(has_video_);
  client_->OnVideoFrameRateChange(fps);
}

scoped_refptr<media::VideoFrame> MediaFoundationRendererClient::Render(
    base::TimeTicks deadline_min,
    base::TimeTicks deadline_max,
    bool background_rendering) {
  // Returns no video frame as it is rendered independently by Windows Direct
  // Composition.
  return nullptr;
}

void MediaFoundationRendererClient::OnFrameDropped() {
  return;
}

base::TimeDelta MediaFoundationRendererClient::GetPreferredRenderInterval() {
  // TODO(frankli): use 'viz::BeginFrameArgs::MinInterval()'.
  return base::TimeDelta::FromSeconds(0);
}

}  // namespace media
