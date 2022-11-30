// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/attribution_utils.h"

#include <string>

#include "content/browser/attribution_reporting/attribution_filter_data.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

TEST(AttributionUtilsTest, EmptyOrMissingAttributionFilters) {
  auto empty_filter = AttributionFilterData();

  auto empty_filter_values =
      AttributionFilterData::FromSourceFilterValues({{"filter1", {}}});
  ASSERT_TRUE(empty_filter_values.has_value());

  auto one_filter =
      AttributionFilterData::FromSourceFilterValues({{"filter1", {"value1"}}});
  ASSERT_TRUE(one_filter.has_value());

  const struct {
    std::string description;
    AttributionFilterData source_filter_data;
    AttributionFilterData trigger_filter_data;
    bool match_expected;
  } kTestCases[] = {{"No source filters, no trigger filters", empty_filter,
                     empty_filter, true},
                    {"No source filters, trigger filter without values",
                     empty_filter, *empty_filter_values, true},
                    {"No source filters, trigger filter with value",
                     empty_filter, *one_filter, true},

                    {"Source filter without values, no trigger filters",
                     *empty_filter_values, empty_filter, true},

                    {"Source filter with value, no trigger filters",
                     *one_filter, empty_filter, true}};

  // Behavior should match for for negated and non-negated filters as it
  // requires a value on each side.
  for (const auto& test_case : kTestCases) {
    EXPECT_EQ(test_case.match_expected,
              AttributionFilterDataMatch(test_case.source_filter_data,
                                         test_case.trigger_filter_data))
        << test_case.description;

    EXPECT_EQ(test_case.match_expected,
              AttributionFilterDataMatch(test_case.source_filter_data,
                                         test_case.trigger_filter_data,
                                         /*negated=*/true))
        << test_case.description << " with negation";
  }
}

TEST(AttributionUtilsTest, AttributionFilterDataMatch) {
  auto empty_filter_values =
      AttributionFilterData::FromSourceFilterValues({{"filter1", {}}});
  ASSERT_TRUE(empty_filter_values.has_value());

  auto one_filter =
      AttributionFilterData::FromSourceFilterValues({{"filter1", {"value1"}}});
  ASSERT_TRUE(one_filter.has_value());

  auto one_filter_different =
      AttributionFilterData::FromSourceFilterValues({{"filter1", {"value2"}}});
  ASSERT_TRUE(one_filter_different.has_value());

  auto two_filters = AttributionFilterData::FromSourceFilterValues(
      {{"filter1", {"value1"}}, {"filter2", {"value2"}}});
  ASSERT_TRUE(two_filters.has_value());

  auto one_mismatched_filter = AttributionFilterData::FromSourceFilterValues(
      {{"filter1", {"value1"}}, {"filter2", {"value3"}}});
  ASSERT_TRUE(one_mismatched_filter.has_value());

  auto two_mismatched_filter = AttributionFilterData::FromSourceFilterValues(
      {{"filter1", {"value3"}}, {"filter2", {"value4"}}});
  ASSERT_TRUE(two_mismatched_filter.has_value());

  const struct {
    std::string description;
    AttributionFilterData source_filter_data;
    AttributionFilterData trigger_filter_data;
    bool match_expected;
  } kTestCases[] = {
      {"Source filter without values, trigger filter with value",
       *empty_filter_values, *one_filter, false},
      {"Source filter without values, trigger filter without values",
       *empty_filter_values, *empty_filter_values, true},
      {"Source filter with value, trigger filter without values", *one_filter,
       *empty_filter_values, false},

      {"One filter with matching values", *one_filter, *one_filter, true},
      {"One filter with no matching values", *one_filter, *one_filter_different,
       false},

      {"Two filters with matching values", *two_filters, *two_filters, true},
      {"Two filters no matching values", *one_mismatched_filter,
       *two_mismatched_filter, false},

      {"One filter not present in source, other matches", *one_filter,
       *two_filters, true},
      {"One filter not present in trigger, other matches", *two_filters,
       *one_filter, true},

      {"Two filters one filter no match", *two_filters, *one_mismatched_filter,
       false},
  };

  for (const auto& test_case : kTestCases) {
    EXPECT_EQ(test_case.match_expected,
              AttributionFilterDataMatch(test_case.source_filter_data,
                                         test_case.trigger_filter_data))
        << test_case.description;
  }
}

TEST(AttributionUtilsTest, NegatedAttributionFilterDataMatch) {
  auto empty_filter_values =
      AttributionFilterData::FromSourceFilterValues({{"filter1", {}}});
  ASSERT_TRUE(empty_filter_values.has_value());

  auto one_filter =
      AttributionFilterData::FromSourceFilterValues({{"filter1", {"value1"}}});
  ASSERT_TRUE(one_filter.has_value());

  auto one_filter_different =
      AttributionFilterData::FromSourceFilterValues({{"filter1", {"value2"}}});
  ASSERT_TRUE(one_filter_different.has_value());

  auto one_filter_one_different = AttributionFilterData::FromSourceFilterValues(
      {{"filter1", {"value1", "value2"}}});
  ASSERT_TRUE(one_filter_different.has_value());

  auto one_filter_multiple_different =
      AttributionFilterData::FromSourceFilterValues(
          {{"filter1", {"value2", "value3"}}});
  ASSERT_TRUE(one_filter_different.has_value());

  auto two_filters = AttributionFilterData::FromSourceFilterValues(
      {{"filter1", {"value1"}}, {"filter2", {"value2"}}});
  ASSERT_TRUE(two_filters.has_value());

  auto one_mismatched_filter = AttributionFilterData::FromSourceFilterValues(
      {{"filter1", {"value1"}}, {"filter2", {"value3"}}});
  ASSERT_TRUE(one_mismatched_filter.has_value());

  auto two_mismatched_filter = AttributionFilterData::FromSourceFilterValues(
      {{"filter1", {"value3"}}, {"filter2", {"value4"}}});
  ASSERT_TRUE(two_mismatched_filter.has_value());

  const struct {
    std::string description;
    AttributionFilterData source_filter_data;
    AttributionFilterData trigger_filter_data;
    bool match_expected;
  } kTestCases[] = {
      // True because there is not matching values within source.
      {"Source filter without values, trigger filter with value",
       *empty_filter_values, *one_filter, true},
      {"Source filter without values, trigger filter without values",
       *empty_filter_values, *empty_filter_values, false},
      {"Source filter with value, trigger filter without values", *one_filter,
       *empty_filter_values, true},

      {"One filter with matching values", *one_filter, *one_filter, false},
      {"One filter with non-matching value", *one_filter, *one_filter_different,
       true},
      {"One filter with one non-matching value", *one_filter,
       *one_filter_one_different, false},
      {"One filter with multiple non-matching values", *one_filter,
       *one_filter_multiple_different, true},

      {"Two filters with matching values", *two_filters, *two_filters, false},
      {"Two filters no matching values", *one_mismatched_filter,
       *two_mismatched_filter, true},

      {"One filter not present in source, other matches", *one_filter,
       *two_filters, false},
      {"One filter not present in trigger, other matches", *two_filters,
       *one_filter, false},

      {"Two filters one filter no match", *two_filters, *one_mismatched_filter,
       false},
  };

  for (const auto& test_case : kTestCases) {
    EXPECT_EQ(test_case.match_expected,
              AttributionFilterDataMatch(test_case.source_filter_data,
                                         test_case.trigger_filter_data,
                                         /*negated=*/true))
        << test_case.description << " with negation";
  }
}

}  // namespace content
