// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/storable_source.h"

#include <utility>

#include "base/time/time.h"
#include "components/attribution_reporting/aggregation_keys.h"
#include "components/attribution_reporting/filters.h"
#include "components/attribution_reporting/source_registration.h"
#include "content/browser/attribution_reporting/attribution_source_type.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {
namespace {

TEST(StorableSourceTest, ReportWindows) {
  const base::Time kSourceTime = base::Time::Now();

  const struct {
    const char* desc;
    absl::optional<base::TimeDelta> expiry;
    absl::optional<base::TimeDelta> event_report_window;
    absl::optional<base::TimeDelta> aggregatable_report_window;
    base::Time expected_expiry_time;
    base::Time expected_event_report_window_time;
    base::Time expected_aggregatable_report_window_time;
  } kTestCases[] = {
      {
          .desc = "none",
          .expected_expiry_time = kSourceTime + base::Days(30),
          .expected_event_report_window_time = kSourceTime + base::Days(30),
          .expected_aggregatable_report_window_time =
              kSourceTime + base::Days(30),
      },
      {
          .desc = "expiry",
          .expiry = base::Days(4),
          .expected_expiry_time = kSourceTime + base::Days(4),
          .expected_event_report_window_time = kSourceTime + base::Days(4),
          .expected_aggregatable_report_window_time =
              kSourceTime + base::Days(4),
      },
      {
          .desc = "event-report-window",
          .event_report_window = base::Days(4),
          .expected_expiry_time = kSourceTime + base::Days(30),
          .expected_event_report_window_time = kSourceTime + base::Days(4),
          .expected_aggregatable_report_window_time =
              kSourceTime + base::Days(30),
      },
      {
          .desc = "aggregatable-report-window",
          .aggregatable_report_window = base::Days(4),
          .expected_expiry_time = kSourceTime + base::Days(30),
          .expected_event_report_window_time = kSourceTime + base::Days(30),
          .expected_aggregatable_report_window_time =
              kSourceTime + base::Days(4),
      },
      {
          .desc = "all",
          .expiry = base::Days(5),
          .event_report_window = base::Days(7),
          .aggregatable_report_window = base::Days(9),
          .expected_expiry_time = kSourceTime + base::Days(5),
          .expected_event_report_window_time = kSourceTime + base::Days(7),
          .expected_aggregatable_report_window_time =
              kSourceTime + base::Days(9),
      },
  };

  for (const auto& test_case : kTestCases) {
    auto reg = attribution_reporting::SourceRegistration::Create(
        /*source_event_id=*/0,
        /*destination=*/url::Origin::Create(GURL("https://dest.test")),
        /*reporting_origin=*/url::Origin::Create(GURL("https://report.test")),
        test_case.expiry, test_case.event_report_window,
        test_case.aggregatable_report_window,
        /*priority=*/0, attribution_reporting::FilterData(),
        /*debug_key=*/absl::nullopt, attribution_reporting::AggregationKeys(),
        /*debug_reporting=*/false);
    ASSERT_TRUE(reg) << test_case.desc;

    StorableSource actual(std::move(*reg), kSourceTime,
                          url::Origin::Create(GURL("https://source.test")),
                          AttributionSourceType::kNavigation,
                          /*is_within_fenced_frame=*/false);

    EXPECT_EQ(actual.common_info().expiry_time(),
              test_case.expected_expiry_time)
        << test_case.desc;

    EXPECT_EQ(actual.common_info().event_report_window_time(),
              test_case.expected_event_report_window_time)
        << test_case.desc;

    EXPECT_EQ(actual.common_info().aggregatable_report_window_time(),
              test_case.expected_aggregatable_report_window_time)
        << test_case.desc;
  }
}

}  // namespace
}  // namespace content
