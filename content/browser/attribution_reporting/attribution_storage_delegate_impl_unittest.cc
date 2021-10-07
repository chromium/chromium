// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/attribution_storage_delegate_impl.h"

#include "base/time/time.h"
#include "content/browser/attribution_reporting/attribution_report.h"
#include "content/browser/attribution_reporting/conversion_test_utils.h"
#include "content/browser/attribution_reporting/storable_source.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

namespace {

constexpr base::TimeDelta kDefaultExpiry = base::Days(30);

AttributionReport GetReport(base::Time impression_time,
                            base::Time conversion_time,
                            base::TimeDelta expiry = kDefaultExpiry,
                            StorableSource::SourceType source_type =
                                StorableSource::SourceType::kNavigation) {
  base::Time report_time = conversion_time;
  return AttributionReport(ImpressionBuilder(impression_time)
                               .SetExpiry(expiry)
                               .SetSourceType(source_type)
                               .Build(),
                           /*conversion_data=*/123, conversion_time,
                           report_time,
                           /*priority=*/0,
                           /*conversion_id=*/absl::nullopt);
}

}  // namespace

class AttributionStorageDelegateImplTest : public testing::Test {
 public:
  AttributionStorageDelegateImplTest() = default;
};

TEST_F(AttributionStorageDelegateImplTest,
       ImmediateConversion_FirstWindowUsed) {
  base::Time impression_time = base::Time::Now();
  const AttributionReport report =
      GetReport(impression_time, /*conversion_time=*/impression_time);
  EXPECT_EQ(impression_time + base::Days(2),
            AttributionStorageDelegateImpl().GetReportTime(
                report.impression, report.conversion_time));
}

TEST_F(AttributionStorageDelegateImplTest,
       ConversionImmediatelyBeforeWindow_NextWindowUsed) {
  base::Time impression_time = base::Time::Now();
  base::Time conversion_time =
      impression_time + base::Days(2) - base::Minutes(1);
  const AttributionReport report = GetReport(impression_time, conversion_time);
  EXPECT_EQ(impression_time + base::Days(7),
            AttributionStorageDelegateImpl().GetReportTime(
                report.impression, report.conversion_time));
}

TEST_F(AttributionStorageDelegateImplTest,
       ConversionBeforeWindowDelay_WindowUsed) {
  base::Time impression_time = base::Time::Now();

  // The deadline for a window is 1 hour before the window. Use a time just
  // before the deadline.
  base::Time conversion_time =
      impression_time + base::Days(2) - base::Minutes(61);
  const AttributionReport report = GetReport(impression_time, conversion_time);
  EXPECT_EQ(impression_time + base::Days(2),
            AttributionStorageDelegateImpl().GetReportTime(
                report.impression, report.conversion_time));
}

TEST_F(AttributionStorageDelegateImplTest,
       ImpressionExpiryBeforeTwoDayWindow_TwoDayWindowUsed) {
  base::Time impression_time = base::Time::Now();
  base::Time conversion_time = impression_time + base::Hours(1);

  // Set the impression to expire before the two day window.
  const AttributionReport report = GetReport(impression_time, conversion_time,
                                             /*expiry=*/base::Hours(2));
  EXPECT_EQ(impression_time + base::Days(2),
            AttributionStorageDelegateImpl().GetReportTime(
                report.impression, report.conversion_time));
}

TEST_F(AttributionStorageDelegateImplTest,
       ImpressionExpiryBeforeSevenDayWindow_ExpiryWindowUsed) {
  base::Time impression_time = base::Time::Now();
  base::Time conversion_time = impression_time + base::Days(3);

  // Set the impression to expire before the two day window.
  const AttributionReport report = GetReport(impression_time, conversion_time,
                                             /*expiry=*/base::Days(4));

  // The expiry window is reported one hour after expiry time.
  EXPECT_EQ(impression_time + base::Days(4) + base::Hours(1),
            AttributionStorageDelegateImpl().GetReportTime(
                report.impression, report.conversion_time));
}

TEST_F(AttributionStorageDelegateImplTest,
       ImpressionExpiryAfterSevenDayWindow_ExpiryWindowUsed) {
  base::Time impression_time = base::Time::Now();
  base::Time conversion_time = impression_time + base::Days(7);

  // Set the impression to expire before the two day window.
  const AttributionReport report = GetReport(impression_time, conversion_time,
                                             /*expiry=*/base::Days(9));

  // The expiry window is reported one hour after expiry time.
  EXPECT_EQ(impression_time + base::Days(9) + base::Hours(1),
            AttributionStorageDelegateImpl().GetReportTime(
                report.impression, report.conversion_time));
}

TEST_F(AttributionStorageDelegateImplTest,
       SourceTypeEvent_ExpiryLessThanTwoDays_TwoDaysUsed) {
  base::Time impression_time = base::Time::Now();
  base::Time conversion_time = impression_time + base::Days(3);
  const AttributionReport report =
      GetReport(impression_time, conversion_time,
                /*expiry=*/base::Days(1), StorableSource::SourceType::kEvent);
  EXPECT_EQ(impression_time + base::Days(2) + base::Hours(1),
            AttributionStorageDelegateImpl().GetReportTime(
                report.impression, report.conversion_time));
}

TEST_F(AttributionStorageDelegateImplTest,
       SourceTypeEvent_ExpiryGreaterThanTwoDays_ExpiryUsed) {
  base::Time impression_time = base::Time::Now();
  base::Time conversion_time = impression_time + base::Days(3);
  const AttributionReport report =
      GetReport(impression_time, conversion_time,
                /*expiry=*/base::Days(4), StorableSource::SourceType::kEvent);
  EXPECT_EQ(impression_time + base::Days(4) + base::Hours(1),
            AttributionStorageDelegateImpl().GetReportTime(
                report.impression, report.conversion_time));
}

}  // namespace content
