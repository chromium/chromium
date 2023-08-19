// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/visible_time_request_trigger.h"

#include <tuple>
#include <utility>

#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/widget/record_content_to_visible_time_request.mojom.h"

namespace content {

namespace {

class VisibleTimeRequestTriggerTest : public testing::Test {
 protected:
  using RecordContentToVisibleTimeRequest =
      blink::mojom::RecordContentToVisibleTimeRequest;
  using RecordContentToVisibleTimeRequestPtr =
      blink::mojom::RecordContentToVisibleTimeRequestPtr;

  // Converts a base::TimeDelta into a base::TimeTicks value suitable for
  // storing in the `event_start_time` field of a
  // RecordContentToVisibleTimeRequest.
  static base::TimeTicks StartTimeFromDelta(base::TimeDelta delta) {
    return base::TimeTicks() + delta;
  }

  // Returns a request with the given `start_time`, which is given as a delta
  // from 0 since callers don't have an easy way to create base::TimeTicks
  // directly.
  static RecordContentToVisibleTimeRequestPtr CreateRequestPtr(
      base::TimeDelta start_time,
      bool destination_is_loaded = false,
      bool show_reason_tab_switching = false,
      bool show_reason_bfcache_restore = false,
      bool show_reason_unfolding = false) {
    return RecordContentToVisibleTimeRequest::New(
        StartTimeFromDelta(start_time), destination_is_loaded,
        show_reason_tab_switching, show_reason_bfcache_restore,
        show_reason_unfolding);
  }

  // Expects that all fields of `request` and `expected` match.
  static void ExpectEqualRequests(
      RecordContentToVisibleTimeRequestPtr request,
      const RecordContentToVisibleTimeRequest& expected) {
    ASSERT_TRUE(request);
    EXPECT_EQ(request->event_start_time, expected.event_start_time);
    EXPECT_EQ(request->destination_is_loaded, expected.destination_is_loaded);
    EXPECT_EQ(request->show_reason_tab_switching,
              expected.show_reason_tab_switching);
    EXPECT_EQ(request->show_reason_bfcache_restore,
              expected.show_reason_bfcache_restore);
  }
};

TEST_F(VisibleTimeRequestTriggerTest, MergeEmpty) {
  EXPECT_TRUE(
      VisibleTimeRequestTrigger::ConsumeAndMergeRequests(nullptr, nullptr)
          .is_null());
  ExpectEqualRequests(VisibleTimeRequestTrigger::ConsumeAndMergeRequests(
                          nullptr, RecordContentToVisibleTimeRequest::New()),
                      {});
  ExpectEqualRequests(VisibleTimeRequestTrigger::ConsumeAndMergeRequests(
                          RecordContentToVisibleTimeRequest::New(), nullptr),
                      {});
  ExpectEqualRequests(VisibleTimeRequestTrigger::ConsumeAndMergeRequests(
                          RecordContentToVisibleTimeRequest::New(),
                          RecordContentToVisibleTimeRequest::New()),
                      {});
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
    RecordContentToVisibleTimeRequestPtr merged_request =
        VisibleTimeRequestTrigger::ConsumeAndMergeRequests(
            CreateRequestPtr(left_time), CreateRequestPtr(right_time));
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
    return CreateRequestPtr(base::TimeDelta(), std::get<0>(params),
                            std::get<1>(params), std::get<2>(params));
  };

  auto request_with_union_of_params = [](const ParamTuple& params1,
                                         const ParamTuple& params2) {
    const bool destination_is_loaded =
        std::get<0>(params1) || std::get<0>(params2);
    const bool show_reason_tab_switching =
        std::get<1>(params1) || std::get<1>(params2);
    const bool show_reason_bfcache_restore =
        std::get<2>(params1) || std::get<2>(params2);
    return CreateRequestPtr(base::TimeDelta(), destination_is_loaded,
                            show_reason_tab_switching,
                            show_reason_bfcache_restore);
  };

  for (const ParamTuple& params : kRequestParams) {
    SCOPED_TRACE(::testing::Message()
                 << "With params " << std::get<0>(params) << ","
                 << std::get<1>(params) << "," << std::get<2>(params));

    {
      // Check that these fields are set in the result if they're set in either
      // or both of the requests.
      const auto expected = request_with_params(params);

      ExpectEqualRequests(VisibleTimeRequestTrigger::ConsumeAndMergeRequests(
                              RecordContentToVisibleTimeRequest::New(),
                              request_with_params(params)),
                          *expected);
      ExpectEqualRequests(VisibleTimeRequestTrigger::ConsumeAndMergeRequests(
                              request_with_params(params),
                              RecordContentToVisibleTimeRequest::New()),
                          *expected);
      ExpectEqualRequests(
          VisibleTimeRequestTrigger::ConsumeAndMergeRequests(
              request_with_params(params), request_with_params(params)),
          *expected);
    }

    // Check that when these fields are combined with another set of fields,
    // all fields are set in the result.
    for (const ParamTuple& params2 : kRequestParams) {
      SCOPED_TRACE(::testing::Message()
                   << "Combining with params " << std::get<0>(params2) << ","
                   << std::get<1>(params2) << "," << std::get<2>(params2));
      const auto expected = request_with_union_of_params(params, params2);
      ExpectEqualRequests(
          VisibleTimeRequestTrigger::ConsumeAndMergeRequests(
              request_with_params(params), request_with_params(params2)),
          *expected);
      ExpectEqualRequests(
          VisibleTimeRequestTrigger::ConsumeAndMergeRequests(
              request_with_params(params2), request_with_params(params)),
          *expected);
    }
  }
}

TEST_F(VisibleTimeRequestTriggerTest, UpdateAndTakeRequest) {
  VisibleTimeRequestTrigger trigger;
  EXPECT_TRUE(trigger.TakeRequest().is_null());

  // Calling Update then Take should clear the stored request.
  {
    const auto expected =
        CreateRequestPtr(base::Seconds(1), /*destination_is_loaded=*/false,
                         /*show_reason_tab_switching=*/true);
    trigger.UpdateRequest(StartTimeFromDelta(base::Seconds(1)),
                          /*destination_is_loaded=*/false,
                          /*show_reason_tab_switching=*/true,
                          /*show_reason_bfcache_restore=*/false);
    ExpectEqualRequests(trigger.TakeRequest(), *expected);
    EXPECT_TRUE(trigger.TakeRequest().is_null());
  }

  // Calling Update twice should merge the requests.
  {
    const auto expected =
        CreateRequestPtr(base::Seconds(2), /*destination_is_loaded=*/true,
                         /*show_reason_tab_switching=*/true,
                         /*show_reason_bfcache_restore=*/true);
    trigger.UpdateRequest(StartTimeFromDelta(base::Seconds(2)),
                          /*destination_is_loaded=*/true,
                          /*show_reason_tab_switching=*/true,
                          /*show_reason_bfcache_restore=*/false);
    trigger.UpdateRequest(StartTimeFromDelta(base::Seconds(3)),
                          /*destination_is_loaded=*/false,
                          /*show_reason_tab_switching=*/false,
                          /*show_reason_bfcache_restore=*/true);
    ExpectEqualRequests(trigger.TakeRequest(), *expected);
    EXPECT_TRUE(trigger.TakeRequest().is_null());
  }
}

}  // namespace

}  // namespace content
