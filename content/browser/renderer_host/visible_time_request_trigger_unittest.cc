// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/visible_time_request_trigger.h"

#include <optional>
#include <tuple>
#include <utility>

#include "base/time/time.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/page/content_to_visible_time_request.h"

namespace content {

namespace {

using ::testing::Optional;

class VisibleTimeRequestTriggerTest : public ::testing::Test {
 protected:
  // Converts a base::TimeDelta into a base::TimeTicks value suitable for
  // storing in the `event_start_time` field of a
  // RecordContentToVisibleTimeRequest.
  static base::TimeTicks StartTimeFromDelta(base::TimeDelta delta) {
    return base::TimeTicks() + delta;
  }
};

TEST_F(VisibleTimeRequestTriggerTest, MergeEmpty) {
  EXPECT_EQ(blink::ConsumeAndMergeContentToVisibleTimeRequests(std::nullopt,
                                                               std::nullopt),
            std::nullopt);
  EXPECT_THAT(blink::ConsumeAndMergeContentToVisibleTimeRequests(
                  std::nullopt, blink::RecordContentToVisibleTimeRequest{}),
              Optional(blink::RecordContentToVisibleTimeRequest{}));
  EXPECT_THAT(blink::ConsumeAndMergeContentToVisibleTimeRequests(
                  blink::RecordContentToVisibleTimeRequest{}, std::nullopt),
              Optional(blink::RecordContentToVisibleTimeRequest{}));
  EXPECT_THAT(blink::ConsumeAndMergeContentToVisibleTimeRequests(
                  blink::RecordContentToVisibleTimeRequest{},
                  blink::RecordContentToVisibleTimeRequest{}),
              Optional(blink::RecordContentToVisibleTimeRequest{}));
}

TEST_F(VisibleTimeRequestTriggerTest, MergeStartTimes) {
  // Overflowing can make a clock wrap around to negatives.
  constexpr auto kNegative = base::TimeDelta::Min();
  constexpr auto kZero = base::TimeDelta();

  auto expect_merged_start_time = [](base::TimeDelta left_time,
                                     base::TimeDelta right_time,
                                     base::TimeDelta expected_time) {
    SCOPED_TRACE(::testing::Message() << "Merging requests with start times "
                                      << left_time << " and " << right_time);
    std::optional<blink::RecordContentToVisibleTimeRequest> merged_request =
        blink::ConsumeAndMergeContentToVisibleTimeRequests(
            blink::RecordContentToVisibleTimeRequest{
                .event_start_time = StartTimeFromDelta(left_time)},
            blink::RecordContentToVisibleTimeRequest{
                .event_start_time = StartTimeFromDelta(right_time)});
    ASSERT_TRUE(merged_request);
    EXPECT_EQ(merged_request->event_start_time,
              StartTimeFromDelta(expected_time));
  };

  // Comparisons with negative times.
  expect_merged_start_time(kNegative, kNegative, kNegative);
  expect_merged_start_time(kNegative, kZero, kNegative);
  expect_merged_start_time(kNegative, base::Seconds(10), kNegative);
  expect_merged_start_time(kNegative, base::TimeDelta::Max(), kNegative);

  // Comparisons with time 0.
  expect_merged_start_time(kZero, kNegative, kNegative);
  expect_merged_start_time(kZero, kZero, kZero);
  expect_merged_start_time(kZero, base::Seconds(10), kZero);
  expect_merged_start_time(kZero, base::TimeDelta::Max(), kZero);

  // Comparisons between a mid-range time and boundary conditions.
  expect_merged_start_time(base::Seconds(10), kNegative, kNegative);
  expect_merged_start_time(base::Seconds(10), kZero, kZero);
  expect_merged_start_time(base::Seconds(10), base::TimeDelta::Max(),
                           base::Seconds(10));

  // Comparisons between two mid-range times.
  expect_merged_start_time(base::Seconds(10), base::Seconds(10),
                           base::Seconds(10));
  expect_merged_start_time(base::Seconds(10), base::Seconds(100),
                           base::Seconds(10));
  expect_merged_start_time(base::Seconds(100), base::Seconds(10),
                           base::Seconds(10));

  // Comparisons with the max time.
  expect_merged_start_time(base::TimeDelta::Max(), kNegative, kNegative);
  expect_merged_start_time(base::TimeDelta::Max(), kZero, kZero);
  expect_merged_start_time(base::TimeDelta::Max(), base::Seconds(10),
                           base::Seconds(10));
  expect_merged_start_time(base::TimeDelta::Max(), base::TimeDelta::Max(),
                           base::TimeDelta::Max());
}

TEST_F(VisibleTimeRequestTriggerTest, MergeRequests) {
  // Iterate over all possible combinations of request parameters. Tuple
  // contains `destination_is_loaded`, `show_reason_tab_switching`,
  // `show_reason_bfcache_restore`.
  using ParamTuple = std::tuple<bool, bool, bool>;
  constexpr ParamTuple kRequestParams[] = {
      // show_reason_tab_switching = true
      ParamTuple(false, true, false),
      ParamTuple(true, true, false),
      ParamTuple(false, true, true),
      ParamTuple(true, true, true),
      // show_reason_tab_switching = false
      ParamTuple(false, false, false),
      ParamTuple(false, false, true),
  };

  auto request_with_params = [](const ParamTuple& params) {
    return blink::RecordContentToVisibleTimeRequest{
        .event_start_time = StartTimeFromDelta(base::TimeDelta()),
        .destination_is_loaded = std::get<0>(params),
        .show_reason_tab_switching = std::get<1>(params),
        .show_reason_bfcache_restore = std::get<2>(params)};
  };

  auto request_with_union_of_params = [](const ParamTuple& params1,
                                         const ParamTuple& params2) {
    return blink::RecordContentToVisibleTimeRequest{
        .event_start_time = StartTimeFromDelta(base::TimeDelta()),
        .destination_is_loaded = std::get<0>(params1) || std::get<0>(params2),
        .show_reason_tab_switching =
            std::get<1>(params1) || std::get<1>(params2),
        .show_reason_bfcache_restore =
            std::get<2>(params1) || std::get<2>(params2)};
  };

  for (const ParamTuple& params : kRequestParams) {
    SCOPED_TRACE(::testing::Message()
                 << "With params " << std::get<0>(params) << ","
                 << std::get<1>(params) << "," << std::get<2>(params));

    {
      // Check that these fields are set in the result if they're set in either
      // or both of the requests.
      const auto expected = request_with_params(params);

      EXPECT_THAT(blink::ConsumeAndMergeContentToVisibleTimeRequests(
                      std::nullopt, request_with_params(params)),
                  Optional(expected));
      EXPECT_THAT(blink::ConsumeAndMergeContentToVisibleTimeRequests(
                      request_with_params(params), std::nullopt),
                  Optional(expected));
      EXPECT_THAT(blink::ConsumeAndMergeContentToVisibleTimeRequests(
                      request_with_params(params), request_with_params(params)),
                  Optional(expected));
    }

    // Check that when these fields are combined with another set of fields,
    // all fields are set in the result.
    for (const ParamTuple& params2 : kRequestParams) {
      SCOPED_TRACE(::testing::Message()
                   << "Combining with params " << std::get<0>(params2) << ","
                   << std::get<1>(params2) << "," << std::get<2>(params2));
      const auto expected = request_with_union_of_params(params, params2);
      EXPECT_THAT(
          blink::ConsumeAndMergeContentToVisibleTimeRequests(
              request_with_params(params), request_with_params(params2)),
          Optional(expected));
      EXPECT_THAT(
          blink::ConsumeAndMergeContentToVisibleTimeRequests(
              request_with_params(params2), request_with_params(params)),
          Optional(expected));
    }
  }
}

TEST_F(VisibleTimeRequestTriggerTest, UpdateAndTakeRequest) {
  VisibleTimeRequestTrigger trigger;
  EXPECT_EQ(trigger.TakeRequest(), std::nullopt);

  // Calling Update then Take should clear the stored request.
  {
    const auto expected = blink::RecordContentToVisibleTimeRequest{
        .event_start_time = StartTimeFromDelta(base::Seconds(1)),
        .destination_is_loaded = false,
        .show_reason_tab_switching = true};
    trigger.UpdateRequest(blink::RecordContentToVisibleTimeRequest{
        .event_start_time = StartTimeFromDelta(base::Seconds(1)),
        .destination_is_loaded = false,
        .show_reason_tab_switching = true});
    EXPECT_THAT(trigger.TakeRequest(), Optional(expected));
    EXPECT_EQ(trigger.TakeRequest(), std::nullopt);
  }

  // Calling Update multiple times should merge the requests.
  {
    const auto expected = blink::RecordContentToVisibleTimeRequest{
        .event_start_time = StartTimeFromDelta(base::Seconds(2)),
        .destination_is_loaded = true,
        .show_reason_tab_switching = true,
        .show_reason_bfcache_restore = true,
        .show_reason_unfolding = true};
    trigger.UpdateRequest(blink::RecordContentToVisibleTimeRequest{
        .event_start_time = StartTimeFromDelta(base::Seconds(3)),
        .destination_is_loaded = true,
        .show_reason_tab_switching = true});
    trigger.UpdateRequest(blink::RecordContentToVisibleTimeRequest{
        .event_start_time = StartTimeFromDelta(base::Seconds(2)),
        .destination_is_loaded = false,
        .show_reason_tab_switching = false,
        .show_reason_bfcache_restore = true});
    trigger.UpdateRequest(blink::RecordContentToVisibleTimeRequest{
        .event_start_time = StartTimeFromDelta(base::Seconds(4)),
        .destination_is_loaded = false,
        .show_reason_tab_switching = false,
        .show_reason_bfcache_restore = false,
        .show_reason_unfolding = true});
    EXPECT_THAT(trigger.TakeRequest(), Optional(expected));
    EXPECT_EQ(trigger.TakeRequest(), std::nullopt);
  }
}

}  // namespace

}  // namespace content
