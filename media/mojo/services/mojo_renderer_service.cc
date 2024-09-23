// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/services/mojo_renderer_service.h"

#include <memory>
#include <optional>
#include <utility>

#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/time/time.h"
#include "media/base/cdm_context.h"
#include "media/base/media_url_demuxer.h"
#include "media/base/renderer.h"
#include "media/mojo/common/media_type_converters.h"
#include "media/mojo/services/media_resource_shim.h"
#include "media/mojo/services/mojo_cdm_service_context.h"

namespace media {

// Time interval to update media time.
constexpr auto kTimeUpdateInterval = base::Milliseconds(125);

// static
mojo::SelfOwnedReceiverRef<mojom::Renderer> MojoRendererService::Create(
    MojoCdmServiceContext* mojo_cdm_service_context,
    std::unique_ptr<media::Renderer> renderer,
    mojo::PendingReceiver<mojom::Renderer> receiver) {
  MojoRendererService* service =
      new MojoRendererService(mojo_cdm_service_context, std::move(renderer));

  mojo::SelfOwnedReceiverRef<mojom::Renderer> self_owned_receiver =
      mojo::MakeSelfOwnedReceiver<mojom::Renderer>(base::WrapUnique(service),
                                                   std::move(receiver));

  return self_owned_receiver;
}

MojoRendererService::MojoRendererService(
    MojoCdmServiceContext* mojo_cdm_service_context,
    std::unique_ptr<media::Renderer> renderer)
    : mojo_cdm_service_context_(mojo_cdm_service_context),
      state_(STATE_UNINITIALIZED),
      playback_rate_(0),
      renderer_(std::move(renderer)) {
  DVLOG(1) << __func__;
  DCHECK(renderer_);

  weak_this_ = weak_factory_.GetWeakPtr();
}

MojoRendererService::~MojoRendererService() = default;

void MojoRendererService::Initialize(
    mojo::PendingAssociatedRemote<mojom::RendererClient> client,
    std::optional<std::vector<mojo::PendingRemote<mojom::DemuxerStream>>>
        streams,
    mojom::MediaUrlParamsPtr media_url_params,
    InitializeCallback callback) {
  DVLOG(1) << __func__;
  DCHECK_EQ(state_, STATE_UNINITIALIZED);

  client_.Bind(std::move(client));
  state_ = STATE_INITIALIZING;

  if (!media_url_params) {
    DCHECK(streams.has_value());
    media_resource_ = std::make_unique<MediaResourceShim>(
        std::move(*streams),
        base::BindOnce(&MojoRendererService::OnAllStreamsReady, weak_this_,
                       std::move(callback)));
    return;
  }

  DCHECK(!media_url_params->media_url.is_empty());
  media_resource_ = std::make_unique<MediaUrlDemuxer>(
      nullptr, media_url_params->media_url, media_url_params->site_for_cookies,
      media_url_params->top_frame_origin,
      media_url_params->storage_access_api_status,
      media_url_params->allow_credentials, media_url_params->is_hls);
  media_resource_->SetHeaders(std::move(media_url_params->headers));
  renderer_->Initialize(
      media_resource_.get(), this,
      base::BindOnce(&MojoRendererService::OnRendererInitializeDone, weak_this_,
                     std::move(callback)));
}

void MojoRendererService::Flush(FlushCallback callback) {
  DVLOG(2) << __func__;
  DCHECK(state_ == STATE_PLAYING || state_ == STATE_ERROR);

  if (state_ == STATE_ERROR) {
    std::move(callback).Run();
    return;
  }

  state_ = STATE_FLUSHING;
  CancelPeriodicMediaTimeUpdates();
  renderer_->Flush(base::BindOnce(&MojoRendererService::OnFlushCompleted,
                                  weak_this_, std::move(callback)));
}

void MojoRendererService::StartPlayingFrom(base::TimeDelta time_delta) {
  DVLOG(2) << __func__ << ": " << time_delta;
  renderer_->StartPlayingFrom(time_delta);
  SchedulePeriodicMediaTimeUpdates();
}

void MojoRendererService::SetPlaybackRate(double playback_rate) {
  DVLOG(2) << __func__ << ": " << playback_rate;
  DCHECK(state_ == STATE_PLAYING || state_ == STATE_ERROR);
  playback_rate_ = playback_rate;
  renderer_->SetPlaybackRate(playback_rate);
}

void MojoRendererService::SetVolume(float volume) {
  renderer_->SetVolume(volume);
}

void MojoRendererService::SetCdm(
    const std::optional<base::UnguessableToken>& cdm_id,
    SetCdmCallback callback) {
  if (cdm_context_ref_) {
    DVLOG(1) << "Switching CDM not supported";
    std::move(callback).Run(false);
    return;
  }

  if (!mojo_cdm_service_context_) {
    DVLOG(1) << "CDM service context not available.";
    std::move(callback).Run(false);
    return;
  }

  if (!cdm_id) {
    DVLOG(1) << "The CDM ID is invalid.";
    std::move(callback).Run(false);
    return;
  }

  auto cdm_context_ref =
      mojo_cdm_service_context_->GetCdmContextRef(cdm_id.value());
  if (!cdm_context_ref) {
    DVLOG(1) << "CdmContextRef not found for CDM ID: " << cdm_id.value();
    std::move(callback).Run(false);
    return;
  }

  // |cdm_context_ref_| must be kept as long as |cdm_context| is used by the
  // |renderer_|.
  cdm_context_ref_ = std::move(cdm_context_ref);
  auto* cdm_context = cdm_context_ref_->GetCdmContext();
  DCHECK(cdm_context);

  renderer_->SetCdm(cdm_context,
                    base::BindOnce(&MojoRendererService::OnCdmAttached,
                                   weak_this_, std::move(callback)));
}

void MojoRendererService::SetLatencyHint(
    std::optional<base::TimeDelta> latency_hint) {
  if (latency_hint.has_value() && latency_hint->is_negative()) {
    mojo::ReportBadMessage("Latency hint should be non-negative");
    return;
  }
  renderer_->SetLatencyHint(latency_hint);
}

void MojoRendererService::OnError(PipelineStatus error) {
  DVLOG(1) << __func__ << "(" << error << ")";
  state_ = STATE_ERROR;
  CancelPeriodicMediaTimeUpdates();
  client_->OnError(std::move(error));
}

void MojoRendererService::OnFallback(PipelineStatus error) {
  NOTREACHED_IN_MIGRATION();
}

void MojoRendererService::OnEnded() {
  DVLOG(1) << __func__;
  CancelPeriodicMediaTimeUpdates();
  client_->OnEnded();
}

void MojoRendererService::OnStatisticsUpdate(const PipelineStatistics& stats) {
  DVLOG(3) << __func__;
  client_->OnStatisticsUpdate(stats);
}

void MojoRendererService::OnBufferingStateChange(
    BufferingState state,
    BufferingStateChangeReason reason) {
  DVLOG(2) << __func__ << "(" << state << ", " << reason << ")";
  client_->OnBufferingStateChange(state, reason);
}

void MojoRendererService::OnWaiting(WaitingReason reason) {
  DVLOG(1) << __func__;
  client_->OnWaiting(reason);
}

void MojoRendererService::OnAudioConfigChange(
    const AudioDecoderConfig& config) {
  DVLOG(2) << __func__ << "(" << config.AsHumanReadableString() << ")";
  client_->OnAudioConfigChange(config);
}

void MojoRendererService::OnVideoConfigChange(
    const VideoDecoderConfig& config) {
  DVLOG(2) << __func__ << "(" << config.AsHumanReadableString() << ")";
  client_->OnVideoConfigChange(config);
}

void MojoRendererService::OnVideoNaturalSizeChange(const gfx::Size& size) {
  DVLOG(2) << __func__ << "(" << size.ToString() << ")";
  client_->OnVideoNaturalSizeChange(size);
}

void MojoRendererService::OnVideoOpacityChange(bool opaque) {
  DVLOG(2) << __func__ << "(" << opaque << ")";
  client_->OnVideoOpacityChange(opaque);
}

void MojoRendererService::OnVideoFrameRateChange(std::optional<int> fps) {
  DVLOG(2) << __func__ << "(" << (fps ? *fps : -1) << ")";
  // TODO(liberato): plumb to |client_|.
}

void MojoRendererService::OnAllStreamsReady(
    base::OnceCallback<void(bool)> callback) {
  DCHECK_EQ(state_, STATE_INITIALIZING);

  renderer_->Initialize(
      media_resource_.get(), this,
      base::BindOnce(&MojoRendererService::OnRendererInitializeDone, weak_this_,
                     std::move(callback)));
}

void MojoRendererService::OnRendererInitializeDone(
    base::OnceCallback<void(bool)> callback,
    PipelineStatus status) {
  DVLOG(1) << __func__;
  DCHECK_EQ(state_, STATE_INITIALIZING);

  if (status != PIPELINE_OK) {
    state_ = STATE_ERROR;
    std::move(callback).Run(false);
    return;
  }

  state_ = STATE_PLAYING;
  std::move(callback).Run(true);
}

void MojoRendererService::UpdateMediaTime(bool force) {
  const base::TimeDelta media_time = renderer_->GetMediaTime();
  if (!force && media_time == last_media_time_)
    return;

  base::TimeDelta max_time = media_time;
  // Allow some slop to account for delays in scheduling time update tasks.
  if (time_update_timer_.IsRunning() && (playback_rate_ > 0))
    max_time += 2 * kTimeUpdateInterval;

  client_->OnTimeUpdate(media_time, max_time, base::TimeTicks::Now());
  last_media_time_ = media_time;
}

void MojoRendererService::CancelPeriodicMediaTimeUpdates() {
  DVLOG(2) << __func__;

  time_update_timer_.Stop();
  UpdateMediaTime(/*force=*/false);
}

void MojoRendererService::SchedulePeriodicMediaTimeUpdates() {
  DVLOG(2) << __func__;

  UpdateMediaTime(/*force=*/true);
  time_update_timer_.Start(
      FROM_HERE, kTimeUpdateInterval,
      base::BindRepeating(&MojoRendererService::UpdateMediaTime, weak_this_,
                          /*force=*/false));
}

void MojoRendererService::OnFlushCompleted(FlushCallback callback) {
  DVLOG(1) << __func__;
  DCHECK(state_ == STATE_FLUSHING || state_ == STATE_ERROR);

  if (state_ == STATE_FLUSHING)
    state_ = STATE_PLAYING;

  std::move(callback).Run();
}

void MojoRendererService::OnCdmAttached(base::OnceCallback<void(bool)> callback,
                                        bool success) {
  DVLOG(1) << __func__ << "(" << success << ")";

  if (!success)
    cdm_context_ref_.reset();

  std::move(callback).Run(success);
}
}  // namespace media
