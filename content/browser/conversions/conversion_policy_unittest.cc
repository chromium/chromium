// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/conversions/conversion_policy.h"

#include <memory>
#include <vector>

#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "content/browser/conversions/conversion_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

namespace {

// Fake ConversionNoiseProvider that return un-noised conversion data.
class EmptyNoiseProvider : public ConversionPolicy::NoiseProvider {
 public:
  uint64_t GetNoisedConversionData(uint64_t conversion_data) const override {
    return conversion_data;
  }
};

// Mock ConversionNoiseProvider that always noises values by +1.
class IncrementingNoiseProvider : public ConversionPolicy::NoiseProvider {
 public:
  uint64_t GetNoisedConversionData(uint64_t conversion_data) const override {
    return conversion_data + 1;
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
  EXPECT_EQ("0", ConversionPolicy::CreateForTesting(
                     std::make_unique<EmptyNoiseProvider>())
                     ->GetSanitizedConversionData(conversion_data));
}

TEST_F(ConversionPolicyTest, SanitizeHighEntropyImpressionData_Unchanged) {
  uint64_t impression_data = 256LU;

  // The policy should not alter the impression data, and return the hexadecimal
  // representation.
  EXPECT_EQ("100",
            ConversionPolicy().GetSanitizedImpressionData(impression_data));
}

TEST_F(ConversionPolicyTest, ThreeBitConversionData_Unchanged) {
  std::unique_ptr<ConversionPolicy> policy = ConversionPolicy::CreateForTesting(
      std::make_unique<EmptyNoiseProvider>());
  for (uint64_t conversion_data = 0; conversion_data < 8; conversion_data++) {
    EXPECT_EQ(base::NumberToString(conversion_data),
              policy->GetSanitizedConversionData(conversion_data));
  }
}

TEST_F(ConversionPolicyTest, SantizizeConversionData_OutputHasNoise) {
  // The policy should include noise when sanitizing data.
  EXPECT_EQ("5", ConversionPolicy::CreateForTesting(
                     std::make_unique<IncrementingNoiseProvider>())
                     ->GetSanitizedConversionData(4UL));
}

// This test will fail flakily if noise is used.
TEST_F(ConversionPolicyTest, DebugMode_ConversionDataNotNoised) {
  uint64_t conversion_data = 0UL;
  for (int i = 0; i < 100; i++) {
    EXPECT_EQ(base::NumberToString(conversion_data),
              ConversionPolicy(true /* debug_mode */)
                  .GetSanitizedConversionData(conversion_data));
  }
}

TEST_F(ConversionPolicyTest, NoExpiryForImpression_DefaultUsed) {
  base::Time impression_time = base::Time::Now();
  EXPECT_EQ(impression_time + base::TimeDelta::FromDays(30),
            ConversionPolicy().GetExpiryTimeForImpression(
                /*declared_expiry=*/base::nullopt, impression_time));
}

TEST_F(ConversionPolicyTest, LargeImpressionExpirySpecified_ClampedTo30Days) {
  constexpr base::TimeDelta declared_expiry = base::TimeDelta::FromDays(60);
  base::Time impression_time = base::Time::Now();
  EXPECT_EQ(impression_time + base::TimeDelta::FromDays(30),
            ConversionPolicy().GetExpiryTimeForImpression(declared_expiry,
                                                          impression_time));
}

TEST_F(ConversionPolicyTest, ImpressionExpirySpecified_ExpiryOverrideDefault) {
  constexpr base::TimeDelta declared_expiry = base::TimeDelta::FromDays(10);
  base::Time impression_time = base::Time::Now();
  EXPECT_EQ(impression_time + base::TimeDelta::FromDays(10),
            ConversionPolicy().GetExpiryTimeForImpression(declared_expiry,
                                                          impression_time));
}

}  // namespace content
