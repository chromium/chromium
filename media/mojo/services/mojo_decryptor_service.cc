// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/services/mojo_decryptor_service.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/numerics/safe_conversions.h"
#include "media/base/audio_decoder_config.h"
#include "media/base/cdm_context.h"
#include "media/base/decoder_buffer.h"
#include "media/base/decryptor.h"
#include "media/base/video_decoder_config.h"
#include "media/base/video_frame.h"
#include "media/mojo/common/media_type_converters.h"
#include "media/mojo/common/mojo_decoder_buffer_converter.h"
#include "media/mojo/mojom/demuxer_stream.mojom.h"
#include "media/mojo/services/mojo_cdm_service_context.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"

namespace media {

namespace {

// A mojom::FrameResourceReleaser implementation. This object is created when
// DecryptAndDecodeVideo() returns a shared memory video frame, and holds
// on to the local frame. When MojoDecryptor is done using the frame,
// the connection should be broken and this will free the shared resources
// associated with the frame.
class FrameResourceReleaserImpl final : public mojom::FrameResourceReleaser {
 public:
  explicit FrameResourceReleaserImpl(scoped_refptr<VideoFrame> frame)
      : frame_(std::move(frame)) {
    DVLOG(3) << __func__;
    DCHECK_EQ(VideoFrame::STORAGE_SHMEM, frame_->storage_type());
  }

  FrameResourceReleaserImpl(const FrameResourceReleaserImpl&) = delete;
  FrameResourceReleaserImpl& operator=(const FrameResourceReleaserImpl&) =
      delete;

  ~FrameResourceReleaserImpl() override { DVLOG(3) << __func__; }

 private:
  scoped_refptr<VideoFrame> frame_;
};

const char kInvalidStateMessage[] = "MojoDecryptorService - invalid state";

}  // namespace

MojoDecryptorService::MojoDecryptorService(
    media::Decryptor* decryptor,
    std::unique_ptr<CdmContextRef> cdm_context_ref)
    : decryptor_(decryptor), cdm_context_ref_(std::move(cdm_context_ref)) {
  DVLOG(1) << __func__;
  DCHECK(decryptor_);
  // |cdm_context_ref_| could be null, in which case the owner of |this| will
  // make sure |decryptor_| is always valid.
  weak_this_ = weak_factory_.GetWeakPtr();
}

MojoDecryptorService::~MojoDecryptorService() {
  DVLOG(1) << __func__;
}

void MojoDecryptorService::Initialize(
    mojo::ScopedDataPipeConsumerHandle audio_pipe,
    mojo::ScopedDataPipeConsumerHandle video_pipe,
    mojo::ScopedDataPipeConsumerHandle decrypt_pipe,
    mojo::ScopedDataPipeProducerHandle decrypted_pipe) {
  DVLOG(1) << __func__;

  if (has_initialize_been_called_) {
    CHECK(mojo::IsInMessageDispatch());
    mojo::ReportBadMessage(kInvalidStateMessage);
    return;
  }
  has_initialize_been_called_ = true;

  audio_buffer_reader_ =
      std::make_unique<MojoDecoderBufferReader>(std::move(audio_pipe));
  video_buffer_reader_ =
      std::make_unique<MojoDecoderBufferReader>(std::move(video_pipe));
  decrypt_buffer_reader_ =
      std::make_unique<MojoDecoderBufferReader>(std::move(decrypt_pipe));
  decrypted_buffer_writer_ =
      std::make_unique<MojoDecoderBufferWriter>(std::move(decrypted_pipe));
}

void MojoDecryptorService::Decrypt(StreamType stream_type,
                                   mojom::DecoderBufferPtr encrypted,
                                   DecryptCallback callback) {
  DVLOG(3) << __func__;

  if (!decrypt_buffer_reader_) {
    CHECK(mojo::IsInMessageDispatch());
    mojo::ReportBadMessage(kInvalidStateMessage);
    return;
  }

  if (!GetBufferReader(stream_type)) {
    CHECK(mojo::IsInMessageDispatch());
    mojo::ReportBadMessage("Unexpected stream_type");
    return;
  }

  decrypt_buffer_reader_->ReadDecoderBuffer(
      std::move(encrypted),
      base::BindOnce(&MojoDecryptorService::OnReadDone, weak_this_,
                     mojo::GetBadMessageCallback(), stream_type,
                     std::move(callback)));
}

void MojoDecryptorService::CancelDecrypt(StreamType stream_type) {
  DVLOG(2) << __func__;
  if (!GetBufferReader(stream_type)) {
    CHECK(mojo::IsInMessageDispatch());
    mojo::ReportBadMessage("Unexpected stream_type");
    return;
  }
  decryptor_->CancelDecrypt(stream_type);
}

void MojoDecryptorService::InitializeAudioDecoder(
    const AudioDecoderConfig& config,
    InitializeAudioDecoderCallback callback) {
  DVLOG(1) << __func__;
  decryptor_->InitializeAudioDecoder(
      config, base::BindOnce(&MojoDecryptorService::OnAudioDecoderInitialized,
                             weak_this_, std::move(callback)));
}

void MojoDecryptorService::InitializeVideoDecoder(
    const VideoDecoderConfig& config,
    InitializeVideoDecoderCallback callback) {
  DVLOG(2) << __func__;

  if (!config.IsValidConfig()) {
    std::move(callback).Run(false);
    mojo::ReportBadMessage("Invalid VideoDecoderConfig");
    return;
  }

  decryptor_->InitializeVideoDecoder(
      config, base::BindOnce(&MojoDecryptorService::OnVideoDecoderInitialized,
                             weak_this_, std::move(callback)));
}

void MojoDecryptorService::DecryptAndDecodeAudio(
    mojom::DecoderBufferPtr encrypted,
    DecryptAndDecodeAudioCallback callback) {
  DVLOG(3) << __func__;

  if (!audio_buffer_reader_) {
    CHECK(mojo::IsInMessageDispatch());
    mojo::ReportBadMessage(kInvalidStateMessage);
    return;
  }

  audio_buffer_reader_->ReadDecoderBuffer(
      std::move(encrypted),
      base::BindOnce(&MojoDecryptorService::OnAudioRead, weak_this_,
                     mojo::GetBadMessageCallback(), std::move(callback)));
}

void MojoDecryptorService::DecryptAndDecodeVideo(
    mojom::DecoderBufferPtr encrypted,
    DecryptAndDecodeVideoCallback callback) {
  DVLOG(3) << __func__;

  if (!video_buffer_reader_) {
    CHECK(mojo::IsInMessageDispatch());
    mojo::ReportBadMessage(kInvalidStateMessage);
    return;
  }

  video_buffer_reader_->ReadDecoderBuffer(
      std::move(encrypted),
      base::BindOnce(&MojoDecryptorService::OnVideoRead, weak_this_,
                     mojo::GetBadMessageCallback(), std::move(callback)));
}

void MojoDecryptorService::ResetDecoder(StreamType stream_type) {
  DVLOG(2) << __func__ << ": stream_type = " << stream_type;

  // Reset the reader so that pending decodes will be dispatched first.
  MojoDecoderBufferReader* reader = GetBufferReader(stream_type);
  if (!reader) {
    CHECK(mojo::IsInMessageDispatch());
    mojo::ReportBadMessage("Unexpected stream_type");
    return;
  }

  reader->Flush(base::BindOnce(&MojoDecryptorService::OnReaderFlushDone,
                               weak_this_, stream_type));
}

void MojoDecryptorService::DeinitializeDecoder(StreamType stream_type) {
  DVLOG(2) << __func__;
  auto* reader = GetBufferReader(stream_type);
  if (!reader) {
    CHECK(mojo::IsInMessageDispatch());
    mojo::ReportBadMessage("Unexpected stream_type");
    return;
  }

  DCHECK(!reader->HasPendingReads())
      << "The decoder should be fully flushed before deinitialized.";

  decryptor_->DeinitializeDecoder(stream_type);
}

void MojoDecryptorService::OnReadDone(
    mojo::ReportBadMessageCallback bad_message_callback,
    StreamType stream_type,
    DecryptCallback callback,
    scoped_refptr<DecoderBuffer> buffer) {
  if (!buffer) {
    std::move(callback).Run(Status::kError, nullptr);
    return;
  }

  if (!buffer->end_of_stream() && buffer->side_data() &&
      buffer->side_data()->secure_handle) {
    std::move(callback).Run(Status::kError, nullptr);
    std::move(bad_message_callback)
        .Run("Renderer sent non-zero DecoderBufferSideData.secure_handle.");
    return;
  }

  if (!GetBufferReader(stream_type)) {
    std::move(bad_message_callback).Run("Unexpected stream_type");
    return;
  }

  decryptor_->Decrypt(stream_type, std::move(buffer),
                      base::BindOnce(&MojoDecryptorService::OnDecryptDone,
                                     weak_this_, std::move(callback)));
}

void MojoDecryptorService::OnDecryptDone(DecryptCallback callback,
                                         Status status,
                                         scoped_refptr<DecoderBuffer> buffer) {
  DVLOG_IF(1, status != Status::kSuccess) << __func__ << "(" << status << ")";
  DVLOG_IF(3, status == Status::kSuccess) << __func__;

  if (!buffer) {
    std::move(callback).Run(status, nullptr);
    return;
  }

  mojom::DecoderBufferPtr mojo_buffer =
      decrypted_buffer_writer_->WriteDecoderBuffer(std::move(buffer));
  if (!mojo_buffer) {
    std::move(callback).Run(Status::kError, nullptr);
    return;
  }

  std::move(callback).Run(status, std::move(mojo_buffer));
}

void MojoDecryptorService::OnAudioDecoderInitialized(
    InitializeAudioDecoderCallback callback,
    bool success) {
  DVLOG(2) << __func__ << "(" << success << ")";
  std::move(callback).Run(success);
}

void MojoDecryptorService::OnVideoDecoderInitialized(
    InitializeVideoDecoderCallback callback,
    bool success) {
  DVLOG(2) << __func__ << "(" << success << ")";
  std::move(callback).Run(success);
}

void MojoDecryptorService::OnAudioRead(
    mojo::ReportBadMessageCallback bad_message_callback,
    DecryptAndDecodeAudioCallback callback,
    scoped_refptr<DecoderBuffer> buffer) {
  if (!buffer) {
    std::move(callback).Run(Status::kError,
                            std::vector<mojom::AudioBufferPtr>());
    return;
  }

  if (!buffer->end_of_stream() && buffer->side_data() &&
      buffer->side_data()->secure_handle) {
    std::move(callback).Run(Status::kError,
                            std::vector<mojom::AudioBufferPtr>());
    std::move(bad_message_callback)
        .Run("Renderer sent non-zero DecoderBufferSideData.secure_handle.");
    return;
  }

  decryptor_->DecryptAndDecodeAudio(
      std::move(buffer), base::BindOnce(&MojoDecryptorService::OnAudioDecoded,
                                        weak_this_, std::move(callback)));
}

void MojoDecryptorService::OnVideoRead(
    mojo::ReportBadMessageCallback bad_message_callback,
    DecryptAndDecodeVideoCallback callback,
    scoped_refptr<DecoderBuffer> buffer) {
  if (!buffer) {
    std::move(callback).Run(Status::kError, nullptr, mojo::NullRemote());
    return;
  }

  if (!buffer->end_of_stream() && buffer->side_data() &&
      buffer->side_data()->secure_handle) {
    std::move(callback).Run(Status::kError, nullptr, mojo::NullRemote());
    std::move(bad_message_callback)
        .Run("Renderer sent non-zero DecoderBufferSideData.secure_handle.");
    return;
  }

  decryptor_->DecryptAndDecodeVideo(
      std::move(buffer), base::BindOnce(&MojoDecryptorService::OnVideoDecoded,
                                        weak_this_, std::move(callback)));
}

void MojoDecryptorService::OnReaderFlushDone(StreamType stream_type) {
  DVLOG(2) << __func__ << ": stream_type = " << stream_type;
  decryptor_->ResetDecoder(stream_type);
}

void MojoDecryptorService::OnAudioDecoded(
    DecryptAndDecodeAudioCallback callback,
    Status status,
    const media::Decryptor::AudioFrames& frames) {
  DVLOG_IF(1, status != Status::kSuccess) << __func__ << "(" << status << ")";
  DVLOG_IF(3, status == Status::kSuccess) << __func__;

  // Note that the audio data is sent over the mojo pipe. This could be
  // improved to use shared memory (http://crbug.com/593896).
  std::vector<mojom::AudioBufferPtr> audio_buffers;
  for (const auto& frame : frames)
    audio_buffers.push_back(mojom::AudioBuffer::From(*frame));

  std::move(callback).Run(status, std::move(audio_buffers));
}

void MojoDecryptorService::OnVideoDecoded(
    DecryptAndDecodeVideoCallback callback,
    Status status,
    scoped_refptr<VideoFrame> frame) {
  DVLOG_IF(1, status != Status::kSuccess)
      << __func__ << ": status = " << status;
  DVLOG_IF(3, status == Status::kSuccess) << __func__;

  if (!frame) {
    DCHECK_NE(status, Status::kSuccess);
    std::move(callback).Run(status, nullptr, mojo::NullRemote());
    return;
  }

  // If |frame| has shared memory that will be passed back, keep the reference
  // to it until the other side is done with the memory.
  mojo::PendingRemote<mojom::FrameResourceReleaser> releaser;
  if (frame->storage_type() == VideoFrame::STORAGE_SHMEM) {
    mojo::MakeSelfOwnedReceiver(
        std::make_unique<FrameResourceReleaserImpl>(frame),
        releaser.InitWithNewPipeAndPassReceiver());
  }

  std::move(callback).Run(status, std::move(frame), std::move(releaser));
}

MojoDecoderBufferReader* MojoDecryptorService::GetBufferReader(
    StreamType stream_type) const {
  switch (stream_type) {
    case StreamType::kAudio:
      return audio_buffer_reader_.get();
    case StreamType::kVideo:
      return video_buffer_reader_.get();
  }

  return nullptr;
}

}  // namespace media
