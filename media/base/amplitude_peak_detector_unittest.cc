// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "media/base/amplitude_peak_detector.h"

#include <string_view>

#include "base/location.h"
#include "base/test/bind.h"
#include "media/base/audio_sample_types.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

// The loudness threshold in amplitude_peak_detector.cc corresponds to 0.5.
constexpr float kLoudSample = 0.6;
constexpr float kQuietSample = 0.4;

// Default values
constexpr int kChannels = 2;
constexpr int kFrames = 240;

struct SampleLocation {
  int channel;
  int index;
};

class AmplitudePeakDetectorTest : public testing::TestWithParam<int> {
 public:
  AmplitudePeakDetectorTest() = default;

  AmplitudePeakDetectorTest(const AmplitudePeakDetectorTest&) = delete;
  AmplitudePeakDetectorTest& operator=(const AmplitudePeakDetectorTest&) =
      delete;

  ~AmplitudePeakDetectorTest() override = default;

  void CreateDetector(AmplitudePeakDetector::PeakDetectedCB callback) {
    peak_detector_ =
        std::make_unique<AmplitudePeakDetector>(std::move(callback));

    peak_detector_->SetIsTracingEnabledForTests(true);
  }

  void CreateDetectorWithPeakCounter(int* peak_count) {
    CreateDetector(
        base::BindLambdaForTesting([peak_count]() { ++(*peak_count); }));
  }

  std::unique_ptr<AudioBus> GetSilentAudioBus() {
    auto bus = AudioBus::Create(kChannels, kFrames);
    bus->Zero();

    return bus;
  }

  // Creates an AudioBus with exactly one sample set.
  std::unique_ptr<AudioBus> GetAudioBusWithSetValue(
      float value,
      const SampleLocation& location) {
    auto bus = GetSilentAudioBus();

    bus->channel(location.channel)[location.index] = value;

    return bus;
  }

  std::unique_ptr<AudioBus> GetAudioBusWithLoudValue() {
    return GetAudioBusWithSetValue(kLoudSample, {0, 0});
  }

  void RunSimpleDetectionTest(float value,
                              bool expect_peak,
                              std::string_view message) {
    const SampleLocation kTestSampleLocations[] = {
        {0, 0},
        {0, kFrames / 2},
        {0, kFrames - 1},
        {kChannels - 1, 0},
        {kChannels - 1, kFrames / 2},
        {kChannels - 1, kFrames - 1},
    };

    for (const auto& location : kTestSampleLocations) {
      CreateDetector(
          expect_peak
              ? base::MakeExpectedRunAtLeastOnceClosure(FROM_HERE, message)
              : base::MakeExpectedNotRunClosure(FROM_HERE, message));

      auto bus = GetAudioBusWithSetValue(value, location);
      peak_detector_->FindPeak(bus.get());
    }
  }

 protected:
  std::unique_ptr<AmplitudePeakDetector> peak_detector_;
};

TEST_F(AmplitudePeakDetectorTest, Constructor) {
  peak_detector_ =
      std::make_unique<AmplitudePeakDetector>(base::MakeExpectedNotRunClosure(
          FROM_HERE, "Callback should not be run by default"));
}

TEST_F(AmplitudePeakDetectorTest, NoPeaks_Silent) {
  CreateDetector(base::MakeExpectedNotRunClosure(
      FROM_HERE, "No peaks should be detected for silent data"));

  auto bus = GetSilentAudioBus();
  peak_detector_->FindPeak(bus.get());
}

TEST_F(AmplitudePeakDetectorTest, NoPeaks_QuietValue) {
  RunSimpleDetectionTest(kQuietSample, /*expect_peak=*/false,
                         "No peaks should be detected for positive quiet data");
  RunSimpleDetectionTest(-kQuietSample, /*expect_peak=*/false,
                         "No peaks should be detected for negative quiet data");
}

TEST_F(AmplitudePeakDetectorTest, Peaks_LoudValue) {
  RunSimpleDetectionTest(kLoudSample, /*expect_peak=*/true,
                         "Peaks should be detected for positive loud data");
  RunSimpleDetectionTest(-kLoudSample, /*expect_peak=*/true,
                         "Peaks should be detected for negative loud data");
}

TEST_F(AmplitudePeakDetectorTest, Sequence_SilenceAndQuiet) {
  auto silent_bus = GetSilentAudioBus();
  auto quiet_bus = GetAudioBusWithSetValue(kQuietSample, {0, 0});

  int peak_count = 0;
  CreateDetectorWithPeakCounter(&peak_count);

  // Make sure peaks are never detected in silent and quiet samples.
  peak_detector_->FindPeak(silent_bus.get());
  peak_detector_->FindPeak(quiet_bus.get());
  peak_detector_->FindPeak(silent_bus.get());
  peak_detector_->FindPeak(quiet_bus.get());

  EXPECT_EQ(peak_count, 0);
}

TEST_F(AmplitudePeakDetectorTest, Sequence_ShortPeak) {
  auto silent_bus = GetSilentAudioBus();
  auto loud_bus = GetAudioBusWithLoudValue();

  int peak_count = 0;
  CreateDetectorWithPeakCounter(&peak_count);

  peak_detector_->FindPeak(silent_bus.get());
  EXPECT_EQ(peak_count, 0);

  // We should immediately find a peak.
  peak_detector_->FindPeak(loud_bus.get());
  EXPECT_EQ(peak_count, 1);

  // Exiting the peak should not run the callback.
  peak_detector_->FindPeak(silent_bus.get());
  EXPECT_EQ(peak_count, 1);

  // The callback should be run again when another peak is found.
  peak_detector_->FindPeak(loud_bus.get());
  EXPECT_EQ(peak_count, 2);
}

TEST_F(AmplitudePeakDetectorTest, Sequence_LongPeak) {
  auto silent_bus = GetSilentAudioBus();
  auto loud_bus = GetAudioBusWithLoudValue();

  int peak_count = 0;
  CreateDetectorWithPeakCounter(&peak_count);

  peak_detector_->FindPeak(silent_bus.get());
  EXPECT_EQ(peak_count, 0);

  // Long peaks should only run the callback once.
  peak_detector_->FindPeak(loud_bus.get());
  peak_detector_->FindPeak(loud_bus.get());
  peak_detector_->FindPeak(loud_bus.get());
  EXPECT_EQ(peak_count, 1);

  // Exiting the peak should not run the callback.
  peak_detector_->FindPeak(silent_bus.get());
  EXPECT_EQ(peak_count, 1);
}

TEST_F(AmplitudePeakDetectorTest, NoTracing) {
  auto loud_bus = GetAudioBusWithLoudValue();

  int peak_count = 0;
  CreateDetectorWithPeakCounter(&peak_count);
  peak_detector_->SetIsTracingEnabledForTests(false);

  // Callbacks should not be run when tracing is disabled.
  peak_detector_->FindPeak(loud_bus.get());
  EXPECT_EQ(peak_count, 0);
}

class FixedSampleAmplitudePeakDetector : public AmplitudePeakDetectorTest {
 public:
  FixedSampleAmplitudePeakDetector() = default;

  FixedSampleAmplitudePeakDetector(const FixedSampleAmplitudePeakDetector&) =
      delete;
  FixedSampleAmplitudePeakDetector& operator=(
      const FixedSampleAmplitudePeakDetector&) = delete;

  ~FixedSampleAmplitudePeakDetector() override = default;

  template <typename SampleType>
  void VerifyNoPeaksFoundInSilence() {
    std::vector<SampleType> samples(
        kFrames, FixedSampleTypeTraits<SampleType>::kZeroPointValue);

    VerifyFindPeaks(samples.data(), /*expect_peak=*/false,
                    "No peak should be detected in zeroed values");
  }

  template <typename SampleType>
  void RunSimpleDetectionTest_Fixed(float value,
                                    bool expect_peak,
                                    std::string_view message) {
    std::vector<SampleType> samples(
        kFrames, FixedSampleTypeTraits<SampleType>::kZeroPointValue);

    // Verify finding peaks for values at the start of the data range.
    samples[0] = FixedSampleTypeTraits<SampleType>::FromFloat(value);
    VerifyFindPeaks(samples.data(), expect_peak, message);

    // Reset the value.
    samples[0] = FixedSampleTypeTraits<SampleType>::kZeroPointValue;

    // Verify finding peaks for values at the end of the data range.
    samples[kFrames - 1] = FixedSampleTypeTraits<SampleType>::FromFloat(value);
    VerifyFindPeaks(samples.data(), expect_peak, message);
  }

  int bytes_per_samples() { return GetParam(); }

  void VerifyFindPeaks(const void* data,
                       bool expect_peak,
                       std::string_view message) {
    CreateDetector(
        expect_peak
            ? base::MakeExpectedRunAtLeastOnceClosure(FROM_HERE, message)
            : base::MakeExpectedNotRunClosure(FROM_HERE, message));

    peak_detector_->FindPeak(data, kFrames, bytes_per_samples());
  }
};

TEST_P(FixedSampleAmplitudePeakDetector, NoPeaks_Silent) {
  switch (bytes_per_samples()) {
    case 1:
      VerifyNoPeaksFoundInSilence<uint8_t>();
      break;
    case 2:
      VerifyNoPeaksFoundInSilence<int16_t>();
      break;
    case 4:
      VerifyNoPeaksFoundInSilence<int32_t>();
      break;
  }
}

TEST_P(FixedSampleAmplitudePeakDetector, NoPeaks_Quiet) {
  constexpr char message[] = "No peaks detected from quiet values";

  switch (bytes_per_samples()) {
    case 1:
      RunSimpleDetectionTest_Fixed<uint8_t>(kQuietSample, false, message);
      RunSimpleDetectionTest_Fixed<uint8_t>(-kQuietSample, false, message);
      break;
    case 2:
      RunSimpleDetectionTest_Fixed<int16_t>(kQuietSample, false, message);
      RunSimpleDetectionTest_Fixed<int16_t>(-kQuietSample, false, message);
      break;
    case 4:
      RunSimpleDetectionTest_Fixed<int32_t>(kQuietSample, false, message);
      RunSimpleDetectionTest_Fixed<int32_t>(-kQuietSample, false, message);
      break;
  }
}

TEST_P(FixedSampleAmplitudePeakDetector, Peaks_Loud) {
  constexpr char message[] = "Peaks detected in loud values";

  switch (bytes_per_samples()) {
    case 1:
      RunSimpleDetectionTest_Fixed<uint8_t>(kLoudSample, true, message);
      RunSimpleDetectionTest_Fixed<uint8_t>(-kLoudSample, true, message);
      break;
    case 2:
      RunSimpleDetectionTest_Fixed<int16_t>(kLoudSample, true, message);
      RunSimpleDetectionTest_Fixed<int16_t>(-kLoudSample, true, message);
      break;
    case 4:
      RunSimpleDetectionTest_Fixed<int32_t>(kLoudSample, true, message);
      RunSimpleDetectionTest_Fixed<int32_t>(-kLoudSample, true, message);
      break;
  }
}

TEST_F(FixedSampleAmplitudePeakDetector, Sequence_SilenceAndQuiet) {
  constexpr int kBytesPerSample = 4;
  std::vector<uint32_t> silent_data(
      kFrames, SignedInt32SampleTypeTraits::kZeroPointValue);
  std::vector<uint32_t> quiet_data(
      kFrames, SignedInt32SampleTypeTraits::FromFloat(kQuietSample));

  int peak_count = 0;
  CreateDetectorWithPeakCounter(&peak_count);

  // Make sure peaks are never detected in silent and quiet samples.
  peak_detector_->FindPeak(silent_data.data(), kFrames, kBytesPerSample);
  peak_detector_->FindPeak(quiet_data.data(), kFrames, kBytesPerSample);
  peak_detector_->FindPeak(silent_data.data(), kFrames, kBytesPerSample);
  peak_detector_->FindPeak(quiet_data.data(), kFrames, kBytesPerSample);

  EXPECT_EQ(peak_count, 0);
}

TEST_F(FixedSampleAmplitudePeakDetector, Sequence_ShortPeak) {
  constexpr int kBytesPerSample = 4;
  std::vector<uint32_t> silent_data(
      kFrames, SignedInt32SampleTypeTraits::kZeroPointValue);
  std::vector<uint32_t> loud_data(
      kFrames, SignedInt32SampleTypeTraits::FromFloat(kLoudSample));

  int peak_count = 0;
  CreateDetectorWithPeakCounter(&peak_count);

  peak_detector_->FindPeak(silent_data.data(), kFrames, kBytesPerSample);
  EXPECT_EQ(peak_count, 0);

  // We should immediately find a peak.
  peak_detector_->FindPeak(loud_data.data(), kFrames, kBytesPerSample);
  EXPECT_EQ(peak_count, 1);

  // Exiting the peak should not run the callback.
  peak_detector_->FindPeak(silent_data.data(), kFrames, kBytesPerSample);
  EXPECT_EQ(peak_count, 1);

  // The callback should be run again when another peak is found.
  peak_detector_->FindPeak(loud_data.data(), kFrames, kBytesPerSample);
  EXPECT_EQ(peak_count, 2);
}

TEST_P(FixedSampleAmplitudePeakDetector, Sequence_LongPeak) {
  constexpr int kBytesPerSample = 4;
  std::vector<uint32_t> silent_data(
      kFrames, SignedInt32SampleTypeTraits::kZeroPointValue);
  std::vector<uint32_t> loud_data(
      kFrames, SignedInt32SampleTypeTraits::FromFloat(kLoudSample));

  int peak_count = 0;
  CreateDetectorWithPeakCounter(&peak_count);

  peak_detector_->FindPeak(silent_data.data(), kFrames, kBytesPerSample);
  EXPECT_EQ(peak_count, 0);

  // Long peaks should only run the callback once.
  peak_detector_->FindPeak(loud_data.data(), kFrames, kBytesPerSample);
  peak_detector_->FindPeak(loud_data.data(), kFrames, kBytesPerSample);
  peak_detector_->FindPeak(loud_data.data(), kFrames, kBytesPerSample);
  EXPECT_EQ(peak_count, 1);

  // Exiting the peak should not run the callback.
  peak_detector_->FindPeak(silent_data.data(), kFrames, kBytesPerSample);
  EXPECT_EQ(peak_count, 1);
}

TEST_P(FixedSampleAmplitudePeakDetector, NoTracing) {
  constexpr int kBytesPerSample = 4;
  std::vector<uint32_t> loud_data(
      kFrames, SignedInt32SampleTypeTraits::FromFloat(kLoudSample));

  int peak_count = 0;
  CreateDetectorWithPeakCounter(&peak_count);
  peak_detector_->SetIsTracingEnabledForTests(false);

  // Callbacks should not be run when tracing is disabled.
  peak_detector_->FindPeak(loud_data.data(), kFrames, kBytesPerSample);
  EXPECT_EQ(peak_count, 0);
}

INSTANTIATE_TEST_SUITE_P(FixedSampleAmplitudePeakDetector,
                         FixedSampleAmplitudePeakDetector,
                         testing::Values(1, 2, 4));

}  // namespace media
