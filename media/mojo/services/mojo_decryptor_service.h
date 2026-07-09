// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MOJO_SERVICES_MOJO_DECRYPTOR_SERVICE_H_
#define MEDIA_MOJO_SERVICES_MOJO_DECRYPTOR_SERVICE_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>

#include "base/compiler_specific.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "media/base/cdm_context.h"
#include "media/base/decryptor.h"
#include "media/mojo/mojom/decryptor.mojom.h"
#include "media/mojo/services/media_mojo_export.h"

namespace media {

class MojoDecoderBufferReader;
class MojoDecoderBufferWriter;

// A mojom::Decryptor implementation that proxies decryptor calls to a
// media::Decryptor.
class MEDIA_MOJO_EXPORT MojoDecryptorService final : public mojom::Decryptor {
 public:
  using StreamType = media::Decryptor::StreamType;
  using Status = media::Decryptor::Status;

  // If |cdm_context_ref| is null, caller must ensure that |decryptor| outlives
  // |this|. Otherwise, |decryptor| is guaranteed to be valid as long as
  // |cdm_context_ref| is held.
  MojoDecryptorService(media::Decryptor* decryptor,
                       std::unique_ptr<CdmContextRef> cdm_context_ref);

  MojoDecryptorService(const MojoDecryptorService&) = delete;
  MojoDecryptorService& operator=(const MojoDecryptorService&) = delete;

  ~MojoDecryptorService() final;

  // mojom::Decryptor implementation.
  void Initialize(mojo::ScopedDataPipeConsumerHandle audio_pipe,
                  mojo::ScopedDataPipeConsumerHandle video_pipe,
                  mojo::ScopedDataPipeConsumerHandle decrypt_pipe,
                  mojo::ScopedDataPipeProducerHandle decrypted_pipe) final;
  void Decrypt(StreamType stream_type,
               mojom::DecoderBufferPtr encrypted,
               DecryptCallback callback) final;
  void CancelDecrypt(StreamType stream_type) final;
  void InitializeAudioDecoder(const AudioDecoderConfig& config,
                              InitializeAudioDecoderCallback callback) final;
  void InitializeVideoDecoder(const VideoDecoderConfig& config,
                              InitializeVideoDecoderCallback callback) final;
  void DecryptAndDecodeAudio(mojom::DecoderBufferPtr encrypted,
                             DecryptAndDecodeAudioCallback callback) final;
  void DecryptAndDecodeVideo(mojom::DecoderBufferPtr encrypted,
                             DecryptAndDecodeVideoCallback callback) final;
  void ResetDecoder(StreamType stream_type) final;
  void DeinitializeDecoder(StreamType stream_type) final;

 private:
  // Encapsulates all state, data pipes, and lifecycle management for a
  // specific media stream (Audio or Video).
  //
  // Note on hardware recovery: A well-behaved client never resets or
  // deinitializes a decoder while a read is still pending. However, on ChromeOS
  // (which is currently the only platform supporting the L1 + CDM hardware
  // path), hardware decoder recovery paths can legitimately trigger a
  // `DeinitializeDecoder` or `InitializeVideoDecoder` while `DecryptAndDecode`
  // requests are stuck in flight (e.g., if the hardware decoder is hung). These
  // stream classes ensure we can safely cancel pending reads for a specific
  // stream during recovery without dropping callbacks for other active streams.
  template <StreamType StreamTypeParam>
  class Stream;

  bool has_initialize_been_called_ = false;

  // Shared DataPipes for pure Decrypt() calls. Owned here and passed as
  // raw_ptrs to the stream objects to handle multiplexing. Must be declared
  // before the streams so they outlive the streams and prevent dangling
  // pointers during teardown.
  std::unique_ptr<MojoDecoderBufferReader> decrypt_buffer_reader_;
  std::unique_ptr<MojoDecoderBufferWriter> decrypted_buffer_writer_;

  // Stream-specific encapsulations.
  std::unique_ptr<Stream<StreamType::kAudio>> audio_stream_;
  std::unique_ptr<Stream<StreamType::kVideo>> video_stream_;

  raw_ptr<media::Decryptor> decryptor_;

  // Holds the CdmContextRef to keep the CdmContext alive for the lifetime of
  // the |decryptor_|.
  std::unique_ptr<CdmContextRef> cdm_context_ref_;
};

}  // namespace media

#endif  // MEDIA_MOJO_SERVICES_MOJO_DECRYPTOR_SERVICE_H_
