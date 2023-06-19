// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MOJO_CLIENTS_MOJO_AUDIO_DECODER_H_
#define MEDIA_MOJO_CLIENTS_MOJO_AUDIO_DECODER_H_

#include <memory>

#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "media/base/audio_decoder.h"
#include "media/mojo/mojom/audio_decoder.mojom.h"
#include "media/mojo/mojom/media_types.mojom.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace base {
class SequencedTaskRunner;
}

namespace media {

class MediaLog;
class MojoDecoderBufferWriter;

// An AudioDecoder that proxies to a mojom::AudioDecoder.
class MojoAudioDecoder final : public AudioDecoder,
                               public mojom::AudioDecoderClient {
 public:
  MojoAudioDecoder(scoped_refptr<base::SequencedTaskRunner> task_runner,
                   MediaLog* media_log,
                   mojo::PendingRemote<mojom::AudioDecoder> remote_decoder);

  MojoAudioDecoder(const MojoAudioDecoder&) = delete;
  MojoAudioDecoder& operator=(const MojoAudioDecoder&) = delete;

  ~MojoAudioDecoder() final;

  // Decoder implementation
  bool IsPlatformDecoder() const final;
  bool SupportsDecryption() const final;
  AudioDecoderType GetDecoderType() const override;

  // AudioDecoder implementation.
  void Initialize(const AudioDecoderConfig& config,
                  CdmContext* cdm_context,
                  InitCB init_cb,
                  const OutputCB& output_cb,
                  const WaitingCB& waiting_cb) final;
  void Decode(scoped_refptr<DecoderBuffer> buffer, DecodeCB decode_cb) final;
  void Reset(base::OnceClosure closure) final;
  bool NeedsBitstreamConversion() const final;

  // AudioDecoderClient implementation.
  void OnBufferDecoded(mojom::AudioBufferPtr buffer) final;
  void OnWaiting(WaitingReason reason) final;

  void set_writer_capacity_for_testing(uint32_t capacity) {
    writer_capacity_ = capacity;
  }

 private:
  void BindRemoteDecoder();

  // Callback for connection error on |remote_decoder_|.
  void OnConnectionError();

  // Fail an initialization with a Status.
  void FailInit(InitCB init_cb, DecoderStatus err);

  // Called when |remote_decoder_| finished initialization.
  void OnInitialized(const DecoderStatus& status,
                     bool needs_bitstream_conversion,
                     AudioDecoderType decoder_type);

  // Called when |remote_decoder_| accepted or rejected DecoderBuffer.
  void OnDecodeStatus(const DecoderStatus& decode_status);

  // called when |remote_decoder_| finished Reset() sequence.
  void OnResetDone();

  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  SEQUENCE_CHECKER(sequence_checker_);

  // This class is constructed on one thread and used exclusively on another
  // thread. This member is used to safely pass the
  // mojo::PendingRemote<AudioDecoder> from one thread to another. It is set in
  // the constructor and is consumed in Initialize().
  mojo::PendingRemote<mojom::AudioDecoder> pending_remote_decoder_;

  mojo::Remote<mojom::AudioDecoder> remote_decoder_;

  std::unique_ptr<MojoDecoderBufferWriter> mojo_decoder_buffer_writer_;

  uint32_t writer_capacity_ = 0;

  // Receiver for AudioDecoderClient, bound to the |task_runner_|.
  mojo::AssociatedReceiver<AudioDecoderClient> client_receiver_{this};

  // Raw pointer is safe since both `this` and the `media_log` are owned by
  // WebMediaPlayerImpl with the correct declaration order.
  raw_ptr<MediaLog, DanglingUntriaged> media_log_;

  InitCB init_cb_;
  OutputCB output_cb_;
  WaitingCB waiting_cb_;

  // |decode_cb_| and |reset_cb_| are replaced by every by Decode() and Reset().
  DecodeCB decode_cb_;
  base::OnceClosure reset_cb_;

  // Flag telling whether this decoder requires bitstream conversion.
  // Passed from |remote_decoder_| as a result of its initialization.
  bool needs_bitstream_conversion_ = false;
  AudioDecoderType decoder_type_ = AudioDecoderType::kUnknown;
};

}  // namespace media

#endif  // MEDIA_MOJO_CLIENTS_MOJO_AUDIO_DECODER_H_
