// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/services/mojo_audio_decoder_service.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/types/optional_util.h"
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

void MojoAudioDecoderService::Initialize(
    const AudioDecoderConfig& config,
    const absl::optional<base::UnguessableToken>& cdm_id,
    InitializeCallback callback) {
  DVLOG(1) << __func__ << " " << config.AsHumanReadableString();

  // |cdm_context_ref_| must be kept as long as |cdm_context| is used by the
  // |decoder_|. We do NOT support resetting |cdm_context_ref_| because in
  // general we don't support resetting CDM in the media pipeline.
  if (cdm_id) {
    if (!cdm_id_) {
      DCHECK(!cdm_context_ref_);
      cdm_id_ = cdm_id;
      cdm_context_ref_ =
          mojo_cdm_service_context_->GetCdmContextRef(cdm_id.value());
    } else if (cdm_id != cdm_id_) {
      // TODO(xhwang): Replace with mojo::ReportBadMessage().
      NOTREACHED() << "The caller should not switch CDM";
      OnInitialized(std::move(callback),
                    DecoderStatus::Codes::kUnsupportedEncryptionMode);
      return;
    }
  }

  // Get CdmContext, which could be null.
  CdmContext* cdm_context =
      cdm_context_ref_ ? cdm_context_ref_->GetCdmContext() : nullptr;

  if (config.is_encrypted() && !cdm_context) {
    DVLOG(1) << "CdmContext for "
             << CdmContext::CdmIdToString(base::OptionalToPtr(cdm_id))
             << " not found for encrypted audio";
    OnInitialized(std::move(callback),
                  DecoderStatus::Codes::kUnsupportedEncryptionMode);
    return;
  }

  decoder_->Initialize(
      config, cdm_context,
      base::BindOnce(&MojoAudioDecoderService::OnInitialized, weak_this_,
                     std::move(callback)),
      base::BindRepeating(&MojoAudioDecoderService::OnAudioBufferReady,
                          weak_this_),
      base::BindRepeating(&MojoAudioDecoderService::OnWaiting, weak_this_));
}

void MojoAudioDecoderService::SetDataSource(
    mojo::ScopedDataPipeConsumerHandle receive_pipe) {
  DVLOG(1) << __func__;

  mojo_decoder_buffer_reader_ =
      std::make_unique<MojoDecoderBufferReader>(std::move(receive_pipe));
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
      base::BindOnce(&MojoAudioDecoderService::OnReaderFlushDone, weak_this_,
                     std::move(callback)));
}

void MojoAudioDecoderService::OnInitialized(InitializeCallback callback,
                                            DecoderStatus status) {
  DVLOG(1) << __func__ << " success:" << status.is_ok();

  if (!status.is_ok()) {
    // Do not call decoder_->NeedsBitstreamConversion() if init failed.
    std::move(callback).Run(
        std::move(status), false,
        decoder_ ? decoder_->GetDecoderType() : AudioDecoderType::kUnknown);
    return;
  }

  std::move(callback).Run(std::move(status),
                          decoder_->NeedsBitstreamConversion(),
                          decoder_->GetDecoderType());
}

// The following methods are needed so that we can bind them with a weak pointer
// to avoid running the |callback| after connection error happens and |this| is
// deleted. It's not safe to run the |callback| after a connection error.

void MojoAudioDecoderService::OnReadDone(DecodeCallback callback,
                                         scoped_refptr<DecoderBuffer> buffer) {
  DVLOG(3) << __func__ << " success:" << !!buffer;

  if (!buffer) {
    std::move(callback).Run(DecoderStatus::Codes::kFailedToGetDecoderBuffer);
    return;
  }

  decoder_->Decode(buffer,
                   base::BindOnce(&MojoAudioDecoderService::OnDecodeStatus,
                                  weak_this_, std::move(callback)));
}

void MojoAudioDecoderService::OnReaderFlushDone(ResetCallback callback) {
  decoder_->Reset(base::BindOnce(&MojoAudioDecoderService::OnResetDone,
                                 weak_this_, std::move(callback)));
}

void MojoAudioDecoderService::OnDecodeStatus(DecodeCallback callback,
                                             const DecoderStatus status) {
  DVLOG(3) << __func__ << " status=" << status.group() << ":"
           << static_cast<int>(status.code());
  std::move(callback).Run(std::move(status));
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
