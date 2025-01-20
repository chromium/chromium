// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/audio_shifter.h"

#include <stddef.h>

#include <algorithm>
#include <cmath>
#include <memory>
#include <vector>

#include "base/time/time.h"
#include "media/base/audio_bus.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

const int kSampleRate = 48000;
const int kInputPacketSize = 48;
const int kOutputPacketSize = 24;

class AudioShifterTest :
      public ::testing::TestWithParam<::testing::tuple<int, int, int, bool> > {
 public:
  AudioShifterTest()
      : shifter_(base::Milliseconds(2000),
                 base::Milliseconds(3),
                 base::Milliseconds(100),
                 kSampleRate,
                 2),
        end2end_latency_(base::Milliseconds(30)),
        playback_latency_(base::Milliseconds(10)) {}

  void SetupInput(int size, base::TimeDelta rate) {
    input_size_ = size;
    input_rate_ = rate;
  }

  std::unique_ptr<AudioBus> CreateTestInput() {
    std::unique_ptr<AudioBus> input(AudioBus::Create(2, input_size_));

    for (auto channel : input->AllChannels()) {
      std::ranges::generate(
          channel, [sample = input_sample_n_]() mutable { return sample++; });
    }

    input_sample_n_ += input_size_;

    if (tag_input_) {
      input->channel_span(0)[0] = 10000000.0;
      tag_input_ = false;
      expect_smooth_output_ = false;
    }
    return input;
  }

  void SetupOutput(int size, base::TimeDelta rate) {
    test_output_ = AudioBus::Create(2, size);
    output_rate_ = rate;
  }

  void SetUp() override {
    SetupInput(
        kInputPacketSize + ::testing::get<0>(GetParam()) - 1,
        base::Microseconds(1000 + ::testing::get<1>(GetParam()) * 5 - 5));
    SetupOutput(
        kOutputPacketSize,
        base::Microseconds(500 + ::testing::get<2>(GetParam()) * 3 - 3));
    if (::testing::get<3>(GetParam())) {
      end2end_latency_ = -end2end_latency_;
    }
  }

  void Run(size_t loops) {
    for (size_t i = 0; i < loops;) {
      if (now_ >= time_to_push_) {
        shifter_.Push(CreateTestInput(), now_ + end2end_latency_);
        time_to_push_ += input_rate_;
        i++;
      }
      if (now_ >= time_to_pull_) {
        shifter_.Pull(test_output_.get(), now_ + playback_latency_);
        bool silence = true;
        auto first_channel = test_output_->channel_span(0);
        for (size_t j = 0;
             j < static_cast<size_t>(test_output_->frames());
             j++) {
          if (first_channel[j] != 0.0) {
            silence = false;
            if (first_channel[j] > 3000000.0) {
              marker_outputs_.push_back(now_ + playback_latency_ +
                                        base::Seconds(j) / kSampleRate);
            } else {
              // We don't expect smooth output once we insert a tag,
              // or in the very beginning.
              if (expect_smooth_output_ && output_sample_ > 500.0) {
                EXPECT_GT(first_channel[j], output_sample_ - 3) << "j = " << j;
                if (first_channel[j] > output_sample_ + kOutputPacketSize / 2) {
                  skip_outputs_.push_back(now_ + playback_latency_);
                }
              }
              output_sample_ = first_channel[j];
            }
          }
        }
        if (silence) {
          silent_outputs_.push_back(now_);
        }
        time_to_pull_ += output_rate_;
      }
      now_ += std::min(time_to_push_ - now_,
                       time_to_pull_ - now_);
    }
  }

  void RunAndCheckSync(size_t loops) {
    Run(100);
    size_t expected_silent_outputs = silent_outputs_.size();
    Run(loops);
    tag_input_ = true;
    CHECK(marker_outputs_.empty());
    base::TimeTicks expected_mark_time = time_to_push_ + end2end_latency_;
    Run(100);
    if (end2end_latency_.is_positive()) {
      CHECK(!marker_outputs_.empty());
      base::TimeDelta actual_offset = marker_outputs_[0] - expected_mark_time;
      EXPECT_LT(actual_offset, base::Microseconds(100));
      EXPECT_GT(actual_offset, base::Microseconds(-100));
    } else {
      EXPECT_GT(marker_outputs_.size(), 0UL);
    }
    EXPECT_EQ(expected_silent_outputs, silent_outputs_.size());
  }

 protected:
  AudioShifter shifter_;
  base::TimeDelta input_rate_;
  base::TimeDelta output_rate_;
  base::TimeDelta end2end_latency_;
  base::TimeDelta playback_latency_;
  base::TimeTicks time_to_push_;
  base::TimeTicks time_to_pull_;
  base::TimeTicks now_;
  std::unique_ptr<AudioBus> test_input_;
  std::unique_ptr<AudioBus> test_output_;
  std::vector<base::TimeTicks> silent_outputs_;
  std::vector<base::TimeTicks> skip_outputs_;
  std::vector<base::TimeTicks> marker_outputs_;
  size_t input_size_;
  bool tag_input_ = false;
  bool expect_smooth_output_ = true;
  size_t input_sample_n_ = 0;
  double output_sample_ = 0;
};

TEST_P(AudioShifterTest, TestSync) {
  RunAndCheckSync(1000);
  EXPECT_EQ(0UL, skip_outputs_.size());
}

TEST_P(AudioShifterTest, TestSyncWithPush) {
  // Push some extra audio.
  shifter_.Push(CreateTestInput(), now_ - base::TimeDelta(input_rate_));
  RunAndCheckSync(1000);
  EXPECT_LE(skip_outputs_.size(), 2UL);
}

TEST_P(AudioShifterTest, TestSyncWithPull) {
  // Output should smooth out eventually, but that is not tested yet.
  expect_smooth_output_ = false;
  Run(100);
  for (int i = 0; i < 100; i++) {
    shifter_.Pull(test_output_.get(), now_ + base::Milliseconds(i));
  }
  RunAndCheckSync(1000);
  EXPECT_LE(skip_outputs_.size(), 1UL);
}

TEST_P(AudioShifterTest, UnderOverFlow) {
  expect_smooth_output_ = false;
  SetupInput(
      kInputPacketSize + ::testing::get<0>(GetParam()) * 10 - 10,
      base::Microseconds(1000 + ::testing::get<1>(GetParam()) * 100 - 100));
  SetupOutput(
      kOutputPacketSize,
      base::Microseconds(500 + ::testing::get<2>(GetParam()) * 50 - 50));
  // Sane output is not expected, but let's make sure we don't crash.
  Run(1000);
}

// Note: First argument is optional and intentionally left blank.
// (it's a prefix for the generated test cases)
INSTANTIATE_TEST_SUITE_P(All,
                         AudioShifterTest,
                         ::testing::Combine(::testing::Range(0, 3),
                                            ::testing::Range(0, 3),
                                            ::testing::Range(0, 3),
                                            ::testing::Bool()));

}  // namespace media
