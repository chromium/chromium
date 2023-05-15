// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/destination_throttler.h"

#include "base/strings/strcat.h"
#include "base/time/time.h"
#include "components/attribution_reporting/destination_set.h"
#include "content/public/test/browser_task_environment.h"
#include "net/base/schemeful_site.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {
namespace {

attribution_reporting::DestinationSet Destinations(
    std::initializer_list<std::string> hosts) {
  attribution_reporting::DestinationSet::Destinations dests;
  for (const std::string& host : hosts) {
    dests.insert(
        net::SchemefulSite::Deserialize(base::StrCat({"https://", host})));
  }
  return attribution_reporting::DestinationSet::Create(dests).value();
}

net::SchemefulSite Site(const std::string& host) {
  return net::SchemefulSite::Deserialize(base::StrCat({"https://", host}));
}

class DestinationThrottlerTest : public testing::Test {
 public:
  DestinationThrottlerTest() = default;
  ~DestinationThrottlerTest() override = default;

 protected:
  BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
};

using Result = DestinationThrottler::Result;

TEST_F(DestinationThrottlerTest, SourceLimits) {
  DestinationThrottler::Policy policy{
      .max_total = 2,
      .max_per_reporting_site = 100,  // irrelevant
      .rate_limit_window = base::Minutes(1)};
  DestinationThrottler throttler(policy);

  // First source site:
  EXPECT_EQ(Result::kAllowed, throttler.UpdateAndGetResult(
                                  Destinations({"foo1.test"}),
                                  Site("source1.test"), Site("report.test")));
  EXPECT_EQ(Result::kAllowed, throttler.UpdateAndGetResult(
                                  Destinations({"foo2.test"}),
                                  Site("source1.test"), Site("report.test")));
  EXPECT_EQ(
      Result::kHitGlobalLimit,
      throttler.UpdateAndGetResult(Destinations({"foo3.test"}),
                                   Site("source1.test"), Site("report.test")));

  // Second source site should be independent:
  EXPECT_EQ(Result::kAllowed, throttler.UpdateAndGetResult(
                                  Destinations({"foo1.test"}),
                                  Site("source2.test"), Site("report.test")));
  EXPECT_EQ(Result::kAllowed, throttler.UpdateAndGetResult(
                                  Destinations({"foo2.test"}),
                                  Site("source2.test"), Site("report.test")));
  EXPECT_EQ(
      Result::kHitGlobalLimit,
      throttler.UpdateAndGetResult(Destinations({"foo3.test"}),
                                   Site("source2.test"), Site("report.test")));
}

TEST_F(DestinationThrottlerTest, ReportingSitesLimits) {
  DestinationThrottler::Policy policy{.max_total = 100,  // irrelevant
                                      .max_per_reporting_site = 2,
                                      .rate_limit_window = base::Minutes(1)};
  DestinationThrottler throttler(policy);

  // First reporting site:
  EXPECT_EQ(Result::kAllowed, throttler.UpdateAndGetResult(
                                  Destinations({"foo1.test"}),
                                  Site("source.test"), Site("report1.test")));
  EXPECT_EQ(Result::kAllowed, throttler.UpdateAndGetResult(
                                  Destinations({"foo2.test"}),
                                  Site("source.test"), Site("report1.test")));
  EXPECT_EQ(
      Result::kHitReportingLimit,
      throttler.UpdateAndGetResult(Destinations({"foo3.test"}),
                                   Site("source.test"), Site("report1.test")));

  // Second reporting site should be independent:
  EXPECT_EQ(Result::kAllowed, throttler.UpdateAndGetResult(
                                  Destinations({"foo1.test"}),
                                  Site("source.test"), Site("report2.test")));
  EXPECT_EQ(Result::kAllowed, throttler.UpdateAndGetResult(
                                  Destinations({"foo2.test"}),
                                  Site("source.test"), Site("report2.test")));
  EXPECT_EQ(
      Result::kHitReportingLimit,
      throttler.UpdateAndGetResult(Destinations({"foo3.test"}),
                                   Site("source.test"), Site("report2.test")));
}

TEST_F(DestinationThrottlerTest, MultipleOverLimit) {
  DestinationThrottler::Policy policy{
      .max_total = 2,
      .max_per_reporting_site = 100,  // irrelevant
      .rate_limit_window = base::Minutes(1)};
  DestinationThrottler throttler(policy);

  EXPECT_EQ(Result::kHitGlobalLimit,
            throttler.UpdateAndGetResult(
                Destinations({"foo1.test", "foo2.test", "foo3.test"}),
                Site("source.test"), Site("report.test")));

  EXPECT_EQ(Result::kAllowed, throttler.UpdateAndGetResult(
                                  Destinations({"foo1.test"}),
                                  Site("source.test"), Site("report.test")));
  EXPECT_EQ(Result::kAllowed, throttler.UpdateAndGetResult(
                                  Destinations({"foo2.test"}),
                                  Site("source.test"), Site("report.test")));
  EXPECT_EQ(
      Result::kHitGlobalLimit,
      throttler.UpdateAndGetResult(Destinations({"foo3.test"}),
                                   Site("source.test"), Site("report.test")));
}

TEST_F(DestinationThrottlerTest, RollingWindow) {
  DestinationThrottler::Policy policy{
      .max_total = 2,
      .max_per_reporting_site = 100,  // irrelevant
      .rate_limit_window = base::Minutes(1)};
  DestinationThrottler throttler(policy);

  // Time 0s
  EXPECT_EQ(Result::kAllowed, throttler.UpdateAndGetResult(
                                  Destinations({"foo1.test"}),
                                  Site("source.test"), Site("report.test")));

  task_environment_.FastForwardBy(base::Seconds(30));

  // Time 30s
  EXPECT_EQ(Result::kAllowed, throttler.UpdateAndGetResult(
                                  Destinations({"foo2.test"}),
                                  Site("source.test"), Site("report.test")));

  task_environment_.FastForwardBy(base::Seconds(31));

  // Time 1:01. foo1.test should be evicted.
  EXPECT_EQ(Result::kAllowed, throttler.UpdateAndGetResult(
                                  Destinations({"foo3.test"}),
                                  Site("source.test"), Site("report.test")));
  EXPECT_EQ(
      Result::kHitGlobalLimit,
      throttler.UpdateAndGetResult(Destinations({"foo4.test"}),
                                   Site("source.test"), Site("report.test")));

  task_environment_.FastForwardBy(base::Seconds(30));

  // Time 1:31. foo2.test should be evicted.
  EXPECT_EQ(Result::kAllowed, throttler.UpdateAndGetResult(
                                  Destinations({"foo5.test"}),
                                  Site("source.test"), Site("report.test")));
  EXPECT_EQ(
      Result::kHitGlobalLimit,
      throttler.UpdateAndGetResult(Destinations({"foo6.test"}),
                                   Site("source.test"), Site("report.test")));
}

// Insert a new element once all previous elements are too old.
TEST_F(DestinationThrottlerTest, CleanUpOldEntries) {
  DestinationThrottler::Policy policy{.max_total = 100,  // irrelevant
                                      .max_per_reporting_site = 2,
                                      .rate_limit_window = base::Minutes(1)};
  DestinationThrottler throttler(policy);

  // Time 0s
  EXPECT_EQ(Result::kAllowed, throttler.UpdateAndGetResult(
                                  Destinations({"foo1.test"}),
                                  Site("source.test"), Site("report.test")));
  EXPECT_EQ(Result::kAllowed, throttler.UpdateAndGetResult(
                                  Destinations({"foo2.test"}),
                                  Site("source.test"), Site("report.test")));
  EXPECT_EQ(
      Result::kHitReportingLimit,
      throttler.UpdateAndGetResult(Destinations({"foo3.test"}),
                                   Site("source.test"), Site("report.test")));

  task_environment_.FastForwardBy(base::Seconds(61));
  EXPECT_EQ(Result::kAllowed, throttler.UpdateAndGetResult(
                                  Destinations({"foo4.test"}),
                                  Site("source.test"), Site("report.test")));
  EXPECT_EQ(Result::kAllowed, throttler.UpdateAndGetResult(
                                  Destinations({"foo5.test"}),
                                  Site("source.test"), Site("report.test")));
  EXPECT_EQ(
      Result::kHitReportingLimit,
      throttler.UpdateAndGetResult(Destinations({"foo6.test"}),
                                   Site("source.test"), Site("report.test")));
}

// Exercises EvictEntriesOlderThan with multiple deleted entries.
TEST_F(DestinationThrottlerTest, EvictMultiple) {
  DestinationThrottler::Policy policy{
      .max_total = 3,
      .max_per_reporting_site = 100,  // irrelevant
      .rate_limit_window = base::Minutes(1)};
  DestinationThrottler throttler(policy);

  EXPECT_EQ(Result::kAllowed, throttler.UpdateAndGetResult(
                                  Destinations({"foo1.test", "foo2.test"}),
                                  Site("source.test"), Site("report.test")));

  task_environment_.FastForwardBy(base::Seconds(31));

  EXPECT_EQ(Result::kAllowed, throttler.UpdateAndGetResult(
                                  Destinations({"foo3.test"}),
                                  Site("source.test"), Site("report.test")));

  task_environment_.FastForwardBy(base::Seconds(30));

  // Should evict the first two destinations.
  EXPECT_EQ(Result::kAllowed, throttler.UpdateAndGetResult(
                                  Destinations({"foo4.test", "foo5.test"}),
                                  Site("source.test"), Site("report.test")));
  EXPECT_EQ(
      Result::kHitGlobalLimit,
      throttler.UpdateAndGetResult(Destinations({"foo6.test"}),
                                   Site("source.test"), Site("report.test")));
}

TEST_F(DestinationThrottlerTest, HitBothLimits) {
  DestinationThrottler::Policy policy{.max_total = 1,
                                      .max_per_reporting_site = 1,
                                      .rate_limit_window = base::Minutes(1)};
  DestinationThrottler throttler(policy);

  EXPECT_EQ(Result::kAllowed, throttler.UpdateAndGetResult(
                                  Destinations({"foo1.test"}),
                                  Site("source.test"), Site("report.test")));
  EXPECT_EQ(Result::kAllowed, throttler.UpdateAndGetResult(
                                  Destinations({"foo1.test"}),
                                  Site("source.test"), Site("report.test")));
  EXPECT_EQ(
      Result::kHitBothLimits,
      throttler.UpdateAndGetResult(Destinations({"foo2.test"}),
                                   Site("source.test"), Site("report.test")));
}

}  // namespace
}  // namespace content
