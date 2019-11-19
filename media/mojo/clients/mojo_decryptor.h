// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MOJO_CLIENTS_MOJO_DECRYPTOR_H_
#define MEDIA_MOJO_CLIENTS_MOJO_DECRYPTOR_H_

#include <memory>
#include <vector>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"
#include "media/base/decryptor.h"
#include "media/mojo/mojom/decryptor.mojom.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace media {

class MojoDecoderBufferReader;
class MojoDecoderBufferWriter;

// A Decryptor implementation based on mojom::DecryptorPtr.
// This class is single threaded. The |remote_decryptor| is connected before
// being passed to MojoDecryptor, but it is bound to the thread MojoDecryptor
// lives on the first time it is used in this class.
class MojoDecryptor : public Decryptor {
 public:
  // |writer_capacity| can be used for testing. If 0, default writer capacity
  // will be used.
  MojoDecryptor(mojo::PendingRemote<mojom::Decryptor> remote_decryptor,
                uint32_t writer_capacity = 0);
  ~MojoDecryptor() final;

  // Decryptor implementation.
  void RegisterNewKeyCB(StreamType stream_type,
                        const NewKeyCB& key_added_cb) final;
  void Decrypt(StreamType stream_type,
               scoped_refptr<DecoderBuffer> encrypted,
               const DecryptCB& decrypt_cb) final;
  void CancelDecrypt(StreamType stream_type) final;
  void InitializeAudioDecoder(const AudioDecoderConfig& config,
                              const DecoderInitCB& init_cb) final;
  void InitializeVideoDecoder(const VideoDecoderConfig& config,
                              const DecoderInitCB& init_cb) final;
  void DecryptAndDecodeAudio(scoped_refptr<DecoderBuffer> encrypted,
                             const AudioDecodeCB& audio_decode_cb) final;
  void DecryptAndDecodeVideo(scoped_refptr<DecoderBuffer> encrypted,
                             const VideoDecodeCB& video_decode_cb) final;
  void ResetDecoder(StreamType stream_type) final;
  void DeinitializeDecoder(StreamType stream_type) final;

  // Called when keys have changed and an additional key is available.
  void OnKeyAdded();

 private:
  // These are once callbacks corresponding to repeating callbacks DecryptCB,
  // DecoderInitCB, AudioDecodeCB and VideoDecodeCB. They are needed so that we
  // can use WrapCallbackWithDefaultInvokeIfNotRun to make sure callbacks always
  // run.
  // TODO(xhwang): Update Decryptor to use OnceCallback. The change is easy,
  // but updating tests is hard given gmock doesn't support move-only types.
  // See http://crbug.com/751838
  using DecryptOnceCB = base::OnceCallback<DecryptCB::RunType>;
  using DecoderInitOnceCB = base::OnceCallback<DecoderInitCB::RunType>;
  using AudioDecodeOnceCB = base::OnceCallback<AudioDecodeCB::RunType>;
  using VideoDecodeOnceCB = base::OnceCallback<VideoDecodeCB::RunType>;

  // Called when a buffer is decrypted.
  void OnBufferDecrypted(DecryptOnceCB decrypt_cb,
                         Status status,
                         mojom::DecoderBufferPtr buffer);
  void OnBufferRead(DecryptOnceCB decrypt_cb,
                    Status status,
                    scoped_refptr<DecoderBuffer> buffer);
  void OnAudioDecoded(AudioDecodeOnceCB audio_decode_cb,
                      Status status,
                      std::vector<mojom::AudioBufferPtr> audio_buffers);
  void OnVideoDecoded(VideoDecodeOnceCB video_decode_cb,
                      Status status,
                      const scoped_refptr<VideoFrame>& video_frame,
                      mojom::FrameResourceReleaserPtr releaser);

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

  NewKeyCB new_audio_key_cb_;
  NewKeyCB new_video_key_cb_;

  base::WeakPtrFactory<MojoDecryptor> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(MojoDecryptor);
};

}  // namespace media

#endif  // MEDIA_MOJO_CLIENTS_MOJO_DECRYPTOR_H_
