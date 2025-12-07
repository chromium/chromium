// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


#include "media/base/amplitude_peak_detector.h"

#include <string_view>

#include "base/containers/span.h"
#include "base/location.h"
#include "base/test/bind.h"
#include "media/base/audio_bus.h"
#include "media/base/audio_sample_types.h"
#include "media/base/sample_format.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

// The loudness threshold in amplitude_peak_detector.cc corresponds to 0.5.
constexpr float kLoudSample = 0.6;
constexpr float kQuietSample = 0.4;

// Default values
constexpr int kChannels = 2;
constexpr int kFrames = 240;
constexpr SampleFormat kSampleFormat = kSampleFormatS32;

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

    bus->channel_span(location.channel)[location.index] = value;

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

class SampleAmplitudePeakDetector : public AmplitudePeakDetectorTest {
 public:
  SampleAmplitudePeakDetector() = default;

  SampleAmplitudePeakDetector(const SampleAmplitudePeakDetector&) = delete;
  SampleAmplitudePeakDetector& operator=(const SampleAmplitudePeakDetector&) =
      delete;

  ~SampleAmplitudePeakDetector() override = default;

  template <typename SampleType>
  void VerifyNoPeaksFoundInSilence() {
    std::vector<SampleType> samples(
        kFrames, FixedSampleTypeTraits<SampleType>::kZeroPointValue);

    VerifyFindPeaks<SampleType>(samples, /*expect_peak=*/false,
                                "No peak should be detected in zeroed values");
  }

  template <>
  void VerifyNoPeaksFoundInSilence<float>() {
    std::vector<float> samples(kFrames,
                               FloatSampleTypeTraits<float>::kZeroPointValue);

    VerifyFindPeaks<float>(samples, /*expect_peak=*/false,
                           "No peak should be detected in zeroed values");
  }

  template <typename SampleType>
  void RunSimpleDetectionTest(float value,
                              bool expect_peak,
                              std::string_view message) {
    std::vector<SampleType> samples(
        kFrames, FixedSampleTypeTraits<SampleType>::kZeroPointValue);

    // Verify finding peaks for values at the start of the data range.
    samples[0] = FixedSampleTypeTraits<SampleType>::FromFloat(value);
    VerifyFindPeaks<SampleType>(samples, expect_peak, message);

    // Reset the value.
    samples[0] = FixedSampleTypeTraits<SampleType>::kZeroPointValue;

    // Verify finding peaks for values at the end of the data range.
    samples[kFrames - 1] = FixedSampleTypeTraits<SampleType>::FromFloat(value);
    VerifyFindPeaks<SampleType>(samples, expect_peak, message);
  }

  template <>
  void RunSimpleDetectionTest<float>(float value,
                                     bool expect_peak,
                                     std::string_view message) {
    std::vector<float> samples(kFrames,
                               FloatSampleTypeTraits<float>::kZeroPointValue);

    // Verify finding peaks for values at the start of the data range.
    samples[0] = FloatSampleTypeTraits<float>::FromFloat(value);
    VerifyFindPeaks<float>(samples, expect_peak, message);

    // Reset the value.
    samples[0] = FloatSampleTypeTraits<float>::kZeroPointValue;

    // Verify finding peaks for values at the end of the data range.
    samples[kFrames - 1] = FloatSampleTypeTraits<float>::FromFloat(value);
    VerifyFindPeaks<float>(samples, expect_peak, message);
  }

  SampleFormat sample_format() { return static_cast<SampleFormat>(GetParam()); }

  template <typename SampleType>
  void VerifyFindPeaks(base::span<const SampleType> data,
                       bool expect_peak,
                       std::string_view message) {
    CreateDetector(
        expect_peak
            ? base::MakeExpectedRunAtLeastOnceClosure(FROM_HERE, message)
            : base::MakeExpectedNotRunClosure(FROM_HERE, message));

    peak_detector_->FindPeak(base::as_byte_span(data), sample_format());
  }

  template <>
  void VerifyFindPeaks<float>(base::span<const float> data,
                              bool expect_peak,
                              std::string_view message) {
    CreateDetector(
        expect_peak
            ? base::MakeExpectedRunAtLeastOnceClosure(FROM_HERE, message)
            : base::MakeExpectedNotRunClosure(FROM_HERE, message));

    peak_detector_->FindPeak(
        base::as_byte_span(base::allow_nonunique_obj, data), sample_format());
  }
};

TEST_P(SampleAmplitudePeakDetector, NoPeaks_Silent) {
  switch (sample_format()) {
    case kSampleFormatU8:
      VerifyNoPeaksFoundInSilence<uint8_t>();
      break;
    case kSampleFormatS16:
      VerifyNoPeaksFoundInSilence<int16_t>();
      break;
    case kSampleFormatS32:
      VerifyNoPeaksFoundInSilence<int32_t>();
      break;
    case kSampleFormatF32: {
      VerifyNoPeaksFoundInSilence<float>();
      break;
    }
    default:
      NOTREACHED();
  }
}

TEST_P(SampleAmplitudePeakDetector, NoPeaks_Quiet) {
  constexpr char message[] = "No peaks detected from quiet values";

  switch (sample_format()) {
    case kSampleFormatU8:
      RunSimpleDetectionTest<uint8_t>(kQuietSample, false, message);
      RunSimpleDetectionTest<uint8_t>(-kQuietSample, false, message);
      break;
    case kSampleFormatS16:
      RunSimpleDetectionTest<int16_t>(kQuietSample, false, message);
      RunSimpleDetectionTest<int16_t>(-kQuietSample, false, message);
      break;
    case kSampleFormatS32:
      RunSimpleDetectionTest<int32_t>(kQuietSample, false, message);
      RunSimpleDetectionTest<int32_t>(-kQuietSample, false, message);
      break;
    case kSampleFormatF32:
      RunSimpleDetectionTest<float>(kQuietSample, false, message);
      RunSimpleDetectionTest<float>(-kQuietSample, false, message);
      break;
    default:
      NOTREACHED();
  }
}

TEST_P(SampleAmplitudePeakDetector, Peaks_Loud) {
  constexpr char message[] = "Peaks detected in loud values";

  switch (sample_format()) {
    case kSampleFormatU8:
      RunSimpleDetectionTest<uint8_t>(kLoudSample, true, message);
      RunSimpleDetectionTest<uint8_t>(-kLoudSample, true, message);
      break;
    case kSampleFormatS16:
      RunSimpleDetectionTest<int16_t>(kLoudSample, true, message);
      RunSimpleDetectionTest<int16_t>(-kLoudSample, true, message);
      break;
    case kSampleFormatS32:
      RunSimpleDetectionTest<int32_t>(kLoudSample, true, message);
      RunSimpleDetectionTest<int32_t>(-kLoudSample, true, message);
      break;
    case kSampleFormatF32:
      RunSimpleDetectionTest<float>(kLoudSample, true, message);
      RunSimpleDetectionTest<float>(-kLoudSample, true, message);
      break;
    default:
      NOTREACHED();
  }
}

TEST_F(SampleAmplitudePeakDetector, Sequence_SilenceAndQuiet) {
  std::vector<int32_t> silent_data(
      kFrames, SignedInt32SampleTypeTraits::kZeroPointValue);
  std::vector<int32_t> quiet_data(
      kFrames, SignedInt32SampleTypeTraits::FromFloat(kQuietSample));

  int peak_count = 0;
  CreateDetectorWithPeakCounter(&peak_count);

  // Make sure peaks are never detected in silent and quiet samples.
  peak_detector_->FindPeak(base::as_byte_span(silent_data), kSampleFormat);
  peak_detector_->FindPeak(base::as_byte_span(quiet_data), kSampleFormat);
  peak_detector_->FindPeak(base::as_byte_span(silent_data), kSampleFormat);
  peak_detector_->FindPeak(base::as_byte_span(quiet_data), kSampleFormat);

  EXPECT_EQ(peak_count, 0);
}

TEST_F(SampleAmplitudePeakDetector, Sequence_ShortPeak) {
  std::vector<int32_t> silent_data(
      kFrames, SignedInt32SampleTypeTraits::kZeroPointValue);
  std::vector<int32_t> loud_data(
      kFrames, SignedInt32SampleTypeTraits::FromFloat(kLoudSample));

  int peak_count = 0;
  CreateDetectorWithPeakCounter(&peak_count);

  peak_detector_->FindPeak(base::as_byte_span(silent_data), kSampleFormat);
  EXPECT_EQ(peak_count, 0);

  // We should immediately find a peak.
  peak_detector_->FindPeak(base::as_byte_span(loud_data), kSampleFormat);
  EXPECT_EQ(peak_count, 1);

  // Exiting the peak should not run the callback.
  peak_detector_->FindPeak(base::as_byte_span(silent_data), kSampleFormat);
  EXPECT_EQ(peak_count, 1);

  // The callback should be run again when another peak is found.
  peak_detector_->FindPeak(base::as_byte_span(loud_data), kSampleFormat);
  EXPECT_EQ(peak_count, 2);
}

TEST_F(SampleAmplitudePeakDetector, Sequence_LongPeak) {
  std::vector<int32_t> silent_data(
      kFrames, SignedInt32SampleTypeTraits::kZeroPointValue);
  std::vector<int32_t> loud_data(
      kFrames, SignedInt32SampleTypeTraits::FromFloat(kLoudSample));

  int peak_count = 0;
  CreateDetectorWithPeakCounter(&peak_count);

  peak_detector_->FindPeak(base::as_byte_span(silent_data), kSampleFormat);
  EXPECT_EQ(peak_count, 0);

  // Long peaks should only run the callback once.
  peak_detector_->FindPeak(base::as_byte_span(loud_data), kSampleFormat);
  peak_detector_->FindPeak(base::as_byte_span(loud_data), kSampleFormat);
  peak_detector_->FindPeak(base::as_byte_span(loud_data), kSampleFormat);
  EXPECT_EQ(peak_count, 1);

  // Exiting the peak should not run the callback.
  peak_detector_->FindPeak(base::as_byte_span(silent_data), kSampleFormat);
  EXPECT_EQ(peak_count, 1);
}

TEST_F(SampleAmplitudePeakDetector, NoTracing) {
  std::vector<int32_t> loud_data(
      kFrames, SignedInt32SampleTypeTraits::FromFloat(kLoudSample));

  int peak_count = 0;
  CreateDetectorWithPeakCounter(&peak_count);
  peak_detector_->SetIsTracingEnabledForTests(false);

  // Callbacks should not be run when tracing is disabled.
  peak_detector_->FindPeak(base::as_byte_span(loud_data), kSampleFormat);
  EXPECT_EQ(peak_count, 0);
}

INSTANTIATE_TEST_SUITE_P(SampleAmplitudePeakDetector,
                         SampleAmplitudePeakDetector,
                         testing::Values(kSampleFormatU8,
                                         kSampleFormatS16,
                                         kSampleFormatS32,
                                         kSampleFormatF32));

}  // namespace media
