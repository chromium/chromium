// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MOJO_SERVICES_MOJO_DECRYPTOR_SERVICE_H_
#define MEDIA_MOJO_SERVICES_MOJO_DECRYPTOR_SERVICE_H_

#include <stddef.h>
#include <stdint.h>

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "media/base/cdm_context.h"
#include "media/base/decryptor.h"
#include "media/mojo/mojom/decryptor.mojom.h"
#include "media/mojo/services/media_mojo_export.h"

namespace media {

class DecoderBuffer;
class MojoCdmServiceContext;
class MojoDecoderBufferReader;
class MojoDecoderBufferWriter;

// A mojom::Decryptor implementation that proxies decryptor calls to a
// media::Decryptor.
class MEDIA_MOJO_EXPORT MojoDecryptorService : public mojom::Decryptor {
 public:
  using StreamType = media::Decryptor::StreamType;
  using Status = media::Decryptor::Status;

  static std::unique_ptr<MojoDecryptorService> Create(
      int cdm_id,
      MojoCdmServiceContext* mojo_cdm_service_context);

  // If |cdm_context_ref| is null, caller must ensure that |decryptor| outlives
  // |this|. Otherwise, |decryptor| is guaranteed to be valid as long as
  // |cdm_context_ref| is held.
  MojoDecryptorService(media::Decryptor* decryptor,
                       std::unique_ptr<CdmContextRef> cdm_context_ref);

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
  void OnReadDone(StreamType stream_type,
                  DecryptCallback callback,
                  scoped_refptr<DecoderBuffer> buffer);

  // Callback executed once Decrypt() is done.
  void OnDecryptDone(DecryptCallback callback,
                     Status status,
                     scoped_refptr<DecoderBuffer> buffer);

  // Callbacks executed once decoder initialized.
  void OnAudioDecoderInitialized(InitializeAudioDecoderCallback callback,
                                 bool success);
  void OnVideoDecoderInitialized(InitializeVideoDecoderCallback callback,
                                 bool success);

  void OnAudioRead(DecryptAndDecodeAudioCallback callback,
                   scoped_refptr<DecoderBuffer> buffer);
  void OnVideoRead(DecryptAndDecodeVideoCallback callback,
                   scoped_refptr<DecoderBuffer> buffer);
  void OnReaderFlushDone(StreamType stream_type);

  // Callbacks executed when DecryptAndDecode are done.
  void OnAudioDecoded(DecryptAndDecodeAudioCallback callback,
                      Status status,
                      const media::Decryptor::AudioFrames& frames);
  void OnVideoDecoded(DecryptAndDecodeVideoCallback callback,
                      Status status,
                      scoped_refptr<VideoFrame> frame);

  // Returns audio/video buffer reader according to the |stream_type|.
  MojoDecoderBufferReader* GetBufferReader(StreamType stream_type) const;

  // Helper classes to receive encrypted DecoderBuffer from the client.
  std::unique_ptr<MojoDecoderBufferReader> audio_buffer_reader_;
  std::unique_ptr<MojoDecoderBufferReader> video_buffer_reader_;
  std::unique_ptr<MojoDecoderBufferReader> decrypt_buffer_reader_;

  // Helper class to send decrypted DecoderBuffer to the client.
  std::unique_ptr<MojoDecoderBufferWriter> decrypted_buffer_writer_;

  media::Decryptor* decryptor_;

  // Holds the CdmContextRef to keep the CdmContext alive for the lifetime of
  // the |decryptor_|.
  std::unique_ptr<CdmContextRef> cdm_context_ref_;

  base::WeakPtr<MojoDecryptorService> weak_this_;
  base::WeakPtrFactory<MojoDecryptorService> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(MojoDecryptorService);
};

}  // namespace media

#endif  // MEDIA_MOJO_SERVICES_MOJO_DECRYPTOR_SERVICE_H_
