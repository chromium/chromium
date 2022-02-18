// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/attribution_storage.h"

#include <stdint.h>

#include <functional>
#include <limits>
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
#include "content/browser/attribution_reporting/aggregatable_attribution.h"
#include "content/browser/attribution_reporting/attribution_report.h"
#include "content/browser/attribution_reporting/attribution_storage_sql.h"
#include "content/browser/attribution_reporting/attribution_test_utils.h"
#include "content/browser/attribution_reporting/attribution_trigger.h"
#include "content/browser/attribution_reporting/common_source_info.h"
#include "content/browser/attribution_reporting/storable_source.h"
#include "content/browser/attribution_reporting/stored_source.h"
#include "content/public/common/url_constants.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"
#include "url/url_util.h"

namespace content {

namespace {

using DeactivatedSource = ::content::AttributionStorage::DeactivatedSource;

using ::testing::AllOf;
using ::testing::ElementsAre;
using ::testing::Ge;
using ::testing::IsEmpty;
using ::testing::IsTrue;
using ::testing::Le;
using ::testing::Optional;
using ::testing::Property;
using ::testing::SizeIs;

// Default max number of conversions for a single impression for testing.
const int kMaxConversions = 3;

// Default delay for when a report should be sent for testing.
constexpr base::TimeDelta kReportDelay = base::Milliseconds(5);

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
    delegate->set_report_delay(kReportDelay);
    delegate->set_max_attributions_per_source(kMaxConversions);
    delegate_ = delegate.get();
    storage_ = std::make_unique<AttributionStorageSql>(dir_.GetPath(),
                                                       std::move(delegate));
  }

  // Given a |conversion|, returns the expected conversion report properties at
  // the current timestamp.
  AttributionReport GetExpectedReport(const StoredSource& source,
                                      const AttributionTrigger& conversion) {
    return ReportBuilder(AttributionInfoBuilder(source)
                             .SetTime(base::Time::Now())
                             .Build())
        .SetTriggerData(conversion.trigger_data())
        .SetReportTime(source.common_info().impression_time() + kReportDelay)
        .SetPriority(conversion.priority())
        .Build();
  }

  AttributionTrigger::Result MaybeCreateAndStoreReport(
      const AttributionTrigger& conversion) {
    return storage_->MaybeCreateAndStoreReport(conversion).status();
  }

  void DeleteReports(const std::vector<AttributionReport>& reports) {
    for (const auto& report : reports) {
      EXPECT_TRUE(storage_->DeleteReport(
          *(absl::get<AttributionReport::EventLevelData>(report.data()).id)));
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
  EXPECT_EQ(AttributionTrigger::Result::kNoMatchingImpressions,
            storage->MaybeCreateAndStoreReport(DefaultTrigger()).status());
  EXPECT_THAT(storage->GetAttributionsToReport(base::Time::Now()), IsEmpty());
  EXPECT_THAT(storage->GetActiveSources(), IsEmpty());
  EXPECT_TRUE(storage->DeleteReport(AttributionReport::EventLevelData::Id(0)));
  EXPECT_NO_FATAL_FAILURE(storage->ClearData(
      base::Time::Min(), base::Time::Max(), base::NullCallback()));
  EXPECT_EQ(storage->AdjustOfflineReportTimes(), absl::nullopt);
}

TEST_F(AttributionStorageTest, ImpressionStoredAndRetrieved_ValuesIdentical) {
  auto impression = SourceBuilder().Build();
  storage()->StoreSource(impression);
  EXPECT_THAT(storage()->GetActiveSources(),
              ElementsAre(CommonSourceInfoIs(impression.common_info())));
}

#if BUILDFLAG(IS_ANDROID)
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
  EXPECT_THAT(storage()->GetActiveSources(),
              ElementsAre(CommonSourceInfoIs(impression.common_info())));
}
#endif

TEST_F(AttributionStorageTest,
       GetWithNoMatchingImpressions_NoImpressionsReturned) {
  EXPECT_EQ(AttributionTrigger::Result::kNoMatchingImpressions,
            MaybeCreateAndStoreReport(DefaultTrigger()));
  EXPECT_THAT(storage()->GetAttributionsToReport(base::Time::Now()), IsEmpty());
}

TEST_F(AttributionStorageTest, GetWithMatchingImpression_ImpressionReturned) {
  storage()->StoreSource(SourceBuilder().Build());
  EXPECT_EQ(AttributionTrigger::Result::kSuccess,
            MaybeCreateAndStoreReport(DefaultTrigger()));
}

TEST_F(AttributionStorageTest, MultipleImpressionsForConversion_OneConverts) {
  storage()->StoreSource(SourceBuilder().Build());
  storage()->StoreSource(SourceBuilder().Build());
  EXPECT_EQ(AttributionTrigger::Result::kSuccess,
            MaybeCreateAndStoreReport(DefaultTrigger()));
}

TEST_F(AttributionStorageTest,
       CrossOriginSameDomainConversion_ImpressionConverted) {
  auto impression =
      SourceBuilder()
          .SetConversionOrigin(url::Origin::Create(GURL("https://sub.a.test")))
          .Build();
  storage()->StoreSource(impression);
  EXPECT_EQ(
      AttributionTrigger::Result::kSuccess,
      MaybeCreateAndStoreReport(
          TriggerBuilder()
              .SetConversionDestination(
                  net::SchemefulSite(GURL("https://a.test")))
              .SetReportingOrigin(impression.common_info().reporting_origin())
              .Build()));
}

TEST_F(AttributionStorageTest, EventSourceImpressionsForConversion_Converts) {
  storage()->StoreSource(
      SourceBuilder()
          .SetSourceType(CommonSourceInfo::SourceType::kEvent)
          .Build());
  EXPECT_EQ(AttributionTrigger::Result::kSuccess,
            MaybeCreateAndStoreReport(
                TriggerBuilder().SetEventSourceTriggerData(456).Build()));

  task_environment_.FastForwardBy(kReportDelay);

  EXPECT_THAT(storage()->GetAttributionsToReport(base::Time::Now()),
              ElementsAre(EventLevelDataIs(TriggerDataIs(456u))));
}

TEST_F(AttributionStorageTest, ImpressionExpired_NoConversionsStored) {
  storage()->StoreSource(
      SourceBuilder().SetExpiry(base::Milliseconds(2)).Build());
  task_environment_.FastForwardBy(base::Milliseconds(2));

  EXPECT_EQ(AttributionTrigger::Result::kNoMatchingImpressions,
            MaybeCreateAndStoreReport(DefaultTrigger()));
}

TEST_F(AttributionStorageTest, ImpressionExpired_ConversionsStoredPrior) {
  storage()->StoreSource(
      SourceBuilder().SetExpiry(base::Milliseconds(4)).Build());

  task_environment_.FastForwardBy(base::Milliseconds(3));

  EXPECT_EQ(AttributionTrigger::Result::kSuccess,
            MaybeCreateAndStoreReport(DefaultTrigger()));

  task_environment_.FastForwardBy(base::Milliseconds(5));

  EXPECT_EQ(AttributionTrigger::Result::kNoMatchingImpressions,
            MaybeCreateAndStoreReport(DefaultTrigger()));
}

TEST_F(AttributionStorageTest,
       ImpressionWithMaxConversions_ConversionReportNotStored) {
  storage()->StoreSource(SourceBuilder().Build());

  for (int i = 0; i < kMaxConversions; i++) {
    EXPECT_EQ(AttributionTrigger::Result::kSuccess,
              MaybeCreateAndStoreReport(DefaultTrigger()));
  }

  // No additional conversion reports should be created.
  EXPECT_THAT(
      storage()->MaybeCreateAndStoreReport(DefaultTrigger()),
      AllOf(CreateReportStatusIs(AttributionTrigger::Result::kPriorityTooLow),
            DroppedReportIs(IsTrue()), DeactivatedSourceIs(absl::nullopt)));
}

TEST_F(AttributionStorageTest, OneConversion_OneReportScheduled) {
  auto conversion = DefaultTrigger();

  storage()->StoreSource(SourceBuilder().Build());
  EXPECT_EQ(AttributionTrigger::Result::kSuccess,
            MaybeCreateAndStoreReport(conversion));

  AttributionReport expected_report =
      GetExpectedReport(SourceBuilder().BuildStored(), conversion);

  task_environment_.FastForwardBy(kReportDelay);

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
  EXPECT_EQ(AttributionTrigger::Result::kNoMatchingImpressions,
            MaybeCreateAndStoreReport(DefaultTrigger()));

  task_environment_.FastForwardBy(kReportDelay);

  EXPECT_THAT(storage()->GetAttributionsToReport(base::Time::Now()), IsEmpty());
}

TEST_F(AttributionStorageTest,
       ConversionWithDifferentConversionOrigin_NoReportScheduled) {
  auto impression = SourceBuilder()
                        .SetConversionOrigin(
                            url::Origin::Create(GURL("https://different.test")))
                        .Build();
  storage()->StoreSource(impression);
  EXPECT_EQ(AttributionTrigger::Result::kNoMatchingImpressions,
            MaybeCreateAndStoreReport(DefaultTrigger()));

  task_environment_.FastForwardBy(kReportDelay);

  EXPECT_THAT(storage()->GetAttributionsToReport(base::Time::Now()), IsEmpty());
}

TEST_F(AttributionStorageTest, ConversionReportDeleted_RemovedFromStorage) {
  storage()->StoreSource(SourceBuilder().Build());
  EXPECT_EQ(AttributionTrigger::Result::kSuccess,
            MaybeCreateAndStoreReport(DefaultTrigger()));

  task_environment_.FastForwardBy(kReportDelay);

  std::vector<AttributionReport> reports =
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
    EXPECT_EQ(AttributionTrigger::Result::kSuccess,
              MaybeCreateAndStoreReport(DefaultTrigger()));
  }

  // No additional conversion reports should be created for any of the
  // impressions.
  EXPECT_EQ(AttributionTrigger::Result::kPriorityTooLow,
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
  EXPECT_EQ(AttributionTrigger::Result::kSuccess,
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
  EXPECT_EQ(AttributionTrigger::Result::kSuccess,
            MaybeCreateAndStoreReport(DefaultTrigger()));

  task_environment_.FastForwardBy(kReportDelay);

  // Delete the report.
  DeleteReports(storage()->GetAttributionsToReport(base::Time::Now()));

  // Store a new impression that should mark the first inactive.
  SourceBuilder builder;
  builder.SetSourceEventId(1000);
  storage()->StoreSource(builder.Build());

  // Only the new impression should convert.
  auto conversion = DefaultTrigger();
  EXPECT_EQ(AttributionTrigger::Result::kSuccess,
            MaybeCreateAndStoreReport(conversion));
  AttributionReport expected_report =
      GetExpectedReport(builder.BuildStored(), conversion);

  task_environment_.FastForwardBy(kReportDelay);

  // Verify it was the new impression that converted.
  EXPECT_THAT(storage()->GetAttributionsToReport(base::Time::Now()),
              ElementsAre(expected_report));
}

TEST_F(AttributionStorageTest,
       NonMatchingImpressionForConvertedImpression_FirstRemainsActive) {
  SourceBuilder builder;
  storage()->StoreSource(builder.Build());

  auto conversion = DefaultTrigger();
  EXPECT_EQ(AttributionTrigger::Result::kSuccess,
            MaybeCreateAndStoreReport(DefaultTrigger()));

  task_environment_.FastForwardBy(kReportDelay);

  // Delete the report.
  DeleteReports(storage()->GetAttributionsToReport(base::Time::Now()));

  // Store a new impression with a different reporting origin.
  storage()->StoreSource(SourceBuilder()
                             .SetReportingOrigin(url::Origin::Create(
                                 GURL("https://different.test")))
                             .Build());

  // The first impression should still be active and able to convert.
  EXPECT_EQ(AttributionTrigger::Result::kSuccess,
            MaybeCreateAndStoreReport(conversion));

  AttributionReport expected_report =
      GetExpectedReport(builder.BuildStored(), conversion);

  // Verify it was the first impression that converted.
  EXPECT_THAT(storage()->GetAttributionsToReport(base::Time::Now()),
              ElementsAre(expected_report));
}

TEST_F(
    AttributionStorageTest,
    MultipleImpressionsForConversionAtDifferentTimes_OneImpressionAttributed) {
  storage()->StoreSource(SourceBuilder().Build());
  storage()->StoreSource(SourceBuilder().Build());

  auto conversion = DefaultTrigger();

  // Advance clock so third impression is stored at a different timestamp.
  task_environment_.FastForwardBy(base::Milliseconds(3));

  // Make a conversion with different impression data.
  SourceBuilder builder;
  builder.SetSourceEventId(10);
  storage()->StoreSource(builder.Build());

  AttributionReport third_expected_conversion =
      GetExpectedReport(builder.BuildStored(), conversion);
  EXPECT_EQ(AttributionTrigger::Result::kSuccess,
            MaybeCreateAndStoreReport(conversion));

  task_environment_.FastForwardBy(kReportDelay);

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

  EXPECT_EQ(AttributionTrigger::Result::kSuccess,
            MaybeCreateAndStoreReport(DefaultTrigger()));

  // Advance to the first impression's report time and verify only its report is
  // available.
  task_environment_.FastForwardBy(kReportDelay - base::Milliseconds(1));
  EXPECT_THAT(storage()->GetAttributionsToReport(base::Time::Now()), IsEmpty());

  task_environment_.FastForwardBy(base::Milliseconds(1));
  EXPECT_THAT(storage()->GetAttributionsToReport(base::Time::Now()), SizeIs(1));
}

TEST_F(AttributionStorageTest,
       GetAttributionsToReportMultipleTimes_SameResult) {
  storage()->StoreSource(SourceBuilder().Build());
  EXPECT_EQ(AttributionTrigger::Result::kSuccess,
            MaybeCreateAndStoreReport(DefaultTrigger()));
  task_environment_.FastForwardBy(kReportDelay);

  std::vector<AttributionReport> first_call_reports =
      storage()->GetAttributionsToReport(base::Time::Now());
  std::vector<AttributionReport> second_call_reports =
      storage()->GetAttributionsToReport(base::Time::Now());

  // Expect that |GetAttributionsToReport()| did not delete any conversions.
  EXPECT_EQ(first_call_reports, second_call_reports);
}

TEST_F(AttributionStorageTest, MaxImpressionsPerOrigin_LimitsStorage) {
  delegate()->set_max_sources_per_origin(2);
  storage()->StoreSource(SourceBuilder().SetSourceEventId(3).Build());
  storage()->StoreSource(SourceBuilder().SetSourceEventId(5).Build());
  EXPECT_EQ(storage()
                ->StoreSource(SourceBuilder().SetSourceEventId(7).Build())
                .status,
            StorableSource::Result::kInsufficientSourceCapacity);

  EXPECT_THAT(storage()->GetActiveSources(),
              ElementsAre(SourceEventIdIs(3u), SourceEventIdIs(5u)));
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
              ElementsAre(SourceEventIdIs(3u), SourceEventIdIs(5u),
                          SourceEventIdIs(7u)));

  // This impression shouldn't be stored, because its origin has already hit the
  // limit of 2.
  EXPECT_EQ(storage()
                ->StoreSource(SourceBuilder()
                                  .SetImpressionOrigin(url::Origin::Create(
                                      GURL("https://foo.a.example")))
                                  .SetSourceEventId(9)
                                  .Build())
                .status,
            StorableSource::Result::kInsufficientSourceCapacity);

  // This impression should be stored, because its origin hasn't hit the limit
  // of 2.
  storage()->StoreSource(SourceBuilder()
                             .SetImpressionOrigin(url::Origin::Create(
                                 GURL("https://bar.a.example")))
                             .SetSourceEventId(11)
                             .Build());

  EXPECT_THAT(storage()->GetActiveSources(),
              ElementsAre(SourceEventIdIs(3u), SourceEventIdIs(5u),
                          SourceEventIdIs(7u), SourceEventIdIs(11u)));
}

TEST_F(AttributionStorageTest, MaxConversionsPerOrigin) {
  delegate()->set_max_attributions_per_origin(1);
  storage()->StoreSource(SourceBuilder().Build());
  storage()->StoreSource(SourceBuilder().Build());
  EXPECT_EQ(AttributionTrigger::Result::kSuccess,
            MaybeCreateAndStoreReport(DefaultTrigger()));
  // Verify that MaxConversionsPerOrigin is enforced.
  auto result = storage()->MaybeCreateAndStoreReport(
      TriggerBuilder().SetTriggerData(5).Build());
  EXPECT_EQ(AttributionTrigger::Result::kNoCapacityForConversionDestination,
            result.status());
  EXPECT_THAT(result.dropped_report(),
              Optional(EventLevelDataIs(TriggerDataIs(5))));
}

TEST_F(AttributionStorageTest, ClearDataWithNoMatch_NoDelete) {
  base::Time now = base::Time::Now();
  auto impression = SourceBuilder(now).Build();
  storage()->StoreSource(impression);
  storage()->ClearData(
      now, now, GetMatcher(url::Origin::Create(GURL("https://no-match.com"))));
  EXPECT_EQ(AttributionTrigger::Result::kSuccess,
            MaybeCreateAndStoreReport(DefaultTrigger()));
}

TEST_F(AttributionStorageTest, ClearDataOutsideRange_NoDelete) {
  base::Time now = base::Time::Now();
  auto impression = SourceBuilder(now).Build();
  storage()->StoreSource(impression);

  storage()->ClearData(
      now + base::Minutes(10), now + base::Minutes(20),
      GetMatcher(impression.common_info().impression_origin()));
  EXPECT_EQ(AttributionTrigger::Result::kSuccess,
            MaybeCreateAndStoreReport(DefaultTrigger()));
}

TEST_F(AttributionStorageTest, ClearDataImpression) {
  base::Time now = base::Time::Now();

  {
    auto impression = SourceBuilder(now).Build();
    storage()->StoreSource(impression);
    storage()->ClearData(
        now, now + base::Minutes(20),
        GetMatcher(impression.common_info().conversion_origin()));
    EXPECT_EQ(AttributionTrigger::Result::kNoMatchingImpressions,
              MaybeCreateAndStoreReport(DefaultTrigger()));
  }
}

TEST_F(AttributionStorageTest, ClearDataImpressionConversion) {
  base::Time now = base::Time::Now();
  auto impression = SourceBuilder(now).Build();
  auto conversion = DefaultTrigger();

  storage()->StoreSource(impression);
  EXPECT_EQ(AttributionTrigger::Result::kSuccess,
            MaybeCreateAndStoreReport(conversion));

  storage()->ClearData(
      now - base::Minutes(20), now + base::Minutes(20),
      GetMatcher(impression.common_info().impression_origin()));

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
    EXPECT_EQ(AttributionTrigger::Result::kSuccess,
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
    EXPECT_EQ(AttributionTrigger::Result::kSuccess,
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

  EXPECT_EQ(AttributionTrigger::Result::kSuccess,
            MaybeCreateAndStoreReport(conversion));
  storage()->ClearData(
      base::Time::Now(), base::Time::Now(),
      GetMatcher(impression.common_info().impression_origin()));
  EXPECT_THAT(storage()->GetAttributionsToReport(base::Time::Max()), IsEmpty());
}

// Deletions with time range between the impression and conversion should not
// delete anything, unless the time range intersects one of the events.
TEST_F(AttributionStorageTest, ClearDataRangeBetweenEvents) {
  base::Time start = base::Time::Now();

  SourceBuilder builder;
  builder.SetExpiry(base::Days(30)).Build();

  auto impression = builder.Build();
  auto conversion = DefaultTrigger();

  storage()->StoreSource(impression);

  task_environment_.FastForwardBy(base::Days(1));

  const AttributionReport expected_report =
      GetExpectedReport(builder.BuildStored(), conversion);

  EXPECT_EQ(AttributionTrigger::Result::kSuccess,
            MaybeCreateAndStoreReport(conversion));

  storage()->ClearData(
      start + base::Minutes(1), start + base::Minutes(10),
      GetMatcher(impression.common_info().impression_origin()));

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

  EXPECT_EQ(AttributionTrigger::Result::kSuccess,
            MaybeCreateAndStoreReport(DefaultTrigger()));

  // Only the first impression should overlap with this time range, but all the
  // impressions should share the origin.
  storage()->ClearData(
      start, start, GetMatcher(impression1.common_info().impression_origin()));
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

  EXPECT_EQ(AttributionTrigger::Result::kSuccess,
            MaybeCreateAndStoreReport(DefaultTrigger()));
  task_environment_.FastForwardBy(base::Days(1));
  EXPECT_EQ(AttributionTrigger::Result::kSuccess,
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

  EXPECT_EQ(AttributionTrigger::Result::kSuccess,
            MaybeCreateAndStoreReport(DefaultTrigger()));
  task_environment_.FastForwardBy(base::Days(1));
  EXPECT_EQ(AttributionTrigger::Result::kSuccess,
            MaybeCreateAndStoreReport(DefaultTrigger()));

  auto null_filter = base::RepeatingCallback<bool(const url::Origin&)>();
  storage()->ClearData(base::Time(), base::Time::Max(), null_filter);

  // Verify that everything is deleted.
  EXPECT_THAT(storage()->GetAttributionsToReport(base::Time::Max()), IsEmpty());
}

TEST_F(AttributionStorageTest, MaxAttributionReportsBetweenSites) {
  delegate()->set_rate_limits({
      .time_window = base::TimeDelta::Max(),
      .max_source_registration_reporting_origins =
          std::numeric_limits<int64_t>::max(),
      .max_attribution_reporting_origins = std::numeric_limits<int64_t>::max(),
      .max_attributions = 2,
  });

  auto conversion = DefaultTrigger();

  storage()->StoreSource(SourceBuilder().Build());
  EXPECT_EQ(AttributionTrigger::Result::kSuccess,
            MaybeCreateAndStoreReport(conversion));
  EXPECT_EQ(AttributionTrigger::Result::kSuccess,
            MaybeCreateAndStoreReport(conversion));
  EXPECT_THAT(storage()->MaybeCreateAndStoreReport(conversion),
              AllOf(CreateReportStatusIs(
                        AttributionTrigger::Result::kExcessiveAttributions),
                    DroppedReportIs(IsTrue())));

  const AttributionReport expected_report =
      GetExpectedReport(SourceBuilder().BuildStored(), conversion);

  EXPECT_THAT(storage()->GetAttributionsToReport(base::Time::Max()),
              ElementsAre(expected_report, expected_report));
}

TEST_F(AttributionStorageTest,
       MaxAttributionReportsBetweenSites_IgnoresSourceType) {
  delegate()->set_rate_limits({
      .time_window = base::TimeDelta::Max(),
      .max_source_registration_reporting_origins =
          std::numeric_limits<int64_t>::max(),
      .max_attribution_reporting_origins = std::numeric_limits<int64_t>::max(),
      .max_attributions = 1,
  });

  storage()->StoreSource(
      SourceBuilder()
          .SetSourceType(CommonSourceInfo::SourceType::kNavigation)
          .Build());
  EXPECT_EQ(AttributionTrigger::Result::kSuccess,
            MaybeCreateAndStoreReport(DefaultTrigger()));

  storage()->StoreSource(
      SourceBuilder()
          .SetSourceType(CommonSourceInfo::SourceType::kEvent)
          .Build());
  // This would fail if the source types had separate limits.
  EXPECT_EQ(AttributionTrigger::Result::kExcessiveAttributions,
            MaybeCreateAndStoreReport(DefaultTrigger()));
}

TEST_F(AttributionStorageTest, NeverAttributeImpression_ReportNotStored) {
  delegate()->set_max_attributions_per_source(1);

  delegate()->set_randomized_response(
      std::vector<AttributionStorageDelegate::FakeReport>{});
  storage()->StoreSource(SourceBuilder().Build());
  delegate()->set_randomized_response(absl::nullopt);

  EXPECT_EQ(AttributionTrigger::Result::kDroppedForNoise,
            MaybeCreateAndStoreReport(DefaultTrigger()));

  task_environment_.FastForwardBy(kReportDelay);

  EXPECT_THAT(storage()->GetAttributionsToReport(base::Time::Now()), IsEmpty());
}

TEST_F(AttributionStorageTest, NeverAttributeImpression_Deactivates) {
  delegate()->set_max_attributions_per_source(1);

  delegate()->set_randomized_response(
      std::vector<AttributionStorageDelegate::FakeReport>{});
  storage()->StoreSource(SourceBuilder().SetSourceEventId(3).Build());
  delegate()->set_randomized_response(absl::nullopt);

  EXPECT_EQ(AttributionTrigger::Result::kDroppedForNoise,
            MaybeCreateAndStoreReport(DefaultTrigger()));

  storage()->StoreSource(SourceBuilder().SetSourceEventId(5).Build());

  EXPECT_EQ(
      AttributionTrigger::Result::kSuccess,
      MaybeCreateAndStoreReport(TriggerBuilder().SetTriggerData(7).Build()));

  task_environment_.FastForwardBy(kReportDelay);

  EXPECT_THAT(storage()->GetAttributionsToReport(base::Time::Now()),
              ElementsAre(AllOf(ReportSourceIs(SourceEventIdIs(5u)),
                                EventLevelDataIs(TriggerDataIs(7u)))));
}

TEST_F(AttributionStorageTest, NeverAttributeImpression_RateLimitsNotChanged) {
  delegate()->set_rate_limits({
      .time_window = base::TimeDelta::Max(),
      .max_source_registration_reporting_origins =
          std::numeric_limits<int64_t>::max(),
      .max_attribution_reporting_origins = std::numeric_limits<int64_t>::max(),
      .max_attributions = 1,
  });

  delegate()->set_randomized_response(
      std::vector<AttributionStorageDelegate::FakeReport>{});
  storage()->StoreSource(SourceBuilder().SetSourceEventId(5).Build());
  delegate()->set_randomized_response(absl::nullopt);

  const auto conversion = DefaultTrigger();
  EXPECT_EQ(AttributionTrigger::Result::kDroppedForNoise,
            MaybeCreateAndStoreReport(conversion));

  SourceBuilder builder;
  builder.SetSourceEventId(7);
  storage()->StoreSource(builder.Build());
  EXPECT_EQ(AttributionTrigger::Result::kSuccess,
            MaybeCreateAndStoreReport(conversion));

  storage()->StoreSource(SourceBuilder().SetSourceEventId(9).Build());
  EXPECT_EQ(AttributionTrigger::Result::kExcessiveAttributions,
            MaybeCreateAndStoreReport(conversion));

  const AttributionReport expected_report =
      GetExpectedReport(builder.BuildStored(), conversion);

  task_environment_.FastForwardBy(kReportDelay);

  EXPECT_THAT(storage()->GetAttributionsToReport(base::Time::Now()),
              ElementsAre(expected_report));
}

TEST_F(AttributionStorageTest,
       NeverAndTruthfullyAttributeImpressions_ReportNotStored) {
  storage()->StoreSource(SourceBuilder().Build());

  task_environment_.FastForwardBy(base::Milliseconds(1));

  delegate()->set_randomized_response(
      std::vector<AttributionStorageDelegate::FakeReport>{});
  storage()->StoreSource(SourceBuilder().Build());
  delegate()->set_randomized_response(absl::nullopt);

  const auto conversion = DefaultTrigger();
  EXPECT_EQ(AttributionTrigger::Result::kDroppedForNoise,
            MaybeCreateAndStoreReport(conversion));
  EXPECT_EQ(AttributionTrigger::Result::kDroppedForNoise,
            MaybeCreateAndStoreReport(conversion));

  task_environment_.FastForwardBy(kReportDelay);

  EXPECT_THAT(storage()->GetAttributionsToReport(base::Time::Now()), IsEmpty());
}

TEST_F(AttributionStorageTest,
       MaxDestinationsPerSource_ScopedToSourceSiteAndReportingOrigin) {
  delegate()->set_max_destinations_per_source_site_reporting_origin(3);

  const auto store_source = [&](const char* impression_origin,
                                const char* reporting_origin,
                                const char* destination_origin) {
    return storage()
        ->StoreSource(
            SourceBuilder()
                .SetImpressionOrigin(
                    url::Origin::Create(GURL(impression_origin)))
                .SetReportingOrigin(url::Origin::Create(GURL(reporting_origin)))
                .SetConversionOrigin(
                    url::Origin::Create(GURL(destination_origin)))
                .SetExpiry(base::Days(30))
                .Build())
        .status;
  };

  store_source("https://s1.test", "https://a.r.test", "https://d1.test");
  store_source("https://s1.test", "https://a.r.test", "https://d2.test");
  store_source("https://s1.test", "https://a.r.test", "https://d3.test");
  EXPECT_THAT(storage()->GetActiveSources(), SizeIs(3));

  // This should succeed because the destination is already present on a pending
  // source.
  store_source("https://s1.test", "https://a.r.test", "https://d2.test");
  EXPECT_THAT(storage()->GetActiveSources(), SizeIs(4));

  // This should fail because there are already 3 distinct destinations.
  EXPECT_EQ(
      store_source("https://s1.test", "https://a.r.test", "https://d4.test"),
      StorableSource::Result::kInsufficientUniqueDestinationCapacity);
  EXPECT_THAT(storage()->GetActiveSources(), SizeIs(4));

  // This should succeed because the source site is different.
  store_source("https://s2.test", "https://a.r.test", "https://d5.test");
  EXPECT_THAT(storage()->GetActiveSources(), SizeIs(5));

  // This should succeed because the reporting origin is different.
  store_source("https://s1.test", "https://b.r.test", "https://d5.test");
  EXPECT_THAT(storage()->GetActiveSources(), SizeIs(6));
}

TEST_F(AttributionStorageTest,
       MaxAttributionDestinationsPerSource_AppliesToNavigationSources) {
  delegate()->set_max_destinations_per_source_site_reporting_origin(1);
  storage()->StoreSource(
      SourceBuilder()
          .SetConversionOrigin(url::Origin::Create(GURL("https://a.example/")))
          .Build());
  storage()->StoreSource(
      SourceBuilder()
          .SetConversionOrigin(url::Origin::Create(GURL("https://b.example")))
          .Build());

  EXPECT_THAT(storage()->GetActiveSources(), SizeIs(1));
}

TEST_F(AttributionStorageTest,
       MaxAttributionDestinationsPerSource_CountsAllSourceTypes) {
  delegate()->set_max_destinations_per_source_site_reporting_origin(1);
  storage()->StoreSource(
      SourceBuilder()
          .SetConversionOrigin(url::Origin::Create(GURL("https://a.example/")))
          .SetSourceType(CommonSourceInfo::SourceType::kNavigation)
          .Build());
  storage()->StoreSource(
      SourceBuilder()
          .SetConversionOrigin(url::Origin::Create(GURL("https://b.example")))
          .SetSourceType(CommonSourceInfo::SourceType::kEvent)
          .Build());

  EXPECT_THAT(storage()->GetActiveSources(), SizeIs(1));
}

TEST_F(AttributionStorageTest,
       MaxAttributionDestinationsPerSource_IgnoresInactiveImpressions) {
  delegate()->set_max_attributions_per_source(1);
  delegate()->set_max_destinations_per_source_site_reporting_origin(INT_MAX);

  const auto origin_a = url::Origin::Create(GURL("https://a.example"));

  storage()->StoreSource(SourceBuilder().SetConversionOrigin(origin_a).Build());
  EXPECT_THAT(storage()->GetActiveSources(), SizeIs(1));

  const auto trigger =
      TriggerBuilder()
          .SetConversionDestination(net::SchemefulSite(origin_a))
          .Build();

  EXPECT_EQ(AttributionTrigger::Result::kSuccess,
            MaybeCreateAndStoreReport(trigger));
  EXPECT_THAT(storage()->GetActiveSources(), SizeIs(1));

  // Force the impression to be deactivated by ensuring that the next report is
  // in a different window.
  delegate()->set_report_delay(kReportDelay + base::Milliseconds(1));
  EXPECT_EQ(AttributionTrigger::Result::kPriorityTooLow,
            MaybeCreateAndStoreReport(trigger));
  EXPECT_THAT(storage()->GetActiveSources(), IsEmpty());

  delegate()->set_max_destinations_per_source_site_reporting_origin(1);
  storage()->StoreSource(
      SourceBuilder()
          .SetConversionOrigin(url::Origin::Create(GURL("https://b.example")))
          .Build());

  EXPECT_THAT(storage()->GetActiveSources(),
              ElementsAre(ConversionOriginIs(
                  url::Origin::Create(GURL("https://b.example")))));
}

TEST_F(AttributionStorageTest,
       MultipleImpressionsPerConversion_MostRecentAttributesForSamePriority) {
  storage()->StoreSource(SourceBuilder().SetSourceEventId(3).Build());

  task_environment_.FastForwardBy(base::Milliseconds(1));
  storage()->StoreSource(SourceBuilder().SetSourceEventId(7).Build());

  task_environment_.FastForwardBy(base::Milliseconds(1));
  storage()->StoreSource(SourceBuilder().SetSourceEventId(5).Build());

  EXPECT_THAT(storage()->GetActiveSources(), SizeIs(3));
  EXPECT_EQ(AttributionTrigger::Result::kSuccess,
            MaybeCreateAndStoreReport(DefaultTrigger()));

  task_environment_.FastForwardBy(kReportDelay);

  EXPECT_THAT(storage()->GetAttributionsToReport(base::Time::Now()),
              ElementsAre(ReportSourceIs(SourceEventIdIs(5u))));
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
  EXPECT_EQ(AttributionTrigger::Result::kSuccess,
            MaybeCreateAndStoreReport(DefaultTrigger()));

  task_environment_.FastForwardBy(kReportDelay);

  EXPECT_THAT(storage()->GetAttributionsToReport(base::Time::Now()),
              ElementsAre(ReportSourceIs(SourceEventIdIs(5u))));
}

TEST_F(AttributionStorageTest, MultipleImpressions_CorrectDeactivation) {
  storage()->StoreSource(
      SourceBuilder().SetSourceEventId(3).SetPriority(0).Build());
  storage()->StoreSource(
      SourceBuilder().SetSourceEventId(5).SetPriority(1).Build());
  EXPECT_THAT(storage()->GetActiveSources(), SizeIs(2));

  EXPECT_EQ(AttributionTrigger::Result::kSuccess,
            MaybeCreateAndStoreReport(DefaultTrigger()));

  // Because the impression with data 5 has the highest priority, it is selected
  // for attribution. The unselected impression with data 3 should be
  // deactivated, but the one with data 5 should remain active.
  EXPECT_THAT(storage()->GetActiveSources(), ElementsAre(SourceEventIdIs(5u)));
}

TEST_F(AttributionStorageTest, FalselyAttributeImpression_ReportStored) {
  delegate()->set_max_attributions_per_source(1);

  const base::Time fake_report_time = base::Time::Now() + kReportDelay;

  SourceBuilder builder;
  builder.SetSourceEventId(4)
      .SetSourceType(CommonSourceInfo::SourceType::kEvent)
      .SetPriority(100);
  delegate()->set_randomized_response(
      std::vector<AttributionStorageDelegate::FakeReport>{
          {.trigger_data = 7, .report_time = fake_report_time}});
  storage()->StoreSource(builder.Build());
  delegate()->set_randomized_response(absl::nullopt);

  const AttributionReport expected_report =
      ReportBuilder(
          AttributionInfoBuilder(
              builder
                  .SetAttributionLogic(StoredSource::AttributionLogic::kFalsely)
                  .BuildStored())
              .SetTime(base::Time::Now())
              .Build())
          .SetTriggerData(7)
          .SetReportTime(fake_report_time)
          .Build();

  task_environment_.FastForwardBy(kReportDelay);

  EXPECT_THAT(storage()->GetAttributionsToReport(base::Time::Now()),
              ElementsAre(expected_report));

  EXPECT_THAT(storage()->GetActiveSources(), IsEmpty());

  // The falsely attributed impression should not be eligible for further
  // attribution.
  EXPECT_EQ(AttributionTrigger::Result::kNoMatchingImpressions,
            MaybeCreateAndStoreReport(DefaultTrigger()));

  EXPECT_THAT(storage()->GetAttributionsToReport(base::Time::Now()),
              ElementsAre(expected_report));
}

TEST_F(AttributionStorageTest, StoreSource_ReturnsMinFakeReportTime) {
  const base::Time now = base::Time::Now();

  const struct {
    AttributionStorageDelegate::RandomizedResponse randomized_response;
    absl::optional<base::Time> expected;
  } kTestCases[] = {
      {absl::nullopt, absl::nullopt},
      {std::vector<AttributionStorageDelegate::FakeReport>(), absl::nullopt},
      {std::vector<AttributionStorageDelegate::FakeReport>{
           {.trigger_data = 0, .report_time = now + base::Days(2)},
           {.trigger_data = 0, .report_time = now + base::Days(1)},
           {.trigger_data = 0, .report_time = now + base::Days(3)},
       },
       now + base::Days(1)},
  };

  for (const auto& test_case : kTestCases) {
    delegate()->set_randomized_response(test_case.randomized_response);

    auto result = storage()->StoreSource(SourceBuilder().Build());
    EXPECT_EQ(result.status, StorableSource::Result::kSuccess);
    EXPECT_EQ(result.min_fake_report_time, test_case.expected);
  }
}

TEST_F(AttributionStorageTest, TriggerPriority) {
  delegate()->set_max_attributions_per_source(1);

  storage()->StoreSource(
      SourceBuilder().SetSourceEventId(3).SetPriority(0).Build());
  storage()->StoreSource(
      SourceBuilder().SetSourceEventId(5).SetPriority(1).Build());

  EXPECT_THAT(storage()->MaybeCreateAndStoreReport(
                  TriggerBuilder().SetPriority(0).SetTriggerData(20).Build()),
              AllOf(CreateReportStatusIs(AttributionTrigger::Result::kSuccess),
                    DroppedReportIs(absl::nullopt)));

  // This conversion should replace the one above because it has a higher
  // priority.
  EXPECT_THAT(
      storage()->MaybeCreateAndStoreReport(
          TriggerBuilder().SetPriority(2).SetTriggerData(21).Build()),
      AllOf(CreateReportStatusIs(
                AttributionTrigger::Result::kSuccessDroppedLowerPriority),
            DroppedReportIs(Optional(EventLevelDataIs(TriggerDataIs(20u))))));

  storage()->StoreSource(
      SourceBuilder().SetSourceEventId(7).SetPriority(2).Build());

  EXPECT_EQ(AttributionTrigger::Result::kSuccess,
            MaybeCreateAndStoreReport(
                TriggerBuilder().SetPriority(1).SetTriggerData(22).Build()));
  // This conversion should be dropped because it has a lower priority than the
  // one above.
  EXPECT_THAT(
      storage()->MaybeCreateAndStoreReport(
          TriggerBuilder().SetPriority(0).SetTriggerData(23).Build()),
      AllOf(CreateReportStatusIs(AttributionTrigger::Result::kPriorityTooLow),
            DroppedReportIs(Optional(EventLevelDataIs(TriggerDataIs(23u))))));

  task_environment_.FastForwardBy(kReportDelay);

  EXPECT_THAT(storage()->GetAttributionsToReport(base::Time::Now()),
              ElementsAre(AllOf(ReportSourceIs(SourceEventIdIs(5u)),
                                EventLevelDataIs(TriggerDataIs(21u))),
                          AllOf(ReportSourceIs(SourceEventIdIs(7u)),
                                EventLevelDataIs(TriggerDataIs(22u)))));
}

TEST_F(AttributionStorageTest, TriggerPriority_Simple) {
  delegate()->set_max_attributions_per_source(1);

  storage()->StoreSource(SourceBuilder().Build());

  int i = 0;
  EXPECT_EQ(AttributionTrigger::Result::kSuccess,
            MaybeCreateAndStoreReport(
                TriggerBuilder().SetPriority(i).SetTriggerData(i).Build()));
  i++;

  for (; i < 10; i++) {
    EXPECT_EQ(AttributionTrigger::Result::kSuccessDroppedLowerPriority,
              MaybeCreateAndStoreReport(
                  TriggerBuilder().SetPriority(i).SetTriggerData(i).Build()));
  }

  task_environment_.FastForwardBy(kReportDelay);

  EXPECT_THAT(storage()->GetAttributionsToReport(base::Time::Now()),
              ElementsAre(EventLevelDataIs(TriggerDataIs(9u))));
}

TEST_F(AttributionStorageTest, TriggerPriority_SamePriorityDeletesMostRecent) {
  delegate()->set_max_attributions_per_source(2);

  storage()->StoreSource(SourceBuilder().Build());

  EXPECT_EQ(AttributionTrigger::Result::kSuccess,
            MaybeCreateAndStoreReport(
                TriggerBuilder().SetPriority(1).SetTriggerData(3).Build()));

  task_environment_.FastForwardBy(base::Milliseconds(1));
  EXPECT_EQ(AttributionTrigger::Result::kSuccess,
            MaybeCreateAndStoreReport(
                TriggerBuilder().SetPriority(1).SetTriggerData(2).Build()));

  // This report should not be stored, as even though it has the same priority
  // as the previous two, it is the most recent.
  task_environment_.FastForwardBy(base::Milliseconds(1));
  EXPECT_EQ(AttributionTrigger::Result::kPriorityTooLow,
            MaybeCreateAndStoreReport(
                TriggerBuilder().SetPriority(1).SetTriggerData(8).Build()));

  // This report should be stored by replacing the one with `trigger_data ==
  // 2`, which is the most recent of the two with `priority == 1`.
  task_environment_.FastForwardBy(base::Milliseconds(1));
  EXPECT_EQ(AttributionTrigger::Result::kSuccessDroppedLowerPriority,
            MaybeCreateAndStoreReport(
                TriggerBuilder().SetPriority(2).SetTriggerData(5).Build()));

  EXPECT_THAT(storage()->GetAttributionsToReport(base::Time::Max()),
              ElementsAre(EventLevelDataIs(TriggerDataIs(3u)),
                          EventLevelDataIs(TriggerDataIs(5u))));
}

TEST_F(AttributionStorageTest, TriggerPriority_DeactivatesImpression) {
  delegate()->set_max_attributions_per_source(1);

  storage()->StoreSource(
      SourceBuilder().SetSourceEventId(3).SetPriority(0).Build());
  storage()->StoreSource(
      SourceBuilder().SetSourceEventId(5).SetPriority(1).Build());
  EXPECT_THAT(storage()->GetActiveSources(), SizeIs(2));

  EXPECT_EQ(AttributionTrigger::Result::kSuccess,
            MaybeCreateAndStoreReport(DefaultTrigger()));

  // Because the impression with data 5 has the highest priority, it is selected
  // for attribution. The unselected impression with data 3 should be
  // deactivated, but the one with data 5 should remain active.
  EXPECT_THAT(storage()->GetActiveSources(), ElementsAre(SourceEventIdIs(5u)));

  // Ensure that the next report is in a different window.
  delegate()->set_report_delay(kReportDelay + base::Milliseconds(1));

  // This conversion should not be stored because all reports for the attributed
  // impression were in an earlier window.
  EXPECT_EQ(AttributionTrigger::Result::kPriorityTooLow,
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
              ElementsAre(DedupKeysAre(IsEmpty()), DedupKeysAre(IsEmpty())));

  EXPECT_EQ(AttributionTrigger::Result::kSuccess,
            MaybeCreateAndStoreReport(
                TriggerBuilder()
                    .SetConversionDestination(net::SchemefulSite(
                        url::Origin::Create(GURL("https://a.example"))))
                    .SetDedupKey(11)
                    .SetTriggerData(71)
                    .Build()));

  // Should be stored because dedup key doesn't match even though conversion
  // destination does.
  EXPECT_EQ(AttributionTrigger::Result::kSuccess,
            MaybeCreateAndStoreReport(
                TriggerBuilder()
                    .SetConversionDestination(net::SchemefulSite(
                        url::Origin::Create(GURL("https://a.example"))))
                    .SetDedupKey(12)
                    .SetTriggerData(72)
                    .Build()));

  // Should be stored because conversion destination doesn't match even though
  // dedup key does.
  EXPECT_EQ(AttributionTrigger::Result::kSuccess,
            MaybeCreateAndStoreReport(
                TriggerBuilder()
                    .SetConversionDestination(net::SchemefulSite(
                        url::Origin::Create(GURL("https://b.example"))))
                    .SetDedupKey(12)
                    .SetTriggerData(73)
                    .Build()));

  // Shouldn't be stored because conversion destination and dedup key match.
  auto result = storage()->MaybeCreateAndStoreReport(
      TriggerBuilder()
          .SetConversionDestination(net::SchemefulSite(
              url::Origin::Create(GURL("https://a.example"))))
          .SetDedupKey(11)
          .SetTriggerData(74)
          .Build());
  EXPECT_EQ(AttributionTrigger::Result::kDeduplicated, result.status());
  EXPECT_THAT(result.dropped_report(),
              Optional(EventLevelDataIs(TriggerDataIs(74))));

  // Shouldn't be stored because conversion destination and dedup key match.
  EXPECT_EQ(AttributionTrigger::Result::kDeduplicated,
            MaybeCreateAndStoreReport(
                TriggerBuilder()
                    .SetConversionDestination(net::SchemefulSite(
                        url::Origin::Create(GURL("https://b.example"))))
                    .SetDedupKey(12)
                    .SetTriggerData(75)
                    .Build()));

  task_environment_.FastForwardBy(kReportDelay);
  EXPECT_THAT(storage()->GetAttributionsToReport(base::Time::Now()),
              ElementsAre(EventLevelDataIs(TriggerDataIs(71u)),
                          EventLevelDataIs(TriggerDataIs(72u)),
                          EventLevelDataIs(TriggerDataIs(73u))));

  EXPECT_THAT(storage()->GetActiveSources(),
              ElementsAre(DedupKeysAre(ElementsAre(11, 12)),
                          DedupKeysAre(ElementsAre(12))));
}

TEST_F(AttributionStorageTest, DedupKey_DedupsAfterConversionDeletion) {
  storage()->StoreSource(
      SourceBuilder()
          .SetSourceEventId(1)
          .SetConversionOrigin(url::Origin::Create(GURL("https://a.example")))
          .Build());
  EXPECT_THAT(storage()->GetActiveSources(), SizeIs(1));

  task_environment_.FastForwardBy(base::Milliseconds(1));

  EXPECT_EQ(AttributionTrigger::Result::kSuccess,
            MaybeCreateAndStoreReport(
                TriggerBuilder()
                    .SetConversionDestination(net::SchemefulSite(
                        url::Origin::Create(GURL("https://a.example"))))
                    .SetDedupKey(2)
                    .SetTriggerData(3)
                    .Build()));

  task_environment_.FastForwardBy(kReportDelay);

  std::vector<AttributionReport> actual_reports =
      storage()->GetAttributionsToReport(base::Time::Now());
  EXPECT_THAT(actual_reports, ElementsAre(EventLevelDataIs(TriggerDataIs(3u))));

  // Simulate the report being sent and deleted from storage.
  DeleteReports(actual_reports);

  task_environment_.FastForwardBy(base::Milliseconds(1));

  // This report shouldn't be stored, as it should be deduped against the
  // previously stored one even though that previous one is no longer in the DB.
  EXPECT_EQ(AttributionTrigger::Result::kDeduplicated,
            MaybeCreateAndStoreReport(
                TriggerBuilder()
                    .SetConversionDestination(net::SchemefulSite(
                        url::Origin::Create(GURL("https://a.example"))))
                    .SetDedupKey(2)
                    .SetTriggerData(5)
                    .Build()));

  task_environment_.FastForwardBy(kReportDelay);
  EXPECT_THAT(storage()->GetAttributionsToReport(base::Time::Now()), IsEmpty());
}

TEST_F(AttributionStorageTest, GetAttributionsToReport_SetsPriority) {
  storage()->StoreSource(SourceBuilder().Build());
  EXPECT_EQ(
      AttributionTrigger::Result::kSuccess,
      MaybeCreateAndStoreReport(TriggerBuilder().SetPriority(13).Build()));

  task_environment_.FastForwardBy(kReportDelay);

  EXPECT_THAT(storage()->GetAttributionsToReport(base::Time::Now()),
              ElementsAre(EventLevelDataIs(TriggerPriorityIs(13))));
}

TEST_F(AttributionStorageTest, NoIDReuse_Impression) {
  storage()->StoreSource(SourceBuilder().Build());
  auto sources = storage()->GetActiveSources();
  const StoredSource::Id id1 = sources.front().source_id();

  storage()->ClearData(base::Time::Min(), base::Time::Max(),
                       base::NullCallback());
  EXPECT_THAT(storage()->GetActiveSources(), IsEmpty());

  storage()->StoreSource(SourceBuilder().Build());
  sources = storage()->GetActiveSources();
  const StoredSource::Id id2 = sources.front().source_id();

  EXPECT_NE(id1, id2);
}

TEST_F(AttributionStorageTest, NoIDReuse_Conversion) {
  storage()->StoreSource(SourceBuilder().Build());
  EXPECT_EQ(AttributionTrigger::Result::kSuccess,
            MaybeCreateAndStoreReport(DefaultTrigger()));
  auto reports = storage()->GetAttributionsToReport(base::Time::Max());
  EXPECT_THAT(reports,
              ElementsAre(Property(&AttributionReport::ReportId, IsTrue())));
  const AttributionReport::Id id1 = *reports.front().ReportId();

  storage()->ClearData(base::Time::Min(), base::Time::Max(),
                       base::NullCallback());
  EXPECT_THAT(storage()->GetAttributionsToReport(base::Time::Max()), IsEmpty());

  storage()->StoreSource(SourceBuilder().Build());
  EXPECT_EQ(AttributionTrigger::Result::kSuccess,
            MaybeCreateAndStoreReport(DefaultTrigger()));
  reports = storage()->GetAttributionsToReport(base::Time::Max());
  EXPECT_THAT(reports,
              ElementsAre(Property(&AttributionReport::ReportId, IsTrue())));
  const AttributionReport::Id id2 = *reports.front().ReportId();

  EXPECT_NE(id1, id2);
}

TEST_F(AttributionStorageTest, UpdateReportForSendFailure) {
  storage()->StoreSource(SourceBuilder().Build());
  EXPECT_EQ(AttributionTrigger::Result::kSuccess,
            MaybeCreateAndStoreReport(DefaultTrigger()));

  task_environment_.FastForwardBy(kReportDelay);

  std::vector<AttributionReport> actual_reports =
      storage()->GetAttributionsToReport(base::Time::Now());
  EXPECT_THAT(actual_reports, ElementsAre(FailedSendAttemptsIs(0)));

  const base::TimeDelta delay = base::Days(2);
  const base::Time new_report_time = actual_reports[0].report_time() + delay;
  EXPECT_TRUE(storage()->UpdateReportForSendFailure(
      *(absl::get<AttributionReport::EventLevelData>(actual_reports[0].data())
            .id),
      new_report_time));

  task_environment_.FastForwardBy(delay);

  EXPECT_THAT(storage()->GetAttributionsToReport(base::Time::Now()),
              ElementsAre(AllOf(FailedSendAttemptsIs(1),
                                ReportTimeIs(new_report_time))));
}

TEST_F(AttributionStorageTest, StoreSource_ReturnsDeactivatedSources) {
  SourceBuilder builder1;
  builder1.SetSourceEventId(7);

  EXPECT_THAT(storage()->StoreSource(builder1.Build()).deactivated_sources,
              IsEmpty());
  EXPECT_THAT(storage()->GetActiveSources(), SizeIs(1));

  task_environment_.FastForwardBy(kReportDelay);

  // Set a dedup key to ensure that the return deactivated source contains it.
  EXPECT_EQ(
      AttributionTrigger::Result::kSuccess,
      MaybeCreateAndStoreReport(TriggerBuilder().SetDedupKey(13).Build()));
  EXPECT_THAT(storage()->GetAttributionsToReport(base::Time::Now()), SizeIs(1));

  SourceBuilder builder2;
  builder2.SetSourceEventId(9);

  builder1.SetDedupKeys({13});
  EXPECT_THAT(storage()->StoreSource(builder2.Build()).deactivated_sources,
              ElementsAre(DeactivatedSource(
                  builder1.BuildStored(),
                  DeactivatedSource::Reason::kReplacedByNewerSource)));

  EXPECT_THAT(storage()->GetActiveSources(),
              ElementsAre(builder2.BuildStored()));
}

TEST_F(AttributionStorageTest, StoreSource_ReturnsDeactivatedSources_Limited) {
  SourceBuilder builder1;
  builder1.SetSourceEventId(1);
  EXPECT_THAT(storage()->StoreSource(builder1.Build()).deactivated_sources,
              IsEmpty());

  SourceBuilder builder2;
  builder2.SetSourceEventId(2);
  EXPECT_THAT(storage()->StoreSource(builder2.Build()).deactivated_sources,
              IsEmpty());

  EXPECT_THAT(storage()->GetActiveSources(), SizeIs(2));

  task_environment_.FastForwardBy(kReportDelay);

  EXPECT_EQ(AttributionTrigger::Result::kSuccess,
            MaybeCreateAndStoreReport(DefaultTrigger()));
  EXPECT_EQ(AttributionTrigger::Result::kSuccess,
            MaybeCreateAndStoreReport(DefaultTrigger()));
  EXPECT_THAT(storage()->GetAttributionsToReport(base::Time::Now()), SizeIs(2));

  // 2 sources are deactivated, but only 1 should be returned.
  SourceBuilder builder3;
  builder3.SetSourceEventId(3);
  EXPECT_THAT(storage()
                  ->StoreSource(builder3.Build(),
                                /*deactivated_source_return_limit=*/1)
                  .deactivated_sources,
              ElementsAre(DeactivatedSource(
                  builder1.BuildStored(),
                  DeactivatedSource::Reason::kReplacedByNewerSource)));
  EXPECT_THAT(storage()->GetActiveSources(),
              ElementsAre(builder3.BuildStored()));
}

TEST_F(AttributionStorageTest,
       MaybeCreateAndStoreReport_ReturnsDeactivatedSources) {
  SourceBuilder builder;
  builder.SetSourceEventId(7);
  EXPECT_THAT(storage()->StoreSource(builder.Build()).deactivated_sources,
              IsEmpty());
  EXPECT_THAT(storage()->GetActiveSources(), SizeIs(1));

  // Store the maximum number of reports for the source.
  for (size_t i = 1; i <= kMaxConversions; i++) {
    EXPECT_EQ(AttributionTrigger::Result::kSuccess,
              MaybeCreateAndStoreReport(DefaultTrigger()));
  }

  task_environment_.FastForwardBy(kReportDelay);
  auto reports = storage()->GetAttributionsToReport(base::Time::Now());
  EXPECT_THAT(reports, SizeIs(3));

  // Simulate the reports being sent and removed from storage.
  DeleteReports(reports);

  // The next report should cause the source to be deactivated; the report
  // itself shouldn't be stored as we've already reached the maximum number of
  // conversions per source.
  EXPECT_THAT(
      storage()->MaybeCreateAndStoreReport(DefaultTrigger()),
      AllOf(CreateReportStatusIs(AttributionTrigger::Result::kPriorityTooLow),
            DroppedReportIs(Optional(ReportSourceIs(builder.BuildStored()))),
            DeactivatedSourceIs(DeactivatedSource(
                builder.BuildStored(),
                DeactivatedSource::Reason::kReachedAttributionLimit))));
}

TEST_F(AttributionStorageTest, ReportID_RoundTrips) {
  storage()->StoreSource(SourceBuilder().Build());
  EXPECT_EQ(AttributionTrigger::Result::kSuccess,
            MaybeCreateAndStoreReport(DefaultTrigger()));

  task_environment_.FastForwardBy(kReportDelay);

  std::vector<AttributionReport> actual_reports =
      storage()->GetAttributionsToReport(base::Time::Now());
  EXPECT_EQ(1u, actual_reports.size());
  EXPECT_EQ(DefaultExternalReportID(), actual_reports[0].external_report_id());
}

TEST_F(AttributionStorageTest, AdjustOfflineReportTimes) {
  EXPECT_EQ(storage()->AdjustOfflineReportTimes(), absl::nullopt);

  delegate()->set_offline_report_delay_config(
      AttributionStorageDelegate::OfflineReportDelayConfig{
          .min = base::Hours(1), .max = base::Hours(1)});
  EXPECT_EQ(storage()->AdjustOfflineReportTimes(), absl::nullopt);

  storage()->StoreSource(SourceBuilder().Build());
  EXPECT_EQ(AttributionTrigger::Result::kSuccess,
            MaybeCreateAndStoreReport(DefaultTrigger()));

  const base::Time original_report_time = base::Time::Now() + kReportDelay;

  EXPECT_THAT(storage()->GetAttributionsToReport(base::Time::Max()),
              ElementsAre(ReportTimeIs(original_report_time)));

  task_environment_.FastForwardBy(kReportDelay);

  EXPECT_EQ(storage()->AdjustOfflineReportTimes(), original_report_time);

  // The report time should not be changed as it is equal to now, not strictly
  // less than it.
  EXPECT_THAT(storage()->GetAttributionsToReport(base::Time::Max()),
              ElementsAre(ReportTimeIs(original_report_time)));

  task_environment_.FastForwardBy(base::Milliseconds(1));

  const base::Time new_report_time = base::Time::Now() + base::Hours(1);

  EXPECT_EQ(storage()->AdjustOfflineReportTimes(), new_report_time);

  // The report time should be changed as it is strictly less than now.
  EXPECT_THAT(storage()->GetAttributionsToReport(base::Time::Max()),
              ElementsAre(ReportTimeIs(new_report_time)));
}

TEST_F(AttributionStorageTest, AdjustOfflineReportTimes_Range) {
  delegate()->set_offline_report_delay_config(
      AttributionStorageDelegate::OfflineReportDelayConfig{
          .min = base::Hours(1), .max = base::Hours(3)});

  storage()->StoreSource(SourceBuilder().Build());
  EXPECT_EQ(AttributionTrigger::Result::kSuccess,
            MaybeCreateAndStoreReport(DefaultTrigger()));

  const base::Time original_report_time = base::Time::Now() + kReportDelay;

  EXPECT_THAT(storage()->GetAttributionsToReport(base::Time::Max()),
              ElementsAre(ReportTimeIs(original_report_time)));

  task_environment_.FastForwardBy(kReportDelay + base::Milliseconds(1));

  storage()->AdjustOfflineReportTimes();

  EXPECT_THAT(
      storage()->GetAttributionsToReport(base::Time::Max()),
      ElementsAre(ReportTimeIs(AllOf(Ge(base::Time::Now() + base::Hours(1)),
                                     Le(base::Time::Now() + base::Hours(3))))));
}

TEST_F(AttributionStorageTest, GetNextReportTime) {
  const auto origin_a = url::Origin::Create(GURL("https://a.example/"));
  const auto origin_b = url::Origin::Create(GURL("https://b.example/"));

  EXPECT_EQ(storage()->GetNextReportTime(base::Time::Min()), absl::nullopt);

  storage()->StoreSource(SourceBuilder().SetReportingOrigin(origin_a).Build());
  EXPECT_EQ(AttributionTrigger::Result::kSuccess,
            MaybeCreateAndStoreReport(
                TriggerBuilder().SetReportingOrigin(origin_a).Build()));

  const base::Time report_time_a = base::Time::Now() + kReportDelay;

  EXPECT_EQ(storage()->GetNextReportTime(base::Time::Min()), report_time_a);
  EXPECT_EQ(storage()->GetNextReportTime(report_time_a), absl::nullopt);

  task_environment_.FastForwardBy(base::Milliseconds(1));
  storage()->StoreSource(SourceBuilder().SetReportingOrigin(origin_b).Build());
  EXPECT_EQ(AttributionTrigger::Result::kSuccess,
            MaybeCreateAndStoreReport(
                TriggerBuilder().SetReportingOrigin(origin_b).Build()));

  const base::Time report_time_b = base::Time::Now() + kReportDelay;

  EXPECT_EQ(storage()->GetNextReportTime(base::Time::Min()), report_time_a);
  EXPECT_EQ(storage()->GetNextReportTime(report_time_a), report_time_b);
  EXPECT_EQ(storage()->GetNextReportTime(report_time_b), absl::nullopt);
}

TEST_F(AttributionStorageTest, GetAttributionsToReport_Shuffles) {
  storage()->StoreSource(SourceBuilder().Build());
  EXPECT_EQ(
      AttributionTrigger::Result::kSuccess,
      MaybeCreateAndStoreReport(TriggerBuilder().SetTriggerData(3).Build()));
  EXPECT_EQ(
      AttributionTrigger::Result::kSuccess,
      MaybeCreateAndStoreReport(TriggerBuilder().SetTriggerData(1).Build()));
  EXPECT_EQ(
      AttributionTrigger::Result::kSuccess,
      MaybeCreateAndStoreReport(TriggerBuilder().SetTriggerData(2).Build()));

  EXPECT_THAT(storage()->GetAttributionsToReport(
                  /*max_report_time=*/base::Time::Max(), /*limit=*/-1),
              ElementsAre(EventLevelDataIs(TriggerDataIs(3)),
                          EventLevelDataIs(TriggerDataIs(1)),
                          EventLevelDataIs(TriggerDataIs(2))));

  delegate()->set_reverse_reports_on_shuffle(true);

  EXPECT_THAT(storage()->GetAttributionsToReport(
                  /*max_report_time=*/base::Time::Max(), /*limit=*/-1),
              ElementsAre(EventLevelDataIs(TriggerDataIs(2)),
                          EventLevelDataIs(TriggerDataIs(1)),
                          EventLevelDataIs(TriggerDataIs(3))));
}

TEST_F(AttributionStorageTest, SourceDebugKey_RoundTrips) {
  storage()->StoreSource(
      SourceBuilder(base::Time::Now()).SetDebugKey(33).Build());
  EXPECT_THAT(storage()->GetActiveSources(), ElementsAre(SourceDebugKeyIs(33)));
}

TEST_F(AttributionStorageTest, TriggerDebugKey_RoundTrips) {
  storage()->StoreSource(
      SourceBuilder(base::Time::Now()).SetDebugKey(22).Build());
  EXPECT_EQ(
      AttributionTrigger::Result::kSuccess,
      MaybeCreateAndStoreReport(TriggerBuilder().SetDebugKey(33).Build()));

  task_environment_.FastForwardBy(kReportDelay);
  EXPECT_THAT(storage()->GetAttributionsToReport(base::Time::Now()),
              ElementsAre(AllOf(ReportSourceIs(SourceDebugKeyIs(22)),
                                TriggerDebugKeyIs(33))));
}

// This is tested more thoroughly by the `RateLimitTable` unit tests. Here just
// ensure that the rate limits are consulted at all.
TEST_F(AttributionStorageTest, MaxReportingOriginsPerSource) {
  delegate()->set_rate_limits({
      .time_window = base::TimeDelta::Max(),
      .max_source_registration_reporting_origins = 2,
      .max_attribution_reporting_origins = std::numeric_limits<int64_t>::max(),
      .max_attributions = std::numeric_limits<int64_t>::max(),
  });

  auto result = storage()->StoreSource(
      SourceBuilder()
          .SetReportingOrigin(url::Origin::Create(GURL("https://r1.test")))
          .SetDebugKey(1)
          .Build());
  ASSERT_EQ(result.status, StorableSource::Result::kSuccess);

  result = storage()->StoreSource(
      SourceBuilder()
          .SetReportingOrigin(url::Origin::Create(GURL("https://r2.test")))
          .SetDebugKey(2)
          .Build());
  ASSERT_EQ(result.status, StorableSource::Result::kSuccess);

  result = storage()->StoreSource(
      SourceBuilder()
          .SetReportingOrigin(url::Origin::Create(GURL("https://r3.test")))
          .SetDebugKey(3)
          .Build());
  ASSERT_EQ(result.status, StorableSource::Result::kExcessiveReportingOrigins);

  EXPECT_THAT(storage()->GetActiveSources(),
              ElementsAre(SourceDebugKeyIs(1), SourceDebugKeyIs(2)));
}

// This is tested more thoroughly by the `RateLimitTable` unit tests. Here just
// ensure that the rate limits are consulted at all.
TEST_F(AttributionStorageTest, MaxReportingOriginsPerAttribution) {
  delegate()->set_rate_limits({
      .time_window = base::TimeDelta::Max(),
      .max_source_registration_reporting_origins =
          std::numeric_limits<int64_t>::max(),
      .max_attribution_reporting_origins = 2,
      .max_attributions = std::numeric_limits<int64_t>::max(),
  });

  const auto origin1 = url::Origin::Create(GURL("https://r1.test"));
  const auto origin2 = url::Origin::Create(GURL("https://r2.test"));
  const auto origin3 = url::Origin::Create(GURL("https://r3.test"));

  storage()->StoreSource(SourceBuilder().SetReportingOrigin(origin1).Build());
  storage()->StoreSource(SourceBuilder().SetReportingOrigin(origin2).Build());
  storage()->StoreSource(SourceBuilder().SetReportingOrigin(origin3).Build());
  ASSERT_THAT(storage()->GetActiveSources(), SizeIs(3));

  ASSERT_EQ(
      MaybeCreateAndStoreReport(
          TriggerBuilder().SetReportingOrigin(origin1).SetDebugKey(1).Build()),
      AttributionTrigger::Result::kSuccess);

  ASSERT_EQ(
      MaybeCreateAndStoreReport(
          TriggerBuilder().SetReportingOrigin(origin2).SetDebugKey(2).Build()),
      AttributionTrigger::Result::kSuccess);

  ASSERT_EQ(
      MaybeCreateAndStoreReport(
          TriggerBuilder().SetReportingOrigin(origin3).SetDebugKey(3).Build()),
      AttributionTrigger::Result::kExcessiveReportingOrigins);

  EXPECT_THAT(storage()->GetAttributionsToReport(base::Time::Max()),
              ElementsAre(TriggerDebugKeyIs(1), TriggerDebugKeyIs(2)));
}

TEST_F(AttributionStorageTest, StoreAggregatableAttribution) {
  storage()->StoreSource(SourceBuilder().Build());

  AggregatableAttribution aggregatable_attribution(
      StoredSource::Id(1), /*trigger_time=*/base::Time::Now(),
      /*report_time=*/base::Time::Now() + base::Hours(2),
      /*contributions=*/
      {HistogramContribution(/*bucket=*/"1", /*value=*/2),
       HistogramContribution(/*bucket=*/"3", /*value=*/4)});

  EXPECT_TRUE(storage()->AddAggregatableAttributionForTesting(
      aggregatable_attribution));

  const HistogramContribution& contribution_1 =
      aggregatable_attribution.contributions[0];
  const HistogramContribution& contribution_2 =
      aggregatable_attribution.contributions[1];

  auto stored_source = SourceBuilder().BuildStored();

  EXPECT_THAT(
      storage()->GetAggregatableContributionReportsForTesting(
          base::Time::Max()),
      ElementsAre(
          AttributionReport(
              AttributionInfo(stored_source,
                              aggregatable_attribution.trigger_time,
                              /*debug_key=*/absl::nullopt),
              aggregatable_attribution.report_time, DefaultExternalReportID(),
              AttributionReport::AggregatableContributionData(
                  HistogramContribution(contribution_1.bucket(),
                                        contribution_1.value()),
                  AttributionReport::AggregatableContributionData::Id(1))),
          AttributionReport(
              AttributionInfo(stored_source,
                              aggregatable_attribution.trigger_time,
                              /*debug_key*/ absl::nullopt),
              aggregatable_attribution.report_time, DefaultExternalReportID(),
              AttributionReport::AggregatableContributionData(
                  HistogramContribution(contribution_2.bucket(),
                                        contribution_2.value()),
                  AttributionReport::AggregatableContributionData::Id(2)))));
}

}  // namespace content
