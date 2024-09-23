// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/private_aggregation/private_aggregation_budget_key.h"

#include <optional>

#include "base/time/time.h"
#include "content/browser/private_aggregation/private_aggregation_caller_api.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

namespace {

constexpr auto kExampleTime =
    base::Time::FromMillisecondsSinceUnixEpoch(1652984901234);

// `kExampleTime` floored to a minute boundary.
constexpr auto kExampleMinuteBoundary =
    base::Time::FromMillisecondsSinceUnixEpoch(1652984880000);

constexpr char kExampleOriginUrl[] = "https://origin.example";

}  // namespace

TEST(PrivateAggregationBudgetKeyTest, Fields_MatchInputs) {
  const url::Origin example_origin =
      url::Origin::Create(GURL(kExampleOriginUrl));

  std::optional<PrivateAggregationBudgetKey> protected_audience_key =
      PrivateAggregationBudgetKey::Create(
          example_origin, kExampleTime,
          PrivateAggregationCallerApi::kProtectedAudience);
  ASSERT_TRUE(protected_audience_key.has_value());
  EXPECT_EQ(protected_audience_key->origin(), example_origin);
  EXPECT_EQ(protected_audience_key->time_window().start_time(),
            kExampleMinuteBoundary);
  EXPECT_EQ(protected_audience_key->api(),
            PrivateAggregationCallerApi::kProtectedAudience);

  std::optional<PrivateAggregationBudgetKey> shared_storage_key =
      PrivateAggregationBudgetKey::Create(
          example_origin, kExampleTime,
          PrivateAggregationCallerApi::kSharedStorage);
  ASSERT_TRUE(shared_storage_key.has_value());
  EXPECT_EQ(shared_storage_key->origin(), example_origin);
  EXPECT_EQ(shared_storage_key->time_window().start_time(),
            kExampleMinuteBoundary);
  EXPECT_EQ(shared_storage_key->api(),
            PrivateAggregationCallerApi::kSharedStorage);
}

TEST(PrivateAggregationBudgetKeyTest, StartTimes_FlooredToTheMinute) {
  const url::Origin example_origin =
      url::Origin::Create(GURL(kExampleOriginUrl));

  std::optional<PrivateAggregationBudgetKey> example_key =
      PrivateAggregationBudgetKey::Create(
          example_origin, /*api_invocation_time=*/kExampleTime,
          PrivateAggregationCallerApi::kProtectedAudience);
  ASSERT_TRUE(example_key.has_value());
  EXPECT_EQ(example_key->time_window().start_time(), kExampleMinuteBoundary);

  std::optional<PrivateAggregationBudgetKey> on_the_minute =
      PrivateAggregationBudgetKey::Create(
          example_origin, /*api_invocation_time=*/kExampleMinuteBoundary,
          PrivateAggregationCallerApi::kProtectedAudience);
  ASSERT_TRUE(on_the_minute.has_value());
  EXPECT_EQ(on_the_minute->time_window().start_time(), kExampleMinuteBoundary);

  std::optional<PrivateAggregationBudgetKey> just_after_the_minute =
      PrivateAggregationBudgetKey::Create(
          example_origin,
          /*api_invocation_time=*/kExampleMinuteBoundary +
              base::Microseconds(1),
          PrivateAggregationCallerApi::kProtectedAudience);
  ASSERT_TRUE(just_after_the_minute.has_value());
  EXPECT_EQ(just_after_the_minute->time_window().start_time(),
            kExampleMinuteBoundary);

  std::optional<PrivateAggregationBudgetKey> just_before_the_minute =
      PrivateAggregationBudgetKey::Create(
          example_origin,
          /*api_invocation_time=*/kExampleMinuteBoundary -
              base::Microseconds(1),
          PrivateAggregationCallerApi::kProtectedAudience);
  ASSERT_TRUE(just_before_the_minute.has_value());
  EXPECT_EQ(just_before_the_minute->time_window().start_time(),
            kExampleMinuteBoundary - base::Minutes(1));
}

TEST(PrivateAggregationBudgetKeyTest, ExtremeStartTimes_HandledCorrectly) {
  // The earliest window should report an extreme start time as its 'conceptual'
  // start time can't be represented.
  EXPECT_EQ(
      PrivateAggregationBudgetKey::TimeWindow(base::Time::Min()).start_time(),
      base::Time::Min());
  EXPECT_EQ(PrivateAggregationBudgetKey::TimeWindow(base::Time::Min() +
                                                    base::Microseconds(1))
                .start_time(),
            base::Time::Min());

  // The second earliest window should report a start time 'on the minute'
  // again.
  PrivateAggregationBudgetKey::TimeWindow second_earliest_window(
      base::Time::Min() + PrivateAggregationBudgetKey::TimeWindow::kDuration);
  EXPECT_NE(second_earliest_window.start_time(), base::Time::Min());
  EXPECT_LE(
      second_earliest_window.start_time(),
      base::Time::Min() + PrivateAggregationBudgetKey::TimeWindow::kDuration);
  EXPECT_EQ(
      second_earliest_window.start_time().since_origin().InMicroseconds() %
          base::Time::kMicrosecondsPerMinute,
      0);

  // `base::Time::Max()` is disallowed, but otherwise the last window should
  // have no issue rounding down.
  PrivateAggregationBudgetKey::TimeWindow last_window(base::Time::Max() -
                                                      base::Microseconds(1));
  EXPECT_LT(last_window.start_time(), base::Time::Max());
  EXPECT_EQ(last_window.start_time().since_origin().InMicroseconds() %
                base::Time::kMicrosecondsPerMinute,
            0);
  EXPECT_LE(base::Time::Max() - last_window.start_time(),
            PrivateAggregationBudgetKey::TimeWindow::kDuration);
}

TEST(PrivateAggregationBudgetKeyTest, UntrustworthyOrigin_KeyCreationFailed) {
  std::optional<PrivateAggregationBudgetKey> opaque_origin_budget_key =
      PrivateAggregationBudgetKey::Create(
          url::Origin(), kExampleTime,
          PrivateAggregationCallerApi::kProtectedAudience);
  EXPECT_FALSE(opaque_origin_budget_key.has_value());

  std::optional<PrivateAggregationBudgetKey> insecure_origin_budget_key =
      PrivateAggregationBudgetKey::Create(
          url::Origin::Create(GURL("http://origin.example")), kExampleTime,
          PrivateAggregationCallerApi::kProtectedAudience);
  EXPECT_FALSE(insecure_origin_budget_key.has_value());
}

}  // namespace content
