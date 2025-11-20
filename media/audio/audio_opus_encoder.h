// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_AUDIO_AUDIO_OPUS_ENCODER_H_
#define MEDIA_AUDIO_AUDIO_OPUS_ENCODER_H_

#include <array>
#include <memory>
#include <vector>

#include "base/containers/heap_array.h"
#include "media/base/audio_converter.h"
#include "media/base/audio_encoder.h"
#include "media/base/audio_timestamp_helper.h"
#include "third_party/opus/src/include/opus.h"

namespace media {

class AudioBus;
class ChannelMixer;
class ConvertingAudioFifo;

using OpusEncoderDeleterType = void (*)(OpusEncoder* encoder_ptr);
using OwnedOpusEncoder = std::unique_ptr<OpusEncoder, OpusEncoderDeleterType>;

using OpusRepacketizerDeleterType =
    void (*)(OpusRepacketizer* repacketizer_ptr);
using OwnedOpusRepacketizer =
    std::unique_ptr<OpusRepacketizer, OpusRepacketizerDeleterType>;

using EncodingBuffer = base::HeapArray<uint8_t>;

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

  void EmitEncodedBuffer(size_t encoded_data_size);

  base::span<uint8_t> GetEncoderDestination();

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

  // Fixed size buffer that all frames are encoded to. Most encoded data is
  // generally only a few hundred bytes, so we copy out from this buffer when
  // vending encoded packets.
  std::array<uint8_t, kOpusMaxDataBytes> encoding_buffer_;

  // True if the next output needs to have extra_data in it, only happens once.
  bool need_to_emit_extra_data_ = true;

  // For bundling several opus frames into a single packet.
  OwnedOpusRepacketizer opus_repacketizer_;

  // Counts the total number of packets (also known as Opus frames) in the
  // repacketizer. Resets to zero every time we Emit an EncodedAudioBuffer. We
  // manually count the packets rather than use the convenience method
  // opus_repacketizer_get_nb_frames() due to how it measures frames. For
  // example, it can correctly deduce 5ms as two 2.5ms frames, but will not
  // count in 40ms or 60ms frames. It uses 20ms in these cases, requiring us to
  // track the total packets ourselves.
  size_t packets_in_repacketizer_ = 0;

  // This value is the number of intermediate durations that will fit in the
  // final duration.
  size_t max_packets_in_repacketizer_ = 1;

  // This is the frame duration calculated from the input params opus options.
  base::TimeDelta final_frame_duration_;

  // We add silence until we emit an encoded buffer to guarantee all audio has
  // been encoded.
  bool waiting_for_output_ = false;

  // If using `opus_repacketizer_`, this contains the size of smaller packets to
  // be concatenated into a large one of size |final_frame_duration_|.
  // Otherwise unused.
  size_t intermediate_frame_count_ = 0;

  // Holds encoded intermediate packets for the repacketizer to ensure
  // the data pointers remain valid until the combined packet is emitted.
  std::vector<EncodingBuffer> pending_packets_;
};

}  // namespace media

#endif  // MEDIA_AUDIO_AUDIO_OPUS_ENCODER_H_
