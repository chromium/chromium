// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/services/mojo_audio_decoder_service.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/logging.h"
#include "media/base/cdm_context.h"
#include "media/base/content_decryption_module.h"
#include "media/mojo/common/media_type_converters.h"
#include "media/mojo/common/mojo_decoder_buffer_converter.h"
#include "media/mojo/services/mojo_cdm_service_context.h"

namespace media {

MojoAudioDecoderService::MojoAudioDecoderService(
    MojoCdmServiceContext* mojo_cdm_service_context,
    std::unique_ptr<media::AudioDecoder> decoder)
    : mojo_cdm_service_context_(mojo_cdm_service_context),
      decoder_(std::move(decoder)) {
  DCHECK(mojo_cdm_service_context_);
  weak_this_ = weak_factory_.GetWeakPtr();
}

MojoAudioDecoderService::~MojoAudioDecoderService() = default;

void MojoAudioDecoderService::Construct(
    mojo::PendingAssociatedRemote<mojom::AudioDecoderClient> client) {
  DVLOG(1) << __func__;
  client_.Bind(std::move(client));
}

void MojoAudioDecoderService::Initialize(const AudioDecoderConfig& config,
                                         int32_t cdm_id,
                                         InitializeCallback callback) {
  DVLOG(1) << __func__ << " " << config.AsHumanReadableString();

  // Get CdmContext from |cdm_id|, which could be null.
  CdmContext* cdm_context = nullptr;
  if (cdm_id != CdmContext::kInvalidCdmId) {
    auto cdm_context_ref = mojo_cdm_service_context_->GetCdmContextRef(cdm_id);
    if (cdm_context_ref) {
      // |cdm_context_ref_| must be kept as long as |cdm_context| is used by the
      // |decoder_|.
      cdm_context_ref_ = std::move(cdm_context_ref);
      cdm_context = cdm_context_ref_->GetCdmContext();
      DCHECK(cdm_context);
    }
  }

  if (config.is_encrypted() && !cdm_context) {
    DVLOG(1) << "CdmContext for " << cdm_id << " not found for encrypted audio";
    OnInitialized(std::move(callback), false);
    return;
  }

  decoder_->Initialize(
      config, cdm_context,
      base::Bind(&MojoAudioDecoderService::OnInitialized, weak_this_,
                 base::Passed(&callback)),
      base::Bind(&MojoAudioDecoderService::OnAudioBufferReady, weak_this_),
      base::Bind(&MojoAudioDecoderService::OnWaiting, weak_this_));
}

void MojoAudioDecoderService::SetDataSource(
    mojo::ScopedDataPipeConsumerHandle receive_pipe) {
  DVLOG(1) << __func__;

  mojo_decoder_buffer_reader_.reset(
      new MojoDecoderBufferReader(std::move(receive_pipe)));
}

void MojoAudioDecoderService::Decode(mojom::DecoderBufferPtr buffer,
                                     DecodeCallback callback) {
  DVLOG(3) << __func__;
  mojo_decoder_buffer_reader_->ReadDecoderBuffer(
      std::move(buffer), base::BindOnce(&MojoAudioDecoderService::OnReadDone,
                                        weak_this_, std::move(callback)));
}

void MojoAudioDecoderService::Reset(ResetCallback callback) {
  DVLOG(1) << __func__;

  // Reset the reader so that pending decodes will be dispatches first.
  mojo_decoder_buffer_reader_->Flush(
      base::Bind(&MojoAudioDecoderService::OnReaderFlushDone, weak_this_,
                 base::Passed(&callback)));
}

void MojoAudioDecoderService::OnInitialized(InitializeCallback callback,
                                            bool success) {
  DVLOG(1) << __func__ << " success:" << success;

  if (!success) {
    cdm_context_ref_.reset();
    // Do not call decoder_->NeedsBitstreamConversion() if init failed.
    std::move(callback).Run(false, false);
    return;
  }

  std::move(callback).Run(success, decoder_->NeedsBitstreamConversion());
}

// The following methods are needed so that we can bind them with a weak pointer
// to avoid running the |callback| after connection error happens and |this| is
// deleted. It's not safe to run the |callback| after a connection error.

void MojoAudioDecoderService::OnReadDone(DecodeCallback callback,
                                         scoped_refptr<DecoderBuffer> buffer) {
  DVLOG(3) << __func__ << " success:" << !!buffer;

  if (!buffer) {
    std::move(callback).Run(DecodeStatus::DECODE_ERROR);
    return;
  }

  decoder_->Decode(buffer, base::Bind(&MojoAudioDecoderService::OnDecodeStatus,
                                      weak_this_, base::Passed(&callback)));
}

void MojoAudioDecoderService::OnReaderFlushDone(ResetCallback callback) {
  decoder_->Reset(base::Bind(&MojoAudioDecoderService::OnResetDone, weak_this_,
                             base::Passed(&callback)));
}

void MojoAudioDecoderService::OnDecodeStatus(DecodeCallback callback,
                                             media::DecodeStatus status) {
  DVLOG(3) << __func__ << " status:" << status;
  std::move(callback).Run(status);
}

void MojoAudioDecoderService::OnResetDone(ResetCallback callback) {
  DVLOG(1) << __func__;
  std::move(callback).Run();
}

void MojoAudioDecoderService::OnAudioBufferReady(
    scoped_refptr<AudioBuffer> audio_buffer) {
  DVLOG(1) << __func__;

  // TODO(timav): Use DataPipe.
  client_->OnBufferDecoded(mojom::AudioBuffer::From(*audio_buffer));
}

void MojoAudioDecoderService::OnWaiting(WaitingReason reason) {
  DVLOG(1) << __func__;
  client_->OnWaiting(reason);
}

}  // namespace media
