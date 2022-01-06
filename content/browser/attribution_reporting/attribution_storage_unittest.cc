// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/attribution_storage.h"

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
#include "base/guid.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/stringprintf.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "content/browser/attribution_reporting/attribution_storage_sql.h"
#include "content/browser/attribution_reporting/attribution_test_utils.h"
#include "content/browser/attribution_reporting/event_attribution_report.h"
#include "content/browser/attribution_reporting/storable_source.h"
#include "content/browser/attribution_reporting/storable_trigger.h"
#include "content/public/common/url_constants.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"
#include "url/url_util.h"

namespace content {

namespace {

using CreateReportResult = ::content::AttributionStorage::CreateReportResult;
using CreateReportStatus =
    ::content::AttributionStorage::CreateReportResult::Status;
using DeactivatedSource = ::content::AttributionStorage::DeactivatedSource;

using ::testing::AllOf;
using ::testing::ElementsAre;
using ::testing::Ge;
using ::testing::IsEmpty;
using ::testing::IsTrue;
using ::testing::Le;
using ::testing::Property;
using ::testing::SizeIs;

// Default max number of conversions for a single impression for testing.
const int kMaxConversions = 3;

// Default delay in milliseconds for when a report should be sent for testing.
const int kReportTime = 5;

base::RepeatingCallback<bool(const url::Origin&)> GetMatcher(
    const url::Origin& to_delete) {
  return base::BindRepeating(std::equal_to<url::Origin>(), to_delete);
}

}  // namespace

// Unit test suite for the AttributionStorage interface. All AttributionStorage
// implementations (including fakes) should be able to re-use this test suite.
class AttributionStorageTest : public testing::Test {
 public:
  AttributionStorageTest() {
    EXPECT_TRUE(dir_.CreateUniqueTempDir());
    auto delegate = std::make_unique<ConfigurableStorageDelegate>();
    delegate->set_report_time_ms(kReportTime);
    delegate->set_max_attributions_per_source(kMaxConversions);
    delegate_ = delegate.get();
    storage_ = std::make_unique<AttributionStorageSql>(dir_.GetPath(),
                                                       std::move(delegate));
  }

  // Given a |conversion|, returns the expected conversion report properties at
  // the current timestamp.
  EventAttributionReport GetExpectedReport(const StorableSource& impression,
                                           const StorableTrigger& conversion) {
    return ReportBuilder(impression)
        .SetTriggerData(conversion.trigger_data())
        .SetConversionTime(base::Time::Now())
        .SetReportTime(impression.impression_time() +
                       base::Milliseconds(kReportTime))
        .SetPriority(conversion.priority())
        .Build();
  }

  CreateReportStatus MaybeCreateAndStoreReport(
      const StorableTrigger& conversion) {
    return storage_->MaybeCreateAndStoreReport(conversion).status();
  }

  void DeleteReports(const std::vector<EventAttributionReport>& reports) {
    for (const auto& report : reports) {
      EXPECT_TRUE(storage_->DeleteReport(*report.report_id()));
    }
  }

  AttributionStorage* storage() { return storage_.get(); }

  ConfigurableStorageDelegate* delegate() { return delegate_; }

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  base::ScopedTempDir dir_;

 private:
  raw_ptr<ConfigurableStorageDelegate> delegate_;
  std::unique_ptr<AttributionStorage> storage_;
};

TEST_F(AttributionStorageTest,
       StorageUsedAfterFailedInitilization_FailsSilently) {
  // We create a failed initialization by writing a dir to the database file
  // path.
  base::CreateDirectoryAndGetError(
      dir_.GetPath().Append(FILE_PATH_LITERAL("Conversions")), nullptr);
  std::unique_ptr<AttributionStorage> storage =
      std::make_unique<AttributionStorageSql>(
          dir_.GetPath(), std::make_unique<ConfigurableStorageDelegate>());
  static_cast<AttributionStorageSql*>(storage.get())
      ->set_ignore_errors_for_testing(true);

  // Test all public methods on AttributionStorage.
  EXPECT_NO_FATAL_FAILURE(storage->StoreSource(SourceBuilder().Build()));
  EXPECT_EQ(CreateReportStatus::kNoMatchingImpressions,
            storage->MaybeCreateAndStoreReport(DefaultTrigger()).status());
  EXPECT_THAT(storage->GetAttributionsToReport(base::Time::Now()), IsEmpty());
  EXPECT_THAT(storage->GetActiveSources(), IsEmpty());
  EXPECT_TRUE(storage->DeleteReport(EventAttributionReport::Id(0)));
  EXPECT_NO_FATAL_FAILURE(storage->ClearData(
      base::Time::Min(), base::Time::Max(), base::NullCallback()));
  EXPECT_EQ(
      storage->AdjustOfflineReportTimes(base::TimeDelta(), base::TimeDelta()),
      absl::nullopt);
}

TEST_F(AttributionStorageTest, ImpressionStoredAndRetrieved_ValuesIdentical) {
  auto impression = SourceBuilder().Build();
  storage()->StoreSource(impression);
  EXPECT_THAT(storage()->GetActiveSources(), ElementsAre(impression));
}

#if defined(OS_ANDROID)
TEST_F(AttributionStorageTest,
       ImpressionStoredAndRetrieved_ValuesIdentical_AndroidApp) {
  url::ScopedSchemeRegistryForTests scoped_registry;
  url::AddStandardScheme(kAndroidAppScheme, url::SCHEME_WITH_HOST);
  auto impression = SourceBuilder()
                        .SetImpressionOrigin(url::Origin::Create(
                            GURL("android-app:com.any.app")))
                        .Build();
  storage()->StoreSource(impression);

  // Verify that each field was stored as expected.
  EXPECT_THAT(storage()->GetActiveSources(), ElementsAre(impression));
}
#endif

TEST_F(AttributionStorageTest,
       GetWithNoMatchingImpressions_NoImpressionsReturned) {
  EXPECT_EQ(CreateReportStatus::kNoMatchingImpressions,
            MaybeCreateAndStoreReport(DefaultTrigger()));
  EXPECT_THAT(storage()->GetAttributionsToReport(base::Time::Now()), IsEmpty());
}

TEST_F(AttributionStorageTest, GetWithMatchingImpression_ImpressionReturned) {
  storage()->StoreSource(SourceBuilder().Build());
  EXPECT_EQ(CreateReportStatus::kSuccess,
            MaybeCreateAndStoreReport(DefaultTrigger()));
}

TEST_F(AttributionStorageTest, MultipleImpressionsForConversion_OneConverts) {
  storage()->StoreSource(SourceBuilder().Build());
  storage()->StoreSource(SourceBuilder().Build());
  EXPECT_EQ(CreateReportStatus::kSuccess,
            MaybeCreateAndStoreReport(DefaultTrigger()));
}

TEST_F(AttributionStorageTest,
       CrossOriginSameDomainConversion_ImpressionConverted) {
  auto impression =
      SourceBuilder()
          .SetConversionOrigin(url::Origin::Create(GURL("https://sub.a.test")))
          .Build();
  storage()->StoreSource(impression);
  EXPECT_EQ(CreateReportStatus::kSuccess,
            MaybeCreateAndStoreReport(
                TriggerBuilder()
                    .SetConversionDestination(
                        net::SchemefulSite(GURL("https://a.test")))
                    .SetReportingOrigin(impression.reporting_origin())
                    .Build()));
}

TEST_F(AttributionStorageTest, EventSourceImpressionsForConversion_Converts) {
  storage()->StoreSource(SourceBuilder()
                             .SetSourceType(StorableSource::SourceType::kEvent)
                             .Build());
  EXPECT_EQ(CreateReportStatus::kSuccess,
            MaybeCreateAndStoreReport(
                TriggerBuilder().SetEventSourceTriggerData(456).Build()));

  task_environment_.FastForwardBy(base::Milliseconds(kReportTime));

  EXPECT_THAT(
      storage()->GetAttributionsToReport(base::Time::Now()),
      ElementsAre(Property(&EventAttributionReport::trigger_data, 456u)));
}

TEST_F(AttributionStorageTest, ImpressionExpired_NoConversionsStored) {
  storage()->StoreSource(
      SourceBuilder().SetExpiry(base::Milliseconds(2)).Build());
  task_environment_.FastForwardBy(base::Milliseconds(2));

  EXPECT_EQ(CreateReportStatus::kNoMatchingImpressions,
            MaybeCreateAndStoreReport(DefaultTrigger()));
}

TEST_F(AttributionStorageTest, ImpressionExpired_ConversionsStoredPrior) {
  storage()->StoreSource(
      SourceBuilder().SetExpiry(base::Milliseconds(4)).Build());

  task_environment_.FastForwardBy(base::Milliseconds(3));

  EXPECT_EQ(CreateReportStatus::kSuccess,
            MaybeCreateAndStoreReport(DefaultTrigger()));

  task_environment_.FastForwardBy(base::Milliseconds(5));

  EXPECT_EQ(CreateReportStatus::kNoMatchingImpressions,
            MaybeCreateAndStoreReport(DefaultTrigger()));
}

TEST_F(AttributionStorageTest,
       ImpressionWithMaxConversions_ConversionReportNotStored) {
  storage()->StoreSource(SourceBuilder().Build());

  for (int i = 0; i < kMaxConversions; i++) {
    EXPECT_EQ(CreateReportStatus::kSuccess,
              MaybeCreateAndStoreReport(DefaultTrigger()));
  }

  // No additional conversion reports should be created.
  EXPECT_THAT(storage()->MaybeCreateAndStoreReport(DefaultTrigger()),
              AllOf(Property(&CreateReportResult::status,
                             CreateReportStatus::kPriorityTooLow),
                    Property(&CreateReportResult::dropped_report, IsTrue()),
                    Property(&CreateReportResult::GetDeactivatedSource,
                             absl::nullopt)));
}

TEST_F(AttributionStorageTest, OneConversion_OneReportScheduled) {
  auto impression = SourceBuilder().Build();
  auto conversion = DefaultTrigger();

  storage()->StoreSource(impression);
  EXPECT_EQ(CreateReportStatus::kSuccess,
            MaybeCreateAndStoreReport(conversion));

  EventAttributionReport expected_report =
      GetExpectedReport(impression, conversion);

  task_environment_.FastForwardBy(base::Milliseconds(kReportTime));

  EXPECT_THAT(storage()->GetAttributionsToReport(base::Time::Now()),
              ElementsAre(expected_report));
}

TEST_F(AttributionStorageTest,
       ConversionWithDifferentReportingOrigin_NoReportScheduled) {
  auto impression = SourceBuilder()
                        .SetReportingOrigin(
                            url::Origin::Create(GURL("https://different.test")))
                        .Build();
  storage()->StoreSource(impression);
  EXPECT_EQ(CreateReportStatus::kNoMatchingImpressions,
            MaybeCreateAndStoreReport(DefaultTrigger()));

  task_environment_.FastForwardBy(base::Milliseconds(kReportTime));

  EXPECT_THAT(storage()->GetAttributionsToReport(base::Time::Now()), IsEmpty());
}

TEST_F(AttributionStorageTest,
       ConversionWithDifferentConversionOrigin_NoReportScheduled) {
  auto impression = SourceBuilder()
                        .SetConversionOrigin(
                            url::Origin::Create(GURL("https://different.test")))
                        .Build();
  storage()->StoreSource(impression);
  EXPECT_EQ(CreateReportStatus::kNoMatchingImpressions,
            MaybeCreateAndStoreReport(DefaultTrigger()));

  task_environment_.FastForwardBy(base::Milliseconds(kReportTime));

  EXPECT_THAT(storage()->GetAttributionsToReport(base::Time::Now()), IsEmpty());
}

TEST_F(AttributionStorageTest, ConversionReportDeleted_RemovedFromStorage) {
  storage()->StoreSource(SourceBuilder().Build());
  EXPECT_EQ(CreateReportStatus::kSuccess,
            MaybeCreateAndStoreReport(DefaultTrigger()));

  task_environment_.FastForwardBy(base::Milliseconds(kReportTime));

  std::vector<EventAttributionReport> reports =
      storage()->GetAttributionsToReport(base::Time::Now());
  EXPECT_THAT(reports, SizeIs(1));
  DeleteReports(reports);

  EXPECT_THAT(storage()->GetAttributionsToReport(base::Time::Now()), IsEmpty());
}

TEST_F(AttributionStorageTest,
       ManyImpressionsWithManyConversions_OneImpressionAttributed) {
  const int kNumMultiTouchImpressions = 20;

  // Store a large, arbitrary number of impressions.
  for (int i = 0; i < kNumMultiTouchImpressions; i++) {
    storage()->StoreSource(SourceBuilder().Build());
  }

  for (int i = 0; i < kMaxConversions; i++) {
    EXPECT_EQ(CreateReportStatus::kSuccess,
              MaybeCreateAndStoreReport(DefaultTrigger()));
  }

  // No additional conversion reports should be created for any of the
  // impressions.
  EXPECT_EQ(CreateReportStatus::kPriorityTooLow,
            MaybeCreateAndStoreReport(DefaultTrigger()));
}

TEST_F(AttributionStorageTest,
       MultipleImpressionsForConversion_UnattributedImpressionsInactive) {
  storage()->StoreSource(SourceBuilder().Build());

  auto new_impression =
      SourceBuilder()
          .SetImpressionOrigin(url::Origin::Create(GURL("https://other.test/")))
          .Build();
  storage()->StoreSource(new_impression);

  // The first impression should be active because even though
  // <reporting_origin, conversion_origin> matches, it has not converted yet.
  EXPECT_THAT(storage()->GetActiveSources(), SizeIs(2));
  EXPECT_EQ(CreateReportStatus::kSuccess,
            MaybeCreateAndStoreReport(DefaultTrigger()));
  EXPECT_THAT(storage()->GetActiveSources(), SizeIs(1));
}

// This test makes sure that when a new click is received for a given
// <reporting_origin, conversion_origin> pair, all existing impressions for that
// origin that have converted are marked ineligible for new conversions per the
// multi-touch model.
TEST_F(AttributionStorageTest,
       NewImpressionForConvertedImpression_MarkedInactive) {
  storage()->StoreSource(SourceBuilder().SetSourceEventId(0).Build());
  EXPECT_EQ(CreateReportStatus::kSuccess,
            MaybeCreateAndStoreReport(DefaultTrigger()));

  task_environment_.FastForwardBy(base::Milliseconds(kReportTime));

  // Delete the report.
  DeleteReports(storage()->GetAttributionsToReport(base::Time::Now()));

  // Store a new impression that should mark the first inactive.
  auto new_impression = SourceBuilder().SetSourceEventId(1000).Build();
  storage()->StoreSource(new_impression);

  // Only the new impression should convert.
  auto conversion = DefaultTrigger();
  EXPECT_EQ(CreateReportStatus::kSuccess,
            MaybeCreateAndStoreReport(conversion));
  EventAttributionReport expected_report =
      GetExpectedReport(new_impression, conversion);

  task_environment_.FastForwardBy(base::Milliseconds(kReportTime));

  // Verify it was the new impression that converted.
  EXPECT_THAT(storage()->GetAttributionsToReport(base::Time::Now()),
              ElementsAre(expected_report));
}

TEST_F(AttributionStorageTest,
       NonMatchingImpressionForConvertedImpression_FirstRemainsActive) {
  auto first_impression = SourceBuilder().Build();
  storage()->StoreSource(first_impression);

  auto conversion = DefaultTrigger();
  EXPECT_EQ(CreateReportStatus::kSuccess,
            MaybeCreateAndStoreReport(DefaultTrigger()));

  task_environment_.FastForwardBy(base::Milliseconds(kReportTime));

  // Delete the report.
  DeleteReports(storage()->GetAttributionsToReport(base::Time::Now()));

  // Store a new impression with a different reporting origin.
  auto new_impression = SourceBuilder()
                            .SetReportingOrigin(url::Origin::Create(
                                GURL("https://different.test")))
                            .Build();
  storage()->StoreSource(new_impression);

  // The first impression should still be active and able to convert.
  EXPECT_EQ(CreateReportStatus::kSuccess,
            MaybeCreateAndStoreReport(conversion));

  EventAttributionReport expected_report =
      GetExpectedReport(first_impression, conversion);

  // Verify it was the first impression that converted.
  EXPECT_THAT(storage()->GetAttributionsToReport(base::Time::Now()),
              ElementsAre(expected_report));
}

TEST_F(
    AttributionStorageTest,
    MultipleImpressionsForConversionAtDifferentTimes_OneImpressionAttributed) {
  auto first_impression = SourceBuilder().Build();
  storage()->StoreSource(first_impression);

  auto second_impression = SourceBuilder().Build();
  storage()->StoreSource(second_impression);

  auto conversion = DefaultTrigger();

  // Advance clock so third impression is stored at a different timestamp.
  task_environment_.FastForwardBy(base::Milliseconds(3));

  // Make a conversion with different impression data.
  auto third_impression = SourceBuilder().SetSourceEventId(10).Build();
  storage()->StoreSource(third_impression);

  EventAttributionReport third_expected_conversion =
      GetExpectedReport(third_impression, conversion);
  EXPECT_EQ(CreateReportStatus::kSuccess,
            MaybeCreateAndStoreReport(conversion));

  task_environment_.FastForwardBy(base::Milliseconds(kReportTime));

  EXPECT_THAT(storage()->GetAttributionsToReport(base::Time::Now()),
              ElementsAre(third_expected_conversion));
}

TEST_F(AttributionStorageTest,
       ImpressionsAtDifferentTimes_AttributedImpressionHasCorrectReportTime) {
  auto first_impression = SourceBuilder().Build();
  storage()->StoreSource(first_impression);

  // Advance clock so the next impression is stored at a different timestamp.
  task_environment_.FastForwardBy(base::Milliseconds(2));
  storage()->StoreSource(SourceBuilder().Build());

  task_environment_.FastForwardBy(base::Milliseconds(2));
  storage()->StoreSource(SourceBuilder().Build());

  EXPECT_EQ(CreateReportStatus::kSuccess,
            MaybeCreateAndStoreReport(DefaultTrigger()));

  // Advance to the first impression's report time and verify only its report is
  // available.
  task_environment_.FastForwardBy(base::Milliseconds(kReportTime - 1));
  EXPECT_THAT(storage()->GetAttributionsToReport(base::Time::Now()), IsEmpty());

  task_environment_.FastForwardBy(base::Milliseconds(1));
  EXPECT_THAT(storage()->GetAttributionsToReport(base::Time::Now()), SizeIs(1));
}

TEST_F(AttributionStorageTest,
       GetAttributionsToReportMultipleTimes_SameResult) {
  storage()->StoreSource(SourceBuilder().Build());
  EXPECT_EQ(CreateReportStatus::kSuccess,
            MaybeCreateAndStoreReport(DefaultTrigger()));
  task_environment_.FastForwardBy(base::Milliseconds(kReportTime));

  std::vector<EventAttributionReport> first_call_reports =
      storage()->GetAttributionsToReport(base::Time::Now());
  std::vector<EventAttributionReport> second_call_reports =
      storage()->GetAttributionsToReport(base::Time::Now());

  // Expect that |GetAttributionsToReport()| did not delete any conversions.
  EXPECT_EQ(first_call_reports, second_call_reports);
}

TEST_F(AttributionStorageTest, MaxImpressionsPerOrigin_LimitsStorage) {
  delegate()->set_max_sources_per_origin(2);
  storage()->StoreSource(SourceBuilder().SetSourceEventId(3).Build());
  storage()->StoreSource(SourceBuilder().SetSourceEventId(5).Build());
  storage()->StoreSource(SourceBuilder().SetSourceEventId(7).Build());

  EXPECT_THAT(storage()->GetActiveSources(),
              ElementsAre(Property(&StorableSource::source_event_id, 3u),
                          Property(&StorableSource::source_event_id, 5u)));
}

TEST_F(AttributionStorageTest, MaxImpressionsPerOrigin_PerOriginNotSite) {
  delegate()->set_max_sources_per_origin(2);
  storage()->StoreSource(SourceBuilder()
                             .SetImpressionOrigin(url::Origin::Create(
                                 GURL("https://foo.a.example")))
                             .SetSourceEventId(3)
                             .Build());
  storage()->StoreSource(SourceBuilder()
                             .SetImpressionOrigin(url::Origin::Create(
                                 GURL("https://foo.a.example")))
                             .SetSourceEventId(5)
                             .Build());
  storage()->StoreSource(SourceBuilder()
                             .SetImpressionOrigin(url::Origin::Create(
                                 GURL("https://bar.a.example")))
                             .SetSourceEventId(7)
                             .Build());

  EXPECT_THAT(storage()->GetActiveSources(),
              ElementsAre(Property(&StorableSource::source_event_id, 3u),
                          Property(&StorableSource::source_event_id, 5u),
                          Property(&StorableSource::source_event_id, 7u)));

  // This impression shouldn't be stored, because its origin has already hit the
  // limit of 2.
  storage()->StoreSource(SourceBuilder()
                             .SetImpressionOrigin(url::Origin::Create(
                                 GURL("https://foo.a.example")))
                             .SetSourceEventId(9)
                             .Build());
  // This impression should be stored, because its origin hasn't hit the limit
  // of 2.
  storage()->StoreSource(SourceBuilder()
                             .SetImpressionOrigin(url::Origin::Create(
                                 GURL("https://bar.a.example")))
                             .SetSourceEventId(11)
                             .Build());

  EXPECT_THAT(storage()->GetActiveSources(),
              ElementsAre(Property(&StorableSource::source_event_id, 3u),
                          Property(&StorableSource::source_event_id, 5u),
                          Property(&StorableSource::source_event_id, 7u),
                          Property(&StorableSource::source_event_id, 11u)));
}

TEST_F(AttributionStorageTest, MaxConversionsPerOrigin) {
  delegate()->set_max_attributions_per_origin(1);
  storage()->StoreSource(SourceBuilder().Build());
  storage()->StoreSource(SourceBuilder().Build());
  EXPECT_EQ(CreateReportStatus::kSuccess,
            MaybeCreateAndStoreReport(DefaultTrigger()));
  // Verify that MaxConversionsPerOrigin is enforced.
  EXPECT_EQ(CreateReportStatus::kNoCapacityForConversionDestination,
            MaybeCreateAndStoreReport(DefaultTrigger()));
}

TEST_F(AttributionStorageTest, ClearDataWithNoMatch_NoDelete) {
  base::Time now = base::Time::Now();
  auto impression = SourceBuilder(now).Build();
  storage()->StoreSource(impression);
  storage()->ClearData(
      now, now, GetMatcher(url::Origin::Create(GURL("https://no-match.com"))));
  EXPECT_EQ(CreateReportStatus::kSuccess,
            MaybeCreateAndStoreReport(DefaultTrigger()));
}

TEST_F(AttributionStorageTest, ClearDataOutsideRange_NoDelete) {
  base::Time now = base::Time::Now();
  auto impression = SourceBuilder(now).Build();
  storage()->StoreSource(impression);

  storage()->ClearData(now + base::Minutes(10), now + base::Minutes(20),
                       GetMatcher(impression.impression_origin()));
  EXPECT_EQ(CreateReportStatus::kSuccess,
            MaybeCreateAndStoreReport(DefaultTrigger()));
}

TEST_F(AttributionStorageTest, ClearDataImpression) {
  base::Time now = base::Time::Now();

  {
    auto impression = SourceBuilder(now).Build();
    storage()->StoreSource(impression);
    storage()->ClearData(now, now + base::Minutes(20),
                         GetMatcher(impression.conversion_origin()));
    EXPECT_EQ(CreateReportStatus::kNoMatchingImpressions,
              MaybeCreateAndStoreReport(DefaultTrigger()));
  }
}

TEST_F(AttributionStorageTest, ClearDataImpressionConversion) {
  base::Time now = base::Time::Now();
  auto impression = SourceBuilder(now).Build();
  auto conversion = DefaultTrigger();

  storage()->StoreSource(impression);
  EXPECT_EQ(CreateReportStatus::kSuccess,
            MaybeCreateAndStoreReport(conversion));

  storage()->ClearData(now - base::Minutes(20), now + base::Minutes(20),
                       GetMatcher(impression.impression_origin()));

  EXPECT_THAT(storage()->GetAttributionsToReport(base::Time::Max()), IsEmpty());
}

// The null filter should match all origins.
TEST_F(AttributionStorageTest, ClearDataNullFilter) {
  base::Time now = base::Time::Now();

  for (int i = 0; i < 10; i++) {
    auto origin =
        url::Origin::Create(GURL(base::StringPrintf("https://%d.com/", i)));
    storage()->StoreSource(SourceBuilder(now)
                               .SetExpiry(base::Days(30))
                               .SetImpressionOrigin(origin)
                               .SetReportingOrigin(origin)
                               .SetConversionOrigin(origin)
                               .Build());
    task_environment_.FastForwardBy(base::Days(1));
  }

  // Convert half of them now, half after another day.
  for (int i = 0; i < 5; i++) {
    auto origin =
        url::Origin::Create(GURL(base::StringPrintf("https://%d.com/", i)));
    EXPECT_EQ(CreateReportStatus::kSuccess,
              MaybeCreateAndStoreReport(
                  TriggerBuilder()
                      .SetConversionDestination(net::SchemefulSite(origin))
                      .SetReportingOrigin(origin)
                      .Build()));
  }
  task_environment_.FastForwardBy(base::Days(1));
  for (int i = 5; i < 10; i++) {
    auto origin =
        url::Origin::Create(GURL(base::StringPrintf("https://%d.com/", i)));
    EXPECT_EQ(CreateReportStatus::kSuccess,
              MaybeCreateAndStoreReport(
                  TriggerBuilder()
                      .SetConversionDestination(net::SchemefulSite(origin))
                      .SetReportingOrigin(origin)
                      .Build()));
  }

  auto null_filter = base::RepeatingCallback<bool(const url::Origin&)>();
  storage()->ClearData(base::Time::Now(), base::Time::Now(), null_filter);
  EXPECT_THAT(storage()->GetAttributionsToReport(base::Time::Max()), SizeIs(5));
}

TEST_F(AttributionStorageTest, ClearDataWithImpressionOutsideRange) {
  base::Time start = base::Time::Now();
  auto impression = SourceBuilder(start).SetExpiry(base::Days(30)).Build();
  auto conversion = DefaultTrigger();

  storage()->StoreSource(impression);

  EXPECT_EQ(CreateReportStatus::kSuccess,
            MaybeCreateAndStoreReport(conversion));
  storage()->ClearData(base::Time::Now(), base::Time::Now(),
                       GetMatcher(impression.impression_origin()));
  EXPECT_THAT(storage()->GetAttributionsToReport(base::Time::Max()), IsEmpty());
}

// Deletions with time range between the impression and conversion should not
// delete anything, unless the time range intersects one of the events.
TEST_F(AttributionStorageTest, ClearDataRangeBetweenEvents) {
  base::Time start = base::Time::Now();
  auto impression = SourceBuilder(start).SetExpiry(base::Days(30)).Build();
  auto conversion = DefaultTrigger();

  storage()->StoreSource(impression);

  task_environment_.FastForwardBy(base::Days(1));

  const EventAttributionReport expected_report =
      GetExpectedReport(impression, conversion);

  EXPECT_EQ(CreateReportStatus::kSuccess,
            MaybeCreateAndStoreReport(conversion));

  storage()->ClearData(start + base::Minutes(1), start + base::Minutes(10),
                       GetMatcher(impression.impression_origin()));

  EXPECT_THAT(storage()->GetAttributionsToReport(base::Time::Max()),
              ElementsAre(expected_report));
}
// Test that only a subset of impressions / conversions are deleted with
// multiple impressions per conversion, if only a subset of impressions match.
TEST_F(AttributionStorageTest, ClearDataWithMultiTouch) {
  base::Time start = base::Time::Now();
  auto impression1 = SourceBuilder(start).SetExpiry(base::Days(30)).Build();
  storage()->StoreSource(impression1);

  task_environment_.FastForwardBy(base::Days(1));
  auto impression2 = SourceBuilder().SetExpiry(base::Days(30)).Build();
  auto impression3 = SourceBuilder().SetExpiry(base::Days(30)).Build();

  storage()->StoreSource(impression2);
  storage()->StoreSource(impression3);

  EXPECT_EQ(CreateReportStatus::kSuccess,
            MaybeCreateAndStoreReport(DefaultTrigger()));

  // Only the first impression should overlap with this time range, but all the
  // impressions should share the origin.
  storage()->ClearData(start, start,
                       GetMatcher(impression1.impression_origin()));
  EXPECT_THAT(storage()->GetAttributionsToReport(base::Time::Max()), SizeIs(1));
}

// The max time range with a null filter should delete everything.
TEST_F(AttributionStorageTest, DeleteAll) {
  base::Time start = base::Time::Now();
  for (int i = 0; i < 10; i++) {
    auto impression = SourceBuilder(start).SetExpiry(base::Days(30)).Build();
    storage()->StoreSource(impression);
    task_environment_.FastForwardBy(base::Days(1));
  }

  EXPECT_EQ(CreateReportStatus::kSuccess,
            MaybeCreateAndStoreReport(DefaultTrigger()));
  task_environment_.FastForwardBy(base::Days(1));
  EXPECT_EQ(CreateReportStatus::kSuccess,
            MaybeCreateAndStoreReport(DefaultTrigger()));

  auto null_filter = base::RepeatingCallback<bool(const url::Origin&)>();
  storage()->ClearData(base::Time::Min(), base::Time::Max(), null_filter);

  // Verify that everything is deleted.
  EXPECT_THAT(storage()->GetAttributionsToReport(base::Time::Max()), IsEmpty());
}

// Same as the above test, but uses base::Time() instead of base::Time::Min()
// for delete_begin, which should yield the same behavior.
TEST_F(AttributionStorageTest, DeleteAllNullDeleteBegin) {
  base::Time start = base::Time::Now();
  for (int i = 0; i < 10; i++) {
    auto impression = SourceBuilder(start).SetExpiry(base::Days(30)).Build();
    storage()->StoreSource(impression);
    task_environment_.FastForwardBy(base::Days(1));
  }

  EXPECT_EQ(CreateReportStatus::kSuccess,
            MaybeCreateAndStoreReport(DefaultTrigger()));
  task_environment_.FastForwardBy(base::Days(1));
  EXPECT_EQ(CreateReportStatus::kSuccess,
            MaybeCreateAndStoreReport(DefaultTrigger()));

  auto null_filter = base::RepeatingCallback<bool(const url::Origin&)>();
  storage()->ClearData(base::Time(), base::Time::Max(), null_filter);

  // Verify that everything is deleted.
  EXPECT_THAT(storage()->GetAttributionsToReport(base::Time::Max()), IsEmpty());
}

TEST_F(AttributionStorageTest, MaxAttributionReportsBetweenSites) {
  delegate()->set_rate_limits({
      .time_window = base::TimeDelta::Max(),
      .max_contributions_per_window = 2,
  });

  auto impression = SourceBuilder().Build();
  auto conversion = DefaultTrigger();

  storage()->StoreSource(SourceBuilder().Build());
  EXPECT_EQ(CreateReportStatus::kSuccess,
            MaybeCreateAndStoreReport(conversion));
  EXPECT_EQ(CreateReportStatus::kSuccess,
            MaybeCreateAndStoreReport(conversion));
  EXPECT_THAT(storage()->MaybeCreateAndStoreReport(conversion),
              AllOf(Property(&CreateReportResult::status,
                             CreateReportStatus::kRateLimited),
                    Property(&CreateReportResult::dropped_report, IsTrue())));

  const EventAttributionReport expected_report =
      GetExpectedReport(impression, conversion);

  EXPECT_THAT(storage()->GetAttributionsToReport(base::Time::Max()),
              ElementsAre(expected_report, expected_report));
}

TEST_F(AttributionStorageTest,
       MaxAttributionReportsBetweenSites_RespectsSourceType) {
  delegate()->set_rate_limits({
      .time_window = base::TimeDelta::Max(),
      .max_contributions_per_window = 1,
  });

  storage()->StoreSource(
      SourceBuilder()
          .SetSourceType(StorableSource::SourceType::kNavigation)
          .Build());
  EXPECT_EQ(CreateReportStatus::kSuccess,
            MaybeCreateAndStoreReport(DefaultTrigger()));

  storage()->StoreSource(SourceBuilder()
                             .SetSourceType(StorableSource::SourceType::kEvent)
                             .Build());
  // This would fail if the source types had a combined limit or the incorrect
  // source type were stored.
  EXPECT_EQ(CreateReportStatus::kSuccess,
            MaybeCreateAndStoreReport(DefaultTrigger()));

  storage()->StoreSource(SourceBuilder()
                             .SetSourceType(StorableSource::SourceType::kEvent)
                             .Build());
  EXPECT_EQ(CreateReportStatus::kRateLimited,
            MaybeCreateAndStoreReport(DefaultTrigger()));

  storage()->StoreSource(
      SourceBuilder()
          .SetSourceType(StorableSource::SourceType::kNavigation)
          .Build());
  EXPECT_EQ(CreateReportStatus::kRateLimited,
            MaybeCreateAndStoreReport(DefaultTrigger()));
}

TEST_F(AttributionStorageTest, NeverAttributeImpression_ReportNotStored) {
  delegate()->set_max_attributions_per_source(1);
  storage()->StoreSource(
      SourceBuilder()
          .SetAttributionLogic(StorableSource::AttributionLogic::kNever)
          .Build());

  EXPECT_EQ(CreateReportStatus::kDroppedForNoise,
            MaybeCreateAndStoreReport(DefaultTrigger()));

  task_environment_.FastForwardBy(base::Milliseconds(kReportTime));

  EXPECT_THAT(storage()->GetAttributionsToReport(base::Time::Now()), IsEmpty());
}

TEST_F(AttributionStorageTest, NeverAttributeImpression_Deactivates) {
  delegate()->set_max_attributions_per_source(1);
  storage()->StoreSource(
      SourceBuilder()
          .SetSourceEventId(3)
          .SetAttributionLogic(StorableSource::AttributionLogic::kNever)
          .Build());

  EXPECT_EQ(CreateReportStatus::kDroppedForNoise,
            MaybeCreateAndStoreReport(DefaultTrigger()));

  storage()->StoreSource(SourceBuilder().SetSourceEventId(5).Build());

  EXPECT_EQ(
      CreateReportStatus::kSuccess,
      MaybeCreateAndStoreReport(TriggerBuilder().SetTriggerData(7).Build()));

  task_environment_.FastForwardBy(base::Milliseconds(kReportTime));

  EXPECT_THAT(storage()->GetAttributionsToReport(base::Time::Now()),
              ElementsAre(AllOf(
                  Property(&EventAttributionReport::source,
                           Property(&StorableSource::source_event_id, 5u)),
                  Property(&EventAttributionReport::trigger_data, 7u))));
}

TEST_F(AttributionStorageTest, NeverAttributeImpression_RateLimitsNotChanged) {
  delegate()->set_rate_limits({
      .time_window = base::TimeDelta::Max(),
      .max_contributions_per_window = 1,
  });

  storage()->StoreSource(
      SourceBuilder()
          .SetSourceEventId(5)
          .SetAttributionLogic(StorableSource::AttributionLogic::kNever)
          .Build());

  const auto conversion = DefaultTrigger();
  EXPECT_EQ(CreateReportStatus::kDroppedForNoise,
            MaybeCreateAndStoreReport(conversion));

  const auto impression = SourceBuilder().SetSourceEventId(7).Build();
  storage()->StoreSource(impression);
  EXPECT_EQ(CreateReportStatus::kSuccess,
            MaybeCreateAndStoreReport(conversion));

  storage()->StoreSource(SourceBuilder().SetSourceEventId(9).Build());
  EXPECT_EQ(CreateReportStatus::kRateLimited,
            MaybeCreateAndStoreReport(conversion));

  const EventAttributionReport expected_report =
      GetExpectedReport(impression, conversion);

  task_environment_.FastForwardBy(base::Milliseconds(kReportTime));

  EXPECT_THAT(storage()->GetAttributionsToReport(base::Time::Now()),
              ElementsAre(expected_report));
}

TEST_F(AttributionStorageTest,
       NeverAndTruthfullyAttributeImpressions_ReportNotStored) {
  storage()->StoreSource(SourceBuilder().Build());

  task_environment_.FastForwardBy(base::Milliseconds(1));

  storage()->StoreSource(
      SourceBuilder()
          .SetAttributionLogic(StorableSource::AttributionLogic::kNever)
          .Build());

  const auto conversion = DefaultTrigger();
  EXPECT_EQ(CreateReportStatus::kDroppedForNoise,
            MaybeCreateAndStoreReport(conversion));
  EXPECT_EQ(CreateReportStatus::kDroppedForNoise,
            MaybeCreateAndStoreReport(conversion));

  task_environment_.FastForwardBy(base::Milliseconds(kReportTime));

  EXPECT_THAT(storage()->GetAttributionsToReport(base::Time::Now()), IsEmpty());
}

TEST_F(AttributionStorageTest,
       MaxAttributionDestinationsPerSource_AlreadyStored) {
  const auto impression =
      SourceBuilder().SetSourceType(StorableSource::SourceType::kEvent).Build();

  // Setting this doesn't affect the test behavior, but makes it clear that the
  // test passes without depending on the default value of |INT_MAX|.
  delegate()->set_max_attribution_destinations_per_event_source(1);
  storage()->StoreSource(impression);
  storage()->StoreSource(impression);

  // The second impression's |conversion_destination| matches the first's, so it
  // doesn't add to the number of distinct values for the |impression_site|,
  // and both impressions should be stored.
  EXPECT_THAT(storage()->GetActiveSources(), SizeIs(2));
}

TEST_F(
    AttributionStorageTest,
    MaxAttributionDestinationsPerSource_DifferentImpressionSitesAreIndependent) {
  // Setting this doesn't affect the test behavior, but makes it clear that the
  // test passes without depending on the default value of |INT_MAX|.
  delegate()->set_max_attribution_destinations_per_event_source(1);
  storage()->StoreSource(
      SourceBuilder()
          .SetImpressionOrigin(url::Origin::Create(GURL("https://a.example")))
          .SetConversionOrigin(url::Origin::Create(GURL("https://c.example")))
          .SetSourceType(StorableSource::SourceType::kEvent)
          .Build());
  storage()->StoreSource(
      SourceBuilder()
          .SetImpressionOrigin(url::Origin::Create(GURL("https://b.example")))
          .SetConversionOrigin(url::Origin::Create(GURL("https://d.example")))
          .SetSourceType(StorableSource::SourceType::kEvent)
          .Build());

  // The two impressions together have 2 distinct |conversion_destination|
  // values, but they are independent because they vary by |impression_site|, so
  // both should be stored.
  EXPECT_THAT(storage()->GetActiveSources(), SizeIs(2));
}

TEST_F(AttributionStorageTest,
       MaxAttributionDestinationsPerSource_IrrelevantForNavigationSources) {
  // Setting this doesn't affect the test behavior, but makes it clear that the
  // test passes without depending on the default value of |INT_MAX|.
  delegate()->set_max_attribution_destinations_per_event_source(1);
  storage()->StoreSource(
      SourceBuilder()
          .SetConversionOrigin(url::Origin::Create(GURL("https://a.example/")))
          .Build());
  storage()->StoreSource(
      SourceBuilder()
          .SetConversionOrigin(url::Origin::Create(GURL("https://b.example")))
          .Build());

  // Both impressions should be stored because they are navigation source.
  EXPECT_THAT(storage()->GetActiveSources(), SizeIs(2));
}

TEST_F(
    AttributionStorageTest,
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
    storage()->StoreSource(
        SourceBuilder()
            .SetImpressionOrigin(
                url::Origin::Create(GURL(impression.impression_origin)))
            .SetConversionOrigin(
                url::Origin::Create(GURL(impression.conversion_origin)))
            .SetSourceType(StorableSource::SourceType::kEvent)
            .Build());
    task_environment_.FastForwardBy(base::Milliseconds(1));
  }

  EXPECT_THAT(
      storage()->GetActiveSources(),
      ElementsAre(
          Property(&StorableSource::impression_origin,
                   url::Origin::Create(GURL("https://ghi.test.example"))),
          Property(&StorableSource::impression_origin,
                   url::Origin::Create(GURL("https://qrs.test.example")))));
}

TEST_F(AttributionStorageTest,
       MaxAttributionDestinationsPerSource_IgnoresInactiveImpressions) {
  delegate()->set_max_attributions_per_source(1);
  delegate()->set_max_attribution_destinations_per_event_source(INT_MAX);

  storage()->StoreSource(SourceBuilder()
                             .SetSourceType(StorableSource::SourceType::kEvent)
                             .Build());
  EXPECT_THAT(storage()->GetActiveSources(), SizeIs(1));

  EXPECT_EQ(CreateReportStatus::kSuccess,
            MaybeCreateAndStoreReport(DefaultTrigger()));
  EXPECT_THAT(storage()->GetActiveSources(), SizeIs(1));

  // Force the impression to be deactivated by ensuring that the next report is
  // in a different window.
  delegate()->set_report_time_ms(kReportTime + 1);
  EXPECT_EQ(CreateReportStatus::kPriorityTooLow,
            MaybeCreateAndStoreReport(DefaultTrigger()));
  EXPECT_THAT(storage()->GetActiveSources(), IsEmpty());

  task_environment_.FastForwardBy(base::Milliseconds(1));
  storage()->StoreSource(
      SourceBuilder()
          .SetConversionOrigin(url::Origin::Create(GURL("https://a.example")))
          .SetSourceType(StorableSource::SourceType::kEvent)
          .Build());
  EXPECT_THAT(storage()->GetActiveSources(), SizeIs(1));

  delegate()->set_max_attribution_destinations_per_event_source(1);
  task_environment_.FastForwardBy(base::Milliseconds(1));
  storage()->StoreSource(
      SourceBuilder()
          .SetConversionOrigin(url::Origin::Create(GURL("https://b.example")))
          .SetSourceType(StorableSource::SourceType::kEvent)
          .Build());

  // The earliest active impression should be deleted to make room for this new
  // one.
  EXPECT_THAT(
      storage()->GetActiveSources(),
      ElementsAre(Property(&StorableSource::conversion_origin,
                           url::Origin::Create(GURL("https://b.example")))));

  // Both the inactive impression and the new one for b.example should be
  // retained; a.example is the only one that should have been deleted. The
  // presence of 1 conversion to report implies that the inactive impression
  // remains in the DB.
  task_environment_.FastForwardBy(base::Milliseconds(kReportTime));
  EXPECT_THAT(storage()->GetAttributionsToReport(base::Time::Now()), SizeIs(1));
}

TEST_F(AttributionStorageTest,
       MultipleImpressionsPerConversion_MostRecentAttributesForSamePriority) {
  storage()->StoreSource(SourceBuilder().SetSourceEventId(3).Build());

  task_environment_.FastForwardBy(base::Milliseconds(1));
  storage()->StoreSource(SourceBuilder().SetSourceEventId(7).Build());

  task_environment_.FastForwardBy(base::Milliseconds(1));
  storage()->StoreSource(SourceBuilder().SetSourceEventId(5).Build());

  EXPECT_THAT(storage()->GetActiveSources(), SizeIs(3));
  EXPECT_EQ(CreateReportStatus::kSuccess,
            MaybeCreateAndStoreReport(DefaultTrigger()));

  task_environment_.FastForwardBy(base::Milliseconds(kReportTime));

  EXPECT_THAT(
      storage()->GetAttributionsToReport(base::Time::Now()),
      ElementsAre(Property(&EventAttributionReport::source,
                           Property(&StorableSource::source_event_id, 5u))));
}

TEST_F(AttributionStorageTest,
       MultipleImpressionsPerConversion_HighestPriorityAttributes) {
  storage()->StoreSource(
      SourceBuilder().SetPriority(100).SetSourceEventId(3).Build());

  task_environment_.FastForwardBy(base::Milliseconds(1));
  storage()->StoreSource(
      SourceBuilder().SetPriority(300).SetSourceEventId(5).Build());

  task_environment_.FastForwardBy(base::Milliseconds(1));
  storage()->StoreSource(
      SourceBuilder().SetPriority(200).SetSourceEventId(7).Build());

  EXPECT_THAT(storage()->GetActiveSources(), SizeIs(3));
  EXPECT_EQ(CreateReportStatus::kSuccess,
            MaybeCreateAndStoreReport(DefaultTrigger()));

  task_environment_.FastForwardBy(base::Milliseconds(kReportTime));

  EXPECT_THAT(
      storage()->GetAttributionsToReport(base::Time::Now()),
      ElementsAre(Property(&EventAttributionReport::source,
                           Property(&StorableSource::source_event_id, 5u))));
}

TEST_F(AttributionStorageTest, MultipleImpressions_CorrectDeactivation) {
  storage()->StoreSource(
      SourceBuilder().SetSourceEventId(3).SetPriority(0).Build());
  storage()->StoreSource(
      SourceBuilder().SetSourceEventId(5).SetPriority(1).Build());
  EXPECT_THAT(storage()->GetActiveSources(), SizeIs(2));

  EXPECT_EQ(CreateReportStatus::kSuccess,
            MaybeCreateAndStoreReport(DefaultTrigger()));

  // Because the impression with data 5 has the highest priority, it is selected
  // for attribution. The unselected impression with data 3 should be
  // deactivated, but the one with data 5 should remain active.
  EXPECT_THAT(storage()->GetActiveSources(),
              ElementsAre(Property(&StorableSource::source_event_id, 5u)));
}

TEST_F(AttributionStorageTest, FalselyAttributeImpression_ReportStored) {
  delegate()->set_fake_event_source_trigger_data(7);
  delegate()->set_max_attributions_per_source(1);

  const auto impression =
      SourceBuilder()
          .SetSourceEventId(4)
          .SetSourceType(StorableSource::SourceType::kEvent)
          .SetPriority(100)
          .SetAttributionLogic(StorableSource::AttributionLogic::kFalsely)
          .Build();
  storage()->StoreSource(impression);

  const EventAttributionReport expected_report =
      ReportBuilder(impression)
          .SetTriggerData(7)
          .SetConversionTime(base::Time::Now())
          .SetReportTime(base::Time::Now() + base::Milliseconds(kReportTime))
          .Build();

  task_environment_.FastForwardBy(base::Milliseconds(kReportTime));

  EXPECT_THAT(storage()->GetAttributionsToReport(base::Time::Now()),
              ElementsAre(expected_report));

  EXPECT_THAT(storage()->GetActiveSources(), IsEmpty());

  // The falsely attributed impression should not be eligible for further
  // attribution.
  EXPECT_EQ(CreateReportStatus::kNoMatchingImpressions,
            MaybeCreateAndStoreReport(DefaultTrigger()));

  EXPECT_THAT(storage()->GetAttributionsToReport(base::Time::Now()),
              ElementsAre(expected_report));
}

TEST_F(AttributionStorageTest, TriggerPriority) {
  delegate()->set_max_attributions_per_source(1);

  storage()->StoreSource(
      SourceBuilder().SetSourceEventId(3).SetPriority(0).Build());
  storage()->StoreSource(
      SourceBuilder().SetSourceEventId(5).SetPriority(1).Build());

  EXPECT_THAT(
      storage()->MaybeCreateAndStoreReport(
          TriggerBuilder().SetPriority(0).SetTriggerData(20).Build()),
      AllOf(Property(&CreateReportResult::status, CreateReportStatus::kSuccess),
            Property(&CreateReportResult::dropped_report, absl::nullopt)));

  // This conversion should replace the one above because it has a higher
  // priority.
  EXPECT_THAT(
      storage()->MaybeCreateAndStoreReport(
          TriggerBuilder().SetPriority(2).SetTriggerData(21).Build()),
      AllOf(Property(&CreateReportResult::status,
                     CreateReportStatus::kSuccessDroppedLowerPriority),
            Property(&CreateReportResult::dropped_report,
                     Optional(Property(&EventAttributionReport::trigger_data,
                                       20u)))));

  storage()->StoreSource(
      SourceBuilder().SetSourceEventId(7).SetPriority(2).Build());

  EXPECT_EQ(CreateReportStatus::kSuccess,
            MaybeCreateAndStoreReport(
                TriggerBuilder().SetPriority(1).SetTriggerData(22).Build()));
  // This conversion should be dropped because it has a lower priority than the
  // one above.
  EXPECT_THAT(
      storage()->MaybeCreateAndStoreReport(
          TriggerBuilder().SetPriority(0).SetTriggerData(23).Build()),
      AllOf(Property(&CreateReportResult::status,
                     CreateReportStatus::kPriorityTooLow),
            Property(&CreateReportResult::dropped_report,
                     Optional(Property(&EventAttributionReport::trigger_data,
                                       23u)))));

  task_environment_.FastForwardBy(base::Milliseconds(kReportTime));

  EXPECT_THAT(
      storage()->GetAttributionsToReport(base::Time::Now()),
      ElementsAre(
          AllOf(Property(&EventAttributionReport::source,
                         Property(&StorableSource::source_event_id, 5u)),
                Property(&EventAttributionReport::trigger_data, 21u)),
          AllOf(Property(&EventAttributionReport::source,
                         Property(&StorableSource::source_event_id, 7u)),
                Property(&EventAttributionReport::trigger_data, 22u))));
}

TEST_F(AttributionStorageTest, TriggerPriority_Simple) {
  delegate()->set_max_attributions_per_source(1);

  storage()->StoreSource(SourceBuilder().Build());

  int i = 0;
  EXPECT_EQ(CreateReportStatus::kSuccess,
            MaybeCreateAndStoreReport(
                TriggerBuilder().SetPriority(i).SetTriggerData(i).Build()));
  i++;

  for (; i < 10; i++) {
    EXPECT_EQ(CreateReportStatus::kSuccessDroppedLowerPriority,
              MaybeCreateAndStoreReport(
                  TriggerBuilder().SetPriority(i).SetTriggerData(i).Build()));
  }

  task_environment_.FastForwardBy(base::Milliseconds(kReportTime));

  EXPECT_THAT(storage()->GetAttributionsToReport(base::Time::Now()),
              ElementsAre(Property(&EventAttributionReport::trigger_data, 9u)));
}

TEST_F(AttributionStorageTest, TriggerPriority_SamePriorityDeletesMostRecent) {
  delegate()->set_max_attributions_per_source(2);

  storage()->StoreSource(SourceBuilder().Build());

  EXPECT_EQ(CreateReportStatus::kSuccess,
            MaybeCreateAndStoreReport(
                TriggerBuilder().SetPriority(1).SetTriggerData(3).Build()));

  task_environment_.FastForwardBy(base::Milliseconds(1));
  EXPECT_EQ(CreateReportStatus::kSuccess,
            MaybeCreateAndStoreReport(
                TriggerBuilder().SetPriority(1).SetTriggerData(2).Build()));

  // This report should not be stored, as even though it has the same priority
  // as the previous two, it is the most recent.
  task_environment_.FastForwardBy(base::Milliseconds(1));
  EXPECT_EQ(CreateReportStatus::kPriorityTooLow,
            MaybeCreateAndStoreReport(
                TriggerBuilder().SetPriority(1).SetTriggerData(8).Build()));

  // This report should be stored by replacing the one with `trigger_data ==
  // 2`, which is the most recent of the two with `priority == 1`.
  task_environment_.FastForwardBy(base::Milliseconds(1));
  EXPECT_EQ(CreateReportStatus::kSuccessDroppedLowerPriority,
            MaybeCreateAndStoreReport(
                TriggerBuilder().SetPriority(2).SetTriggerData(5).Build()));

  EXPECT_THAT(storage()->GetAttributionsToReport(base::Time::Max()),
              ElementsAre(Property(&EventAttributionReport::trigger_data, 3u),
                          Property(&EventAttributionReport::trigger_data, 5u)));
}

TEST_F(AttributionStorageTest, TriggerPriority_DeactivatesImpression) {
  delegate()->set_max_attributions_per_source(1);

  storage()->StoreSource(
      SourceBuilder().SetSourceEventId(3).SetPriority(0).Build());
  storage()->StoreSource(
      SourceBuilder().SetSourceEventId(5).SetPriority(1).Build());
  EXPECT_THAT(storage()->GetActiveSources(), SizeIs(2));

  EXPECT_EQ(CreateReportStatus::kSuccess,
            MaybeCreateAndStoreReport(DefaultTrigger()));

  // Because the impression with data 5 has the highest priority, it is selected
  // for attribution. The unselected impression with data 3 should be
  // deactivated, but the one with data 5 should remain active.
  EXPECT_THAT(storage()->GetActiveSources(),
              ElementsAre(Property(&StorableSource::source_event_id, 5u)));

  // Ensure that the next report is in a different window.
  delegate()->set_report_time_ms(kReportTime + 1);

  // This conversion should not be stored because all reports for the attributed
  // impression were in an earlier window.
  EXPECT_EQ(CreateReportStatus::kPriorityTooLow,
            MaybeCreateAndStoreReport(TriggerBuilder().SetPriority(2).Build()));

  // As a result, the impression with data 5 should also be deactivated.
  EXPECT_THAT(storage()->GetActiveSources(), IsEmpty());
}

TEST_F(AttributionStorageTest, DedupKey_Dedups) {
  storage()->StoreSource(
      SourceBuilder()
          .SetSourceEventId(1)
          .SetConversionOrigin(url::Origin::Create(GURL("https://a.example")))
          .Build());
  storage()->StoreSource(
      SourceBuilder()
          .SetSourceEventId(2)
          .SetConversionOrigin(url::Origin::Create(GURL("https://b.example")))
          .Build());
  EXPECT_THAT(storage()->GetActiveSources(),
              ElementsAre(Property(&StorableSource::dedup_keys, IsEmpty()),
                          Property(&StorableSource::dedup_keys, IsEmpty())));

  EXPECT_EQ(CreateReportStatus::kSuccess,
            MaybeCreateAndStoreReport(
                TriggerBuilder()
                    .SetConversionDestination(net::SchemefulSite(
                        url::Origin::Create(GURL("https://a.example"))))
                    .SetDedupKey(11)
                    .SetTriggerData(71)
                    .Build()));

  // Should be stored because dedup key doesn't match even though conversion
  // destination does.
  EXPECT_EQ(CreateReportStatus::kSuccess,
            MaybeCreateAndStoreReport(
                TriggerBuilder()
                    .SetConversionDestination(net::SchemefulSite(
                        url::Origin::Create(GURL("https://a.example"))))
                    .SetDedupKey(12)
                    .SetTriggerData(72)
                    .Build()));

  // Should be stored because conversion destination doesn't match even though
  // dedup key does.
  EXPECT_EQ(CreateReportStatus::kSuccess,
            MaybeCreateAndStoreReport(
                TriggerBuilder()
                    .SetConversionDestination(net::SchemefulSite(
                        url::Origin::Create(GURL("https://b.example"))))
                    .SetDedupKey(12)
                    .SetTriggerData(73)
                    .Build()));

  // Shouldn't be stored because conversion destination and dedup key match.
  EXPECT_EQ(CreateReportStatus::kDeduplicated,
            MaybeCreateAndStoreReport(
                TriggerBuilder()
                    .SetConversionDestination(net::SchemefulSite(
                        url::Origin::Create(GURL("https://a.example"))))
                    .SetDedupKey(11)
                    .SetTriggerData(74)
                    .Build()));

  // Shouldn't be stored because conversion destination and dedup key match.
  EXPECT_EQ(CreateReportStatus::kDeduplicated,
            MaybeCreateAndStoreReport(
                TriggerBuilder()
                    .SetConversionDestination(net::SchemefulSite(
                        url::Origin::Create(GURL("https://b.example"))))
                    .SetDedupKey(12)
                    .SetTriggerData(75)
                    .Build()));

  task_environment_.FastForwardBy(base::Milliseconds(kReportTime));
  EXPECT_THAT(
      storage()->GetAttributionsToReport(base::Time::Now()),
      ElementsAre(Property(&EventAttributionReport::trigger_data, 71u),
                  Property(&EventAttributionReport::trigger_data, 72u),
                  Property(&EventAttributionReport::trigger_data, 73u)));

  EXPECT_THAT(
      storage()->GetActiveSources(),
      ElementsAre(Property(&StorableSource::dedup_keys, ElementsAre(11, 12)),
                  Property(&StorableSource::dedup_keys, ElementsAre(12))));
}

TEST_F(AttributionStorageTest, DedupKey_DedupsAfterConversionDeletion) {
  storage()->StoreSource(
      SourceBuilder()
          .SetSourceEventId(1)
          .SetConversionOrigin(url::Origin::Create(GURL("https://a.example")))
          .Build());
  EXPECT_THAT(storage()->GetActiveSources(), SizeIs(1));

  task_environment_.FastForwardBy(base::Milliseconds(1));

  EXPECT_EQ(CreateReportStatus::kSuccess,
            MaybeCreateAndStoreReport(
                TriggerBuilder()
                    .SetConversionDestination(net::SchemefulSite(
                        url::Origin::Create(GURL("https://a.example"))))
                    .SetDedupKey(2)
                    .SetTriggerData(3)
                    .Build()));

  task_environment_.FastForwardBy(base::Milliseconds(kReportTime));

  std::vector<EventAttributionReport> actual_reports =
      storage()->GetAttributionsToReport(base::Time::Now());
  EXPECT_THAT(actual_reports,
              ElementsAre(Property(&EventAttributionReport::trigger_data, 3u)));

  // Simulate the report being sent and deleted from storage.
  DeleteReports(actual_reports);

  task_environment_.FastForwardBy(base::Milliseconds(1));

  // This report shouldn't be stored, as it should be deduped against the
  // previously stored one even though that previous one is no longer in the DB.
  EXPECT_EQ(CreateReportStatus::kDeduplicated,
            MaybeCreateAndStoreReport(
                TriggerBuilder()
                    .SetConversionDestination(net::SchemefulSite(
                        url::Origin::Create(GURL("https://a.example"))))
                    .SetDedupKey(2)
                    .SetTriggerData(5)
                    .Build()));

  task_environment_.FastForwardBy(base::Milliseconds(kReportTime));
  EXPECT_THAT(storage()->GetAttributionsToReport(base::Time::Now()), IsEmpty());
}

TEST_F(AttributionStorageTest, GetAttributionsToReport_SetsPriority) {
  storage()->StoreSource(SourceBuilder().Build());
  EXPECT_EQ(
      CreateReportStatus::kSuccess,
      MaybeCreateAndStoreReport(TriggerBuilder().SetPriority(13).Build()));

  task_environment_.FastForwardBy(base::Milliseconds(kReportTime));

  EXPECT_THAT(storage()->GetAttributionsToReport(base::Time::Now()),
              ElementsAre(Property(&EventAttributionReport::priority, 13)));
}

TEST_F(AttributionStorageTest, NoIDReuse_Impression) {
  storage()->StoreSource(SourceBuilder().Build());
  auto sources = storage()->GetActiveSources();
  EXPECT_THAT(sources,
              ElementsAre(Property(&StorableSource::impression_id, IsTrue())));
  const StorableSource::Id id1 = *sources.front().impression_id();

  storage()->ClearData(base::Time::Min(), base::Time::Max(),
                       base::NullCallback());
  EXPECT_THAT(storage()->GetActiveSources(), IsEmpty());

  storage()->StoreSource(SourceBuilder().Build());
  sources = storage()->GetActiveSources();
  EXPECT_THAT(sources,
              ElementsAre(Property(&StorableSource::impression_id, IsTrue())));
  const StorableSource::Id id2 = *sources.front().impression_id();

  EXPECT_NE(id1, id2);
}

TEST_F(AttributionStorageTest, NoIDReuse_Conversion) {
  storage()->StoreSource(SourceBuilder().Build());
  EXPECT_EQ(CreateReportStatus::kSuccess,
            MaybeCreateAndStoreReport(DefaultTrigger()));
  auto reports = storage()->GetAttributionsToReport(base::Time::Max());
  EXPECT_THAT(reports, ElementsAre(Property(&EventAttributionReport::report_id,
                                            IsTrue())));
  const EventAttributionReport::Id id1 = *reports.front().report_id();

  storage()->ClearData(base::Time::Min(), base::Time::Max(),
                       base::NullCallback());
  EXPECT_THAT(storage()->GetAttributionsToReport(base::Time::Max()), IsEmpty());

  storage()->StoreSource(SourceBuilder().Build());
  EXPECT_EQ(CreateReportStatus::kSuccess,
            MaybeCreateAndStoreReport(DefaultTrigger()));
  reports = storage()->GetAttributionsToReport(base::Time::Max());
  EXPECT_THAT(reports, ElementsAre(Property(&EventAttributionReport::report_id,
                                            IsTrue())));
  const EventAttributionReport::Id id2 = *reports.front().report_id();

  EXPECT_NE(id1, id2);
}

TEST_F(AttributionStorageTest, UpdateReportForSendFailure) {
  storage()->StoreSource(SourceBuilder().Build());
  EXPECT_EQ(CreateReportStatus::kSuccess,
            MaybeCreateAndStoreReport(DefaultTrigger()));

  task_environment_.FastForwardBy(base::Milliseconds(kReportTime));

  std::vector<EventAttributionReport> actual_reports =
      storage()->GetAttributionsToReport(base::Time::Now());
  EXPECT_THAT(
      actual_reports,
      ElementsAre(Property(&EventAttributionReport::failed_send_attempts, 0)));

  const base::TimeDelta delay = base::Days(2);
  const base::Time new_report_time = actual_reports[0].report_time() + delay;
  EXPECT_TRUE(storage()->UpdateReportForSendFailure(
      *actual_reports[0].report_id(), new_report_time));

  task_environment_.FastForwardBy(delay);

  EXPECT_THAT(
      storage()->GetAttributionsToReport(base::Time::Now()),
      ElementsAre(AllOf(
          Property(&EventAttributionReport::failed_send_attempts, 1),
          Property(&EventAttributionReport::report_time, new_report_time))));
}

TEST_F(AttributionStorageTest, StoreSource_ReturnsDeactivatedSources) {
  auto source1 = SourceBuilder().SetSourceEventId(7).Build();
  EXPECT_THAT(storage()->StoreSource(source1), IsEmpty());
  EXPECT_THAT(storage()->GetActiveSources(), SizeIs(1));

  task_environment_.FastForwardBy(base::Milliseconds(kReportTime));

  // Set a dedup key to ensure that the return deactivated source contains it.
  EXPECT_EQ(
      CreateReportStatus::kSuccess,
      MaybeCreateAndStoreReport(TriggerBuilder().SetDedupKey(13).Build()));
  EXPECT_THAT(storage()->GetAttributionsToReport(base::Time::Now()), SizeIs(1));

  auto source2 = SourceBuilder().SetSourceEventId(9).Build();

  source1.SetDedupKeys({13});
  EXPECT_THAT(storage()->StoreSource(source2),
              ElementsAre(DeactivatedSource(
                  source1, DeactivatedSource::Reason::kReplacedByNewerSource)));

  EXPECT_THAT(storage()->GetActiveSources(), ElementsAre(source2));
}

TEST_F(AttributionStorageTest, StoreSource_ReturnsDeactivatedSources_Limited) {
  auto source1 = SourceBuilder().SetSourceEventId(1).Build();
  EXPECT_THAT(storage()->StoreSource(source1), IsEmpty());

  auto source2 = SourceBuilder().SetSourceEventId(2).Build();
  EXPECT_THAT(storage()->StoreSource(source1), IsEmpty());

  EXPECT_THAT(storage()->GetActiveSources(), SizeIs(2));

  task_environment_.FastForwardBy(base::Milliseconds(kReportTime));

  EXPECT_EQ(CreateReportStatus::kSuccess,
            MaybeCreateAndStoreReport(DefaultTrigger()));
  EXPECT_EQ(CreateReportStatus::kSuccess,
            MaybeCreateAndStoreReport(DefaultTrigger()));
  EXPECT_THAT(storage()->GetAttributionsToReport(base::Time::Now()), SizeIs(2));

  // 2 sources are deactivated, but only 1 should be returned.
  auto source3 = SourceBuilder().SetSourceEventId(3).Build();
  EXPECT_THAT(
      storage()->StoreSource(source3, /*deactivated_source_return_limit=*/1),
      ElementsAre(DeactivatedSource(
          source1, DeactivatedSource::Reason::kReplacedByNewerSource)));
  EXPECT_THAT(storage()->GetActiveSources(), ElementsAre(source3));
}

TEST_F(AttributionStorageTest,
       MaybeCreateAndStoreReport_ReturnsDeactivatedSources) {
  auto source1 = SourceBuilder().SetSourceEventId(7).Build();
  EXPECT_THAT(storage()->StoreSource(source1), IsEmpty());
  EXPECT_THAT(storage()->GetActiveSources(), SizeIs(1));

  // Store the maximum number of reports for the source.
  for (size_t i = 1; i <= kMaxConversions; i++) {
    EXPECT_EQ(CreateReportStatus::kSuccess,
              MaybeCreateAndStoreReport(DefaultTrigger()));
  }

  task_environment_.FastForwardBy(base::Milliseconds(kReportTime));
  auto reports = storage()->GetAttributionsToReport(base::Time::Now());
  EXPECT_THAT(reports, SizeIs(3));

  // Simulate the reports being sent and removed from storage.
  DeleteReports(reports);

  // The next report should cause the source to be deactivated; the report
  // itself shouldn't be stored as we've already reached the maximum number of
  // conversions per source.
  EXPECT_THAT(
      storage()->MaybeCreateAndStoreReport(DefaultTrigger()),
      AllOf(
          Property(&CreateReportResult::status,
                   CreateReportStatus::kPriorityTooLow),
          Property(
              &CreateReportResult::dropped_report,
              Optional(Property(&EventAttributionReport::source, source1))),
          Property(&CreateReportResult::GetDeactivatedSource,
                   DeactivatedSource(
                       source1,
                       DeactivatedSource::Reason::kReachedAttributionLimit))));
}

TEST_F(AttributionStorageTest, ReportID_RoundTrips) {
  storage()->StoreSource(SourceBuilder().Build());
  EXPECT_EQ(CreateReportStatus::kSuccess,
            MaybeCreateAndStoreReport(DefaultTrigger()));

  task_environment_.FastForwardBy(base::Milliseconds(kReportTime));

  std::vector<EventAttributionReport> actual_reports =
      storage()->GetAttributionsToReport(base::Time::Now());
  EXPECT_EQ(1u, actual_reports.size());
  EXPECT_EQ(DefaultExternalReportID(), actual_reports[0].external_report_id());
}

TEST_F(AttributionStorageTest, AdjustOfflineReportTimes) {
  EXPECT_EQ(
      storage()->AdjustOfflineReportTimes(base::TimeDelta(), base::TimeDelta()),
      absl::nullopt);

  storage()->StoreSource(SourceBuilder().Build());
  EXPECT_EQ(CreateReportStatus::kSuccess,
            MaybeCreateAndStoreReport(DefaultTrigger()));

  const base::Time original_report_time =
      base::Time::Now() + base::Milliseconds(kReportTime);

  EXPECT_THAT(storage()->GetAttributionsToReport(base::Time::Max()),
              ElementsAre(Property(&EventAttributionReport::report_time,
                                   original_report_time)));

  task_environment_.FastForwardBy(base::Milliseconds(kReportTime));

  EXPECT_EQ(storage()->AdjustOfflineReportTimes(/*min_delay=*/base::Hours(1),
                                                /*max_delay=*/base::Hours(1)),
            original_report_time);

  // The report time should not be changed as it is equal to now, not strictly
  // less than it.
  EXPECT_THAT(storage()->GetAttributionsToReport(base::Time::Max()),
              ElementsAre(Property(&EventAttributionReport::report_time,
                                   original_report_time)));

  task_environment_.FastForwardBy(base::Milliseconds(1));

  const base::Time new_report_time = base::Time::Now() + base::Hours(1);

  EXPECT_EQ(storage()->AdjustOfflineReportTimes(/*min_delay=*/base::Hours(1),
                                                /*max_delay=*/base::Hours(1)),
            new_report_time);

  // The report time should be changed as it is strictly less than now.
  EXPECT_THAT(storage()->GetAttributionsToReport(base::Time::Max()),
              ElementsAre(Property(&EventAttributionReport::report_time,
                                   new_report_time)));
}

TEST_F(AttributionStorageTest, AdjustOfflineReportTimes_Range) {
  storage()->StoreSource(SourceBuilder().Build());
  EXPECT_EQ(CreateReportStatus::kSuccess,
            MaybeCreateAndStoreReport(DefaultTrigger()));

  const base::Time original_report_time =
      base::Time::Now() + base::Milliseconds(kReportTime);

  EXPECT_THAT(storage()->GetAttributionsToReport(base::Time::Max()),
              ElementsAre(Property(&EventAttributionReport::report_time,
                                   original_report_time)));

  task_environment_.FastForwardBy(base::Milliseconds(kReportTime + 1));

  storage()->AdjustOfflineReportTimes(/*min_delay=*/base::Hours(1),
                                      /*max_delay=*/base::Hours(3));

  EXPECT_THAT(
      storage()->GetAttributionsToReport(base::Time::Max()),
      ElementsAre(Property(&EventAttributionReport::report_time,
                           AllOf(Ge(base::Time::Now() + base::Hours(1)),
                                 Le(base::Time::Now() + base::Hours(3))))));
}

TEST_F(AttributionStorageTest, GetNextReportTime) {
  const auto origin_a = url::Origin::Create(GURL("https://a.example/"));
  const auto origin_b = url::Origin::Create(GURL("https://b.example/"));

  EXPECT_EQ(storage()->GetNextReportTime(base::Time::Min()), absl::nullopt);

  storage()->StoreSource(SourceBuilder().SetReportingOrigin(origin_a).Build());
  EXPECT_EQ(CreateReportStatus::kSuccess,
            MaybeCreateAndStoreReport(
                TriggerBuilder().SetReportingOrigin(origin_a).Build()));

  const base::Time report_time_a =
      base::Time::Now() + base::Milliseconds(kReportTime);

  EXPECT_EQ(storage()->GetNextReportTime(base::Time::Min()), report_time_a);
  EXPECT_EQ(storage()->GetNextReportTime(report_time_a), absl::nullopt);

  task_environment_.FastForwardBy(base::Milliseconds(1));
  storage()->StoreSource(SourceBuilder().SetReportingOrigin(origin_b).Build());
  EXPECT_EQ(CreateReportStatus::kSuccess,
            MaybeCreateAndStoreReport(
                TriggerBuilder().SetReportingOrigin(origin_b).Build()));

  const base::Time report_time_b =
      base::Time::Now() + base::Milliseconds(kReportTime);

  EXPECT_EQ(storage()->GetNextReportTime(base::Time::Min()), report_time_a);
  EXPECT_EQ(storage()->GetNextReportTime(report_time_a), report_time_b);
  EXPECT_EQ(storage()->GetNextReportTime(report_time_b), absl::nullopt);
}

}  // namespace content
