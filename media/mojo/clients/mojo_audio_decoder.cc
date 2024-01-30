// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/clients/mojo_audio_decoder.h"

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/task/sequenced_task_runner.h"
#include "build/build_config.h"
#include "media/base/audio_buffer.h"
#include "media/base/cdm_context.h"
#include "media/base/demuxer_stream.h"
#include "media/mojo/clients/mojo_media_log_service.h"
#include "media/mojo/common/media_type_converters.h"
#include "media/mojo/common/mojo_decoder_buffer_converter.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"

namespace media {

MojoAudioDecoder::MojoAudioDecoder(
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    MediaLog* media_log,
    mojo::PendingRemote<mojom::AudioDecoder> remote_decoder)
    : task_runner_(task_runner),
      pending_remote_decoder_(std::move(remote_decoder)),
      writer_capacity_(
          GetDefaultDecoderBufferConverterCapacity(DemuxerStream::AUDIO)),
      media_log_(media_log) {
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
#if BUILDFLAG(IS_ANDROID)
  return true;
#else
  return false;
#endif
}

AudioDecoderType MojoAudioDecoder::GetDecoderType() const {
  return decoder_type_;
}

void MojoAudioDecoder::FailInit(InitCB init_cb, DecoderStatus err) {
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
    FailInit(std::move(init_cb), DecoderStatus::Codes::kDisconnected);
    return;
  }

  // Fail immediately if the stream is encrypted but |cdm_context| is invalid.
  std::optional<base::UnguessableToken> cdm_id;
  if (config.is_encrypted() && cdm_context)
    cdm_id = cdm_context->GetCdmId();

  if (config.is_encrypted() && !cdm_id) {
    DVLOG(1) << __func__ << ": Invalid CdmContext.";
    FailInit(std::move(init_cb),
             DecoderStatus::Codes::kUnsupportedEncryptionMode);
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
    task_runner_->PostTask(FROM_HERE,
                           base::BindOnce(std::move(decode_cb),
                                          DecoderStatus::Codes::kDisconnected));
    return;
  }

  mojom::DecoderBufferPtr buffer =
      mojo_decoder_buffer_writer_->WriteDecoderBuffer(std::move(media_buffer));
  if (!buffer) {
    task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(decode_cb), DecoderStatus::Codes::kFailed));
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
          FROM_HERE, base::BindOnce(std::move(decode_cb_),
                                    DecoderStatus::Codes::kDisconnected));
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

  // Use mojo::MakeSelfOwnedReceiver() for MediaLog so logs may go through even
  // after MojoAudioDecoder is destructed.
  mojo::PendingReceiver<mojom::MediaLog> media_log_pending_receiver;
  auto media_log_pending_remote =
      media_log_pending_receiver.InitWithNewPipeAndPassRemote();
  mojo::MakeSelfOwnedReceiver(
      std::make_unique<MojoMediaLogService>(media_log_->Clone()),
      std::move(media_log_pending_receiver));

  remote_decoder_->Construct(client_receiver_.BindNewEndpointAndPassRemote(),
                             std::move(media_log_pending_remote));
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
    FailInit(std::move(init_cb_), DecoderStatus::Codes::kDisconnected);
    return;
  }

  if (decode_cb_)
    std::move(decode_cb_).Run(DecoderStatus::Codes::kDisconnected);
  if (reset_cb_)
    std::move(reset_cb_).Run();
}

void MojoAudioDecoder::OnInitialized(const DecoderStatus& status,
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

void MojoAudioDecoder::OnDecodeStatus(const DecoderStatus& status) {
  DVLOG(1) << __func__ << ": status:" << static_cast<int>(status.code());
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
