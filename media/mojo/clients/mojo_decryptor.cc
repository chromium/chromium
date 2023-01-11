// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/clients/mojo_decryptor.h"

#include <stddef.h>
#include <stdint.h>

#include <utility>

#include "base/functional/bind.h"
#include "base/numerics/safe_conversions.h"
#include "media/base/audio_buffer.h"
#include "media/base/decoder_buffer.h"
#include "media/base/video_frame.h"
#include "media/mojo/common/media_type_converters.h"
#include "media/mojo/common/mojo_decoder_buffer_converter.h"
#include "media/mojo/mojom/decryptor.mojom.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"

namespace media {

namespace {

void ReleaseFrameResource(
    mojo::PendingRemote<mojom::FrameResourceReleaser> releaser) {
  // Close the connection, which will result in the service realizing the frame
  // resource is no longer needed.
  releaser.reset();
}

}  // namespace

// TODO(xhwang): Consider adding an Initialize() to reduce the amount of work
// done in the constructor.
MojoDecryptor::MojoDecryptor(
    mojo::PendingRemote<mojom::Decryptor> remote_decryptor,
    uint32_t writer_capacity)
    : remote_decryptor_(std::move(remote_decryptor)) {
  DVLOG(1) << __func__;

  uint32_t audio_writer_capacity =
      writer_capacity
          ? writer_capacity
          : GetDefaultDecoderBufferConverterCapacity(DemuxerStream::AUDIO);
  uint32_t video_writer_capacity =
      writer_capacity
          ? writer_capacity
          : GetDefaultDecoderBufferConverterCapacity(DemuxerStream::VIDEO);

  mojo::ScopedDataPipeConsumerHandle audio_consumer_handle;
  audio_buffer_writer_ = MojoDecoderBufferWriter::Create(
      audio_writer_capacity, &audio_consumer_handle);

  mojo::ScopedDataPipeConsumerHandle video_consumer_handle;
  video_buffer_writer_ = MojoDecoderBufferWriter::Create(
      video_writer_capacity, &video_consumer_handle);

  mojo::ScopedDataPipeConsumerHandle decrypt_consumer_handle;
  // Allocate decrypt-only DataPipe size based on video content.
  decrypt_buffer_writer_ = MojoDecoderBufferWriter::Create(
      video_writer_capacity, &decrypt_consumer_handle);

  mojo::ScopedDataPipeProducerHandle decrypted_producer_handle;
  // Allocate decrypt-only DataPipe size based on video content.
  decrypted_buffer_reader_ = MojoDecoderBufferReader::Create(
      GetDefaultDecoderBufferConverterCapacity(DemuxerStream::VIDEO),
      &decrypted_producer_handle);

  remote_decryptor_.set_disconnect_with_reason_handler(base::BindOnce(
      &MojoDecryptor::OnConnectionError, base::Unretained(this)));

  // Pass the other end of each pipe to |remote_decryptor_|.
  remote_decryptor_->Initialize(
      std::move(audio_consumer_handle), std::move(video_consumer_handle),
      std::move(decrypt_consumer_handle), std::move(decrypted_producer_handle));
}

MojoDecryptor::~MojoDecryptor() {
  DVLOG(1) << __func__;
  DCHECK(thread_checker_.CalledOnValidThread());
}

void MojoDecryptor::Decrypt(StreamType stream_type,
                            scoped_refptr<DecoderBuffer> encrypted,
                            DecryptCB decrypt_cb) {
  DVLOG(3) << __func__;
  DCHECK(thread_checker_.CalledOnValidThread());

  mojom::DecoderBufferPtr mojo_buffer =
      decrypt_buffer_writer_->WriteDecoderBuffer(std::move(encrypted));
  if (!mojo_buffer) {
    std::move(decrypt_cb).Run(kError, nullptr);
    return;
  }

  remote_decryptor_->Decrypt(
      stream_type, std::move(mojo_buffer),
      base::BindOnce(&MojoDecryptor::OnBufferDecrypted,
                     weak_factory_.GetWeakPtr(),
                     mojo::WrapCallbackWithDefaultInvokeIfNotRun(
                         std::move(decrypt_cb), kError, nullptr)));
}

void MojoDecryptor::CancelDecrypt(StreamType stream_type) {
  DVLOG(1) << __func__;
  DCHECK(thread_checker_.CalledOnValidThread());

  remote_decryptor_->CancelDecrypt(stream_type);
}

void MojoDecryptor::InitializeAudioDecoder(const AudioDecoderConfig& config,
                                           DecoderInitCB init_cb) {
  DVLOG(1) << __func__;
  DCHECK(thread_checker_.CalledOnValidThread());

  remote_decryptor_->InitializeAudioDecoder(
      config,
      mojo::WrapCallbackWithDefaultInvokeIfNotRun(std::move(init_cb), false));
}

void MojoDecryptor::InitializeVideoDecoder(const VideoDecoderConfig& config,
                                           DecoderInitCB init_cb) {
  DVLOG(1) << __func__;
  DCHECK(thread_checker_.CalledOnValidThread());

  remote_decryptor_->InitializeVideoDecoder(
      config,
      mojo::WrapCallbackWithDefaultInvokeIfNotRun(std::move(init_cb), false));
}

void MojoDecryptor::DecryptAndDecodeAudio(
    scoped_refptr<DecoderBuffer> encrypted,
    AudioDecodeCB audio_decode_cb) {
  DVLOG(3) << __func__ << ": " << encrypted->AsHumanReadableString();
  DCHECK(thread_checker_.CalledOnValidThread());

  mojom::DecoderBufferPtr mojo_buffer =
      audio_buffer_writer_->WriteDecoderBuffer(std::move(encrypted));
  if (!mojo_buffer) {
    std::move(audio_decode_cb).Run(kError, AudioFrames());
    return;
  }

  remote_decryptor_->DecryptAndDecodeAudio(
      std::move(mojo_buffer),
      base::BindOnce(&MojoDecryptor::OnAudioDecoded, weak_factory_.GetWeakPtr(),
                     mojo::WrapCallbackWithDefaultInvokeIfNotRun(
                         std::move(audio_decode_cb), kError, AudioFrames())));
}

void MojoDecryptor::DecryptAndDecodeVideo(
    scoped_refptr<DecoderBuffer> encrypted,
    VideoDecodeCB video_decode_cb) {
  DVLOG(3) << __func__ << ": " << encrypted->AsHumanReadableString();
  DCHECK(thread_checker_.CalledOnValidThread());

  mojom::DecoderBufferPtr mojo_buffer =
      video_buffer_writer_->WriteDecoderBuffer(std::move(encrypted));
  if (!mojo_buffer) {
    std::move(video_decode_cb).Run(kError, nullptr);
    return;
  }

  remote_decryptor_->DecryptAndDecodeVideo(
      std::move(mojo_buffer),
      base::BindOnce(&MojoDecryptor::OnVideoDecoded, weak_factory_.GetWeakPtr(),
                     mojo::WrapCallbackWithDefaultInvokeIfNotRun(
                         std::move(video_decode_cb), kError, nullptr)));
}

void MojoDecryptor::ResetDecoder(StreamType stream_type) {
  DVLOG(1) << __func__ << ": stream_type = " << stream_type;
  DCHECK(thread_checker_.CalledOnValidThread());

  remote_decryptor_->ResetDecoder(stream_type);
}

void MojoDecryptor::DeinitializeDecoder(StreamType stream_type) {
  DVLOG(1) << __func__;
  DCHECK(thread_checker_.CalledOnValidThread());

  remote_decryptor_->DeinitializeDecoder(stream_type);
}

void MojoDecryptor::OnBufferDecrypted(DecryptCB decrypt_cb,
                                      Status status,
                                      mojom::DecoderBufferPtr buffer) {
  DVLOG_IF(1, status != kSuccess) << __func__ << "(" << status << ")";
  DVLOG_IF(3, status == kSuccess) << __func__;
  DCHECK(thread_checker_.CalledOnValidThread());

  if (buffer.is_null()) {
    std::move(decrypt_cb).Run(status, nullptr);
    return;
  }

  decrypted_buffer_reader_->ReadDecoderBuffer(
      std::move(buffer),
      base::BindOnce(&MojoDecryptor::OnBufferRead, weak_factory_.GetWeakPtr(),
                     std::move(decrypt_cb), status));
}

void MojoDecryptor::OnBufferRead(DecryptCB decrypt_cb,
                                 Status status,
                                 scoped_refptr<DecoderBuffer> buffer) {
  if (!buffer) {
    std::move(decrypt_cb).Run(kError, nullptr);
    return;
  }

  std::move(decrypt_cb).Run(status, std::move(buffer));
}

void MojoDecryptor::OnAudioDecoded(
    AudioDecodeCB audio_decode_cb,
    Status status,
    std::vector<mojom::AudioBufferPtr> audio_buffers) {
  DVLOG_IF(1, status != kSuccess) << __func__ << "(" << status << ")";
  DVLOG_IF(3, status == kSuccess) << __func__;
  DCHECK(thread_checker_.CalledOnValidThread());

  Decryptor::AudioFrames audio_frames;
  for (size_t i = 0; i < audio_buffers.size(); ++i)
    audio_frames.push_back(audio_buffers[i].To<scoped_refptr<AudioBuffer>>());

  std::move(audio_decode_cb).Run(status, audio_frames);
}

void MojoDecryptor::OnVideoDecoded(
    VideoDecodeCB video_decode_cb,
    Status status,
    const scoped_refptr<VideoFrame>& video_frame,
    mojo::PendingRemote<mojom::FrameResourceReleaser> releaser) {
  DVLOG_IF(1, status != kSuccess) << __func__ << ": status = " << status;
  DVLOG_IF(3, status == kSuccess) << __func__;
  DCHECK(thread_checker_.CalledOnValidThread());

  // If using shared memory, ensure that |releaser| is closed when
  // |frame| is destroyed.
  if (video_frame && releaser) {
    video_frame->AddDestructionObserver(
        base::BindOnce(&ReleaseFrameResource, std::move(releaser)));
  }

  std::move(video_decode_cb).Run(status, video_frame);
}

void MojoDecryptor::OnConnectionError(uint32_t custom_reason,
                                      const std::string& description) {
  DVLOG(1) << "Remote CDM connection error: custom_reason=" << custom_reason
           << ", description=\"" << description << "\"";
  DCHECK(thread_checker_.CalledOnValidThread());

  // All pending callbacks will be fired automatically because they are wrapped
  // in WrapCallbackWithDefaultInvokeIfNotRun.
}

}  // namespace media
