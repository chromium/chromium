// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_AUDIO_AUDIO_OPUS_ENCODER_H_
#define MEDIA_AUDIO_AUDIO_OPUS_ENCODER_H_

#include <memory>
#include <vector>

#include "media/base/audio_bus.h"
#include "media/base/audio_converter.h"
#include "media/base/audio_encoder.h"
#include "media/base/audio_push_fifo.h"
#include "third_party/opus/src/include/opus.h"

namespace media {

using OpusEncoderDeleterType = void (*)(OpusEncoder* encoder_ptr);
using OwnedOpusEncoder = std::unique_ptr<OpusEncoder, OpusEncoderDeleterType>;

// Performs Opus encoding of the input audio. The input audio is converted to a
// a format suitable for Opus before it is passed to the libopus encoder
// instance to do the actual encoding.
class MEDIA_EXPORT AudioOpusEncoder : public AudioEncoder {
 public:
  AudioOpusEncoder(const AudioParameters& input_params,
                   EncodeCB encode_callback,
                   StatusCB status_callback,
                   int32_t opus_bitrate);
  AudioOpusEncoder(const AudioOpusEncoder&) = delete;
  AudioOpusEncoder& operator=(const AudioOpusEncoder&) = delete;
  ~AudioOpusEncoder() override;

 protected:
  // AudioEncoder:
  void EncodeAudioImpl(const AudioBus& audio_bus,
                       base::TimeTicks capture_time) override;

 private:
  // Called synchronously by |fifo_| once enough audio frames have been
  // buffered.
  void OnFifoOutput(const AudioBus& output_bus, int frame_delay);

  // Target bitrate for Opus. If 0, Opus-provided automatic bitrate is used.
  // Note: As of 2013-10-31, the encoder in "auto bitrate" mode would use a
  // variable bitrate up to 102 kbps for 2-channel, 48 kHz audio and a 10 ms
  // buffer duration. The Opus library authors may, of course, adjust this in
  // later versions.
  const int32_t bits_per_second_;

  // Output parameters after audio conversion. This may differ from the input
  // params in the number of channels, sample rate, and the frames per buffer.
  // (See CreateOpusInputParams() in the .cc file for details).
  AudioParameters converted_params_;

  // Sample rate adapter from the input audio to what OpusEncoder desires.
  AudioConverter converter_;

  // Buffer for holding the original input audio before it goes to the
  // converter.
  AudioPushFifo fifo_;

  // This is the destination AudioBus where the |converter_| teh audio into.
  std::unique_ptr<AudioBus> converted_audio_bus_;

  // Buffer for passing AudioBus data from the converter to the encoder.
  std::vector<float> buffer_;

  // The actual libopus encoder instance. This is nullptr if creating the
  // encoder fails.
  OwnedOpusEncoder opus_encoder_;
};

}  // namespace media

#endif  // MEDIA_AUDIO_AUDIO_OPUS_ENCODER_H_
