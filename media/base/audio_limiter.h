// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_AUDIO_LIMITER_H_
#define MEDIA_BASE_AUDIO_LIMITER_H_

#include "base/containers/circular_deque.h"
#include "base/containers/span.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr_exclusion.h"
#include "base/moving_window.h"
#include "media/base/audio_bus.h"
#include "media/base/media_export.h"

namespace media {

// Simple limiter which reduces gain from an input in order to prevent the
// output from exceeding the [-1.0, 1.0] range. This limiter uses a fixed attack
// and release, a hard knee, and a "1 : infinity" gain reduction ratio. It
// incurrs a delay of 5ms (based on its attack time in the .cc file), which
// is used to "look ahead" and to smooth gain changes.
//
// Flush() must be called for the last inputs to be written out. The limiter
// will not be usable after that point.
//
// Note: The gain reduction is "linked" across channels. One very loud channel
//       will result in all channels being compressed equally.
//
// Note: All outputs are clamped to [-1.0, 1.0], post gain reduction. From
//       experimenting with UTs, only peaks exceed this range (due to floating
//       point arithmetics) by an order of 10^-6. Clamping these values should
//       not introduce audible artifacts.
class MEDIA_EXPORT AudioLimiter {
 public:
  using OutputChannels = std::vector<base::span<uint8_t>>;
  using OutputFilledCB = base::OnceClosure;

  AudioLimiter(int sample_rate, int channels);
  ~AudioLimiter();

  AudioLimiter(const AudioLimiter&) = delete;
  AudioLimiter& operator=(const AudioLimiter&) = delete;

  // Fills `output_channels` with the gain adjusted values from
  // `input_channels`, reducing gain when necessary so output samples fit into
  // the [-1.0, 1.0] range. `on_output_filled_cb` will be synchronously be run
  // during a future call to LimitPeaks() or Flush(), once an additional
  // `attack_frames_` have been processed, and `output_channels` has been fully
  // written to.
  //
  // `input_channels` and `output_channels` must contain the same number of
  // channels, and have the same size in bytes.
  //
  // Note: Due to floating point precision rounding errors, the gain adjusted
  //       peak values sometime exceed the [-1.0, 1.0] range by less than 1E-6.
  //       We clamp all output to this range, which should not be audible at
  //       all, considering that this clamping reduces gain by one millionth of
  //       a decibel.
  //
  // Note: Cannot be called once Flush() has been called.
  void LimitPeaks(const AudioBus& input,
                  const OutputChannels& output_channels,
                  OutputFilledCB on_output_filled_cb);

  // Same as `LimitPeaks()`, but only pushes in the first `num_frames` frames
  // from `input`. Each channel in `output_channels` must have the exact size
  // to contain `num_frames`.
  void LimitPeaksPartial(const AudioBus& input,
                         int num_frames,
                         const OutputChannels& output_channels,
                         OutputFilledCB on_output_filled_cb);

  // Feeds in silence to forces the remaining input data to be written out.
  // Can only be called once, after which the limiter cannot be re-used.
  void Flush();

 private:
  // Represents unowned chunks of external output memory which should gradually
  // be filled, along with a callback notifying owners when the memory has fully
  // been written to.
  struct PendingOutput {
    PendingOutput(OutputChannels channels, OutputFilledCB filled_callback);
    ~PendingOutput();

    PendingOutput(PendingOutput&&);

    // TODO(367764863) Rewrite to base::raw_span.
    RAW_PTR_EXCLUSION OutputFilledCB on_filled_callback;
    // TODO(367764863) Rewrite to base::raw_span.
    RAW_PTR_EXCLUSION OutputChannels channels;
  };

  void FeedInput(const AudioBus& input, int num_frames);

  // Updates `target_gain_` and `smoothed_gain_`.
  void UpdateGain(float current_maximum);

  // Write the first frame of `delayed_interleaved_input_` to the front of
  // `pending_outputs_`, after adjusting it by `smoothed_gain`.
  void WriteLimitedFrameToOutput();

  const int channels_;

  // Number of frames over which the limiter ramps up or ramps down its gain
  // reduction, when inputs exceed the specified range.
  // Calculated from the constructor's `sample_rate`, such that these correspond
  // to 5ms' worth of frames for the attack, and 50ms for the release.
  const int attack_frames_;
  const int release_frames_;

  // Constants used as coefficients in the smoothing of `smoothed_gain_`. These
  // are calculated such that, for a starting `smoothed_gain_` and
  // `target_gain_`, `smoothed_gain_` is 90% of the way towards `target_gain_`
  // after processing enough frames (either `attack_frames_` or `relase_frames_`
  // depending on whether we are attacking or releasing).
  const double attack_constant_;
  const double release_constant_;

  // Rolling window containing the absolute maximum of previous frames.
  // The window is of size `attack_frames_`.
  base::MovingMax<float> moving_max_;

  // Input frames waiting to be written to output. Samples are interleaved, such
  // that the first N samples at the front of the queue correspond to channels 0
  // through (n-1) of the oldest frame.
  // This should eventually contain `attack_frames_` frames, afterwhich each new
  // frame added will cause the oldest frame to be written out.
  base::circular_deque<float> delayed_interleaved_input_;

  // Number of iterations to run before starting to write to output.
  int initial_output_delay_in_frames_;

  // Queue of outputs to be written out to. After each `PendingOutput` is
  // filled, its corresponding `on_filled_callback` is run, letting owners of
  // the output memory know that it's ready for use.
  base::circular_deque<PendingOutput> outputs_;

  // The gain towards which `smoothed_gain_` should converge to.
  double target_gain_ = 1.0;

  // The gradually changing gain which is actually applied to the input as it is
  // written to output.
  double smoothed_gain_ = 1.0;

  bool was_flushed_ = false;
};

}  // namespace media

#endif  // MEDIA_BASE_AUDIO_LIMITER_H_
