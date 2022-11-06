// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/attribution_utils.h"

#include <string>

#include "components/attribution_reporting/filters.h"
#include "content/browser/attribution_reporting/attribution_source_type.h"
#include "content/browser/attribution_reporting/attribution_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {
namespace {

using AttributionFilterData = ::attribution_reporting::FilterData;
using AttributionFilters = ::attribution_reporting::Filters;
using AttributionFilterValues = ::attribution_reporting::FilterValues;

TEST(AttributionUtilsTest, EmptyOrMissingAttributionFilters) {
  const auto empty_filter = AttributionFilterValues();

  const auto empty_filter_values = AttributionFilterValues({{"filter1", {}}});

  const auto one_filter = AttributionFilterValues({{"filter1", {"value1"}}});

  const struct {
    const char* description;
    AttributionFilterValues filter_data;
    AttributionFilterValues filters;
  } kTestCases[] = {
      {"No source filters, no trigger filters", empty_filter, empty_filter},
      {"No source filters, trigger filter without values", empty_filter,
       empty_filter_values},
      {"No source filters, trigger filter with value", empty_filter,
       one_filter},

      {"Source filter without values, no trigger filters", empty_filter_values,
       empty_filter},

      {"Source filter with value, no trigger filters", one_filter,
       empty_filter}};

  // Behavior should match for for negated and non-negated filters as it
  // requires a value on each side.
  for (const auto& test_case : kTestCases) {
    absl::optional<AttributionFilterData> filter_data =
        AttributionFilterData::Create(test_case.filter_data);
    ASSERT_TRUE(filter_data) << test_case.description;

    absl::optional<AttributionFilters> filters =
        AttributionFilters::Create(test_case.filters);
    ASSERT_TRUE(filters) << test_case.description;

    EXPECT_TRUE(AttributionFilterDataMatch(
        *filter_data, AttributionSourceType::kNavigation, *filters))
        << test_case.description;

    EXPECT_TRUE(AttributionFilterDataMatch(
        *filter_data, AttributionSourceType::kNavigation, *filters,
        /*negated=*/true))
        << test_case.description << " with negation";
  }
}

TEST(AttributionUtilsTest, AttributionFilterDataMatch) {
  const auto empty_filter_values = AttributionFilterValues({{"filter1", {}}});

  const auto one_filter = AttributionFilterValues({{"filter1", {"value1"}}});

  const auto one_filter_different =
      AttributionFilterValues({{"filter1", {"value2"}}});

  const auto two_filters = AttributionFilterValues(
      {{"filter1", {"value1"}}, {"filter2", {"value2"}}});

  const auto one_mismatched_filter = AttributionFilterValues(
      {{"filter1", {"value1"}}, {"filter2", {"value3"}}});

  const auto two_mismatched_filter = AttributionFilterValues(
      {{"filter1", {"value3"}}, {"filter2", {"value4"}}});

  const struct {
    const char* description;
    AttributionFilterValues filter_data;
    AttributionFilterValues filters;
    bool match_expected;
  } kTestCases[] = {
      {"Source filter without values, trigger filter with value",
       empty_filter_values, one_filter, false},
      {"Source filter without values, trigger filter without values",
       empty_filter_values, empty_filter_values, true},
      {"Source filter with value, trigger filter without values", one_filter,
       empty_filter_values, false},

      {"One filter with matching values", one_filter, one_filter, true},
      {"One filter with no matching values", one_filter, one_filter_different,
       false},

      {"Two filters with matching values", two_filters, two_filters, true},
      {"Two filters no matching values", one_mismatched_filter,
       two_mismatched_filter, false},

      {"One filter not present in source, other matches", one_filter,
       two_filters, true},
      {"One filter not present in trigger, other matches", two_filters,
       one_filter, true},

      {"Two filters one filter no match", two_filters, one_mismatched_filter,
       false},
  };

  for (const auto& test_case : kTestCases) {
    absl::optional<AttributionFilterData> filter_data =
        AttributionFilterData::Create(test_case.filter_data);
    ASSERT_TRUE(filter_data) << test_case.description;

    absl::optional<AttributionFilters> filters =
        AttributionFilters::Create(test_case.filters);
    ASSERT_TRUE(filters) << test_case.description;

    EXPECT_EQ(test_case.match_expected,
              AttributionFilterDataMatch(
                  *filter_data, AttributionSourceType::kNavigation, *filters))
        << test_case.description;
  }
}

TEST(AttributionUtilsTest, NegatedAttributionFilterDataMatch) {
  const auto empty_filter_values = AttributionFilterValues({{"filter1", {}}});

  const auto one_filter = AttributionFilterValues({{"filter1", {"value1"}}});

  const auto one_filter_different =
      AttributionFilterValues({{"filter1", {"value2"}}});

  const auto one_filter_one_different =
      AttributionFilterValues({{"filter1", {"value1", "value2"}}});

  const auto one_filter_multiple_different =
      AttributionFilterValues({{"filter1", {"value2", "value3"}}});

  const auto two_filters = AttributionFilterValues(
      {{"filter1", {"value1"}}, {"filter2", {"value2"}}});

  const auto one_mismatched_filter = AttributionFilterValues(
      {{"filter1", {"value1"}}, {"filter2", {"value3"}}});

  const auto two_mismatched_filter = AttributionFilterValues(
      {{"filter1", {"value3"}}, {"filter2", {"value4"}}});

  const struct {
    const char* description;
    AttributionFilterValues filter_data;
    AttributionFilterValues filters;
    bool match_expected;
  } kTestCases[] = {
      // True because there is not matching values within source.
      {"Source filter without values, trigger filter with value",
       empty_filter_values, one_filter, true},
      {"Source filter without values, trigger filter without values",
       empty_filter_values, empty_filter_values, false},
      {"Source filter with value, trigger filter without values", one_filter,
       empty_filter_values, true},

      {"One filter with matching values", one_filter, one_filter, false},
      {"One filter with non-matching value", one_filter, one_filter_different,
       true},
      {"One filter with one non-matching value", one_filter,
       one_filter_one_different, false},
      {"One filter with multiple non-matching values", one_filter,
       one_filter_multiple_different, true},

      {"Two filters with matching values", two_filters, two_filters, false},
      {"Two filters no matching values", one_mismatched_filter,
       two_mismatched_filter, true},

      {"One filter not present in source, other matches", one_filter,
       two_filters, false},
      {"One filter not present in trigger, other matches", two_filters,
       one_filter, false},

      {"Two filters one filter no match", two_filters, one_mismatched_filter,
       false},
  };

  for (const auto& test_case : kTestCases) {
    absl::optional<AttributionFilterData> filter_data =
        AttributionFilterData::Create(test_case.filter_data);
    ASSERT_TRUE(filter_data) << test_case.description;

    absl::optional<AttributionFilters> filters =
        AttributionFilters::Create(test_case.filters);
    ASSERT_TRUE(filters) << test_case.description;

    EXPECT_EQ(test_case.match_expected,
              AttributionFilterDataMatch(
                  *filter_data, AttributionSourceType::kNavigation, *filters,
                  /*negated=*/true))
        << test_case.description << " with negation";
  }
}

TEST(AttributionUtilsTest, AttributionFilterDataMatch_SourceType) {
  const struct {
    const char* description;
    AttributionSourceType source_type;
    AttributionFilters filters;
    bool negated;
    bool match_expected;
  } kTestCases[] = {
      {
          .description = "empty-filters",
          .source_type = AttributionSourceType::kNavigation,
          .filters = AttributionFilters(),
          .negated = false,
          .match_expected = true,
      },
      {
          .description = "empty-filters-negated",
          .source_type = AttributionSourceType::kNavigation,
          .filters = AttributionFilters(),
          .negated = true,
          .match_expected = true,
      },
      {
          .description = "empty-filter-values",
          .source_type = AttributionSourceType::kNavigation,
          .filters = *AttributionFilters::Create({
              {AttributionFilterData::kSourceTypeFilterKey, {}},
          }),
          .negated = false,
          .match_expected = false,
      },
      {
          .description = "empty-filter-values-negated",
          .source_type = AttributionSourceType::kNavigation,
          .filters = *AttributionFilters::Create({
              {AttributionFilterData::kSourceTypeFilterKey, {}},
          }),
          .negated = true,
          .match_expected = true,
      },
      {
          .description = "same-source-type",
          .source_type = AttributionSourceType::kNavigation,
          .filters = AttributionFiltersForSourceType(
              AttributionSourceType::kNavigation),
          .negated = false,
          .match_expected = true,
      },
      {
          .description = "same-source-type-negated",
          .source_type = AttributionSourceType::kNavigation,
          .filters = AttributionFiltersForSourceType(
              AttributionSourceType::kNavigation),
          .negated = true,
          .match_expected = false,
      },
      {
          .description = "other-source-type",
          .source_type = AttributionSourceType::kNavigation,
          .filters =
              AttributionFiltersForSourceType(AttributionSourceType::kEvent),
          .negated = false,
          .match_expected = false,
      },
      {
          .description = "other-source-type-negated",
          .source_type = AttributionSourceType::kNavigation,
          .filters =
              AttributionFiltersForSourceType(AttributionSourceType::kEvent),
          .negated = true,
          .match_expected = true,
      },
  };

  for (const auto& test_case : kTestCases) {
    EXPECT_EQ(test_case.match_expected,
              AttributionFilterDataMatch(AttributionFilterData(),
                                         test_case.source_type,
                                         test_case.filters, test_case.negated))
        << test_case.description;
  }
}

}  // namespace
}  // namespace content
