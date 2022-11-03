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

TEST(AttributionDebugReportTest, NoDebugReporting_NoReportReturned) {
  EXPECT_FALSE(AttributionDebugReport::Create(
      SourceBuilder().Build(),
      /*is_debug_cookie_set=*/false,
      AttributionStorage::StoreSourceResult(
          StorableSource::Result::kInsufficientUniqueDestinationCapacity,
          /*min_fake_report_time=*/absl::nullopt,
          /*max_destinations_per_source_site_reporting_origin=*/3)));
}

TEST(AttributionDebugReportTest,
     SourceDestinationLimitError_ValidReportReturned) {
  absl::optional<AttributionDebugReport> report =
      AttributionDebugReport::Create(
          SourceBuilder().SetDebugReporting(true).Build(),
          /*is_debug_cookie_set=*/false,
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
     SourceDestinationLimitErrorWithinFencedFrame_NoDebugReport) {
  AttributionConfig config;
  config.max_destinations_per_source_site_reporting_origin = 3;

  EXPECT_FALSE(AttributionDebugReport::Create(
      SourceBuilder().SetIsWithinFencedFrame(true).Build(),
      /*is_debug_cookie_set=*/false,
      AttributionStorage::StoreSourceResult(
          StorableSource::Result::kInsufficientUniqueDestinationCapacity,
          /*min_fake_report_time=*/absl::nullopt,
          /*max_destinations_per_source_site_reporting_origin=*/3)));
}

TEST(AttributionDebugReportTest, SourceDebugging) {
  const struct {
    StorableSource::Result result;
    absl::optional<int> max_destinations_per_source_site_reporting_origin;
    const char* expected_report_body_without_cookie;
    const char* expected_report_body_with_cookie;
  } kTestCases[] = {
      {StorableSource::Result::kSuccess,
       /*max_destinations_per_source_site_reporting_origin=*/absl::nullopt,
       /*expected_report_body_without_cookie=*/nullptr,
       /*expected_report_body_with_cookie=*/nullptr},
      {StorableSource::Result::kInternalError,
       /*max_destinations_per_source_site_reporting_origin=*/absl::nullopt,
       /*expected_report_body_without_cookie=*/nullptr,
       /*expected_report_body_with_cookie=*/nullptr},
      {StorableSource::Result::kInsufficientSourceCapacity,
       /*max_destinations_per_source_site_reporting_origin=*/absl::nullopt,
       /*expected_report_body_without_cookie=*/nullptr,
       /*expected_report_body_with_cookie=*/nullptr},
      {StorableSource::Result::kProhibitedByBrowserPolicy,
       /*max_destinations_per_source_site_reporting_origin=*/absl::nullopt,
       /*expected_report_body_without_cookie=*/nullptr,
       /*expected_report_body_with_cookie=*/nullptr},
      {StorableSource::Result::kInsufficientUniqueDestinationCapacity,
       /*max_destinations_per_source_site_reporting_origin=*/3,
       /*expected_report_body_without_cookie=*/
       R"json([{
         "body": {
           "attribution_destination": "https://conversion.test",
           "limit": 3,
           "source_event_id": "123",
           "source_site": "https://impression.test"
         },
         "type": "source-destination-limit"
       }])json",
       /*expected_report_body_with_cookie=*/
       R"json([{
         "body": {
           "attribution_destination": "https://conversion.test",
           "limit": 3,
           "source_event_id": "123",
           "source_site": "https://impression.test"
         },
         "type": "source-destination-limit"
       }])json"},
      {StorableSource::Result::kSuccessNoised,
       /*max_destinations_per_source_site_reporting_origin=*/absl::nullopt,
       /*expected_report_body_without_cookie=*/nullptr,
       /*expected_report_body_with_cookie=*/
       R"json([{
         "body": {
           "attribution_destination": "https://conversion.test",
           "source_event_id": "123",
           "source_site": "https://impression.test"
         },
         "type": "source-noised"
       }])json"},
  };

  for (bool is_debug_cookie_set : {false, true}) {
    for (const auto& test_case : kTestCases) {
      absl::optional<AttributionDebugReport> report =
          AttributionDebugReport::Create(
              SourceBuilder().SetDebugReporting(true).Build(),
              is_debug_cookie_set,
              AttributionStorage::StoreSourceResult(
                  test_case.result,
                  /*min_fake_report_time=*/absl::nullopt,
                  /*max_destinations_per_source_site_reporting_origin=*/
                  test_case.max_destinations_per_source_site_reporting_origin));
      const char* expected_report_body =
          is_debug_cookie_set ? test_case.expected_report_body_with_cookie
                              : test_case.expected_report_body_without_cookie;
      EXPECT_EQ(report.has_value(), expected_report_body != nullptr)
          << test_case.result << ", " << is_debug_cookie_set;
      if (expected_report_body) {
        EXPECT_EQ(report->ReportBody(),
                  base::test::ParseJson(expected_report_body))
            << test_case.result << ", " << is_debug_cookie_set;
      }
    }
  }
}

}  // namespace
}  // namespace content
