// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/services/mojo_renderer_service.h"

#include <utility>

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/optional.h"
#include "media/base/cdm_context.h"
#include "media/base/media_url_demuxer.h"
#include "media/base/renderer.h"
#include "media/mojo/common/media_type_converters.h"
#include "media/mojo/services/media_resource_shim.h"
#include "media/mojo/services/mojo_cdm_service_context.h"

namespace media {

namespace {

void CloseReceiverOnBadMessage(
    mojo::SelfOwnedReceiverRef<mojom::Renderer> receiver) {
  LOG(ERROR) << __func__;
  DCHECK(receiver);
  receiver->Close();
}

}  // namespace

// Time interval to update media time.
const int kTimeUpdateIntervalMs = 50;

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

  service->set_bad_message_cb(
      base::Bind(&CloseReceiverOnBadMessage, self_owned_receiver));

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
    base::Optional<std::vector<mojo::PendingRemote<mojom::DemuxerStream>>>
        streams,
    mojom::MediaUrlParamsPtr media_url_params,
    InitializeCallback callback) {
  DVLOG(1) << __func__;
  DCHECK_EQ(state_, STATE_UNINITIALIZED);

  client_.Bind(std::move(client));
  state_ = STATE_INITIALIZING;

  if (!media_url_params) {
    DCHECK(streams.has_value());
    media_resource_.reset(new MediaResourceShim(
        std::move(*streams), base::Bind(&MojoRendererService::OnStreamReady,
                                        weak_this_, base::Passed(&callback))));
    return;
  }

  DCHECK(!media_url_params->media_url.is_empty());
  media_resource_.reset(new MediaUrlDemuxer(
      nullptr, media_url_params->media_url, media_url_params->site_for_cookies,
      media_url_params->top_frame_origin, media_url_params->allow_credentials,
      media_url_params->is_hls));
  renderer_->Initialize(
      media_resource_.get(), this,
      base::BindOnce(&MojoRendererService::OnRendererInitializeDone, weak_this_,
                     base::Passed(&callback)));
}

void MojoRendererService::Flush(FlushCallback callback) {
  DVLOG(2) << __func__;
  DCHECK_EQ(state_, STATE_PLAYING);

  state_ = STATE_FLUSHING;
  CancelPeriodicMediaTimeUpdates();
  renderer_->Flush(base::BindOnce(&MojoRendererService::OnFlushCompleted,
                                  weak_this_, base::Passed(&callback)));
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

void MojoRendererService::SetCdm(int32_t cdm_id, SetCdmCallback callback) {
  if (!mojo_cdm_service_context_) {
    DVLOG(1) << "CDM service context not available.";
    std::move(callback).Run(false);
    return;
  }

  auto cdm_context_ref = mojo_cdm_service_context_->GetCdmContextRef(cdm_id);
  if (!cdm_context_ref) {
    DVLOG(1) << "CdmContextRef not found for CDM ID: " << cdm_id;
    std::move(callback).Run(false);
    return;
  }

  // |cdm_context_ref_| must be kept as long as |cdm_context| is used by the
  // |renderer_|.
  cdm_context_ref_ = std::move(cdm_context_ref);
  auto* cdm_context = cdm_context_ref_->GetCdmContext();
  DCHECK(cdm_context);

  renderer_->SetCdm(cdm_context,
                    base::Bind(&MojoRendererService::OnCdmAttached, weak_this_,
                               base::Passed(&callback)));
}

void MojoRendererService::OnError(PipelineStatus error) {
  DVLOG(1) << __func__ << "(" << error << ")";
  state_ = STATE_ERROR;
  client_->OnError();
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

void MojoRendererService::OnStreamReady(
    base::OnceCallback<void(bool)> callback) {
  DCHECK_EQ(state_, STATE_INITIALIZING);

  renderer_->Initialize(
      media_resource_.get(), this,
      base::BindOnce(&MojoRendererService::OnRendererInitializeDone, weak_this_,
                     base::Passed(&callback)));
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
    max_time += base::TimeDelta::FromMilliseconds(2 * kTimeUpdateIntervalMs);

  client_->OnTimeUpdate(media_time, max_time, base::TimeTicks::Now());
  last_media_time_ = media_time;
}

void MojoRendererService::CancelPeriodicMediaTimeUpdates() {
  DVLOG(2) << __func__;

  time_update_timer_.Stop();
  UpdateMediaTime(false);
}

void MojoRendererService::SchedulePeriodicMediaTimeUpdates() {
  DVLOG(2) << __func__;

  UpdateMediaTime(true);
  time_update_timer_.Start(
      FROM_HERE, base::TimeDelta::FromMilliseconds(kTimeUpdateIntervalMs),
      base::Bind(&MojoRendererService::UpdateMediaTime, weak_this_, false));
}

void MojoRendererService::OnFlushCompleted(FlushCallback callback) {
  DVLOG(1) << __func__;
  DCHECK_EQ(state_, STATE_FLUSHING);
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
