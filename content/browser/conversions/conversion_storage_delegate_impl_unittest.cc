// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/conversions/conversion_storage_delegate_impl.h"

#include <vector>

#include "base/optional.h"
#include "base/time/time.h"
#include "content/browser/conversions/conversion_report.h"
#include "content/browser/conversions/conversion_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

namespace {

constexpr base::TimeDelta kDefaultExpiry = base::TimeDelta::FromDays(30);

ConversionReport GetReport(base::Time impression_time,
                           base::Time conversion_time,
                           base::TimeDelta expiry = kDefaultExpiry) {
  base::Time report_time = conversion_time;
  return ConversionReport(
      ImpressionBuilder(impression_time).SetExpiry(expiry).Build(),
      /*conversion_data=*/"123", conversion_time, report_time,
      /*conversion_id=*/base::nullopt);
}

}  // namespace

class ConversionStorageDelegateImplTest : public testing::Test {
 public:
  ConversionStorageDelegateImplTest() = default;
};

TEST_F(ConversionStorageDelegateImplTest, ImmediateConversion_FirstWindowUsed) {
  base::Time impression_time = base::Time::Now();
  std::vector<ConversionReport> reports = {
      GetReport(impression_time, /*conversion_time=*/impression_time)};
  ConversionStorageDelegateImpl().ProcessNewConversionReports(&reports);
  EXPECT_EQ(impression_time + base::TimeDelta::FromDays(2),
            reports[0].report_time);
}

TEST_F(ConversionStorageDelegateImplTest,
       ConversionImmediatelyBeforeWindow_NextWindowUsed) {
  base::Time impression_time = base::Time::Now();
  base::Time conversion_time = impression_time + base::TimeDelta::FromDays(2) -
                               base::TimeDelta::FromMinutes(1);
  std::vector<ConversionReport> reports = {
      GetReport(impression_time, conversion_time)};
  ConversionStorageDelegateImpl().ProcessNewConversionReports(&reports);
  EXPECT_EQ(impression_time + base::TimeDelta::FromDays(7),
            reports[0].report_time);
}

TEST_F(ConversionStorageDelegateImplTest,
       ConversionBeforeWindowDelay_WindowUsed) {
  base::Time impression_time = base::Time::Now();

  // The deadline for a window is 1 hour before the window. Use a time just
  // before the deadline.
  base::Time conversion_time = impression_time + base::TimeDelta::FromDays(2) -
                               base::TimeDelta::FromMinutes(61);
  std::vector<ConversionReport> reports = {
      GetReport(impression_time, conversion_time)};
  ConversionStorageDelegateImpl().ProcessNewConversionReports(&reports);
  EXPECT_EQ(impression_time + base::TimeDelta::FromDays(2),
            reports[0].report_time);
}

TEST_F(ConversionStorageDelegateImplTest,
       ImpressionExpiryBeforeTwoDayWindow_TwoDayWindowUsed) {
  base::Time impression_time = base::Time::Now();
  base::Time conversion_time = impression_time + base::TimeDelta::FromHours(1);

  // Set the impression to expire before the two day window.
  std::vector<ConversionReport> reports = {
      GetReport(impression_time, conversion_time,
                /*expiry=*/base::TimeDelta::FromHours(2))};
  ConversionStorageDelegateImpl().ProcessNewConversionReports(&reports);
  EXPECT_EQ(impression_time + base::TimeDelta::FromDays(2),
            reports[0].report_time);
}

TEST_F(ConversionStorageDelegateImplTest,
       ImpressionExpiryBeforeSevenDayWindow_ExpiryWindowUsed) {
  base::Time impression_time = base::Time::Now();
  base::Time conversion_time = impression_time + base::TimeDelta::FromDays(3);

  // Set the impression to expire before the two day window.
  std::vector<ConversionReport> reports = {
      GetReport(impression_time, conversion_time,
                /*expiry=*/base::TimeDelta::FromDays(4))};
  ConversionStorageDelegateImpl().ProcessNewConversionReports(&reports);

  // The expiry window is reported one hour after expiry time.
  EXPECT_EQ(impression_time + base::TimeDelta::FromDays(4) +
                base::TimeDelta::FromHours(1),
            reports[0].report_time);
}

TEST_F(ConversionStorageDelegateImplTest,
       ImpressionExpiryAfterSevenDayWindow_ExpiryWindowUsed) {
  base::Time impression_time = base::Time::Now();
  base::Time conversion_time = impression_time + base::TimeDelta::FromDays(7);

  // Set the impression to expire before the two day window.
  std::vector<ConversionReport> reports = {
      GetReport(impression_time, conversion_time,
                /*expiry=*/base::TimeDelta::FromDays(9))};
  ConversionStorageDelegateImpl().ProcessNewConversionReports(&reports);

  // The expiry window is reported one hour after expiry time.
  EXPECT_EQ(impression_time + base::TimeDelta::FromDays(9) +
                base::TimeDelta::FromHours(1),
            reports[0].report_time);
}

TEST_F(ConversionStorageDelegateImplTest,
       SingleReportForConversion_AttributionCreditAssigned) {
  base::Time now = base::Time::Now();
  std::vector<ConversionReport> reports = {
      GetReport(/*impression_time=*/now, /*conversion_time=*/now)};
  ConversionStorageDelegateImpl().ProcessNewConversionReports(&reports);
  EXPECT_EQ(1u, reports.size());
  EXPECT_EQ(100, reports[0].attribution_credit);
}

TEST_F(ConversionStorageDelegateImplTest,
       TwoReportsForConversion_LastReceivesCredit) {
  base::Time now = base::Time::Now();
  std::vector<ConversionReport> reports = {
      GetReport(/*impression_time=*/now, /*conversion_time=*/now),
      GetReport(/*impression_time=*/now + base::TimeDelta::FromHours(100),
                /*conversion_time=*/now)};
  ConversionStorageDelegateImpl().ProcessNewConversionReports(&reports);
  EXPECT_EQ(2u, reports.size());
  EXPECT_EQ(0, reports[0].attribution_credit);
  EXPECT_EQ(100, reports[1].attribution_credit);

  // Ensure the reports were not rearranged.
  EXPECT_EQ(now + base::TimeDelta::FromHours(100),
            reports[1].impression.impression_time());
}

}  // namespace content
