// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/audio_limiter.h"

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "media/audio/simple_sources.h"
#include "media/base/audio_bus.h"
#include "media/base/audio_timestamp_helper.h"
#include "testing/gtest/include/gtest/gtest.h"

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/41494069): Update these tests once AudioBus is spanified..
#pragma allow_unsafe_buffers
#endif

namespace media {

constexpr int kSampleRate = 48000;
constexpr int kChannels = 2;
constexpr int kBufferSize = 960;  // 20ms at 48khz
constexpr int kFrequency = 20;

namespace {
bool AudioBusAreEqual(AudioBus* a, AudioBus* b) {
  if (a->frames() != b->frames() || a->channels() != b->channels()) {
    return false;
  }

  for (int ch = 0; ch < kChannels; ++ch) {
    if (base::span(a->channel(ch), static_cast<size_t>(a->frames())) !=
        base::span(b->channel(ch), static_cast<size_t>(b->frames()))) {
      return false;
    }
  }

  return true;
}

void SetFirstNFrames(AudioBus* bus, const int number_of_frames, float value) {
  for (int ch = 0; ch < kChannels; ++ch) {
    float* channel_data = bus->channel(ch);
    for (int i = 0; i < number_of_frames; ++i) {
      channel_data[i] = value;
    }
  }
}
}  // namespace

class LimiterTest : public testing::Test {
 public:
  using AudioBusVector = std::vector<std::unique_ptr<AudioBus>>;

  LimiterTest()
      : limiter_(std::make_unique<AudioLimiter>(kSampleRate, kChannels)),
        audio_source_(kChannels, kFrequency, kSampleRate) {
    source_bus_ = AudioBus::Create(kChannels, kBufferSize);
    destination_bus_ = AudioBus::Create(kChannels, kBufferSize);
  }

  LimiterTest(const LimiterTest&) = delete;
  LimiterTest& operator=(const LimiterTest&) = delete;

  ~LimiterTest() override = default;

  void FillWithSine(AudioBus* bus, float scale = 1.0f) {
    audio_source_.OnMoreData(base::TimeDelta(), current_timestamp_, {}, bus);
    current_timestamp_ +=
        AudioTimestampHelper::FramesToTime(kBufferSize, kSampleRate);

    if (scale != 1.0f) {
      for (int ch = 0; ch < kChannels; ++ch) {
        float* channel_data = bus->channel(ch);
        for (int i = 0; i < bus->frames(); ++i) {
          channel_data[i] *= scale;
        }
      }
    }

    ASSERT_EQ(bus->channels(), kChannels);
    // Flip the values in this channel, and slightly change them. This makes
    // sure we catch errors from outputting to the wrong channels.
    float* channel_data = bus->channel(1);
    for (int i = 0; i < bus->frames(); ++i) {
      channel_data[i] *= -0.99f;
    }
  }

 protected:
  AudioLimiter::OutputChannels AudioBusAsOutputs(AudioBus* audio_bus) {
    AudioLimiter::OutputChannels channels;
    for (int ch = 0; ch < audio_bus->channels(); ++ch) {
      channels.emplace_back(reinterpret_cast<uint8_t*>(audio_bus->channel(ch)),
                            audio_bus->frames() * sizeof(float));
    }

    return channels;
  }

  base::TimeTicks current_timestamp_;
  std::unique_ptr<AudioLimiter> limiter_;
  SineWaveAudioSource audio_source_;
  std::unique_ptr<AudioBus> source_bus_;
  std::unique_ptr<AudioBus> destination_bus_;
};

// Makes sure we can flush a limiter that has never had any input.
TEST_F(LimiterTest, EmptyFlush) {
  limiter_->Flush();
}

// Makes sure inputs and outputs are bit-wise identical when the limiter isn't
// adjusting gain.
TEST_F(LimiterTest, NoLimiting_IsPassthrough) {
  FillWithSine(source_bus_.get());

  bool callback_signaled = false;

  limiter_->LimitPeaks(
      *source_bus_, AudioBusAsOutputs(destination_bus_.get()),
      base::BindLambdaForTesting([&]() { callback_signaled = true; }));

  // The limiter has a delay. The output should not be filled at this point.
  EXPECT_FALSE(callback_signaled);

  limiter_->Flush();

  EXPECT_TRUE(callback_signaled);

  EXPECT_TRUE(AudioBusAreEqual(source_bus_.get(), destination_bus_.get()));
}

// Makes sure inputs and outputs are bit-wise identical when the limiter isn't
// adjusting gain.
TEST_F(LimiterTest, NoLimiting_PartialBuffer_IsPassthrough) {
  FillWithSine(source_bus_.get());

  constexpr int kPartialSize = kBufferSize - 256;
  auto dest_bus = AudioBus::Create(kChannels, kPartialSize);

  bool callback_signaled = false;

  limiter_->LimitPeaksPartial(
      *source_bus_, kPartialSize, AudioBusAsOutputs(dest_bus.get()),
      base::BindLambdaForTesting([&]() { callback_signaled = true; }));

  // The limiter has a delay. The output should not be filled at this point.
  EXPECT_FALSE(callback_signaled);

  limiter_->Flush();

  EXPECT_TRUE(callback_signaled);

  // Create a trimmed copy input for ease of comparison.
  auto resized_input = AudioBus::Create(kChannels, kPartialSize);
  source_bus_->CopyPartialFramesTo(0, kPartialSize, 0, resized_input.get());

  EXPECT_TRUE(AudioBusAreEqual(resized_input.get(), dest_bus.get()));
}

// Makes sure the limiter adjust the signal appropriately.
TEST_F(LimiterTest, WithLimiting_CompressesSignal) {
  const int longer_frame_size =
      AudioTimestampHelper::TimeToFrames(base::Milliseconds(500), kSampleRate);

  source_bus_ = AudioBus::Create(kChannels, longer_frame_size);
  destination_bus_ = AudioBus::Create(kChannels, longer_frame_size);

  std::vector<float> amplitudes({1.001, 1.1, 1.5, 2.0, 5.0, 100.0, 1000.0});

  for (float amplitude : amplitudes) {
    SCOPED_TRACE(base::StringPrintf("Amplitude: %f", amplitude));
    FillWithSine(source_bus_.get(), amplitude);

    bool callback_signaled = false;
    limiter_ = std::make_unique<AudioLimiter>(kSampleRate, kChannels);
    limiter_->LimitPeaks(
        *source_bus_, AudioBusAsOutputs(destination_bus_.get()),
        base::BindLambdaForTesting([&]() { callback_signaled = true; }));

    // The limiter has a delay. The output should not be filled at this point.
    EXPECT_FALSE(callback_signaled);

    limiter_->Flush();

    EXPECT_TRUE(callback_signaled);

    int out_of_bounds_before = 0;
    int out_of_bounds_after = 0;
    const float kAbsoluteBound = 1.0f;
    for (int ch = 0; ch < kChannels; ++ch) {
      float* src_data = source_bus_->channel(ch);
      float* dest_data = destination_bus_->channel(ch);
      for (int i = 0; i < source_bus_->frames(); ++i) {
        if (std::abs(src_data[i]) > kAbsoluteBound) {
          ++out_of_bounds_before;
        }
        if (std::abs(dest_data[i]) > kAbsoluteBound) {
          ++out_of_bounds_after;
        }
      }
    }

    // Ensure we have out of bounds data for testing.
    ASSERT_GT(out_of_bounds_before, 0);
    EXPECT_EQ(out_of_bounds_after, 0);
  }
}

// Makes sure the limiter writes to outputs in FIFO order.
TEST_F(LimiterTest, MultipleCalls) {
  constexpr int kIterations = 5;

  int total_output_calls = 0;

  AudioBusVector inputs;
  AudioBusVector outputs;

  for (int i = 0; i < kIterations; ++i) {
    // Create and fill a new input bus.
    auto bus = AudioBus::Create(kChannels, kBufferSize);
    FillWithSine(bus.get());
    inputs.push_back(std::move(bus));

    // Create an empty output destination.
    outputs.push_back(AudioBus::Create(kChannels, kBufferSize));

    // Each time an output is filled, make sure it matches the input.
    auto verify_outputs = base::BindOnce(
        [](AudioBusVector* inputs, AudioBusVector* outputs,
           int* total_output_calls, int iteration) {
          EXPECT_TRUE(AudioBusAreEqual(inputs->at(iteration).get(),
                                       outputs->at(iteration).get()));
          ++(*total_output_calls);
        },
        &inputs, &outputs, &total_output_calls, i);

    limiter_->LimitPeaks(*inputs.back(),
                         AudioBusAsOutputs(outputs.back().get()),
                         std::move(verify_outputs));
  }

  // Ensure the inputs are different, otherwise this test wouldn't catch any
  // issues. This check could fail if the size of our busses was synced up with
  // the frequency of our sine generator.
  ASSERT_FALSE(AudioBusAreEqual(inputs[0].get(), inputs[1].get()));

  limiter_->Flush();

  EXPECT_EQ(total_output_calls, kIterations);
}

// Makes sure the limiter writes to outputs in FIFO order, and handles partial
// inputs.
TEST_F(LimiterTest, MultipleCalls_PartialBuffer) {
  constexpr int kIterations = 5;

  // Choose an arbitrary data size which isn't a multiple of 2.
  constexpr int kPartialSize = kBufferSize / 2 + 3;

  int total_output_calls = 0;

  AudioBusVector inputs;
  AudioBusVector outputs;

  for (int i = 0; i < kIterations; ++i) {
    // Create and fill a new input bus.
    auto bus = AudioBus::Create(kChannels, kBufferSize);
    FillWithSine(bus.get());
    inputs.push_back(std::move(bus));

    // Create an empty output destination, with a smaller size than `bus`.
    outputs.push_back(AudioBus::Create(kChannels, kPartialSize));

    // Each time an output is filled, make sure it matches the input.
    auto verify_outputs = base::BindOnce(
        [](AudioBusVector* inputs, AudioBusVector* outputs,
           int* total_output_calls, int iteration) {
          // Copy the input to a smaller bus, for ease of comparison.
          auto resized_input = AudioBus::Create(kChannels, kPartialSize);
          inputs->at(iteration)->CopyPartialFramesTo(0, kPartialSize, 0,
                                                     resized_input.get());
          EXPECT_TRUE(AudioBusAreEqual(resized_input.get(),
                                       outputs->at(iteration).get()));
          ++(*total_output_calls);
        },
        &inputs, &outputs, &total_output_calls, i);

    limiter_->LimitPeaksPartial(*inputs.back(), kPartialSize,
                                AudioBusAsOutputs(outputs.back().get()),
                                std::move(verify_outputs));
  }

  limiter_->Flush();

  EXPECT_EQ(total_output_calls, kIterations);
}

// Makes sure the limiter eventual returns to bit-wise identical passthrough of
// audio data, after limiting outputs.
TEST_F(LimiterTest, WithLimitingThenNoLimiting_ReturnsToPassthrough) {
  const int kMaxIterations =
      base::Milliseconds(600) /
      AudioTimestampHelper::FramesToTime(kBufferSize, kSampleRate);

  bool returned_to_passthrough = false;

  AudioBusVector inputs;
  AudioBusVector outputs;

  for (int i = 0; i < kMaxIterations; ++i) {
    auto bus = AudioBus::Create(kChannels, kBufferSize);

    // Create an input bus which needs to be limited as the first bus, and quiet
    // audio thereafter.
    if (i == 0) {
      FillWithSine(bus.get(), 1000.0);
    } else {
      FillWithSine(bus.get());
    }

    inputs.push_back(std::move(bus));

    // Create an empty output destination.
    outputs.push_back(AudioBus::Create(kChannels, kBufferSize));

    // Each time an output is filled, make sure it matches the input.
    auto verify_outputs = base::BindOnce(
        [](AudioBusVector* inputs, AudioBusVector* outputs,
           bool* returned_to_passthrough, int iteration) {
          if (iteration == 0) {
            EXPECT_FALSE(AudioBusAreEqual(inputs->at(iteration).get(),
                                          outputs->at(iteration).get()));
          } else {
            bool was_passthrough = AudioBusAreEqual(
                inputs->at(iteration).get(), outputs->at(iteration).get());

            if (*returned_to_passthrough) {
              // Once a previous iteration was passthrough, all other iterations
              // should be passthrough.
              EXPECT_TRUE(was_passthrough);
            }

            *returned_to_passthrough = was_passthrough;
          }
        },
        &inputs, &outputs, &returned_to_passthrough, i);

    limiter_->LimitPeaks(*inputs.back(),
                         AudioBusAsOutputs(outputs.back().get()),
                         std::move(verify_outputs));
  }

  EXPECT_TRUE(returned_to_passthrough);
}

// Makes sure the output callback is called immediately after an output is
// filled, before we start filling the next output.
TEST_F(LimiterTest, OutputsFilledSequentially) {
  FillWithSine(source_bus_.get());

  // Create a destination buffer, with a special value at the first buffer of
  // the first sample. We will verify when this value gets overwritten.
  constexpr float kGuardSampleValue = 12345.0f;
  auto other_destination = AudioBus::Create(kChannels, kBufferSize);
  other_destination->channel(0)[0] = kGuardSampleValue;

  bool callback_signaled = false;

  limiter_->LimitPeaks(*source_bus_, AudioBusAsOutputs(destination_bus_.get()),
                       base::BindLambdaForTesting([&]() {
                         // At the time when `destination_bus_` is filled and
                         // this callback is run, `other_destination` should not
                         // have been written to at all.
                         EXPECT_EQ(kGuardSampleValue,
                                   other_destination->channel(0)[0]);
                         callback_signaled = true;
                       }));

  // The limiter has a delay. The output should not be filled at this point.
  EXPECT_FALSE(callback_signaled);

  limiter_->LimitPeaks(*source_bus_, AudioBusAsOutputs(other_destination.get()),
                       base::DoNothing());

  // `other_destination` should be partially written to after LimitPeaks()
  // returns.
  EXPECT_TRUE(callback_signaled);
  EXPECT_NE(kGuardSampleValue, other_destination->channel(0)[0]);
}

// Makes sure the limiter handles buffers of various sizes, including buffers
// that are smaller than its attack time. Also makes sure that one input
// pushed in can trigger multiple outputs to be filled at once.
TEST_F(LimiterTest, VariableSizes) {
  const int kTinyBufferSize =
      AudioTimestampHelper::TimeToFrames(base::Milliseconds(1), kSampleRate);

  unsigned int total_output_calls = 0;
  AudioBusVector inputs;
  AudioBusVector outputs;

  // Push in 4ms' worth of data, when the limiter's internal delay is 5ms (see
  // the limiter's implementation).
  inputs.push_back(AudioBus::Create(kChannels, kTinyBufferSize));
  inputs.push_back(AudioBus::Create(kChannels, 2 * kTinyBufferSize));
  inputs.push_back(AudioBus::Create(kChannels, kTinyBufferSize));

  for (auto& input : inputs) {
    FillWithSine(input.get());

    // Create an empty output destination.
    outputs.push_back(AudioBus::Create(kChannels, input->frames()));

    limiter_->LimitPeaks(
        *input, AudioBusAsOutputs(outputs.back().get()),
        base::BindLambdaForTesting([&]() { ++total_output_calls; }));
  }

  // We shouldn't have pushed enough data into the limiter for the first inputs
  // to be written out.
  EXPECT_EQ(0u, total_output_calls);

  limiter_->Flush();

  EXPECT_EQ(inputs.size(), total_output_calls);

  for (size_t i = 0; i < inputs.size(); ++i) {
    EXPECT_TRUE(AudioBusAreEqual(inputs[i].get(), outputs[i].get()));
  }
}

// Makes sure inputs and outputs are bit-wise identical when the limiter isn't
// adjusting gain.
TEST_F(LimiterTest, EdgeCaseNumbers) {
  const std::vector<float> special_numbers = {
      std::numeric_limits<float>::quiet_NaN(),
      std::numeric_limits<float>::signaling_NaN(),
      std::numeric_limits<float>::infinity(),
      -std::numeric_limits<float>::infinity()};

  constexpr int kNumberFrames = 5;

  for (float special_number : special_numbers) {
    SCOPED_TRACE(special_number);
    FillWithSine(source_bus_.get());
    // Set an edge-case number in the first few frames.
    SetFirstNFrames(source_bus_.get(), kNumberFrames, special_number);

    limiter_ = std::make_unique<AudioLimiter>(kSampleRate, kChannels);

    limiter_->LimitPeaks(*source_bus_,
                         AudioBusAsOutputs(destination_bus_.get()),
                         base::DoNothing());
    limiter_->Flush();

    // We expect the edge-case numbers to be treated as zeros. Update the input
    // for ease of comparison.
    SetFirstNFrames(source_bus_.get(), kNumberFrames, 0.0f);

    EXPECT_TRUE(AudioBusAreEqual(source_bus_.get(), destination_bus_.get()));
  }
}

}  // namespace media
