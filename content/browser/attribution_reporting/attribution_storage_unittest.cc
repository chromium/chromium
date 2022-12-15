// Copyright 2020 The Chromium Authors
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
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/aggregation_service/aggregation_service.mojom.h"
#include "components/attribution_reporting/aggregatable_trigger_data.h"
#include "components/attribution_reporting/aggregatable_values.h"
#include "components/attribution_reporting/aggregation_keys.h"
#include "components/attribution_reporting/event_trigger_data.h"
#include "components/attribution_reporting/filters.h"
#include "components/attribution_reporting/suitable_origin.h"
#include "components/attribution_reporting/trigger_registration.h"
#include "content/browser/attribution_reporting/aggregatable_histogram_contribution.h"
#include "content/browser/attribution_reporting/attribution_observer_types.h"
#include "content/browser/attribution_reporting/attribution_report.h"
#include "content/browser/attribution_reporting/attribution_source_type.h"
#include "content/browser/attribution_reporting/attribution_storage_sql.h"
#include "content/browser/attribution_reporting/attribution_test_utils.h"
#include "content/browser/attribution_reporting/attribution_trigger.h"
#include "content/browser/attribution_reporting/attribution_utils.h"
#include "content/browser/attribution_reporting/common_source_info.h"
#include "content/browser/attribution_reporting/rate_limit_result.h"
#include "content/browser/attribution_reporting/storable_source.h"
#include "content/browser/attribution_reporting/stored_source.h"
#include "content/public/browser/storage_partition.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/numeric/int128.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
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

using AttributionFilterData = ::attribution_reporting::FilterData;
using AttributionFilters = ::attribution_reporting::Filters;

using ::attribution_reporting::SuitableOrigin;

// Default max number of conversions for a single impression for testing.
const int kMaxConversions = 3;

// Default delay for when a report should be sent for testing.
constexpr base::TimeDelta kReportDelay = base::Milliseconds(5);

StoragePartition::StorageKeyMatcherFunction GetMatcher(
    const url::Origin& to_delete) {
  return base::BindRepeating(std::equal_to<blink::StorageKey>(),
                             blink::StorageKey(to_delete));
}

MATCHER_P(CreateReportSourceIs, matcher, "") {
  return ExplainMatchResult(matcher, arg.source(), result_listener);
}

MATCHER_P(CreateReportMaxAttributionsLimitIs, matcher, "") {
  return ExplainMatchResult(matcher, arg.limits().rate_limits_max_attributions,
                            result_listener);
}

MATCHER_P(CreateReportAggreggatableBudgetPerSourceIs, matcher, "") {
  return ExplainMatchResult(
      matcher, arg.limits().aggregatable_budget_per_source, result_listener);
}

MATCHER_P(CreateReportMaxAttributionReportingOriginsLimitIs, matcher, "") {
  return ExplainMatchResult(
      matcher, arg.limits().rate_limits_max_attribution_reporting_origins,
      result_listener);
}

MATCHER_P(CreateReportMaxEventLevelReportsLimitIs, matcher, "") {
  return ExplainMatchResult(
      matcher, arg.limits().max_event_level_reports_per_destination,
      result_listener);
}

MATCHER_P(CreateReportMaxAggregatableReportsLimitIs, matcher, "") {
  return ExplainMatchResult(
      matcher, arg.limits().max_aggregatable_reports_per_destination,
      result_listener);
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
        conversion.registration().event_triggers.vec(),
        [&](const attribution_reporting::EventTriggerData& event_trigger) {
          return AttributionFiltersMatch(source.common_info().filter_data(),
                                         source.common_info().source_type(),
                                         event_trigger.filters,
                                         event_trigger.not_filters);
        });
    CHECK(event_trigger !=
          conversion.registration().event_triggers.vec().end());

    return ReportBuilder(AttributionInfoBuilder(source)
                             .SetTime(base::Time::Now())
                             .Build())
        .SetTriggerData(event_trigger->data)
        .SetReportTime(source.common_info().source_time() + kReportDelay)
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
  EXPECT_THAT(
      storage()->GetActiveSources(),
      ElementsAre(CommonSourceInfoIs(SourceBuilder().BuildCommonInfo())));
}

TEST_F(AttributionStorageTest, UniqueReportWindowsStored_ValuesIdentical) {
  storage()->StoreSource(SourceBuilder()
                             .SetExpiry(base::Days(30))
                             .SetEventReportWindow(base::Days(15))
                             .SetAggregatableReportWindow(base::Days(5))
                             .Build());
  EXPECT_THAT(storage()->GetActiveSources(),
              ElementsAre(CommonSourceInfoIs(
                  SourceBuilder()
                      .SetExpiry(base::Days(30))
                      .SetEventReportWindow(base::Days(15))
                      .SetAggregatableReportWindow(base::Days(5))
                      .BuildCommonInfo())));
}

TEST_F(AttributionStorageTest,
       GetWithNoMatchingImpressions_NoImpressionsReturned) {
  EXPECT_THAT(
      storage()->MaybeCreateAndStoreReport(DefaultTrigger()),
      AllOf(CreateReportEventLevelStatusIs(
                AttributionTrigger::EventLevelResult::kNoMatchingImpressions),
            NewEventLevelReportIs(absl::nullopt),
            NewAggregatableReportIs(absl::nullopt),
            CreateReportSourceIs(absl::nullopt)));
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
  auto impression = SourceBuilder()
                        .SetDestinationOrigin(
                            *SuitableOrigin::Deserialize("https://sub.a.test"))
                        .Build();
  storage()->StoreSource(impression);
  EXPECT_EQ(
      AttributionTrigger::EventLevelResult::kSuccess,
      MaybeCreateAndStoreEventLevelReport(
          TriggerBuilder()
              .SetDestinationOrigin(
                  *SuitableOrigin::Deserialize("https://a.test"))
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

TEST_F(AttributionStorageTest, ImpressionReportWindowPassed_NoReportGenerated) {
  storage()->StoreSource(
      SourceBuilder().SetEventReportWindow(base::Milliseconds(2)).Build());
  task_environment_.FastForwardBy(base::Milliseconds(3));

  EXPECT_EQ(AttributionTrigger::EventLevelResult::kReportWindowPassed,
            MaybeCreateAndStoreEventLevelReport(DefaultTrigger()));
}

TEST_F(AttributionStorageTest,
       AggregatableReportWindowPassed_NoReportGenerated) {
  SourceBuilder source_builder = TestAggregatableSourceProvider().GetBuilder();

  storage()->StoreSource(
      source_builder.SetAggregatableReportWindow(base::Milliseconds(2))
          .Build());

  task_environment_.FastForwardBy(base::Milliseconds(3));
  EXPECT_THAT(
      storage()->MaybeCreateAndStoreReport(
          DefaultAggregatableTriggerBuilder().Build()),
      AllOf(CreateReportEventLevelStatusIs(
                AttributionTrigger::EventLevelResult::kSuccess),
            CreateReportAggregatableStatusIs(
                AttributionTrigger::AggregatableResult::kReportWindowPassed)));
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
  EXPECT_THAT(storage()->MaybeCreateAndStoreReport(
                  TriggerBuilder().SetTriggerData(20).Build()),
              AllOf(CreateReportEventLevelStatusIs(
                        AttributionTrigger::EventLevelResult::kPriorityTooLow),
                    ReplacedEventLevelReportIs(absl::nullopt),
                    DroppedEventLevelReportIs(
                        Optional(EventLevelDataIs(TriggerDataIs(20u))))));
}

TEST_F(AttributionStorageTest, OneConversion_OneReportScheduled) {
  auto conversion = DefaultTrigger();

  storage()->StoreSource(SourceBuilder().Build());
  EXPECT_EQ(AttributionTrigger::EventLevelResult::kSuccess,
            MaybeCreateAndStoreEventLevelReport(conversion));

  AttributionReport expected_report =
      GetExpectedEventLevelReport(SourceBuilder().BuildStored(), conversion);

  task_environment_.FastForwardBy(kReportDelay);

  EXPECT_THAT(storage()->GetAttributionReports(base::Time::Now()),
              ElementsAre(expected_report));
}

TEST_F(AttributionStorageTest,
       ConversionWithDifferentReportingOrigin_NoReportScheduled) {
  auto impression = SourceBuilder()
                        .SetReportingOrigin(*SuitableOrigin::Deserialize(
                            "https://different.test"))
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
                        .SetDestinationOrigin(*SuitableOrigin::Deserialize(
                            "https://different.test"))
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
          .SetSourceOrigin(*SuitableOrigin::Deserialize("https://other.test/"))
          .Build();
  storage()->StoreSource(new_impression);

  // The first impression should be active because even though
  // <reporting_origin, destination_origin> matches, it has not converted yet.
  EXPECT_THAT(storage()->GetActiveSources(), SizeIs(2));
  EXPECT_EQ(AttributionTrigger::EventLevelResult::kSuccess,
            MaybeCreateAndStoreEventLevelReport(DefaultTrigger()));
  EXPECT_THAT(storage()->GetActiveSources(), SizeIs(1));
}

// This test makes sure that when a new click is received for a given
// <reporting_origin, destination_origin> pair, all existing impressions for
// that origin that have converted are marked ineligible for new conversions per
// the multi-touch model.
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
  AttributionReport expected_report =
      GetExpectedEventLevelReport(builder.BuildStored(), conversion);

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
                             .SetReportingOrigin(*SuitableOrigin::Deserialize(
                                 "https://different.test"))
                             .Build());

  // The first impression should still be active and able to convert.
  EXPECT_EQ(AttributionTrigger::EventLevelResult::kSuccess,
            MaybeCreateAndStoreEventLevelReport(conversion));

  AttributionReport expected_report =
      GetExpectedEventLevelReport(builder.BuildStored(), conversion);

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

  AttributionReport third_expected_conversion =
      GetExpectedEventLevelReport(builder.BuildStored(), conversion);
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
  delegate()->set_max_attributions_per_source(1);

  ASSERT_EQ(storage()
                ->StoreSource(
                    SourceBuilder().SetSourceEventId(3).SetPriority(1).Build())
                .status,
            StorableSource::Result::kSuccess);

  ASSERT_EQ(storage()
                ->StoreSource(
                    SourceBuilder().SetSourceEventId(5).SetPriority(2).Build())
                .status,
            StorableSource::Result::kSuccess);

  // Force the lower-priority source to be deactivated.
  ASSERT_EQ(AttributionTrigger::EventLevelResult::kSuccess,
            MaybeCreateAndStoreEventLevelReport(DefaultTrigger()));

  ASSERT_THAT(storage()->GetActiveSources(), ElementsAre(SourceEventIdIs(5u)));

  // There's still room for this source, as the limit applies only to active
  // sources.
  ASSERT_EQ(storage()
                ->StoreSource(SourceBuilder().SetSourceEventId(6).Build())
                .status,
            StorableSource::Result::kSuccess);

  ASSERT_EQ(storage()
                ->StoreSource(SourceBuilder().SetSourceEventId(7).Build())
                .status,
            StorableSource::Result::kInsufficientSourceCapacity);

  ASSERT_THAT(storage()->GetActiveSources(),
              ElementsAre(SourceEventIdIs(5u), SourceEventIdIs(6u)));
}

TEST_F(AttributionStorageTest, MaxImpressionsPerOrigin_PerOriginNotSite) {
  delegate()->set_max_sources_per_origin(2);
  storage()->StoreSource(SourceBuilder()
                             .SetSourceOrigin(*SuitableOrigin::Deserialize(
                                 "https://foo.a.example"))
                             .SetSourceEventId(3)
                             .Build());
  storage()->StoreSource(SourceBuilder()
                             .SetSourceOrigin(*SuitableOrigin::Deserialize(
                                 "https://foo.a.example"))
                             .SetSourceEventId(5)
                             .Build());
  storage()->StoreSource(SourceBuilder()
                             .SetSourceOrigin(*SuitableOrigin::Deserialize(
                                 "https://bar.a.example"))
                             .SetSourceEventId(7)
                             .Build());

  EXPECT_THAT(storage()->GetActiveSources(),
              ElementsAre(SourceEventIdIs(3u), SourceEventIdIs(5u),
                          SourceEventIdIs(7u)));

  // This impression shouldn't be stored, because its origin has already hit the
  // limit of 2.
  EXPECT_EQ(storage()
                ->StoreSource(SourceBuilder()
                                  .SetSourceOrigin(*SuitableOrigin::Deserialize(
                                      "https://foo.a.example"))
                                  .SetSourceEventId(9)
                                  .Build())
                .status,
            StorableSource::Result::kInsufficientSourceCapacity);

  // This impression should be stored, because its origin hasn't hit the limit
  // of 2.
  storage()->StoreSource(SourceBuilder()
                             .SetSourceOrigin(*SuitableOrigin::Deserialize(
                                 "https://bar.a.example"))
                             .SetSourceEventId(11)
                             .Build());

  EXPECT_THAT(storage()->GetActiveSources(),
              ElementsAre(SourceEventIdIs(3u), SourceEventIdIs(5u),
                          SourceEventIdIs(7u), SourceEventIdIs(11u)));
}

TEST_F(AttributionStorageTest, MaxEventLevelReportsPerDestination) {
  SourceBuilder source_builder = TestAggregatableSourceProvider().GetBuilder();

  delegate()->set_max_reports_per_destination(
      AttributionReport::Type::kEventLevel, 1);
  storage()->StoreSource(source_builder.Build());
  storage()->StoreSource(source_builder.Build());

  EXPECT_THAT(storage()->MaybeCreateAndStoreReport(
                  DefaultAggregatableTriggerBuilder().Build()),
              AllOf(CreateReportEventLevelStatusIs(
                        AttributionTrigger::EventLevelResult::kSuccess),
                    CreateReportAggregatableStatusIs(
                        AttributionTrigger::AggregatableResult::kSuccess),
                    CreateReportMaxEventLevelReportsLimitIs(absl::nullopt),
                    CreateReportMaxAggregatableReportsLimitIs(absl::nullopt)));

  // Verify that MaxReportsPerDestination is enforced.
  EXPECT_THAT(storage()->MaybeCreateAndStoreReport(
                  DefaultAggregatableTriggerBuilder().Build()),
              AllOf(CreateReportEventLevelStatusIs(
                        AttributionTrigger::EventLevelResult::
                            kNoCapacityForConversionDestination),
                    CreateReportAggregatableStatusIs(
                        AttributionTrigger::AggregatableResult::kSuccess),
                    ReplacedEventLevelReportIs(absl::nullopt),
                    DroppedEventLevelReportIs(absl::nullopt),
                    CreateReportMaxEventLevelReportsLimitIs(1),
                    CreateReportMaxAggregatableReportsLimitIs(absl::nullopt)));
}

TEST_F(AttributionStorageTest, MaxAggregatableReportsPerDestination) {
  SourceBuilder source_builder = TestAggregatableSourceProvider().GetBuilder();

  delegate()->set_max_reports_per_destination(
      AttributionReport::Type::kAggregatableAttribution, 1);
  storage()->StoreSource(source_builder.Build());
  storage()->StoreSource(source_builder.Build());

  EXPECT_THAT(storage()->MaybeCreateAndStoreReport(
                  DefaultAggregatableTriggerBuilder().Build()),
              AllOf(CreateReportEventLevelStatusIs(
                        AttributionTrigger::EventLevelResult::kSuccess),
                    CreateReportAggregatableStatusIs(
                        AttributionTrigger::AggregatableResult::kSuccess),
                    CreateReportMaxEventLevelReportsLimitIs(absl::nullopt),
                    CreateReportMaxAggregatableReportsLimitIs(absl::nullopt)));

  // Verify that MaxReportsPerDestination is enforced.
  EXPECT_THAT(storage()->MaybeCreateAndStoreReport(
                  DefaultAggregatableTriggerBuilder().Build()),
              AllOf(CreateReportEventLevelStatusIs(
                        AttributionTrigger::EventLevelResult::kSuccess),
                    CreateReportAggregatableStatusIs(
                        AttributionTrigger::AggregatableResult::
                            kNoCapacityForConversionDestination),
                    ReplacedEventLevelReportIs(absl::nullopt),
                    DroppedEventLevelReportIs(absl::nullopt),
                    CreateReportMaxEventLevelReportsLimitIs(absl::nullopt),
                    CreateReportMaxAggregatableReportsLimitIs(1)));
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

  storage()->ClearData(now + base::Minutes(10), now + base::Minutes(20),
                       GetMatcher(impression.common_info().source_origin()));
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
        GetMatcher(impression.common_info().destination_origin()));
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

  storage()->ClearData(now - base::Minutes(20), now + base::Minutes(20),
                       GetMatcher(impression.common_info().source_origin()));

  EXPECT_THAT(storage()->GetAttributionReports(base::Time::Max()), IsEmpty());
}

// The null filter should match all origins.
TEST_F(AttributionStorageTest, ClearDataNullFilter) {
  base::Time now = base::Time::Now();

  for (int i = 0; i < 10; i++) {
    auto origin =
        *SuitableOrigin::Deserialize(base::StringPrintf("https://%d.com/", i));
    storage()->StoreSource(SourceBuilder(now)
                               .SetExpiry(base::Days(30))
                               .SetSourceOrigin(origin)
                               .SetReportingOrigin(origin)
                               .SetDestinationOrigin(origin)
                               .Build());
    task_environment_.FastForwardBy(base::Days(1));
  }

  // Convert half of them now, half after another day.
  for (int i = 0; i < 5; i++) {
    auto origin =
        *SuitableOrigin::Deserialize(base::StringPrintf("https://%d.com/", i));
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
        *SuitableOrigin::Deserialize(base::StringPrintf("https://%d.com/", i));
    EXPECT_EQ(
        AttributionTrigger::EventLevelResult::kSuccess,
        MaybeCreateAndStoreEventLevelReport(TriggerBuilder()
                                                .SetDestinationOrigin(origin)
                                                .SetReportingOrigin(origin)
                                                .Build()));
  }

  auto null_filter = StoragePartition::StorageKeyMatcherFunction();
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
  storage()->ClearData(base::Time::Now(), base::Time::Now(),
                       GetMatcher(impression.common_info().source_origin()));
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

  const AttributionReport expected_report =
      GetExpectedEventLevelReport(builder.BuildStored(), conversion);

  EXPECT_EQ(AttributionTrigger::EventLevelResult::kSuccess,
            MaybeCreateAndStoreEventLevelReport(conversion));

  storage()->ClearData(start + base::Minutes(1), start + base::Minutes(10),
                       GetMatcher(impression.common_info().source_origin()));

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
  storage()->ClearData(start, start,
                       GetMatcher(impression1.common_info().source_origin()));
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

  auto null_filter = StoragePartition::StorageKeyMatcherFunction();
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

  auto null_filter = StoragePartition::StorageKeyMatcherFunction();
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
  EXPECT_THAT(storage()->MaybeCreateAndStoreReport(conversion1),
              AllOf(CreateReportEventLevelStatusIs(
                        AttributionTrigger::EventLevelResult::kSuccess),
                    CreateReportAggregatableStatusIs(
                        AttributionTrigger::AggregatableResult::kNotRegistered),
                    CreateReportMaxAttributionsLimitIs(absl::nullopt)));

  auto conversion2 =
      DefaultAggregatableTriggerBuilder(/*histogram_values=*/{5}).Build();
  EXPECT_THAT(storage()->MaybeCreateAndStoreReport(conversion2),
              AllOf(CreateReportEventLevelStatusIs(
                        AttributionTrigger::EventLevelResult::kSuccess),
                    CreateReportAggregatableStatusIs(
                        AttributionTrigger::AggregatableResult::kSuccess),
                    CreateReportMaxAttributionsLimitIs(absl::nullopt)));

  // Event-level reports and aggregatable reports share the attribution limit.
  EXPECT_THAT(
      storage()->MaybeCreateAndStoreReport(conversion2),
      AllOf(CreateReportEventLevelStatusIs(
                AttributionTrigger::EventLevelResult::kExcessiveAttributions),
            CreateReportAggregatableStatusIs(
                AttributionTrigger::AggregatableResult::kExcessiveAttributions),
            ReplacedEventLevelReportIs(absl::nullopt),
            CreateReportMaxAttributionsLimitIs(2),
            DroppedEventLevelReportIs(absl::nullopt)));

  const auto source =
      source_builder.SetAggregatableBudgetConsumed(5).BuildStored();
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
  AttributionStorage::StoreSourceResult result = storage()->StoreSource(
      TestAggregatableSourceProvider().GetBuilder().Build());
  EXPECT_EQ(result.status, StorableSource::Result::kSuccessNoised);
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
  builder.SetPriority(100);
  storage()->StoreSource(builder.Build());
  EXPECT_EQ(AttributionTrigger::EventLevelResult::kSuccess,
            MaybeCreateAndStoreEventLevelReport(conversion));

  storage()->StoreSource(SourceBuilder().SetSourceEventId(9).Build());
  EXPECT_EQ(AttributionTrigger::EventLevelResult::kExcessiveAttributions,
            MaybeCreateAndStoreEventLevelReport(conversion));

  const AttributionReport expected_report =
      GetExpectedEventLevelReport(builder.BuildStored(), conversion);

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

  storage()->StoreSource(builder.SetSourceEventId(7).SetPriority(100).Build());
  EXPECT_EQ(AttributionTrigger::AggregatableResult::kExcessiveAttributions,
            MaybeCreateAndStoreAggregatableReport(trigger));

  const AttributionReport expected_report = GetExpectedAggregatableReport(
      builder.SetSourceEventId(5)
          .SetAttributionLogic(StoredSource::AttributionLogic::kNever)
          .SetPriority(0)
          .SetAggregatableBudgetConsumed(1)
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

  const auto store_source = [&](const char* source_origin,
                                const char* reporting_origin,
                                const char* destination_origin) {
    return storage()
        ->StoreSource(
            SourceBuilder()
                .SetSourceOrigin(*SuitableOrigin::Deserialize(source_origin))
                .SetReportingOrigin(
                    *SuitableOrigin::Deserialize(reporting_origin))
                .SetDestinationOrigin(
                    *SuitableOrigin::Deserialize(destination_origin))
                .SetExpiry(base::Days(30))
                .Build())
        .status;
  };

  store_source("https://s1.test", "https://a.r.test", "https://d1.test");
  store_source("https://s1.test", "https://a.r.test", "https://d2.test");
  store_source("https://s1.test", "https://a.r.test", "https://d3.test");
  EXPECT_THAT(storage()->GetActiveSources(), SizeIs(3));

  // This should succeed because the destination is already present on an
  // unexpired source.
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

TEST_F(AttributionStorageTest, DestinationLimit_ApplyLimit) {
  delegate()->set_max_destinations_per_source_site_reporting_origin(1);
  delegate()->set_delete_expired_sources_frequency(base::Milliseconds(10));

  const base::TimeDelta expiry = base::Milliseconds(5);

  const auto store_source = [&](const char* source_origin,
                                const char* reporting_origin,
                                const char* destination_origin) {
    return storage()
        ->StoreSource(
            SourceBuilder()
                .SetSourceOrigin(*SuitableOrigin::Deserialize(source_origin))
                .SetReportingOrigin(
                    *SuitableOrigin::Deserialize(reporting_origin))
                .SetDestinationOrigin(
                    *SuitableOrigin::Deserialize(destination_origin))
                .SetExpiry(expiry)
                .Build())
        .status;
  };

  // Allowed by pending, allowed by unexpired.
  EXPECT_EQ(
      store_source("https://s.test", "https://a.r.test", "https://d1.test"),
      StorableSource::Result::kSuccess);

  // Dropped by pending, dropped by unexpired.
  EXPECT_EQ(
      store_source("https://s.test", "https://a.r.test", "https://d2.test"),
      StorableSource::Result::kInsufficientUniqueDestinationCapacity);

  EXPECT_EQ(AttributionTrigger::EventLevelResult::kSuccess,
            MaybeCreateAndStoreEventLevelReport(
                TriggerBuilder()
                    .SetReportingOrigin(
                        *SuitableOrigin::Deserialize("https://a.r.test"))
                    .SetDestinationOrigin(
                        *SuitableOrigin::Deserialize("https://d1.test"))
                    .Build()));

  // Allowed by pending, dropped by unexpired (therefore dropped and not stored).
  EXPECT_EQ(
      store_source("https://s.test", "https://a.r.test", "https://d2.test"),
      StorableSource::Result::kInsufficientUniqueDestinationCapacity);

  task_environment_.FastForwardBy(expiry);

  // Allowed by pending, allowed by unexpired.
  EXPECT_EQ(
      store_source("https://s.test", "https://a.r.test", "https://d3.test"),
      StorableSource::Result::kSuccess);
}

TEST_F(AttributionStorageTest,
       MaxAttributionDestinationsPerSource_AppliesToNavigationSources) {
  delegate()->set_max_destinations_per_source_site_reporting_origin(1);
  storage()->StoreSource(SourceBuilder()
                             .SetDestinationOrigin(*SuitableOrigin::Deserialize(
                                 "https://a.example/"))
                             .Build());
  storage()->StoreSource(SourceBuilder()
                             .SetDestinationOrigin(*SuitableOrigin::Deserialize(
                                 "https://b.example"))
                             .Build());

  EXPECT_THAT(storage()->GetActiveSources(), SizeIs(1));
}

TEST_F(AttributionStorageTest,
       MaxAttributionDestinationsPerSource_CountsAllSourceTypes) {
  delegate()->set_max_destinations_per_source_site_reporting_origin(1);
  storage()->StoreSource(SourceBuilder()
                             .SetDestinationOrigin(*SuitableOrigin::Deserialize(
                                 "https://a.example/"))
                             .SetSourceType(AttributionSourceType::kNavigation)
                             .Build());
  AttributionStorage::StoreSourceResult result = storage()->StoreSource(
      SourceBuilder()
          .SetDestinationOrigin(
              *SuitableOrigin::Deserialize("https://b.example"))
          .SetSourceType(AttributionSourceType::kEvent)
          .Build());
  EXPECT_EQ(result.status,
            StorableSource::Result::kInsufficientUniqueDestinationCapacity);
  EXPECT_EQ(result.max_destinations_per_source_site_reporting_origin, 1);

  EXPECT_THAT(storage()->GetActiveSources(), SizeIs(1));
}

TEST_F(AttributionStorageTest,
       MaxAttributionDestinationsPerSource_CountsUnexpiredSources) {
  delegate()->set_max_destinations_per_source_site_reporting_origin(1);
  delegate()->set_delete_expired_rate_limits_frequency(base::Milliseconds(10));

  const base::TimeDelta expiry = base::Milliseconds(5);

  storage()->StoreSource(SourceBuilder()
                             .SetDestinationOrigin(*SuitableOrigin::Deserialize(
                                 "https://a.example/"))
                             .SetSourceType(AttributionSourceType::kNavigation)
                             .SetExpiry(expiry)
                             .Build());
  storage()->StoreSource(SourceBuilder()
                             .SetDestinationOrigin(*SuitableOrigin::Deserialize(
                                 "https://b.example"))
                             .SetSourceType(AttributionSourceType::kEvent)
                             .Build());

  EXPECT_THAT(storage()->GetActiveSources(), SizeIs(1));

  task_environment_.FastForwardBy(expiry);
  EXPECT_THAT(storage()->GetActiveSources(), IsEmpty());

  storage()->StoreSource(SourceBuilder()
                             .SetDestinationOrigin(*SuitableOrigin::Deserialize(
                                 "https://b.example"))
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
  AttributionStorage::StoreSourceResult result =
      storage()->StoreSource(builder.Build());
  EXPECT_EQ(result.status, StorableSource::Result::kSuccessNoised);
  delegate()->set_randomized_response(absl::nullopt);

  base::Time trigger_time = base::Time::Now();

  AttributionReport expected_event_level_report =
      ReportBuilder(
          AttributionInfoBuilder(
              builder
                  .SetAttributionLogic(StoredSource::AttributionLogic::kFalsely)
                  .SetActiveState(StoredSource::ActiveState::
                                      kReachedEventLevelAttributionLimit)
                  .BuildStored())
              .SetTime(trigger_time)
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
                AttributionTrigger::EventLevelResult::kFalselyAttributedSource),
            CreateReportAggregatableStatusIs(
                AttributionTrigger::AggregatableResult::kSuccess)));

  // The source's aggregatable budget consumed changes between the two
  // GetAttributionReports() calls due to the aggregatable trigger, which
  // requires a reflection of that change within the event level report
  // for the test to pass.
  expected_event_level_report =
      ReportBuilder(
          AttributionInfoBuilder(
              builder
                  .SetAttributionLogic(StoredSource::AttributionLogic::kFalsely)
                  .SetAggregatableBudgetConsumed(1)
                  .SetActiveState(StoredSource::ActiveState::
                                      kReachedEventLevelAttributionLimit)
                  .BuildStored())
              .SetTime(trigger_time)
              .Build())
          .SetTriggerData(7)
          .SetReportTime(fake_report_time)
          .Build();

  const AttributionReport expected_aggregatable_report =
      GetExpectedAggregatableReport(
          builder.SetAggregatableBudgetConsumed(1).BuildStored(),
          DefaultAggregatableHistogramContributions({1}));

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
    EXPECT_EQ(result.status, test_case.randomized_response
                                 ? StorableSource::Result::kSuccessNoised
                                 : StorableSource::Result::kSuccess);
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
                    ReplacedEventLevelReportIs(absl::nullopt),
                    CreateReportSourceIs(Optional(SourceEventIdIs(5u))),
                    DroppedEventLevelReportIs(absl::nullopt)));

  // This conversion should replace the one above because it has a higher
  // priority.
  EXPECT_THAT(storage()->MaybeCreateAndStoreReport(
                  TriggerBuilder().SetPriority(2).SetTriggerData(21).Build()),
              AllOf(CreateReportEventLevelStatusIs(
                        AttributionTrigger::EventLevelResult::
                            kSuccessDroppedLowerPriority),
                    ReplacedEventLevelReportIs(
                        Optional(EventLevelDataIs(TriggerDataIs(20u)))),
                    CreateReportSourceIs(Optional(SourceEventIdIs(5u))),
                    DroppedEventLevelReportIs(absl::nullopt)));

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
                    ReplacedEventLevelReportIs(absl::nullopt),
                    CreateReportSourceIs(Optional(SourceEventIdIs(7u))),
                    DroppedEventLevelReportIs(
                        Optional(EventLevelDataIs(TriggerDataIs(23u))))));

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
  EXPECT_THAT(
      storage()->MaybeCreateAndStoreReport(
          TriggerBuilder().SetPriority(2).Build()),
      AllOf(CreateReportEventLevelStatusIs(
                AttributionTrigger::EventLevelResult::kExcessiveReports),
            DroppedEventLevelReportIs(
                Optional(EventLevelDataIs(TriggerPriorityIs(2))))));

  // As a result, the impression with data 5 should have reached event-level
  // attribution limit.
  EXPECT_THAT(
      storage()->GetActiveSources(),
      ElementsAre(SourceActiveStateIs(
          StoredSource::ActiveState::kReachedEventLevelAttributionLimit)));
}

TEST_F(AttributionStorageTest, DedupKey_Dedups) {
  storage()->StoreSource(SourceBuilder()
                             .SetSourceEventId(1)
                             .SetDestinationOrigin(*SuitableOrigin::Deserialize(
                                 "https://a.example"))
                             .Build());
  storage()->StoreSource(SourceBuilder()
                             .SetSourceEventId(2)
                             .SetDestinationOrigin(*SuitableOrigin::Deserialize(
                                 "https://b.example"))
                             .Build());
  EXPECT_THAT(storage()->GetActiveSources(),
              ElementsAre(DedupKeysAre(IsEmpty()), DedupKeysAre(IsEmpty())));

  EXPECT_EQ(AttributionTrigger::EventLevelResult::kSuccess,
            MaybeCreateAndStoreEventLevelReport(
                TriggerBuilder()
                    .SetDestinationOrigin(
                        *SuitableOrigin::Deserialize("https://a.example"))
                    .SetDedupKey(11)
                    .SetTriggerData(71)
                    .Build()));

  // Should be stored because dedup key doesn't match even though conversion
  // destination does.
  EXPECT_EQ(AttributionTrigger::EventLevelResult::kSuccess,
            MaybeCreateAndStoreEventLevelReport(
                TriggerBuilder()
                    .SetDestinationOrigin(
                        *SuitableOrigin::Deserialize("https://a.example"))
                    .SetDedupKey(12)
                    .SetTriggerData(72)
                    .Build()));

  // Should be stored because conversion destination doesn't match even though
  // dedup key does.
  EXPECT_EQ(AttributionTrigger::EventLevelResult::kSuccess,
            MaybeCreateAndStoreEventLevelReport(
                TriggerBuilder()
                    .SetDestinationOrigin(
                        *SuitableOrigin::Deserialize("https://b.example"))
                    .SetDedupKey(12)
                    .SetTriggerData(73)
                    .Build()));

  // Shouldn't be stored because conversion destination and dedup key match.
  auto result = storage()->MaybeCreateAndStoreReport(
      TriggerBuilder()
          .SetDestinationOrigin(
              *SuitableOrigin::Deserialize("https://a.example"))
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
                        *SuitableOrigin::Deserialize("https://b.example"))
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
  storage()->StoreSource(SourceBuilder()
                             .SetSourceEventId(1)
                             .SetDestinationOrigin(*SuitableOrigin::Deserialize(
                                 "https://a.example"))
                             .Build());
  EXPECT_THAT(storage()->GetActiveSources(), SizeIs(1));

  task_environment_.FastForwardBy(base::Milliseconds(1));

  EXPECT_EQ(AttributionTrigger::EventLevelResult::kSuccess,
            MaybeCreateAndStoreEventLevelReport(
                TriggerBuilder()
                    .SetDestinationOrigin(
                        *SuitableOrigin::Deserialize("https://a.example"))
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
                        *SuitableOrigin::Deserialize("https://a.example"))
                    .SetDedupKey(2)
                    .SetTriggerData(5)
                    .Build()));

  task_environment_.FastForwardBy(kReportDelay);
  EXPECT_THAT(storage()->GetAttributionReports(base::Time::Now()), IsEmpty());
}

TEST_F(AttributionStorageTest, AggregatableDedupKey_Dedups) {
  TestAggregatableSourceProvider provider;
  storage()->StoreSource(provider.GetBuilder()
                             .SetSourceEventId(1)
                             .SetDestinationOrigin(*SuitableOrigin::Deserialize(
                                 "https://a.example"))
                             .Build());
  storage()->StoreSource(provider.GetBuilder()
                             .SetSourceEventId(2)
                             .SetDestinationOrigin(*SuitableOrigin::Deserialize(
                                 "https://b.example"))
                             .Build());
  EXPECT_THAT(storage()->GetActiveSources(),
              ElementsAre(AggregatableDedupKeysAre(IsEmpty()),
                          AggregatableDedupKeysAre(IsEmpty())));

  EXPECT_EQ(AttributionTrigger::AggregatableResult::kSuccess,
            MaybeCreateAndStoreAggregatableReport(
                DefaultAggregatableTriggerBuilder()
                    .SetDestinationOrigin(
                        *SuitableOrigin::Deserialize("https://a.example"))
                    .SetAggregatableDedupKey(11)
                    .SetDebugKey(71)
                    .Build(/*generate_event_trigger_data=*/false)));

  // Should be stored because dedup key doesn't match even though attribution
  // destination does.
  EXPECT_EQ(AttributionTrigger::AggregatableResult::kSuccess,
            MaybeCreateAndStoreAggregatableReport(
                DefaultAggregatableTriggerBuilder()
                    .SetDestinationOrigin(
                        *SuitableOrigin::Deserialize("https://a.example"))
                    .SetAggregatableDedupKey(12)
                    .SetDebugKey(72)
                    .Build(/*generate_event_trigger_data=*/false)));

  // Should be stored because attribution destination doesn't match even though
  // dedup key does.
  EXPECT_EQ(AttributionTrigger::AggregatableResult::kSuccess,
            MaybeCreateAndStoreAggregatableReport(
                DefaultAggregatableTriggerBuilder()
                    .SetDestinationOrigin(
                        *SuitableOrigin::Deserialize("https://b.example"))
                    .SetAggregatableDedupKey(12)
                    .SetDebugKey(73)
                    .Build(/*generate_event_trigger_data=*/false)));

  // Shouldn't be stored because attribution destination and dedup key match.
  EXPECT_EQ(AttributionTrigger::AggregatableResult::kDeduplicated,
            MaybeCreateAndStoreAggregatableReport(
                DefaultAggregatableTriggerBuilder()
                    .SetDestinationOrigin(
                        *SuitableOrigin::Deserialize("https://a.example"))
                    .SetAggregatableDedupKey(11)
                    .SetDebugKey(74)
                    .Build(/*generate_event_trigger_data=*/false)));

  // Shouldn't be stored because attribution destination and dedup key match.
  EXPECT_EQ(AttributionTrigger::AggregatableResult::kDeduplicated,
            MaybeCreateAndStoreAggregatableReport(
                DefaultAggregatableTriggerBuilder()
                    .SetDestinationOrigin(
                        *SuitableOrigin::Deserialize("https://b.example"))
                    .SetAggregatableDedupKey(12)
                    .SetDebugKey(75)
                    .Build(/*generate_event_trigger_data=*/false)));

  task_environment_.FastForwardBy(kReportDelay);
  EXPECT_THAT(storage()->GetAttributionReports(base::Time::Now()),
              ElementsAre(TriggerDebugKeyIs(71u), TriggerDebugKeyIs(72u),
                          TriggerDebugKeyIs(73u)));

  EXPECT_THAT(storage()->GetActiveSources(),
              ElementsAre(AggregatableDedupKeysAre(ElementsAre(11, 12)),
                          AggregatableDedupKeysAre(ElementsAre(12))));
}

TEST_F(AttributionStorageTest,
       AggregatableDedupKey_DedupsAfterConversionDeletion) {
  storage()->StoreSource(TestAggregatableSourceProvider()
                             .GetBuilder()
                             .SetSourceEventId(1)
                             .SetDestinationOrigin(*SuitableOrigin::Deserialize(
                                 "https://a.example"))
                             .Build());
  EXPECT_THAT(storage()->GetActiveSources(), SizeIs(1));

  task_environment_.FastForwardBy(base::Milliseconds(1));

  EXPECT_EQ(AttributionTrigger::AggregatableResult::kSuccess,
            MaybeCreateAndStoreAggregatableReport(
                DefaultAggregatableTriggerBuilder()
                    .SetDestinationOrigin(
                        *SuitableOrigin::Deserialize("https://a.example"))
                    .SetAggregatableDedupKey(2)
                    .SetDebugKey(3)
                    .Build(/*generate_event_trigger_data=*/false)));

  task_environment_.FastForwardBy(kReportDelay);

  std::vector<AttributionReport> actual_reports =
      storage()->GetAttributionReports(base::Time::Now());
  EXPECT_THAT(actual_reports, ElementsAre(TriggerDebugKeyIs(3u)));

  // Simulate the report being sent and deleted from storage.
  DeleteReports(actual_reports);

  task_environment_.FastForwardBy(base::Milliseconds(1));

  // This report shouldn't be stored, as it should be deduped against the
  // previously stored one even though that previous one is no longer in the DB.
  EXPECT_EQ(AttributionTrigger::AggregatableResult::kDeduplicated,
            MaybeCreateAndStoreAggregatableReport(
                DefaultAggregatableTriggerBuilder()
                    .SetDestinationOrigin(
                        *SuitableOrigin::Deserialize("https://a.example"))
                    .SetAggregatableDedupKey(2)
                    .SetDebugKey(5)
                    .Build(/*generate_event_trigger_data=*/false)));

  task_environment_.FastForwardBy(kReportDelay);
  EXPECT_THAT(storage()->GetAttributionReports(base::Time::Now()), IsEmpty());
}

TEST_F(AttributionStorageTest, DedupKey_AggregatableReportNotDedups) {
  storage()->StoreSource(TestAggregatableSourceProvider()
                             .GetBuilder()
                             .SetSourceEventId(1)
                             .SetDestinationOrigin(*SuitableOrigin::Deserialize(
                                 "https://a.example"))
                             .Build());

  auto result = storage()->MaybeCreateAndStoreReport(
      DefaultAggregatableTriggerBuilder()
          .SetDestinationOrigin(
              *SuitableOrigin::Deserialize("https://a.example"))
          .SetDedupKey(11)
          .Build());
  EXPECT_EQ(AttributionTrigger::EventLevelResult::kSuccess,
            result.event_level_status());
  EXPECT_EQ(AttributionTrigger::AggregatableResult::kSuccess,
            result.aggregatable_status());

  result = storage()->MaybeCreateAndStoreReport(
      DefaultAggregatableTriggerBuilder()
          .SetDestinationOrigin(
              *SuitableOrigin::Deserialize("https://a.example"))
          .SetDedupKey(11)
          .Build());

  EXPECT_EQ(AttributionTrigger::EventLevelResult::kDeduplicated,
            result.event_level_status());
  EXPECT_EQ(AttributionTrigger::AggregatableResult::kSuccess,
            result.aggregatable_status());
}

TEST_F(AttributionStorageTest, AggregatableDedupKey_EventLevelReportNotDedups) {
  storage()->StoreSource(TestAggregatableSourceProvider()
                             .GetBuilder()
                             .SetSourceEventId(1)
                             .SetDestinationOrigin(*SuitableOrigin::Deserialize(
                                 "https://a.example"))
                             .Build());

  auto result = storage()->MaybeCreateAndStoreReport(
      DefaultAggregatableTriggerBuilder()
          .SetDestinationOrigin(
              *SuitableOrigin::Deserialize("https://a.example"))
          .SetAggregatableDedupKey(11)
          .Build());
  EXPECT_EQ(AttributionTrigger::EventLevelResult::kSuccess,
            result.event_level_status());
  EXPECT_EQ(AttributionTrigger::AggregatableResult::kSuccess,
            result.aggregatable_status());

  result = storage()->MaybeCreateAndStoreReport(
      DefaultAggregatableTriggerBuilder()
          .SetDestinationOrigin(
              *SuitableOrigin::Deserialize("https://a.example"))
          .SetAggregatableDedupKey(11)
          .Build());

  EXPECT_EQ(AttributionTrigger::EventLevelResult::kSuccess,
            result.event_level_status());
  EXPECT_EQ(AttributionTrigger::AggregatableResult::kDeduplicated,
            result.aggregatable_status());
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
          AllOf(ReportTypeIs(AttributionReport::Type::kEventLevel),
                FailedSendAttemptsIs(0)),
          AllOf(ReportTypeIs(AttributionReport::Type::kAggregatableAttribution),
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

TEST_F(AttributionStorageTest,
       MaybeCreateAndStoreEventLevelReport_ReturnsDeactivatedSources) {
  SourceBuilder builder;
  builder.SetSourceEventId(7);
  storage()->StoreSource(builder.Build());
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
  EXPECT_THAT(
      storage()->MaybeCreateAndStoreReport(
          TriggerBuilder().SetTriggerData(20).Build()),
      AllOf(CreateReportEventLevelStatusIs(
                AttributionTrigger::EventLevelResult::kExcessiveReports),
            ReplacedEventLevelReportIs(absl::nullopt),
            DroppedEventLevelReportIs(
                Optional(EventLevelDataIs(TriggerDataIs(20u))))));
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

TEST_F(AttributionStorageTest,
       AdjustOfflineReportTimes_ReturnsMinReportTimeWithoutDelay) {
  delegate()->set_offline_report_delay_config(absl::nullopt);

  ASSERT_EQ(storage()->AdjustOfflineReportTimes(), absl::nullopt);

  storage()->StoreSource(SourceBuilder().Build());
  ASSERT_EQ(AttributionTrigger::EventLevelResult::kSuccess,
            MaybeCreateAndStoreEventLevelReport(DefaultTrigger()));

  std::vector<AttributionReport> reports =
      storage()->GetAttributionReports(base::Time::Max());
  ASSERT_THAT(reports, SizeIs(1));

  ASSERT_EQ(storage()->AdjustOfflineReportTimes(),
            reports.front().report_time());
}

TEST_F(AttributionStorageTest, GetNextEventReportTime) {
  const auto origin_a = *SuitableOrigin::Deserialize("https://a.example/");
  const auto origin_b = *SuitableOrigin::Deserialize("https://b.example/");

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
  auto aggregation_keys =
      attribution_reporting::AggregationKeys::FromKeys({{"key", 345}});
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
          .SetReportingOrigin(*SuitableOrigin::Deserialize("https://r1.test"))
          .SetDebugKey(1)
          .Build());
  ASSERT_EQ(result.status, StorableSource::Result::kSuccess);

  result = storage()->StoreSource(
      SourceBuilder()
          .SetReportingOrigin(*SuitableOrigin::Deserialize("https://r2.test"))
          .SetDebugKey(2)
          .Build());
  ASSERT_EQ(result.status, StorableSource::Result::kSuccess);

  result = storage()->StoreSource(
      SourceBuilder()
          .SetReportingOrigin(*SuitableOrigin::Deserialize("https://r3.test"))
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

  const auto origin1 = *SuitableOrigin::Deserialize("https://r1.test");
  const auto origin2 = *SuitableOrigin::Deserialize("https://r2.test");
  const auto origin3 = *SuitableOrigin::Deserialize("https://r3.test");

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
                AttributionTrigger::AggregatableResult::kSuccess),
            CreateReportMaxAttributionReportingOriginsLimitIs(absl::nullopt)));

  ASSERT_THAT(
      storage()->MaybeCreateAndStoreReport(
          trigger_builder.SetReportingOrigin(origin2).SetDebugKey(2).Build()),
      AllOf(CreateReportEventLevelStatusIs(
                AttributionTrigger::EventLevelResult::kSuccess),
            CreateReportAggregatableStatusIs(
                AttributionTrigger::AggregatableResult::kSuccess),
            CreateReportMaxAttributionReportingOriginsLimitIs(absl::nullopt)));

  ASSERT_THAT(
      storage()->MaybeCreateAndStoreReport(
          trigger_builder.SetReportingOrigin(origin3).SetDebugKey(3).Build()),
      AllOf(
          CreateReportEventLevelStatusIs(
              AttributionTrigger::EventLevelResult::kExcessiveReportingOrigins),
          CreateReportAggregatableStatusIs(
              AttributionTrigger::AggregatableResult::
                  kExcessiveReportingOrigins),
          CreateReportMaxAttributionReportingOriginsLimitIs(2)));

  // Two event-level reports, two aggregatable reports.
  EXPECT_THAT(storage()->GetAttributionReports(base::Time::Max()),
              ElementsAre(TriggerDebugKeyIs(1), TriggerDebugKeyIs(2),
                          TriggerDebugKeyIs(1), TriggerDebugKeyIs(2)));
}

TEST_F(AttributionStorageTest, SourceBudgetValueRetrieved) {
  storage()->StoreSource(SourceBuilder().Build());
  EXPECT_THAT(storage()->GetActiveSources(),
              ElementsAre(AggregatableBudgetConsumedIs(0)));
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
  EXPECT_THAT(
      storage()->MaybeCreateAndStoreReport(DefaultAggregatableTriggerBuilder(
                                               /*histogram_values=*/{17})
                                               .Build()),
      AllOf(CreateReportAggregatableStatusIs(
                AttributionTrigger::AggregatableResult::kInsufficientBudget),
            CreateReportAggreggatableBudgetPerSourceIs(16)));

  EXPECT_THAT(
      storage()->MaybeCreateAndStoreReport(DefaultAggregatableTriggerBuilder(
                                               /*histogram_values=*/{2, 5})
                                               .Build()),
      AllOf(CreateReportAggregatableStatusIs(
                AttributionTrigger::AggregatableResult::kSuccess),
            CreateReportAggreggatableBudgetPerSourceIs(absl::nullopt)));

  EXPECT_THAT(
      storage()->MaybeCreateAndStoreReport(DefaultAggregatableTriggerBuilder(
                                               /*histogram_values=*/{10})
                                               .Build()),
      AllOf(CreateReportAggregatableStatusIs(
                AttributionTrigger::AggregatableResult::kInsufficientBudget),
            CreateReportAggreggatableBudgetPerSourceIs(16)));

  EXPECT_THAT(
      storage()->MaybeCreateAndStoreReport(DefaultAggregatableTriggerBuilder(
                                               /*histogram_values=*/{9})
                                               .Build()),
      AllOf(CreateReportAggregatableStatusIs(
                AttributionTrigger::AggregatableResult::kSuccess),
            CreateReportAggreggatableBudgetPerSourceIs(absl::nullopt)));

  EXPECT_THAT(
      storage()->MaybeCreateAndStoreReport(DefaultAggregatableTriggerBuilder(
                                               /*histogram_values=*/{1})
                                               .Build()),
      AllOf(CreateReportAggregatableStatusIs(
                AttributionTrigger::AggregatableResult::kInsufficientBudget),
            CreateReportAggreggatableBudgetPerSourceIs(16)));

  // The second source has higher priority and should have capacity.
  storage()->StoreSource(provider.GetBuilder().SetPriority(10).Build());

  EXPECT_THAT(
      storage()->MaybeCreateAndStoreReport(DefaultAggregatableTriggerBuilder(
                                               /*histogram_values=*/{9})
                                               .Build()),
      AllOf(CreateReportAggregatableStatusIs(
                AttributionTrigger::AggregatableResult::kSuccess),
            CreateReportAggreggatableBudgetPerSourceIs(absl::nullopt)));
}

TEST_F(AttributionStorageTest, BudgetConsumedAfterTriggerIsRetrieved) {
  auto provider = TestAggregatableSourceProvider(/*size=*/1);
  storage()->StoreSource(provider.GetBuilder().Build());

  EXPECT_EQ(
      MaybeCreateAndStoreAggregatableReport(DefaultAggregatableTriggerBuilder(
                                                /*histogram_values=*/{2})
                                                .Build()),
      AttributionTrigger::AggregatableResult::kSuccess);

  EXPECT_THAT(storage()->GetActiveSources(),
              ElementsAre(AggregatableBudgetConsumedIs(2)));
}

TEST_F(AttributionStorageTest,
       GetAttributionReports_SetsRandomizedTriggerRate) {
  delegate()->set_randomized_response_rates(/*navigation=*/.2, /*event=*/.4);

  const auto origin1 = *SuitableOrigin::Deserialize("https://r1.test");
  const auto origin2 = *SuitableOrigin::Deserialize("https://r2.test");

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

  const auto origin1 = *SuitableOrigin::Deserialize("https://r1.test");
  const auto origin2 = *SuitableOrigin::Deserialize("https://r2.test");

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

  const auto filter_data = AttributionFilterData::Create({{"abc", {"x", "y"}}});
  ASSERT_TRUE(filter_data.has_value());

  storage()->StoreSource(SourceBuilder()
                             .SetFilterData(*filter_data)
                             .SetSourceType(AttributionSourceType::kEvent)
                             .Build());

  EXPECT_THAT(storage()->GetActiveSources(),
              ElementsAre(SourceFilterDataIs(AttributionFilterData()),
                          SourceFilterDataIs(filter_data)));
}

TEST_F(AttributionStorageTest, NoMatchingTriggerData_ReturnsError) {
  const auto origin = *SuitableOrigin::Deserialize("https://r.test");

  storage()->StoreSource(SourceBuilder()
                             .SetSourceType(AttributionSourceType::kNavigation)
                             .SetDestinationOrigin(origin)
                             .SetReportingOrigin(origin)
                             .Build());

  EXPECT_EQ(
      AttributionTrigger::EventLevelResult::kNoMatchingConfigurations,
      MaybeCreateAndStoreEventLevelReport(AttributionTrigger(
          /*reporting_origin=*/origin,
          attribution_reporting::TriggerRegistration(
              /*filters=*/AttributionFilters(),
              /*not_filters=*/AttributionFilters(),
              /*debug_key=*/absl::nullopt,
              /*aggregatable_dedup_key=*/absl::nullopt,
              *attribution_reporting::EventTriggerDataList::Create(
                  {attribution_reporting::EventTriggerData(
                      /*data=*/11,
                      /*priority=*/12,
                      /*dedup_key=*/13,
                      /*filters=*/
                      AttributionFiltersForSourceType(
                          AttributionSourceType::kEvent),
                      /*not_filters=*/AttributionFilters())}),
              /*aggregatable_trigger_data=*/
              attribution_reporting::AggregatableTriggerDataList(),
              /*aggregatable_values=*/
              attribution_reporting::AggregatableValues(),
              /*debug_reporting=*/false,
              ::aggregation_service::mojom::AggregationCoordinator::kDefault),
          /*destination_origin=*/origin,
          /*is_within_fenced_frame=*/false)));

  EXPECT_THAT(storage()->GetAttributionReports(base::Time::Max()), IsEmpty());

  EXPECT_THAT(storage()->GetActiveSources(),
              ElementsAre(DedupKeysAre(IsEmpty())));
}

TEST_F(AttributionStorageTest, MatchingTriggerData_UsesCorrectData) {
  const auto origin = *SuitableOrigin::Deserialize("https://r.test");

  storage()->StoreSource(
      SourceBuilder()
          .SetSourceType(AttributionSourceType::kNavigation)
          .SetDestinationOrigin(origin)
          .SetReportingOrigin(origin)
          .SetFilterData(*AttributionFilterData::Create({{"abc", {"123"}}}))
          .Build());

  const std::vector<attribution_reporting::EventTriggerData> event_triggers = {
      // Filters don't match.
      attribution_reporting::EventTriggerData(
          /*data=*/11,
          /*priority=*/12,
          /*dedup_key=*/13,
          /*filters=*/
          *AttributionFilters::Create({
              {"abc", {"456"}},
          }),
          /*not_filters=*/AttributionFilters()),

      // Filters match, but negated filters do not.
      attribution_reporting::EventTriggerData(
          /*data=*/21,
          /*priority=*/22,
          /*dedup_key=*/23,
          /*filters=*/
          *AttributionFilters::Create({
              {"abc", {"123"}},
          }),
          /*not_filters=*/
          *AttributionFilters::Create({
              {"source_type", {"navigation"}},
          })),

      // Filters and negated filters match.
      attribution_reporting::EventTriggerData(
          /*data=*/31,
          /*priority=*/32,
          /*dedup_key=*/33,
          /*filters=*/
          *AttributionFilters::Create({
              {"abc", {"123"}},
          }),
          /*not_filters=*/
          *AttributionFilters::Create({
              {"source_type", {"event"}},
          })),

      // Filters and negated filters match, but not the first event
      // trigger to match.
      attribution_reporting::EventTriggerData(
          /*data=*/41,
          /*priority=*/42,
          /*dedup_key=*/43,
          /*filters=*/
          *AttributionFilters::Create({
              {"abc", {"123"}},
          }),
          /*not_filters=*/
          *AttributionFilters::Create({
              {"source_type", {"event"}},
          })),
  };

  EXPECT_EQ(
      AttributionTrigger::EventLevelResult::kSuccess,
      MaybeCreateAndStoreEventLevelReport(AttributionTrigger(
          /*reporting_origin=*/origin,
          attribution_reporting::TriggerRegistration(
              /*filters=*/AttributionFilters(),
              /*not_filters=*/AttributionFilters(),
              /*debug_key=*/absl::nullopt,
              /*aggregatable_dedup_key=*/absl::nullopt,
              *attribution_reporting::EventTriggerDataList::Create(
                  event_triggers),
              /*aggregatable_trigger_data=*/
              attribution_reporting::AggregatableTriggerDataList(),
              /*aggregatable_values=*/
              attribution_reporting::AggregatableValues(),
              /*debug_reporting=*/false,
              ::aggregation_service::mojom::AggregationCoordinator::kDefault),
          /*destination_origin=*/origin,
          /*is_within_fenced_frame=*/false)));

  EXPECT_THAT(storage()->GetAttributionReports(base::Time::Max()),
              ElementsAre(EventLevelDataIs(
                  AllOf(TriggerDataIs(31), TriggerPriorityIs(32)))));

  EXPECT_THAT(storage()->GetActiveSources(),
              ElementsAre(DedupKeysAre(ElementsAre(33))));
}

TEST_F(AttributionStorageTest, TopLevelTriggerFiltering) {
  const auto origin = *SuitableOrigin::Deserialize("https://r.test");

  std::vector<attribution_reporting::AggregatableTriggerData>
      aggregatable_trigger_data{
          *attribution_reporting::AggregatableTriggerData::Create(
              absl::MakeUint128(/*high=*/1, /*low=*/0),
              /*source_keys=*/{"0"},
              /*filters=*/AttributionFilters(),
              /*not_filters=*/AttributionFilters())};

  auto aggregatable_values =
      *attribution_reporting::AggregatableValues::Create({{"0", 1}});

  storage()->StoreSource(
      SourceBuilder()
          .SetDestinationOrigin(origin)
          .SetReportingOrigin(origin)
          .SetFilterData(*AttributionFilterData::Create({{"abc", {"123"}}}))
          .SetAggregationKeys(
              *attribution_reporting::AggregationKeys::FromKeys({{"0", 1}}))
          .Build());

  AttributionTrigger trigger1(
      /*reporting_origin=*/origin,
      attribution_reporting::TriggerRegistration(
          /*filters=*/
          *AttributionFilters::Create({
              {"abc", {"456"}},
          }),
          /*not_filters=*/AttributionFilters(),
          /*debug_key=*/absl::nullopt,
          /*aggregatable_dedup_key=*/absl::nullopt,
          /*event_triggers=*/attribution_reporting::EventTriggerDataList(),
          *attribution_reporting::AggregatableTriggerDataList::Create(
              aggregatable_trigger_data),
          aggregatable_values,
          /*debug_reporting=*/false,
          ::aggregation_service::mojom::AggregationCoordinator::kDefault),
      /*destination_origin=*/origin, /*is_within_fenced_frame=*/false);

  AttributionTrigger trigger2(
      /*reporting_origin=*/origin,
      attribution_reporting::TriggerRegistration(
          /*filters=*/
          *AttributionFilters::Create({
              {"abc", {"123"}},
          }),
          /*not_filters=*/AttributionFilters(),
          /*debug_key=*/absl::nullopt,
          /*aggregatable_dedup_key=*/absl::nullopt,
          /*event_triggers=*/attribution_reporting::EventTriggerDataList(),
          *attribution_reporting::AggregatableTriggerDataList::Create(
              aggregatable_trigger_data),
          aggregatable_values,
          /*debug_reporting=*/false,
          ::aggregation_service::mojom::AggregationCoordinator::kDefault),
      /*destination_origin=*/origin, /*is_within_fenced_frame=*/false);

  AttributionTrigger trigger3(
      /*reporting_origin=*/origin,
      attribution_reporting::TriggerRegistration(
          /*filters=*/AttributionFilters(),
          /*not_filters=*/
          AttributionFiltersForSourceType(AttributionSourceType::kNavigation),
          /*debug_key=*/absl::nullopt,
          /*aggregatable_dedup_key=*/absl::nullopt,
          /*event_triggers=*/attribution_reporting::EventTriggerDataList(),
          *attribution_reporting::AggregatableTriggerDataList::Create(
              aggregatable_trigger_data),
          aggregatable_values,
          /*debug_reporting=*/false,
          ::aggregation_service::mojom::AggregationCoordinator::kDefault),
      /*destination_origin=*/origin,
      /*is_within_fenced_frame=*/false);

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

  EXPECT_THAT(storage()->MaybeCreateAndStoreReport(trigger3),
              AllOf(CreateReportEventLevelStatusIs(
                        AttributionTrigger::EventLevelResult::
                            kNoMatchingSourceFilterData),
                    CreateReportAggregatableStatusIs(
                        AttributionTrigger::AggregatableResult::
                            kNoMatchingSourceFilterData)));
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

  const auto source =
      source_builder.SetAggregatableBudgetConsumed(5).BuildStored();
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
  storage()->StoreSource(builder.Build());
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
                AttributionTrigger::EventLevelResult::kExcessiveReports),
            CreateReportAggregatableStatusIs(
                AttributionTrigger::AggregatableResult::kSuccess),
            ReplacedEventLevelReportIs(absl::nullopt),
            NewEventLevelReportIs(absl::nullopt),
            NewAggregatableReportIs(Optional(AggregatableAttributionDataIs(
                AggregatableHistogramContributionsAre(
                    DefaultAggregatableHistogramContributions(
                        /*histogram_values=*/{5}))))),
            DroppedEventLevelReportIs(
                Optional(EventLevelDataIs(TriggerDataIs(5u))))));
  EXPECT_THAT(
      storage()->GetActiveSources(),
      ElementsAre(SourceActiveStateIs(
          StoredSource::ActiveState::kReachedEventLevelAttributionLimit)));
}

TEST_F(AttributionStorageTest, AggregatableReportFiltering) {
  storage()->StoreSource(
      SourceBuilder()
          .SetFilterData(*AttributionFilterData::Create({{"abc", {"123"}}}))
          .SetAggregationKeys(
              *attribution_reporting::AggregationKeys::FromKeys({{"0", 1}}))
          .Build());

  EXPECT_EQ(
      MaybeCreateAndStoreAggregatableReport(
          TriggerBuilder()
              .SetAggregatableTriggerData(
                  {*attribution_reporting::AggregatableTriggerData::Create(
                      absl::MakeUint128(/*high=*/1, /*low=*/0),
                      /*source_keys=*/{"0"},
                      /*filters=*/
                      AttributionFilters(),
                      /*not_filters=*/AttributionFilters())})
              .Build()),
      AttributionTrigger::AggregatableResult::kNoHistograms);
}

TEST_F(AttributionStorageTest,
       PrioritizationConsidersAttributedAndUnattributedSources) {
  storage()->StoreSource(
      SourceBuilder().SetSourceEventId(3).SetPriority(10).Build());

  ASSERT_EQ(AttributionTrigger::EventLevelResult::kSuccess,
            MaybeCreateAndStoreEventLevelReport(DefaultTrigger()));

  storage()->StoreSource(
      SourceBuilder().SetSourceEventId(0).SetPriority(2).Build());

  ASSERT_EQ(AttributionTrigger::EventLevelResult::kSuccess,
            MaybeCreateAndStoreEventLevelReport(DefaultTrigger()));

  EXPECT_THAT(storage()->GetAttributionReports(base::Time::Max()),
              ElementsAre(ReportSourceIs(SourceEventIdIs(3)),
                          ReportSourceIs(SourceEventIdIs(3))));
}

TEST_F(AttributionStorageTest,
       MaybeCreateAndStoreEventLevelReport_DeactivatesUnattributedSources) {
  storage()->StoreSource(
      SourceBuilder().SetSourceEventId(3).SetPriority(1).Build());

  ASSERT_EQ(AttributionTrigger::EventLevelResult::kSuccess,
            MaybeCreateAndStoreEventLevelReport(DefaultTrigger()));

  storage()->StoreSource(
      SourceBuilder().SetSourceEventId(7).SetPriority(2).Build());

  ASSERT_EQ(AttributionTrigger::EventLevelResult::kSuccess,
            MaybeCreateAndStoreEventLevelReport(DefaultTrigger()));

  ASSERT_THAT(storage()->GetActiveSources(), ElementsAre(SourceEventIdIs(7)));

  // If the first source were deleted instead of deactivated, this would return
  // only a single report, as the join against the sources table would fail.
  ASSERT_THAT(storage()->GetAttributionReports(base::Time::Max()),
              ElementsAre(ReportSourceIs(SourceEventIdIs(3)),
                          ReportSourceIs(SourceEventIdIs(7))));
}

TEST_F(AttributionStorageTest, AggregationCoordinator_RoundTrip) {
  for (auto aggregation_coordinator :
       {::aggregation_service::mojom::AggregationCoordinator::kAwsCloud}) {
    storage()->StoreSource(
        TestAggregatableSourceProvider().GetBuilder().Build());

    EXPECT_THAT(
        storage()->MaybeCreateAndStoreReport(
            DefaultAggregatableTriggerBuilder()
                .SetAggregationCoordinator(aggregation_coordinator)
                .Build(/*generate_event_trigger_data=*/false)),
        AllOf(CreateReportAggregatableStatusIs(
                  AttributionTrigger::AggregatableResult::kSuccess),
              NewAggregatableReportIs(Optional(AggregatableAttributionDataIs(
                  AggregationCoordinatorIs(aggregation_coordinator))))));
    EXPECT_THAT(
        storage()->GetAttributionReports(/*max_report_time=*/base::Time::Max()),
        ElementsAre(AggregatableAttributionDataIs(
            AggregationCoordinatorIs(aggregation_coordinator))));

    storage()->ClearData(/*delete_begin=*/base::Time::Min(),
                         /*delete_end=*/base::Time::Max(),
                         /*filter=*/base::NullCallback());
  }
}

}  // namespace content
