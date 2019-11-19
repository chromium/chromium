// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIARECORDER_AUDIO_TRACK_OPUS_ENCODER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIARECORDER_AUDIO_TRACK_OPUS_ENCODER_H_

#include <memory>

#include "base/macros.h"
#include "media/base/audio_bus.h"
#include "media/base/audio_converter.h"
#include "media/base/audio_fifo.h"
#include "media/base/audio_parameters.h"
#include "third_party/blink/renderer/modules/mediarecorder/audio_track_encoder.h"

#include "third_party/opus/src/include/opus.h"

namespace blink {

// Class encapsulating Opus-related encoding details. It contains an
// AudioConverter to adapt incoming data to the format Opus likes to have.
class AudioTrackOpusEncoder : public AudioTrackEncoder,
                              public media::AudioConverter::InputCallback {
 public:
  AudioTrackOpusEncoder(OnEncodedAudioCB on_encoded_audio_cb,
                        int32_t bits_per_second);

  void OnSetFormat(const media::AudioParameters& params) override;
  void EncodeAudio(std::unique_ptr<media::AudioBus> input_bus,
                   base::TimeTicks capture_time) override;

 private:
  ~AudioTrackOpusEncoder() override;

  bool is_initialized() const { return !!opus_encoder_; }

  void DestroyExistingOpusEncoder();

  // media::AudioConverted::InputCallback implementation.
  double ProvideInput(media::AudioBus* audio_bus,
                      uint32_t frames_delayed) override;

  // Target bitrate for Opus. If 0, Opus provide automatic bitrate is used.
  const int32_t bits_per_second_;

  // Output parameters after audio conversion. This differs from the input
  // parameters only in sample_rate() and frames_per_buffer(): output should be
  // 48ksamples/s and 2880, respectively.
  media::AudioParameters converted_params_;

  // Sample rate adapter from the input audio to what OpusEncoder desires.
  std::unique_ptr<media::AudioConverter> converter_;

  // Buffer for holding the original input audio before it goes to the
  // converter.
  std::unique_ptr<media::AudioFifo> fifo_;

  // Buffer for passing AudioBus data from the converter to the encoder.
  std::unique_ptr<float[]> buffer_;

  OpusEncoder* opus_encoder_;

  DISALLOW_COPY_AND_ASSIGN(AudioTrackOpusEncoder);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIARECORDER_AUDIO_TRACK_OPUS_ENCODER_H_
