// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_CODEC_AUDIO_DECODER_OPUS_H_
#define REMOTING_CODEC_AUDIO_DECODER_OPUS_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "remoting/codec/audio_decoder.h"

struct OpusDecoder;

namespace remoting {

class AudioPacket;

class AudioDecoderOpus : public AudioDecoder {
 public:
  AudioDecoderOpus();

  AudioDecoderOpus(const AudioDecoderOpus&) = delete;
  AudioDecoderOpus& operator=(const AudioDecoderOpus&) = delete;

  ~AudioDecoderOpus() override;

  // AudioDecoder interface.
  std::unique_ptr<AudioPacket> Decode(
      std::unique_ptr<AudioPacket> packet) override;

 private:
  void InitDecoder();
  void DestroyDecoder();
  bool ResetForPacket(AudioPacket* packet);

  int sampling_rate_;
  int channels_;
  raw_ptr<OpusDecoder, DanglingUntriaged> decoder_;
};

}  // namespace remoting

#endif  // REMOTING_CODEC_AUDIO_DECODER_OPUS_H_
