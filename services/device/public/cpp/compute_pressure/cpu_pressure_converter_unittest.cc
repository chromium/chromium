// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/public/cpp/compute_pressure/cpu_pressure_converter.h"

#include <memory>

#include "base/sequence_checker.h"
#include "base/test/gtest_util.h"
#include "base/test/task_environment.h"
#include "base/test/test_timeouts.h"
#include "base/time/time.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest-spi.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace device {

class CpuPressureConverterTest : public ::testing::Test {
 public:
  CpuPressureConverterTest(const CpuPressureConverterTest&) = delete;
  CpuPressureConverterTest& operator=(const CpuPressureConverterTest&) = delete;

 protected:
  CpuPressureConverterTest() {}
  ~CpuPressureConverterTest() override {}

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  CpuPressureConverter converter_;
};

TEST_F(CpuPressureConverterTest, CalculateStateValueTooLarge) {
  EXPECT_DCHECK_DEATH_WITH(converter_.CalculateState(1.1),
                           "unexpected value: 1.1");
}

TEST_F(CpuPressureConverterTest, CheckCalculateStateHysteresisUp) {
  std::array<double, 4> samples = {// kNominal value should be reported.
                                   0.3,
                                   // kFair value should be reported.
                                   0.7,
                                   // kSerious value should be reported.
                                   0.8,
                                   // kCritical value should be reported.
                                   1.0};

  std::array<mojom::PressureState, 4> states;
  for (size_t i = 0;
       i < static_cast<size_t>(mojom::PressureState::kMaxValue) + 1; i++) {
    states[i] = converter_.CalculateState(samples[i]);
  }

  EXPECT_THAT(states,
              ::testing::ElementsAre(
                  mojom::PressureState{mojom::PressureState::kNominal},
                  mojom::PressureState{mojom::PressureState::kFair},
                  mojom::PressureState{mojom::PressureState::kSerious},
                  mojom::PressureState{mojom::PressureState::kCritical}));
}

TEST_F(CpuPressureConverterTest, CheckCalculateStateHysteresisDown) {
  const size_t samples_count = 4;
  std::array<double, samples_count> samples = {
      // kCritical value should be reported.
      1.0,
      // kSerious value should be reported.
      0.85,
      // kFair value should be reported.
      0.70,
      // kNominal value should be reported.
      0.55};
  std::array<mojom::PressureState, samples_count> states;

  for (size_t i = 0; i < samples_count; i++) {
    states[i] = converter_.CalculateState(samples[i]);
  }

  EXPECT_THAT(states,
              ::testing::ElementsAre(
                  mojom::PressureState{mojom::PressureState::kCritical},
                  mojom::PressureState{mojom::PressureState::kSerious},
                  mojom::PressureState{mojom::PressureState::kFair},
                  mojom::PressureState{mojom::PressureState::kNominal}));
}

TEST_F(CpuPressureConverterTest, CheckCalculateStateHysteresisDownByDelta) {
  const size_t samples_count = 4;
  std::array<double, samples_count> samples = {
      // kCritical value should be reported.
      0.95,
      // kCritical value should be reported due to hysteresis.
      0.88,
      // kFair value should be reported.
      0.73,
      // kNominal value should be reported.
      0.56};

  std::array<mojom::PressureState, samples_count> states;
  for (size_t i = 0; i < samples_count; i++) {
    states[i] = converter_.CalculateState(samples[i]);
  }

  EXPECT_THAT(states,
              ::testing::ElementsAre(
                  mojom::PressureState{mojom::PressureState::kCritical},
                  mojom::PressureState{mojom::PressureState::kCritical},
                  mojom::PressureState{mojom::PressureState::kFair},
                  mojom::PressureState{mojom::PressureState::kNominal}));
}

TEST_F(CpuPressureConverterTest,
       CheckCalculateStateHysteresisDownByDeltaTwoStates) {
  const size_t samples_count = 3;
  std::array<double, samples_count> samples = {
      // kCritical value should be reported.
      0.95,
      // kFair should be reported.
      0.73,
      // kFair value should be reported due to hysteresis.
      0.58};

  std::array<mojom::PressureState, samples_count> states;
  for (size_t i = 0; i < samples_count; i++) {
    states[i] = converter_.CalculateState(samples[i]);
  }

  EXPECT_THAT(states, ::testing::ElementsAre(
                          mojom::PressureState{mojom::PressureState::kCritical},
                          mojom::PressureState{mojom::PressureState::kFair},
                          mojom::PressureState{mojom::PressureState::kFair}));
}

TEST_F(CpuPressureConverterTest,
       CheckCalculateStateHysteresisUpByDeltaTwoStates) {
  const size_t samples_count = 4;
  std::array<double, samples_count> samples = {
      // kNominal value should be reported.
      0.6,
      // kFair should be reported.
      0.62,
      // kSerious value should be reported.
      0.77,
      // kCritical value should be reported.
      0.91};

  std::array<mojom::PressureState, samples_count> states;
  for (size_t i = 0; i < samples_count; i++) {
    states[i] = converter_.CalculateState(samples[i]);
  }

  EXPECT_THAT(states,
              ::testing::ElementsAre(
                  mojom::PressureState{mojom::PressureState::kNominal},
                  mojom::PressureState{mojom::PressureState::kFair},
                  mojom::PressureState{mojom::PressureState::kSerious},
                  mojom::PressureState{mojom::PressureState::kCritical}));
}

TEST_F(CpuPressureConverterTest, CheckBreakCalibrationMitigation) {
  converter_.EnableStateRandomizationMitigation();

  EXPECT_THAT(converter_.CalculateState(0.86),
              mojom::PressureState(mojom::PressureState::kSerious));

  // First transition: switch to randomized thresholds.
  task_environment_.FastForwardBy(converter_.GetRandomizationTimeForTesting());
  EXPECT_THAT(converter_.CalculateState(0.86),
              mojom::PressureState(mojom::PressureState::kCritical));
  // Second transition: switch back to base thresholds.
  task_environment_.FastForwardBy(converter_.GetRandomizationTimeForTesting());
  EXPECT_THAT(converter_.CalculateState(0.86),
              mojom::PressureState(mojom::PressureState::kSerious));
}

TEST_F(CpuPressureConverterTest,
       BreakCalibrationMitigationSharedAcrossInstances) {
  CpuPressureConverter converter2;

  converter_.EnableStateRandomizationMitigation();
  converter2.EnableStateRandomizationMitigation();

  // Utilization of 0.55 lies in the divergence band:
  // - Base thresholds: 0.55 <= 0.6 -> kNominal
  // - Randomized thresholds: 0.55 > 0.5 and <= 0.8 -> kFair

  // Initially, both converters are in base threshold state.
  EXPECT_EQ(converter_.CalculateState(0.55), mojom::PressureState::kNominal);
  EXPECT_EQ(converter2.CalculateState(0.55), mojom::PressureState::kNominal);

  // First transition: both converters synchronously switch to randomized
  // thresholds.
  task_environment_.FastForwardBy(converter_.GetRandomizationTimeForTesting());
  EXPECT_EQ(converter_.CalculateState(0.55), mojom::PressureState::kFair);
  EXPECT_EQ(converter2.CalculateState(0.55), mojom::PressureState::kFair);

  // Second transition: both converters synchronously switch back to base
  // thresholds.
  task_environment_.FastForwardBy(converter_.GetRandomizationTimeForTesting());
  EXPECT_EQ(converter_.CalculateState(0.55), mojom::PressureState::kNominal);
  EXPECT_EQ(converter2.CalculateState(0.55), mojom::PressureState::kNominal);
}

TEST_F(CpuPressureConverterTest,
       BreakCalibrationMitigationSharedStateLifecycle) {
  auto converter1 = std::make_unique<CpuPressureConverter>();
  auto converter2 = std::make_unique<CpuPressureConverter>();

  converter1->EnableStateRandomizationMitigation();
  converter2->EnableStateRandomizationMitigation();

  // Advance time partially.
  task_environment_.FastForwardBy(base::Seconds(30));

  // Destroy converter1. converter2 must remain valid and keep the shared timer
  // alive.
  converter1.reset();
  EXPECT_EQ(converter2->CalculateState(0.55), mojom::PressureState::kNominal);

  // Destroy converter2. All references are released; g_shared_state must be
  // cleaned up.
  converter2.reset();

  // A newly created converter starts cleanly with base thresholds and a new
  // timer.
  CpuPressureConverter converter3;
  converter3.EnableStateRandomizationMitigation();
  EXPECT_EQ(converter3.CalculateState(0.55), mojom::PressureState::kNominal);
  task_environment_.FastForwardBy(converter3.GetRandomizationTimeForTesting());
  EXPECT_EQ(converter3.CalculateState(0.55), mojom::PressureState::kFair);
}

}  // namespace device
