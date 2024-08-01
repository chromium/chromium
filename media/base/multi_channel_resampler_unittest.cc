// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "media/base/multi_channel_resampler.h"

#include <cmath>
#include <memory>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "media/base/audio_bus.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

// Just test a basic resampling case.  The SincResampler unit test will take
// care of accuracy testing; we just need to check that multichannel works as
// expected within some tolerance.
static const float kScaleFactor = 192000.0f / 44100.0f;

// Simulate large and small sample requests used by the different audio paths.
static const int kHighLatencySize = 8192;
// Low latency buffers show a larger error than high latency ones.  Which makes
// sense since each error represents a larger portion of the total request.
static const int kLowLatencySize = 128;

// Test fill value.
static const float kFillValue = 0.1f;

// Chosen arbitrarily based on what each resampler reported during testing.
static const double kLowLatencyMaxRMSError = 0.0036;
static const double kLowLatencyMaxError = 0.04;
static const double kHighLatencyMaxRMSError = 0.0036;
static const double kHighLatencyMaxError = 0.04;

class MultiChannelResamplerTest
    : public testing::TestWithParam<int> {
 public:
  MultiChannelResamplerTest()
      : last_frame_delay_(-1) {
  }

  MultiChannelResamplerTest(const MultiChannelResamplerTest&) = delete;
  MultiChannelResamplerTest& operator=(const MultiChannelResamplerTest&) =
      delete;

  virtual ~MultiChannelResamplerTest() = default;

  void InitializeAudioData(int channels, int frames) {
    frames_ = frames;
    audio_bus_ = AudioBus::Create(channels, frames);
  }

  // MultiChannelResampler::MultiChannelAudioSourceProvider implementation, just
  // fills the provided audio_data with |kFillValue|.
  virtual void ProvideInput(int frame_delay, AudioBus* audio_bus) {
    EXPECT_GE(frame_delay, last_frame_delay_);
    last_frame_delay_ = frame_delay;

    float fill_value = fill_junk_values_ ? (1 / kFillValue) : kFillValue;
    EXPECT_EQ(audio_bus->channels(), audio_bus_->channels());
    for (int i = 0; i < audio_bus->channels(); ++i)
      for (int j = 0; j < audio_bus->frames(); ++j)
        audio_bus->channel(i)[j] = fill_value;
  }

  void MultiChannelTest(int channels, int frames, double expected_max_rms_error,
                        double expected_max_error) {
    InitializeAudioData(channels, frames);
    MultiChannelResampler resampler(
        channels, kScaleFactor, SincResampler::kDefaultRequestSize,
        base::BindRepeating(&MultiChannelResamplerTest::ProvideInput,
                            base::Unretained(this)));

    // First prime the resampler with some junk data, so we can verify Flush().
    fill_junk_values_ = true;
    resampler.Resample(1, audio_bus_.get());
    resampler.Flush();
    fill_junk_values_ = false;

    // The last frame delay should be strictly less than the total frame count.
    EXPECT_LT(last_frame_delay_, audio_bus_->frames());
    last_frame_delay_ = -1;

    // If Flush() didn't work, the rest of the tests will fail.
    resampler.Resample(frames, audio_bus_.get());
    TestValues(expected_max_rms_error, expected_max_error);
  }

  void HighLatencyTest(int channels) {
    MultiChannelTest(channels, kHighLatencySize, kHighLatencyMaxRMSError,
                     kHighLatencyMaxError);
  }

  void LowLatencyTest(int channels) {
    MultiChannelTest(channels, kLowLatencySize, kLowLatencyMaxRMSError,
                     kLowLatencyMaxError);
  }

  void TestValues(double expected_max_rms_error, double expected_max_error ) {
    // Calculate Root-Mean-Square-Error for the resampling.
    double max_error = 0.0;
    double sum_of_squares = 0.0;
    for (int i = 0; i < audio_bus_->channels(); ++i) {
      for (int j = 0; j < frames_; ++j) {
        // Ensure all values are accounted for.
        ASSERT_NE(audio_bus_->channel(i)[j], 0);

        double error = fabs(audio_bus_->channel(i)[j] - kFillValue);
        max_error = std::max(max_error, error);
        sum_of_squares += error * error;
      }
    }

    double rms_error = sqrt(
        sum_of_squares / (frames_ * audio_bus_->channels()));

    EXPECT_LE(rms_error, expected_max_rms_error);
    EXPECT_LE(max_error, expected_max_error);
  }

 protected:
  int frames_;
  bool fill_junk_values_;
  std::unique_ptr<AudioBus> audio_bus_;
  int last_frame_delay_;
};

TEST_P(MultiChannelResamplerTest, HighLatency) {
  HighLatencyTest(GetParam());
}

TEST_P(MultiChannelResamplerTest, LowLatency) {
  LowLatencyTest(GetParam());
}

// Test common channel layouts: mono, stereo, 5.1, 7.1.
INSTANTIATE_TEST_SUITE_P(MultiChannelResamplerTest,
                         MultiChannelResamplerTest,
                         testing::Values(1, 2, 6, 8));

}  // namespace media
