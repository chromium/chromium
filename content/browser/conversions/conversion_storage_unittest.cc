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
#include "base/callback_helpers.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/strings/stringprintf.h"
#include "base/test/simple_test_clock.h"
#include "build/build_config.h"
#include "content/browser/conversions/conversion_report.h"
#include "content/browser/conversions/conversion_storage_sql.h"
#include "content/browser/conversions/conversion_test_utils.h"
#include "content/browser/conversions/storable_conversion.h"
#include "content/browser/conversions/storable_impression.h"
#include "content/public/common/url_constants.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"
#include "url/url_util.h"

namespace content {

namespace {

using ::testing::ElementsAre;
using ::testing::IsEmpty;

// Default max number of conversions for a single impression for testing.
const int kMaxConversions = 3;

// Default delay in milliseconds for when a report should be sent for testing.
const int kReportTime = 5;

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
                                     const StorableConversion& conversion) {
    ConversionReport report(impression, conversion.conversion_data(),
                            /*conversion_time=*/clock_.Now(),
                            /*report_time=*/impression.impression_time() +
                                base::TimeDelta::FromMilliseconds(kReportTime),
                            /*conversion_id=*/absl::nullopt);
    return report;
  }

  void DeleteConversionReports(std::vector<ConversionReport> reports) {
    for (auto report : reports) {
      EXPECT_TRUE(storage_->DeleteConversion(*report.conversion_id));
    }
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
  EXPECT_FALSE(
      storage->MaybeCreateAndStoreConversionReport(DefaultConversion()));
  EXPECT_TRUE(storage->GetConversionsToReport(clock()->Now()).empty());
  EXPECT_TRUE(storage->GetActiveImpressions().empty());
  EXPECT_EQ(0, storage->DeleteConversion(0));
  EXPECT_NO_FATAL_FAILURE(storage->ClearData(
      base::Time::Min(), base::Time::Max(), base::NullCallback()));
}

TEST_F(ConversionStorageTest, ImpressionStoredAndRetrieved_ValuesIdentical) {
  auto impression = ImpressionBuilder(clock()->Now()).Build();
  storage()->StoreImpression(impression);
  std::vector<StorableImpression> stored_impressions =
      storage()->GetActiveImpressions();
  EXPECT_THAT(stored_impressions, ElementsAre(impression));
}

#if defined(OS_ANDROID)
TEST_F(ConversionStorageTest,
       ImpressionStoredAndRetrieved_ValuesIdentical_AndroidApp) {
  url::ScopedSchemeRegistryForTests scoped_registry;
  url::AddStandardScheme(kAndroidAppScheme, url::SCHEME_WITH_HOST);
  auto impression = ImpressionBuilder(clock()->Now())
                        .SetImpressionOrigin(url::Origin::Create(
                            GURL("android-app:com.any.app")))
                        .Build();
  storage()->StoreImpression(impression);
  std::vector<StorableImpression> stored_impressions =
      storage()->GetActiveImpressions();

  // Verify that each field was stored as expected.
  EXPECT_THAT(stored_impressions, ElementsAre(impression));
}
#endif

TEST_F(ConversionStorageTest,
       GetWithNoMatchingImpressions_NoImpressionsReturned) {
  EXPECT_FALSE(
      storage()->MaybeCreateAndStoreConversionReport(DefaultConversion()));
  EXPECT_TRUE(storage()->GetConversionsToReport(clock()->Now()).empty());
}

TEST_F(ConversionStorageTest, GetWithMatchingImpression_ImpressionReturned) {
  storage()->StoreImpression(ImpressionBuilder(clock()->Now()).Build());
  EXPECT_TRUE(
      storage()->MaybeCreateAndStoreConversionReport(DefaultConversion()));
}

TEST_F(ConversionStorageTest, MultipleImpressionsForConversion_OneConverts) {
  storage()->StoreImpression(ImpressionBuilder(clock()->Now()).Build());
  storage()->StoreImpression(ImpressionBuilder(clock()->Now()).Build());
  EXPECT_TRUE(
      storage()->MaybeCreateAndStoreConversionReport(DefaultConversion()));
}

TEST_F(ConversionStorageTest,
       CrossOriginSameDomainConversion_ImpressionConverted) {
  auto impression =
      ImpressionBuilder(clock()->Now())
          .SetConversionOrigin(url::Origin::Create(GURL("https://sub.a.test")))
          .Build();
  storage()->StoreImpression(impression);
  EXPECT_TRUE(storage()->MaybeCreateAndStoreConversionReport(
      ConversionBuilder()
          .SetConversionDestination(net::SchemefulSite(GURL("https://a.test")))
          .SetReportingOrigin(impression.reporting_origin())
          .Build()));
}

TEST_F(ConversionStorageTest, EventSourceImpressionsForConversion_Converts) {
  storage()->StoreImpression(
      ImpressionBuilder(clock()->Now())
          .SetSourceType(StorableImpression::SourceType::kEvent)
          .Build());
  EXPECT_TRUE(storage()->MaybeCreateAndStoreConversionReport(
      ConversionBuilder().SetEventSourceTriggerData(456).Build()));

  clock()->Advance(base::TimeDelta::FromMilliseconds(kReportTime));

  std::vector<ConversionReport> actual_reports =
      storage()->GetConversionsToReport(clock()->Now());
  EXPECT_EQ(1u, actual_reports.size());
  EXPECT_EQ(456u, actual_reports[0].conversion_data);
}

TEST_F(ConversionStorageTest, ImpressionExpired_NoConversionsStored) {
  storage()->StoreImpression(
      ImpressionBuilder(clock()->Now())
          .SetExpiry(base::TimeDelta::FromMilliseconds(2))
          .Build());
  clock()->Advance(base::TimeDelta::FromMilliseconds(2));

  EXPECT_FALSE(
      storage()->MaybeCreateAndStoreConversionReport(DefaultConversion()));
}

TEST_F(ConversionStorageTest, ImpressionExpired_ConversionsStoredPrior) {
  storage()->StoreImpression(
      ImpressionBuilder(clock()->Now())
          .SetExpiry(base::TimeDelta::FromMilliseconds(4))
          .Build());

  clock()->Advance(base::TimeDelta::FromMilliseconds(3));

  EXPECT_TRUE(
      storage()->MaybeCreateAndStoreConversionReport(DefaultConversion()));

  clock()->Advance(base::TimeDelta::FromMilliseconds(5));

  EXPECT_FALSE(
      storage()->MaybeCreateAndStoreConversionReport(DefaultConversion()));
}

TEST_F(ConversionStorageTest,
       ImpressionWithMaxConversions_ConversionReportNotStored) {
  storage()->StoreImpression(ImpressionBuilder(clock()->Now()).Build());

  for (int i = 0; i < kMaxConversions; i++) {
    EXPECT_TRUE(
        storage()->MaybeCreateAndStoreConversionReport(DefaultConversion()));
  }

  // No additional conversion reports should be created.
  EXPECT_FALSE(
      storage()->MaybeCreateAndStoreConversionReport(DefaultConversion()));
}

TEST_F(ConversionStorageTest, OneConversion_OneReportScheduled) {
  auto impression = ImpressionBuilder(clock()->Now()).Build();
  auto conversion = DefaultConversion();

  storage()->StoreImpression(impression);
  EXPECT_TRUE(storage()->MaybeCreateAndStoreConversionReport(conversion));

  ConversionReport expected_report = GetExpectedReport(impression, conversion);

  clock()->Advance(base::TimeDelta::FromMilliseconds(kReportTime));

  std::vector<ConversionReport> actual_reports =
      storage()->GetConversionsToReport(clock()->Now());
  EXPECT_THAT(actual_reports, ElementsAre(expected_report));
}

TEST_F(ConversionStorageTest,
       ConversionWithDifferentReportingOrigin_NoReportScheduled) {
  auto impression = ImpressionBuilder(clock()->Now())
                        .SetReportingOrigin(
                            url::Origin::Create(GURL("https://different.test")))
                        .Build();
  storage()->StoreImpression(impression);
  EXPECT_FALSE(
      storage()->MaybeCreateAndStoreConversionReport(DefaultConversion()));

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
  EXPECT_FALSE(
      storage()->MaybeCreateAndStoreConversionReport(DefaultConversion()));

  clock()->Advance(base::TimeDelta::FromMilliseconds(kReportTime));

  EXPECT_EQ(0u, storage()->GetConversionsToReport(clock()->Now()).size());
}

TEST_F(ConversionStorageTest, ConversionReportDeleted_RemovedFromStorage) {
  storage()->StoreImpression(ImpressionBuilder(clock()->Now()).Build());
  EXPECT_TRUE(
      storage()->MaybeCreateAndStoreConversionReport(DefaultConversion()));

  clock()->Advance(base::TimeDelta::FromMilliseconds(kReportTime));

  std::vector<ConversionReport> reports =
      storage()->GetConversionsToReport(clock()->Now());
  EXPECT_EQ(1u, reports.size());
  DeleteConversionReports(reports);

  EXPECT_TRUE(storage()->GetConversionsToReport(clock()->Now()).empty());
}

TEST_F(ConversionStorageTest,
       ManyImpressionsWithManyConversions_OneImpressionAttributed) {
  const int kNumMultiTouchImpressions = 20;

  // Store a large, arbitrary number of impressions.
  for (int i = 0; i < kNumMultiTouchImpressions; i++) {
    storage()->StoreImpression(ImpressionBuilder(clock()->Now()).Build());
  }

  for (int i = 0; i < kMaxConversions; i++) {
    EXPECT_TRUE(
        storage()->MaybeCreateAndStoreConversionReport(DefaultConversion()));
  }

  // No additional conversion reports should be created for any of the
  // impressions.
  EXPECT_FALSE(
      storage()->MaybeCreateAndStoreConversionReport(DefaultConversion()));
}

TEST_F(ConversionStorageTest,
       MultipleImpressionsForConversion_UnattributedImpressionsInactive) {
  storage()->StoreImpression(ImpressionBuilder(clock()->Now()).Build());

  auto new_impression =
      ImpressionBuilder(clock()->Now())
          .SetImpressionOrigin(url::Origin::Create(GURL("https://other.test/")))
          .Build();
  storage()->StoreImpression(new_impression);

  // The first impression should be active because even though
  // <reporting_origin, conversion_origin> matches, it has not converted yet.
  EXPECT_EQ(2u, storage()->GetActiveImpressions().size());
  EXPECT_TRUE(
      storage()->MaybeCreateAndStoreConversionReport(DefaultConversion()));
  EXPECT_EQ(1u, storage()->GetActiveImpressions().size());
}

// This test makes sure that when a new click is received for a given
// <reporting_origin, conversion_origin> pair, all existing impressions for that
// origin that have converted are marked ineligible for new conversions per the
// multi-touch model.
TEST_F(ConversionStorageTest,
       NewImpressionForConvertedImpression_MarkedInactive) {
  storage()->StoreImpression(
      ImpressionBuilder(clock()->Now()).SetData(0).Build());
  EXPECT_TRUE(
      storage()->MaybeCreateAndStoreConversionReport(DefaultConversion()));

  clock()->Advance(base::TimeDelta::FromMilliseconds(kReportTime));

  // Delete the report.
  DeleteConversionReports(storage()->GetConversionsToReport(clock()->Now()));

  // Store a new impression that should mark the first inactive.
  auto new_impression = ImpressionBuilder(clock()->Now()).SetData(1000).Build();
  storage()->StoreImpression(new_impression);

  // Only the new impression should convert.
  auto conversion = DefaultConversion();
  EXPECT_TRUE(storage()->MaybeCreateAndStoreConversionReport(conversion));
  ConversionReport expected_report =
      GetExpectedReport(new_impression, conversion);

  clock()->Advance(base::TimeDelta::FromMilliseconds(kReportTime));

  // Verify it was the new impression that converted.
  EXPECT_THAT(storage()->GetConversionsToReport(clock()->Now()),
              ElementsAre(expected_report));
}

TEST_F(ConversionStorageTest,
       NonMatchingImpressionForConvertedImpression_FirstRemainsActive) {
  auto first_impression = ImpressionBuilder(clock()->Now()).Build();
  storage()->StoreImpression(first_impression);

  auto conversion = DefaultConversion();
  EXPECT_TRUE(
      storage()->MaybeCreateAndStoreConversionReport(DefaultConversion()));

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
  EXPECT_TRUE(storage()->MaybeCreateAndStoreConversionReport(conversion));

  ConversionReport expected_report =
      GetExpectedReport(first_impression, conversion);

  // Verify it was the first impression that converted.
  EXPECT_THAT(storage()->GetConversionsToReport(clock()->Now()),
              ElementsAre(expected_report));
}

TEST_F(
    ConversionStorageTest,
    MultipleImpressionsForConversionAtDifferentTimes_OneImpressionAttributed) {
  auto first_impression = ImpressionBuilder(clock()->Now()).Build();
  storage()->StoreImpression(first_impression);

  auto second_impression = ImpressionBuilder(clock()->Now()).Build();
  storage()->StoreImpression(second_impression);

  auto conversion = DefaultConversion();

  // Advance clock so third impression is stored at a different timestamp.
  clock()->Advance(base::TimeDelta::FromMilliseconds(3));

  // Make a conversion with different impression data.
  auto third_impression = ImpressionBuilder(clock()->Now()).SetData(10).Build();
  storage()->StoreImpression(third_impression);

  ConversionReport third_expected_conversion =
      GetExpectedReport(third_impression, conversion);
  EXPECT_TRUE(storage()->MaybeCreateAndStoreConversionReport(conversion));

  clock()->Advance(base::TimeDelta::FromMilliseconds(kReportTime));

  std::vector<ConversionReport> actual_reports =
      storage()->GetConversionsToReport(clock()->Now());

  EXPECT_THAT(actual_reports, ElementsAre(third_expected_conversion));
}

TEST_F(ConversionStorageTest,
       ImpressionsAtDifferentTimes_AttributedImpressionHasCorrectReportTime) {
  auto first_impression = ImpressionBuilder(clock()->Now()).Build();
  storage()->StoreImpression(first_impression);

  // Advance clock so the next impression is stored at a different timestamp.
  clock()->Advance(base::TimeDelta::FromMilliseconds(3));
  storage()->StoreImpression(ImpressionBuilder(clock()->Now()).Build());

  clock()->Advance(base::TimeDelta::FromMilliseconds(3));
  storage()->StoreImpression(ImpressionBuilder(clock()->Now()).Build());

  EXPECT_TRUE(
      storage()->MaybeCreateAndStoreConversionReport(DefaultConversion()));

  // Advance to the first impression's report time and verify only its report is
  // available.
  clock()->Advance(base::TimeDelta::FromMilliseconds(kReportTime - 6));
  EXPECT_EQ(0u, storage()->GetConversionsToReport(clock()->Now()).size());

  clock()->Advance(base::TimeDelta::FromMilliseconds(3));
  EXPECT_EQ(0u, storage()->GetConversionsToReport(clock()->Now()).size());

  clock()->Advance(base::TimeDelta::FromMilliseconds(3));
  EXPECT_EQ(1u, storage()->GetConversionsToReport(clock()->Now()).size());
}

TEST_F(ConversionStorageTest, GetConversionsToReportMultipleTimes_SameResult) {
  storage()->StoreImpression(ImpressionBuilder(clock()->Now()).Build());
  EXPECT_TRUE(
      storage()->MaybeCreateAndStoreConversionReport(DefaultConversion()));
  clock()->Advance(base::TimeDelta::FromMilliseconds(kReportTime));

  std::vector<ConversionReport> first_call_reports =
      storage()->GetConversionsToReport(clock()->Now());
  std::vector<ConversionReport> second_call_reports =
      storage()->GetConversionsToReport(clock()->Now());

  // Expect that |GetConversionsToReport()| did not delete any conversions.
  EXPECT_EQ(first_call_reports, second_call_reports);
}

TEST_F(ConversionStorageTest, MaxImpressionsPerOrigin_LimitsStorage) {
  delegate()->set_max_impressions_per_origin(2);
  storage()->StoreImpression(
      ImpressionBuilder(clock()->Now()).SetData(3).Build());
  storage()->StoreImpression(
      ImpressionBuilder(clock()->Now()).SetData(5).Build());
  storage()->StoreImpression(
      ImpressionBuilder(clock()->Now()).SetData(7).Build());

  std::vector<StorableImpression> stored_impressions =
      storage()->GetActiveImpressions();
  EXPECT_EQ(2u, stored_impressions.size());
  EXPECT_EQ(3u, stored_impressions[0].impression_data());
  EXPECT_EQ(5u, stored_impressions[1].impression_data());
}

TEST_F(ConversionStorageTest, MaxImpressionsPerOrigin_PerOriginNotSite) {
  delegate()->set_max_impressions_per_origin(2);
  storage()->StoreImpression(ImpressionBuilder(clock()->Now())
                                 .SetImpressionOrigin(url::Origin::Create(
                                     GURL("https://foo.a.example")))
                                 .SetData(3)
                                 .Build());
  storage()->StoreImpression(ImpressionBuilder(clock()->Now())
                                 .SetImpressionOrigin(url::Origin::Create(
                                     GURL("https://foo.a.example")))
                                 .SetData(5)
                                 .Build());
  storage()->StoreImpression(ImpressionBuilder(clock()->Now())
                                 .SetImpressionOrigin(url::Origin::Create(
                                     GURL("https://bar.a.example")))
                                 .SetData(7)
                                 .Build());

  std::vector<StorableImpression> stored_impressions =
      storage()->GetActiveImpressions();
  EXPECT_EQ(3u, stored_impressions.size());
  EXPECT_EQ(3u, stored_impressions[0].impression_data());
  EXPECT_EQ(5u, stored_impressions[1].impression_data());
  EXPECT_EQ(7u, stored_impressions[2].impression_data());

  // This impression shouldn't be stored, because its origin has already hit the
  // limit of 2.
  storage()->StoreImpression(ImpressionBuilder(clock()->Now())
                                 .SetImpressionOrigin(url::Origin::Create(
                                     GURL("https://foo.a.example")))
                                 .SetData(9)
                                 .Build());
  // This impression should be stored, because its origin hasn't hit the limit
  // of 2.
  storage()->StoreImpression(ImpressionBuilder(clock()->Now())
                                 .SetImpressionOrigin(url::Origin::Create(
                                     GURL("https://bar.a.example")))
                                 .SetData(11)
                                 .Build());

  stored_impressions = storage()->GetActiveImpressions();
  EXPECT_EQ(4u, stored_impressions.size());
  EXPECT_EQ(3u, stored_impressions[0].impression_data());
  EXPECT_EQ(5u, stored_impressions[1].impression_data());
  EXPECT_EQ(7u, stored_impressions[2].impression_data());
  EXPECT_EQ(11u, stored_impressions[3].impression_data());
}

TEST_F(ConversionStorageTest, MaxConversionsPerOrigin) {
  delegate()->set_max_conversions_per_origin(1);
  storage()->StoreImpression(ImpressionBuilder(clock()->Now()).Build());
  storage()->StoreImpression(ImpressionBuilder(clock()->Now()).Build());
  EXPECT_TRUE(
      storage()->MaybeCreateAndStoreConversionReport(DefaultConversion()));
  // Verify that MaxConversionsPerOrigin is enforced.
  EXPECT_FALSE(
      storage()->MaybeCreateAndStoreConversionReport(DefaultConversion()));
}

TEST_F(ConversionStorageTest, ClearDataWithNoMatch_NoDelete) {
  base::Time now = clock()->Now();
  auto impression = ImpressionBuilder(now).Build();
  storage()->StoreImpression(impression);
  storage()->ClearData(
      now, now, GetMatcher(url::Origin::Create(GURL("https://no-match.com"))));
  EXPECT_TRUE(
      storage()->MaybeCreateAndStoreConversionReport(DefaultConversion()));
}

TEST_F(ConversionStorageTest, ClearDataOutsideRange_NoDelete) {
  base::Time now = clock()->Now();
  auto impression = ImpressionBuilder(now).Build();
  storage()->StoreImpression(impression);

  storage()->ClearData(now + base::TimeDelta::FromMinutes(10),
                       now + base::TimeDelta::FromMinutes(20),
                       GetMatcher(impression.impression_origin()));
  EXPECT_TRUE(
      storage()->MaybeCreateAndStoreConversionReport(DefaultConversion()));
}

TEST_F(ConversionStorageTest, ClearDataImpression) {
  base::Time now = clock()->Now();

  {
    auto impression = ImpressionBuilder(now).Build();
    storage()->StoreImpression(impression);
    storage()->ClearData(now, now + base::TimeDelta::FromMinutes(20),
                         GetMatcher(impression.conversion_origin()));
    EXPECT_FALSE(
        storage()->MaybeCreateAndStoreConversionReport(DefaultConversion()));
  }
}

TEST_F(ConversionStorageTest, ClearDataImpressionConversion) {
  base::Time now = clock()->Now();
  auto impression = ImpressionBuilder(now).Build();
  auto conversion = DefaultConversion();

  storage()->StoreImpression(impression);
  EXPECT_TRUE(storage()->MaybeCreateAndStoreConversionReport(conversion));

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
    EXPECT_TRUE(storage()->MaybeCreateAndStoreConversionReport(
        ConversionBuilder()
            .SetConversionDestination(net::SchemefulSite(origin))
            .SetReportingOrigin(origin)
            .Build()));
  }
  clock()->Advance(base::TimeDelta::FromDays(1));
  for (int i = 5; i < 10; i++) {
    auto origin =
        url::Origin::Create(GURL(base::StringPrintf("https://%d.com/", i)));
    EXPECT_TRUE(storage()->MaybeCreateAndStoreConversionReport(
        ConversionBuilder()
            .SetConversionDestination(net::SchemefulSite(origin))
            .SetReportingOrigin(origin)
            .Build()));
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

  EXPECT_TRUE(storage()->MaybeCreateAndStoreConversionReport(conversion));
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

  storage()->StoreImpression(impression);

  clock()->Advance(base::TimeDelta::FromDays(1));

  const ConversionReport expected_report =
      GetExpectedReport(impression, conversion);

  EXPECT_TRUE(storage()->MaybeCreateAndStoreConversionReport(conversion));

  storage()->ClearData(start + base::TimeDelta::FromMinutes(1),
                       start + base::TimeDelta::FromMinutes(10),
                       GetMatcher(impression.impression_origin()));

  std::vector<ConversionReport> actual_reports =
      storage()->GetConversionsToReport(base::Time::Max());
  EXPECT_THAT(actual_reports, ElementsAre(expected_report));
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

  EXPECT_TRUE(
      storage()->MaybeCreateAndStoreConversionReport(DefaultConversion()));

  // Only the first impression should overlap with this time range, but all the
  // impressions should share the origin.
  storage()->ClearData(start, start,
                       GetMatcher(impression1.impression_origin()));
  EXPECT_EQ(1u, storage()->GetConversionsToReport(base::Time::Max()).size());
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

  EXPECT_TRUE(
      storage()->MaybeCreateAndStoreConversionReport(DefaultConversion()));
  clock()->Advance(base::TimeDelta::FromDays(1));
  EXPECT_TRUE(
      storage()->MaybeCreateAndStoreConversionReport(DefaultConversion()));

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

  EXPECT_TRUE(
      storage()->MaybeCreateAndStoreConversionReport(DefaultConversion()));
  clock()->Advance(base::TimeDelta::FromDays(1));
  EXPECT_TRUE(
      storage()->MaybeCreateAndStoreConversionReport(DefaultConversion()));

  auto null_filter = base::RepeatingCallback<bool(const url::Origin&)>();
  storage()->ClearData(base::Time(), base::Time::Max(), null_filter);

  // Verify that everything is deleted.
  EXPECT_TRUE(storage()->GetConversionsToReport(base::Time::Max()).empty());
}

TEST_F(ConversionStorageTest, MaxAttributionReportsBetweenSites) {
  delegate()->set_rate_limits({
      .time_window = base::TimeDelta::Max(),
      .max_attributions_per_window = 2,
  });

  auto impression = ImpressionBuilder(clock()->Now()).Build();
  auto conversion = DefaultConversion();

  storage()->StoreImpression(ImpressionBuilder(clock()->Now()).Build());
  EXPECT_TRUE(storage()->MaybeCreateAndStoreConversionReport(conversion));
  EXPECT_TRUE(storage()->MaybeCreateAndStoreConversionReport(conversion));
  EXPECT_FALSE(storage()->MaybeCreateAndStoreConversionReport(conversion));

  const ConversionReport expected_report =
      GetExpectedReport(impression, conversion);

  std::vector<ConversionReport> actual_reports =
      storage()->GetConversionsToReport(base::Time::Max());
  EXPECT_THAT(actual_reports, ElementsAre(expected_report, expected_report));
}

TEST_F(ConversionStorageTest,
       MaxAttributionReportsBetweenSites_RespectsSourceType) {
  delegate()->set_rate_limits({
      .time_window = base::TimeDelta::Max(),
      .max_attributions_per_window = 1,
  });

  storage()->StoreImpression(
      ImpressionBuilder(clock()->Now())
          .SetSourceType(StorableImpression::SourceType::kNavigation)
          .Build());
  EXPECT_TRUE(
      storage()->MaybeCreateAndStoreConversionReport(DefaultConversion()));

  storage()->StoreImpression(
      ImpressionBuilder(clock()->Now())
          .SetSourceType(StorableImpression::SourceType::kEvent)
          .Build());
  // This would fail if the source types had a combined limit or the incorrect
  // source type were stored.
  EXPECT_TRUE(
      storage()->MaybeCreateAndStoreConversionReport(DefaultConversion()));

  storage()->StoreImpression(
      ImpressionBuilder(clock()->Now())
          .SetSourceType(StorableImpression::SourceType::kEvent)
          .Build());
  EXPECT_FALSE(
      storage()->MaybeCreateAndStoreConversionReport(DefaultConversion()));

  storage()->StoreImpression(
      ImpressionBuilder(clock()->Now())
          .SetSourceType(StorableImpression::SourceType::kNavigation)
          .Build());
  EXPECT_FALSE(
      storage()->MaybeCreateAndStoreConversionReport(DefaultConversion()));
}

TEST_F(ConversionStorageTest, NeverAttributeImpression_ReportNotStored) {
  delegate()->set_attribution_logic(
      StorableImpression::AttributionLogic::kNever);
  delegate()->set_max_conversions_per_impression(1);
  storage()->StoreImpression(ImpressionBuilder(clock()->Now()).Build());

  EXPECT_FALSE(
      storage()->MaybeCreateAndStoreConversionReport(DefaultConversion()));

  clock()->Advance(base::TimeDelta::FromMilliseconds(kReportTime));

  std::vector<ConversionReport> actual_reports =
      storage()->GetConversionsToReport(clock()->Now());
  EXPECT_THAT(actual_reports, IsEmpty());
}

TEST_F(ConversionStorageTest, NeverAttributeImpression_Deactivates) {
  delegate()->set_attribution_logic(
      StorableImpression::AttributionLogic::kNever);
  delegate()->set_max_conversions_per_impression(1);
  storage()->StoreImpression(
      ImpressionBuilder(clock()->Now()).SetData(3).Build());

  EXPECT_FALSE(
      storage()->MaybeCreateAndStoreConversionReport(DefaultConversion()));

  delegate()->set_attribution_logic(
      StorableImpression::AttributionLogic::kTruthfully);
  storage()->StoreImpression(
      ImpressionBuilder(clock()->Now()).SetData(5).Build());

  EXPECT_TRUE(storage()->MaybeCreateAndStoreConversionReport(
      ConversionBuilder().SetConversionData(7).Build()));

  clock()->Advance(base::TimeDelta::FromMilliseconds(kReportTime));

  std::vector<ConversionReport> actual_reports =
      storage()->GetConversionsToReport(clock()->Now());
  EXPECT_EQ(1u, actual_reports.size());
  EXPECT_EQ(5u, actual_reports[0].impression.impression_data());
  EXPECT_EQ(7u, actual_reports[0].conversion_data);
}

TEST_F(ConversionStorageTest, NeverAttributeImpression_RateLimitsNotChanged) {
  delegate()->set_rate_limits({
      .time_window = base::TimeDelta::Max(),
      .max_attributions_per_window = 1,
  });

  delegate()->set_attribution_logic(
      StorableImpression::AttributionLogic::kNever);
  storage()->StoreImpression(
      ImpressionBuilder(clock()->Now()).SetData(5).Build());

  const auto conversion = DefaultConversion();
  EXPECT_FALSE(storage()->MaybeCreateAndStoreConversionReport(conversion));

  delegate()->set_attribution_logic(
      StorableImpression::AttributionLogic::kTruthfully);
  const auto impression = ImpressionBuilder(clock()->Now()).SetData(7).Build();
  storage()->StoreImpression(impression);
  EXPECT_TRUE(storage()->MaybeCreateAndStoreConversionReport(conversion));

  storage()->StoreImpression(
      ImpressionBuilder(clock()->Now()).SetData(9).Build());
  EXPECT_FALSE(storage()->MaybeCreateAndStoreConversionReport(conversion));

  const ConversionReport expected_report =
      GetExpectedReport(impression, conversion);

  clock()->Advance(base::TimeDelta::FromMilliseconds(kReportTime));

  std::vector<ConversionReport> actual_reports =
      storage()->GetConversionsToReport(clock()->Now());
  EXPECT_THAT(actual_reports, ElementsAre(expected_report));
}

TEST_F(ConversionStorageTest,
       NeverAndTruthfullyAttributeImpressions_ReportNotStored) {
  delegate()->set_attribution_logic(
      StorableImpression::AttributionLogic::kTruthfully);
  storage()->StoreImpression(ImpressionBuilder(clock()->Now()).Build());

  clock()->Advance(base::TimeDelta::FromMilliseconds(1));

  delegate()->set_attribution_logic(
      StorableImpression::AttributionLogic::kNever);
  storage()->StoreImpression(ImpressionBuilder(clock()->Now()).Build());

  const auto conversion = DefaultConversion();
  EXPECT_FALSE(storage()->MaybeCreateAndStoreConversionReport(conversion));
  EXPECT_FALSE(storage()->MaybeCreateAndStoreConversionReport(conversion));

  clock()->Advance(base::TimeDelta::FromMilliseconds(kReportTime));

  std::vector<ConversionReport> actual_reports =
      storage()->GetConversionsToReport(clock()->Now());
  EXPECT_THAT(actual_reports, IsEmpty());
}

TEST_F(ConversionStorageTest,
       MaxAttributionDestinationsPerSource_AlreadyStored) {
  const auto impression =
      ImpressionBuilder(clock()->Now())
          .SetSourceType(StorableImpression::SourceType::kEvent)
          .Build();

  // Setting this doesn't affect the test behavior, but makes it clear that the
  // test passes without depending on the default value of |INT_MAX|.
  delegate()->set_max_attribution_destinations_per_event_source(1);
  storage()->StoreImpression(impression);
  storage()->StoreImpression(impression);

  // The second impression's |conversion_destination| matches the first's, so it
  // doesn't add to the number of distinct values for the |impression_site|,
  // and both impressions should be stored.
  EXPECT_EQ(2u, storage()->GetActiveImpressions().size());
}

TEST_F(
    ConversionStorageTest,
    MaxAttributionDestinationsPerSource_DifferentImpressionSitesAreIndependent) {
  // Setting this doesn't affect the test behavior, but makes it clear that the
  // test passes without depending on the default value of |INT_MAX|.
  delegate()->set_max_attribution_destinations_per_event_source(1);
  storage()->StoreImpression(
      ImpressionBuilder(clock()->Now())
          .SetImpressionOrigin(url::Origin::Create(GURL("https://a.example")))
          .SetConversionOrigin(url::Origin::Create(GURL("https://c.example")))
          .SetSourceType(StorableImpression::SourceType::kEvent)
          .Build());
  storage()->StoreImpression(
      ImpressionBuilder(clock()->Now())
          .SetImpressionOrigin(url::Origin::Create(GURL("https://b.example")))
          .SetConversionOrigin(url::Origin::Create(GURL("https://d.example")))
          .SetSourceType(StorableImpression::SourceType::kEvent)
          .Build());

  // The two impressions together have 2 distinct |conversion_destination|
  // values, but they are independent because they vary by |impression_site|, so
  // both should be stored.
  EXPECT_EQ(2u, storage()->GetActiveImpressions().size());
}

TEST_F(ConversionStorageTest,
       MaxAttributionDestinationsPerSource_IrrelevantForNavigationSources) {
  // Setting this doesn't affect the test behavior, but makes it clear that the
  // test passes without depending on the default value of |INT_MAX|.
  delegate()->set_max_attribution_destinations_per_event_source(1);
  storage()->StoreImpression(
      ImpressionBuilder(clock()->Now())
          .SetConversionOrigin(url::Origin::Create(GURL("https://a.example/")))
          .Build());
  storage()->StoreImpression(
      ImpressionBuilder(clock()->Now())
          .SetConversionOrigin(url::Origin::Create(GURL("https://b.example")))
          .Build());

  // Both impressions should be stored because they are navigation source.
  EXPECT_EQ(2u, storage()->GetActiveImpressions().size());
}

TEST_F(
    ConversionStorageTest,
    MaxAttributionDestinationsPerSource_InsufficientCapacityDeletesOldImpressions) {
  // Verifies that active sources are removed in order, and that the destination
  // limit handles multiple active impressions for the same destination when
  // deleting.

  struct {
    std::string impression_origin;
    std::string conversion_origin;
    int max;
  } kImpressions[] = {
      {"https://foo.test.example", "https://a.example", INT_MAX},
      {"https://bar.test.example", "https://b.example", INT_MAX},
      {"https://xyz.test.example", "https://a.example", INT_MAX},
      {"https://ghi.test.example", "https://b.example", INT_MAX},
      {"https://qrs.test.example", "https://c.example", 2},
  };

  for (const auto& impression : kImpressions) {
    delegate()->set_max_attribution_destinations_per_event_source(
        impression.max);
    storage()->StoreImpression(
        ImpressionBuilder(clock()->Now())
            .SetImpressionOrigin(
                url::Origin::Create(GURL(impression.impression_origin)))
            .SetConversionOrigin(
                url::Origin::Create(GURL(impression.conversion_origin)))
            .SetSourceType(StorableImpression::SourceType::kEvent)
            .Build());
    clock()->Advance(base::TimeDelta::FromMilliseconds(1));
  }

  std::vector<StorableImpression> stored_impressions =
      storage()->GetActiveImpressions();
  EXPECT_EQ(2u, stored_impressions.size());

  EXPECT_EQ(url::Origin::Create(GURL("https://ghi.test.example")),
            stored_impressions[0].impression_origin());
  EXPECT_EQ(url::Origin::Create(GURL("https://qrs.test.example")),
            stored_impressions[1].impression_origin());
}

TEST_F(ConversionStorageTest,
       MaxAttributionDestinationsPerSource_IgnoresInactiveImpressions) {
  delegate()->set_max_conversions_per_impression(1);
  delegate()->set_max_attribution_destinations_per_event_source(INT_MAX);

  storage()->StoreImpression(
      ImpressionBuilder(clock()->Now())
          .SetSourceType(StorableImpression::SourceType::kEvent)
          .Build());
  EXPECT_EQ(1u, storage()->GetActiveImpressions().size());

  EXPECT_TRUE(
      storage()->MaybeCreateAndStoreConversionReport(DefaultConversion()));
  EXPECT_EQ(1u, storage()->GetActiveImpressions().size());

  // Force the impression to be deactivated by ensuring that the next report is
  // in a different window.
  delegate()->set_report_time_ms(kReportTime + 1);
  EXPECT_FALSE(
      storage()->MaybeCreateAndStoreConversionReport(DefaultConversion()));
  EXPECT_EQ(0u, storage()->GetActiveImpressions().size());

  clock()->Advance(base::TimeDelta::FromMilliseconds(1));
  storage()->StoreImpression(
      ImpressionBuilder(clock()->Now())
          .SetConversionOrigin(url::Origin::Create(GURL("https://a.example")))
          .SetSourceType(StorableImpression::SourceType::kEvent)
          .Build());
  EXPECT_EQ(1u, storage()->GetActiveImpressions().size());

  delegate()->set_max_attribution_destinations_per_event_source(1);
  clock()->Advance(base::TimeDelta::FromMilliseconds(1));
  storage()->StoreImpression(
      ImpressionBuilder(clock()->Now())
          .SetConversionOrigin(url::Origin::Create(GURL("https://b.example")))
          .SetSourceType(StorableImpression::SourceType::kEvent)
          .Build());

  // The earliest active impression should be deleted to make room for this new
  // one.
  std::vector<StorableImpression> stored_impressions =
      storage()->GetActiveImpressions();
  EXPECT_EQ(1u, stored_impressions.size());
  EXPECT_EQ(url::Origin::Create(GURL("https://b.example")),
            stored_impressions[0].conversion_origin());

  // Both the inactive impression and the new one for b.example should be
  // retained; a.example is the only one that should have been deleted. The
  // presence of 1 conversion to report implies that the inactive impression
  // remains in the DB.
  clock()->Advance(base::TimeDelta::FromMilliseconds(kReportTime));
  EXPECT_EQ(1u, storage()->GetConversionsToReport(clock()->Now()).size());
}

TEST_F(ConversionStorageTest,
       MultipleImpressionsPerConversion_MostRecentAttributesForSamePriority) {
  storage()->StoreImpression(
      ImpressionBuilder(clock()->Now()).SetData(3).Build());

  clock()->Advance(base::TimeDelta::FromMilliseconds(1));
  storage()->StoreImpression(
      ImpressionBuilder(clock()->Now()).SetData(7).Build());

  clock()->Advance(base::TimeDelta::FromMilliseconds(1));
  storage()->StoreImpression(
      ImpressionBuilder(clock()->Now()).SetData(5).Build());

  EXPECT_EQ(3u, storage()->GetActiveImpressions().size());
  EXPECT_TRUE(
      storage()->MaybeCreateAndStoreConversionReport(DefaultConversion()));

  clock()->Advance(base::TimeDelta::FromMilliseconds(kReportTime));

  std::vector<ConversionReport> actual_reports =
      storage()->GetConversionsToReport(clock()->Now());
  EXPECT_EQ(1u, actual_reports.size());
  EXPECT_EQ(5u, actual_reports[0].impression.impression_data());
}

TEST_F(ConversionStorageTest,
       MultipleImpressionsPerConversion_HighestPriorityAttributes) {
  storage()->StoreImpression(
      ImpressionBuilder(clock()->Now()).SetPriority(100).SetData(3).Build());

  clock()->Advance(base::TimeDelta::FromMilliseconds(1));
  storage()->StoreImpression(
      ImpressionBuilder(clock()->Now()).SetPriority(300).SetData(5).Build());

  clock()->Advance(base::TimeDelta::FromMilliseconds(1));
  storage()->StoreImpression(
      ImpressionBuilder(clock()->Now()).SetPriority(200).SetData(7).Build());

  EXPECT_EQ(3u, storage()->GetActiveImpressions().size());
  EXPECT_TRUE(
      storage()->MaybeCreateAndStoreConversionReport(DefaultConversion()));

  clock()->Advance(base::TimeDelta::FromMilliseconds(kReportTime));

  std::vector<ConversionReport> actual_reports =
      storage()->GetConversionsToReport(clock()->Now());
  EXPECT_EQ(1u, actual_reports.size());
  EXPECT_EQ(5u, actual_reports[0].impression.impression_data());
}

TEST_F(ConversionStorageTest, MultipleImpressions_CorrectDeactivation) {
  storage()->StoreImpression(
      ImpressionBuilder(clock()->Now()).SetData(3).SetPriority(0).Build());
  storage()->StoreImpression(
      ImpressionBuilder(clock()->Now()).SetData(5).SetPriority(1).Build());
  EXPECT_EQ(2u, storage()->GetActiveImpressions().size());

  EXPECT_TRUE(
      storage()->MaybeCreateAndStoreConversionReport(DefaultConversion()));

  // Because the impression with data 5 has the highest priority, it is selected
  // for attribution. The unselected impression with data 3 should be
  // deactivated, but the one with data 5 should remain active.
  std::vector<StorableImpression> active_impressions =
      storage()->GetActiveImpressions();
  EXPECT_EQ(1u, active_impressions.size());
  EXPECT_EQ(5u, active_impressions[0].impression_data());
}

TEST_F(ConversionStorageTest, FalselyAttributeImpression_ReportStored) {
  delegate()->set_attribution_logic(
      StorableImpression::AttributionLogic::kFalsely);
  delegate()->set_fake_event_source_trigger_data(7);
  delegate()->set_max_conversions_per_impression(1);

  const auto impression =
      ImpressionBuilder(clock()->Now())
          .SetData(4)
          .SetSourceType(StorableImpression::SourceType::kEvent)
          .SetPriority(100)
          .Build();
  storage()->StoreImpression(impression);

  const ConversionReport expected_report(
      impression, /*conversion_data=*/7,
      /*conversion_time=*/clock()->Now(),
      /*report_time=*/clock()->Now() +
          base::TimeDelta::FromMilliseconds(kReportTime),
      /*conversion_id=*/absl::nullopt);

  clock()->Advance(base::TimeDelta::FromMilliseconds(kReportTime));

  std::vector<ConversionReport> actual_reports =
      storage()->GetConversionsToReport(clock()->Now());
  EXPECT_THAT(actual_reports, ElementsAre(expected_report));

  EXPECT_TRUE(storage()->GetActiveImpressions().empty());

  // The falsely attributed impression should not be eligible for further
  // attribution.
  EXPECT_FALSE(
      storage()->MaybeCreateAndStoreConversionReport(DefaultConversion()));

  actual_reports = storage()->GetConversionsToReport(clock()->Now());
  EXPECT_THAT(actual_reports, ElementsAre(expected_report));
}

TEST_F(ConversionStorageTest, TriggerPriority) {
  delegate()->set_max_conversions_per_impression(1);

  storage()->StoreImpression(
      ImpressionBuilder(clock()->Now()).SetData(3).SetPriority(0).Build());
  storage()->StoreImpression(
      ImpressionBuilder(clock()->Now()).SetData(5).SetPriority(1).Build());

  EXPECT_TRUE(
      storage()->MaybeCreateAndStoreConversionReport(DefaultConversion()));

  // This conversion should replace the one above because it has a higher
  // priority.
  EXPECT_TRUE(storage()->MaybeCreateAndStoreConversionReport(
      ConversionBuilder().SetPriority(2).SetConversionData(21).Build()));

  storage()->StoreImpression(
      ImpressionBuilder(clock()->Now()).SetData(7).SetPriority(2).Build());

  EXPECT_TRUE(storage()->MaybeCreateAndStoreConversionReport(
      ConversionBuilder().SetPriority(1).SetConversionData(22).Build()));
  // This conversion should be dropped because it has a lower priority than the
  // one above.
  EXPECT_FALSE(
      storage()->MaybeCreateAndStoreConversionReport(DefaultConversion()));

  clock()->Advance(base::TimeDelta::FromMilliseconds(kReportTime));

  std::vector<ConversionReport> actual_reports =
      storage()->GetConversionsToReport(clock()->Now());
  EXPECT_EQ(2u, actual_reports.size());

  EXPECT_EQ(5u, actual_reports[0].impression.impression_data());
  EXPECT_EQ(21u, actual_reports[0].conversion_data);

  EXPECT_EQ(7u, actual_reports[1].impression.impression_data());
  EXPECT_EQ(22u, actual_reports[1].conversion_data);
}

TEST_F(ConversionStorageTest, TriggerPriority_Simple) {
  delegate()->set_max_conversions_per_impression(1);

  storage()->StoreImpression(ImpressionBuilder(clock()->Now()).Build());

  for (int i = 0; i < 10; i++) {
    EXPECT_TRUE(storage()->MaybeCreateAndStoreConversionReport(
        ConversionBuilder().SetPriority(i).SetConversionData(i).Build()));
  }

  clock()->Advance(base::TimeDelta::FromMilliseconds(kReportTime));

  std::vector<ConversionReport> actual_reports =
      storage()->GetConversionsToReport(clock()->Now());
  EXPECT_EQ(1u, actual_reports.size());
  EXPECT_EQ(9u, actual_reports[0].conversion_data);
}

TEST_F(ConversionStorageTest, TriggerPriority_SamePriorityDeletesMostRecent) {
  delegate()->set_max_conversions_per_impression(2);

  storage()->StoreImpression(ImpressionBuilder(clock()->Now()).Build());

  EXPECT_TRUE(storage()->MaybeCreateAndStoreConversionReport(
      ConversionBuilder().SetPriority(1).SetConversionData(3).Build()));

  clock()->Advance(base::TimeDelta::FromMilliseconds(1));
  EXPECT_TRUE(storage()->MaybeCreateAndStoreConversionReport(
      ConversionBuilder().SetPriority(1).SetConversionData(2).Build()));

  // This report should not be stored, as even though it has the same priority
  // as the previous two, it is the most recent.
  clock()->Advance(base::TimeDelta::FromMilliseconds(1));
  EXPECT_FALSE(storage()->MaybeCreateAndStoreConversionReport(
      ConversionBuilder().SetPriority(1).SetConversionData(8).Build()));

  // This report should be stored by replacing the one with `conversion_data ==
  // 2`, which is the most recent of the two with `priority == 1`.
  clock()->Advance(base::TimeDelta::FromMilliseconds(1));
  EXPECT_TRUE(storage()->MaybeCreateAndStoreConversionReport(
      ConversionBuilder().SetPriority(2).SetConversionData(5).Build()));

  std::vector<ConversionReport> actual_reports =
      storage()->GetConversionsToReport(base::Time::Max());
  EXPECT_EQ(2u, actual_reports.size());
  EXPECT_EQ(3u, actual_reports[0].conversion_data);
  EXPECT_EQ(5u, actual_reports[1].conversion_data);
}

TEST_F(ConversionStorageTest, TriggerPriority_DeactivatesImpression) {
  delegate()->set_max_conversions_per_impression(1);

  storage()->StoreImpression(
      ImpressionBuilder(clock()->Now()).SetData(3).SetPriority(0).Build());
  storage()->StoreImpression(
      ImpressionBuilder(clock()->Now()).SetData(5).SetPriority(1).Build());
  EXPECT_EQ(2u, storage()->GetActiveImpressions().size());

  EXPECT_TRUE(
      storage()->MaybeCreateAndStoreConversionReport(DefaultConversion()));

  // Because the impression with data 5 has the highest priority, it is selected
  // for attribution. The unselected impression with data 3 should be
  // deactivated, but the one with data 5 should remain active.
  std::vector<StorableImpression> active_impressions =
      storage()->GetActiveImpressions();
  EXPECT_EQ(1u, active_impressions.size());
  EXPECT_EQ(5u, active_impressions[0].impression_data());

  // Ensure that the next report is in a different window.
  delegate()->set_report_time_ms(kReportTime + 1);

  // This conversion should not be stored because all reports for the attributed
  // impression were in an earlier window.
  EXPECT_FALSE(storage()->MaybeCreateAndStoreConversionReport(
      ConversionBuilder().SetPriority(2).Build()));

  // As a result, the impression with data 5 should also be deactivated.
  EXPECT_TRUE(storage()->GetActiveImpressions().empty());
}

TEST_F(ConversionStorageTest, DedupKey_Dedups) {
  storage()->StoreImpression(
      ImpressionBuilder(clock()->Now())
          .SetData(1)
          .SetConversionOrigin(url::Origin::Create(GURL("https://a.example")))
          .Build());
  storage()->StoreImpression(
      ImpressionBuilder(clock()->Now())
          .SetData(2)
          .SetConversionOrigin(url::Origin::Create(GURL("https://b.example")))
          .Build());
  auto active_impressions = storage()->GetActiveImpressions();
  EXPECT_EQ(2u, active_impressions.size());
  EXPECT_THAT(active_impressions[0].dedup_keys(), IsEmpty());
  EXPECT_THAT(active_impressions[1].dedup_keys(), IsEmpty());

  EXPECT_TRUE(storage()->MaybeCreateAndStoreConversionReport(
      ConversionBuilder()
          .SetConversionDestination(net::SchemefulSite(
              url::Origin::Create(GURL("https://a.example"))))
          .SetDedupKey(11)
          .SetConversionData(71)
          .Build()));

  // Should be stored because dedup key doesn't match even though conversion
  // destination does.
  EXPECT_TRUE(storage()->MaybeCreateAndStoreConversionReport(
      ConversionBuilder()
          .SetConversionDestination(net::SchemefulSite(
              url::Origin::Create(GURL("https://a.example"))))
          .SetDedupKey(12)
          .SetConversionData(72)
          .Build()));

  // Should be stored because conversion destination doesn't match even though
  // dedup key does.
  EXPECT_TRUE(storage()->MaybeCreateAndStoreConversionReport(
      ConversionBuilder()
          .SetConversionDestination(net::SchemefulSite(
              url::Origin::Create(GURL("https://b.example"))))
          .SetDedupKey(12)
          .SetConversionData(73)
          .Build()));

  // Shouldn't be stored because conversion destination and dedup key match.
  EXPECT_FALSE(storage()->MaybeCreateAndStoreConversionReport(
      ConversionBuilder()
          .SetConversionDestination(net::SchemefulSite(
              url::Origin::Create(GURL("https://a.example"))))
          .SetDedupKey(11)
          .SetConversionData(74)
          .Build()));

  // Shouldn't be stored because conversion destination and dedup key match.
  EXPECT_FALSE(storage()->MaybeCreateAndStoreConversionReport(
      ConversionBuilder()
          .SetConversionDestination(net::SchemefulSite(
              url::Origin::Create(GURL("https://b.example"))))
          .SetDedupKey(12)
          .SetConversionData(75)
          .Build()));

  clock()->Advance(base::TimeDelta::FromMilliseconds(kReportTime));
  std::vector<ConversionReport> actual_reports =
      storage()->GetConversionsToReport(clock()->Now());
  EXPECT_EQ(3u, actual_reports.size());
  EXPECT_EQ(71u, actual_reports[0].conversion_data);
  EXPECT_EQ(72u, actual_reports[1].conversion_data);
  EXPECT_EQ(73u, actual_reports[2].conversion_data);

  active_impressions = storage()->GetActiveImpressions();
  EXPECT_EQ(2u, active_impressions.size());
  EXPECT_THAT(active_impressions[0].dedup_keys(), ElementsAre(11, 12));
  EXPECT_THAT(active_impressions[1].dedup_keys(), ElementsAre(12));
}

TEST_F(ConversionStorageTest, DedupKey_DedupsAfterConversionDeletion) {
  storage()->StoreImpression(
      ImpressionBuilder(clock()->Now())
          .SetData(1)
          .SetConversionOrigin(url::Origin::Create(GURL("https://a.example")))
          .Build());
  EXPECT_EQ(1u, storage()->GetActiveImpressions().size());

  clock()->Advance(base::TimeDelta::FromMilliseconds(1));

  EXPECT_TRUE(storage()->MaybeCreateAndStoreConversionReport(
      ConversionBuilder()
          .SetConversionDestination(net::SchemefulSite(
              url::Origin::Create(GURL("https://a.example"))))
          .SetDedupKey(2)
          .SetConversionData(3)
          .Build()));

  clock()->Advance(base::TimeDelta::FromMilliseconds(kReportTime));

  std::vector<ConversionReport> actual_reports =
      storage()->GetConversionsToReport(clock()->Now());
  EXPECT_EQ(1u, actual_reports.size());
  EXPECT_EQ(3u, actual_reports[0].conversion_data);

  // Simulate the report being sent and deleted from storage.
  DeleteConversionReports(actual_reports);

  clock()->Advance(base::TimeDelta::FromMilliseconds(1));

  // This report shouldn't be stored, as it should be deduped against the
  // previously stored one even though that previous one is no longer in the DB.
  EXPECT_FALSE(storage()->MaybeCreateAndStoreConversionReport(
      ConversionBuilder()
          .SetConversionDestination(net::SchemefulSite(
              url::Origin::Create(GURL("https://a.example"))))
          .SetDedupKey(2)
          .SetConversionData(5)
          .Build()));

  clock()->Advance(base::TimeDelta::FromMilliseconds(kReportTime));
  EXPECT_THAT(storage()->GetConversionsToReport(clock()->Now()), IsEmpty());
}

}  // namespace content
