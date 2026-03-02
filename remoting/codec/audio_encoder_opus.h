// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_CODEC_AUDIO_ENCODER_OPUS_H_
#define REMOTING_CODEC_AUDIO_ENCODER_OPUS_H_

#include "base/memory/aligned_memory.h"
#include "base/memory/raw_ptr.h"
#include "remoting/codec/audio_encoder.h"
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

  AudioEncoderOpus(const AudioEncoderOpus&) = delete;
  AudioEncoderOpus& operator=(const AudioEncoderOpus&) = delete;

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

  int sampling_rate_ = 0;
  AudioPacket::Channels channels_ = AudioPacket::CHANNELS_STEREO;
  raw_ptr<OpusEncoder, DanglingUntriaged> encoder_ = nullptr;

  int frame_size_ = 0;
  std::unique_ptr<media::MultiChannelResampler> resampler_;
  base::AlignedHeapArray<int16_t> resample_buffer_;
  std::unique_ptr<media::AudioBus> resampler_bus_;

  // Used to pass packet to the FetchBytesToResampler() callback.
  const char* resampling_data_ = nullptr;
  int resampling_data_size_ = 0;
  int resampling_data_pos_ = 0;

  // Left-over unencoded samples from the previous AudioPacket.
  std::unique_ptr<int16_t[]> leftover_samples_;
  int leftover_samples_size_in_frames_ = 0;
  int leftover_frames_ = 0;
};

}  // namespace remoting

#endif  // REMOTING_CODEC_AUDIO_ENCODER_OPUS_H_
