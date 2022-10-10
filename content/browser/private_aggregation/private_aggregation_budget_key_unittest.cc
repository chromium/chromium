// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/private_aggregation/private_aggregation_budget_key.h"

#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

namespace {

// TODO(alexmt): Consider making FromJavaTime() constexpr.
const base::Time kExampleTime = base::Time::FromJavaTime(1652984901234);

// `kExampleTime` floored to an hour boundary.
const base::Time kExampleHourBoundary = base::Time::FromJavaTime(1652983200000);

constexpr char kExampleOriginUrl[] = "https://origin.example";

}  // namespace

TEST(PrivateAggregationBudgetKeyTest, Fields_MatchInputs) {
  const url::Origin example_origin =
      url::Origin::Create(GURL(kExampleOriginUrl));

  absl::optional<PrivateAggregationBudgetKey> fledge_key =
      PrivateAggregationBudgetKey::Create(
          example_origin, kExampleTime,
          PrivateAggregationBudgetKey::Api::kFledge);
  ASSERT_TRUE(fledge_key.has_value());
  EXPECT_EQ(fledge_key->origin(), example_origin);
  EXPECT_EQ(fledge_key->time_window().start_time(), kExampleHourBoundary);
  EXPECT_EQ(fledge_key->api(), PrivateAggregationBudgetKey::Api::kFledge);

  absl::optional<PrivateAggregationBudgetKey> shared_storage_key =
      PrivateAggregationBudgetKey::Create(
          example_origin, kExampleTime,
          PrivateAggregationBudgetKey::Api::kSharedStorage);
  ASSERT_TRUE(shared_storage_key.has_value());
  EXPECT_EQ(shared_storage_key->origin(), example_origin);
  EXPECT_EQ(shared_storage_key->time_window().start_time(),
            kExampleHourBoundary);
  EXPECT_EQ(shared_storage_key->api(),
            PrivateAggregationBudgetKey::Api::kSharedStorage);
}

TEST(PrivateAggregationBudgetKeyTest, StartTimes_FlooredToTheHour) {
  const url::Origin example_origin =
      url::Origin::Create(GURL(kExampleOriginUrl));

  absl::optional<PrivateAggregationBudgetKey> example_key =
      PrivateAggregationBudgetKey::Create(
          example_origin, /*api_invocation_time=*/kExampleTime,
          PrivateAggregationBudgetKey::Api::kFledge);
  ASSERT_TRUE(example_key.has_value());
  EXPECT_EQ(example_key->time_window().start_time(), kExampleHourBoundary);

  absl::optional<PrivateAggregationBudgetKey> on_the_hour =
      PrivateAggregationBudgetKey::Create(
          example_origin, /*api_invocation_time=*/kExampleHourBoundary,
          PrivateAggregationBudgetKey::Api::kFledge);
  ASSERT_TRUE(on_the_hour.has_value());
  EXPECT_EQ(on_the_hour->time_window().start_time(), kExampleHourBoundary);

  absl::optional<PrivateAggregationBudgetKey> just_after_the_hour =
      PrivateAggregationBudgetKey::Create(
          example_origin,
          /*api_invocation_time=*/kExampleHourBoundary + base::Microseconds(1),
          PrivateAggregationBudgetKey::Api::kFledge);
  ASSERT_TRUE(just_after_the_hour.has_value());
  EXPECT_EQ(just_after_the_hour->time_window().start_time(),
            kExampleHourBoundary);

  absl::optional<PrivateAggregationBudgetKey> just_before_the_hour =
      PrivateAggregationBudgetKey::Create(
          example_origin,
          /*api_invocation_time=*/kExampleHourBoundary - base::Microseconds(1),
          PrivateAggregationBudgetKey::Api::kFledge);
  ASSERT_TRUE(just_before_the_hour.has_value());
  EXPECT_EQ(just_before_the_hour->time_window().start_time(),
            kExampleHourBoundary - base::Hours(1));
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

  // The second earliest window should report a start time 'on the hour' again.
  PrivateAggregationBudgetKey::TimeWindow second_earliest_window(
      base::Time::Min() + PrivateAggregationBudgetKey::TimeWindow::kDuration);
  EXPECT_NE(second_earliest_window.start_time(), base::Time::Min());
  EXPECT_LE(
      second_earliest_window.start_time(),
      base::Time::Min() + PrivateAggregationBudgetKey::TimeWindow::kDuration);
  EXPECT_EQ(
      second_earliest_window.start_time().since_origin().InMicroseconds() %
          base::Time::kMicrosecondsPerHour,
      0);

  // `base::Time::Max()` is disallowed, but otherwise the last window should
  // have no issue rounding down.
  PrivateAggregationBudgetKey::TimeWindow last_window(base::Time::Max() -
                                                      base::Microseconds(1));
  EXPECT_LT(last_window.start_time(), base::Time::Max());
  EXPECT_EQ(last_window.start_time().since_origin().InMicroseconds() %
                base::Time::kMicrosecondsPerHour,
            0);
  EXPECT_LE(base::Time::Max() - last_window.start_time(),
            PrivateAggregationBudgetKey::TimeWindow::kDuration);
}

TEST(PrivateAggregationBudgetKeyTest, UntrustworthyOrigin_KeyCreationFailed) {
  absl::optional<PrivateAggregationBudgetKey> opaque_origin_budget_key =
      PrivateAggregationBudgetKey::Create(
          url::Origin(), base::Time::FromJavaTime(1652984901234),
          PrivateAggregationBudgetKey::Api::kFledge);
  EXPECT_FALSE(opaque_origin_budget_key.has_value());

  absl::optional<PrivateAggregationBudgetKey> insecure_origin_budget_key =
      PrivateAggregationBudgetKey::Create(
          url::Origin::Create(GURL("http://origin.example")),
          base::Time::FromJavaTime(1652984901234),
          PrivateAggregationBudgetKey::Api::kFledge);
  EXPECT_FALSE(insecure_origin_budget_key.has_value());
}

}  // namespace content
