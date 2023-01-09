// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webcodecs/codec_pressure_gauge.h"

#include "base/test/bind.h"
#include "base/test/mock_callback.h"
#include "build/build_config.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/modules/webcodecs/reclaimable_codec.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/thread_state.h"

using testing::_;

namespace blink {

namespace {

constexpr size_t kTestPressureThreshold = 5;

using MockPressureChangeCallback =
    base::MockCallback<CodecPressureGauge::PressureThresholdChangedCallback>;

}  // namespace

class CodecPressureGaugeTest
    : public testing::TestWithParam<ReclaimableCodec::CodecType> {
 public:
  using RegistrationResult = CodecPressureGauge::RegistrationResult;

  CodecPressureGaugeTest() {
    PressureGauge().set_pressure_threshold_for_testing(kTestPressureThreshold);
  }

  ~CodecPressureGaugeTest() override = default;

  CodecPressureGauge& PressureGauge() {
    return CodecPressureGauge::GetInstance(GetParam());
  }
};

TEST_P(CodecPressureGaugeTest, DefaultState) {
  // Sanity check.
  EXPECT_EQ(0u, PressureGauge().global_pressure_for_testing());
  EXPECT_FALSE(PressureGauge().is_global_pressure_exceeded_for_testing());
}

TEST_P(CodecPressureGaugeTest, GaugeIsSharedForDecodersEncoders) {
  // Sanity check.
  bool gauge_is_shared =
      &CodecPressureGauge::GetInstance(ReclaimableCodec::CodecType::kDecoder) ==
      &CodecPressureGauge::GetInstance(ReclaimableCodec::CodecType::kEncoder);

#if BUILDFLAG(IS_WIN)
  EXPECT_FALSE(gauge_is_shared);
#else
  EXPECT_TRUE(gauge_is_shared);
#endif
}

TEST_P(CodecPressureGaugeTest, RegisterUnregisterCallbacks) {
  MockPressureChangeCallback callback;

  RegistrationResult result_a =
      PressureGauge().RegisterPressureCallback(callback.Get());
  RegistrationResult result_b =
      PressureGauge().RegisterPressureCallback(callback.Get());

  // Callbacks should have different IDs.
  EXPECT_NE(result_a.first, result_b.first);

  // We should not have exceeded global pressure.
  EXPECT_FALSE(result_a.second);
  EXPECT_FALSE(result_b.second);

  PressureGauge().UnregisterPressureCallback(result_a.first, 0);
  PressureGauge().UnregisterPressureCallback(result_b.first, 0);
}

TEST_P(CodecPressureGaugeTest, IncrementDecrement) {
  MockPressureChangeCallback callback;
  EXPECT_CALL(callback, Run(_)).Times(0);

  RegistrationResult result =
      PressureGauge().RegisterPressureCallback(callback.Get());

  for (size_t i = 1u; i <= kTestPressureThreshold; ++i) {
    PressureGauge().Increment();
    EXPECT_EQ(i, PressureGauge().global_pressure_for_testing());
  }

  for (size_t i = kTestPressureThreshold; i > 0; --i) {
    PressureGauge().Decrement();
    EXPECT_EQ(i - 1u, PressureGauge().global_pressure_for_testing());
  }

  // Test cleanup.
  PressureGauge().UnregisterPressureCallback(result.first, 0);
}

TEST_P(CodecPressureGaugeTest, UnregisterAllLeftoverPressure) {
  MockPressureChangeCallback callback;

  RegistrationResult result =
      PressureGauge().RegisterPressureCallback(callback.Get());

  // Increase pressure up to the threshold.
  for (size_t i = 0; i < kTestPressureThreshold; ++i)
    PressureGauge().Increment();

  EXPECT_EQ(kTestPressureThreshold,
            PressureGauge().global_pressure_for_testing());

  // Releasing all pressure should reset global pressure.
  PressureGauge().UnregisterPressureCallback(result.first,
                                             kTestPressureThreshold);

  EXPECT_EQ(0u, PressureGauge().global_pressure_for_testing());
}

TEST_P(CodecPressureGaugeTest, UnregisterPartialLeftoverPressure) {
  MockPressureChangeCallback callback;

  RegistrationResult result =
      PressureGauge().RegisterPressureCallback(callback.Get());

  RegistrationResult other_result =
      PressureGauge().RegisterPressureCallback(callback.Get());

  // Increase pressure up to the threshold.
  for (size_t i = 0; i < kTestPressureThreshold; ++i)
    PressureGauge().Increment();

  constexpr size_t kPartialPressure = 3;

  EXPECT_EQ(kTestPressureThreshold,
            PressureGauge().global_pressure_for_testing());

  // Releasing partial pressure should properly update global pressure.
  PressureGauge().UnregisterPressureCallback(result.first, kPartialPressure);

  EXPECT_EQ(kTestPressureThreshold - kPartialPressure,
            PressureGauge().global_pressure_for_testing());

  // Test cleanup
  PressureGauge().UnregisterPressureCallback(
      other_result.first, PressureGauge().global_pressure_for_testing());
}

TEST_P(CodecPressureGaugeTest, ExceedingThresholdRunsCallbacks) {
  MockPressureChangeCallback callback;
  MockPressureChangeCallback other_callback;
  EXPECT_CALL(callback, Run(true));
  EXPECT_CALL(other_callback, Run(true));

  RegistrationResult result =
      PressureGauge().RegisterPressureCallback(callback.Get());

  RegistrationResult other_result =
      PressureGauge().RegisterPressureCallback(other_callback.Get());

  for (size_t i = 0; i < kTestPressureThreshold; ++i)
    PressureGauge().Increment();

  // We should be at the limit, but not over it.
  EXPECT_FALSE(PressureGauge().is_global_pressure_exceeded_for_testing());

  // Pass over the threshold.
  PressureGauge().Increment();

  EXPECT_TRUE(PressureGauge().is_global_pressure_exceeded_for_testing());

  testing::Mock::VerifyAndClearExpectations(&callback);
  testing::Mock::VerifyAndClearExpectations(&other_callback);

  // Test cleanup
  PressureGauge().UnregisterPressureCallback(
      result.first, PressureGauge().global_pressure_for_testing());
  PressureGauge().UnregisterPressureCallback(other_result.first, 0);
}

TEST_P(CodecPressureGaugeTest, PassingUnderThresholdRunsCallbacks_Decrement) {
  MockPressureChangeCallback callback;
  MockPressureChangeCallback other_callback;
  EXPECT_CALL(other_callback, Run(false));

  RegistrationResult result =
      PressureGauge().RegisterPressureCallback(callback.Get());

  // Make sure we are above the threshold.
  for (size_t i = 0; i < kTestPressureThreshold + 1; ++i)
    PressureGauge().Increment();

  RegistrationResult other_result =
      PressureGauge().RegisterPressureCallback(other_callback.Get());

  // Make the results match the expected global threshold.
  EXPECT_TRUE(PressureGauge().is_global_pressure_exceeded_for_testing());
  EXPECT_TRUE(other_result.second);

  // Reset expectations.
  testing::Mock::VerifyAndClearExpectations(&callback);
  EXPECT_CALL(callback, Run(false));

  // Pass under the global threshold via a call to Decrement().
  PressureGauge().Decrement();

  EXPECT_FALSE(PressureGauge().is_global_pressure_exceeded_for_testing());

  testing::Mock::VerifyAndClearExpectations(&other_callback);

  // Test cleanup
  PressureGauge().UnregisterPressureCallback(
      result.first, PressureGauge().global_pressure_for_testing());
  PressureGauge().UnregisterPressureCallback(other_result.first, 0);
}

TEST_P(CodecPressureGaugeTest, PassingUnderThresholdRunsCallbacks_Unregister) {
  MockPressureChangeCallback callback;
  MockPressureChangeCallback other_callback;
  EXPECT_CALL(other_callback, Run(false));

  RegistrationResult result =
      PressureGauge().RegisterPressureCallback(callback.Get());

  // Make sure we are above the threshold.
  for (size_t i = 0; i < kTestPressureThreshold + 1; ++i)
    PressureGauge().Increment();

  RegistrationResult other_result =
      PressureGauge().RegisterPressureCallback(other_callback.Get());

  // Make the results match the expected global threshold.
  EXPECT_TRUE(PressureGauge().is_global_pressure_exceeded_for_testing());
  EXPECT_TRUE(other_result.second);

  // Pass under the global threshold unregistering.
  constexpr size_t kPartialPressure = 3;
  PressureGauge().UnregisterPressureCallback(result.first, kPartialPressure);

  EXPECT_FALSE(PressureGauge().is_global_pressure_exceeded_for_testing());

  testing::Mock::VerifyAndClearExpectations(&other_callback);

  // Test cleanup.
  PressureGauge().UnregisterPressureCallback(
      other_result.first, PressureGauge().global_pressure_for_testing());
}

TEST_P(CodecPressureGaugeTest, RepeatedlyCrossingThresholds) {
  size_t number_of_exceeds_calls = 0u;
  size_t number_of_receeds_calls = 0u;

  auto pressure_cb = [&](bool pressure_exceeded) {
    if (pressure_exceeded)
      ++number_of_exceeds_calls;
    else
      ++number_of_receeds_calls;
  };

  RegistrationResult result = PressureGauge().RegisterPressureCallback(
      base::BindLambdaForTesting(pressure_cb));

  // Make sure we at the threshold.
  for (size_t i = 0; i < kTestPressureThreshold; ++i)
    PressureGauge().Increment();

  EXPECT_EQ(0u, number_of_exceeds_calls);
  EXPECT_EQ(0u, number_of_receeds_calls);

  constexpr size_t kNumberOfCrossings = 3;

  // Go back and forth across the threshold.
  for (size_t i = 1; i <= kNumberOfCrossings; ++i) {
    PressureGauge().Increment();
    EXPECT_EQ(i, number_of_exceeds_calls);
    PressureGauge().Decrement();
    EXPECT_EQ(i, number_of_receeds_calls);
  }

  // Test cleanup.
  PressureGauge().UnregisterPressureCallback(
      result.first, PressureGauge().global_pressure_for_testing());
}

TEST_P(CodecPressureGaugeTest, ZeroThreshold) {
  constexpr size_t kZeroPressureThreshold = 0u;
  PressureGauge().set_pressure_threshold_for_testing(kZeroPressureThreshold);

  MockPressureChangeCallback callback;
  EXPECT_CALL(callback, Run(true));
  EXPECT_CALL(callback, Run(false));

  RegistrationResult result =
      PressureGauge().RegisterPressureCallback(callback.Get());

  PressureGauge().Increment();
  PressureGauge().Decrement();

  // Test cleanup.
  PressureGauge().UnregisterPressureCallback(result.first, 0);
  PressureGauge().set_pressure_threshold_for_testing(kTestPressureThreshold);
}

INSTANTIATE_TEST_SUITE_P(
    ,
    CodecPressureGaugeTest,
    testing::Values(ReclaimableCodec::CodecType::kDecoder,
                    ReclaimableCodec::CodecType::kEncoder));

}  // namespace blink
