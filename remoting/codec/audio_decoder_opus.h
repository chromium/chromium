// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_CODEC_AUDIO_DECODER_OPUS_H_
#define REMOTING_CODEC_AUDIO_DECODER_OPUS_H_

#include <memory>

#include "base/macros.h"
#include "remoting/codec/audio_decoder.h"

struct OpusDecoder;

namespace remoting {

class AudioPacket;

class AudioDecoderOpus : public AudioDecoder {
 public:
  AudioDecoderOpus();
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
  OpusDecoder* decoder_;

  DISALLOW_COPY_AND_ASSIGN(AudioDecoderOpus);
};

}  // namespace remoting

#endif  // REMOTING_CODEC_AUDIO_DECODER_OPUS_H_
