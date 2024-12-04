// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "media/base/converting_audio_fifo.h"

#include <memory>

#include "media/base/audio_bus.h"
#include "media/base/audio_parameters.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

constexpr int kDefaultChannels = 2;
constexpr int kDefaultFrames = 480;
constexpr int kInputSampleRate = 48000;

struct TestAudioParams {
  const int channels;
  const int sample_rate;
};

const AudioParameters kDefaultParams =
    AudioParameters(AudioParameters::Format::AUDIO_PCM_LINEAR,
                    ChannelLayoutConfig::Guess(kDefaultChannels),
                    kInputSampleRate,
                    kDefaultFrames);

// The combination of these values should cover all combinationso of up/down
// sampling/buffering/mixing, relative to a steady input of |kDefaultParams|.
// This does result in 27 combinations.
constexpr int kTestChannels[] = {1, 2, 3};
constexpr int kTestSampleRates[] = {8000, 48000, 647744};
constexpr int kTestOutputFrames[] = {395, 480, 512};

class ConvertingAudioFifoTest
    : public ::testing::TestWithParam<std::tuple<int, int, int>> {
 public:
  ConvertingAudioFifoTest() = default;

  ConvertingAudioFifoTest(const ConvertingAudioFifoTest&) = delete;
  ConvertingAudioFifoTest& operator=(const ConvertingAudioFifoTest&) = delete;

  ~ConvertingAudioFifoTest() override = default;

  AudioParameters TestOutputParams() {
    return AudioParameters(AudioParameters::Format::AUDIO_PCM_LINEAR,
                           ChannelLayoutConfig::Guess(output_channels()),
                           output_sample_rate(), output_frames());
  }

  void CreateFifo(const AudioParameters& output_params) {
    DCHECK(!fifo_);
    fifo_ =
        std::make_unique<ConvertingAudioFifo>(kDefaultParams, output_params);
    output_params_ = output_params;
  }

  void PushFrames(int frames, int channels = kDefaultChannels) {
    DCHECK(frames);
    DCHECK(channels);
    auto bus = AudioBus::Create(channels, frames);
    bus->Zero();
    fifo_->Push(std::move(bus));
  }

  void PushFramesWithValue(AudioParameters params, float value) {
    auto audio_bus = AudioBus::Create(params);
    float* channel = audio_bus->channel(0);
    for (int i = 0; i < audio_bus->frames(); ++i) {
      channel[i] = value;
    }
  }

  int min_number_input_frames_needed() {
    return fifo_->min_input_frames_needed_;
  }

  int current_frames_in_fifo() { return fifo()->total_frames_; }

  ConvertingAudioFifo* fifo() {
    DCHECK(fifo_);
    return fifo_.get();
  }

  void DrainAndVerifyOutputs() {
    while (fifo_->HasOutput()) {
      ++number_outputs_;
      auto* output = fifo_->PeekOutput();
      EXPECT_EQ(output->frames(), output_params_.frames_per_buffer());
      EXPECT_EQ(output->channels(), output_params_.channels());
      fifo_->PopOutput();
    }
  }

  int number_outputs() { return number_outputs_; }

 private:
  int output_channels() { return std::get<0>(GetParam()); }
  int output_sample_rate() { return std::get<1>(GetParam()); }
  int output_frames() { return std::get<2>(GetParam()); }

  AudioParameters output_params_;

  int number_outputs_ = 0;
  std::unique_ptr<ConvertingAudioFifo> fifo_;
};

// Verify that construction works as intended.
TEST_F(ConvertingAudioFifoTest, Construct) {
  CreateFifo(kDefaultParams);
  EXPECT_EQ(0, current_frames_in_fifo());
  EXPECT_FALSE(fifo()->HasOutput());
}

// Verify that flushing an empty FIFO is a noop.
TEST_F(ConvertingAudioFifoTest, EmptyFlush) {
  CreateFifo(kDefaultParams);

  fifo()->Flush();
  EXPECT_EQ(0, current_frames_in_fifo());
  EXPECT_FALSE(fifo()->HasOutput());
}

// Verify that the fifo can be flushed .
TEST_F(ConvertingAudioFifoTest, PushFlush) {
  CreateFifo(kDefaultParams);

  // Push a single frame so the flush won't be a no-op.
  PushFrames(1);
  EXPECT_FALSE(fifo()->HasOutput());
  EXPECT_EQ(1, current_frames_in_fifo());

  fifo()->Flush();
  DrainAndVerifyOutputs();
  EXPECT_EQ(1, number_outputs());
  EXPECT_EQ(0, current_frames_in_fifo());
}

// Verify that the fifo can be flushed twice in a row.
TEST_F(ConvertingAudioFifoTest, PushFlushFlush) {
  CreateFifo(kDefaultParams);

  // Push a single frame so the flush won't be a no-op.
  PushFrames(1);
  DrainAndVerifyOutputs();
  EXPECT_EQ(0, number_outputs());

  fifo()->Flush();
  DrainAndVerifyOutputs();
  EXPECT_EQ(1, number_outputs());

  fifo()->Flush();
  DrainAndVerifyOutputs();
  EXPECT_EQ(1, number_outputs());
}

// Verify that the fifo can be flushed twice.
TEST_P(ConvertingAudioFifoTest, PushFlushTwice) {
  CreateFifo(TestOutputParams());

  // Push a single frame so the flush won't be a no-op.
  PushFrames(1);
  DrainAndVerifyOutputs();
  EXPECT_EQ(0, number_outputs());

  fifo()->Flush();
  DrainAndVerifyOutputs();
  EXPECT_EQ(1, number_outputs());

  PushFrames(1);
  fifo()->Flush();
  DrainAndVerifyOutputs();
  EXPECT_EQ(2, number_outputs());
}

// Verify that the fifo doesn't output until it has enough frames.
TEST_P(ConvertingAudioFifoTest, Push_NotEnoughFrames) {
  CreateFifo(TestOutputParams());

  int total_frames_pushed = 0;
  int frames_to_push = min_number_input_frames_needed() / 5;

  // Avoid pushing 0 frames.
  DCHECK(frames_to_push);

  // Push in multiple small batches, but not enough to force an output.
  while (frames_to_push + total_frames_pushed <
         min_number_input_frames_needed()) {
    PushFrames(frames_to_push);
    total_frames_pushed += frames_to_push;
    DrainAndVerifyOutputs();
    EXPECT_EQ(0, number_outputs());
    EXPECT_EQ(current_frames_in_fifo(), total_frames_pushed);
  }

  // One more push should be enough for a conversion.
  PushFrames(frames_to_push);
  DrainAndVerifyOutputs();
  EXPECT_EQ(1, number_outputs());

  EXPECT_LT(current_frames_in_fifo(), min_number_input_frames_needed());

  // Flush for added test coverage.
  fifo()->Flush();
}

// Verify that the fifo outputs immediately if it has enough frames.
TEST_P(ConvertingAudioFifoTest, Push_EnoughFrames) {
  CreateFifo(TestOutputParams());

  // Push enough frames to trigger an single output.
  PushFrames(min_number_input_frames_needed());
  DrainAndVerifyOutputs();
  EXPECT_EQ(1, number_outputs());
}

// Verify that the fifo produces more than one output if it has enough frames.
TEST_P(ConvertingAudioFifoTest, Push_MoreThanEnoughFrames) {
  CreateFifo(TestOutputParams());

  // Push enough frames to trigger multiple outputs.
  PushFrames(min_number_input_frames_needed() * 5);
  DrainAndVerifyOutputs();
  EXPECT_GE(number_outputs(), 5);

  // There should not be enough frames leftover to create a new output.
  EXPECT_LT(current_frames_in_fifo(), min_number_input_frames_needed());
}

// Verify we can partially drain the fifo before pushing more data.
TEST_P(ConvertingAudioFifoTest, Push_MoreThanEnoughFrames_PartialDrain) {
  CreateFifo(TestOutputParams());

  // Push enough frames to trigger multiple outputs.
  PushFrames(min_number_input_frames_needed() * 3);
  EXPECT_TRUE(fifo()->HasOutput());

  // Partially drain the fifo.
  fifo()->PopOutput();
  fifo()->PopOutput();

  EXPECT_TRUE(fifo()->HasOutput());

  // Push more frames.
  PushFrames(min_number_input_frames_needed() * 3);

  // Partially drain the fifo.
  fifo()->PopOutput();
  fifo()->PopOutput();
  EXPECT_TRUE(fifo()->HasOutput());

  // For good measure, flush any remaining output.
  fifo()->Flush();

  DrainAndVerifyOutputs();

  EXPECT_FALSE(fifo()->HasOutput());
}

// Verify that the FIFO returns outputs in FIFO order.
TEST_F(ConvertingAudioFifoTest, Push_MoreThanEnoughFrames_IsFifoOrder) {
  // Do not perform any conversion, as to preserve the values pushed in.
  CreateFifo(kDefaultParams);

  // Push data with increasing values.
  PushFramesWithValue(kDefaultParams, 0.0);
  PushFramesWithValue(kDefaultParams, 0.25);
  PushFramesWithValue(kDefaultParams, 0.5);
  PushFramesWithValue(kDefaultParams, 0.75);

  float last_value = -1.0;

  // Drain the FIFO, making sure output values are increasing.
  while (fifo()->HasOutput()) {
    // Get the first value of the output.
    auto* output = fifo()->PeekOutput();
    float current_value = output->channel(0)[0];
    EXPECT_GT(current_value, last_value);
    last_value = current_value;
  }
}

// Verify that the fifo can handle variable numbers of input frames.
TEST_P(ConvertingAudioFifoTest, Push_VaryingFrames) {
  CreateFifo(TestOutputParams());

  int base_frame_count = min_number_input_frames_needed() / 4;
  constexpr int kFrameVariations[] = {-3, 13, -10, 18};

  // Push a varying amount of frames into |fifo_|.
  for (const int& variation : kFrameVariations) {
    PushFrames(base_frame_count + variation);
  }

  // We should still get one output.
  DrainAndVerifyOutputs();
  EXPECT_EQ(1, number_outputs());
}

// Verify that the fifo can handle variable numbers of input channels.
TEST_P(ConvertingAudioFifoTest, Push_VaryingChannels) {
  CreateFifo(TestOutputParams());

  // Superpermutation of {1, 2, 3}, covering all transitions between upmixing,
  // downmixing and not mixing.
  const int kChannelCountSequence[] = {1, 2, 3, 1, 2, 2, 1, 3, 2, 1};

  // Push frames with variable channels into |fifo_|.
  for (const int& channels : kChannelCountSequence) {
    PushFrames(kDefaultFrames, channels);
  }

  // Flush to make sure we consume the frames.
  fifo()->Flush();
  DrainAndVerifyOutputs();
  EXPECT_GT(number_outputs(), 1);
}

INSTANTIATE_TEST_SUITE_P(
    ConvertingAudioFifoTest,
    ConvertingAudioFifoTest,
    testing::Combine(testing::ValuesIn(kTestChannels),
                     testing::ValuesIn(kTestSampleRates),
                     testing::ValuesIn(kTestOutputFrames)));

}  // namespace media
