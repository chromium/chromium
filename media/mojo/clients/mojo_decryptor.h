// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MOJO_CLIENTS_MOJO_DECRYPTOR_H_
#define MEDIA_MOJO_CLIENTS_MOJO_DECRYPTOR_H_

#include <memory>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"
#include "media/base/decryptor.h"
#include "media/mojo/mojom/decryptor.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace media {

class MojoDecoderBufferReader;
class MojoDecoderBufferWriter;

// A Decryptor implementation based on mojo::PendingRemote<mojom::Decryptor>.
// This class is single threaded. The |remote_decryptor| is connected before
// being passed to MojoDecryptor, but it is bound to the thread MojoDecryptor
// lives on the first time it is used in this class.
class MojoDecryptor final : public Decryptor {
 public:
  // |writer_capacity| can be used for testing. If 0, default writer capacity
  // will be used.
  MojoDecryptor(mojo::PendingRemote<mojom::Decryptor> remote_decryptor,
                uint32_t writer_capacity = 0);

  MojoDecryptor(const MojoDecryptor&) = delete;
  MojoDecryptor& operator=(const MojoDecryptor&) = delete;

  ~MojoDecryptor() final;

  // Decryptor implementation.
  void Decrypt(StreamType stream_type,
               scoped_refptr<DecoderBuffer> encrypted,
               DecryptCB decrypt_cb) final;
  void CancelDecrypt(StreamType stream_type) final;
  void InitializeAudioDecoder(const AudioDecoderConfig& config,
                              DecoderInitCB init_cb) final;
  void InitializeVideoDecoder(const VideoDecoderConfig& config,
                              DecoderInitCB init_cb) final;
  void DecryptAndDecodeAudio(scoped_refptr<DecoderBuffer> encrypted,
                             AudioDecodeCB audio_decode_cb) final;
  void DecryptAndDecodeVideo(scoped_refptr<DecoderBuffer> encrypted,
                             VideoDecodeCB video_decode_cb) final;
  void ResetDecoder(StreamType stream_type) final;
  void DeinitializeDecoder(StreamType stream_type) final;

 private:
  // Called when a buffer is decrypted.
  void OnBufferDecrypted(DecryptCB decrypt_cb,
                         Status status,
                         mojom::DecoderBufferPtr buffer);
  void OnBufferRead(DecryptCB decrypt_cb,
                    Status status,
                    scoped_refptr<DecoderBuffer> buffer);
  void OnAudioDecoded(AudioDecodeCB audio_decode_cb,
                      Status status,
                      std::vector<mojom::AudioBufferPtr> audio_buffers);
  void OnVideoDecoded(
      VideoDecodeCB video_decode_cb,
      Status status,
      const scoped_refptr<VideoFrame>& video_frame,
      mojo::PendingRemote<mojom::FrameResourceReleaser> releaser);

  void OnConnectionError(uint32_t custom_reason,
                         const std::string& description);

  // Helper class to get the correct MojoDecoderBufferWriter;
  MojoDecoderBufferWriter* GetWriter(StreamType stream_type);

  base::ThreadChecker thread_checker_;

  mojo::Remote<mojom::Decryptor> remote_decryptor_;

  // Helper class to send DecoderBuffer to the |remote_decryptor_| for
  // DecryptAndDecodeAudio(), DecryptAndDecodeVideo() and Decrypt().
  std::unique_ptr<MojoDecoderBufferWriter> audio_buffer_writer_;
  std::unique_ptr<MojoDecoderBufferWriter> video_buffer_writer_;
  std::unique_ptr<MojoDecoderBufferWriter> decrypt_buffer_writer_;

  // Helper class to receive decrypted DecoderBuffer from the
  // |remote_decryptor_|, shared by audio and video.
  std::unique_ptr<MojoDecoderBufferReader> decrypted_buffer_reader_;

  base::WeakPtrFactory<MojoDecryptor> weak_factory_{this};
};

}  // namespace media

#endif  // MEDIA_MOJO_CLIENTS_MOJO_DECRYPTOR_H_
