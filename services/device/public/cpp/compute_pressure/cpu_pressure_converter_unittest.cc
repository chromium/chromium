// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/public/cpp/compute_pressure/cpu_pressure_converter.h"

#include "base/sequence_checker.h"
#include "base/test/gtest_util.h"
#include "base/test/task_environment.h"
#include "base/test/test_timeouts.h"
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

  // First toggling.
  task_environment_.FastForwardBy(converter_.GetRandomizationTimeForTesting());
  EXPECT_THAT(converter_.CalculateState(0.86),
              mojom::PressureState(mojom::PressureState::kCritical));
  // Second toggling.
  task_environment_.FastForwardBy(converter_.GetRandomizationTimeForTesting());
  EXPECT_THAT(converter_.CalculateState(0.86),
              mojom::PressureState(mojom::PressureState::kSerious));
}

}  // namespace device
