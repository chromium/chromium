// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/storable_source.h"

#include <utility>

#include "base/time/time.h"
#include "components/attribution_reporting/destination_set.h"
#include "components/attribution_reporting/source_registration.h"
#include "components/attribution_reporting/source_type.mojom.h"
#include "components/attribution_reporting/suitable_origin.h"
#include "net/base/schemeful_site.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace content {
namespace {

using ::attribution_reporting::SuitableOrigin;

TEST(StorableSourceTest, ReportWindows) {
  const attribution_reporting::DestinationSet destinations =
      *attribution_reporting::DestinationSet::Create(
          {net::SchemefulSite::Deserialize("https://dest.test")});

  const auto reporting_origin =
      *SuitableOrigin::Deserialize("https://report.test");

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
          .desc = "clamp-event-report-window",
          .expiry = base::Days(4),
          .event_report_window = base::Days(30),
          .expected_expiry_time = kSourceTime + base::Days(4),
          .expected_event_report_window_time = kSourceTime + base::Days(4),
          .expected_aggregatable_report_window_time =
              kSourceTime + base::Days(4),
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
          .desc = "clamp-aggregatable-report-window",
          .expiry = base::Days(4),
          .aggregatable_report_window = base::Days(30),
          .expected_expiry_time = kSourceTime + base::Days(4),
          .expected_event_report_window_time = kSourceTime + base::Days(4),
          .expected_aggregatable_report_window_time =
              kSourceTime + base::Days(4),
      },
      {
          .desc = "all",
          .expiry = base::Days(9),
          .event_report_window = base::Days(7),
          .aggregatable_report_window = base::Days(5),
          .expected_expiry_time = kSourceTime + base::Days(9),
          .expected_event_report_window_time = kSourceTime + base::Days(7),
          .expected_aggregatable_report_window_time =
              kSourceTime + base::Days(5),
      },
  };

  for (const auto& test_case : kTestCases) {
    attribution_reporting::SourceRegistration reg(destinations);
    reg.expiry = test_case.expiry;
    reg.event_report_window = test_case.event_report_window;
    reg.aggregatable_report_window = test_case.aggregatable_report_window;

    StorableSource actual(reporting_origin, std::move(reg), kSourceTime,
                          *SuitableOrigin::Deserialize("https://source.test"),
                          attribution_reporting::mojom::SourceType::kNavigation,
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
