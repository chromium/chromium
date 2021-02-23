// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_AUDIO_AUDIO_OPUS_ENCODER_H_
#define MEDIA_AUDIO_AUDIO_OPUS_ENCODER_H_

#include <memory>
#include <vector>

#include "base/optional.h"
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
  AudioOpusEncoder();
  AudioOpusEncoder(const AudioOpusEncoder&) = delete;
  AudioOpusEncoder& operator=(const AudioOpusEncoder&) = delete;
  ~AudioOpusEncoder() override;

  // AudioEncoder:
  void Initialize(const Options& options,
                  OutputCB output_callback,
                  StatusCB done_cb) override;

  void Encode(std::unique_ptr<AudioBus> audio_bus,
              base::TimeTicks capture_time,
              StatusCB done_cb) override;

  void Flush(StatusCB done_cb) override;

 private:
  // Called synchronously by |fifo_| once enough audio frames have been
  // buffered. Calls libopus to do actual encoding.
  void OnFifoOutput(const AudioBus& output_bus, int frame_delay);

  void FlushInternal();

  CodecDescription PrepareExtraData();

  StatusOr<OwnedOpusEncoder> CreateOpusEncoder();

  // Computes the timestamp of an AudioBus which has |num_frames| and was
  // captured at |capture_time|. This timestamp is the capture time of the first
  // sample in that AudioBus.
  base::TimeTicks ComputeTimestamp(int num_frames,
                                   base::TimeTicks capture_time) const;

  AudioParameters input_params_;

  // Output parameters after audio conversion. This may differ from the input
  // params in the number of channels, sample rate, and the frames per buffer.
  // (See CreateOpusInputParams() in the .cc file for details).
  AudioParameters converted_params_;

  // Sample rate adapter from the input audio to what OpusEncoder desires.
  std::unique_ptr<AudioConverter> converter_;

  // Buffer for holding the original input audio before it goes to the
  // converter.
  std::unique_ptr<AudioPushFifo> fifo_;

  // This is the destination AudioBus where the |converter_| teh audio into.
  std::unique_ptr<AudioBus> converted_audio_bus_;

  // Buffer for passing AudioBus data from the converter to the encoder.
  std::vector<float> buffer_;

  // The actual libopus encoder instance. This is nullptr if creating the
  // encoder fails.
  OwnedOpusEncoder opus_encoder_;

  // The capture time of the most recent |audio_bus| delivered to
  // EncodeAudio().
  base::TimeTicks last_capture_time_;

  // If FlushImpl() was called while |fifo_| has some frames but not full yet,
  // this will be the number of flushed frames, which is used to compute the
  // timestamp provided in the output |EncodedAudioBuffer|.
  base::Optional<int> number_of_flushed_frames_;

  // Timestamp that should be reported by the next call of |output_callback_|
  base::TimeTicks next_timestamp_;

  // Callback for reporting completion and status of the current Flush() or
  // Encoder()
  StatusCB current_done_cb_;

  // True if the next output needs to have extra_data in it, only happens once.
  bool need_to_emit_extra_data_ = true;
};

}  // namespace media

#endif  // MEDIA_AUDIO_AUDIO_OPUS_ENCODER_H_
