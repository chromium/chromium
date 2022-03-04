// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/attribution_utils.h"

#include <string>

#include "content/browser/attribution_reporting/attribution_filter_data.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

TEST(AttributionUtilsTest, AttributionFilterDataMatch) {
  auto filter_data_no_filters = AttributionFilterData();

  auto filter_data_no_filter_values =
      AttributionFilterData::FromFilterValues({{"filter1", {}}});
  ASSERT_TRUE(filter_data_no_filter_values.has_value());

  auto filter_data_with_filter_value =
      AttributionFilterData::FromFilterValues({{"filter1", {"value1"}}});
  ASSERT_TRUE(filter_data_with_filter_value.has_value());

  auto filter_data_two_filters = AttributionFilterData::FromFilterValues(
      {{"filter1", {"value1"}}, {"filter2", {"value2"}}});
  ASSERT_TRUE(filter_data_two_filters.has_value());

  auto filter_data_mismatched_filter = AttributionFilterData::FromFilterValues(
      {{"filter1", {"value1"}}, {"filter2", {"value3"}}});
  ASSERT_TRUE(filter_data_mismatched_filter.has_value());

  const struct {
    std::string description;
    AttributionFilterData source_filter_data;
    AttributionFilterData trigger_filter_data;
    bool match_expected;
  } kTestCases[] = {
      {"No source filters, no trigger filters", filter_data_no_filters,
       filter_data_no_filters, true},
      {"No source filters, trigger filter without values",
       filter_data_no_filters, *filter_data_no_filter_values, true},
      {"No source filters, trigger filter with value", filter_data_no_filters,
       *filter_data_with_filter_value, true},

      {"Source filter without values, no trigger filters",
       *filter_data_no_filter_values, filter_data_no_filters, true},
      {"Source filter without values, trigger filter without values",
       *filter_data_no_filter_values, *filter_data_no_filter_values, false},
      {"Source filter without values, trigger filter with value",
       *filter_data_no_filter_values, *filter_data_with_filter_value, false},

      {"Source filter with value, no trigger filters",
       *filter_data_with_filter_value, filter_data_no_filters, true},
      {"Source filter with value, trigger filter without values",
       *filter_data_with_filter_value, *filter_data_no_filter_values, false},
      {"Source filter with value, trigger filter with value",
       *filter_data_with_filter_value, *filter_data_with_filter_value, true},

      {"Two filters", *filter_data_two_filters, *filter_data_two_filters, true},
      {"One filter not present in source", *filter_data_with_filter_value,
       *filter_data_two_filters, true},
      {"One filter not present in trigger", *filter_data_two_filters,
       *filter_data_with_filter_value, true},
      {"One filter not match", *filter_data_two_filters,
       *filter_data_mismatched_filter, false},
  };

  for (const auto& test_case : kTestCases) {
    EXPECT_EQ(test_case.match_expected,
              AttributionFilterDataMatch(test_case.source_filter_data,
                                         test_case.trigger_filter_data))
        << test_case.description;
  }
}

}  // namespace content
