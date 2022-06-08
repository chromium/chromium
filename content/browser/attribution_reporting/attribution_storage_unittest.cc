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
#include "base/check.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/guid.h"
#include "base/memory/raw_ptr.h"
#include "base/ranges/algorithm.h"
#include "base/strings/stringprintf.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "content/browser/attribution_reporting/aggregatable_histogram_contribution.h"
#include "content/browser/attribution_reporting/attribution_aggregatable_trigger_data.h"
#include "content/browser/attribution_reporting/attribution_aggregatable_values.h"
#include "content/browser/attribution_reporting/attribution_aggregation_keys.h"
#include "content/browser/attribution_reporting/attribution_filter_data.h"
#include "content/browser/attribution_reporting/attribution_observer_types.h"
#include "content/browser/attribution_reporting/attribution_report.h"
#include "content/browser/attribution_reporting/attribution_source_type.h"
#include "content/browser/attribution_reporting/attribution_storage_sql.h"
#include "content/browser/attribution_reporting/attribution_test_utils.h"
#include "content/browser/attribution_reporting/attribution_trigger.h"
#include "content/browser/attribution_reporting/attribution_utils.h"
#include "content/browser/attribution_reporting/common_source_info.h"
#include "content/browser/attribution_reporting/storable_source.h"
#include "content/browser/attribution_reporting/stored_source.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/numeric/int128.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

namespace {

using ::testing::AllOf;
using ::testing::ElementsAre;
using ::testing::Ge;
using ::testing::IsEmpty;
using ::testing::IsTrue;
using ::testing::Le;
using ::testing::Optional;
using ::testing::Property;
using ::testing::SizeIs;
using ::testing::UnorderedElementsAre;

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
  AttributionReport GetExpectedEventLevelReport(
      const StoredSource& source,
      const AttributionTrigger& conversion) {
    // TOO(apaseltiner): Replace this logic with explicit setting of expected
    // values.
    auto event_trigger = base::ranges::find_if(
        conversion.event_triggers(),
        [&](const AttributionTrigger::EventTriggerData& event_trigger) {
          return AttributionFiltersMatch(source.common_info().filter_data(),
                                         event_trigger.filters,
                                         event_trigger.not_filters);
        });
    CHECK(event_trigger != conversion.event_triggers().end());

    return ReportBuilder(AttributionInfoBuilder(source)
                             .SetTime(base::Time::Now())
                             .Build())
        .SetTriggerData(event_trigger->data)
        .SetReportTime(source.common_info().impression_time() + kReportDelay)
        .SetPriority(event_trigger->priority)
        .Build();
  }

  AttributionReport GetExpectedAggregatableReport(
      const StoredSource& source,
      std::vector<AggregatableHistogramContribution> contributions) {
    return ReportBuilder(AttributionInfoBuilder(source)
                             .SetTime(base::Time::Now())
                             .Build())
        .SetReportTime(base::Time::Now() + kReportDelay)
        .SetAggregatableHistogramContributions(std::move(contributions))
        .BuildAggregatableAttribution();
  }

  AttributionTrigger::EventLevelResult MaybeCreateAndStoreEventLevelReport(
      const AttributionTrigger& conversion) {
    return storage_->MaybeCreateAndStoreReport(conversion).event_level_status();
  }

  AttributionTrigger::AggregatableResult MaybeCreateAndStoreAggregatableReport(
      const AttributionTrigger& trigger) {
    return storage_->MaybeCreateAndStoreReport(trigger).aggregatable_status();
  }

  void DeleteReports(const std::vector<AttributionReport>& reports) {
    for (const auto& report : reports) {
      EXPECT_TRUE(storage_->DeleteReport(report.ReportId()));
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
  EXPECT_EQ(AttributionTrigger::EventLevelResult::kNoMatchingImpressions,
            storage->MaybeCreateAndStoreReport(DefaultTrigger())
                .event_level_status());
  EXPECT_THAT(storage->GetAttributionReports(base::Time::Now()), IsEmpty());
  EXPECT_THAT(storage->GetActiveSources(), IsEmpty());
  EXPECT_TRUE(storage->DeleteReport(AttributionReport::EventLevelData::Id(0)));
  EXPECT_NO_FATAL_FAILURE(storage->ClearData(
      base::Time::Min(), base::Time::Max(), base::NullCallback()));
  EXPECT_EQ(storage->AdjustOfflineReportTimes(), absl::nullopt);
}

TEST_F(AttributionStorageTest, ImpressionStoredAndRetrieved_ValuesIdentical) {
  storage()->StoreSource(SourceBuilder().Build());
  EXPECT_THAT(storage()->GetActiveSources(),
              ElementsAre(CommonSourceInfoIs(
                  SourceBuilder().SetDefaultFilterData().BuildCommonInfo())));
}

TEST_F(AttributionStorageTest,
       GetWithNoMatchingImpressions_NoImpressionsReturned) {
  EXPECT_THAT(
      storage()->MaybeCreateAndStoreReport(DefaultTrigger()),
      AllOf(CreateReportEventLevelStatusIs(
                AttributionTrigger::EventLevelResult::kNoMatchingImpressions),
            NewEventLevelReportIs(absl::nullopt),
            NewAggregatableReportIs(absl::nullopt)));
  EXPECT_THAT(storage()->GetAttributionReports(base::Time::Now()), IsEmpty());
}

TEST_F(AttributionStorageTest, GetWithMatchingImpression_ImpressionReturned) {
  storage()->StoreSource(SourceBuilder().Build());
  EXPECT_EQ(AttributionTrigger::EventLevelResult::kSuccess,
            MaybeCreateAndStoreEventLevelReport(DefaultTrigger()));
}

TEST_F(AttributionStorageTest, MultipleImpressionsForConversion_OneConverts) {
  storage()->StoreSource(SourceBuilder().Build());
  storage()->StoreSource(SourceBuilder().Build());
  EXPECT_EQ(AttributionTrigger::EventLevelResult::kSuccess,
            MaybeCreateAndStoreEventLevelReport(DefaultTrigger()));
}

TEST_F(AttributionStorageTest,
       CrossOriginSameDomainConversion_ImpressionConverted) {
  auto impression =
      SourceBuilder()
          .SetConversionOrigin(url::Origin::Create(GURL("https://sub.a.test")))
          .Build();
  storage()->StoreSource(impression);
  EXPECT_EQ(
      AttributionTrigger::EventLevelResult::kSuccess,
      MaybeCreateAndStoreEventLevelReport(
          TriggerBuilder()
              .SetDestinationOrigin(url::Origin::Create(GURL("https://a.test")))
              .SetReportingOrigin(impression.common_info().reporting_origin())
              .Build()));
}

TEST_F(AttributionStorageTest, EventSourceImpressionsForConversion_Converts) {
  storage()->StoreSource(
      SourceBuilder().SetSourceType(AttributionSourceType::kEvent).Build());
  EXPECT_EQ(AttributionTrigger::EventLevelResult::kSuccess,
            MaybeCreateAndStoreEventLevelReport(
                TriggerBuilder().SetEventSourceTriggerData(456).Build()));

  task_environment_.FastForwardBy(kReportDelay);

  EXPECT_THAT(storage()->GetAttributionReports(base::Time::Now()),
              ElementsAre(EventLevelDataIs(TriggerDataIs(456u))));
}

TEST_F(AttributionStorageTest, ImpressionExpired_NoConversionsStored) {
  storage()->StoreSource(
      SourceBuilder().SetExpiry(base::Milliseconds(2)).Build());
  task_environment_.FastForwardBy(base::Milliseconds(2));

  EXPECT_EQ(AttributionTrigger::EventLevelResult::kNoMatchingImpressions,
            MaybeCreateAndStoreEventLevelReport(DefaultTrigger()));
}

TEST_F(AttributionStorageTest, ImpressionExpired_ConversionsStoredPrior) {
  storage()->StoreSource(
      SourceBuilder().SetExpiry(base::Milliseconds(4)).Build());

  task_environment_.FastForwardBy(base::Milliseconds(3));

  EXPECT_EQ(AttributionTrigger::EventLevelResult::kSuccess,
            MaybeCreateAndStoreEventLevelReport(DefaultTrigger()));

  task_environment_.FastForwardBy(base::Milliseconds(5));

  EXPECT_EQ(AttributionTrigger::EventLevelResult::kNoMatchingImpressions,
            MaybeCreateAndStoreEventLevelReport(DefaultTrigger()));
}

TEST_F(AttributionStorageTest,
       ImpressionWithMaxConversions_ConversionReportNotStored) {
  storage()->StoreSource(SourceBuilder().Build());

  for (int i = 0; i < kMaxConversions; i++) {
    EXPECT_EQ(AttributionTrigger::EventLevelResult::kSuccess,
              MaybeCreateAndStoreEventLevelReport(DefaultTrigger()));
  }

  // No additional conversion reports should be created.
  EXPECT_THAT(storage()->MaybeCreateAndStoreReport(DefaultTrigger()),
              AllOf(CreateReportEventLevelStatusIs(
                        AttributionTrigger::EventLevelResult::kPriorityTooLow),
                    ReplacedEventLevelReportIs(absl::nullopt)));
}

TEST_F(AttributionStorageTest, OneConversion_OneReportScheduled) {
  auto conversion = DefaultTrigger();

  storage()->StoreSource(SourceBuilder().Build());
  EXPECT_EQ(AttributionTrigger::EventLevelResult::kSuccess,
            MaybeCreateAndStoreEventLevelReport(conversion));

  AttributionReport expected_report = GetExpectedEventLevelReport(
      SourceBuilder().SetDefaultFilterData().BuildStored(), conversion);

  task_environment_.FastForwardBy(kReportDelay);

  EXPECT_THAT(storage()->GetAttributionReports(base::Time::Now()),
              ElementsAre(expected_report));
}

TEST_F(AttributionStorageTest,
       ConversionWithDifferentReportingOrigin_NoReportScheduled) {
  auto impression = SourceBuilder()
                        .SetReportingOrigin(
                            url::Origin::Create(GURL("https://different.test")))
                        .Build();
  storage()->StoreSource(impression);
  EXPECT_EQ(AttributionTrigger::EventLevelResult::kNoMatchingImpressions,
            MaybeCreateAndStoreEventLevelReport(DefaultTrigger()));

  task_environment_.FastForwardBy(kReportDelay);

  EXPECT_THAT(storage()->GetAttributionReports(base::Time::Now()), IsEmpty());
}

TEST_F(AttributionStorageTest,
       ConversionWithDifferentConversionOrigin_NoReportScheduled) {
  auto impression = SourceBuilder()
                        .SetConversionOrigin(
                            url::Origin::Create(GURL("https://different.test")))
                        .Build();
  storage()->StoreSource(impression);
  EXPECT_EQ(AttributionTrigger::EventLevelResult::kNoMatchingImpressions,
            MaybeCreateAndStoreEventLevelReport(DefaultTrigger()));

  task_environment_.FastForwardBy(kReportDelay);

  EXPECT_THAT(storage()->GetAttributionReports(base::Time::Now()), IsEmpty());
}

TEST_F(AttributionStorageTest, ConversionReportDeleted_RemovedFromStorage) {
  storage()->StoreSource(SourceBuilder().Build());
  EXPECT_EQ(AttributionTrigger::EventLevelResult::kSuccess,
            MaybeCreateAndStoreEventLevelReport(DefaultTrigger()));

  task_environment_.FastForwardBy(kReportDelay);

  std::vector<AttributionReport> reports =
      storage()->GetAttributionReports(base::Time::Now());
  EXPECT_THAT(reports, SizeIs(1));
  DeleteReports(reports);

  EXPECT_THAT(storage()->GetAttributionReports(base::Time::Now()), IsEmpty());
}

TEST_F(AttributionStorageTest,
       ManyImpressionsWithManyConversions_OneImpressionAttributed) {
  const int kNumMultiTouchImpressions = 20;

  // Store a large, arbitrary number of impressions.
  for (int i = 0; i < kNumMultiTouchImpressions; i++) {
    storage()->StoreSource(SourceBuilder().Build());
  }

  for (int i = 0; i < kMaxConversions; i++) {
    EXPECT_EQ(AttributionTrigger::EventLevelResult::kSuccess,
              MaybeCreateAndStoreEventLevelReport(DefaultTrigger()));
  }

  // No additional conversion reports should be created for any of the
  // impressions.
  EXPECT_EQ(AttributionTrigger::EventLevelResult::kPriorityTooLow,
            MaybeCreateAndStoreEventLevelReport(DefaultTrigger()));
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
  EXPECT_EQ(AttributionTrigger::EventLevelResult::kSuccess,
            MaybeCreateAndStoreEventLevelReport(DefaultTrigger()));
  EXPECT_THAT(storage()->GetActiveSources(), SizeIs(1));
}

// This test makes sure that when a new click is received for a given
// <reporting_origin, conversion_origin> pair, all existing impressions for that
// origin that have converted are marked ineligible for new conversions per the
// multi-touch model.
TEST_F(AttributionStorageTest,
       NewImpressionForConvertedImpression_MarkedInactive) {
  storage()->StoreSource(SourceBuilder().SetSourceEventId(0).Build());
  EXPECT_EQ(AttributionTrigger::EventLevelResult::kSuccess,
            MaybeCreateAndStoreEventLevelReport(DefaultTrigger()));

  task_environment_.FastForwardBy(kReportDelay);

  // Delete the report.
  DeleteReports(storage()->GetAttributionReports(base::Time::Now()));

  // Store a new impression that should mark the first inactive.
  SourceBuilder builder;
  builder.SetSourceEventId(1000);
  storage()->StoreSource(builder.Build());

  // Only the new impression should convert.
  auto conversion = DefaultTrigger();
  EXPECT_EQ(AttributionTrigger::EventLevelResult::kSuccess,
            MaybeCreateAndStoreEventLevelReport(conversion));
  AttributionReport expected_report = GetExpectedEventLevelReport(
      builder.SetDefaultFilterData().BuildStored(), conversion);

  task_environment_.FastForwardBy(kReportDelay);

  // Verify it was the new impression that converted.
  EXPECT_THAT(storage()->GetAttributionReports(base::Time::Now()),
              ElementsAre(expected_report));
}

TEST_F(AttributionStorageTest,
       NonMatchingImpressionForConvertedImpression_FirstRemainsActive) {
  SourceBuilder builder;
  storage()->StoreSource(builder.Build());

  auto conversion = DefaultTrigger();
  EXPECT_EQ(AttributionTrigger::EventLevelResult::kSuccess,
            MaybeCreateAndStoreEventLevelReport(DefaultTrigger()));

  task_environment_.FastForwardBy(kReportDelay);

  // Delete the report.
  DeleteReports(storage()->GetAttributionReports(base::Time::Now()));

  // Store a new impression with a different reporting origin.
  storage()->StoreSource(SourceBuilder()
                             .SetReportingOrigin(url::Origin::Create(
                                 GURL("https://different.test")))
                             .Build());

  // The first impression should still be active and able to convert.
  EXPECT_EQ(AttributionTrigger::EventLevelResult::kSuccess,
            MaybeCreateAndStoreEventLevelReport(conversion));

  AttributionReport expected_report = GetExpectedEventLevelReport(
      builder.SetDefaultFilterData().BuildStored(), conversion);

  // Verify it was the first impression that converted.
  EXPECT_THAT(storage()->GetAttributionReports(base::Time::Now()),
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

  AttributionReport third_expected_conversion = GetExpectedEventLevelReport(
      builder.SetDefaultFilterData().BuildStored(), conversion);
  EXPECT_EQ(AttributionTrigger::EventLevelResult::kSuccess,
            MaybeCreateAndStoreEventLevelReport(conversion));

  task_environment_.FastForwardBy(kReportDelay);

  EXPECT_THAT(storage()->GetAttributionReports(base::Time::Now()),
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

  EXPECT_EQ(AttributionTrigger::EventLevelResult::kSuccess,
            MaybeCreateAndStoreEventLevelReport(DefaultTrigger()));

  // Advance to the first impression's report time and verify only its report is
  // available.
  task_environment_.FastForwardBy(kReportDelay - base::Milliseconds(1));
  EXPECT_THAT(storage()->GetAttributionReports(base::Time::Now()), IsEmpty());

  task_environment_.FastForwardBy(base::Milliseconds(1));
  EXPECT_THAT(storage()->GetAttributionReports(base::Time::Now()), SizeIs(1));
}

TEST_F(AttributionStorageTest, GetAttributionReportsMultipleTimes_SameResult) {
  storage()->StoreSource(SourceBuilder().Build());
  EXPECT_EQ(AttributionTrigger::EventLevelResult::kSuccess,
            MaybeCreateAndStoreEventLevelReport(DefaultTrigger()));
  task_environment_.FastForwardBy(kReportDelay);

  std::vector<AttributionReport> first_call_reports =
      storage()->GetAttributionReports(base::Time::Now());
  std::vector<AttributionReport> second_call_reports =
      storage()->GetAttributionReports(base::Time::Now());

  // Expect that |GetAttributionReports()| did not delete any conversions.
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

TEST_F(AttributionStorageTest, MaxEventLevelAttributionsPerOrigin) {
  SourceBuilder source_builder = TestAggregatableSourceProvider().GetBuilder();

  delegate()->set_max_attributions_per_origin(
      AttributionReport::ReportType::kEventLevel, 1);
  storage()->StoreSource(source_builder.Build());
  storage()->StoreSource(source_builder.Build());

  EXPECT_THAT(storage()->MaybeCreateAndStoreReport(
                  DefaultAggregatableTriggerBuilder().Build()),
              AllOf(CreateReportEventLevelStatusIs(
                        AttributionTrigger::EventLevelResult::kSuccess),
                    CreateReportAggregatableStatusIs(
                        AttributionTrigger::AggregatableResult::kSuccess)));

  // Verify that MaxAttributionsPerOrigin is enforced.
  EXPECT_THAT(storage()->MaybeCreateAndStoreReport(
                  DefaultAggregatableTriggerBuilder().Build()),
              AllOf(CreateReportEventLevelStatusIs(
                        AttributionTrigger::EventLevelResult::
                            kNoCapacityForConversionDestination),
                    CreateReportAggregatableStatusIs(
                        AttributionTrigger::AggregatableResult::kSuccess),
                    ReplacedEventLevelReportIs(absl::nullopt)));
}

TEST_F(AttributionStorageTest, MaxAggregatableAttributionsPerOrigin) {
  SourceBuilder source_builder = TestAggregatableSourceProvider().GetBuilder();

  delegate()->set_max_attributions_per_origin(
      AttributionReport::ReportType::kAggregatableAttribution, 1);
  storage()->StoreSource(source_builder.Build());
  storage()->StoreSource(source_builder.Build());

  EXPECT_THAT(storage()->MaybeCreateAndStoreReport(
                  DefaultAggregatableTriggerBuilder().Build()),
              AllOf(CreateReportEventLevelStatusIs(
                        AttributionTrigger::EventLevelResult::kSuccess),
                    CreateReportAggregatableStatusIs(
                        AttributionTrigger::AggregatableResult::kSuccess)));

  // Verify that MaxAttributionsPerOrigin is enforced.
  EXPECT_THAT(storage()->MaybeCreateAndStoreReport(
                  DefaultAggregatableTriggerBuilder().Build()),
              AllOf(CreateReportEventLevelStatusIs(
                        AttributionTrigger::EventLevelResult::kSuccess),
                    CreateReportAggregatableStatusIs(
                        AttributionTrigger::AggregatableResult::
                            kNoCapacityForConversionDestination),
                    ReplacedEventLevelReportIs(absl::nullopt)));
}

TEST_F(AttributionStorageTest, ClearDataWithNoMatch_NoDelete) {
  base::Time now = base::Time::Now();
  auto impression = SourceBuilder(now).Build();
  storage()->StoreSource(impression);
  storage()->ClearData(
      now, now, GetMatcher(url::Origin::Create(GURL("https://no-match.com"))));
  EXPECT_EQ(AttributionTrigger::EventLevelResult::kSuccess,
            MaybeCreateAndStoreEventLevelReport(DefaultTrigger()));
}

TEST_F(AttributionStorageTest, ClearDataOutsideRange_NoDelete) {
  base::Time now = base::Time::Now();
  auto impression = SourceBuilder(now).Build();
  storage()->StoreSource(impression);

  storage()->ClearData(
      now + base::Minutes(10), now + base::Minutes(20),
      GetMatcher(impression.common_info().impression_origin()));
  EXPECT_EQ(AttributionTrigger::EventLevelResult::kSuccess,
            MaybeCreateAndStoreEventLevelReport(DefaultTrigger()));
}

TEST_F(AttributionStorageTest, ClearDataImpression) {
  base::Time now = base::Time::Now();

  {
    auto impression = SourceBuilder(now).Build();
    storage()->StoreSource(impression);
    storage()->ClearData(
        now, now + base::Minutes(20),
        GetMatcher(impression.common_info().conversion_origin()));
    EXPECT_EQ(AttributionTrigger::EventLevelResult::kNoMatchingImpressions,
              MaybeCreateAndStoreEventLevelReport(DefaultTrigger()));
  }
}

TEST_F(AttributionStorageTest, ClearDataImpressionConversion) {
  base::Time now = base::Time::Now();
  auto impression = SourceBuilder(now).Build();
  auto conversion = DefaultTrigger();

  storage()->StoreSource(impression);
  EXPECT_EQ(AttributionTrigger::EventLevelResult::kSuccess,
            MaybeCreateAndStoreEventLevelReport(conversion));

  storage()->ClearData(
      now - base::Minutes(20), now + base::Minutes(20),
      GetMatcher(impression.common_info().impression_origin()));

  EXPECT_THAT(storage()->GetAttributionReports(base::Time::Max()), IsEmpty());
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
    EXPECT_EQ(
        AttributionTrigger::EventLevelResult::kSuccess,
        MaybeCreateAndStoreEventLevelReport(TriggerBuilder()
                                                .SetDestinationOrigin(origin)
                                                .SetReportingOrigin(origin)
                                                .Build()));
  }
  task_environment_.FastForwardBy(base::Days(1));
  for (int i = 5; i < 10; i++) {
    auto origin =
        url::Origin::Create(GURL(base::StringPrintf("https://%d.com/", i)));
    EXPECT_EQ(
        AttributionTrigger::EventLevelResult::kSuccess,
        MaybeCreateAndStoreEventLevelReport(TriggerBuilder()
                                                .SetDestinationOrigin(origin)
                                                .SetReportingOrigin(origin)
                                                .Build()));
  }

  auto null_filter = base::RepeatingCallback<bool(const url::Origin&)>();
  storage()->ClearData(base::Time::Now(), base::Time::Now(), null_filter);
  EXPECT_THAT(storage()->GetAttributionReports(base::Time::Max()), SizeIs(5));
}

TEST_F(AttributionStorageTest, ClearDataWithImpressionOutsideRange) {
  base::Time start = base::Time::Now();
  auto impression = SourceBuilder(start).SetExpiry(base::Days(30)).Build();
  auto conversion = DefaultTrigger();

  storage()->StoreSource(impression);

  EXPECT_EQ(AttributionTrigger::EventLevelResult::kSuccess,
            MaybeCreateAndStoreEventLevelReport(conversion));
  storage()->ClearData(
      base::Time::Now(), base::Time::Now(),
      GetMatcher(impression.common_info().impression_origin()));
  EXPECT_THAT(storage()->GetAttributionReports(base::Time::Max()), IsEmpty());
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

  const AttributionReport expected_report = GetExpectedEventLevelReport(
      builder.SetDefaultFilterData().BuildStored(), conversion);

  EXPECT_EQ(AttributionTrigger::EventLevelResult::kSuccess,
            MaybeCreateAndStoreEventLevelReport(conversion));

  storage()->ClearData(
      start + base::Minutes(1), start + base::Minutes(10),
      GetMatcher(impression.common_info().impression_origin()));

  EXPECT_THAT(storage()->GetAttributionReports(base::Time::Max()),
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

  EXPECT_EQ(AttributionTrigger::EventLevelResult::kSuccess,
            MaybeCreateAndStoreEventLevelReport(DefaultTrigger()));

  // Only the first impression should overlap with this time range, but all the
  // impressions should share the origin.
  storage()->ClearData(
      start, start, GetMatcher(impression1.common_info().impression_origin()));
  EXPECT_THAT(storage()->GetAttributionReports(base::Time::Max()), SizeIs(1));
}

// The max time range with a null filter should delete everything.
TEST_F(AttributionStorageTest, DeleteAll) {
  base::Time start = base::Time::Now();
  for (int i = 0; i < 10; i++) {
    auto impression = SourceBuilder(start).SetExpiry(base::Days(30)).Build();
    storage()->StoreSource(impression);
    task_environment_.FastForwardBy(base::Days(1));
  }

  EXPECT_EQ(AttributionTrigger::EventLevelResult::kSuccess,
            MaybeCreateAndStoreEventLevelReport(DefaultTrigger()));
  task_environment_.FastForwardBy(base::Days(1));
  EXPECT_EQ(AttributionTrigger::EventLevelResult::kSuccess,
            MaybeCreateAndStoreEventLevelReport(DefaultTrigger()));

  auto null_filter = base::RepeatingCallback<bool(const url::Origin&)>();
  storage()->ClearData(base::Time::Min(), base::Time::Max(), null_filter);

  // Verify that everything is deleted.
  EXPECT_THAT(storage()->GetAttributionReports(base::Time::Max()), IsEmpty());
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

  EXPECT_EQ(AttributionTrigger::EventLevelResult::kSuccess,
            MaybeCreateAndStoreEventLevelReport(DefaultTrigger()));
  task_environment_.FastForwardBy(base::Days(1));
  EXPECT_EQ(AttributionTrigger::EventLevelResult::kSuccess,
            MaybeCreateAndStoreEventLevelReport(DefaultTrigger()));

  auto null_filter = base::RepeatingCallback<bool(const url::Origin&)>();
  storage()->ClearData(base::Time(), base::Time::Max(), null_filter);

  // Verify that everything is deleted.
  EXPECT_THAT(storage()->GetAttributionReports(base::Time::Max()), IsEmpty());
}

TEST_F(AttributionStorageTest, MaxAttributionsBetweenSites) {
  delegate()->set_rate_limits({
      .time_window = base::TimeDelta::Max(),
      .max_source_registration_reporting_origins =
          std::numeric_limits<int64_t>::max(),
      .max_attribution_reporting_origins = std::numeric_limits<int64_t>::max(),
      .max_attributions = 2,
  });

  SourceBuilder source_builder = TestAggregatableSourceProvider().GetBuilder();
  storage()->StoreSource(source_builder.Build());

  auto conversion1 = DefaultTrigger();
  EXPECT_THAT(
      storage()->MaybeCreateAndStoreReport(conversion1),
      AllOf(CreateReportEventLevelStatusIs(
                AttributionTrigger::EventLevelResult::kSuccess),
            CreateReportAggregatableStatusIs(
                AttributionTrigger::AggregatableResult::kNotRegistered)));

  auto conversion2 =
      DefaultAggregatableTriggerBuilder(/*histogram_values=*/{5}).Build();
  EXPECT_THAT(storage()->MaybeCreateAndStoreReport(conversion2),
              AllOf(CreateReportEventLevelStatusIs(
                        AttributionTrigger::EventLevelResult::kSuccess),
                    CreateReportAggregatableStatusIs(
                        AttributionTrigger::AggregatableResult::kSuccess)));

  // Event-level reports and aggregatable reports share the attribution limit.
  EXPECT_THAT(
      storage()->MaybeCreateAndStoreReport(conversion2),
      AllOf(CreateReportEventLevelStatusIs(
                AttributionTrigger::EventLevelResult::kExcessiveAttributions),
            CreateReportAggregatableStatusIs(
                AttributionTrigger::AggregatableResult::kExcessiveAttributions),
            ReplacedEventLevelReportIs(absl::nullopt)));

  const auto source = source_builder.SetDefaultFilterData().BuildStored();
  auto contributions =
      DefaultAggregatableHistogramContributions(/*histogram_values=*/{5});
  ASSERT_THAT(contributions, SizeIs(1));
  EXPECT_THAT(storage()->GetAttributionReports(base::Time::Max()),
              ElementsAre(GetExpectedEventLevelReport(source, conversion1),
                          GetExpectedEventLevelReport(source, conversion2),
                          GetExpectedAggregatableReport(
                              source, std::move(contributions))));
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

  storage()->StoreSource(SourceBuilder()
                             .SetSourceType(AttributionSourceType::kNavigation)
                             .Build());
  EXPECT_EQ(AttributionTrigger::EventLevelResult::kSuccess,
            MaybeCreateAndStoreEventLevelReport(DefaultTrigger()));

  storage()->StoreSource(
      SourceBuilder().SetSourceType(AttributionSourceType::kEvent).Build());
  // This would fail if the source types had separate limits.
  EXPECT_EQ(AttributionTrigger::EventLevelResult::kExcessiveAttributions,
            MaybeCreateAndStoreEventLevelReport(DefaultTrigger()));
}

TEST_F(AttributionStorageTest,
       NeverAttributeImpression_EventLevelReportNotStored) {
  delegate()->set_max_attributions_per_source(1);

  delegate()->set_randomized_response(
      std::vector<AttributionStorageDelegate::FakeReport>{});
  storage()->StoreSource(TestAggregatableSourceProvider().GetBuilder().Build());
  delegate()->set_randomized_response(absl::nullopt);

  EXPECT_THAT(storage()->MaybeCreateAndStoreReport(
                  DefaultAggregatableTriggerBuilder().Build()),
              AllOf(CreateReportEventLevelStatusIs(
                        AttributionTrigger::EventLevelResult::kDroppedForNoise),
                    CreateReportAggregatableStatusIs(
                        AttributionTrigger::AggregatableResult::kSuccess)));

  task_environment_.FastForwardBy(kReportDelay);

  EXPECT_THAT(storage()->GetAttributionReports(base::Time::Now()),
              ElementsAre(AggregatableAttributionDataIs(
                  AggregatableHistogramContributionsAre(
                      DefaultAggregatableHistogramContributions()))));
}

TEST_F(AttributionStorageTest, NeverAttributeImpression_Deactivates) {
  delegate()->set_max_attributions_per_source(1);

  delegate()->set_randomized_response(
      std::vector<AttributionStorageDelegate::FakeReport>{});
  storage()->StoreSource(SourceBuilder().SetSourceEventId(3).Build());
  delegate()->set_randomized_response(absl::nullopt);

  EXPECT_EQ(AttributionTrigger::EventLevelResult::kDroppedForNoise,
            MaybeCreateAndStoreEventLevelReport(DefaultTrigger()));

  storage()->StoreSource(SourceBuilder().SetSourceEventId(5).Build());

  EXPECT_EQ(AttributionTrigger::EventLevelResult::kSuccess,
            MaybeCreateAndStoreEventLevelReport(
                TriggerBuilder().SetTriggerData(7).Build()));

  task_environment_.FastForwardBy(kReportDelay);

  EXPECT_THAT(storage()->GetAttributionReports(base::Time::Now()),
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
  EXPECT_EQ(AttributionTrigger::EventLevelResult::kDroppedForNoise,
            MaybeCreateAndStoreEventLevelReport(conversion));

  SourceBuilder builder;
  builder.SetSourceEventId(7);
  storage()->StoreSource(builder.Build());
  EXPECT_EQ(AttributionTrigger::EventLevelResult::kSuccess,
            MaybeCreateAndStoreEventLevelReport(conversion));

  storage()->StoreSource(SourceBuilder().SetSourceEventId(9).Build());
  EXPECT_EQ(AttributionTrigger::EventLevelResult::kExcessiveAttributions,
            MaybeCreateAndStoreEventLevelReport(conversion));

  const AttributionReport expected_report = GetExpectedEventLevelReport(
      builder.SetDefaultFilterData()
          .SetActiveState(StoredSource::ActiveState::kInactive)
          .BuildStored(),
      conversion);

  task_environment_.FastForwardBy(kReportDelay);

  EXPECT_THAT(storage()->GetAttributionReports(base::Time::Now()),
              ElementsAre(expected_report));
}

TEST_F(AttributionStorageTest,
       NeverAttributeSource_AggregatableReportStoredAndRateLimitsChanged) {
  delegate()->set_rate_limits({
      .time_window = base::TimeDelta::Max(),
      .max_source_registration_reporting_origins =
          std::numeric_limits<int64_t>::max(),
      .max_attribution_reporting_origins = std::numeric_limits<int64_t>::max(),
      .max_attributions = 1,
  });

  SourceBuilder builder = TestAggregatableSourceProvider().GetBuilder();

  delegate()->set_randomized_response(
      std::vector<AttributionStorageDelegate::FakeReport>{});
  storage()->StoreSource(builder.SetSourceEventId(5).Build());
  delegate()->set_randomized_response(absl::nullopt);

  const auto trigger = DefaultAggregatableTriggerBuilder().Build();
  EXPECT_EQ(AttributionTrigger::AggregatableResult::kSuccess,
            MaybeCreateAndStoreAggregatableReport(trigger));

  storage()->StoreSource(builder.SetSourceEventId(7).Build());
  EXPECT_EQ(AttributionTrigger::AggregatableResult::kExcessiveAttributions,
            MaybeCreateAndStoreAggregatableReport(trigger));

  const AttributionReport expected_report = GetExpectedAggregatableReport(
      builder.SetDefaultFilterData()
          .SetSourceEventId(5)
          .SetAttributionLogic(StoredSource::AttributionLogic::kNever)
          .SetActiveState(StoredSource::ActiveState::kInactive)
          .BuildStored(),
      DefaultAggregatableHistogramContributions());

  task_environment_.FastForwardBy(kReportDelay);

  EXPECT_THAT(storage()->GetAttributionReports(base::Time::Now()),
              ElementsAre(expected_report));
}

TEST_F(AttributionStorageTest,
       NeverAndTruthfullyAttributeImpressions_EventLevelReportNotStored) {
  TestAggregatableSourceProvider provider;

  storage()->StoreSource(provider.GetBuilder().Build());

  task_environment_.FastForwardBy(base::Milliseconds(1));

  delegate()->set_randomized_response(
      std::vector<AttributionStorageDelegate::FakeReport>{});

  storage()->StoreSource(provider.GetBuilder().Build());
  delegate()->set_randomized_response(absl::nullopt);

  const auto conversion = DefaultAggregatableTriggerBuilder().Build();

  EXPECT_THAT(storage()->MaybeCreateAndStoreReport(conversion),
              AllOf(CreateReportEventLevelStatusIs(
                        AttributionTrigger::EventLevelResult::kDroppedForNoise),
                    CreateReportAggregatableStatusIs(
                        AttributionTrigger::AggregatableResult::kSuccess)));

  EXPECT_THAT(storage()->MaybeCreateAndStoreReport(conversion),
              AllOf(CreateReportEventLevelStatusIs(
                        AttributionTrigger::EventLevelResult::kDroppedForNoise),
                    CreateReportAggregatableStatusIs(
                        AttributionTrigger::AggregatableResult::kSuccess)));

  task_environment_.FastForwardBy(kReportDelay);

  auto contributions = DefaultAggregatableHistogramContributions();
  EXPECT_THAT(
      storage()->GetAttributionReports(base::Time::Now()),
      ElementsAre(AggregatableAttributionDataIs(
                      AggregatableHistogramContributionsAre(contributions)),
                  AggregatableAttributionDataIs(
                      AggregatableHistogramContributionsAre(contributions))));
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
          .SetSourceType(AttributionSourceType::kNavigation)
          .Build());
  storage()->StoreSource(
      SourceBuilder()
          .SetConversionOrigin(url::Origin::Create(GURL("https://b.example")))
          .SetSourceType(AttributionSourceType::kEvent)
          .Build());

  EXPECT_THAT(storage()->GetActiveSources(), SizeIs(1));
}

TEST_F(AttributionStorageTest,
       MultipleImpressionsPerConversion_MostRecentAttributesForSamePriority) {
  storage()->StoreSource(SourceBuilder().SetSourceEventId(3).Build());

  task_environment_.FastForwardBy(base::Milliseconds(1));
  storage()->StoreSource(SourceBuilder().SetSourceEventId(7).Build());

  task_environment_.FastForwardBy(base::Milliseconds(1));
  storage()->StoreSource(SourceBuilder().SetSourceEventId(5).Build());

  EXPECT_THAT(storage()->GetActiveSources(), SizeIs(3));
  EXPECT_EQ(AttributionTrigger::EventLevelResult::kSuccess,
            MaybeCreateAndStoreEventLevelReport(DefaultTrigger()));

  task_environment_.FastForwardBy(kReportDelay);

  EXPECT_THAT(storage()->GetAttributionReports(base::Time::Now()),
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
  EXPECT_EQ(AttributionTrigger::EventLevelResult::kSuccess,
            MaybeCreateAndStoreEventLevelReport(DefaultTrigger()));

  task_environment_.FastForwardBy(kReportDelay);

  EXPECT_THAT(storage()->GetAttributionReports(base::Time::Now()),
              ElementsAre(ReportSourceIs(SourceEventIdIs(5u))));
}

TEST_F(AttributionStorageTest, MultipleImpressions_CorrectDeactivation) {
  storage()->StoreSource(
      SourceBuilder().SetSourceEventId(3).SetPriority(0).Build());
  storage()->StoreSource(
      SourceBuilder().SetSourceEventId(5).SetPriority(1).Build());
  EXPECT_THAT(storage()->GetActiveSources(), SizeIs(2));

  EXPECT_EQ(AttributionTrigger::EventLevelResult::kSuccess,
            MaybeCreateAndStoreEventLevelReport(DefaultTrigger()));

  // Because the impression with data 5 has the highest priority, it is selected
  // for attribution. The unselected impression with data 3 should be
  // deactivated, but the one with data 5 should remain active.
  EXPECT_THAT(storage()->GetActiveSources(), ElementsAre(SourceEventIdIs(5u)));
}

TEST_F(AttributionStorageTest, FalselyAttributeImpression_ReportStored) {
  delegate()->set_max_attributions_per_source(1);

  const base::Time fake_report_time = base::Time::Now() + kReportDelay;

  SourceBuilder builder = TestAggregatableSourceProvider().GetBuilder();
  builder.SetSourceEventId(4)
      .SetSourceType(AttributionSourceType::kEvent)
      .SetPriority(100);
  delegate()->set_randomized_response(
      std::vector<AttributionStorageDelegate::FakeReport>{
          {.trigger_data = 7, .report_time = fake_report_time}});
  storage()->StoreSource(builder.Build());
  delegate()->set_randomized_response(absl::nullopt);

  const AttributionReport expected_event_level_report =
      ReportBuilder(
          AttributionInfoBuilder(
              builder
                  .SetAttributionLogic(StoredSource::AttributionLogic::kFalsely)
                  .SetDefaultFilterData()
                  .SetActiveState(StoredSource::ActiveState::
                                      kReachedEventLevelAttributionLimit)
                  .BuildStored())
              .SetTime(base::Time::Now())
              .Build())
          .SetTriggerData(7)
          .SetReportTime(fake_report_time)
          .Build();

  task_environment_.FastForwardBy(kReportDelay);

  EXPECT_THAT(storage()->GetAttributionReports(base::Time::Now()),
              ElementsAre(expected_event_level_report));

  EXPECT_THAT(
      storage()->GetActiveSources(),
      ElementsAre(SourceActiveStateIs(
          StoredSource::ActiveState::kReachedEventLevelAttributionLimit)));

  // The falsely attributed impression should only be eligible for further
  // aggregatable reports, but not event-level reports.
  EXPECT_THAT(
      storage()->MaybeCreateAndStoreReport(
          DefaultAggregatableTriggerBuilder().Build()),
      AllOf(CreateReportEventLevelStatusIs(
                AttributionTrigger::EventLevelResult::kNoMatchingImpressions),
            CreateReportAggregatableStatusIs(
                AttributionTrigger::AggregatableResult::kSuccess)));

  const AttributionReport expected_aggregatable_report =
      GetExpectedAggregatableReport(
          builder.BuildStored(), DefaultAggregatableHistogramContributions());

  task_environment_.FastForwardBy(kReportDelay);

  EXPECT_THAT(
      storage()->GetAttributionReports(base::Time::Now()),
      ElementsAre(expected_event_level_report, expected_aggregatable_report));
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
              AllOf(CreateReportEventLevelStatusIs(
                        AttributionTrigger::EventLevelResult::kSuccess),
                    ReplacedEventLevelReportIs(absl::nullopt)));

  // This conversion should replace the one above because it has a higher
  // priority.
  EXPECT_THAT(storage()->MaybeCreateAndStoreReport(
                  TriggerBuilder().SetPriority(2).SetTriggerData(21).Build()),
              AllOf(CreateReportEventLevelStatusIs(
                        AttributionTrigger::EventLevelResult::
                            kSuccessDroppedLowerPriority),
                    ReplacedEventLevelReportIs(
                        Optional(EventLevelDataIs(TriggerDataIs(20u))))));

  storage()->StoreSource(
      SourceBuilder().SetSourceEventId(7).SetPriority(2).Build());

  EXPECT_EQ(AttributionTrigger::EventLevelResult::kSuccess,
            MaybeCreateAndStoreEventLevelReport(
                TriggerBuilder().SetPriority(1).SetTriggerData(22).Build()));
  // This conversion should be dropped because it has a lower priority than the
  // one above.
  EXPECT_THAT(storage()->MaybeCreateAndStoreReport(
                  TriggerBuilder().SetPriority(0).SetTriggerData(23).Build()),
              AllOf(CreateReportEventLevelStatusIs(
                        AttributionTrigger::EventLevelResult::kPriorityTooLow),
                    ReplacedEventLevelReportIs(absl::nullopt)));

  task_environment_.FastForwardBy(kReportDelay);

  EXPECT_THAT(storage()->GetAttributionReports(base::Time::Now()),
              ElementsAre(AllOf(ReportSourceIs(SourceEventIdIs(5u)),
                                EventLevelDataIs(TriggerDataIs(21u))),
                          AllOf(ReportSourceIs(SourceEventIdIs(7u)),
                                EventLevelDataIs(TriggerDataIs(22u)))));
}

TEST_F(AttributionStorageTest, TriggerPriority_Simple) {
  delegate()->set_max_attributions_per_source(1);

  storage()->StoreSource(SourceBuilder().Build());

  int i = 0;
  EXPECT_EQ(AttributionTrigger::EventLevelResult::kSuccess,
            MaybeCreateAndStoreEventLevelReport(
                TriggerBuilder().SetPriority(i).SetTriggerData(i).Build()));
  i++;

  for (; i < 10; i++) {
    EXPECT_EQ(
        AttributionTrigger::EventLevelResult::kSuccessDroppedLowerPriority,
        MaybeCreateAndStoreEventLevelReport(
            TriggerBuilder().SetPriority(i).SetTriggerData(i).Build()));
  }

  task_environment_.FastForwardBy(kReportDelay);

  EXPECT_THAT(storage()->GetAttributionReports(base::Time::Now()),
              ElementsAre(EventLevelDataIs(TriggerDataIs(9u))));
}

TEST_F(AttributionStorageTest, TriggerPriority_SamePriorityDeletesMostRecent) {
  delegate()->set_max_attributions_per_source(2);

  storage()->StoreSource(SourceBuilder().Build());

  EXPECT_EQ(AttributionTrigger::EventLevelResult::kSuccess,
            MaybeCreateAndStoreEventLevelReport(
                TriggerBuilder().SetPriority(1).SetTriggerData(3).Build()));

  task_environment_.FastForwardBy(base::Milliseconds(1));
  EXPECT_EQ(AttributionTrigger::EventLevelResult::kSuccess,
            MaybeCreateAndStoreEventLevelReport(
                TriggerBuilder().SetPriority(1).SetTriggerData(2).Build()));

  // This report should not be stored, as even though it has the same priority
  // as the previous two, it is the most recent.
  task_environment_.FastForwardBy(base::Milliseconds(1));
  EXPECT_EQ(AttributionTrigger::EventLevelResult::kPriorityTooLow,
            MaybeCreateAndStoreEventLevelReport(
                TriggerBuilder().SetPriority(1).SetTriggerData(8).Build()));

  // This report should be stored by replacing the one with `trigger_data ==
  // 2`, which is the most recent of the two with `priority == 1`.
  task_environment_.FastForwardBy(base::Milliseconds(1));
  EXPECT_EQ(AttributionTrigger::EventLevelResult::kSuccessDroppedLowerPriority,
            MaybeCreateAndStoreEventLevelReport(
                TriggerBuilder().SetPriority(2).SetTriggerData(5).Build()));

  EXPECT_THAT(storage()->GetAttributionReports(base::Time::Max()),
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

  EXPECT_EQ(AttributionTrigger::EventLevelResult::kSuccess,
            MaybeCreateAndStoreEventLevelReport(DefaultTrigger()));

  // Because the impression with data 5 has the highest priority, it is selected
  // for attribution. The unselected impression with data 3 should be
  // deactivated, but the one with data 5 should remain active.
  EXPECT_THAT(storage()->GetActiveSources(), ElementsAre(SourceEventIdIs(5u)));

  // Ensure that the next report is in a different window.
  delegate()->set_report_delay(kReportDelay + base::Milliseconds(1));

  // This conversion should not be stored because all reports for the attributed
  // impression were in an earlier window.
  EXPECT_EQ(AttributionTrigger::EventLevelResult::kPriorityTooLow,
            MaybeCreateAndStoreEventLevelReport(
                TriggerBuilder().SetPriority(2).Build()));

  // As a result, the impression with data 5 should have reached event-level
  // attribution limit.
  EXPECT_THAT(
      storage()->GetActiveSources(),
      ElementsAre(SourceActiveStateIs(
          StoredSource::ActiveState::kReachedEventLevelAttributionLimit)));
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

  EXPECT_EQ(AttributionTrigger::EventLevelResult::kSuccess,
            MaybeCreateAndStoreEventLevelReport(
                TriggerBuilder()
                    .SetDestinationOrigin(
                        url::Origin::Create(GURL("https://a.example")))
                    .SetDedupKey(11)
                    .SetTriggerData(71)
                    .Build()));

  // Should be stored because dedup key doesn't match even though conversion
  // destination does.
  EXPECT_EQ(AttributionTrigger::EventLevelResult::kSuccess,
            MaybeCreateAndStoreEventLevelReport(
                TriggerBuilder()
                    .SetDestinationOrigin(
                        url::Origin::Create(GURL("https://a.example")))
                    .SetDedupKey(12)
                    .SetTriggerData(72)
                    .Build()));

  // Should be stored because conversion destination doesn't match even though
  // dedup key does.
  EXPECT_EQ(AttributionTrigger::EventLevelResult::kSuccess,
            MaybeCreateAndStoreEventLevelReport(
                TriggerBuilder()
                    .SetDestinationOrigin(
                        url::Origin::Create(GURL("https://b.example")))
                    .SetDedupKey(12)
                    .SetTriggerData(73)
                    .Build()));

  // Shouldn't be stored because conversion destination and dedup key match.
  auto result = storage()->MaybeCreateAndStoreReport(
      TriggerBuilder()
          .SetDestinationOrigin(url::Origin::Create(GURL("https://a.example")))
          .SetDedupKey(11)
          .SetTriggerData(74)
          .Build());
  EXPECT_EQ(AttributionTrigger::EventLevelResult::kDeduplicated,
            result.event_level_status());
  EXPECT_EQ(result.replaced_event_level_report(), absl::nullopt);

  // Shouldn't be stored because conversion destination and dedup key match.
  EXPECT_EQ(AttributionTrigger::EventLevelResult::kDeduplicated,
            MaybeCreateAndStoreEventLevelReport(
                TriggerBuilder()
                    .SetDestinationOrigin(
                        url::Origin::Create(GURL("https://b.example")))
                    .SetDedupKey(12)
                    .SetTriggerData(75)
                    .Build()));

  task_environment_.FastForwardBy(kReportDelay);
  EXPECT_THAT(storage()->GetAttributionReports(base::Time::Now()),
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

  EXPECT_EQ(AttributionTrigger::EventLevelResult::kSuccess,
            MaybeCreateAndStoreEventLevelReport(
                TriggerBuilder()
                    .SetDestinationOrigin(
                        url::Origin::Create(GURL("https://a.example")))
                    .SetDedupKey(2)
                    .SetTriggerData(3)
                    .Build()));

  task_environment_.FastForwardBy(kReportDelay);

  std::vector<AttributionReport> actual_reports =
      storage()->GetAttributionReports(base::Time::Now());
  EXPECT_THAT(actual_reports, ElementsAre(EventLevelDataIs(TriggerDataIs(3u))));

  // Simulate the report being sent and deleted from storage.
  DeleteReports(actual_reports);

  task_environment_.FastForwardBy(base::Milliseconds(1));

  // This report shouldn't be stored, as it should be deduped against the
  // previously stored one even though that previous one is no longer in the DB.
  EXPECT_EQ(AttributionTrigger::EventLevelResult::kDeduplicated,
            MaybeCreateAndStoreEventLevelReport(
                TriggerBuilder()
                    .SetDestinationOrigin(
                        url::Origin::Create(GURL("https://a.example")))
                    .SetDedupKey(2)
                    .SetTriggerData(5)
                    .Build()));

  task_environment_.FastForwardBy(kReportDelay);
  EXPECT_THAT(storage()->GetAttributionReports(base::Time::Now()), IsEmpty());
}

TEST_F(AttributionStorageTest, GetAttributionReports_SetsPriority) {
  storage()->StoreSource(SourceBuilder().Build());
  EXPECT_EQ(AttributionTrigger::EventLevelResult::kSuccess,
            MaybeCreateAndStoreEventLevelReport(
                TriggerBuilder().SetPriority(13).Build()));

  task_environment_.FastForwardBy(kReportDelay);

  EXPECT_THAT(storage()->GetAttributionReports(base::Time::Now()),
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
  EXPECT_EQ(AttributionTrigger::EventLevelResult::kSuccess,
            MaybeCreateAndStoreEventLevelReport(DefaultTrigger()));
  auto reports = storage()->GetAttributionReports(base::Time::Max());
  ASSERT_THAT(reports, SizeIs(1));
  const AttributionReport::Id id1 = reports.front().ReportId();

  storage()->ClearData(base::Time::Min(), base::Time::Max(),
                       base::NullCallback());
  EXPECT_THAT(storage()->GetAttributionReports(base::Time::Max()), IsEmpty());

  storage()->StoreSource(SourceBuilder().Build());
  EXPECT_EQ(AttributionTrigger::EventLevelResult::kSuccess,
            MaybeCreateAndStoreEventLevelReport(DefaultTrigger()));
  reports = storage()->GetAttributionReports(base::Time::Max());
  ASSERT_THAT(reports, SizeIs(1));
  const AttributionReport::Id id2 = reports.front().ReportId();

  EXPECT_NE(id1, id2);
}

TEST_F(AttributionStorageTest, UpdateReportForSendFailure) {
  storage()->StoreSource(TestAggregatableSourceProvider().GetBuilder().Build());
  EXPECT_THAT(storage()->MaybeCreateAndStoreReport(
                  DefaultAggregatableTriggerBuilder().Build()),
              AllOf(CreateReportEventLevelStatusIs(
                        AttributionTrigger::EventLevelResult::kSuccess),
                    CreateReportAggregatableStatusIs(
                        AttributionTrigger::AggregatableResult::kSuccess)));

  task_environment_.FastForwardBy(kReportDelay);

  std::vector<AttributionReport> actual_reports =
      storage()->GetAttributionReports(base::Time::Now());
  EXPECT_THAT(
      actual_reports,
      ElementsAre(
          AllOf(ReportTypeIs(AttributionReport::ReportType::kEventLevel),
                FailedSendAttemptsIs(0)),
          AllOf(ReportTypeIs(
                    AttributionReport::ReportType::kAggregatableAttribution),
                FailedSendAttemptsIs(0))));

  const base::TimeDelta delay = base::Days(2);
  const base::Time new_report_time = actual_reports[0].report_time() + delay;
  EXPECT_TRUE(storage()->UpdateReportForSendFailure(
      actual_reports[0].ReportId(), new_report_time));
  EXPECT_TRUE(storage()->UpdateReportForSendFailure(
      actual_reports[1].ReportId(), new_report_time));

  task_environment_.FastForwardBy(delay);

  EXPECT_THAT(
      storage()->GetAttributionReports(base::Time::Now()),
      ElementsAre(
          AllOf(FailedSendAttemptsIs(1), ReportTimeIs(new_report_time)),
          AllOf(FailedSendAttemptsIs(1), ReportTimeIs(new_report_time))));
}

TEST_F(AttributionStorageTest, StoreSource_ReturnsDeactivatedSources) {
  SourceBuilder builder1;
  builder1.SetSourceEventId(7);

  EXPECT_THAT(storage()->StoreSource(builder1.Build()).deactivated_sources,
              IsEmpty());
  EXPECT_THAT(storage()->GetActiveSources(), SizeIs(1));

  task_environment_.FastForwardBy(kReportDelay);

  // Set a dedup key to ensure that the return deactivated source contains it.
  EXPECT_EQ(AttributionTrigger::EventLevelResult::kSuccess,
            MaybeCreateAndStoreEventLevelReport(
                TriggerBuilder().SetDedupKey(13).Build()));
  EXPECT_THAT(storage()->GetAttributionReports(base::Time::Now()), SizeIs(1));

  SourceBuilder builder2;
  builder2.SetSourceEventId(9);

  builder1.SetDedupKeys({13});
  EXPECT_THAT(storage()->StoreSource(builder2.Build()).deactivated_sources,
              ElementsAre(builder1.SetDefaultFilterData().BuildStored()));

  EXPECT_THAT(storage()->GetActiveSources(),
              ElementsAre(builder2.SetDefaultFilterData().BuildStored()));
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

  EXPECT_EQ(AttributionTrigger::EventLevelResult::kSuccess,
            MaybeCreateAndStoreEventLevelReport(DefaultTrigger()));
  EXPECT_EQ(AttributionTrigger::EventLevelResult::kSuccess,
            MaybeCreateAndStoreEventLevelReport(DefaultTrigger()));
  EXPECT_THAT(storage()->GetAttributionReports(base::Time::Now()), SizeIs(2));

  // 2 sources are deactivated, but only 1 should be returned.
  SourceBuilder builder3;
  builder3.SetSourceEventId(3);
  EXPECT_THAT(storage()
                  ->StoreSource(builder3.Build(),
                                /*deactivated_source_return_limit=*/1)
                  .deactivated_sources,
              ElementsAre(builder1.SetDefaultFilterData().BuildStored()));
  EXPECT_THAT(storage()->GetActiveSources(),
              ElementsAre(builder3.SetDefaultFilterData().BuildStored()));
}

TEST_F(AttributionStorageTest,
       MaybeCreateAndStoreEventLevelReport_ReturnsDeactivatedSources) {
  SourceBuilder builder;
  builder.SetSourceEventId(7);
  EXPECT_THAT(storage()->StoreSource(builder.Build()).deactivated_sources,
              IsEmpty());
  EXPECT_THAT(storage()->GetActiveSources(), SizeIs(1));

  // Store the maximum number of reports for the source.
  for (size_t i = 1; i <= kMaxConversions; i++) {
    EXPECT_EQ(AttributionTrigger::EventLevelResult::kSuccess,
              MaybeCreateAndStoreEventLevelReport(DefaultTrigger()));
  }

  task_environment_.FastForwardBy(kReportDelay);
  auto reports = storage()->GetAttributionReports(base::Time::Now());
  EXPECT_THAT(reports, SizeIs(3));

  // Simulate the reports being sent and removed from storage.
  DeleteReports(reports);

  // The next trigger should cause the source to reach event-level attribution
  // limit; the report itself shouldn't be stored as we've already reached the
  // maximum number of event-level reports per source.
  EXPECT_THAT(storage()->MaybeCreateAndStoreReport(DefaultTrigger()),
              AllOf(CreateReportEventLevelStatusIs(
                        AttributionTrigger::EventLevelResult::kPriorityTooLow),
                    ReplacedEventLevelReportIs(absl::nullopt)));
  EXPECT_THAT(
      storage()->GetActiveSources(),
      ElementsAre(SourceActiveStateIs(
          StoredSource::ActiveState::kReachedEventLevelAttributionLimit)));
}

TEST_F(AttributionStorageTest, ReportID_RoundTrips) {
  storage()->StoreSource(SourceBuilder().Build());
  EXPECT_EQ(AttributionTrigger::EventLevelResult::kSuccess,
            MaybeCreateAndStoreEventLevelReport(DefaultTrigger()));

  task_environment_.FastForwardBy(kReportDelay);

  std::vector<AttributionReport> actual_reports =
      storage()->GetAttributionReports(base::Time::Now());
  EXPECT_EQ(1u, actual_reports.size());
  EXPECT_EQ(DefaultExternalReportID(), actual_reports[0].external_report_id());
}

TEST_F(AttributionStorageTest, AdjustOfflineReportTimes) {
  EXPECT_EQ(storage()->AdjustOfflineReportTimes(), absl::nullopt);

  delegate()->set_offline_report_delay_config(
      AttributionStorageDelegate::OfflineReportDelayConfig{
          .min = base::Hours(1), .max = base::Hours(1)});
  EXPECT_EQ(storage()->AdjustOfflineReportTimes(), absl::nullopt);

  storage()->StoreSource(TestAggregatableSourceProvider().GetBuilder().Build());
  EXPECT_THAT(storage()->MaybeCreateAndStoreReport(
                  DefaultAggregatableTriggerBuilder().Build()),
              AllOf(CreateReportEventLevelStatusIs(
                        AttributionTrigger::EventLevelResult::kSuccess),
                    CreateReportAggregatableStatusIs(
                        AttributionTrigger::AggregatableResult::kSuccess)));

  const base::Time original_report_time = base::Time::Now() + kReportDelay;

  EXPECT_THAT(
      storage()->GetAttributionReports(base::Time::Max()),
      ElementsAre(ReportTimeIs(original_report_time),
                  AllOf(ReportTimeIs(original_report_time),
                        AggregatableAttributionDataIs(
                            InitialReportTimeIs(original_report_time)))));

  task_environment_.FastForwardBy(kReportDelay);

  EXPECT_EQ(storage()->AdjustOfflineReportTimes(), original_report_time);

  // The report time should not be changed as it is equal to now, not strictly
  // less than it.
  EXPECT_THAT(
      storage()->GetAttributionReports(base::Time::Max()),
      ElementsAre(ReportTimeIs(original_report_time),
                  AllOf(ReportTimeIs(original_report_time),
                        AggregatableAttributionDataIs(
                            InitialReportTimeIs(original_report_time)))));

  task_environment_.FastForwardBy(base::Milliseconds(1));

  const base::Time new_report_time = base::Time::Now() + base::Hours(1);

  EXPECT_EQ(storage()->AdjustOfflineReportTimes(), new_report_time);

  // The report time should be changed as it is strictly less than now.
  EXPECT_THAT(
      storage()->GetAttributionReports(base::Time::Max()),
      ElementsAre(ReportTimeIs(new_report_time),
                  AllOf(ReportTimeIs(new_report_time),
                        AggregatableAttributionDataIs(
                            InitialReportTimeIs(original_report_time)))));
}

TEST_F(AttributionStorageTest, AdjustOfflineReportTimes_Range) {
  delegate()->set_offline_report_delay_config(
      AttributionStorageDelegate::OfflineReportDelayConfig{
          .min = base::Hours(1), .max = base::Hours(3)});

  storage()->StoreSource(TestAggregatableSourceProvider().GetBuilder().Build());
  EXPECT_THAT(storage()->MaybeCreateAndStoreReport(
                  DefaultAggregatableTriggerBuilder().Build()),
              AllOf(CreateReportEventLevelStatusIs(
                        AttributionTrigger::EventLevelResult::kSuccess),
                    CreateReportAggregatableStatusIs(
                        AttributionTrigger::AggregatableResult::kSuccess)));

  const base::Time original_report_time = base::Time::Now() + kReportDelay;

  EXPECT_THAT(
      storage()->GetAttributionReports(base::Time::Max()),
      ElementsAre(ReportTimeIs(original_report_time),
                  AllOf(ReportTimeIs(original_report_time),
                        AggregatableAttributionDataIs(
                            InitialReportTimeIs(original_report_time)))));

  task_environment_.FastForwardBy(kReportDelay + base::Milliseconds(1));

  storage()->AdjustOfflineReportTimes();

  EXPECT_THAT(
      storage()->GetAttributionReports(base::Time::Max()),
      ElementsAre(
          ReportTimeIs(AllOf(Ge(base::Time::Now() + base::Hours(1)),
                             Le(base::Time::Now() + base::Hours(3)))),
          AllOf(ReportTimeIs(AllOf(Ge(base::Time::Now() + base::Hours(1)),
                                   Le(base::Time::Now() + base::Hours(3)))),
                AggregatableAttributionDataIs(
                    InitialReportTimeIs(original_report_time)))));
}

TEST_F(AttributionStorageTest, GetNextEventReportTime) {
  const auto origin_a = url::Origin::Create(GURL("https://a.example/"));
  const auto origin_b = url::Origin::Create(GURL("https://b.example/"));

  EXPECT_EQ(storage()->GetNextReportTime(base::Time::Min()), absl::nullopt);

  storage()->StoreSource(SourceBuilder().SetReportingOrigin(origin_a).Build());
  EXPECT_EQ(AttributionTrigger::EventLevelResult::kSuccess,
            MaybeCreateAndStoreEventLevelReport(
                TriggerBuilder().SetReportingOrigin(origin_a).Build()));

  const base::Time report_time_a = base::Time::Now() + kReportDelay;

  EXPECT_EQ(storage()->GetNextReportTime(base::Time::Min()), report_time_a);
  EXPECT_EQ(storage()->GetNextReportTime(report_time_a), absl::nullopt);

  task_environment_.FastForwardBy(base::Milliseconds(1));
  storage()->StoreSource(SourceBuilder().SetReportingOrigin(origin_b).Build());
  EXPECT_EQ(AttributionTrigger::EventLevelResult::kSuccess,
            MaybeCreateAndStoreEventLevelReport(
                TriggerBuilder().SetReportingOrigin(origin_b).Build()));

  const base::Time report_time_b = base::Time::Now() + kReportDelay;

  EXPECT_EQ(storage()->GetNextReportTime(base::Time::Min()), report_time_a);
  EXPECT_EQ(storage()->GetNextReportTime(report_time_a), report_time_b);
  EXPECT_EQ(storage()->GetNextReportTime(report_time_b), absl::nullopt);
}

TEST_F(AttributionStorageTest, GetAttributionReports_Shuffles) {
  storage()->StoreSource(SourceBuilder().Build());
  EXPECT_EQ(AttributionTrigger::EventLevelResult::kSuccess,
            MaybeCreateAndStoreEventLevelReport(
                TriggerBuilder().SetTriggerData(3).Build()));
  EXPECT_EQ(AttributionTrigger::EventLevelResult::kSuccess,
            MaybeCreateAndStoreEventLevelReport(
                TriggerBuilder().SetTriggerData(1).Build()));
  EXPECT_EQ(AttributionTrigger::EventLevelResult::kSuccess,
            MaybeCreateAndStoreEventLevelReport(
                TriggerBuilder().SetTriggerData(2).Build()));

  EXPECT_THAT(storage()->GetAttributionReports(
                  /*max_report_time=*/base::Time::Max(), /*limit=*/-1),
              ElementsAre(EventLevelDataIs(TriggerDataIs(3)),
                          EventLevelDataIs(TriggerDataIs(1)),
                          EventLevelDataIs(TriggerDataIs(2))));

  delegate()->set_reverse_reports_on_shuffle(true);

  EXPECT_THAT(storage()->GetAttributionReports(
                  /*max_report_time=*/base::Time::Max(), /*limit=*/-1),
              ElementsAre(EventLevelDataIs(TriggerDataIs(2)),
                          EventLevelDataIs(TriggerDataIs(1)),
                          EventLevelDataIs(TriggerDataIs(3))));
}

TEST_F(AttributionStorageTest, GetAttributionReportsExceedLimit_Shuffles) {
  storage()->StoreSource(TestAggregatableSourceProvider().GetBuilder().Build());
  EXPECT_EQ(AttributionTrigger::EventLevelResult::kSuccess,
            MaybeCreateAndStoreEventLevelReport(
                TriggerBuilder().SetTriggerData(3).Build()));

  delegate()->set_report_delay(base::Hours(1));
  EXPECT_EQ(AttributionTrigger::EventLevelResult::kSuccess,
            MaybeCreateAndStoreEventLevelReport(
                TriggerBuilder().SetTriggerData(1).Build()));

  delegate()->set_report_delay(base::Hours(2));
  EXPECT_EQ(AttributionTrigger::EventLevelResult::kSuccess,
            MaybeCreateAndStoreEventLevelReport(
                TriggerBuilder().SetTriggerData(2).Build()));

  // Will be dropped as the report time is latest.
  delegate()->set_report_delay(base::Hours(3));
  EXPECT_EQ(AttributionTrigger::AggregatableResult::kSuccess,
            MaybeCreateAndStoreAggregatableReport(
                DefaultAggregatableTriggerBuilder().Build()));

  EXPECT_THAT(storage()->GetAttributionReports(
                  /*max_report_time=*/base::Time::Max(), /*limit=*/3),
              ElementsAre(EventLevelDataIs(TriggerDataIs(3)),
                          EventLevelDataIs(TriggerDataIs(1)),
                          EventLevelDataIs(TriggerDataIs(2))));

  delegate()->set_reverse_reports_on_shuffle(true);

  EXPECT_THAT(storage()->GetAttributionReports(
                  /*max_report_time=*/base::Time::Max(), /*limit=*/3),
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
  EXPECT_EQ(AttributionTrigger::EventLevelResult::kSuccess,
            MaybeCreateAndStoreEventLevelReport(
                TriggerBuilder().SetDebugKey(33).Build()));

  task_environment_.FastForwardBy(kReportDelay);
  EXPECT_THAT(storage()->GetAttributionReports(base::Time::Now()),
              ElementsAre(AllOf(ReportSourceIs(SourceDebugKeyIs(22)),
                                TriggerDebugKeyIs(33))));
}

TEST_F(AttributionStorageTest, AttributionAggregationKeys_RoundTrips) {
  auto aggregation_keys = AttributionAggregationKeys::FromKeys({{"key", 345}});
  ASSERT_TRUE(aggregation_keys.has_value());
  storage()->StoreSource(
      SourceBuilder().SetAggregationKeys(*aggregation_keys).Build());
  EXPECT_THAT(storage()->GetActiveSources(),
              ElementsAre(AggregationKeysAre(*aggregation_keys)));
}

TEST_F(AttributionStorageTest, MaybeCreateAndStoreReport_ReturnsNewReport) {
  storage()->StoreSource(SourceBuilder(base::Time::Now()).Build());
  EXPECT_THAT(storage()->MaybeCreateAndStoreReport(
                  TriggerBuilder().SetTriggerData(123).Build()),
              AllOf(CreateReportEventLevelStatusIs(
                        AttributionTrigger::EventLevelResult::kSuccess),
                    NewEventLevelReportIs(
                        Optional(EventLevelDataIs(TriggerDataIs(123)))),
                    NewAggregatableReportIs(absl::nullopt)));
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

  SourceBuilder source_builder = TestAggregatableSourceProvider().GetBuilder();
  TriggerBuilder trigger_builder = DefaultAggregatableTriggerBuilder();

  storage()->StoreSource(source_builder.SetReportingOrigin(origin1).Build());
  storage()->StoreSource(source_builder.SetReportingOrigin(origin2).Build());
  storage()->StoreSource(source_builder.SetReportingOrigin(origin3).Build());
  ASSERT_THAT(storage()->GetActiveSources(), SizeIs(3));

  ASSERT_THAT(
      storage()->MaybeCreateAndStoreReport(
          trigger_builder.SetReportingOrigin(origin1).SetDebugKey(1).Build()),
      AllOf(CreateReportEventLevelStatusIs(
                AttributionTrigger::EventLevelResult::kSuccess),
            CreateReportAggregatableStatusIs(
                AttributionTrigger::AggregatableResult::kSuccess)));

  ASSERT_THAT(
      storage()->MaybeCreateAndStoreReport(
          trigger_builder.SetReportingOrigin(origin2).SetDebugKey(2).Build()),
      AllOf(CreateReportEventLevelStatusIs(
                AttributionTrigger::EventLevelResult::kSuccess),
            CreateReportAggregatableStatusIs(
                AttributionTrigger::AggregatableResult::kSuccess)));

  ASSERT_THAT(
      storage()->MaybeCreateAndStoreReport(
          trigger_builder.SetReportingOrigin(origin3).SetDebugKey(3).Build()),
      AllOf(
          CreateReportEventLevelStatusIs(
              AttributionTrigger::EventLevelResult::kExcessiveReportingOrigins),
          CreateReportAggregatableStatusIs(
              AttributionTrigger::AggregatableResult::
                  kExcessiveReportingOrigins)));

  // Two event-level reports, two aggregatable reports.
  EXPECT_THAT(storage()->GetAttributionReports(base::Time::Max()),
              ElementsAre(TriggerDebugKeyIs(1), TriggerDebugKeyIs(2),
                          TriggerDebugKeyIs(1), TriggerDebugKeyIs(2)));
}

TEST_F(AttributionStorageTest, MaxAggregatableBudgetPerSource) {
  delegate()->set_aggregatable_budget_per_source(16);

  auto provider = TestAggregatableSourceProvider(/*size=*/2);
  storage()->StoreSource(provider.GetBuilder().Build());

  ReportBuilder builder(
      AttributionInfoBuilder(
          SourceBuilder().SetSourceId(StoredSource::Id(1)).BuildStored())
          .Build());

  // A single contribution exceeds the budget.
  EXPECT_EQ(
      MaybeCreateAndStoreAggregatableReport(DefaultAggregatableTriggerBuilder(
                                                /*histogram_values=*/{17})
                                                .Build()),
      AttributionTrigger::AggregatableResult::kInsufficientBudget);

  EXPECT_EQ(
      MaybeCreateAndStoreAggregatableReport(DefaultAggregatableTriggerBuilder(
                                                /*histogram_values=*/{2, 5})
                                                .Build()),
      AttributionTrigger::AggregatableResult::kSuccess);

  EXPECT_EQ(
      MaybeCreateAndStoreAggregatableReport(DefaultAggregatableTriggerBuilder(
                                                /*histogram_values=*/{10})
                                                .Build()),
      AttributionTrigger::AggregatableResult::kInsufficientBudget);

  EXPECT_EQ(
      MaybeCreateAndStoreAggregatableReport(DefaultAggregatableTriggerBuilder(
                                                /*histogram_values=*/{9})
                                                .Build()),
      AttributionTrigger::AggregatableResult::kSuccess);

  EXPECT_EQ(
      MaybeCreateAndStoreAggregatableReport(DefaultAggregatableTriggerBuilder(
                                                /*histogram_values=*/{1})
                                                .Build()),
      AttributionTrigger::AggregatableResult::kInsufficientBudget);

  // The first source will be deactivated and the second source should have
  // capacity.
  storage()->StoreSource(provider.GetBuilder().Build());

  EXPECT_EQ(
      MaybeCreateAndStoreAggregatableReport(DefaultAggregatableTriggerBuilder(
                                                /*histogram_values=*/{9})
                                                .Build()),
      AttributionTrigger::AggregatableResult::kSuccess);
}

TEST_F(AttributionStorageTest,
       GetAttributionReports_SetsRandomizedTriggerRate) {
  delegate()->set_randomized_response_rates({
      .navigation = .2,
      .event = .4,
  });

  const auto origin1 = url::Origin::Create(GURL("https://r1.test"));
  const auto origin2 = url::Origin::Create(GURL("https://r2.test"));

  storage()->StoreSource(SourceBuilder()
                             .SetReportingOrigin(origin1)
                             .SetSourceType(AttributionSourceType::kNavigation)
                             .Build());
  MaybeCreateAndStoreEventLevelReport(
      TriggerBuilder().SetReportingOrigin(origin1).Build());

  storage()->StoreSource(SourceBuilder()
                             .SetReportingOrigin(origin2)
                             .SetSourceType(AttributionSourceType::kEvent)
                             .Build());
  MaybeCreateAndStoreEventLevelReport(
      TriggerBuilder().SetReportingOrigin(origin2).Build());

  EXPECT_THAT(
      storage()->GetAttributionReports(base::Time::Max()),
      UnorderedElementsAre(
          AllOf(
              ReportSourceIs(SourceTypeIs(AttributionSourceType::kNavigation)),
              EventLevelDataIs(RandomizedTriggerRateIs(.2))),
          AllOf(ReportSourceIs(SourceTypeIs(AttributionSourceType::kEvent)),
                EventLevelDataIs(RandomizedTriggerRateIs(.4)))));
}

// Will return minimum of next event-level report and next aggregatable report
// time if both present.
TEST_F(AttributionStorageTest, GetNextReportTime) {
  delegate()->set_max_attributions_per_source(1);

  EXPECT_EQ(storage()->GetNextReportTime(base::Time::Min()), absl::nullopt);

  storage()->StoreSource(TestAggregatableSourceProvider().GetBuilder().Build());

  EXPECT_EQ(AttributionTrigger::EventLevelResult::kSuccess,
            MaybeCreateAndStoreEventLevelReport(DefaultTrigger()));

  const base::Time report_time_a = base::Time::Now() + kReportDelay;

  EXPECT_EQ(storage()->GetNextReportTime(base::Time::Min()), report_time_a);
  EXPECT_EQ(storage()->GetNextReportTime(report_time_a), absl::nullopt);

  task_environment_.FastForwardBy(base::Milliseconds(1));

  EXPECT_EQ(AttributionTrigger::AggregatableResult::kSuccess,
            MaybeCreateAndStoreAggregatableReport(
                DefaultAggregatableTriggerBuilder().Build()));

  const base::Time report_time_b = base::Time::Now() + kReportDelay;

  EXPECT_EQ(storage()->GetNextReportTime(base::Time::Min()), report_time_a);
  EXPECT_EQ(storage()->GetNextReportTime(report_time_a), report_time_b);
  EXPECT_EQ(storage()->GetNextReportTime(report_time_b), absl::nullopt);

  task_environment_.FastForwardBy(base::Milliseconds(1));

  storage()->StoreSource(SourceBuilder().Build());

  EXPECT_EQ(AttributionTrigger::EventLevelResult::kSuccess,
            MaybeCreateAndStoreEventLevelReport(DefaultTrigger()));

  base::Time report_time_c = base::Time::Now() + kReportDelay;

  EXPECT_EQ(storage()->GetNextReportTime(base::Time::Min()), report_time_a);
  EXPECT_EQ(storage()->GetNextReportTime(report_time_a), report_time_b);
  EXPECT_EQ(storage()->GetNextReportTime(report_time_b), report_time_c);
  EXPECT_EQ(storage()->GetNextReportTime(report_time_c), absl::nullopt);
}

TEST_F(AttributionStorageTest, SourceEventIdSanitized) {
  delegate()->set_source_event_id_cardinality(4);

  storage()->StoreSource(SourceBuilder().SetSourceEventId(5).Build());
  EXPECT_THAT(storage()->GetActiveSources(), ElementsAre(SourceEventIdIs(1)));
}

TEST_F(AttributionStorageTest, TriggerDataSanitized) {
  delegate()->set_trigger_data_cardinality(/*navigation=*/4, /*event=*/3);

  const auto origin1 = url::Origin::Create(GURL("https://r1.test"));
  const auto origin2 = url::Origin::Create(GURL("https://r2.test"));

  storage()->StoreSource(SourceBuilder()
                             .SetReportingOrigin(origin1)
                             .SetSourceType(AttributionSourceType::kNavigation)
                             .Build());
  MaybeCreateAndStoreEventLevelReport(
      TriggerBuilder().SetReportingOrigin(origin1).SetTriggerData(6).Build());

  storage()->StoreSource(SourceBuilder()
                             .SetReportingOrigin(origin2)
                             .SetSourceType(AttributionSourceType::kEvent)
                             .Build());
  MaybeCreateAndStoreEventLevelReport(TriggerBuilder()
                                          .SetReportingOrigin(origin2)
                                          .SetEventSourceTriggerData(4)
                                          .Build());

  EXPECT_THAT(
      storage()->GetAttributionReports(base::Time::Max()),
      UnorderedElementsAre(
          AllOf(
              ReportSourceIs(SourceTypeIs(AttributionSourceType::kNavigation)),
              EventLevelDataIs(TriggerDataIs(2))),
          AllOf(ReportSourceIs(SourceTypeIs(AttributionSourceType::kEvent)),
                EventLevelDataIs(TriggerDataIs(1)))));
}

TEST_F(AttributionStorageTest, SourceFilterData_RoundTrips) {
  storage()->StoreSource(SourceBuilder()
                             .SetFilterData(AttributionFilterData())
                             .SetSourceType(AttributionSourceType::kNavigation)
                             .Build());

  auto filter_data =
      AttributionFilterData::FromSourceFilterValues({{"abc", {"x", "y"}}});
  ASSERT_TRUE(filter_data.has_value());

  storage()->StoreSource(SourceBuilder()
                             .SetFilterData(*filter_data)
                             .SetSourceType(AttributionSourceType::kEvent)
                             .Build());

  EXPECT_THAT(
      storage()->GetActiveSources(),
      ElementsAre(SourceFilterDataIs(AttributionFilterData::CreateForTesting(
                      {{"source_type", {"navigation"}}})),
                  SourceFilterDataIs(AttributionFilterData::CreateForTesting({
                      {"abc", {"x", "y"}},
                      {"source_type", {"event"}},
                  }))));
}

TEST_F(AttributionStorageTest, NoMatchingTriggerData_ReturnsError) {
  const auto origin = url::Origin::Create(GURL("https://r.test"));

  storage()->StoreSource(SourceBuilder()
                             .SetSourceType(AttributionSourceType::kNavigation)
                             .SetConversionOrigin(origin)
                             .SetReportingOrigin(origin)
                             .Build());

  EXPECT_EQ(AttributionTrigger::EventLevelResult::kNoMatchingConfigurations,
            MaybeCreateAndStoreEventLevelReport(AttributionTrigger(
                origin, origin,
                /*filters=*/AttributionFilterData(),
                /*debug_key=*/absl::nullopt,
                {AttributionTrigger::EventTriggerData(
                    /*data=*/11,
                    /*priority=*/12,
                    /*dedup_key=*/13,
                    /*filters=*/
                    AttributionFilterData::ForSourceType(
                        AttributionSourceType::kEvent),
                    /*not_filters=*/AttributionFilterData())},
                /*aggregatable_trigger_data=*/{},
                /*aggregatable_values=*/AttributionAggregatableValues())));

  EXPECT_THAT(storage()->GetAttributionReports(base::Time::Max()), IsEmpty());

  EXPECT_THAT(storage()->GetActiveSources(),
              ElementsAre(DedupKeysAre(IsEmpty())));
}

TEST_F(AttributionStorageTest, MatchingTriggerData_UsesCorrectData) {
  const auto origin = url::Origin::Create(GURL("https://r.test"));

  storage()->StoreSource(
      SourceBuilder()
          .SetSourceType(AttributionSourceType::kNavigation)
          .SetConversionOrigin(origin)
          .SetReportingOrigin(origin)
          .SetFilterData(*AttributionFilterData::FromSourceFilterValues(
              {{"abc", {"123"}}}))
          .Build());

  const std::vector<AttributionTrigger::EventTriggerData> event_triggers = {
      // Filters don't match.
      AttributionTrigger::EventTriggerData(
          /*data=*/11,
          /*priority=*/12,
          /*dedup_key=*/13,
          /*filters=*/
          *AttributionFilterData::FromTriggerFilterValues({
              {"abc", {"456"}},
          }),
          /*not_filters=*/AttributionFilterData()),

      // Filters match, but negated filters do not.
      AttributionTrigger::EventTriggerData(
          /*data=*/21,
          /*priority=*/22,
          /*dedup_key=*/23,
          /*filters=*/
          *AttributionFilterData::FromTriggerFilterValues({
              {"abc", {"123"}},
          }),
          /*not_filters=*/
          *AttributionFilterData::FromTriggerFilterValues({
              {"source_type", {"navigation"}},
          })),

      // Filters and negated filters match.
      AttributionTrigger::EventTriggerData(
          /*data=*/31,
          /*priority=*/32,
          /*dedup_key=*/33,
          /*filters=*/
          *AttributionFilterData::FromTriggerFilterValues({
              {"abc", {"123"}},
          }),
          /*not_filters=*/
          *AttributionFilterData::FromTriggerFilterValues({
              {"source_type", {"event"}},
          })),

      // Filters and negated filters match, but not the first event
      // trigger to match.
      AttributionTrigger::EventTriggerData(
          /*data=*/41,
          /*priority=*/42,
          /*dedup_key=*/43,
          /*filters=*/
          *AttributionFilterData::FromTriggerFilterValues({
              {"abc", {"123"}},
          }),
          /*not_filters=*/
          *AttributionFilterData::FromTriggerFilterValues({
              {"source_type", {"event"}},
          })),
  };

  EXPECT_EQ(AttributionTrigger::EventLevelResult::kSuccess,
            MaybeCreateAndStoreEventLevelReport(AttributionTrigger(
                origin, origin,
                /*filters=*/AttributionFilterData(),
                /*debug_key=*/absl::nullopt, event_triggers,
                /*aggregatable_trigger_data=*/{},
                /*aggregatable_values=*/AttributionAggregatableValues())));

  EXPECT_THAT(storage()->GetAttributionReports(base::Time::Max()),
              ElementsAre(EventLevelDataIs(
                  AllOf(TriggerDataIs(31), TriggerPriorityIs(32)))));

  EXPECT_THAT(storage()->GetActiveSources(),
              ElementsAre(DedupKeysAre(ElementsAre(33))));
}

TEST_F(AttributionStorageTest, TopLevelTriggerFiltering) {
  const auto origin = url::Origin::Create(GURL("https://r.test"));

  std::vector<AttributionAggregatableTriggerData> aggregatable_trigger_data{
      AttributionAggregatableTriggerData::CreateForTesting(
          absl::MakeUint128(/*high=*/1, /*low=*/0),
          /*source_keys=*/{"0"},
          /*filters=*/AttributionFilterData(),
          /*not_filters=*/AttributionFilterData())};

  auto aggregatable_values =
      AttributionAggregatableValues::CreateForTesting({{"0", 1}});

  storage()->StoreSource(
      SourceBuilder()
          .SetConversionOrigin(origin)
          .SetReportingOrigin(origin)
          .SetFilterData(*AttributionFilterData::FromSourceFilterValues(
              {{"abc", {"123"}}}))
          .SetAggregationKeys(*AttributionAggregationKeys::FromKeys({{"0", 1}}))
          .Build());

  AttributionTrigger trigger1(origin, origin,
                              /*filters=*/
                              *AttributionFilterData::FromTriggerFilterValues({
                                  {"abc", {"456"}},
                              }),
                              /*debug_key=*/absl::nullopt,
                              /*event_triggers=*/{}, aggregatable_trigger_data,
                              aggregatable_values);

  AttributionTrigger trigger2(origin, origin,
                              /*filters=*/
                              *AttributionFilterData::FromTriggerFilterValues({
                                  {"abc", {"123"}},
                              }),
                              /*debug_key=*/absl::nullopt,
                              /*event_triggers=*/{},
                              std::move(aggregatable_trigger_data),
                              std::move(aggregatable_values));

  EXPECT_THAT(storage()->MaybeCreateAndStoreReport(trigger1),
              AllOf(CreateReportEventLevelStatusIs(
                        AttributionTrigger::EventLevelResult::
                            kNoMatchingSourceFilterData),
                    CreateReportAggregatableStatusIs(
                        AttributionTrigger::AggregatableResult::
                            kNoMatchingSourceFilterData)));
  EXPECT_THAT(
      storage()->MaybeCreateAndStoreReport(trigger2),
      AllOf(
          CreateReportEventLevelStatusIs(
              AttributionTrigger::EventLevelResult::kNoMatchingConfigurations),
          CreateReportAggregatableStatusIs(
              AttributionTrigger::AggregatableResult::kSuccess)));
}

TEST_F(AttributionStorageTest,
       AggregatableAttributionNoMatchingSources_NoSourcesReturned) {
  EXPECT_THAT(
      storage()->MaybeCreateAndStoreReport(
          DefaultAggregatableTriggerBuilder().Build()),
      AllOf(CreateReportAggregatableStatusIs(
                AttributionTrigger::AggregatableResult::kNoMatchingImpressions),
            NewEventLevelReportIs(absl::nullopt),
            NewAggregatableReportIs(absl::nullopt)));
  EXPECT_THAT(storage()->GetAttributionReports(base::Time::Now()), IsEmpty());
}

TEST_F(AttributionStorageTest, AggregatableAttribution_ReportsScheduled) {
  auto source_builder = TestAggregatableSourceProvider().GetBuilder();
  storage()->StoreSource(source_builder.Build());

  AttributionTrigger trigger =
      DefaultAggregatableTriggerBuilder(/*histogram_values=*/{5})
          .SetTriggerData(5)
          .Build();
  auto contributions =
      DefaultAggregatableHistogramContributions(/*histogram_values=*/{5});
  ASSERT_THAT(contributions, SizeIs(1));

  EXPECT_THAT(
      storage()->MaybeCreateAndStoreReport(trigger),
      AllOf(CreateReportEventLevelStatusIs(
                AttributionTrigger::EventLevelResult::kSuccess),
            CreateReportAggregatableStatusIs(
                AttributionTrigger::AggregatableResult::kSuccess),
            NewEventLevelReportIs(Optional(EventLevelDataIs(TriggerDataIs(5)))),
            NewAggregatableReportIs(Optional(AggregatableAttributionDataIs(
                AggregatableHistogramContributionsAre(contributions))))));

  const auto source = source_builder.SetDefaultFilterData().BuildStored();
  auto expected_event_level_report =
      GetExpectedEventLevelReport(source, trigger);
  auto expected_aggregatable_report =
      GetExpectedAggregatableReport(source, std::move(contributions));

  task_environment_.FastForwardBy(kReportDelay);

  EXPECT_THAT(
      storage()->GetAttributionReports(base::Time::Now()),
      ElementsAre(expected_event_level_report, expected_aggregatable_report));

  EXPECT_EQ(expected_aggregatable_report.report_time(),
            absl::get<AttributionReport::AggregatableAttributionData>(
                expected_aggregatable_report.data())
                .initial_report_time);
}

TEST_F(
    AttributionStorageTest,
    MaybeCreateAndStoreAggregatableReport_reachedEventLevelAttributionLimit) {
  SourceBuilder builder = TestAggregatableSourceProvider().GetBuilder();
  builder.SetSourceEventId(7);
  EXPECT_THAT(storage()->StoreSource(builder.Build()).deactivated_sources,
              IsEmpty());
  EXPECT_THAT(storage()->GetActiveSources(), SizeIs(1));

  // Store the maximum number of reports for the source.
  for (size_t i = 1; i <= kMaxConversions; i++) {
    EXPECT_THAT(storage()->MaybeCreateAndStoreReport(
                    DefaultAggregatableTriggerBuilder().Build()),
                AllOf(CreateReportEventLevelStatusIs(
                          AttributionTrigger::EventLevelResult::kSuccess),
                      CreateReportAggregatableStatusIs(
                          AttributionTrigger::AggregatableResult::kSuccess)));
  }

  task_environment_.FastForwardBy(kReportDelay);
  auto reports = storage()->GetAttributionReports(base::Time::Now());
  // 3 event-level reports, 3 aggregatable reports
  EXPECT_THAT(reports, SizeIs(6));

  // Simulate the reports being sent and removed from storage.
  DeleteReports(reports);

  // The next trigger should cause the source to reach event-level attribution
  // limit; the event-level report itself shouldn't be stored as we've already
  // reached the maximum number of event-level reports per source, whereas the
  // aggregatable report is still stored.
  EXPECT_THAT(
      storage()->MaybeCreateAndStoreReport(
          DefaultAggregatableTriggerBuilder(/*histogram_values=*/{5})
              .SetTriggerData(5)
              .Build()),
      AllOf(CreateReportEventLevelStatusIs(
                AttributionTrigger::EventLevelResult::kPriorityTooLow),
            CreateReportAggregatableStatusIs(
                AttributionTrigger::AggregatableResult::kSuccess),
            ReplacedEventLevelReportIs(absl::nullopt),
            NewEventLevelReportIs(absl::nullopt),
            NewAggregatableReportIs(Optional(AggregatableAttributionDataIs(
                AggregatableHistogramContributionsAre(
                    DefaultAggregatableHistogramContributions(
                        /*histogram_values=*/{5})))))));
  EXPECT_THAT(
      storage()->GetActiveSources(),
      ElementsAre(SourceActiveStateIs(
          StoredSource::ActiveState::kReachedEventLevelAttributionLimit)));
}

TEST_F(AttributionStorageTest, AggregatableReportFiltering) {
  storage()->StoreSource(
      SourceBuilder()
          .SetFilterData(*AttributionFilterData::FromSourceFilterValues(
              {{"abc", {"123"}}}))
          .SetAggregationKeys(*AttributionAggregationKeys::FromKeys({{"0", 1}}))
          .Build());

  EXPECT_EQ(MaybeCreateAndStoreAggregatableReport(
                TriggerBuilder()
                    .SetAggregatableTriggerData(
                        {AttributionAggregatableTriggerData::CreateForTesting(
                            absl::MakeUint128(/*high=*/1, /*low=*/0),
                            /*source_keys=*/{"0"},
                            /*filters=*/
                            AttributionFilterData(),
                            /*not_filters=*/AttributionFilterData())})
                    .Build()),
            AttributionTrigger::AggregatableResult::kNoHistograms);
}

}  // namespace content
