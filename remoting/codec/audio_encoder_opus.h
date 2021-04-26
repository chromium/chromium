// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_CODEC_AUDIO_ENCODER_OPUS_H_
#define REMOTING_CODEC_AUDIO_ENCODER_OPUS_H_

#include "remoting/codec/audio_encoder.h"

#include <stdint.h>

#include "base/macros.h"
#include "remoting/proto/audio.pb.h"

struct OpusEncoder;

namespace media {
class AudioBus;
class MultiChannelResampler;
}  // namespace media

namespace remoting {

class AudioPacket;

class AudioEncoderOpus : public AudioEncoder {
 public:
  AudioEncoderOpus();
  ~AudioEncoderOpus() override;

  // AudioEncoder interface.
  std::unique_ptr<AudioPacket> Encode(
      std::unique_ptr<AudioPacket> packet) override;
  int GetBitrate() override;

 private:
  void InitEncoder();
  void DestroyEncoder();
  bool ResetForPacket(AudioPacket* packet);

  void FetchBytesToResample(int resampler_frame_delay,
                            media::AudioBus* audio_bus);

  int sampling_rate_;
  AudioPacket::Channels channels_;
  OpusEncoder* encoder_;

  int frame_size_;
  std::unique_ptr<media::MultiChannelResampler> resampler_;
  std::unique_ptr<char[]> resample_buffer_;
  std::unique_ptr<media::AudioBus> resampler_bus_;

  // Used to pass packet to the FetchBytesToResampler() callback.
  const char* resampling_data_;
  int resampling_data_size_;
  int resampling_data_pos_;

  // Left-over unencoded samples from the previous AudioPacket.
  std::unique_ptr<int16_t[]> leftover_buffer_;
  int leftover_buffer_size_;
  int leftover_samples_;

  DISALLOW_COPY_AND_ASSIGN(AudioEncoderOpus);
};

}  // namespace remoting

#endif  // REMOTING_CODEC_AUDIO_ENCODER_OPUS_H_
