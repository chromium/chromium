// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/attribution_insecure_random_generator.h"

#include <vector>

#include "content/browser/attribution_reporting/attribution_report.h"
#include "content/browser/attribution_reporting/attribution_test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/numeric/int128.h"

namespace content {
namespace {

using ::testing::ElementsAre;
using ::testing::Ge;
using ::testing::Le;
using ::testing::Lt;

// These tests use empirically determined golden data based on the seed below.
class AttributionInsecureRandomGeneratorTest : public ::testing::Test {
 protected:
  AttributionInsecureRandomGenerator gen_{absl::MakeUint128(0xabcd, 0x1234)};
};

TEST_F(AttributionInsecureRandomGeneratorTest, RandDouble) {
  const double kExpected[] = {
      0x1.4p-49,           0x1.234086p-29,      0x1.73f2b98p-26,
      0x1.23400246c8fcp-6, 0x1.579a02e7b3c3p-3,
  };

  for (double expected : kExpected) {
    const double actual = gen_.RandDouble();
    EXPECT_THAT(actual, Ge(0));
    EXPECT_THAT(actual, Lt(1));
    EXPECT_EQ(actual, expected);
  }
}

TEST_F(AttributionInsecureRandomGeneratorTest, RandInt) {
  const int kMin = 2;
  const int kMax = 11;

  const int kExpected[] = {8, 5, 2, 3, 10};

  for (int expected : kExpected) {
    const int actual = gen_.RandInt(kMin, kMax);
    EXPECT_THAT(actual, Ge(kMin));
    EXPECT_THAT(actual, Le(kMax));
    EXPECT_EQ(actual, expected);
  }
}

TEST_F(AttributionInsecureRandomGeneratorTest, RandomShuffle) {
  ReportBuilder builder(
      AttributionInfoBuilder(SourceBuilder().BuildStored()).Build());

  std::vector<AttributionReport> reports = {
      builder.SetTriggerData(1).Build(), builder.SetTriggerData(2).Build(),
      builder.SetTriggerData(3).Build(), builder.SetTriggerData(4).Build(),
      builder.SetTriggerData(5).Build(),
  };

  gen_.RandomShuffle(reports);

  EXPECT_THAT(reports, ElementsAre(EventLevelDataIs(TriggerDataIs(4)),
                                   EventLevelDataIs(TriggerDataIs(2)),
                                   EventLevelDataIs(TriggerDataIs(3)),
                                   EventLevelDataIs(TriggerDataIs(5)),
                                   EventLevelDataIs(TriggerDataIs(1))));
}

}  // namespace
}  // namespace content
