// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/audio_pull_fifo.h"

#include <memory>

#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/strings/stringprintf.h"
#include "media/base/audio_bus.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

// Block diagram of a possible real-world usage:
//
//       | Producer | ----> | AudioPullFifo | ----> | Consumer |
//                    push                    pull
//          2048      ---->       (2048)      ---->     ~512

// Number of channels in each audio bus.
static int kChannels = 2;

// Max number of audio framed the FIFO can contain.
static const int kMaxFramesInFifo = 2048;

class AudioPullFifoTest
    : public testing::TestWithParam<int> {
 public:
  AudioPullFifoTest()
      : pull_fifo_(kChannels,
                   kMaxFramesInFifo,
                   base::BindRepeating(&AudioPullFifoTest::ProvideInput,
                                       base::Unretained(this))),
        audio_bus_(AudioBus::Create(kChannels, kMaxFramesInFifo)) {
    EXPECT_EQ(kMaxFramesInFifo, pull_fifo_.SizeInFrames());
  }

  AudioPullFifoTest(const AudioPullFifoTest&) = delete;
  AudioPullFifoTest& operator=(const AudioPullFifoTest&) = delete;

  virtual ~AudioPullFifoTest() = default;

  void VerifyValue(base::span<float> data, float start_value) {
    float value = start_value;
    for (size_t i = 0; i < data.size(); ++i) {
      ASSERT_FLOAT_EQ(value++, data[i]) << "i=" << i;
    }
  }

  // Consume data using different sizes, acquire audio frames from the FIFO
  // and verify that the retrieved values matches the values written by the
  // producer.
  void ConsumeTest(int frames_to_consume) {
    int start_value = 0;
    SCOPED_TRACE(base::StringPrintf("Checking frames_to_consume %d",
                 frames_to_consume));
    pull_fifo_.Consume(audio_bus_.get(), frames_to_consume);
    for (auto channel : audio_bus_->AllChannels()) {
      VerifyValue(channel.first(static_cast<size_t>(frames_to_consume)),
                  start_value);
    }
    start_value += frames_to_consume;
    EXPECT_LT(last_frame_delay_, audio_bus_->frames());
  }

  // AudioPullFifo::ReadCB implementation where we increase a value for each
  // audio frame that we provide. Note that all channels are given the same
  // value to simplify the verification.
  virtual void ProvideInput(int frame_delay, AudioBus* audio_bus) {
    ASSERT_GT(frame_delay, last_frame_delay_);
    last_frame_delay_ = frame_delay;

    EXPECT_EQ(audio_bus->channels(), audio_bus_->channels());
    EXPECT_EQ(audio_bus->frames(), kMaxFramesInFifo);
    for (auto channel : audio_bus->AllChannels()) {
      std::ranges::generate(
          channel, [value = fill_value_]() mutable { return value++; });
    }

    fill_value_ += audio_bus_->frames();
  }

 protected:
  AudioPullFifo pull_fifo_;
  std::unique_ptr<AudioBus> audio_bus_;
  int fill_value_ = 0;
  int last_frame_delay_ = -1;
};

TEST_P(AudioPullFifoTest, Consume) {
  ConsumeTest(GetParam());
}

// Test common |frames_to_consume| values which will be used as input
// parameter to AudioPullFifo::Consume() when the consumer asks for data.
INSTANTIATE_TEST_SUITE_P(
    AudioPullFifoTest,
    AudioPullFifoTest,
    testing::Values(544, 512, 512, 512, 512, 2048, 544, 441, 440, 433, 500));

}  // namespace media
