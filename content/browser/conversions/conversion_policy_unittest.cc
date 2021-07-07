// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/conversions/conversion_policy.h"

#include <memory>
#include <vector>

#include "base/time/time.h"
#include "content/browser/conversions/conversion_test_utils.h"
#include "content/browser/conversions/storable_impression.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

namespace {

const StorableImpression::SourceType kSourceTypes[] = {
    StorableImpression::SourceType::kNavigation,
    StorableImpression::SourceType::kEvent,
};

// Fake ConversionNoiseProvider that return un-noised conversion data.
class EmptyNoiseProvider : public ConversionPolicy::NoiseProvider {
 public:
  uint64_t GetNoisedConversionData(uint64_t conversion_data) const override {
    return conversion_data;
  }

  uint64_t GetNoisedEventSourceTriggerData(
      uint64_t event_source_trigger_data) const override {
    return event_source_trigger_data;
  }
};

// Mock ConversionNoiseProvider that always noises values by +1.
class IncrementingNoiseProvider : public ConversionPolicy::NoiseProvider {
 public:
  uint64_t GetNoisedConversionData(uint64_t conversion_data) const override {
    return conversion_data + 1;
  }

  uint64_t GetNoisedEventSourceTriggerData(
      uint64_t event_source_trigger_data) const override {
    return event_source_trigger_data + 1;
  }
};

}  // namespace

class ConversionPolicyTest : public testing::Test {
 public:
  ConversionPolicyTest() = default;
};

TEST_F(ConversionPolicyTest, HighEntropyConversionData_StrippedToLowerBits) {
  uint64_t conversion_data = 8LU;

  // The policy should strip the data to the lower 3 bits.
  EXPECT_EQ(0LU, ConversionPolicy::CreateForTesting(
                     std::make_unique<EmptyNoiseProvider>())
                     ->GetSanitizedConversionData(conversion_data));
}

TEST_F(ConversionPolicyTest, SanitizeHighEntropyImpressionData_Unchanged) {
  uint64_t impression_data = 256LU;

  // The policy should not alter the impression data, and return the base 10
  // representation.
  EXPECT_EQ(256LU,
            ConversionPolicy().GetSanitizedImpressionData(impression_data));
}

TEST_F(ConversionPolicyTest, ThreeBitConversionData_Unchanged) {
  std::unique_ptr<ConversionPolicy> policy = ConversionPolicy::CreateForTesting(
      std::make_unique<EmptyNoiseProvider>());
  for (uint64_t conversion_data = 0; conversion_data < 8; conversion_data++) {
    EXPECT_EQ(conversion_data,
              policy->GetSanitizedConversionData(conversion_data));
  }
}

TEST_F(ConversionPolicyTest, SanitizeConversionData_OutputHasNoise) {
  // The policy should include noise when sanitizing data.
  EXPECT_EQ(5LU, ConversionPolicy::CreateForTesting(
                     std::make_unique<IncrementingNoiseProvider>())
                     ->GetSanitizedConversionData(4UL));
}

// This test will fail flakily if noise is used.
TEST_F(ConversionPolicyTest, DebugMode_ConversionDataNotNoised) {
  uint64_t conversion_data = 0UL;
  for (int i = 0; i < 100; i++) {
    EXPECT_EQ(conversion_data,
              ConversionPolicy(/*debug_mode=*/true)
                  .GetSanitizedConversionData(conversion_data));
  }
}

TEST_F(ConversionPolicyTest,
       HighEntropyEventSourceTriggerData_StrippedToLowerBits) {
  uint64_t event_source_trigger_data = 4LU;

  // The policy should strip the data to the lower 1 bit.
  EXPECT_EQ(
      0LU,
      ConversionPolicy::CreateForTesting(std::make_unique<EmptyNoiseProvider>())
          ->GetSanitizedEventSourceTriggerData(event_source_trigger_data));
}

TEST_F(ConversionPolicyTest, OneBitEventSourceTriggerData_Unchanged) {
  std::unique_ptr<ConversionPolicy> policy = ConversionPolicy::CreateForTesting(
      std::make_unique<EmptyNoiseProvider>());
  for (uint64_t event_source_trigger_data = 0; event_source_trigger_data < 2;
       event_source_trigger_data++) {
    EXPECT_EQ(
        event_source_trigger_data,
        policy->GetSanitizedEventSourceTriggerData(event_source_trigger_data));
  }
}

TEST_F(ConversionPolicyTest, SanitizeEventSourceTriggerData_OutputHasNoise) {
  // The policy should include noise when sanitizing data.
  EXPECT_EQ(0LU, ConversionPolicy::CreateForTesting(
                     std::make_unique<IncrementingNoiseProvider>())
                     ->GetSanitizedEventSourceTriggerData(1UL));
}

// This test will fail flakily if noise is used.
TEST_F(ConversionPolicyTest, DebugMode_EventSourceTriggerDataNotNoised) {
  uint64_t event_source_trigger_data = 0UL;
  for (int i = 0; i < 100; i++) {
    EXPECT_EQ(
        event_source_trigger_data,
        ConversionPolicy(/*debug_mode=*/true)
            .GetSanitizedEventSourceTriggerData(event_source_trigger_data));
  }
}

TEST_F(ConversionPolicyTest, NoExpiryForImpression_DefaultUsed) {
  const base::Time impression_time = base::Time::Now();

  for (auto source_type : kSourceTypes) {
    EXPECT_EQ(
        impression_time + base::TimeDelta::FromDays(30),
        ConversionPolicy().GetExpiryTimeForImpression(
            /*declared_expiry=*/absl::nullopt, impression_time, source_type));
  }
}

TEST_F(ConversionPolicyTest, LargeImpressionExpirySpecified_ClampedTo30Days) {
  constexpr base::TimeDelta declared_expiry = base::TimeDelta::FromDays(60);
  const base::Time impression_time = base::Time::Now();

  for (auto source_type : kSourceTypes) {
    EXPECT_EQ(impression_time + base::TimeDelta::FromDays(30),
              ConversionPolicy().GetExpiryTimeForImpression(
                  declared_expiry, impression_time, source_type));
  }
}

TEST_F(ConversionPolicyTest, SmallImpressionExpirySpecified_ClampedTo1Day) {
  const struct {
    base::TimeDelta declared_expiry;
    base::TimeDelta want_expiry;
  } kTestCases[] = {
      {base::TimeDelta::FromDays(-1), base::TimeDelta::FromDays(1)},
      {base::TimeDelta::FromDays(0), base::TimeDelta::FromDays(1)},
      {base::TimeDelta::FromDays(1) - base::TimeDelta::FromMilliseconds(1),
       base::TimeDelta::FromDays(1)},
  };

  const base::Time impression_time = base::Time::Now();

  for (auto source_type : kSourceTypes) {
    for (const auto& test_case : kTestCases) {
      EXPECT_EQ(impression_time + test_case.want_expiry,
                ConversionPolicy().GetExpiryTimeForImpression(
                    test_case.declared_expiry, impression_time, source_type));
    }
  }
}

TEST_F(ConversionPolicyTest, NonWholeDayImpressionExpirySpecified_Rounded) {
  const struct {
    StorableImpression::SourceType source_type;
    base::TimeDelta declared_expiry;
    base::TimeDelta want_expiry;
  } kTestCases[] = {
      {StorableImpression::SourceType::kNavigation,
       base::TimeDelta::FromHours(36), base::TimeDelta::FromHours(36)},
      {StorableImpression::SourceType::kEvent, base::TimeDelta::FromHours(36),
       base::TimeDelta::FromDays(2)},

      {StorableImpression::SourceType::kNavigation,
       base::TimeDelta::FromDays(1) + base::TimeDelta::FromMilliseconds(1),
       base::TimeDelta::FromDays(1) + base::TimeDelta::FromMilliseconds(1)},
      {StorableImpression::SourceType::kEvent,
       base::TimeDelta::FromDays(1) + base::TimeDelta::FromMilliseconds(1),
       base::TimeDelta::FromDays(1)},
  };

  const base::Time impression_time = base::Time::Now();

  for (const auto& test_case : kTestCases) {
    EXPECT_EQ(
        impression_time + test_case.want_expiry,
        ConversionPolicy().GetExpiryTimeForImpression(
            test_case.declared_expiry, impression_time, test_case.source_type));
  }
}

TEST_F(ConversionPolicyTest, ImpressionExpirySpecified_ExpiryOverrideDefault) {
  constexpr base::TimeDelta declared_expiry = base::TimeDelta::FromDays(10);
  const base::Time impression_time = base::Time::Now();

  for (auto source_type : kSourceTypes) {
    EXPECT_EQ(impression_time + base::TimeDelta::FromDays(10),
              ConversionPolicy().GetExpiryTimeForImpression(
                  declared_expiry, impression_time, source_type));
  }
}

}  // namespace content
