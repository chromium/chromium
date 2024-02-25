// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_CONVERTING_AUDIO_FIFO_H_
#define MEDIA_BASE_CONVERTING_AUDIO_FIFO_H_

#include <memory>

#include "base/containers/circular_deque.h"
#include "base/sequence_checker.h"
#include "media/base/audio_converter.h"
#include "media/base/audio_parameters.h"
#include "media/base/media_export.h"

namespace media {

class AudioBus;
class AudioBusPool;
class ChannelMixer;

// FIFO which uses an AudioConverter to convert input frames into an output
// format. When enough input frames are pushed into the FIFO, it converts them
// synchronously, and notifies the availability of that output via its
// |output_ready_callback_|.
class MEDIA_EXPORT ConvertingAudioFifo final
    : public AudioConverter::InputCallback {
 public:
  ConvertingAudioFifo(const AudioParameters& input_params,
                      const AudioParameters& converted_params);

  ConvertingAudioFifo(const ConvertingAudioFifo&) = delete;
  ConvertingAudioFifo& operator=(const ConvertingAudioFifo&) = delete;

  ~ConvertingAudioFifo() override;

  // Adds inputs into the FIFO. `input_bus` must have the same sample rate as
  // |input_params_|, but the number of channels or frames can be different.
  void Push(std::unique_ptr<AudioBus> input_bus);

  // Returns whether there is any available converted output.
  bool HasOutput();

  // Gets the current output.
  const AudioBus* PeekOutput();

  // Releases the current output.
  void PopOutput();

  // Forces all remaining frames to be converted, ouputing silence in case there
  // isn't enough data. Noop if there aren't any available frames.
  void Flush();

  int min_number_input_frames_needed_for_testing() {
    return min_input_frames_needed_;
  }

 private:
  friend class ConvertingAudioFifoTest;

  // AudioConverter::InputCallback implementation.
  double ProvideInput(AudioBus* audio_bus,
                      uint32_t frames_delayed,
                      const AudioGlitchInfo& glitch_info) override;

  // Consumes frames from |inputs_|, converts them to match
  // |output_params_| fills |dest|.
  void Convert();

  // Returns an AudioBus with the same number of channels as |input_params_|,
  // mixing |audio_bus| if necessary.
  std::unique_ptr<AudioBus> EnsureExpectedChannelCount(
      std::unique_ptr<AudioBus> audio_bus);

  const AudioParameters input_params_;
  const AudioParameters converted_params_;

  bool is_flushing_ = false;

  // Index to the first unused frame in |inputs_.front()|.
  int front_frame_index_ = 0;

  // Total number of input frames currently in the FIFO.
  int total_frames_ = 0;

  // Number of input frames needed to fully fulfill one Convert() call.
  int min_input_frames_needed_;

  // Used to mix incoming Push()'ed buffers to match the |input_params_|'s
  // channel count. This is needed because we can receive multiple inputs with
  // varying channel counts before having the |min_input_frames_needed_|
  // to satisfy one round of conversions. If we created a new |converter_| each
  // time the input channel changes, we would risk introducing silence when
  // flushing (or losing data if we didn't flush).
  std::unique_ptr<ChannelMixer> mixer_ GUARDED_BY_CONTEXT(sequence_checker_);
  AudioParameters mixer_input_params_ GUARDED_BY_CONTEXT(sequence_checker_);

  // Synchronously performs audio conversions.
  std::unique_ptr<AudioConverter> converter_
      GUARDED_BY_CONTEXT(sequence_checker_);

  base::circular_deque<std::unique_ptr<AudioBus>> pending_outputs_
      GUARDED_BY_CONTEXT(sequence_checker_);
  std::unique_ptr<AudioBusPool> output_pool_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // All current input frames.
  base::circular_deque<std::unique_ptr<AudioBus>> inputs_
      GUARDED_BY_CONTEXT(sequence_checker_);

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace media

#endif  // MEDIA_BASE_CONVERTING_AUDIO_FIFO_H_
