// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/filters/video_cadence_estimator.h"

#include <math.h>
#include <stddef.h>

#include <memory>

#include "base/numerics/safe_conversions.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/stringprintf.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "media/base/media_switches.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

// See VideoCadenceEstimator header for more details.
constexpr auto kMinimumAcceptableTimeBetweenGlitches = base::Seconds(8);

// Slows down the given |fps| according to NTSC field reduction standards; see
// http://en.wikipedia.org/wiki/Frame_rate#Digital_video_and_television
static double NTSC(double fps) {
  return fps / 1.001;
}

static base::TimeDelta Interval(double hertz) {
  return base::Seconds(1.0 / hertz);
}

std::vector<int> CreateCadenceFromString(const std::string& cadence) {
  CHECK_EQ('[', cadence.front());
  CHECK_EQ(']', cadence.back());

  std::vector<int> result;
  for (const std::string& token :
       base::SplitString(cadence.substr(1, cadence.length() - 2),
                         ":", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL)) {
    int cadence_value = 0;
    CHECK(base::StringToInt(token, &cadence_value)) << token;
    result.push_back(cadence_value);
  }

  return result;
}

static void VerifyCadenceVectorWithCustomDeviationAndDrift(
    VideoCadenceEstimator* estimator,
    double frame_hertz,
    double render_hertz,
    base::TimeDelta deviation,
    base::TimeDelta acceptable_drift,
    const std::string& expected_cadence) {
  SCOPED_TRACE(base::StringPrintf("Checking %.03f fps into %0.03f", frame_hertz,
                                  render_hertz));

  const std::vector<int> expected_cadence_vector =
      CreateCadenceFromString(expected_cadence);

  estimator->Reset();
  const bool cadence_changed = estimator->UpdateCadenceEstimate(
      Interval(render_hertz), Interval(frame_hertz), deviation,
      acceptable_drift);
  EXPECT_EQ(cadence_changed, estimator->has_cadence());
  EXPECT_EQ(expected_cadence_vector.empty(), !estimator->has_cadence());

  // Nothing further to test.
  if (expected_cadence_vector.empty() || !estimator->has_cadence())
    return;

  EXPECT_EQ(expected_cadence_vector.size(),
            estimator->cadence_size_for_testing());

  // Spot two cycles of the cadence.
  for (size_t i = 0; i < expected_cadence_vector.size() * 2; ++i) {
    ASSERT_EQ(expected_cadence_vector[i % expected_cadence_vector.size()],
              estimator->GetCadenceForFrame(i));
  }
}

static void VerifyCadenceVectorWithCustomDrift(
    VideoCadenceEstimator* estimator,
    double frame_hertz,
    double render_hertz,
    base::TimeDelta acceptable_drift,
    const std::string& expected_cadence) {
  VerifyCadenceVectorWithCustomDeviationAndDrift(
      estimator, frame_hertz, render_hertz, base::TimeDelta(), acceptable_drift,
      expected_cadence);
}

static void VerifyCadenceVectorWithCustomDeviation(
    VideoCadenceEstimator* estimator,
    double frame_hertz,
    double render_hertz,
    base::TimeDelta deviation,
    const std::string& expected_cadence) {
  const base::TimeDelta acceptable_drift =
      std::max(Interval(frame_hertz) / 2, Interval(render_hertz));
  VerifyCadenceVectorWithCustomDeviationAndDrift(
      estimator, frame_hertz, render_hertz, deviation, acceptable_drift,
      expected_cadence);
}

static void VerifyCadenceVector(VideoCadenceEstimator* estimator,
                                double frame_hertz,
                                double render_hertz,
                                const std::string& expected_cadence) {
  const base::TimeDelta acceptable_drift =
      std::max(Interval(frame_hertz) / 2, Interval(render_hertz));
  VerifyCadenceVectorWithCustomDeviationAndDrift(
      estimator, frame_hertz, render_hertz, base::TimeDelta(), acceptable_drift,
      expected_cadence);
}

// Spot check common display and frame rate pairs for correctness.
TEST(VideoCadenceEstimatorTest, CadenceCalculations) {
  VideoCadenceEstimator estimator(kMinimumAcceptableTimeBetweenGlitches);
  estimator.set_cadence_hysteresis_threshold_for_testing(base::TimeDelta());

  const std::string kEmptyCadence = "[]";
  VerifyCadenceVector(&estimator, 1, NTSC(60), "[60]");

  VerifyCadenceVector(&estimator, 24, 60, "[3:2]");
  VerifyCadenceVector(&estimator, NTSC(24), 60, "[3:2]");
  VerifyCadenceVector(&estimator, 24, NTSC(60), "[3:2]");

  VerifyCadenceVector(&estimator, 25, 60, "[2:3:2:3:2]");
  VerifyCadenceVector(&estimator, NTSC(25), 60, "[2:3:2:3:2]");
  VerifyCadenceVector(&estimator, 25, NTSC(60), "[2:3:2:3:2]");

  VerifyCadenceVector(&estimator, 30, 60, "[2]");
  VerifyCadenceVector(&estimator, NTSC(30), 60, "[2]");
  VerifyCadenceVector(&estimator, 29.5, 60, kEmptyCadence);

  VerifyCadenceVector(&estimator, 50, 60, "[1:1:2:1:1]");
  VerifyCadenceVector(&estimator, NTSC(50), 60, "[1:1:2:1:1]");
  VerifyCadenceVector(&estimator, 50, NTSC(60), "[1:1:2:1:1]");

  VerifyCadenceVector(&estimator, NTSC(60), 60, "[1]");
  VerifyCadenceVector(&estimator, 60, NTSC(60), "[1]");

  VerifyCadenceVector(&estimator, 120, 60, "[1:0]");
  VerifyCadenceVector(&estimator, NTSC(120), 60, "[1:0]");
  VerifyCadenceVector(&estimator, 120, NTSC(60), "[1:0]");

  // Test cases for cadence below 1.
  VerifyCadenceVector(&estimator, 120, 24, "[1:0:0:0:0]");
  VerifyCadenceVector(&estimator, 120, 48, "[1:0:0:1:0]");
  VerifyCadenceVector(&estimator, 120, 72, "[1:0:1:0:1]");
  VerifyCadenceVector(&estimator, 90, 60, "[1:0:1]");

  // 50Hz is common in the EU.
  VerifyCadenceVector(&estimator, NTSC(24), 50, kEmptyCadence);
  VerifyCadenceVector(&estimator, 24, 50, kEmptyCadence);

  VerifyCadenceVector(&estimator, NTSC(25), 50, "[2]");
  VerifyCadenceVector(&estimator, 25, 50, "[2]");

  VerifyCadenceVector(&estimator, NTSC(30), 50, "[2:1:2]");
  VerifyCadenceVector(&estimator, 30, 50, "[2:1:2]");

  VerifyCadenceVector(&estimator, NTSC(60), 50, kEmptyCadence);
  VerifyCadenceVector(&estimator, 60, 50, kEmptyCadence);

}

// Check the extreme case that max_acceptable_drift is larger than
// minimum_time_until_max_drift.
TEST(VideoCadenceEstimatorTest, CadenceCalculationWithLargeDrift) {
  VideoCadenceEstimator estimator(kMinimumAcceptableTimeBetweenGlitches);
  estimator.set_cadence_hysteresis_threshold_for_testing(base::TimeDelta());

  base::TimeDelta drift = base::Hours(1);
  VerifyCadenceVectorWithCustomDrift(&estimator, 1, NTSC(60), drift, "[60]");

  VerifyCadenceVectorWithCustomDrift(&estimator, 30, 60, drift, "[2]");
  VerifyCadenceVectorWithCustomDrift(&estimator, NTSC(30), 60, drift, "[2]");
  VerifyCadenceVectorWithCustomDrift(&estimator, 30, NTSC(60), drift, "[2]");

  VerifyCadenceVectorWithCustomDrift(&estimator, 25, 60, drift, "[2]");
  VerifyCadenceVectorWithCustomDrift(&estimator, NTSC(25), 60, drift, "[2]");
  VerifyCadenceVectorWithCustomDrift(&estimator, 25, NTSC(60), drift, "[2]");

  // Test cases for cadence below 1.
  VerifyCadenceVectorWithCustomDrift(&estimator, 120, 24, drift, "[1]");
  VerifyCadenceVectorWithCustomDrift(&estimator, 120, 48, drift, "[1]");
  VerifyCadenceVectorWithCustomDrift(&estimator, 120, 72, drift, "[1]");
  VerifyCadenceVectorWithCustomDrift(&estimator, 90, 60, drift, "[1]");
}

TEST(VideoCadenceEstimatorTest, SimpleCadenceTest) {
  bool simple_cadence = VideoCadenceEstimator::HasSimpleCadence(
      Interval(60), Interval(30), kMinimumAcceptableTimeBetweenGlitches);
  // 60 Hz screen with 30 FPS video should be considered a simple cadence.
  EXPECT_TRUE(simple_cadence);
  simple_cadence = VideoCadenceEstimator::HasSimpleCadence(
      Interval(60), Interval(24), kMinimumAcceptableTimeBetweenGlitches);
  EXPECT_FALSE(simple_cadence);
}

// Check the case that the estimator excludes variable FPS case from Cadence.
TEST(VideoCadenceEstimatorTest, CadenceCalculationWithLargeDeviation) {
  VideoCadenceEstimator estimator(kMinimumAcceptableTimeBetweenGlitches);
  estimator.set_cadence_hysteresis_threshold_for_testing(base::TimeDelta());

  const base::TimeDelta deviation = base::Milliseconds(30);
  VerifyCadenceVectorWithCustomDeviation(&estimator, 1, 60, deviation, "[]");
  VerifyCadenceVectorWithCustomDeviation(&estimator, 30, 60, deviation, "[]");
  VerifyCadenceVectorWithCustomDeviation(&estimator, 25, 60, deviation, "[]");

  // Test cases for cadence with low refresh rate.
  VerifyCadenceVectorWithCustomDeviation(&estimator, 60, 12, deviation,
                                         "[1:0:0:0:0]");
}

TEST(VideoCadenceEstimatorTest, CadenceVariesWithAcceptableDrift) {
  VideoCadenceEstimator estimator(kMinimumAcceptableTimeBetweenGlitches);
  estimator.set_cadence_hysteresis_threshold_for_testing(base::TimeDelta());

  const base::TimeDelta render_interval = Interval(NTSC(60));
  const base::TimeDelta frame_interval = Interval(120);

  base::TimeDelta acceptable_drift = frame_interval / 2;
  EXPECT_FALSE(estimator.UpdateCadenceEstimate(
      render_interval, frame_interval, base::TimeDelta(), acceptable_drift));
  EXPECT_FALSE(estimator.has_cadence());

  // Increasing the acceptable drift should be result in more permissive
  // detection of cadence.
  acceptable_drift = render_interval;
  EXPECT_TRUE(estimator.UpdateCadenceEstimate(
      render_interval, frame_interval, base::TimeDelta(), acceptable_drift));
  EXPECT_TRUE(estimator.has_cadence());
  EXPECT_EQ("[1:0]", estimator.GetCadenceForTesting());
}

TEST(VideoCadenceEstimatorTest, CadenceVariesWithAcceptableGlitchTime) {
  VideoCadenceEstimator estimator(kMinimumAcceptableTimeBetweenGlitches);
  estimator.set_cadence_hysteresis_threshold_for_testing(base::TimeDelta());

  const base::TimeDelta render_interval = Interval(NTSC(60));
  const base::TimeDelta frame_interval = Interval(120);
  const base::TimeDelta acceptable_drift = frame_interval / 2;

  EXPECT_FALSE(estimator.UpdateCadenceEstimate(
      render_interval, frame_interval, base::TimeDelta(), acceptable_drift));
  EXPECT_FALSE(estimator.has_cadence());

  // Decreasing the acceptable glitch time should be result in more permissive
  // detection of cadence.
  VideoCadenceEstimator permissive_estimator(
      kMinimumAcceptableTimeBetweenGlitches / 2);
  permissive_estimator.set_cadence_hysteresis_threshold_for_testing(
      base::TimeDelta());
  EXPECT_TRUE(permissive_estimator.UpdateCadenceEstimate(
      render_interval, frame_interval, base::TimeDelta(), acceptable_drift));
  EXPECT_TRUE(permissive_estimator.has_cadence());
  EXPECT_EQ("[1:0]", permissive_estimator.GetCadenceForTesting());
}

TEST(VideoCadenceEstimatorTest, CadenceHystersisPreventsOscillation) {
  VideoCadenceEstimator estimator(kMinimumAcceptableTimeBetweenGlitches);

  const base::TimeDelta render_interval = Interval(30);
  const base::TimeDelta frame_interval = Interval(60);
  const base::TimeDelta acceptable_drift = frame_interval / 2;
  estimator.set_cadence_hysteresis_threshold_for_testing(render_interval * 2);

  // Cadence hysteresis should prevent the cadence from taking effect yet.
  EXPECT_FALSE(estimator.UpdateCadenceEstimate(
      render_interval, frame_interval, base::TimeDelta(), acceptable_drift));
  EXPECT_FALSE(estimator.has_cadence());

  // A second call should exceed cadence hysteresis and take into effect.
  EXPECT_TRUE(estimator.UpdateCadenceEstimate(
      render_interval, frame_interval, base::TimeDelta(), acceptable_drift));
  EXPECT_TRUE(estimator.has_cadence());

  // One bad interval shouldn't cause cadence to drop
  EXPECT_FALSE(
      estimator.UpdateCadenceEstimate(render_interval, frame_interval * 0.75,
                                      base::TimeDelta(), acceptable_drift));
  EXPECT_TRUE(estimator.has_cadence());

  // Resumption of cadence should clear bad interval count.
  EXPECT_FALSE(estimator.UpdateCadenceEstimate(
      render_interval, frame_interval, base::TimeDelta(), acceptable_drift));
  EXPECT_TRUE(estimator.has_cadence());

  // So one more bad interval shouldn't cause cadence to drop
  EXPECT_FALSE(
      estimator.UpdateCadenceEstimate(render_interval, frame_interval * 0.75,
                                      base::TimeDelta(), acceptable_drift));
  EXPECT_TRUE(estimator.has_cadence());

  // Two bad intervals should.
  EXPECT_TRUE(
      estimator.UpdateCadenceEstimate(render_interval, frame_interval * 0.75,
                                      base::TimeDelta(), acceptable_drift));
  EXPECT_FALSE(estimator.has_cadence());
}

TEST(VideoCadenceEstimatorTest, RenderIntervalChangingSkipsHystersis) {
  VideoCadenceEstimator estimator(kMinimumAcceptableTimeBetweenGlitches);

  const base::TimeDelta render_interval = Interval(60);
  const base::TimeDelta frame_interval = Interval(30);
  const base::TimeDelta acceptable_drift = frame_interval / 2;
  estimator.set_cadence_hysteresis_threshold_for_testing(render_interval * 4);

  // Wait for cadence to be detected.
  int it_count = 0;
  while (!estimator.has_cadence()) {
    estimator.UpdateCadenceEstimate(render_interval, frame_interval,
                                    base::TimeDelta(), acceptable_drift);
    it_count++;
    EXPECT_LE(it_count, 4);
  }

  // If |render_interval| changes, the hysteresis should be skipped and the
  // candence should be updated immediately.
  EXPECT_TRUE(estimator.UpdateCadenceEstimate(render_interval * 2,
                                              frame_interval, base::TimeDelta(),
                                              acceptable_drift));
  EXPECT_TRUE(estimator.has_cadence());

  // Minor changes (+/-10%) on |render_interval| should not trigger hyseteresis
  // skipping.
  EXPECT_FALSE(estimator.UpdateCadenceEstimate(
      render_interval * 2 * 0.91, frame_interval, base::TimeDelta(),
      acceptable_drift));
  EXPECT_TRUE(estimator.has_cadence());
}

void VerifyCadenceSequence(VideoCadenceEstimator* estimator,
                           double frame_rate,
                           double display_rate,
                           std::vector<int> expected_cadence) {
  SCOPED_TRACE(base::StringPrintf("Checking %.03f fps into %0.03f", frame_rate,
                                  display_rate));

  const base::TimeDelta render_interval = Interval(display_rate);
  const base::TimeDelta frame_interval = Interval(frame_rate);
  const base::TimeDelta acceptable_drift =
      frame_interval < render_interval ? render_interval : frame_interval;
  const base::TimeDelta test_runtime = base::Seconds(10 * 60);
  const int test_frames = base::ClampFloor(test_runtime / frame_interval);

  estimator->Reset();
  EXPECT_TRUE(estimator->UpdateCadenceEstimate(
      render_interval, frame_interval, base::TimeDelta(), acceptable_drift));
  EXPECT_TRUE(estimator->has_cadence());
  for (auto i = 0u; i < expected_cadence.size(); i++) {
    ASSERT_EQ(expected_cadence[i], estimator->GetCadenceForFrame(i))
        << " i=" << i;
  }

  int total_display_cycles = 0;
  for (int i = 0; i < test_frames; i++) {
    total_display_cycles += estimator->GetCadenceForFrame(i);
    base::TimeDelta drift =
        (total_display_cycles * render_interval) - ((i + 1) * frame_interval);
    EXPECT_LE(drift.magnitude(), acceptable_drift)
        << " i=" << i << " time=" << (total_display_cycles * render_interval);
    if (drift.magnitude() > acceptable_drift)
      break;
  }
}

}  // namespace media
