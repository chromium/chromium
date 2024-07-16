// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_AUDIO_AUDIO_OPUS_ENCODER_H_
#define MEDIA_AUDIO_AUDIO_OPUS_ENCODER_H_

#include <memory>
#include <vector>

#include "media/base/audio_bus.h"
#include "media/base/audio_converter.h"
#include "media/base/audio_encoder.h"
#include "media/base/audio_timestamp_helper.h"
#include "third_party/opus/src/include/opus.h"

namespace media {

class ChannelMixer;
class ConvertingAudioFifo;

using OpusEncoderDeleterType = void (*)(OpusEncoder* encoder_ptr);
using OwnedOpusEncoder = std::unique_ptr<OpusEncoder, OpusEncoderDeleterType>;

// Performs Opus encoding of the input audio. The input audio is converted to a
// a format suitable for Opus before it is passed to the libopus encoder
// instance to do the actual encoding.
class MEDIA_EXPORT AudioOpusEncoder : public AudioEncoder {
 public:
  AudioOpusEncoder();
  AudioOpusEncoder(const AudioOpusEncoder&) = delete;
  AudioOpusEncoder& operator=(const AudioOpusEncoder&) = delete;
  ~AudioOpusEncoder() override;

  // AudioEncoder:
  void Initialize(const Options& options,
                  OutputCB output_callback,
                  EncoderStatusCB done_cb) override;

  void Encode(std::unique_ptr<AudioBus> audio_bus,
              base::TimeTicks capture_time,
              EncoderStatusCB done_cb) override;

  void Flush(EncoderStatusCB done_cb) override;

  static constexpr int kMinBitrate = 6000;

 private:
  friend class AudioEncodersTest;

  // Calls libopus to do actual encoding.
  void DoEncode(const AudioBus* audio_bus);

  void DrainFifoOutput();

  CodecDescription PrepareExtraData();

  EncoderStatus::Or<OwnedOpusEncoder> CreateOpusEncoder(
      const std::optional<AudioEncoder::OpusOptions>& opus_options);

  AudioParameters input_params_;

  // Output parameters after audio conversion. This may differ from the input
  // params in the number of channels, sample rate, and the frames per buffer.
  // (See CreateOpusInputParams() in the .cc file for details).
  AudioParameters converted_params_;

  std::unique_ptr<ConvertingAudioFifo> fifo_;
  bool fifo_has_data_ = false;

  // Used to mix incoming Encode() buffers to match the expect input channel
  // count.
  std::unique_ptr<ChannelMixer> mixer_;
  AudioParameters mixer_input_params_;

  // Buffer for passing AudioBus data from the converter to the encoder.
  std::vector<float> buffer_;

  // The actual libopus encoder instance. This is nullptr if creating the
  // encoder fails.
  OwnedOpusEncoder opus_encoder_;

  // Keeps track of the timestamps for the each |output_callback_|
  std::unique_ptr<AudioTimestampHelper> timestamp_tracker_;

  // Callback for reporting completion and status of the current Flush() or
  // Encoder()
  EncoderStatusCB current_done_cb_;

  // Recommended value for opus_encode_float(), according to documentation in
  // third_party/opus/src/include/opus.h, so that the Opus encoder does not
  // degrade the audio due to memory constraints, and is independent of the
  // duration of the encoded buffer.
  static inline constexpr int kOpusMaxDataBytes = 4000;

  // Fixed size buffer that all frames are encoded too. Most encoded data is
  // generally only a few hundred bytes, so we copy out from this buffer when
  // vending encoded packets.
  std::array<uint8_t, kOpusMaxDataBytes> encoding_buffer_;

  // True if the next output needs to have extra_data in it, only happens once.
  bool need_to_emit_extra_data_ = true;
};

}  // namespace media

#endif  // MEDIA_AUDIO_AUDIO_OPUS_ENCODER_H_
