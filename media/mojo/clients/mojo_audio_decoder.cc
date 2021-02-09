// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/clients/mojo_audio_decoder.h"

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/sequenced_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "media/base/audio_buffer.h"
#include "media/base/cdm_context.h"
#include "media/base/demuxer_stream.h"
#include "media/mojo/common/media_type_converters.h"
#include "media/mojo/common/mojo_decoder_buffer_converter.h"

namespace media {

MojoAudioDecoder::MojoAudioDecoder(
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    mojo::PendingRemote<mojom::AudioDecoder> remote_decoder)
    : task_runner_(task_runner),
      pending_remote_decoder_(std::move(remote_decoder)),
      writer_capacity_(
          GetDefaultDecoderBufferConverterCapacity(DemuxerStream::AUDIO)) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
  DVLOG(1) << __func__;
}

MojoAudioDecoder::~MojoAudioDecoder() {
  DVLOG(1) << __func__;
}

bool MojoAudioDecoder::IsPlatformDecoder() const {
  return true;
}

bool MojoAudioDecoder::SupportsDecryption() const {
  // Currently only the android backends support decryption
#if defined(OS_ANDROID)
  return true;
#else
  return false;
#endif
}

AudioDecoderType MojoAudioDecoder::GetDecoderType() const {
  return decoder_type_;
}

std::string MojoAudioDecoder::GetDisplayName() const {
  return "MojoAudioDecoder";
}

void MojoAudioDecoder::FailInit(InitCB init_cb, Status err) {
  task_runner_->PostTask(FROM_HERE,
                         base::BindOnce(std::move(init_cb), std::move(err)));
}

void MojoAudioDecoder::Initialize(const AudioDecoderConfig& config,
                                  CdmContext* cdm_context,
                                  InitCB init_cb,
                                  const OutputCB& output_cb,
                                  const WaitingCB& waiting_cb) {
  DVLOG(1) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!remote_decoder_.is_bound())
    BindRemoteDecoder();

  // This could happen during reinitialization.
  if (!remote_decoder_.is_connected()) {
    DVLOG(1) << __func__ << ": Connection error happened.";
    FailInit(std::move(init_cb), StatusCode::kMojoDecoderNoConnection);
    return;
  }

  // Fail immediately if the stream is encrypted but |cdm_context| is invalid.
  base::Optional<base::UnguessableToken> cdm_id;
  if (config.is_encrypted() && cdm_context)
    cdm_id = cdm_context->GetCdmId();

  if (config.is_encrypted() && !cdm_id) {
    DVLOG(1) << __func__ << ": Invalid CdmContext.";
    FailInit(std::move(init_cb),
             StatusCode::kDecoderMissingCdmForEncryptedContent);
    return;
  }

  init_cb_ = std::move(init_cb);
  output_cb_ = output_cb;
  waiting_cb_ = waiting_cb;

  // Using base::Unretained(this) is safe because |this| owns |remote_decoder_|,
  // and the callback won't be dispatched if |remote_decoder_| is destroyed.
  remote_decoder_->Initialize(
      config, cdm_id,
      base::BindOnce(&MojoAudioDecoder::OnInitialized, base::Unretained(this)));
}

void MojoAudioDecoder::Decode(scoped_refptr<DecoderBuffer> media_buffer,
                              DecodeCB decode_cb) {
  DVLOG(3) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!remote_decoder_.is_connected()) {
    task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(decode_cb), DecodeStatus::DECODE_ERROR));
    return;
  }

  mojom::DecoderBufferPtr buffer =
      mojo_decoder_buffer_writer_->WriteDecoderBuffer(std::move(media_buffer));
  if (!buffer) {
    task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(decode_cb), DecodeStatus::DECODE_ERROR));
    return;
  }

  DCHECK(!decode_cb_);
  decode_cb_ = std::move(decode_cb);

  remote_decoder_->Decode(std::move(buffer),
                          base::BindOnce(&MojoAudioDecoder::OnDecodeStatus,
                                         base::Unretained(this)));
}

void MojoAudioDecoder::Reset(base::OnceClosure closure) {
  DVLOG(2) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!remote_decoder_.is_connected()) {
    if (decode_cb_) {
      task_runner_->PostTask(
          FROM_HERE,
          base::BindOnce(std::move(decode_cb_), DecodeStatus::DECODE_ERROR));
    }

    task_runner_->PostTask(FROM_HERE, std::move(closure));
    return;
  }

  DCHECK(!reset_cb_);
  reset_cb_ = std::move(closure);
  remote_decoder_->Reset(
      base::BindOnce(&MojoAudioDecoder::OnResetDone, base::Unretained(this)));
}

bool MojoAudioDecoder::NeedsBitstreamConversion() const {
  DVLOG(1) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  return needs_bitstream_conversion_;
}

void MojoAudioDecoder::BindRemoteDecoder() {
  DVLOG(1) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  remote_decoder_.Bind(std::move(pending_remote_decoder_));

  // Using base::Unretained(this) is safe because |this| owns |remote_decoder_|,
  // and the error handler can't be invoked once |remote_decoder_| is destroyed.
  remote_decoder_.set_disconnect_handler(base::BindOnce(
      &MojoAudioDecoder::OnConnectionError, base::Unretained(this)));

  remote_decoder_->Construct(client_receiver_.BindNewEndpointAndPassRemote());
}

void MojoAudioDecoder::OnBufferDecoded(mojom::AudioBufferPtr buffer) {
  DVLOG(1) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  output_cb_.Run(buffer.To<scoped_refptr<AudioBuffer>>());
}

void MojoAudioDecoder::OnWaiting(WaitingReason reason) {
  DVLOG(1) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  waiting_cb_.Run(reason);
}

void MojoAudioDecoder::OnConnectionError() {
  DVLOG(1) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!remote_decoder_.is_connected());

  if (init_cb_) {
    FailInit(std::move(init_cb_), StatusCode::kMojoDecoderNoConnection);
    return;
  }

  if (decode_cb_)
    std::move(decode_cb_).Run(DecodeStatus::DECODE_ERROR);
  if (reset_cb_)
    std::move(reset_cb_).Run();
}

void MojoAudioDecoder::OnInitialized(const Status& status,
                                     bool needs_bitstream_conversion,
                                     AudioDecoderType decoder_type) {
  DVLOG(1) << __func__ << ": success:" << status.is_ok();
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  needs_bitstream_conversion_ = needs_bitstream_conversion;
  decoder_type_ = decoder_type;

  if (status.is_ok() && !mojo_decoder_buffer_writer_) {
    mojo::ScopedDataPipeConsumerHandle remote_consumer_handle;
    mojo_decoder_buffer_writer_ = MojoDecoderBufferWriter::Create(
        writer_capacity_, &remote_consumer_handle);

    // Pass consumer end to |remote_decoder_|.
    remote_decoder_->SetDataSource(std::move(remote_consumer_handle));
  }

  std::move(init_cb_).Run(std::move(status));
}

void MojoAudioDecoder::OnDecodeStatus(const Status& status) {
  DVLOG(1) << __func__ << ": status:" << status.code();
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  DCHECK(decode_cb_);
  std::move(decode_cb_).Run(status);
}

void MojoAudioDecoder::OnResetDone() {
  DVLOG(1) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // For pending decodes OnDecodeStatus() should arrive before OnResetDone().
  DCHECK(!decode_cb_);

  DCHECK(reset_cb_);
  std::move(reset_cb_).Run();
}

}  // namespace media
