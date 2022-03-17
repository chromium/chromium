// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/clients/win/media_foundation_renderer_client.h"

#include <utility>

#include "base/callback_helpers.h"
#include "base/task/bind_post_task.h"
#include "media/base/media_log.h"
#include "media/base/win/mf_helpers.h"
#include "media/renderers/win/media_foundation_renderer.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"

namespace media {

MediaFoundationRendererClient::MediaFoundationRendererClient(
    scoped_refptr<base::SingleThreadTaskRunner> media_task_runner,
    std::unique_ptr<MediaLog> media_log,
    std::unique_ptr<MojoRenderer> mojo_renderer,
    mojo::PendingRemote<RendererExtension> pending_renderer_extension,
    mojo::PendingReceiver<ClientExtension> client_extension_receiver,
    std::unique_ptr<DCOMPTextureWrapper> dcomp_texture_wrapper,
    VideoRendererSink* sink)
    : media_task_runner_(std::move(media_task_runner)),
      media_log_(std::move(media_log)),
      mojo_renderer_(std::move(mojo_renderer)),
      pending_renderer_extension_(std::move(pending_renderer_extension)),
      dcomp_texture_wrapper_(std::move(dcomp_texture_wrapper)),
      sink_(sink),
      pending_client_extension_receiver_(std::move(client_extension_receiver)),
      client_extension_receiver_(this) {
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
  DCHECK(media_task_runner_->BelongsToCurrentThread());
  DCHECK(!init_cb_);

  // Consume and bind the delayed PendingRemote and PendingReceiver now that
  // we are on |media_task_runner_|.
  renderer_extension_.Bind(std::move(pending_renderer_extension_),
                           media_task_runner_);

  client_extension_receiver_.Bind(std::move(pending_client_extension_receiver_),
                                  media_task_runner_);

  // Handle unexpected mojo pipe disconnection such as "mf_cdm" utility process
  // crashed or killed in Browser task manager.
  renderer_extension_.set_disconnect_handler(
      base::BindOnce(&MediaFoundationRendererClient::OnConnectionError,
                     base::Unretained(this)));

  client_ = client;
  init_cb_ = std::move(init_cb);

  auto media_streams = media_resource->GetAllStreams();
  bool start_in_dcomp_mode = false;
  // Start off at 60 fps for our render interval, however it will be updated
  // later in OnVideoFrameRateChange
  render_interval_ = base::Microseconds(16666);
  for (DemuxerStream* stream : media_streams) {
    if (stream->type() == DemuxerStream::Type::VIDEO) {
      if (stream->video_decoder_config().is_encrypted()) {
        // If the content is clear we'll start in frame server mode
        // and wait to be promoted to DComp.
        // This conditional must match the conditional in
        // MediaFoundationRenderer::Initialize
        start_in_dcomp_mode = true;
      }
      has_video_ = true;
      break;
    }
  }

  if (!start_in_dcomp_mode) {
    media_engine_in_frame_server_mode_ = true;
  }

  mojo_renderer_->Initialize(
      media_resource, this,
      base::BindOnce(
          &MediaFoundationRendererClient::OnRemoteRendererInitialized,
          weak_factory_.GetWeakPtr()));
}

void MediaFoundationRendererClient::InitializeFramePool(
    mojom::FramePoolInitializationParametersPtr pool_info) {
  DCHECK_GT(pool_info->frame_textures.size(), static_cast<size_t>(0));

  // Release our references to the video pool so that once the
  // rendering is complete the memory will be freed.
  video_frame_pool_.clear();

  for (const auto& frame_info : pool_info->frame_textures) {
    dcomp_texture_wrapper_->CreateVideoFrame(
        pool_info->texture_size, std::move(frame_info->texture_handle),
        base::BindOnce(
            [](base::flat_map<base::UnguessableToken,
                              scoped_refptr<VideoFrame>>& video_frame_pool,
               const base::UnguessableToken& token,
               scoped_refptr<VideoFrame> video_frame) {
              video_frame_pool.insert({token, std::move(video_frame)});
            },
            std::ref(video_frame_pool_), frame_info->token));
  }
}

bool MediaFoundationRendererClient::IsFrameServerMode() const {
  return media_engine_in_frame_server_mode_;
}

void MediaFoundationRendererClient::OnFrameAvailable(
    const base::UnguessableToken& frame_token,
    const gfx::Size& size,
    base::TimeDelta timestamp) {
  DCHECK(media_task_runner_->BelongsToCurrentThread());
  DCHECK(has_video_);

  auto video_frame = video_frame_pool_.find(frame_token);
  // It is possible to become unsynced when we are reinitializing the frame
  // pool so we are just checking to make sure the frame has been acquired.
  if (video_frame == video_frame_pool_.end()) {
    return;
  }

  scoped_refptr<VideoFrame> texture_pool_video_frame = video_frame->second;

  texture_pool_video_frame->set_timestamp(timestamp);

  // The Video Frame object's Destruction Observer is called when the video
  // frame is no longer needed and the underlying texture can be reused. We
  // cannot use the video frame we created in InitializeFramePool() directly
  // because we hold onto a reference in our video frame pool so the callback
  // would not be called, and for those their callback is to destroy the shared
  // image anyway. Therefore we wrap the shared image based video frame in
  // another video frame and add the callback which allows us to reuse the
  // texture for a new video frame.
  scoped_refptr<VideoFrame> frame = VideoFrame::WrapVideoFrame(
      texture_pool_video_frame, texture_pool_video_frame->format(),
      gfx::Rect(size), size);
  frame->metadata().wants_promotion_hint = true;
  frame->metadata().allow_overlay = true;
  frame->AddDestructionObserver(base::BindPostTask(
      media_task_runner_,
      base::BindOnce(&MediaFoundationRendererClient::OnPaintComplete,
                     weak_factory_.GetWeakPtr(), frame_token)));

  // The sink needs a frame ASAP so the first frame will be painted, all
  // following frames will be returned in the Render callback.
  if (!next_video_frame_) {
    sink_->PaintSingleFrame(frame);
  }
  next_video_frame_ = frame;
}

void MediaFoundationRendererClient::OnPaintComplete(
    const base::UnguessableToken& token) {
  DCHECK(media_task_runner_->BelongsToCurrentThread());
  renderer_extension_->NotifyFrameReleased(token);
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
  NOTIMPLEMENTED() << "Latency hint not supported in MediaFoundationRenderer";
}

void MediaFoundationRendererClient::Flush(base::OnceClosure flush_cb) {
  mojo_renderer_->Flush(std::move(flush_cb));
}

void MediaFoundationRendererClient::StartPlayingFrom(base::TimeDelta time) {
  DCHECK(media_task_runner_->BelongsToCurrentThread());
  SignalMediaPlayingStateChange(true);
  next_video_frame_.reset();
  mojo_renderer_->StartPlayingFrom(time);
  // Request the first frame (if we are not in frame server mode this just
  // gets dropped).
  base::TimeTicks request_min = base::TimeTicks::Now();
  base::TimeTicks request_max =
      base::TimeTicks::Now() + GetPreferredRenderInterval();

  renderer_extension_->RequestNextFrameBetweenTimestamps(request_min,
                                                         request_max);
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

  SignalMediaPlayingStateChange(false);
  // Do not call MediaFoundationRenderer::ReportErrorReason() since it should've
  // already been reported in MediaFoundationRenderer.
  client_->OnError(status);
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
  DCHECK(media_task_runner_->BelongsToCurrentThread());
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
    absl::optional<int> fps) {
  DVLOG_FUNC(1) << "fps=" << (fps ? *fps : -1);
  DCHECK(has_video_);

  if (fps.has_value()) {
    // We use microseconds as that is the max resolution of TimeDelta
    render_interval_ = base::Microseconds(1000000 / *fps);
  }

  client_->OnVideoFrameRateChange(fps);
}

// RenderCallback implementation.
scoped_refptr<media::VideoFrame> MediaFoundationRendererClient::Render(
    base::TimeTicks deadline_min,
    base::TimeTicks deadline_max,
    RenderingMode mode) {
  // Sends a frame request if in frame server mode, otherwise return nothing as
  // it is rendered independently by Windows Direct Composition.
  if (!media_engine_in_frame_server_mode_) {
    return nullptr;
  }

  base::TimeTicks next_request_min = deadline_max;
  base::TimeTicks next_request_max =
      deadline_max + GetPreferredRenderInterval();

  auto callback =
      [](base::TimeTicks deadline_min, base::TimeTicks deadline_max,
         base::WeakPtr<MediaFoundationRendererClient> renderer_client) {
        if (renderer_client.MaybeValid()) {
          renderer_client->renderer_extension_
              ->RequestNextFrameBetweenTimestamps(deadline_min, deadline_max);
        }
      };

  media_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(callback, next_request_min, next_request_max,
                                weak_factory_.GetWeakPtr()));

  // TODO(crbug.com/1298093): Need to report underflow when we don't have a
  // frame ready for presentation by calling OnBufferingStateChange

  return next_video_frame_;
}

void MediaFoundationRendererClient::OnFrameDropped() {
  // TODO(crbug.com/1298093): Need to notify when frames were not presented.
  return;
}

base::TimeDelta MediaFoundationRendererClient::GetPreferredRenderInterval() {
  return render_interval_;
}

// private

void MediaFoundationRendererClient::OnRemoteRendererInitialized(
    PipelineStatus status) {
  DVLOG_FUNC(1) << "status=" << status;
  DCHECK(media_task_runner_->BelongsToCurrentThread());
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
    std::move(init_cb_).Run(PIPELINE_ERROR_INITIALIZATION_FAILED);
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
  DCHECK(media_task_runner_->BelongsToCurrentThread());
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
  DCHECK(media_task_runner_->BelongsToCurrentThread());
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

  if (media_engine_in_frame_server_mode_) {
    return;
  }

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
    const absl::optional<base::UnguessableToken>& token,
    const std::string& error) {
  DVLOG_FUNC(1);
  DCHECK(has_video_);
  DCHECK(media_task_runner_->BelongsToCurrentThread());

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
  DCHECK(media_task_runner_->BelongsToCurrentThread());
  DCHECK(has_video_);

  if (!success) {
    MEDIA_LOG(ERROR, media_log_) << "Failed to set DCOMP surface handle";
    MediaFoundationRenderer::ReportErrorReason(
        MediaFoundationRenderer::ErrorReason::kOnDCompSurfaceHandleSetError);
    OnError(PIPELINE_ERROR_COULD_NOT_RENDER);
  }
}

void MediaFoundationRendererClient::OnVideoFrameCreated(
    scoped_refptr<VideoFrame> video_frame) {
  DVLOG_FUNC(1);
  DCHECK(media_task_runner_->BelongsToCurrentThread());
  DCHECK(has_video_);

  video_frame->metadata().allow_overlay = true;

  if (cdm_context_) {
    video_frame->metadata().protected_video = true;
  } else {
    video_frame->metadata().wants_promotion_hint = true;
  }

  dcomp_video_frame_ = video_frame;
  sink_->PaintSingleFrame(dcomp_video_frame_, true);
}

void MediaFoundationRendererClient::OnCdmAttached(bool success) {
  DCHECK(cdm_attached_cb_);
  std::move(cdm_attached_cb_).Run(success);
}

void MediaFoundationRendererClient::OnConnectionError() {
  DVLOG_FUNC(1);
  DCHECK(media_task_runner_->BelongsToCurrentThread());
  MEDIA_LOG(ERROR, media_log_) << "MediaFoundationRendererClient disconnected";
  MediaFoundationRenderer::ReportErrorReason(
      MediaFoundationRenderer::ErrorReason::kOnConnectionError);
  OnError(PIPELINE_ERROR_DISCONNECTED);
}

void MediaFoundationRendererClient::SignalMediaPlayingStateChange(
    bool is_playing) {
  // Skip if we are already in the same playing state
  if (is_playing == is_playing_) {
    return;
  }

  // Only start the render loop if we are in frame server mode
  if (media_engine_in_frame_server_mode_) {
    if (is_playing) {
      sink_->Start(this);
    } else {
      sink_->Stop();
    }
  }
  is_playing_ = is_playing;
}

}  // namespace media
