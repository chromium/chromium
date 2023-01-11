// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/android/media_player_renderer_client.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"

namespace content {

MediaPlayerRendererClient::MediaPlayerRendererClient(
    mojo::PendingRemote<RendererExtention> renderer_extension_remote,
    mojo::PendingReceiver<ClientExtention> client_extension_receiver,
    scoped_refptr<base::SequencedTaskRunner> media_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> compositor_task_runner,
    std::unique_ptr<media::MojoRenderer> mojo_renderer,
    media::ScopedStreamTextureWrapper stream_texture_wrapper,
    media::VideoRendererSink* sink)
    : MojoRendererWrapper(std::move(mojo_renderer)),
      stream_texture_wrapper_(std::move(stream_texture_wrapper)),
      client_(nullptr),
      sink_(sink),
      media_task_runner_(std::move(media_task_runner)),
      compositor_task_runner_(std::move(compositor_task_runner)),
      delayed_bind_client_extension_receiver_(
          std::move(client_extension_receiver)),
      delayed_bind_renderer_extention_remote_(
          std::move(renderer_extension_remote)) {}

MediaPlayerRendererClient::~MediaPlayerRendererClient() {
  // Clearing the STW's callback into |this| must happen first. Otherwise, the
  // underlying StreamTextureProxy can callback into OnFrameAvailable() on the
  // |compositor_task_runner_|, while we are destroying |this|.
  // See https://crbug.com/688466.
  stream_texture_wrapper_->ClearReceivedFrameCBOnAnyThread();
}

void MediaPlayerRendererClient::Initialize(
    media::MediaResource* media_resource,
    media::RendererClient* client,
    media::PipelineStatusCallback init_cb) {
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());
  DCHECK(!init_cb_);

  // Consume and bind the delayed PendingRemote and PendingReceiver now that we
  // are on |media_task_runner_|.
  renderer_extension_remote_.Bind(
      std::move(delayed_bind_renderer_extention_remote_), media_task_runner_);
  client_extension_receiver_.Bind(
      std::move(delayed_bind_client_extension_receiver_), media_task_runner_);

  media_resource_ = media_resource;
  client_ = client;
  init_cb_ = std::move(init_cb);

  // Unretained is safe here because |stream_texture_wrapper_| resets the
  // Closure it has before destroying itself on |compositor_task_runner_|,
  // and |this| is garanteed to live until the Closure has been reset.
  stream_texture_wrapper_->Initialize(
      base::BindRepeating(&MediaPlayerRendererClient::OnFrameAvailable,
                          base::Unretained(this)),
      compositor_task_runner_,
      base::BindOnce(
          &MediaPlayerRendererClient::OnStreamTextureWrapperInitialized,
          weak_factory_.GetWeakPtr(), media_resource));
}

media::RendererType MediaPlayerRendererClient::GetRendererType() {
  return media::RendererType::kMediaPlayer;
}

void MediaPlayerRendererClient::OnStreamTextureWrapperInitialized(
    media::MediaResource* media_resource,
    bool success) {
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());
  if (!success) {
    std::move(init_cb_).Run(
        media::PipelineStatus::Codes::PIPELINE_ERROR_INITIALIZATION_FAILED);
    return;
  }

  MojoRendererWrapper::Initialize(
      media_resource, client_,
      base::BindOnce(&MediaPlayerRendererClient::OnRemoteRendererInitialized,
                     weak_factory_.GetWeakPtr()));
}

void MediaPlayerRendererClient::OnScopedSurfaceRequested(
    const base::UnguessableToken& request_token) {
  if (request_token == base::UnguessableToken::Null()) {
    client_->OnError(media::PIPELINE_ERROR_INITIALIZATION_FAILED);
    return;
  }

  stream_texture_wrapper_->ForwardStreamTextureForSurfaceRequest(request_token);
}

void MediaPlayerRendererClient::OnRemoteRendererInitialized(
    media::PipelineStatus status) {
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());
  DCHECK(!init_cb_.is_null());

  if (status == media::PIPELINE_OK) {
    // TODO(tguilbert): Measure and smooth out the initialization's ordering to
    // have the lowest total initialization time.
    renderer_extension_remote_->InitiateScopedSurfaceRequest(
        base::BindOnce(&MediaPlayerRendererClient::OnScopedSurfaceRequested,
                       weak_factory_.GetWeakPtr()));

    // Signal that we're using MediaPlayer so that we can properly differentiate
    // within our metrics.
    media::PipelineStatistics stats;
    stats.video_pipeline_info = {true, false,
                                 media::VideoDecoderType::kMediaCodec};
    stats.audio_pipeline_info = {true, false,
                                 media::AudioDecoderType::kMediaCodec};
    client_->OnStatisticsUpdate(stats);
  }
  std::move(init_cb_).Run(status);
}

void MediaPlayerRendererClient::OnFrameAvailable() {
  DCHECK(compositor_task_runner_->BelongsToCurrentThread());

  // The frame generated by the StreamTextureWrapper is "static", i.e., even as
  // new frames are drawn it does not change. Downstream components expect that
  // each new VideoFrame will have a different unique_id() when it changes, so
  // we need to add a wrapping frame with a new unique_id().
  auto frame = stream_texture_wrapper_->GetCurrentFrame();
  auto unique_frame = media::VideoFrame::WrapVideoFrame(
      frame, frame->format(), frame->visible_rect(), frame->natural_size());
  sink_->PaintSingleFrame(std::move(unique_frame));
}

void MediaPlayerRendererClient::OnVideoSizeChange(const gfx::Size& size) {
  stream_texture_wrapper_->UpdateTextureSize(size);
  client_->OnVideoNaturalSizeChange(size);
}

void MediaPlayerRendererClient::OnDurationChange(base::TimeDelta duration) {
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());

  media_resource_->ForwardDurationChangeToDemuxerHost(duration);
}

}  // namespace content
