// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cast/receiver/cast_streaming_renderer.h"

#include "base/notreached.h"

namespace media {
namespace cast {

CastStreamingRenderer::CastStreamingRenderer(
    std::unique_ptr<Renderer> renderer,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    mojo::PendingReceiver<media::mojom::Renderer> pending_renderer_controls)
    : real_renderer_(std::move(renderer)),
      pending_renderer_controls_(std::move(pending_renderer_controls)),
      task_runner_(std::move(task_runner)),
      weak_factory_(this) {
  DCHECK(real_renderer_);
  DCHECK(pending_renderer_controls_);
}

CastStreamingRenderer::~CastStreamingRenderer() = default;

void CastStreamingRenderer::Initialize(MediaResource* media_resource,
                                       RendererClient* client,
                                       PipelineStatusCallback init_cb) {
  DCHECK(!init_cb_);

  init_cb_ = std::move(init_cb);
  real_renderer_->Initialize(
      media_resource, client,
      base::BindOnce(
          &CastStreamingRenderer::OnRealRendererInitializationComplete,
          weak_factory_.GetWeakPtr()));
}

void CastStreamingRenderer::SetCdm(CdmContext* cdm_context,
                                   CdmAttachedCB cdm_attached_cb) {
  NOTREACHED();
}

void CastStreamingRenderer::SetLatencyHint(
    absl::optional<base::TimeDelta> latency_hint) {}

void CastStreamingRenderer::Flush(base::OnceClosure flush_cb) {}

void CastStreamingRenderer::StartPlayingFrom(base::TimeDelta time) {
  DCHECK(task_runner_->BelongsToCurrentThread());
}

void CastStreamingRenderer::SetPlaybackRate(double playback_rate) {
  DCHECK(task_runner_->BelongsToCurrentThread());
}

void CastStreamingRenderer::SetVolume(float volume) {}

base::TimeDelta CastStreamingRenderer::GetMediaTime() {
  return real_renderer_->GetMediaTime();
}

void CastStreamingRenderer::OnRealRendererInitializationComplete(
    PipelineStatus status) {
  DCHECK(init_cb_);
  DCHECK(!playback_controller_);

  playback_controller_ = std::make_unique<PlaybackController>(
      std::move(pending_renderer_controls_), task_runner_,
      real_renderer_.get());

  std::move(init_cb_).Run(status);
}

// NOTE: The mojo pipe CANNOT be bound to task runner |task_runner_| - this will
// result in runtime errors on the release branch due to an outdated mojo
// implementation. Calls must instead be bounced to the correct task runner in
// each receiver method.
// TODO(b/205307190): Bind the mojo pipe to the task runner directly.
CastStreamingRenderer::PlaybackController::PlaybackController(
    mojo::PendingReceiver<media::mojom::Renderer> pending_renderer_controls,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    media::Renderer* real_renderer)
    : real_renderer_(real_renderer),
      task_runner_(std::move(task_runner)),
      playback_controller_(this, std::move(pending_renderer_controls)),
      weak_factory_(this) {
  DCHECK(real_renderer_);
  DCHECK(task_runner_);
}

CastStreamingRenderer::PlaybackController::~PlaybackController() = default;

void CastStreamingRenderer::PlaybackController::Initialize(
    ::mojo::PendingAssociatedRemote<media::mojom::RendererClient> client,
    absl::optional<
        std::vector<::mojo::PendingRemote<::media::mojom::DemuxerStream>>>
        streams,
    mojom::MediaUrlParamsPtr media_url_params,
    InitializeCallback callback) {
  NOTIMPLEMENTED();
  std::move(callback).Run(false);
}

void CastStreamingRenderer::PlaybackController::Flush(FlushCallback callback) {
  NOTIMPLEMENTED();
  std::move(callback).Run();
}

void CastStreamingRenderer::PlaybackController::StartPlayingFrom(
    ::base::TimeDelta time) {
  if (!task_runner_->BelongsToCurrentThread()) {
    task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(
            &CastStreamingRenderer::PlaybackController::StartPlayingFrom,
            weak_factory_.GetWeakPtr(), time));
    return;
  }

  real_renderer_->StartPlayingFrom(time);
}

void CastStreamingRenderer::PlaybackController::SetPlaybackRate(
    double playback_rate) {
  if (!task_runner_->BelongsToCurrentThread()) {
    task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(
            &CastStreamingRenderer::PlaybackController::SetPlaybackRate,
            weak_factory_.GetWeakPtr(), playback_rate));
    return;
  }

  real_renderer_->SetPlaybackRate(playback_rate);
}

void CastStreamingRenderer::PlaybackController::SetVolume(float volume) {
  NOTIMPLEMENTED();
}

void CastStreamingRenderer::PlaybackController::SetCdm(
    const absl::optional<::base::UnguessableToken>& cdm_id,
    SetCdmCallback callback) {
  NOTIMPLEMENTED();
  std::move(callback).Run(false);
}

}  // namespace cast
}  // namespace media
