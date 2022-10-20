// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/attribution_debug_report.h"

#include "base/test/values_test_util.h"
#include "content/browser/attribution_reporting/attribution_storage.h"
#include "content/browser/attribution_reporting/attribution_test_utils.h"
#include "content/browser/attribution_reporting/storable_source.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

namespace content {
namespace {

TEST(AttributionDebugReportTest,
     SourceDestinationLimitError_ValidReportReturned) {
  absl::optional<AttributionDebugReport> report =
      AttributionDebugReport::Create(
          SourceBuilder().Build(),
          AttributionStorage::StoreSourceResult(
              StorableSource::Result::kInsufficientUniqueDestinationCapacity,
              /*min_fake_report_time=*/absl::nullopt,
              /*max_destinations_per_source_site_reporting_origin=*/3));
  ASSERT_TRUE(report);

  static constexpr char kExpectedJsonString[] = R"([{
    "body": {
      "attribution_destination": "https://conversion.test",
      "limit": 3,
      "source_event_id": "123",
      "source_site": "https://impression.test"
    },
    "type": "source-destination-limit"
  }])";
  EXPECT_EQ(report->ReportBody(), base::test::ParseJson(kExpectedJsonString));

  EXPECT_EQ(report->ReportURL(), GURL("https://report.test/.well-known/"
                                      "attribution-reporting/debug/verbose"));
}

TEST(AttributionDebugReportTest,
     SourceDestinationLimitErrorWithinFencedFrame_ValidReportReturned) {
  AttributionConfig config;
  config.max_destinations_per_source_site_reporting_origin = 3;

  absl::optional<AttributionDebugReport> report =
      AttributionDebugReport::Create(
          SourceBuilder().SetIsWithinFencedFrame(true).Build(),
          AttributionStorage::StoreSourceResult(
              StorableSource::Result::kInsufficientUniqueDestinationCapacity,
              /*min_fake_report_time=*/absl::nullopt,
              /*max_destinations_per_source_site_reporting_origin=*/3));
  ASSERT_TRUE(report);

  static constexpr char kExpectedJsonString[] = R"([{
    "body": {
      "attribution_destination": "https://conversion.test",
      "limit": 3,
      "source_event_id": "123"
    },
    "type": "source-destination-limit"
  }])";
  EXPECT_EQ(report->ReportBody(), base::test::ParseJson(kExpectedJsonString));

  EXPECT_EQ(report->ReportURL(), GURL("https://report.test/.well-known/"
                                      "attribution-reporting/debug/verbose"));
}

TEST(AttributionDebugReportTest, UnsupportedError_NoDebugReports) {
  static constexpr StorableSource::Result kResults[] = {
      StorableSource::Result::kSuccess,
      StorableSource::Result::kInternalError,
      StorableSource::Result::kInsufficientSourceCapacity,
      StorableSource::Result::kExcessiveReportingOrigins,
      StorableSource::Result::kProhibitedByBrowserPolicy,
  };

  for (auto result : kResults) {
    EXPECT_FALSE(AttributionDebugReport::Create(
        SourceBuilder().Build(),
        AttributionStorage::StoreSourceResult(result)));
  }
}

}  // namespace
}  // namespace content
