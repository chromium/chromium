// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/attribution_filter_data.h"

#include <stddef.h>

#include <string>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/strings/string_number_conversions.h"
#include "components/attribution_reporting/constants.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace content {

namespace {

AttributionFilterValues CreateFilterValues(size_t n) {
  AttributionFilterValues filter_values;
  for (size_t i = 0; i < n; i++) {
    filter_values.emplace(base::NumberToString(i), std::vector<std::string>());
  }
  CHECK_EQ(filter_values.size(), n);
  return filter_values;
}

TEST(AttributionFilterDataTest, Create_ProhibitsSourceTypeFilter) {
  EXPECT_FALSE(AttributionFilterData::Create({{"source_type", {"event"}}}));
}

TEST(AttributionFilterDataTest,
     FromTriggerFilterValues_AllowsSourceTypeFilter) {
  EXPECT_TRUE(AttributionFilters::Create({{"source_type", {"event"}}}));
}

TEST(AttributionFilterDatTest, Create_LimitsFilterCount) {
  EXPECT_TRUE(
      AttributionFilterData::Create(
          CreateFilterValues(attribution_reporting::kMaxFiltersPerSource))
          .has_value());

  EXPECT_FALSE(
      AttributionFilterData::Create(
          CreateFilterValues(attribution_reporting::kMaxFiltersPerSource + 1))
          .has_value());
}

TEST(AttributionFilterDatTest, FromTriggerFilterValues_LimitsFilterCount) {
  EXPECT_TRUE(
      AttributionFilters::Create(
          CreateFilterValues(attribution_reporting::kMaxFiltersPerSource))
          .has_value());

  EXPECT_FALSE(
      AttributionFilters::Create(
          CreateFilterValues(attribution_reporting::kMaxFiltersPerSource + 1))
          .has_value());
}

}  // namespace
}  // namespace content
