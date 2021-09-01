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

class ConfigurableConversionPolicy : public ConversionPolicy {
 public:
  explicit ConfigurableConversionPolicy(bool should_noise)
      : should_noise_(should_noise) {}

 protected:
  bool ShouldNoiseConversionData() const override { return should_noise_; }

  uint64_t MakeNoisedConversionData(uint64_t max) const override { return 1; }

 private:
  bool should_noise_;
};

class ConversionPolicyTest : public testing::Test {
 public:
  ConversionPolicyTest() = default;
};

}  // namespace

TEST_F(ConversionPolicyTest, HighEntropyConversionData_StrippedToLowerBits) {
  std::unique_ptr<ConversionPolicy> policy =
      std::make_unique<ConfigurableConversionPolicy>(/*should_noise=*/false);

  EXPECT_EQ(0u, policy->GetSanitizedConversionData(
                    8, StorableImpression::SourceType::kNavigation));
  EXPECT_EQ(1u, policy->GetSanitizedConversionData(
                    9, StorableImpression::SourceType::kNavigation));

  EXPECT_EQ(0u, policy->GetSanitizedConversionData(
                    2, StorableImpression::SourceType::kEvent));
  EXPECT_EQ(1u, policy->GetSanitizedConversionData(
                    3, StorableImpression::SourceType::kEvent));
}

TEST_F(ConversionPolicyTest, SanitizeHighEntropyImpressionData_Unchanged) {
  uint64_t impression_data = 256LU;

  // The policy should not alter the impression data, and return the base 10
  // representation.
  EXPECT_EQ(256LU,
            ConversionPolicy().GetSanitizedImpressionData(impression_data));
}

TEST_F(ConversionPolicyTest, LowEntropyConversionData_Unchanged) {
  std::unique_ptr<ConversionPolicy> policy =
      std::make_unique<ConfigurableConversionPolicy>(/*should_noise=*/false);

  for (uint64_t conversion_data = 0; conversion_data < 8; conversion_data++) {
    EXPECT_EQ(
        conversion_data,
        policy->GetSanitizedConversionData(
            conversion_data, StorableImpression::SourceType::kNavigation));
  }
  for (uint64_t conversion_data = 0; conversion_data < 2; conversion_data++) {
    EXPECT_EQ(conversion_data,
              policy->GetSanitizedConversionData(
                  conversion_data, StorableImpression::SourceType::kEvent));
  }
}

TEST_F(ConversionPolicyTest, SanitizeConversionData_OutputHasNoise) {
  // The policy should include noise when sanitizing data.
  for (auto source_type : kSourceTypes) {
    EXPECT_EQ(1LU, ConfigurableConversionPolicy(/*should_noise=*/true)
                       .GetSanitizedConversionData(0UL, source_type));
  }
}

// This test will fail flakily if noise is used.
TEST_F(ConversionPolicyTest, DebugMode_ConversionDataNotNoised) {
  const uint64_t conversion_data = 0UL;
  for (auto source_type : kSourceTypes) {
    for (int i = 0; i < 100; i++) {
      EXPECT_EQ(conversion_data,
                ConversionPolicy(/*debug_mode=*/true)
                    .GetSanitizedConversionData(conversion_data, source_type));
    }
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
