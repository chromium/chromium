// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/clients/win/media_foundation_renderer_client.h"

#include <utility>

#include "base/callback_helpers.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "media/base/win/mf_helpers.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"

namespace media {

MediaFoundationRendererClient::MediaFoundationRendererClient(
    scoped_refptr<base::SingleThreadTaskRunner> media_task_runner,
    std::unique_ptr<MojoRenderer> mojo_renderer,
    mojo::PendingRemote<RendererExtension> pending_renderer_extension,
    std::unique_ptr<DCOMPTextureWrapper> dcomp_texture_wrapper,
    VideoRendererSink* sink)
    : media_task_runner_(std::move(media_task_runner)),
      mojo_renderer_(std::move(mojo_renderer)),
      pending_renderer_extension_(std::move(pending_renderer_extension)),
      dcomp_texture_wrapper_(std::move(dcomp_texture_wrapper)),
      sink_(sink) {
  DVLOG_FUNC(1);
}

MediaFoundationRendererClient::~MediaFoundationRendererClient() {
  DVLOG_FUNC(1);
}

// TODO(xhwang): Reorder method definitions to match the header file.

// Renderer implementation.

void MediaFoundationRendererClient::Initialize(MediaResource* media_resource,
                                               RendererClient* client,
                                               PipelineStatusCallback init_cb) {
  DVLOG_FUNC(1);
  DCHECK(media_task_runner_->BelongsToCurrentThread());
  DCHECK(!init_cb_);

  // Consume and bind the delayed PendingRemote now that we
  // are on |media_task_runner_|.
  renderer_extension_.Bind(std::move(pending_renderer_extension_),
                           media_task_runner_);

  // Handle unexpected mojo pipe disconnection such as "mf_cdm" utility process
  // crashed or killed in Browser task manager.
  renderer_extension_.set_disconnect_handler(
      base::BindOnce(&MediaFoundationRendererClient::OnConnectionError,
                     base::Unretained(this)));

  client_ = client;
  init_cb_ = std::move(init_cb);

  auto media_streams = media_resource->GetAllStreams();
  for (const DemuxerStream* stream : media_streams) {
    if (stream->type() == DemuxerStream::Type::VIDEO) {
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

  OnError(PIPELINE_ERROR_DECODE);
}

void MediaFoundationRendererClient::OnRemoteRendererInitialized(
    PipelineStatus status) {
  DVLOG_FUNC(1) << "status=" << status;
  DCHECK(media_task_runner_->BelongsToCurrentThread());
  DCHECK(!init_cb_.is_null());

  if (status != PipelineStatus::PIPELINE_OK) {
    std::move(init_cb_).Run(status);
    return;
  }

  if (has_video_) {
    using Self = MediaFoundationRendererClient;
    auto weak_ptr = weak_factory_.GetWeakPtr();
    dcomp_texture_wrapper_->Initialize(
        gfx::Size(1, 1),
        base::BindOnce(&Self::OnDCOMPSurfaceHandleCreated, weak_ptr),
        base::BindRepeating(&Self::OnCompositionParamsReceived, weak_ptr),
        base::BindOnce(&Self::OnDCOMPTextureInitialized, weak_ptr));
    // `init_cb_` will be handled in `OnDCOMPTextureInitialized()`.
    return;
  }

  std::move(init_cb_).Run(status);
}

// TODO(xhwang): Rename this method to be consistent across the stack.
void MediaFoundationRendererClient::OnDCOMPSurfaceHandleCreated(bool success) {
  DVLOG_FUNC(1);
  DCHECK(media_task_runner_->BelongsToCurrentThread());
  DCHECK(has_video_);

  dcomp_texture_wrapper_->CreateVideoFrame(
      base::BindOnce(&MediaFoundationRendererClient::OnVideoFrameCreated,
                     weak_factory_.GetWeakPtr()));
}

void MediaFoundationRendererClient::OnDCOMPSurfaceReceived(
    const absl::optional<base::UnguessableToken>& token) {
  DVLOG_FUNC(1);
  DCHECK(has_video_);
  DCHECK(media_task_runner_->BelongsToCurrentThread());

  if (!token) {
    DLOG(ERROR) << "Failed to initialize DCOMP mode or failed to get or "
                   "register DCOMP surface handle on remote renderer";
    OnError(PIPELINE_ERROR_COULD_NOT_RENDER);
    return;
  }

  dcomp_texture_wrapper_->SetDCOMPSurface(token.value());
}

void MediaFoundationRendererClient::OnDCOMPTextureInitialized(bool success) {
  DVLOG_FUNC(1) << "success=" << success;
  DCHECK(media_task_runner_->BelongsToCurrentThread());
  DCHECK(!init_cb_.is_null());
  DCHECK(has_video_);

  if (!success) {
    std::move(init_cb_).Run(PIPELINE_ERROR_INITIALIZATION_FAILED);
    return;
  }

  // Initialize DCOMP texture size to {1, 1} to signify to SwapChainPresenter
  // that the video output size is not yet known. {1, 1} is chosen as opposed to
  // {0, 0} because VideoFrameSubmitter will not submit 0x0 video frames.
  if (natural_size_.IsEmpty())
    dcomp_texture_wrapper_->UpdateTextureSize(gfx::Size(1, 1));

  std::move(init_cb_).Run(PIPELINE_OK);
}

void MediaFoundationRendererClient::OnVideoFrameCreated(
    scoped_refptr<VideoFrame> video_frame) {
  DVLOG_FUNC(1);
  DCHECK(media_task_runner_->BelongsToCurrentThread());
  DCHECK(has_video_);

  video_frame->metadata().protected_video = true;
  video_frame->metadata().allow_overlay = true;

  dcomp_video_frame_ = video_frame;
  sink_->PaintSingleFrame(dcomp_video_frame_, true);
}
void MediaFoundationRendererClient::OnCompositionParamsReceived(
    gfx::Rect output_rect) {
  DVLOG_FUNC(1) << "output_rect=" << output_rect.ToString();
  DCHECK(media_task_runner_->BelongsToCurrentThread());
  DCHECK(has_video_);

  renderer_extension_->SetOutputParams(output_rect);
}

void MediaFoundationRendererClient::InitializeDCOMPRenderingIfNeeded() {
  DVLOG_FUNC(1);
  DCHECK(has_video_);

  if (dcomp_rendering_initialized_)
    return;

  dcomp_rendering_initialized_ = true;

  // Set DirectComposition mode and get DirectComposition surface from
  // MediaFoundationRenderer.
  renderer_extension_->GetDCOMPSurface(
      mojo::WrapCallbackWithDefaultInvokeIfNotRun(
          base::BindOnce(&MediaFoundationRendererClient::OnDCOMPSurfaceReceived,
                         weak_factory_.GetWeakPtr()),
          absl::nullopt));
}

void MediaFoundationRendererClient::SetCdm(CdmContext* cdm_context,
                                           CdmAttachedCB cdm_attached_cb) {
  DVLOG_FUNC(1) << "cdm_context=" << cdm_context;
  DCHECK(cdm_context);

  if (cdm_context_) {
    DLOG(ERROR) << "Switching CDM not supported";
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
    absl::optional<base::TimeDelta> /*latency_hint*/) {
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
    const std::vector<DemuxerStream*>& enabled_tracks,
    base::OnceClosure change_completed_cb) {
  bool video_track_selected = (enabled_tracks.size() > 0);
  DVLOG_FUNC(1) << "video_track_selected=" << video_track_selected;
  renderer_extension_->SetVideoStreamEnabled(video_track_selected);
  std::move(change_completed_cb).Run();
}

// RendererClient implementation.

void MediaFoundationRendererClient::OnError(PipelineStatus status) {
  DVLOG_FUNC(1) << "status=" << status;
  client_->OnError(status);
}

void MediaFoundationRendererClient::OnEnded() {
  client_->OnEnded();
}

void MediaFoundationRendererClient::OnStatisticsUpdate(
    const PipelineStatistics& stats) {
  client_->OnStatisticsUpdate(stats);
}

void MediaFoundationRendererClient::OnBufferingStateChange(
    BufferingState state,
    BufferingStateChangeReason reason) {
  client_->OnBufferingStateChange(state, reason);
}

void MediaFoundationRendererClient::OnWaiting(WaitingReason reason) {
  client_->OnWaiting(reason);
}

void MediaFoundationRendererClient::OnAudioConfigChange(
    const AudioDecoderConfig& config) {
  client_->OnAudioConfigChange(config);
}
void MediaFoundationRendererClient::OnVideoConfigChange(
    const VideoDecoderConfig& config) {
  client_->OnVideoConfigChange(config);
}

void MediaFoundationRendererClient::OnVideoNaturalSizeChange(
    const gfx::Size& size) {
  DVLOG_FUNC(1) << "size=" << size.ToString();
  DCHECK(media_task_runner_->BelongsToCurrentThread());
  DCHECK(has_video_);

  natural_size_ = size;

  // Ensure we don't update with an empty size as |dcomp_text_wrapper_| was
  // initialized with size of 1x1.
  auto texture_size = natural_size_.IsEmpty() ? gfx::Size(1, 1) : natural_size_;
  dcomp_texture_wrapper_->UpdateTextureSize(texture_size);
  InitializeDCOMPRenderingIfNeeded();

  if (dcomp_video_frame_)
    sink_->PaintSingleFrame(dcomp_video_frame_, true);

  client_->OnVideoNaturalSizeChange(natural_size_);
}

void MediaFoundationRendererClient::OnVideoOpacityChange(bool opaque) {
  DVLOG_FUNC(1) << "opaque=" << opaque;
  DCHECK(has_video_);
  client_->OnVideoOpacityChange(opaque);
}

void MediaFoundationRendererClient::OnVideoFrameRateChange(
    absl::optional<int> fps) {
  DVLOG_FUNC(1) << "fps=" << (fps ? *fps : -1);
  DCHECK(has_video_);
  client_->OnVideoFrameRateChange(fps);
}

}  // namespace media
