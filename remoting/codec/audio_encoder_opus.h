// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_CODEC_AUDIO_ENCODER_OPUS_H_
#define REMOTING_CODEC_AUDIO_ENCODER_OPUS_H_

#include "base/memory/aligned_memory.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_span.h"
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
  // Helper class which segments samples into chunks, while minimizing copies.
  // Exposed as an interface here for ease of testing.
  class ResamplerFifo {
   public:
    virtual ~ResamplerFifo() = default;

    // Add samples to the FIFO, without copying them.
    virtual void AddNewSamples(base::span<const int16_t> samples) = 0;

    // Copies unused samples added by `AddNewSamples()` to internal storage.
    virtual void SaveNewSamples() = 0;

    // Consumes samples from the FIFO.
    virtual base::span<const int16_t> TakeChunk() = 0;

    // Returns the number of samples currently in the FIFO, saved or not.
    virtual size_t remaining_samples() const = 0;

    // Returns the size of each chunk returned by `TakeChunk()`.
    virtual size_t GetChunkSizeForTesting() const = 0;
  };

  AudioEncoderOpus();

  AudioEncoderOpus(const AudioEncoderOpus&) = delete;
  AudioEncoderOpus& operator=(const AudioEncoderOpus&) = delete;

  ~AudioEncoderOpus() override;

  // AudioEncoder interface.
  std::unique_ptr<AudioPacket> Encode(
      std::unique_ptr<AudioPacket> packet) override;
  int GetBitrate() override;

  static std::unique_ptr<ResamplerFifo> GetEmptyFifoForTesting(
      size_t size_in_frames,
      size_t channels);

 private:
  void InitEncoder();
  void DestroyEncoder();
  bool ResetForPacket(AudioPacket* packet);

  std::unique_ptr<AudioPacket> CreatePacket();

  std::unique_ptr<AudioPacket> EncodeInternal(base::span<const int16_t> data);
  std::unique_ptr<AudioPacket> EncodeInternalWithResampling(
      base::span<const int16_t> data);

  void FetchBytesToResample(int resampler_frame_delay,
                            media::AudioBus* audio_bus);

  bool EncodeData(base::span<const int16_t> data, AudioPacket* destination);

  bool needs_resampling_ = false;

  // Holds samples (always at 48kHz) that have not yet been encoded.
  base::AlignedHeapArray<int16_t> encoder_input_;

  // The portion of `encoder_input_` which contains samples that have not yet
  // been encoded.
  // Unused when `needs_resampling_` is true, since extra samples will be stored
  // in `resampler_fifo_` instead.
  base::raw_span<int16_t> leftover_encoder_samples_;

  // Number of samples needed to encode a single "Opus frame".
  size_t encoder_samples_needed_ = 0;

  // Manages samples which have not been resampled yet.
  std::unique_ptr<ResamplerFifo> resampler_fifo_;

  // The minimum number of samples needed to guarantee to have enough for
  // one resample call.
  size_t resampling_samples_needed_ = 0;

  int sampling_rate_ = 0;
  AudioPacket::Channels channels_ = AudioPacket::CHANNELS_STEREO;
  raw_ptr<OpusEncoder, DanglingUntriaged> encoder_ = nullptr;

  std::unique_ptr<media::MultiChannelResampler> resampler_;
  std::unique_ptr<media::AudioBus> resampler_bus_;
};

}  // namespace remoting

#endif  // REMOTING_CODEC_AUDIO_ENCODER_OPUS_H_
