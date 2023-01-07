// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/converting_audio_fifo.h"

#include <stdint.h>
#include <memory>

#include "base/logging.h"

#include "base/callback_helpers.h"
#include "base/location.h"
#include "base/test/bind.h"
#include "base/time/time.h"
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

  void CreateFifo(AudioParameters output_params) {
    DCHECK(!fifo_);
    fifo_ = std::make_unique<ConvertingAudioFifo>(
        kDefaultParams, output_params, VerifyOutputCallback(output_params));
  }

  void PushFrames(int frames, int channels = kDefaultChannels) {
    DCHECK(frames);
    DCHECK(channels);
    fifo_->Push(AudioBus::Create(channels, frames));
  }

  int min_number_input_frames_needed() {
    return fifo_->min_input_frames_needed_;
  }

  int current_frames_in_fifo() { return fifo()->total_frames_; }

  ConvertingAudioFifo* fifo() {
    DCHECK(fifo_);
    return fifo_.get();
  }

  int number_outputs() { return number_outputs_; }

 private:
  int output_channels() { return std::get<0>(GetParam()); }
  int output_sample_rate() { return std::get<1>(GetParam()); }
  int output_frames() { return std::get<2>(GetParam()); }

  ConvertingAudioFifo::OuputCallback VerifyOutputCallback(
      AudioParameters expected_params) {
    return base::BindLambdaForTesting(
        [this, expected_params](AudioBus* audio_bus) {
          EXPECT_EQ(audio_bus->frames(), expected_params.frames_per_buffer());
          EXPECT_EQ(audio_bus->channels(), expected_params.channels());
          ++number_outputs_;
        });
  }

  int number_outputs_ = 0;
  std::unique_ptr<ConvertingAudioFifo> fifo_;
};

// Verify that construction works as intended.
TEST_F(ConvertingAudioFifoTest, Construct) {
  CreateFifo(kDefaultParams);
  EXPECT_EQ(0, current_frames_in_fifo());
  EXPECT_EQ(0, number_outputs());
}

// Verify that flushing an empty FIFO is a noop.
TEST_F(ConvertingAudioFifoTest, EmptyFlush) {
  CreateFifo(kDefaultParams);

  fifo()->Flush();
  EXPECT_EQ(0, current_frames_in_fifo());
  EXPECT_EQ(0, number_outputs());
}

// Verify that the fifo can be flushed .
TEST_F(ConvertingAudioFifoTest, PushFlush) {
  CreateFifo(kDefaultParams);

  // Push a single frame so the flush won't be a no-op.
  PushFrames(1);
  EXPECT_EQ(0, number_outputs());
  EXPECT_EQ(1, current_frames_in_fifo());

  fifo()->Flush();
  EXPECT_EQ(1, number_outputs());
  EXPECT_EQ(0, current_frames_in_fifo());
}

// Verify that the fifo can be flushed twice in a row.
TEST_F(ConvertingAudioFifoTest, PushFlushFlush) {
  CreateFifo(kDefaultParams);

  // Push a single frame so the flush won't be a no-op.
  PushFrames(1);
  EXPECT_EQ(0, number_outputs());

  fifo()->Flush();
  EXPECT_EQ(1, number_outputs());

  fifo()->Flush();
  EXPECT_EQ(1, number_outputs());
}

// Verify that the fifo can be flushed twice.
TEST_P(ConvertingAudioFifoTest, PushFlushTwice) {
  CreateFifo(TestOutputParams());

  // Push a single frame so the flush won't be a no-op.
  PushFrames(1);
  EXPECT_EQ(0, number_outputs());

  fifo()->Flush();
  EXPECT_EQ(1, number_outputs());

  PushFrames(1);
  fifo()->Flush();
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
    EXPECT_EQ(0, number_outputs());
    EXPECT_EQ(current_frames_in_fifo(), total_frames_pushed);
  }

  // One more push should be enough for a conversion.
  PushFrames(frames_to_push);
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
  EXPECT_EQ(1, number_outputs());
}

// Verify that the fifo produces more than one output if it has enough frames.
TEST_P(ConvertingAudioFifoTest, Push_MoreThanEnoughFrames) {
  CreateFifo(TestOutputParams());

  // Push enough frames to trigger multiple outputs.
  PushFrames(min_number_input_frames_needed() * 5);
  EXPECT_GE(number_outputs(), 5);

  // There should not be enough frames leftover to create a new output.
  EXPECT_LT(current_frames_in_fifo(), min_number_input_frames_needed());
}

// Verify that the fifo can handle variable numbers of input frames.
TEST_P(ConvertingAudioFifoTest, Push_VaryingFrames) {
  CreateFifo(TestOutputParams());

  int base_frame_count = min_number_input_frames_needed() / 4;
  constexpr int kFrameVariations[] = {-3, 13, -10, 18};

  // Push a varying amount of frames into |fifo_|.
  for (const int& variation : kFrameVariations)
    PushFrames(base_frame_count + variation);

  // We should still get one output.
  EXPECT_EQ(1, number_outputs());
}

// Verify that the fifo can handle variable numbers of input channels.
TEST_P(ConvertingAudioFifoTest, Push_VaryingChannels) {
  CreateFifo(TestOutputParams());

  // Superpermutation of {1, 2, 3}, covering all transitions between upmixing,
  // downmixing and not mixing.
  const int kChannelCountSequence[] = {1, 2, 3, 1, 2, 2, 1, 3, 2, 1};

  // Push frames with variable channels into |fifo_|.
  for (const int& channels : kChannelCountSequence)
    PushFrames(kDefaultFrames, channels);

  // Flush to make sure we consume the frames.
  fifo()->Flush();
  EXPECT_GT(number_outputs(), 1);
}

INSTANTIATE_TEST_SUITE_P(
    ConvertingAudioFifoTest,
    ConvertingAudioFifoTest,
    testing::Combine(testing::ValuesIn(kTestChannels),
                     testing::ValuesIn(kTestSampleRates),
                     testing::ValuesIn(kTestOutputFrames)));

}  // namespace media
