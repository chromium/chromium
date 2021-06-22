// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/conversions/conversion_storage_delegate_impl.h"

#include "base/time/time.h"
#include "content/browser/conversions/conversion_report.h"
#include "content/browser/conversions/conversion_test_utils.h"
#include "content/browser/conversions/storable_impression.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

namespace {

constexpr base::TimeDelta kDefaultExpiry = base::TimeDelta::FromDays(30);

ConversionReport GetReport(base::Time impression_time,
                           base::Time conversion_time,
                           base::TimeDelta expiry = kDefaultExpiry,
                           StorableImpression::SourceType source_type =
                               StorableImpression::SourceType::kNavigation) {
  base::Time report_time = conversion_time;
  return ConversionReport(ImpressionBuilder(impression_time)
                              .SetExpiry(expiry)
                              .SetSourceType(source_type)
                              .Build(),
                          /*conversion_data=*/123, conversion_time, report_time,
                          /*conversion_id=*/absl::nullopt);
}

}  // namespace

class ConversionStorageDelegateImplTest : public testing::Test {
 public:
  ConversionStorageDelegateImplTest() = default;
};

TEST_F(ConversionStorageDelegateImplTest, ImmediateConversion_FirstWindowUsed) {
  base::Time impression_time = base::Time::Now();
  const ConversionReport report =
      GetReport(impression_time, /*conversion_time=*/impression_time);
  EXPECT_EQ(impression_time + base::TimeDelta::FromDays(2),
            ConversionStorageDelegateImpl().GetReportTime(report));
}

TEST_F(ConversionStorageDelegateImplTest,
       ConversionImmediatelyBeforeWindow_NextWindowUsed) {
  base::Time impression_time = base::Time::Now();
  base::Time conversion_time = impression_time + base::TimeDelta::FromDays(2) -
                               base::TimeDelta::FromMinutes(1);
  const ConversionReport report = GetReport(impression_time, conversion_time);
  EXPECT_EQ(impression_time + base::TimeDelta::FromDays(7),
            ConversionStorageDelegateImpl().GetReportTime(report));
}

TEST_F(ConversionStorageDelegateImplTest,
       ConversionBeforeWindowDelay_WindowUsed) {
  base::Time impression_time = base::Time::Now();

  // The deadline for a window is 1 hour before the window. Use a time just
  // before the deadline.
  base::Time conversion_time = impression_time + base::TimeDelta::FromDays(2) -
                               base::TimeDelta::FromMinutes(61);
  const ConversionReport report = GetReport(impression_time, conversion_time);
  EXPECT_EQ(impression_time + base::TimeDelta::FromDays(2),
            ConversionStorageDelegateImpl().GetReportTime(report));
}

TEST_F(ConversionStorageDelegateImplTest,
       ImpressionExpiryBeforeTwoDayWindow_TwoDayWindowUsed) {
  base::Time impression_time = base::Time::Now();
  base::Time conversion_time = impression_time + base::TimeDelta::FromHours(1);

  // Set the impression to expire before the two day window.
  const ConversionReport report =
      GetReport(impression_time, conversion_time,
                /*expiry=*/base::TimeDelta::FromHours(2));
  EXPECT_EQ(impression_time + base::TimeDelta::FromDays(2),
            ConversionStorageDelegateImpl().GetReportTime(report));
}

TEST_F(ConversionStorageDelegateImplTest,
       ImpressionExpiryBeforeSevenDayWindow_ExpiryWindowUsed) {
  base::Time impression_time = base::Time::Now();
  base::Time conversion_time = impression_time + base::TimeDelta::FromDays(3);

  // Set the impression to expire before the two day window.
  const ConversionReport report =
      GetReport(impression_time, conversion_time,
                /*expiry=*/base::TimeDelta::FromDays(4));

  // The expiry window is reported one hour after expiry time.
  EXPECT_EQ(impression_time + base::TimeDelta::FromDays(4) +
                base::TimeDelta::FromHours(1),
            ConversionStorageDelegateImpl().GetReportTime(report));
}

TEST_F(ConversionStorageDelegateImplTest,
       ImpressionExpiryAfterSevenDayWindow_ExpiryWindowUsed) {
  base::Time impression_time = base::Time::Now();
  base::Time conversion_time = impression_time + base::TimeDelta::FromDays(7);

  // Set the impression to expire before the two day window.
  const ConversionReport report =
      GetReport(impression_time, conversion_time,
                /*expiry=*/base::TimeDelta::FromDays(9));

  // The expiry window is reported one hour after expiry time.
  EXPECT_EQ(impression_time + base::TimeDelta::FromDays(9) +
                base::TimeDelta::FromHours(1),
            ConversionStorageDelegateImpl().GetReportTime(report));
}

TEST_F(ConversionStorageDelegateImplTest,
       SourceTypeEvent_ExpiryLessThanTwoDays_TwoDaysUsed) {
  base::Time impression_time = base::Time::Now();
  base::Time conversion_time = impression_time + base::TimeDelta::FromDays(3);
  const ConversionReport report =
      GetReport(impression_time, conversion_time,
                /*expiry=*/base::TimeDelta::FromDays(1),
                StorableImpression::SourceType::kEvent);
  EXPECT_EQ(impression_time + base::TimeDelta::FromDays(2) +
                base::TimeDelta::FromHours(1),
            ConversionStorageDelegateImpl().GetReportTime(report));
}

TEST_F(ConversionStorageDelegateImplTest,
       SourceTypeEvent_ExpiryGreaterThanTwoDays_ExpiryUsed) {
  base::Time impression_time = base::Time::Now();
  base::Time conversion_time = impression_time + base::TimeDelta::FromDays(3);
  const ConversionReport report =
      GetReport(impression_time, conversion_time,
                /*expiry=*/base::TimeDelta::FromDays(4),
                StorableImpression::SourceType::kEvent);
  EXPECT_EQ(impression_time + base::TimeDelta::FromDays(4) +
                base::TimeDelta::FromHours(1),
            ConversionStorageDelegateImpl().GetReportTime(report));
}

}  // namespace content
