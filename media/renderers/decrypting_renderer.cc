// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/renderers/decrypting_renderer.h"

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/task/sequenced_task_runner.h"
#include "media/base/cdm_context.h"
#include "media/base/demuxer_stream.h"
#include "media/base/media_log.h"
#include "media/base/media_resource.h"
#include "media/base/renderer_client.h"
#include "media/filters/decrypting_demuxer_stream.h"
#include "media/filters/decrypting_media_resource.h"

namespace media {

DecryptingRenderer::DecryptingRenderer(
    std::unique_ptr<Renderer> renderer,
    MediaLog* media_log,
    const scoped_refptr<base::SequencedTaskRunner> media_task_runner)
    : renderer_(std::move(renderer)),
      media_log_(media_log),
      media_task_runner_(media_task_runner),
      client_(nullptr),
      media_resource_(nullptr),
      decrypting_media_resource_(nullptr) {
  DCHECK(renderer_);
}

DecryptingRenderer::~DecryptingRenderer() {}

// The behavior of Initialize():
//
// Streams    CdmContext    Action
// ---------------------------------------------------------------------
// Clear      nullptr       InitializeRenderer()
// Clear      AesDecryptor  CreateAndInitializeDecryptingMediaResource()
// Clear      Other         InitializeRenderer()
// Encrypted  nullptr       Wait
// Encrypted  AesDecryptor  CreateAndInitializeDecryptingMediaResource()
// Encrypted  Other         InitializeRenderer()
void DecryptingRenderer::Initialize(MediaResource* media_resource,
                                    RendererClient* client,
                                    PipelineStatusCallback init_cb) {
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());
  DCHECK(media_resource);
  DCHECK(client);

  // Using |this| with a MediaResource::Type::URL will result in a crash.
  DCHECK_EQ(media_resource->GetType(), MediaResource::Type::kStream);

  media_resource_ = media_resource;
  client_ = client;
  init_cb_ = std::move(init_cb);

  bool has_encrypted_stream = HasEncryptedStream();

  // If we do not have a valid |cdm_context_| and there are encrypted streams we
  // need to wait.
  if (!cdm_context_ && has_encrypted_stream) {
    waiting_for_cdm_ = true;
    return;
  }

  if (cdm_context_ && cdm_context_->GetDecryptor() &&
      cdm_context_->GetDecryptor()->CanAlwaysDecrypt()) {
    CreateAndInitializeDecryptingMediaResource();
    return;
  }

  InitializeRenderer(true);
}

void DecryptingRenderer::SetCdm(CdmContext* cdm_context,
                                CdmAttachedCB cdm_attached_cb) {
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());

  if (cdm_context_) {
    DVLOG(1) << "Switching CDM not supported.";
    std::move(cdm_attached_cb).Run(false);
    return;
  }

  cdm_context_ = cdm_context;

  // If we are using an AesDecryptor all decryption will be handled by the
  // DecryptingMediaResource instead of the renderer implementation.
  if (cdm_context_->GetDecryptor() &&
      cdm_context_->GetDecryptor()->CanAlwaysDecrypt()) {
    // If Initialize() was invoked prior to this function then
    // |waiting_for_cdm_| will be true (if we reached this branch). In this
    // scenario we want to initialize the DecryptingMediaResource here.
    if (waiting_for_cdm_)
      CreateAndInitializeDecryptingMediaResource();
    std::move(cdm_attached_cb).Run(true);
    return;
  }

  renderer_->SetCdm(cdm_context_, std::move(cdm_attached_cb));

  // We only want to initialize the renderer if we were waiting for the
  // CdmContext, otherwise it will already have been initialized.
  if (waiting_for_cdm_)
    InitializeRenderer(true);
}

void DecryptingRenderer::SetLatencyHint(
    std::optional<base::TimeDelta> latency_hint) {
  renderer_->SetLatencyHint(latency_hint);
}

void DecryptingRenderer::SetPreservesPitch(bool preserves_pitch) {
  renderer_->SetPreservesPitch(preserves_pitch);
}

void DecryptingRenderer::SetWasPlayedWithUserActivationAndHighMediaEngagement(
    bool was_played_with_user_activation_and_high_media_engagement) {
  renderer_->SetWasPlayedWithUserActivationAndHighMediaEngagement(
      was_played_with_user_activation_and_high_media_engagement);
}

void DecryptingRenderer::Flush(base::OnceClosure flush_cb) {
  renderer_->Flush(std::move(flush_cb));
}

void DecryptingRenderer::StartPlayingFrom(base::TimeDelta time) {
  renderer_->StartPlayingFrom(time);
}

void DecryptingRenderer::SetPlaybackRate(double playback_rate) {
  renderer_->SetPlaybackRate(playback_rate);
}

void DecryptingRenderer::SetVolume(float volume) {
  renderer_->SetVolume(volume);
}

base::TimeDelta DecryptingRenderer::GetMediaTime() {
  return renderer_->GetMediaTime();
}

void DecryptingRenderer::OnSelectedVideoTracksChanged(
    const std::vector<DemuxerStream*>& enabled_tracks,
    base::OnceClosure change_completed_cb) {
  renderer_->OnSelectedVideoTracksChanged(enabled_tracks,
                                          std::move(change_completed_cb));
}

void DecryptingRenderer::OnEnabledAudioTracksChanged(
    const std::vector<DemuxerStream*>& enabled_tracks,
    base::OnceClosure change_completed_cb) {
  renderer_->OnEnabledAudioTracksChanged(enabled_tracks,
                                         std::move(change_completed_cb));
}

RendererType DecryptingRenderer::GetRendererType() {
  // DecryptingRenderer is a thin wrapping layer; return the underlying type.
  return renderer_->GetRendererType();
}

void DecryptingRenderer::CreateAndInitializeDecryptingMediaResource() {
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());
  DCHECK(init_cb_);

  decrypting_media_resource_ = std::make_unique<DecryptingMediaResource>(
      media_resource_, cdm_context_, media_log_, media_task_runner_);
  decrypting_media_resource_->Initialize(
      base::BindOnce(&DecryptingRenderer::InitializeRenderer,
                     weak_factory_.GetWeakPtr()),
      base::BindRepeating(&DecryptingRenderer::OnWaiting,
                          weak_factory_.GetWeakPtr()));
}

void DecryptingRenderer::InitializeRenderer(bool success) {
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());

  if (!success) {
    std::move(init_cb_).Run(PIPELINE_ERROR_INITIALIZATION_FAILED);
    return;
  }

  // |decrypting_media_resource_| when |cdm_context_| is null and there are no
  // encrypted streams.
  MediaResource* const maybe_decrypting_media_resource =
      decrypting_media_resource_ ? decrypting_media_resource_.get()
                                 : media_resource_.get();
  renderer_->Initialize(maybe_decrypting_media_resource, client_,
                        std::move(init_cb_));
}

bool DecryptingRenderer::HasEncryptedStream() {
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());

  for (media::DemuxerStream* stream : media_resource_->GetAllStreams()) {
    if ((stream->type() == DemuxerStream::AUDIO &&
         stream->audio_decoder_config().is_encrypted()) ||
        (stream->type() == DemuxerStream::VIDEO &&
         stream->video_decoder_config().is_encrypted())) {
      return true;
    }
  }

  return false;
}

bool DecryptingRenderer::HasDecryptingMediaResourceForTesting() const {
  return decrypting_media_resource_ != nullptr;
}

void DecryptingRenderer::OnWaiting(WaitingReason reason) {
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());
  client_->OnWaiting(reason);
}

}  // namespace media
