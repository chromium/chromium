// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/audio_limiter.h"

#include "base/containers/span_reader.h"
#include "base/functional/bind.h"
#include "base/ranges/algorithm.h"
#include "base/time/time.h"
#include "media/base/audio_timestamp_helper.h"

namespace media {

// Chosen to be responsive, while not introducing too much distortion. Faster
// values could "crush" transients, slower values might result in values
// being clipped further down the pipeline.
constexpr base::TimeDelta kAttackTime = base::Milliseconds(5);
constexpr base::TimeDelta kReleaseTime = base::Milliseconds(50);

// Do not leave any headroom, only start limiting above 0dbfs.
constexpr double kThreshold = 1.0;

AudioLimiter::PendingOutput::PendingOutput(OutputChannels channels,
                                           OutputFilledCB filled_callback)
    : on_filled_callback(std::move(filled_callback)),
      channels(std::move(channels)) {}

AudioLimiter::PendingOutput::~PendingOutput() = default;
AudioLimiter::PendingOutput::PendingOutput(PendingOutput&&) = default;

AudioLimiter::AudioLimiter(int sample_rate, int channels)
    : channels_(channels),
      attack_frames_(
          AudioTimestampHelper::TimeToFrames(kAttackTime, sample_rate)),
      release_frames_(
          AudioTimestampHelper::TimeToFrames(kReleaseTime, sample_rate)),
      // Avoid dividing by 0, if `sample_rate` is 0, and CHECK below instead.
      attack_constant_(attack_frames_ ? std::pow(0.1, 1.0 / attack_frames_)
                                      : 0.0),
      release_constant_(release_frames_ ? std::pow(0.1, 1.0 / release_frames_)
                                        : 0.0),
      moving_max_(attack_frames_),
      initial_output_delay_in_frames_(attack_frames_) {
  CHECK(sample_rate);
  CHECK(channels_);
}

AudioLimiter::~AudioLimiter() = default;

void AudioLimiter::LimitPeaks(const AudioBus& input,
                              const OutputChannels& output,
                              OutputFilledCB on_output_filled_cb) {
  LimitPeaksPartial(input, input.frames(), output,
                    std::move(on_output_filled_cb));
}

void AudioLimiter::LimitPeaksPartial(const AudioBus& input,
                                     int num_frames,
                                     const OutputChannels& output,
                                     OutputFilledCB on_output_filled_cb) {
  CHECK(!was_flushed_);
  CHECK_GT(num_frames, 0);
  CHECK_LE(num_frames, input.frames());
  CHECK_EQ(input.channels(), channels_);
  CHECK_EQ(static_cast<size_t>(input.channels()), output.size());
  for (int ch = 0; ch < channels_; ++ch) {
    CHECK_EQ(num_frames * sizeof(float), output[ch].size_bytes());
  }

  outputs_.emplace_back(std::move(output), std::move(on_output_filled_cb));

  FeedInput(input, num_frames);
}

void AudioLimiter::Flush() {
  CHECK(!was_flushed_);

  // Feed in silence to make sure ever pending input is written to output.
  auto silence = AudioBus::Create(channels_, attack_frames_);
  silence->Zero();

  FeedInput(*silence, attack_frames_);

  // All outputs should have been filled.
  CHECK(outputs_.empty());

  was_flushed_ = true;
}

void AudioLimiter::FeedInput(const AudioBus& input, int num_frames) {
  CHECK_EQ(input.channels(), channels_);

  const uint32_t frame_size = channels_;

  std::vector<float> interleaved_input;
  interleaved_input.resize(num_frames * frame_size);

  input.ToInterleaved<Float32SampleTypeTraitsNoClip>(num_frames,
                                                     interleaved_input.data());

  // Sanitize the input, removing unusual values. This is a destructive
  // operation which changes the nature of the audio signal, but it avoids
  // undefined behavior.
  base::ranges::for_each(interleaved_input, [](float& sample) {
    if (std::isnan(sample) || std::isinf(sample)) {
      sample = 0.0f;
    }
  });

  delayed_interleaved_input_.reserve(delayed_interleaved_input_.size() +
                                     interleaved_input.size());

  base::ranges::copy(interleaved_input,
                     std::back_inserter(delayed_interleaved_input_));

  base::SpanReader<float> input_reader(interleaved_input);

  while (input_reader.remaining()) {
    auto frame_data = input_reader.Read(frame_size);

    // Use a floor of `kThreshold`. This will limit churn in `moving_max_`, and
    // simplify calculations when we don't need to adjust gain for small
    // samples.
    float max_sample_for_frame = kThreshold;
    for (float sample : *frame_data) {
      max_sample_for_frame = std::max(std::abs(sample), max_sample_for_frame);
    }

    moving_max_.AddSample(max_sample_for_frame);

    UpdateGain(moving_max_.Max());

    WriteLimitedFrameToOutput();
  }

  // We should never have more than `attack_frames_` left. Extra frames should
  // have been written to output.
  CHECK_LE(delayed_interleaved_input_.size(),
           static_cast<size_t>(channels_ * attack_frames_));
}

void AudioLimiter::UpdateGain(float current_maximum) {
  double gain = 1.0;
  if (current_maximum > kThreshold) {
    gain = kThreshold / current_maximum;
  }

  if (gain < smoothed_gain_) {
    // We are in the "attack" portion.
    // Our smoothing function and `attack_constant_` are chosen such that, after
    // `attack_frames_`'s worth of data has been processed, `smoothed_gain_` is
    // 90% closer to `target_gain_`. E.g.:
    //
    //     final_gain = initial_gain - 90% * (initial_gain - target_gain)
    //
    // If we used `gain` directly as a `target_gain`, we would always undershoot
    // gain reduction: by the time a maximum needs to be written to output
    // (after accounting for the `kAttackTime` delay), `smoothed_gain_` is still
    // 10% off, and we'd have to clip the maximum.
    // By setting our desired `final_gain` and `initial_gain` in the the formula
    // above, we can solve for a `target_gain`, that will make `smoothed_gain_`
    // be exactly equal to `gain` when a maximum needs to be written to output
    // (after `attack_frames_` iterations of our gain smoothing function).
    //
    //     gain = smoothed_gain_ - 90% * (smoothed_gain_ - target_gain)
    //     gain = smoothed_gain_ - 9/10 * smoothed_gain_ + 9/10 * target_gain
    //     gain = 1/10 * smoothed_gain_ + 9/10 * target_gain
    //     gain  - 1/10 * smoothed_gain_ = 9/10 * target_gain
    //     10/9 * (gain  - 1/10 * smoothed_gain_) = target_gain
    //
    constexpr double kOneTenth = 1.0 / 10.0;
    constexpr double kTenNinths = 10.0 / 9.0;
    const double target_gain = kTenNinths * (gain - kOneTenth * smoothed_gain_);
    target_gain_ = std::min(target_gain_, target_gain);
  } else {
    target_gain_ = gain;
  }

  // Gain smoothing.
  if (target_gain_ < smoothed_gain_) {
    // Attack portion.
    smoothed_gain_ =
        attack_constant_ * (smoothed_gain_ - target_gain_) + target_gain_;

    // Don't overshoot gain reduction.
    smoothed_gain_ = std::max(smoothed_gain_, gain);
  } else {
    // Release portion.
    smoothed_gain_ =
        release_constant_ * (smoothed_gain_ - target_gain_) + target_gain_;
  }

  // A gain above 1.0 would be an expander, not a limiter.
  CHECK_LE(smoothed_gain_, 1.0);
}

void AudioLimiter::WriteLimitedFrameToOutput() {
  if (initial_output_delay_in_frames_ > 0) {
    // Delay writing the first frames to output. This accumulates
    // `attack_frames_` frames into `delayed_interleaved_input_`, and acts as
    // the look-ahead for this limiter.
    --initial_output_delay_in_frames_;
    return;
  }

  CHECK(!outputs_.empty());

  // Our input queue should contain more than `attack_frames_` frames. We will
  // write one extra frame to output below.
  CHECK_GT(delayed_interleaved_input_.size(),
           static_cast<size_t>(channels_ * attack_frames_));

  OutputChannels& output_channels = outputs_.front().channels;
  CHECK(!output_channels.empty());
  CHECK(!output_channels[0].empty());

  if (smoothed_gain_ < 1.0) {
    // Apply gain reduction.
    for (int ch = 0; ch < channels_; ++ch) {
      auto [dest, remainder] = output_channels[ch].split_at<4>();

      const float adjusted_sample = static_cast<float>(
          static_cast<double>(delayed_interleaved_input_.front()) *
          smoothed_gain_);
      delayed_interleaved_input_.pop_front();

      dest.copy_from(base::byte_span_from_ref(adjusted_sample));

      output_channels[ch] = remainder;
    }
  } else {
    // Passthrough.
    for (int ch = 0; ch < channels_; ++ch) {
      auto [dest, remainder] = output_channels[ch].split_at<4>();

      dest.copy_from(
          base::byte_span_from_ref(delayed_interleaved_input_.front()));

      delayed_interleaved_input_.pop_front();

      output_channels[ch] = remainder;
    }
  }

  // Notify the owner that we've filled this output completely.
  if (outputs_.front().channels[0].empty()) {
    std::move(outputs_.front().on_filled_callback).Run();
    outputs_.pop_front();
  }
}

}  // namespace media
