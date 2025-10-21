// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/clients/win/media_foundation_renderer_client.h"

#include <utility>

#include "base/functional/callback_helpers.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "media/base/media_log.h"
#include "media/base/win/mf_feature_checks.h"
#include "media/base/win/mf_helpers.h"
#include "media/mojo/mojom/speech_recognition_service.mojom.h"
#include "media/renderers/win/media_foundation_renderer.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace media {

#define REPORT_ERROR_REASON(reason)           \
  MediaFoundationRenderer::ReportErrorReason( \
      MediaFoundationRenderer::ErrorReason::reason)

MediaFoundationRendererClient::MediaFoundationRendererClient(
    scoped_refptr<base::SequencedTaskRunner> media_task_runner,
    std::unique_ptr<MediaLog> media_log,
    std::unique_ptr<MojoRenderer> mojo_renderer,
    mojo::PendingRemote<RendererExtension> pending_renderer_extension,
    std::unique_ptr<DCOMPTextureWrapper> dcomp_texture_wrapper,
    VideoRendererSink* sink,
    mojo::PendingRemote<media::mojom::MediaFoundationRendererObserver>
        media_foundation_renderer_observer)
    : media_task_runner_(std::move(media_task_runner)),
      media_log_(std::move(media_log)),
      mojo_renderer_(std::move(mojo_renderer)),
      pending_renderer_extension_(std::move(pending_renderer_extension)),
      dcomp_texture_wrapper_(std::move(dcomp_texture_wrapper)),
      sink_(sink),
      pending_media_foundation_renderer_observer_(
          std::move(media_foundation_renderer_observer)) {
  DVLOG_FUNC(1);
}

MediaFoundationRendererClient::~MediaFoundationRendererClient() {
  DVLOG_FUNC(1);
  SignalMediaPlayingStateChange(false);
}

// Renderer implementation.

void MediaFoundationRendererClient::Initialize(MediaResource* media_resource,
                                               RendererClient* client,
                                               PipelineStatusCallback init_cb) {
  DVLOG_FUNC(1);
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());
  DCHECK(!init_cb_);

  if (!dcomp_texture_wrapper_) {
    MEDIA_LOG(ERROR, media_log_) << "Failed to create DCOMPTextureWrapper";
    REPORT_ERROR_REASON(kFailedToCreateDCompTextureWrapper);
    std::move(init_cb).Run({PIPELINE_ERROR_INITIALIZATION_FAILED,
                            "DComTextureWrapper creation failed"});
    return;
  }

  // Consume and bind the delayed PendingRemote and PendingReceiver now that
  // we are on |media_task_runner_|.
  renderer_extension_.Bind(std::move(pending_renderer_extension_),
                           media_task_runner_);

  media_foundation_renderer_observer_.Bind(
      std::move(pending_media_foundation_renderer_observer_),
      media_task_runner_);

  // Handle unexpected mojo pipe disconnection such as "mf_cdm" utility process
  // crashed or killed in Browser task manager.
  renderer_extension_.set_disconnect_handler(
      base::BindOnce(&MediaFoundationRendererClient::OnConnectionError,
                     base::Unretained(this)));

  client_ = client;
  init_cb_ = std::move(init_cb);

  auto media_streams = media_resource->GetAllStreams();

  for (DemuxerStream* stream : media_streams) {
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
    std::optional<base::TimeDelta> latency_hint) {
  mojo_renderer_->SetLatencyHint(latency_hint);
}

void MediaFoundationRendererClient::Flush(base::OnceClosure flush_cb) {
  mojo_renderer_->Flush(std::move(flush_cb));
}

void MediaFoundationRendererClient::StartPlayingFrom(base::TimeDelta time) {
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());
  SignalMediaPlayingStateChange(true);
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

void MediaFoundationRendererClient::OnTracksChanged(
    DemuxerStream::Type track_type,
    DemuxerStream* enabled_track,
    base::OnceClosure change_completed_cb) {
  if (track_type != DemuxerStream::VIDEO) {
    DLOG(WARNING) << "Audio track changes are not supported.";
    std::move(change_completed_cb).Run();
    return;
  }
  renderer_extension_->SetVideoStreamEnabled(enabled_track != nullptr);
  std::move(change_completed_cb).Run();
}

RendererType MediaFoundationRendererClient::GetRendererType() {
  return RendererType::kMediaFoundation;
}

// RendererClient implementation.

void MediaFoundationRendererClient::OnError(PipelineStatus status) {
  DVLOG_FUNC(1) << "status=" << status;

  SignalMediaPlayingStateChange(false);

  // When hardware context reset happens, presenting the `dcomp_video_frame_`
  // could cause issues like black screen flash (see crbug.com/1384544).
  // Render a black frame to avoid this issue. This is fine since the player
  // is already in an error state and `this` will be recreated.
  if (status == PIPELINE_ERROR_HARDWARE_CONTEXT_RESET && dcomp_video_frame_) {
    dcomp_video_frame_.reset();
    auto black_frame = media::VideoFrame::CreateBlackFrame(natural_size_);
    sink_->PaintSingleFrame(black_frame, true);
  }

  // Do not call MediaFoundationRenderer::ReportErrorReason() since it should've
  // already been reported in MediaFoundationRenderer.
  client_->OnError(status);
}

void MediaFoundationRendererClient::OnFallback(PipelineStatus fallback) {
  SignalMediaPlayingStateChange(false);
  client_->OnFallback(std::move(fallback).AddHere());
}

void MediaFoundationRendererClient::OnEnded() {
  SignalMediaPlayingStateChange(false);
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
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());
  DCHECK(has_video_);

  natural_size_ = size;
  dcomp_texture_wrapper_->CreateVideoFrame(
      natural_size_,
      base::BindOnce(&MediaFoundationRendererClient::OnVideoFrameCreated,
                     weak_factory_.GetWeakPtr()));

  client_->OnVideoNaturalSizeChange(natural_size_);
}

void MediaFoundationRendererClient::OnVideoOpacityChange(bool opaque) {
  DVLOG_FUNC(1) << "opaque=" << opaque;
  DCHECK(has_video_);
  client_->OnVideoOpacityChange(opaque);
}

void MediaFoundationRendererClient::OnVideoFrameRateChange(
    std::optional<int> fps) {
  DVLOG_FUNC(1) << "fps=" << (fps ? *fps : -1);
  DCHECK(has_video_);

  client_->OnVideoFrameRateChange(fps);
}

// private

void MediaFoundationRendererClient::OnConnectionError() {
  DVLOG_FUNC(1);
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());
  MEDIA_LOG(ERROR, media_log_) << "MediaFoundationRendererClient disconnected";
  REPORT_ERROR_REASON(kOnConnectionError);
  OnError(PIPELINE_ERROR_DISCONNECTED);
}

void MediaFoundationRendererClient::OnRemoteRendererInitialized(
    PipelineStatus status) {
  DVLOG_FUNC(1) << "status=" << status;
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());
  DCHECK(!init_cb_.is_null());

  if (status != PIPELINE_OK) {
    std::move(init_cb_).Run(status);
    return;
  }

  if (!has_video_) {
    std::move(init_cb_).Run(PIPELINE_OK);
    return;
  }

  // For playback with video, initialize `dcomp_texture_wrapper_` for direct
  // composition.
  bool success = dcomp_texture_wrapper_->Initialize(
      gfx::Size(1, 1),
      base::BindRepeating(&MediaFoundationRendererClient::OnOutputRectChange,
                          weak_factory_.GetWeakPtr()));
  if (!success) {
    REPORT_ERROR_REASON(kFailedToInitDCompTextureWrapper);
    std::move(init_cb_).Run({PIPELINE_ERROR_INITIALIZATION_FAILED,
                             "DComTextureWrapper init failed"});
    return;
  }

  // Initialize DCOMP texture size to {1, 1} to signify to SwapChainPresenter
  // that the video output size is not yet known.
  if (output_size_.IsEmpty())
    dcomp_texture_wrapper_->UpdateTextureSize(gfx::Size(1, 1));

  std::move(init_cb_).Run(PIPELINE_OK);
}

void MediaFoundationRendererClient::OnOutputRectChange(gfx::Rect output_rect) {
  DVLOG_FUNC(1) << "output_rect=" << output_rect.ToString();
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());
  DCHECK(has_video_);

  renderer_extension_->SetOutputRect(
      output_rect,
      base::BindOnce(&MediaFoundationRendererClient::OnSetOutputRectDone,
                     weak_factory_.GetWeakPtr(), output_rect.size()));
}

void MediaFoundationRendererClient::OnSetOutputRectDone(
    const gfx::Size& output_size,
    bool success) {
  DVLOG_FUNC(1) << "output_size=" << output_size.ToString();
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());
  DCHECK(has_video_);

  if (!success) {
    DLOG(ERROR) << "Failed to SetOutputRect";
    MEDIA_LOG(WARNING, media_log_) << "Failed to SetOutputRect";
    // Ignore this error as video can possibly be seen but displayed incorrectly
    // against the video output area.
    return;
  }

  output_size_ = output_size;
  if (output_size_updated_)
    return;

  // Call UpdateTextureSize() only 1 time to indicate DCOMP rendering is
  // ready. The actual size does not matter as long as it is not empty and not
  // (1x1).
  if (!output_size_.IsEmpty() && output_size_ != gfx::Size(1, 1)) {
    dcomp_texture_wrapper_->UpdateTextureSize(output_size_);
    output_size_updated_ = true;
  }

  InitializeDCOMPRenderingIfNeeded();

  // Ensures `SwapChainPresenter::PresentDCOMPSurface()` is invoked to add
  // video into DCOMP visual tree if needed.
  if (dcomp_video_frame_) {
    sink_->PaintSingleFrame(dcomp_video_frame_, true);
  }
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
      base::BindOnce(&MediaFoundationRendererClient::OnDCOMPSurfaceReceived,
                     weak_factory_.GetWeakPtr()));
}

void MediaFoundationRendererClient::OnDCOMPSurfaceReceived(
    const std::optional<base::UnguessableToken>& token,
    const std::string& error) {
  DVLOG_FUNC(1);
  DCHECK(has_video_);
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());

  // The error should've already been handled in MediaFoundationRenderer.
  if (!token) {
    DLOG(ERROR) << "GetDCOMPSurface failed: " + error;
    return;
  }

  dcomp_texture_wrapper_->SetDCOMPSurfaceHandle(
      token.value(),
      base::BindOnce(&MediaFoundationRendererClient::OnDCOMPSurfaceHandleSet,
                     weak_factory_.GetWeakPtr()));
}

void MediaFoundationRendererClient::OnDCOMPSurfaceHandleSet(bool success) {
  DVLOG_FUNC(1);
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());
  DCHECK(has_video_);

  if (!success) {
    MEDIA_LOG(ERROR, media_log_) << "Failed to set DCOMP surface handle";
    REPORT_ERROR_REASON(kOnDCompSurfaceHandleSetError);
    OnError(PIPELINE_ERROR_COULD_NOT_RENDER);
    return;
  }

  // Ensure `SwapChainPresenter::PresentDCOMPSurface()` is invoked to add video
  // into DCOMP visual tree since `DCOMPTexture::SetDCOMPSurfaceHandle()`
  // has just succeeded.
  if (dcomp_video_frame_) {
    sink_->PaintSingleFrame(dcomp_video_frame_,
                            /*repaint_duplicate_frame=*/true);
  }
}

void MediaFoundationRendererClient::OnVideoFrameCreated(
    scoped_refptr<VideoFrame> video_frame,
    const gpu::Mailbox& mailbox) {
  DVLOG_FUNC(1);
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());
  DCHECK(has_video_);

  video_frame->metadata().allow_overlay = true;

  if (cdm_context_) {
    video_frame->metadata().protected_video = true;
    video_frame->metadata().hw_protected = true;
  } else {
    DCHECK(SupportMediaFoundationClearPlayback());
  }

  dcomp_video_frame_ = std::move(video_frame);
  sink_->PaintSingleFrame(dcomp_video_frame_, true);
}

void MediaFoundationRendererClient::OnCdmAttached(bool success) {
  DCHECK(cdm_attached_cb_);
  std::move(cdm_attached_cb_).Run(success);
}

void MediaFoundationRendererClient::SignalMediaPlayingStateChange(
    bool is_playing) {
  // Skip if we are already in the same playing state
  if (is_playing == is_playing_) {
    return;
  }
  is_playing_ = is_playing;
}

}  // namespace media
