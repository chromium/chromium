// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/services/mojo_decryptor_service.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
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
#include "mojo/public/cpp/bindings/callback_helpers.h"
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

template <media::Decryptor::StreamType StreamTypeParam>
struct StreamTraits;

template <>
struct StreamTraits<media::Decryptor::kAudio> {
  using ConfigType = AudioDecoderConfig;
  using InitCallback = mojom::Decryptor::InitializeAudioDecoderCallback;
  using DecodeCallback = mojom::Decryptor::DecryptAndDecodeAudioCallback;

  static void RunDecodeCallbackWithError(DecodeCallback callback) {
    std::move(callback).Run(media::Decryptor::Status::kError,
                            std::vector<mojom::AudioBufferPtr>());
  }
};

template <>
struct StreamTraits<media::Decryptor::kVideo> {
  using ConfigType = VideoDecoderConfig;
  using InitCallback = mojom::Decryptor::InitializeVideoDecoderCallback;
  using DecodeCallback = mojom::Decryptor::DecryptAndDecodeVideoCallback;

  static void RunDecodeCallbackWithError(DecodeCallback callback) {
    std::move(callback).Run(media::Decryptor::Status::kError, nullptr,
                            mojo::NullRemote());
  }
};

}  // namespace

template <MojoDecryptorService::StreamType StreamTypeParam>
class MojoDecryptorService::Stream {
 public:
  using Traits = StreamTraits<StreamTypeParam>;

  Stream(media::Decryptor* decryptor,
         MojoDecoderBufferReader* shared_decrypt_reader,
         MojoDecoderBufferWriter* shared_decrypt_writer)
      : decryptor_(decryptor),
        decrypt_reader_(shared_decrypt_reader),
        decrypt_writer_(shared_decrypt_writer) {}

  ~Stream() = default;

  void InitializeDecodePipe(mojo::ScopedDataPipeConsumerHandle pipe) {
    decode_reader_ = std::make_unique<MojoDecoderBufferReader>(std::move(pipe));
  }

  void Decrypt(mojom::DecoderBufferPtr encrypted, DecryptCallback callback) {
    decrypt_reader_->ReadDecoderBuffer(
        std::move(encrypted),
        base::BindOnce(&Stream::OnDecryptReadDone,
                       decrypt_weak_factory_.GetWeakPtr(),
                       mojo::WrapCallbackWithDefaultInvokeIfNotRun(
                           std::move(callback), Status::kError, nullptr),
                       mojo::GetBadMessageCallback()));
  }

  void CancelDecrypt() {
    decrypt_weak_factory_.InvalidateWeakPtrs();
    decryptor_->CancelDecrypt(StreamTypeParam);
  }

  void InitializeDecoder(const typename Traits::ConfigType& config,
                         typename Traits::InitCallback callback) {
    decode_weak_factory_.InvalidateWeakPtrs();

    if constexpr (StreamTypeParam == StreamType::kVideo) {
      if (!config.IsValidConfig()) {
        std::move(callback).Run(false);
        mojo::ReportBadMessage("Invalid VideoDecoderConfig");
        return;
      }
    }

    auto bound_cb = base::BindOnce(&Stream::OnDecoderInitialized,
                                   decode_weak_factory_.GetWeakPtr(),
                                   mojo::WrapCallbackWithDefaultInvokeIfNotRun(
                                       std::move(callback), false));

    if constexpr (StreamTypeParam == StreamType::kAudio) {
      decryptor_->InitializeAudioDecoder(config, std::move(bound_cb));
    } else {
      decryptor_->InitializeVideoDecoder(config, std::move(bound_cb));
    }
  }

  void DecryptAndDecode(mojom::DecoderBufferPtr encrypted,
                        typename Traits::DecodeCallback callback) {
    if (!decode_reader_) {
      mojo::ReportBadMessage(kInvalidStateMessage);
      return;
    }

    auto wrapped_callback = WrapDecodeCallback(std::move(callback));

    decode_reader_->ReadDecoderBuffer(
        std::move(encrypted), base::BindOnce(&Stream::OnDecodeReadDone,
                                             decode_weak_factory_.GetWeakPtr(),
                                             mojo::GetBadMessageCallback(),
                                             std::move(wrapped_callback)));
  }

  void ResetDecoder() {
    if (!decode_reader_) {
      mojo::ReportBadMessage(kInvalidStateMessage);
      return;
    }

    decode_reader_->Flush(base::BindOnce(&Stream::OnReaderFlushDone,
                                         decode_weak_factory_.GetWeakPtr()));
  }

  void DeinitializeDecoder() {
    decode_weak_factory_.InvalidateWeakPtrs();
    decryptor_->DeinitializeDecoder(StreamTypeParam);
  }

 private:
  typename Traits::DecodeCallback WrapDecodeCallback(
      typename Traits::DecodeCallback callback) {
    if constexpr (StreamTypeParam == StreamType::kAudio) {
      return mojo::WrapCallbackWithDefaultInvokeIfNotRun(
          std::move(callback), Status::kError,
          std::vector<mojom::AudioBufferPtr>());
    } else {
      return mojo::WrapCallbackWithDefaultInvokeIfNotRun(
          std::move(callback), Status::kError, nullptr, mojo::NullRemote());
    }
  }

  void OnDecryptReadDone(DecryptCallback callback,
                         mojo::ReportBadMessageCallback bad_message_callback,
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

    decryptor_->Decrypt(StreamTypeParam, std::move(buffer),
                        base::BindOnce(&Stream::OnDecryptDone,
                                       decrypt_weak_factory_.GetWeakPtr(),
                                       std::move(callback)));
  }

  void OnDecryptDone(DecryptCallback callback,
                     Status status,
                     scoped_refptr<DecoderBuffer> buffer) {
    if (!buffer) {
      std::move(callback).Run(status, nullptr);
      return;
    }

    mojom::DecoderBufferPtr mojo_buffer =
        decrypt_writer_->WriteDecoderBuffer(std::move(buffer));
    if (!mojo_buffer) {
      std::move(callback).Run(Status::kError, nullptr);
      return;
    }

    std::move(callback).Run(status, std::move(mojo_buffer));
  }

  void OnDecoderInitialized(base::OnceCallback<void(bool)> callback,
                            bool success) {
    std::move(callback).Run(success);
  }

  void OnDecodeReadDone(mojo::ReportBadMessageCallback bad_message_callback,
                        typename Traits::DecodeCallback callback,
                        scoped_refptr<DecoderBuffer> buffer) {
    if (!buffer) {
      Traits::RunDecodeCallbackWithError(std::move(callback));
      return;
    }

    if (!buffer->end_of_stream() && buffer->side_data() &&
        buffer->side_data()->secure_handle) {
      Traits::RunDecodeCallbackWithError(std::move(callback));
      std::move(bad_message_callback)
          .Run("Renderer sent non-zero DecoderBufferSideData.secure_handle.");
      return;
    }

    if constexpr (StreamTypeParam == StreamType::kAudio) {
      decryptor_->DecryptAndDecodeAudio(
          std::move(buffer), base::BindOnce(&Stream::OnAudioDecoded,
                                            decode_weak_factory_.GetWeakPtr(),
                                            std::move(callback)));
    } else {
      decryptor_->DecryptAndDecodeVideo(
          std::move(buffer), base::BindOnce(&Stream::OnVideoDecoded,
                                            decode_weak_factory_.GetWeakPtr(),
                                            std::move(callback)));
    }
  }

  void OnAudioDecoded(mojom::Decryptor::DecryptAndDecodeAudioCallback callback,
                      Status status,
                      const media::Decryptor::AudioFrames& frames) {
    std::vector<mojom::AudioBufferPtr> audio_buffers;
    for (const auto& frame : frames) {
      audio_buffers.push_back(mojom::AudioBuffer::From(*frame));
    }
    std::move(callback).Run(status, std::move(audio_buffers));
  }

  void OnVideoDecoded(mojom::Decryptor::DecryptAndDecodeVideoCallback callback,
                      Status status,
                      scoped_refptr<VideoFrame> frame) {
    if (!frame) {
      DCHECK_NE(status, Status::kSuccess);
      std::move(callback).Run(status, nullptr, mojo::NullRemote());
      return;
    }

    mojo::PendingRemote<mojom::FrameResourceReleaser> releaser;
    if (frame->storage_type() == VideoFrame::STORAGE_SHMEM) {
      mojo::MakeSelfOwnedReceiver(
          std::make_unique<FrameResourceReleaserImpl>(frame),
          releaser.InitWithNewPipeAndPassReceiver());
    }

    std::move(callback).Run(status, std::move(frame), std::move(releaser));
  }

  void OnReaderFlushDone() { decryptor_->ResetDecoder(StreamTypeParam); }

  const raw_ptr<media::Decryptor> decryptor_;
  const raw_ptr<MojoDecoderBufferReader> decrypt_reader_;
  const raw_ptr<MojoDecoderBufferWriter> decrypt_writer_;
  std::unique_ptr<MojoDecoderBufferReader> decode_reader_;

  base::WeakPtrFactory<Stream> decrypt_weak_factory_{this};
  base::WeakPtrFactory<Stream> decode_weak_factory_{this};
};

MojoDecryptorService::MojoDecryptorService(
    media::Decryptor* decryptor,
    std::unique_ptr<CdmContextRef> cdm_context_ref)
    : decryptor_(decryptor), cdm_context_ref_(std::move(cdm_context_ref)) {
  DVLOG(1) << __func__;
  DCHECK(decryptor_);
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
    mojo::ReportBadMessage(kInvalidStateMessage);
    return;
  }
  has_initialize_been_called_ = true;

  decrypt_buffer_reader_ =
      std::make_unique<MojoDecoderBufferReader>(std::move(decrypt_pipe));
  decrypted_buffer_writer_ =
      std::make_unique<MojoDecoderBufferWriter>(std::move(decrypted_pipe));

  audio_stream_ = std::make_unique<Stream<StreamType::kAudio>>(
      decryptor_, decrypt_buffer_reader_.get(), decrypted_buffer_writer_.get());
  video_stream_ = std::make_unique<Stream<StreamType::kVideo>>(
      decryptor_, decrypt_buffer_reader_.get(), decrypted_buffer_writer_.get());

  audio_stream_->InitializeDecodePipe(std::move(audio_pipe));
  video_stream_->InitializeDecodePipe(std::move(video_pipe));
}

void MojoDecryptorService::Decrypt(StreamType stream_type,
                                   mojom::DecoderBufferPtr encrypted,
                                   DecryptCallback callback) {
  DVLOG(3) << __func__;

  if (!decrypt_buffer_reader_) {
    mojo::ReportBadMessage(kInvalidStateMessage);
    return;
  }

  if (stream_type == StreamType::kAudio) {
    audio_stream_->Decrypt(std::move(encrypted), std::move(callback));
  } else {
    video_stream_->Decrypt(std::move(encrypted), std::move(callback));
  }
}

void MojoDecryptorService::CancelDecrypt(StreamType stream_type) {
  DVLOG(2) << __func__;
  if (stream_type == StreamType::kAudio) {
    audio_stream_->CancelDecrypt();
  } else {
    video_stream_->CancelDecrypt();
  }
}

void MojoDecryptorService::InitializeAudioDecoder(
    const AudioDecoderConfig& config,
    InitializeAudioDecoderCallback callback) {
  DVLOG(1) << __func__;
  audio_stream_->InitializeDecoder(config, std::move(callback));
}

void MojoDecryptorService::InitializeVideoDecoder(
    const VideoDecoderConfig& config,
    InitializeVideoDecoderCallback callback) {
  DVLOG(2) << __func__;
  video_stream_->InitializeDecoder(config, std::move(callback));
}

void MojoDecryptorService::DecryptAndDecodeAudio(
    mojom::DecoderBufferPtr encrypted,
    DecryptAndDecodeAudioCallback callback) {
  DVLOG(3) << __func__;
  audio_stream_->DecryptAndDecode(std::move(encrypted), std::move(callback));
}

void MojoDecryptorService::DecryptAndDecodeVideo(
    mojom::DecoderBufferPtr encrypted,
    DecryptAndDecodeVideoCallback callback) {
  DVLOG(3) << __func__;
  video_stream_->DecryptAndDecode(std::move(encrypted), std::move(callback));
}

void MojoDecryptorService::ResetDecoder(StreamType stream_type) {
  DVLOG(2) << __func__ << ": stream_type = " << stream_type;
  if (stream_type == StreamType::kAudio) {
    audio_stream_->ResetDecoder();
  } else {
    video_stream_->ResetDecoder();
  }
}

void MojoDecryptorService::DeinitializeDecoder(StreamType stream_type) {
  DVLOG(2) << __func__ << " stream_type=" << stream_type;
  if (stream_type == StreamType::kAudio) {
    audio_stream_->DeinitializeDecoder();
  } else {
    video_stream_->DeinitializeDecoder();
  }
}

}  // namespace media
