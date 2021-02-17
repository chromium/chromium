// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/conversions/conversion_storage.h"

#include <functional>
#include <list>
#include <memory>
#include <tuple>
#include <utility>
#include <vector>

#include "base/callback.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/test/simple_test_clock.h"
#include "content/browser/conversions/conversion_report.h"
#include "content/browser/conversions/conversion_storage_sql.h"
#include "content/browser/conversions/conversion_test_utils.h"
#include "content/browser/conversions/storable_conversion.h"
#include "content/browser/conversions/storable_impression.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

namespace {

// Default max number of conversions for a single impression for testing.
const int kMaxConversions = 3;

// Default delay in milliseconds for when a report should be sent for testing.
const int kReportTime = 5;

using AttributionCredits = std::list<int>;

base::RepeatingCallback<bool(const url::Origin&)> GetMatcher(
    const url::Origin& to_delete) {
  return base::BindRepeating(std::equal_to<url::Origin>(), to_delete);
}

}  // namespace

// Unit test suite for the ConversionStorage interface. All ConversionStorage
// implementations (including fakes) should be able to re-use this test suite.
class ConversionStorageTest : public testing::Test {
 public:
  ConversionStorageTest() {
    EXPECT_TRUE(dir_.CreateUniqueTempDir());
    auto delegate = std::make_unique<ConfigurableStorageDelegate>();
    delegate->set_report_time_ms(kReportTime);
    delegate->set_max_conversions_per_impression(kMaxConversions);
    delegate_ = delegate.get();
    storage_ = std::make_unique<ConversionStorageSql>(
        dir_.GetPath(), std::move(delegate), &clock_);
  }

  // Given a |conversion|, returns the expected conversion report properties at
  // the current timestamp.
  ConversionReport GetExpectedReport(const StorableImpression& impression,
                                     const StorableConversion& conversion,
                                     int attribution_credit = 0) {
    ConversionReport report(impression, conversion.conversion_data(),
                            /*conversion_time=*/clock_.Now(),
                            /*report_time=*/clock_.Now() +
                                base::TimeDelta::FromMilliseconds(kReportTime),
                            base::nullopt /* conversion_id */);
    report.attribution_credit = attribution_credit;
    return report;
  }

  void DeleteConversionReports(std::vector<ConversionReport> reports) {
    for (auto report : reports) {
      EXPECT_TRUE(storage_->DeleteConversion(*report.conversion_id));
    }
  }

  void AddAttributionCredits(AttributionCredits credits) {
    delegate_->AddCredits(credits);
  }

  base::SimpleTestClock* clock() { return &clock_; }

  ConversionStorage* storage() { return storage_.get(); }

  ConfigurableStorageDelegate* delegate() { return delegate_; }

 protected:
  base::ScopedTempDir dir_;

 private:
  ConfigurableStorageDelegate* delegate_;
  base::SimpleTestClock clock_;
  std::unique_ptr<ConversionStorage> storage_;
};

TEST_F(ConversionStorageTest,
       StorageUsedAfterFailedInitilization_FailsSilently) {
  // We create a failed initialization by writing a dir to the database file
  // path.
  base::CreateDirectoryAndGetError(
      dir_.GetPath().Append(FILE_PATH_LITERAL("Conversions")), nullptr);
  std::unique_ptr<ConversionStorage> storage =
      std::make_unique<ConversionStorageSql>(
          dir_.GetPath(), std::make_unique<ConfigurableStorageDelegate>(),
          clock());
  static_cast<ConversionStorageSql*>(storage.get())
      ->set_ignore_errors_for_testing(true);

  // Test all public methods on ConversionStorage.
  EXPECT_NO_FATAL_FAILURE(
      storage->StoreImpression(ImpressionBuilder(clock()->Now()).Build()));
  EXPECT_EQ(0,
            storage->MaybeCreateAndStoreConversionReports(DefaultConversion()));
  EXPECT_TRUE(storage->GetConversionsToReport(clock()->Now()).empty());
  EXPECT_TRUE(storage->GetActiveImpressions().empty());
  EXPECT_EQ(0, storage->DeleteExpiredImpressions());
  EXPECT_EQ(0, storage->DeleteConversion(0UL));
  EXPECT_NO_FATAL_FAILURE(storage->ClearData(
      base::Time::Min(), base::Time::Max(), base::NullCallback()));
}

TEST_F(ConversionStorageTest, ImpressionStoredAndRetrieved_ValuesIdentical) {
  auto impression = ImpressionBuilder(clock()->Now()).Build();
  storage()->StoreImpression(impression);
  std::vector<StorableImpression> stored_impressions =
      storage()->GetActiveImpressions();
  EXPECT_EQ(1u, stored_impressions.size());

  // Verify that each field was stored as expected.
  EXPECT_EQ(impression.impression_data(),
            stored_impressions[0].impression_data());
  EXPECT_EQ(impression.impression_origin(),
            stored_impressions[0].impression_origin());
  EXPECT_EQ(impression.conversion_origin(),
            stored_impressions[0].conversion_origin());
  EXPECT_EQ(impression.reporting_origin(),
            stored_impressions[0].reporting_origin());
  EXPECT_EQ(impression.impression_time(),
            stored_impressions[0].impression_time());
  EXPECT_EQ(impression.expiry_time(), stored_impressions[0].expiry_time());
}

TEST_F(ConversionStorageTest,
       GetWithNoMatchingImpressions_NoImpressionsReturned) {
  EXPECT_EQ(
      0, storage()->MaybeCreateAndStoreConversionReports(DefaultConversion()));
  EXPECT_TRUE(storage()->GetConversionsToReport(clock()->Now()).empty());
}

TEST_F(ConversionStorageTest, GetWithMatchingImpression_ImpressionReturned) {
  storage()->StoreImpression(ImpressionBuilder(clock()->Now()).Build());
  EXPECT_EQ(
      1, storage()->MaybeCreateAndStoreConversionReports(DefaultConversion()));
}

TEST_F(ConversionStorageTest, MultipleImpressionsForConversion_AllConvert) {
  storage()->StoreImpression(ImpressionBuilder(clock()->Now()).Build());
  storage()->StoreImpression(ImpressionBuilder(clock()->Now()).Build());
  EXPECT_EQ(
      2, storage()->MaybeCreateAndStoreConversionReports(DefaultConversion()));
}

TEST_F(ConversionStorageTest,
       CrossOriginSameDomainConversion_ImpressionConverted) {
  auto impression =
      ImpressionBuilder(clock()->Now())
          .SetConversionOrigin(url::Origin::Create(GURL("https://sub.a.test")))
          .Build();
  storage()->StoreImpression(impression);
  StorableConversion conversion("1", net::SchemefulSite(GURL("https://a.test")),
                                impression.reporting_origin());
  EXPECT_EQ(1, storage()->MaybeCreateAndStoreConversionReports(conversion));
}

TEST_F(ConversionStorageTest, ImpressionExpired_NoConversionsStored) {
  storage()->StoreImpression(
      ImpressionBuilder(clock()->Now())
          .SetExpiry(base::TimeDelta::FromMilliseconds(2))
          .Build());
  clock()->Advance(base::TimeDelta::FromMilliseconds(2));

  EXPECT_EQ(
      0, storage()->MaybeCreateAndStoreConversionReports(DefaultConversion()));
}

TEST_F(ConversionStorageTest, ImpressionExpired_ConversionsStoredPrior) {
  storage()->StoreImpression(
      ImpressionBuilder(clock()->Now())
          .SetExpiry(base::TimeDelta::FromMilliseconds(4))
          .Build());

  clock()->Advance(base::TimeDelta::FromMilliseconds(3));

  EXPECT_EQ(
      1, storage()->MaybeCreateAndStoreConversionReports(DefaultConversion()));

  clock()->Advance(base::TimeDelta::FromMilliseconds(5));

  EXPECT_EQ(
      0, storage()->MaybeCreateAndStoreConversionReports(DefaultConversion()));
}

TEST_F(ConversionStorageTest, ImpressionNotExpired_NotDeleted) {
  storage()->StoreImpression(
      ImpressionBuilder(clock()->Now())
          .SetExpiry(base::TimeDelta::FromMilliseconds(3))
          .Build());
  EXPECT_EQ(0, storage()->DeleteExpiredImpressions());
}

TEST_F(ConversionStorageTest, ImpressionExpired_Deleted) {
  storage()->StoreImpression(
      ImpressionBuilder(clock()->Now())
          .SetExpiry(base::TimeDelta::FromMilliseconds(3))
          .Build());
  clock()->Advance(base::TimeDelta::FromMilliseconds(3));
  EXPECT_EQ(1, storage()->DeleteExpiredImpressions());
}

TEST_F(ConversionStorageTest,
       ImpressionWithMaxConversions_ConversionReportNotStored) {
  storage()->StoreImpression(ImpressionBuilder(clock()->Now()).Build());

  for (int i = 0; i < kMaxConversions; i++) {
    EXPECT_EQ(1, storage()->MaybeCreateAndStoreConversionReports(
                     DefaultConversion()));
  }

  // No additional conversion reports should be created.
  EXPECT_EQ(
      0, storage()->MaybeCreateAndStoreConversionReports(DefaultConversion()));
}

TEST_F(ConversionStorageTest, OneConversion_OneReportScheduled) {
  auto impression = ImpressionBuilder(clock()->Now()).Build();
  auto conversion = DefaultConversion();

  storage()->StoreImpression(impression);
  EXPECT_EQ(1, storage()->MaybeCreateAndStoreConversionReports(conversion));

  ConversionReport expected_report = GetExpectedReport(impression, conversion);

  clock()->Advance(base::TimeDelta::FromMilliseconds(kReportTime));

  std::vector<ConversionReport> actual_reports =
      storage()->GetConversionsToReport(clock()->Now());
  EXPECT_TRUE(ReportsEqual({expected_report}, actual_reports));
}

TEST_F(ConversionStorageTest,
       ConversionWithDifferentReportingOrigin_NoReportScheduled) {
  auto impression = ImpressionBuilder(clock()->Now())
                        .SetReportingOrigin(
                            url::Origin::Create(GURL("https://different.test")))
                        .Build();
  storage()->StoreImpression(impression);
  EXPECT_EQ(
      0, storage()->MaybeCreateAndStoreConversionReports(DefaultConversion()));

  clock()->Advance(base::TimeDelta::FromMilliseconds(kReportTime));

  EXPECT_EQ(0u, storage()->GetConversionsToReport(clock()->Now()).size());
}

TEST_F(ConversionStorageTest,
       ConversionWithDifferentConversionOrigin_NoReportScheduled) {
  auto impression = ImpressionBuilder(clock()->Now())
                        .SetConversionOrigin(
                            url::Origin::Create(GURL("https://different.test")))
                        .Build();
  storage()->StoreImpression(impression);
  EXPECT_EQ(
      0, storage()->MaybeCreateAndStoreConversionReports(DefaultConversion()));

  clock()->Advance(base::TimeDelta::FromMilliseconds(kReportTime));

  EXPECT_EQ(0u, storage()->GetConversionsToReport(clock()->Now()).size());
}

TEST_F(ConversionStorageTest, OneConversion_AttributionCreditSet) {
  auto impression = ImpressionBuilder(clock()->Now()).Build();
  auto conversion = DefaultConversion();

  const int kAttributionCredit = 100;
  AddAttributionCredits({kAttributionCredit});

  storage()->StoreImpression(impression);
  EXPECT_EQ(1, storage()->MaybeCreateAndStoreConversionReports(conversion));

  ConversionReport expected_report =
      GetExpectedReport(impression, conversion, kAttributionCredit);
  clock()->Advance(base::TimeDelta::FromMilliseconds(kReportTime));

  std::vector<ConversionReport> actual_reports =
      storage()->GetConversionsToReport(clock()->Now());
  EXPECT_TRUE(ReportsEqual({expected_report}, actual_reports));
}

TEST_F(ConversionStorageTest,
       ExpiredImpressionWithPendingConversion_NotDeleted) {
  storage()->StoreImpression(
      ImpressionBuilder(clock()->Now())
          .SetExpiry(base::TimeDelta::FromMilliseconds(3))
          .Build());
  EXPECT_EQ(
      1, storage()->MaybeCreateAndStoreConversionReports(DefaultConversion()));

  clock()->Advance(base::TimeDelta::FromMilliseconds(3));
  EXPECT_EQ(0, storage()->DeleteExpiredImpressions());
}

TEST_F(ConversionStorageTest, TwoImpressionsOneExpired_OneDeleted) {
  storage()->StoreImpression(
      ImpressionBuilder(clock()->Now())
          .SetExpiry(base::TimeDelta::FromMilliseconds(3))
          .Build());
  storage()->StoreImpression(
      ImpressionBuilder(clock()->Now())
          .SetExpiry(base::TimeDelta::FromMilliseconds(4))
          .Build());

  clock()->Advance(base::TimeDelta::FromMilliseconds(3));
  EXPECT_EQ(
      1, storage()->MaybeCreateAndStoreConversionReports(DefaultConversion()));
  EXPECT_EQ(1, storage()->DeleteExpiredImpressions());
}

TEST_F(ConversionStorageTest, ExpiredImpressionWithSentConversion_Deleted) {
  storage()->StoreImpression(
      ImpressionBuilder(clock()->Now())
          .SetExpiry(base::TimeDelta::FromMilliseconds(3))
          .Build());
  EXPECT_EQ(
      1, storage()->MaybeCreateAndStoreConversionReports(DefaultConversion()));

  clock()->Advance(base::TimeDelta::FromMilliseconds(3));
  EXPECT_EQ(0, storage()->DeleteExpiredImpressions());

  // Advance past the default report time.
  clock()->Advance(base::TimeDelta::FromMilliseconds(kReportTime));
  EXPECT_EQ(0, storage()->DeleteExpiredImpressions());

  std::vector<ConversionReport> reports =
      storage()->GetConversionsToReport(clock()->Now());
  EXPECT_EQ(1u, reports.size());
  DeleteConversionReports(reports);

  EXPECT_EQ(1, storage()->DeleteExpiredImpressions());
}

TEST_F(ConversionStorageTest, ConversionReportDeleted_RemovedFromStorage) {
  storage()->StoreImpression(ImpressionBuilder(clock()->Now()).Build());
  EXPECT_EQ(
      1, storage()->MaybeCreateAndStoreConversionReports(DefaultConversion()));

  clock()->Advance(base::TimeDelta::FromMilliseconds(kReportTime));

  std::vector<ConversionReport> reports =
      storage()->GetConversionsToReport(clock()->Now());
  EXPECT_EQ(1u, reports.size());
  DeleteConversionReports(reports);

  EXPECT_TRUE(storage()->GetConversionsToReport(clock()->Now()).empty());
}

TEST_F(ConversionStorageTest,
       ManyImpressionsWithManyConversions_ConversionReportsCreated) {
  const int kNumMultiTouchImpressions = 20;

  // Store a large, arbitrary number of impressions.
  for (int i = 0; i < kNumMultiTouchImpressions; i++) {
    storage()->StoreImpression(ImpressionBuilder(clock()->Now()).Build());
  }

  for (int i = 0; i < kMaxConversions; i++) {
    EXPECT_EQ(
        kNumMultiTouchImpressions,
        storage()->MaybeCreateAndStoreConversionReports(DefaultConversion()));
  }

  // No additional conversion reports should be created for any of the
  // impressions.
  EXPECT_EQ(
      0, storage()->MaybeCreateAndStoreConversionReports(DefaultConversion()));
}

TEST_F(ConversionStorageTest,
       NewImpressionForUnconvertedImpression_ImpressionRemainsActive) {
  storage()->StoreImpression(ImpressionBuilder(clock()->Now()).Build());

  auto new_impression =
      ImpressionBuilder(clock()->Now())
          .SetImpressionOrigin(url::Origin::Create(GURL("https://other.test/")))
          .Build();
  storage()->StoreImpression(new_impression);

  // The first impression should be active because even though
  // <reporting_origin, conversion_origin> matches, it has not converted yet.
  EXPECT_EQ(
      2, storage()->MaybeCreateAndStoreConversionReports(DefaultConversion()));
}

// This test makes sure that when a new click is received for a given
// <reporting_origin, conversion_origin> pair, all existing impressions for that
// origin that have converted are marked ineligible for new conversions per the
// multi-touch model.
TEST_F(ConversionStorageTest,
       NewImpressionForConvertedImpression_MarkedInactive) {
  storage()->StoreImpression(
      ImpressionBuilder(clock()->Now()).SetData("0").Build());
  EXPECT_EQ(
      1, storage()->MaybeCreateAndStoreConversionReports(DefaultConversion()));

  clock()->Advance(base::TimeDelta::FromMilliseconds(kReportTime));

  // Delete the report.
  DeleteConversionReports(storage()->GetConversionsToReport(clock()->Now()));

  // Store a new impression that should mark the first inactive.
  auto new_impression =
      ImpressionBuilder(clock()->Now()).SetData("1000").Build();
  storage()->StoreImpression(new_impression);

  // Only the new impression should convert.
  auto conversion = DefaultConversion();
  EXPECT_EQ(1, storage()->MaybeCreateAndStoreConversionReports(conversion));
  ConversionReport expected_report =
      GetExpectedReport(new_impression, conversion);

  clock()->Advance(base::TimeDelta::FromMilliseconds(kReportTime));

  // Verify it was the new impression that converted.
  EXPECT_TRUE(ReportsEqual({expected_report},
                           storage()->GetConversionsToReport(clock()->Now())));
}

TEST_F(ConversionStorageTest,
       NonMatchingImpressionForConvertedImpression_FirstRemainsActive) {
  auto first_impression = ImpressionBuilder(clock()->Now()).Build();
  storage()->StoreImpression(first_impression);

  auto conversion = DefaultConversion();
  EXPECT_EQ(
      1, storage()->MaybeCreateAndStoreConversionReports(DefaultConversion()));

  // With the mock delegate, conversions are reported relative to impression
  // time not conversion time. This report will match both the first and second
  // conversion.
  ConversionReport expected_report =
      GetExpectedReport(first_impression, conversion);

  clock()->Advance(base::TimeDelta::FromMilliseconds(kReportTime));

  // Delete the report.
  DeleteConversionReports(storage()->GetConversionsToReport(clock()->Now()));

  // Store a new impression with a different reporting origin.
  auto new_impression = ImpressionBuilder(clock()->Now())
                            .SetReportingOrigin(url::Origin::Create(
                                GURL("https://different.test")))
                            .Build();
  storage()->StoreImpression(new_impression);

  // The first impression should still be active and able to convert.
  EXPECT_EQ(1, storage()->MaybeCreateAndStoreConversionReports(conversion));

  // Verify it was the first impression that converted.
  EXPECT_TRUE(ReportsEqual({expected_report},
                           storage()->GetConversionsToReport(clock()->Now())));
}

TEST_F(
    ConversionStorageTest,
    MultipleImpressionsForConversionAtDifferentTimes_AllImpressionsConverted) {
  auto first_impression = ImpressionBuilder(clock()->Now()).Build();
  storage()->StoreImpression(first_impression);

  auto second_impression = ImpressionBuilder(clock()->Now()).Build();
  storage()->StoreImpression(second_impression);

  auto conversion = DefaultConversion();
  ConversionReport first_expected_conversion =
      GetExpectedReport(first_impression, conversion);
  ConversionReport second_expected_conversion =
      GetExpectedReport(second_impression, conversion);

  // Advance clock so third impression is stored at a different timestamp.
  clock()->Advance(base::TimeDelta::FromMilliseconds(3));

  // Make a conversion with different impression data.
  auto third_impression =
      ImpressionBuilder(clock()->Now()).SetData("10").Build();
  storage()->StoreImpression(third_impression);

  ConversionReport third_expected_conversion =
      GetExpectedReport(third_impression, conversion);
  EXPECT_EQ(3, storage()->MaybeCreateAndStoreConversionReports(conversion));

  clock()->Advance(base::TimeDelta::FromMilliseconds(kReportTime));

  std::vector<ConversionReport> expected_reports = {first_expected_conversion,
                                                    second_expected_conversion,
                                                    third_expected_conversion};
  std::vector<ConversionReport> actual_reports =
      storage()->GetConversionsToReport(clock()->Now());

  EXPECT_TRUE(ReportsEqual(expected_reports, actual_reports));
}

TEST_F(ConversionStorageTest,
       ImpressionsAtDifferentTimes_ReportedAtDifferentTimes) {
  auto first_impression = ImpressionBuilder(clock()->Now()).Build();
  storage()->StoreImpression(first_impression);

  // Advance clock so the next impression is stored at a different timestamp.
  clock()->Advance(base::TimeDelta::FromMilliseconds(3));
  storage()->StoreImpression(ImpressionBuilder(clock()->Now()).Build());

  clock()->Advance(base::TimeDelta::FromMilliseconds(3));
  storage()->StoreImpression(ImpressionBuilder(clock()->Now()).Build());

  EXPECT_EQ(
      3, storage()->MaybeCreateAndStoreConversionReports(DefaultConversion()));

  // Advance to the first impression's report time and verify only its report is
  // available.
  clock()->Advance(base::TimeDelta::FromMilliseconds(kReportTime - 6));
  EXPECT_EQ(1u, storage()->GetConversionsToReport(clock()->Now()).size());

  clock()->Advance(base::TimeDelta::FromMilliseconds(3));
  EXPECT_EQ(2u, storage()->GetConversionsToReport(clock()->Now()).size());

  clock()->Advance(base::TimeDelta::FromMilliseconds(3));
  EXPECT_EQ(3u, storage()->GetConversionsToReport(clock()->Now()).size());
}

TEST_F(ConversionStorageTest, GetConversionsToReportMultipleTimes_SameResult) {
  storage()->StoreImpression(ImpressionBuilder(clock()->Now()).Build());
  EXPECT_EQ(
      1, storage()->MaybeCreateAndStoreConversionReports(DefaultConversion()));
  clock()->Advance(base::TimeDelta::FromMilliseconds(kReportTime));

  std::vector<ConversionReport> first_call_reports =
      storage()->GetConversionsToReport(clock()->Now());
  std::vector<ConversionReport> second_call_reports =
      storage()->GetConversionsToReport(clock()->Now());

  // Expect that |GetConversionsToReport| did not delete any conversions.
  EXPECT_EQ(1u, first_call_reports.size());
  EXPECT_EQ(1u, second_call_reports.size());
  EXPECT_TRUE(ReportsEqual(first_call_reports, second_call_reports));
}

TEST_F(ConversionStorageTest,
       ManyImpressionsWithAttributionCredits_CreditsAssignedCorrectly) {
  const int kNumImpressions = 10;
  std::vector<ConversionReport> expected_reports;
  AttributionCredits credits;
  auto conversion = DefaultConversion();

  // Store a large, arbitrary number of impressions.
  for (int i = 0; i < kNumImpressions; i++) {
    auto impression = ImpressionBuilder(clock()->Now())
                          .SetData(base::NumberToString(i))
                          .Build();
    storage()->StoreImpression(impression);
    expected_reports.push_back(GetExpectedReport(impression, conversion, i));
    credits.push_back(i);
  }

  // Add the expected credits to the delegate.
  AddAttributionCredits(credits);
  EXPECT_EQ(kNumImpressions,
            storage()->MaybeCreateAndStoreConversionReports(conversion));

  // Verify that the attribution credits were associated with scheduled
  // conversions as expected.
  clock()->Advance(base::TimeDelta::FromMilliseconds(kReportTime));
  std::vector<ConversionReport> actual_reports =
      storage()->GetConversionsToReport(clock()->Now());
  EXPECT_TRUE(ReportsEqual(expected_reports, actual_reports));
}

TEST_F(ConversionStorageTest, MaxImpressionsPerOrigin) {
  delegate()->set_max_impressions_per_origin(2);
  storage()->StoreImpression(ImpressionBuilder(clock()->Now()).Build());
  storage()->StoreImpression(ImpressionBuilder(clock()->Now()).Build());
  storage()->StoreImpression(ImpressionBuilder(clock()->Now()).Build());
  EXPECT_EQ(
      2, storage()->MaybeCreateAndStoreConversionReports(DefaultConversion()));
}

TEST_F(ConversionStorageTest, MaxConversionsPerOrigin) {
  delegate()->set_max_conversions_per_origin(2);
  storage()->StoreImpression(ImpressionBuilder(clock()->Now()).Build());
  EXPECT_EQ(
      1, storage()->MaybeCreateAndStoreConversionReports(DefaultConversion()));
  EXPECT_EQ(
      1, storage()->MaybeCreateAndStoreConversionReports(DefaultConversion()));
  EXPECT_EQ(
      0, storage()->MaybeCreateAndStoreConversionReports(DefaultConversion()));
}

TEST_F(ConversionStorageTest, ClearDataWithNoMatch_NoDelete) {
  base::Time now = clock()->Now();
  auto impression = ImpressionBuilder(now).Build();
  storage()->StoreImpression(impression);
  storage()->ClearData(
      now, now, GetMatcher(url::Origin::Create(GURL("https://no-match.com"))));
  EXPECT_EQ(
      1, storage()->MaybeCreateAndStoreConversionReports(DefaultConversion()));
}

TEST_F(ConversionStorageTest, ClearDataOutsideRange_NoDelete) {
  base::Time now = clock()->Now();
  auto impression = ImpressionBuilder(now).Build();
  storage()->StoreImpression(impression);

  storage()->ClearData(now + base::TimeDelta::FromMinutes(10),
                       now + base::TimeDelta::FromMinutes(20),
                       GetMatcher(impression.impression_origin()));
  EXPECT_EQ(
      1, storage()->MaybeCreateAndStoreConversionReports(DefaultConversion()));
}

TEST_F(ConversionStorageTest, ClearDataImpression) {
  base::Time now = clock()->Now();

  {
    auto impression = ImpressionBuilder(now).Build();
    storage()->StoreImpression(impression);
    storage()->ClearData(now, now + base::TimeDelta::FromMinutes(20),
                         GetMatcher(impression.conversion_origin()));
    EXPECT_EQ(0, storage()->MaybeCreateAndStoreConversionReports(
                     DefaultConversion()));
  }
}

TEST_F(ConversionStorageTest, ClearDataImpressionConversion) {
  base::Time now = clock()->Now();
  auto impression = ImpressionBuilder(now).Build();
  auto conversion = DefaultConversion();

  storage()->StoreImpression(impression);
  EXPECT_EQ(1, storage()->MaybeCreateAndStoreConversionReports(conversion));

  storage()->ClearData(now - base::TimeDelta::FromMinutes(20),
                       now + base::TimeDelta::FromMinutes(20),
                       GetMatcher(impression.impression_origin()));

  EXPECT_TRUE(storage()->GetConversionsToReport(base::Time::Max()).empty());
}

// The null filter should match all origins.
TEST_F(ConversionStorageTest, ClearDataNullFilter) {
  base::Time now = clock()->Now();

  for (int i = 0; i < 10; i++) {
    auto origin =
        url::Origin::Create(GURL(base::StringPrintf("https://%d.com/", i)));
    storage()->StoreImpression(ImpressionBuilder(now)
                                   .SetExpiry(base::TimeDelta::FromDays(30))
                                   .SetImpressionOrigin(origin)
                                   .SetReportingOrigin(origin)
                                   .SetConversionOrigin(origin)
                                   .Build());
    clock()->Advance(base::TimeDelta::FromDays(1));
  }

  // Convert half of them now, half after another day.
  for (int i = 0; i < 5; i++) {
    auto origin =
        url::Origin::Create(GURL(base::StringPrintf("https://%d.com/", i)));
    StorableConversion conversion("1", net::SchemefulSite(origin), origin);
    EXPECT_EQ(1, storage()->MaybeCreateAndStoreConversionReports(conversion));
  }
  clock()->Advance(base::TimeDelta::FromDays(1));
  for (int i = 5; i < 10; i++) {
    auto origin =
        url::Origin::Create(GURL(base::StringPrintf("https://%d.com/", i)));
    StorableConversion conversion("1", net::SchemefulSite(origin), origin);
    EXPECT_EQ(1, storage()->MaybeCreateAndStoreConversionReports(conversion));
  }

  auto null_filter = base::RepeatingCallback<bool(const url::Origin&)>();
  storage()->ClearData(clock()->Now(), clock()->Now(), null_filter);
  EXPECT_EQ(5u, storage()->GetConversionsToReport(base::Time::Max()).size());
}

TEST_F(ConversionStorageTest, ClearDataWithImpressionOutsideRange) {
  base::Time start = clock()->Now();
  auto impression =
      ImpressionBuilder(start).SetExpiry(base::TimeDelta::FromDays(30)).Build();
  auto conversion = DefaultConversion();

  storage()->StoreImpression(impression);

  EXPECT_EQ(1, storage()->MaybeCreateAndStoreConversionReports(conversion));
  storage()->ClearData(clock()->Now(), clock()->Now(),
                       GetMatcher(impression.impression_origin()));
  EXPECT_TRUE(storage()->GetConversionsToReport(base::Time::Max()).empty());
}

// Deletions with time range between the impression and conversion should not
// delete anything, unless the time range intersects one of the events.
TEST_F(ConversionStorageTest, ClearDataRangeBetweenEvents) {
  base::Time start = clock()->Now();
  auto impression =
      ImpressionBuilder(start).SetExpiry(base::TimeDelta::FromDays(30)).Build();
  auto conversion = DefaultConversion();

  std::vector<ConversionReport> expected_reports = {
      GetExpectedReport(impression, conversion, 0)};

  storage()->StoreImpression(impression);

  clock()->Advance(base::TimeDelta::FromDays(1));

  EXPECT_EQ(1, storage()->MaybeCreateAndStoreConversionReports(conversion));

  storage()->ClearData(start + base::TimeDelta::FromMinutes(1),
                       start + base::TimeDelta::FromMinutes(10),
                       GetMatcher(impression.impression_origin()));

  std::vector<ConversionReport> actual_reports =
      storage()->GetConversionsToReport(base::Time::Max());
  EXPECT_TRUE(ReportsEqual(expected_reports, actual_reports));
}
// Test that only a subset of impressions / conversions are deleted with
// multiple impressions per conversion, if only a subset of impressions match.
TEST_F(ConversionStorageTest, ClearDataWithMultiTouch) {
  base::Time start = clock()->Now();
  auto impression1 =
      ImpressionBuilder(start).SetExpiry(base::TimeDelta::FromDays(30)).Build();
  storage()->StoreImpression(impression1);

  clock()->Advance(base::TimeDelta::FromDays(1));
  auto impression2 = ImpressionBuilder(clock()->Now())
                         .SetExpiry(base::TimeDelta::FromDays(30))
                         .Build();
  auto impression3 = ImpressionBuilder(clock()->Now())
                         .SetExpiry(base::TimeDelta::FromDays(30))
                         .Build();

  storage()->StoreImpression(impression2);
  storage()->StoreImpression(impression3);

  EXPECT_EQ(
      3, storage()->MaybeCreateAndStoreConversionReports(DefaultConversion()));

  // Only the first impression should overlap with this time range, but all the
  // impressions should share the origin.
  storage()->ClearData(start, start,
                       GetMatcher(impression1.impression_origin()));
  EXPECT_EQ(2u, storage()->GetConversionsToReport(base::Time::Max()).size());
}

// Attribution occurs at conversion time, not report time, so deleted
// impressions should not adjust credit allocation.
TEST_F(ConversionStorageTest, ClearData_AttributionUnaffected) {
  auto impression1 = ImpressionBuilder(clock()->Now())
                         .SetData("xyz")
                         .SetExpiry(base::TimeDelta::FromDays(30))
                         .Build();
  auto impression2 = ImpressionBuilder(clock()->Now())
                         .SetData("abc")
                         .SetExpiry(base::TimeDelta::FromDays(30))
                         .Build();
  auto conversion = DefaultConversion();
  storage()->StoreImpression(impression1);
  storage()->StoreImpression(impression2);
  std::vector<ConversionReport> expected_reports = {
      GetExpectedReport(impression1, conversion, 0),
      GetExpectedReport(impression2, conversion, 0)};

  clock()->Advance(base::TimeDelta::FromDays(1));
  auto impression3 = ImpressionBuilder(clock()->Now())
                         .SetExpiry(base::TimeDelta::FromDays(30))
                         .Build();
  storage()->StoreImpression(impression3);
  base::Time delete_time = clock()->Now();
  clock()->Advance(base::TimeDelta::FromDays(1));

  AddAttributionCredits({100, 0, 0});
  EXPECT_EQ(3, storage()->MaybeCreateAndStoreConversionReports(conversion));

  // The last impression should be deleted, but the conversion shouldn't be.
  storage()->ClearData(delete_time, delete_time,
                       GetMatcher(impression1.impression_origin()));
  std::vector<ConversionReport> actual_reports =
      storage()->GetConversionsToReport(base::Time::Max());
  EXPECT_TRUE(ReportsEqual(expected_reports, actual_reports));
}

// The max time range with a null filter should delete everything.
TEST_F(ConversionStorageTest, DeleteAll) {
  base::Time start = clock()->Now();
  for (int i = 0; i < 10; i++) {
    auto impression = ImpressionBuilder(start)
                          .SetExpiry(base::TimeDelta::FromDays(30))
                          .Build();
    storage()->StoreImpression(impression);
    clock()->Advance(base::TimeDelta::FromDays(1));
  }

  EXPECT_EQ(
      10, storage()->MaybeCreateAndStoreConversionReports(DefaultConversion()));
  clock()->Advance(base::TimeDelta::FromDays(1));
  EXPECT_EQ(
      10, storage()->MaybeCreateAndStoreConversionReports(DefaultConversion()));

  auto null_filter = base::RepeatingCallback<bool(const url::Origin&)>();
  storage()->ClearData(base::Time::Min(), base::Time::Max(), null_filter);

  // Verify that everything is deleted.
  EXPECT_TRUE(storage()->GetConversionsToReport(base::Time::Max()).empty());
}

// Same as the above test, but uses base::Time() instead of base::Time::Min()
// for delete_begin, which should yield the same behavior.
TEST_F(ConversionStorageTest, DeleteAllNullDeleteBegin) {
  base::Time start = clock()->Now();
  for (int i = 0; i < 10; i++) {
    auto impression = ImpressionBuilder(start)
                          .SetExpiry(base::TimeDelta::FromDays(30))
                          .Build();
    storage()->StoreImpression(impression);
    clock()->Advance(base::TimeDelta::FromDays(1));
  }

  EXPECT_EQ(
      10, storage()->MaybeCreateAndStoreConversionReports(DefaultConversion()));
  clock()->Advance(base::TimeDelta::FromDays(1));
  EXPECT_EQ(
      10, storage()->MaybeCreateAndStoreConversionReports(DefaultConversion()));

  auto null_filter = base::RepeatingCallback<bool(const url::Origin&)>();
  storage()->ClearData(base::Time(), base::Time::Max(), null_filter);

  // Verify that everything is deleted.
  EXPECT_TRUE(storage()->GetConversionsToReport(base::Time::Max()).empty());
}

}  // namespace content
