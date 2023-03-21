// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_TEST_UTILS_H_
#define CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_TEST_UTILS_H_

#include <stdint.h>

#include <iosfwd>
#include <string>
#include <vector>

#include "base/containers/enum_set.h"
#include "base/containers/flat_set.h"
#include "base/guid.h"
#include "base/time/time.h"
#include "components/aggregation_service/aggregation_service.mojom.h"
#include "components/attribution_reporting/aggregatable_values.h"
#include "components/attribution_reporting/aggregation_keys.h"
#include "components/attribution_reporting/destination_set.h"
#include "components/attribution_reporting/filters.h"
#include "components/attribution_reporting/source_type.mojom.h"
#include "components/attribution_reporting/suitable_origin.h"
#include "components/attribution_reporting/test_utils.h"
#include "components/attribution_reporting/trigger_registration.h"
#include "content/browser/attribution_reporting/aggregatable_histogram_contribution.h"
#include "content/browser/attribution_reporting/attribution_info.h"
#include "content/browser/attribution_reporting/attribution_report.h"
#include "content/browser/attribution_reporting/attribution_storage_delegate.h"
#include "content/browser/attribution_reporting/attribution_trigger.h"
#include "content/browser/attribution_reporting/common_source_info.h"
#include "content/browser/attribution_reporting/create_report_result.h"
#include "content/browser/attribution_reporting/send_result.h"
#include "content/browser/attribution_reporting/storable_source.h"
#include "content/browser/attribution_reporting/stored_source.h"
#include "content/public/browser/attribution_data_model.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace net {
class SchemefulSite;
}  // namespace net

namespace network {
class TriggerAttestation;
}  // namespace network

namespace content {

class AttributionManager;

enum class RateLimitResult : int;

constexpr auto kSourceTypes =
    base::EnumSet<attribution_reporting::mojom::SourceType,
                  attribution_reporting::mojom::SourceType::kMinValue,
                  attribution_reporting::mojom::SourceType::kMaxValue>::All();

base::GUID DefaultExternalReportID();

base::Time GetExpiryTimeForTesting(base::TimeDelta declared_expiry,
                                   base::Time source_time);

absl::optional<base::Time> GetReportWindowTimeForTesting(
    absl::optional<base::TimeDelta> declared_window,
    base::Time source_time);

// Helper class to construct a StorableSource for tests using default data.
// StorableSource members are not mutable after construction requiring a
// builder pattern.
class SourceBuilder {
 public:
  explicit SourceBuilder(base::Time time = base::Time::Now());
  ~SourceBuilder();

  SourceBuilder(const SourceBuilder&);
  SourceBuilder(SourceBuilder&&);

  SourceBuilder& operator=(const SourceBuilder&);
  SourceBuilder& operator=(SourceBuilder&&);

  SourceBuilder& SetExpiry(base::TimeDelta delta);

  SourceBuilder& SetEventReportWindow(base::TimeDelta delta);

  SourceBuilder& SetAggregatableReportWindow(base::TimeDelta delta);

  SourceBuilder& SetSourceEventId(uint64_t source_event_id);

  SourceBuilder& SetSourceOrigin(attribution_reporting::SuitableOrigin);

  SourceBuilder& SetDestinationSites(base::flat_set<net::SchemefulSite>);

  SourceBuilder& SetReportingOrigin(attribution_reporting::SuitableOrigin);

  SourceBuilder& SetSourceType(attribution_reporting::mojom::SourceType);

  SourceBuilder& SetPriority(int64_t priority);

  SourceBuilder& SetAttributionLogic(
      StoredSource::AttributionLogic attribution_logic);

  SourceBuilder& SetFilterData(attribution_reporting::FilterData filter_data);

  SourceBuilder& SetActiveState(StoredSource::ActiveState active_state);

  SourceBuilder& SetDebugKey(absl::optional<uint64_t> debug_key);

  SourceBuilder& SetSourceId(StoredSource::Id source_id);

  SourceBuilder& SetDedupKeys(std::vector<uint64_t> dedup_keys);

  SourceBuilder& SetAggregationKeys(
      attribution_reporting::AggregationKeys aggregation_keys);

  SourceBuilder& SetAggregatableBudgetConsumed(
      int64_t aggregatable_budget_consumed);

  SourceBuilder& SetAggregatableDedupKeys(
      std::vector<uint64_t> aggregatable_dedup_keys);

  SourceBuilder& SetIsWithinFencedFrame(bool is_within_fenced_frame);

  SourceBuilder& SetDebugReporting(bool debug_reporting);

  StorableSource Build() const;

  StoredSource BuildStored() const;

  CommonSourceInfo BuildCommonInfo() const;

 private:
  uint64_t source_event_id_ = 123;
  base::Time source_time_;
  base::TimeDelta expiry_;
  absl::optional<base::TimeDelta> event_report_window_;
  absl::optional<base::TimeDelta> aggregatable_report_window_;
  attribution_reporting::SuitableOrigin source_origin_;
  attribution_reporting::DestinationSet destination_sites_;
  attribution_reporting::SuitableOrigin reporting_origin_;
  attribution_reporting::mojom::SourceType source_type_ =
      attribution_reporting::mojom::SourceType::kNavigation;
  int64_t priority_ = 0;
  StoredSource::AttributionLogic attribution_logic_ =
      StoredSource::AttributionLogic::kTruthfully;
  attribution_reporting::FilterData filter_data_;
  StoredSource::ActiveState active_state_ = StoredSource::ActiveState::kActive;
  absl::optional<uint64_t> debug_key_;
  // `base::StrongAlias` does not automatically initialize the value here.
  // Ensure that we don't use uninitialized memory.
  StoredSource::Id source_id_{0};
  std::vector<uint64_t> dedup_keys_;
  attribution_reporting::AggregationKeys aggregation_keys_;
  int64_t aggregatable_budget_consumed_ = 0;
  std::vector<uint64_t> aggregatable_dedup_keys_;
  bool is_within_fenced_frame_ = false;
  bool debug_reporting_ = false;
};

// Returns a AttributionTrigger with default data which matches the default
// impressions created by SourceBuilder.
AttributionTrigger DefaultTrigger();

// Helper class to construct a AttributionTrigger for tests using default data.
// AttributionTrigger members are not mutable after construction requiring a
// builder pattern.
class TriggerBuilder {
 public:
  TriggerBuilder();
  ~TriggerBuilder();

  TriggerBuilder(const TriggerBuilder&);
  TriggerBuilder(TriggerBuilder&&);

  TriggerBuilder& operator=(const TriggerBuilder&);
  TriggerBuilder& operator=(TriggerBuilder&&);

  TriggerBuilder& SetTriggerData(uint64_t trigger_data);

  TriggerBuilder& SetEventSourceTriggerData(uint64_t event_source_trigger_data);

  TriggerBuilder& SetDestinationOrigin(attribution_reporting::SuitableOrigin);

  TriggerBuilder& SetReportingOrigin(attribution_reporting::SuitableOrigin);

  TriggerBuilder& SetPriority(int64_t priority);

  TriggerBuilder& SetDedupKey(absl::optional<uint64_t> dedup_key);

  TriggerBuilder& SetDebugKey(absl::optional<uint64_t> debug_key);

  TriggerBuilder& SetAggregatableTriggerData(
      std::vector<attribution_reporting::AggregatableTriggerData>);

  TriggerBuilder& SetAggregatableValues(
      attribution_reporting::AggregatableValues);

  TriggerBuilder& SetAggregatableDedupKey(
      absl::optional<uint64_t> aggregatable_dedup_key);

  TriggerBuilder& SetIsWithinFencedFrame(bool is_within_fenced_frame);

  TriggerBuilder& SetDebugReporting(bool debug_reporting);

  TriggerBuilder& SetAggregationCoordinator(
      ::aggregation_service::mojom::AggregationCoordinator);

  TriggerBuilder& SetAttestation(
      absl::optional<network::TriggerAttestation> attestation);

  AttributionTrigger Build(bool generate_event_trigger_data = true) const;

 private:
  uint64_t trigger_data_ = 111;
  uint64_t event_source_trigger_data_ = 0;
  attribution_reporting::SuitableOrigin destination_origin_;
  attribution_reporting::SuitableOrigin reporting_origin_;
  int64_t priority_ = 0;
  absl::optional<uint64_t> dedup_key_;
  absl::optional<uint64_t> debug_key_;
  std::vector<attribution_reporting::AggregatableTriggerData>
      aggregatable_trigger_data_;
  attribution_reporting::AggregatableValues aggregatable_values_;
  absl::optional<uint64_t> aggregatable_dedup_key_;
  bool is_within_fenced_frame_ = false;
  bool debug_reporting_ = false;
  ::aggregation_service::mojom::AggregationCoordinator
      aggregation_coordinator_ =
          ::aggregation_service::mojom::AggregationCoordinator::kDefault;
  absl::optional<network::TriggerAttestation> attestation_;
};

// Helper class to construct an `AttributionInfo` for tests using default data.
class AttributionInfoBuilder {
 public:
  explicit AttributionInfoBuilder(
      StoredSource source,
      // For most tests, the context origin is irrelevant.
      attribution_reporting::SuitableOrigin context_origin =
          *attribution_reporting::SuitableOrigin::Deserialize(
              "https://conversion.test"));
  ~AttributionInfoBuilder();

  AttributionInfoBuilder& SetTime(base::Time time);

  AttributionInfoBuilder& SetDebugKey(absl::optional<uint64_t> debug_key);

  AttributionInfo Build() const;

 private:
  StoredSource source_;
  base::Time time_;
  absl::optional<uint64_t> debug_key_;
  attribution_reporting::SuitableOrigin context_origin_;
};

// Helper class to construct an `AttributionReport` for tests using default
// data.
class ReportBuilder {
 public:
  explicit ReportBuilder(AttributionInfo attribution_info);
  ~ReportBuilder();

  ReportBuilder& SetTriggerData(uint64_t trigger_data);

  ReportBuilder& SetReportTime(base::Time time);

  ReportBuilder& SetPriority(int64_t priority);

  ReportBuilder& SetExternalReportId(base::GUID external_report_id);

  ReportBuilder& SetRandomizedTriggerRate(double rate);

  ReportBuilder& SetReportId(AttributionReport::EventLevelData::Id id);

  ReportBuilder& SetReportId(
      AttributionReport::AggregatableAttributionData::Id id);

  ReportBuilder& SetAggregatableHistogramContributions(
      std::vector<AggregatableHistogramContribution> contributions);

  ReportBuilder& SetAggregationCoordinator(
      ::aggregation_service::mojom::AggregationCoordinator);

  ReportBuilder& SetAttestationToken(
      absl::optional<std::string> attestation_token);

  AttributionReport Build() const;

  AttributionReport BuildAggregatableAttribution() const;

 private:
  AttributionInfo attribution_info_;
  uint64_t trigger_data_ = 0;
  base::Time report_time_;
  int64_t priority_ = 0;
  base::GUID external_report_id_;
  double randomized_trigger_rate_ = 0;
  AttributionReport::EventLevelData::Id report_id_{0};
  AttributionReport::AggregatableAttributionData::Id
      aggregatable_attribution_report_id_{0};
  std::vector<AggregatableHistogramContribution> contributions_;
  ::aggregation_service::mojom::AggregationCoordinator
      aggregation_coordinator_ =
          ::aggregation_service::mojom::AggregationCoordinator::kDefault;
  absl::optional<std::string> attestation_token_;
};

bool operator==(const AttributionTrigger& a, const AttributionTrigger& b);

bool operator==(const CommonSourceInfo& a, const CommonSourceInfo& b);

bool operator==(const AttributionInfo& a, const AttributionInfo& b);

bool operator==(const AttributionStorageDelegate::FakeReport& a,
                const AttributionStorageDelegate::FakeReport& b);

bool operator<(const AttributionStorageDelegate::FakeReport& a,
               const AttributionStorageDelegate::FakeReport& b);

bool operator==(const StorableSource& a, const StorableSource& b);

bool operator==(const StoredSource& a, const StoredSource& b);

bool operator==(const AggregatableHistogramContribution& a,
                const AggregatableHistogramContribution& b);

bool operator==(const AttributionReport::EventLevelData& a,
                const AttributionReport::EventLevelData& b);

bool operator==(const AttributionReport::AggregatableAttributionData& a,
                const AttributionReport::AggregatableAttributionData& b);

bool operator==(const AttributionReport& a, const AttributionReport& b);

bool operator==(const SendResult& a, const SendResult& b);

std::ostream& operator<<(std::ostream& out,
                         AttributionTrigger::EventLevelResult status);

std::ostream& operator<<(std::ostream& out,
                         AttributionTrigger::AggregatableResult status);

std::ostream& operator<<(std::ostream& out, RateLimitResult result);

std::ostream& operator<<(std::ostream& out,
                         const AttributionTrigger& conversion);

std::ostream& operator<<(std::ostream& out, const CommonSourceInfo& source);

std::ostream& operator<<(std::ostream& out,
                         const AttributionInfo& attribution_info);

std::ostream& operator<<(std::ostream& out,
                         const AttributionStorageDelegate::FakeReport&);

std::ostream& operator<<(std::ostream& out, const StorableSource& source);

std::ostream& operator<<(std::ostream& out, const StoredSource& source);

std::ostream& operator<<(std::ostream& out,
                         const AggregatableHistogramContribution& contribution);

std::ostream& operator<<(std::ostream& out,
                         const AttributionReport::EventLevelData& data);

std::ostream& operator<<(
    std::ostream& out,
    const AttributionReport::AggregatableAttributionData& data);

std::ostream& operator<<(std::ostream& out, const AttributionReport& report);

std::ostream& operator<<(std::ostream& out,
                         AttributionReport::Type report_type);

std::ostream& operator<<(std::ostream& out, SendResult::Status status);

std::ostream& operator<<(std::ostream& out, const SendResult& info);

std::ostream& operator<<(std::ostream& out,
                         StoredSource::AttributionLogic attribution_logic);

std::ostream& operator<<(std::ostream& out,
                         StoredSource::ActiveState active_state);

// TODO: Move to test-only public header to be reused by other test code
// that rely on DataKey
std::ostream& operator<<(std::ostream& out,
                         const AttributionDataModel::DataKey& key);

std::vector<AttributionReport> GetAttributionReportsForTesting(
    AttributionManager* manager);

// Source matchers

MATCHER_P(SourceRegistrationIs, matcher, "") {
  return ExplainMatchResult(matcher, arg.registration(), result_listener);
}

MATCHER_P(CommonSourceInfoIs, matcher, "") {
  return ExplainMatchResult(matcher, arg.common_info(), result_listener);
}

MATCHER_P(SourceEventIdIs, matcher, "") {
  return ExplainMatchResult(matcher, arg.source_event_id(), result_listener);
}

MATCHER_P(ImpressionOriginIs, matcher, "") {
  return ExplainMatchResult(matcher, arg.common_info().source_origin(),
                            result_listener);
}

MATCHER_P(ReportingOriginIs, matcher, "") {
  return ExplainMatchResult(matcher, arg.common_info().reporting_origin(),
                            result_listener);
}

MATCHER_P(SourceTypeIs, matcher, "") {
  return ExplainMatchResult(matcher, arg.common_info().source_type(),
                            result_listener);
}

MATCHER_P(SourceDebugKeyIs, matcher, "") {
  return ExplainMatchResult(matcher, arg.debug_key(), result_listener);
}

MATCHER_P(SourceFilterDataIs, matcher, "") {
  return ExplainMatchResult(matcher, arg.filter_data(), result_listener);
}

MATCHER_P(DedupKeysAre, matcher, "") {
  return ExplainMatchResult(matcher, arg.dedup_keys(), result_listener);
}

MATCHER_P(AggregatableDedupKeysAre, matcher, "") {
  return ExplainMatchResult(matcher, arg.aggregatable_dedup_keys(),
                            result_listener);
}

MATCHER_P(AggregationKeysAre, matcher, "") {
  return ExplainMatchResult(matcher, arg.aggregation_keys(), result_listener);
}

MATCHER_P(AggregatableBudgetConsumedIs, matcher, "") {
  return ExplainMatchResult(matcher, arg.aggregatable_budget_consumed(),
                            result_listener);
}

MATCHER_P(SourceActiveStateIs, matcher, "") {
  return ExplainMatchResult(matcher, arg.active_state(), result_listener);
}

// Trigger matchers.

MATCHER_P(TriggerDestinationOriginIs, matcher, "") {
  return ExplainMatchResult(matcher, arg.destination_origin(), result_listener);
}

// Report matchers

MATCHER_P(ReportSourceIs, matcher, "") {
  return ExplainMatchResult(matcher, arg.attribution_info().source,
                            result_listener);
}

MATCHER_P(ReportTimeIs, matcher, "") {
  return ExplainMatchResult(matcher, arg.report_time(), result_listener);
}

MATCHER_P(FailedSendAttemptsIs, matcher, "") {
  return ExplainMatchResult(matcher, arg.failed_send_attempts(),
                            result_listener);
}

MATCHER_P(TriggerDebugKeyIs, matcher, "") {
  return ExplainMatchResult(matcher, arg.attribution_info().debug_key,
                            result_listener);
}

MATCHER_P(EventLevelDataIs, matcher, "") {
  return ExplainMatchResult(
      ::testing::VariantWith<AttributionReport::EventLevelData>(matcher),
      arg.data(), result_listener);
}

MATCHER_P(TriggerDataIs, matcher, "") {
  return ExplainMatchResult(matcher, arg.trigger_data, result_listener);
}

MATCHER_P(TriggerPriorityIs, matcher, "") {
  return ExplainMatchResult(matcher, arg.priority, result_listener);
}

MATCHER_P(RandomizedTriggerRateIs, matcher, "") {
  return ExplainMatchResult(matcher, arg.randomized_trigger_rate,
                            result_listener);
}

MATCHER_P(ReportURLIs, matcher, "") {
  return ExplainMatchResult(matcher, arg.ReportURL(), result_listener);
}

MATCHER_P(ReportTypeIs, matcher, "") {
  return ExplainMatchResult(matcher, arg.GetReportType(), result_listener);
}

MATCHER_P(AggregatableAttributionDataIs, matcher, "") {
  return ExplainMatchResult(
      ::testing::VariantWith<AttributionReport::AggregatableAttributionData>(
          matcher),
      arg.data(), result_listener);
}

MATCHER_P(AggregatableHistogramContributionsAre, matcher, "") {
  return ExplainMatchResult(matcher, arg.contributions, result_listener);
}

MATCHER_P(InitialReportTimeIs, matcher, "") {
  return ExplainMatchResult(matcher, arg.initial_report_time, result_listener);
}

MATCHER_P(AggregationCoordinatorIs, matcher, "") {
  return ExplainMatchResult(matcher, arg.aggregation_coordinator,
                            result_listener);
}

// `CreateReportResult` matchers

MATCHER_P(CreateReportEventLevelStatusIs, matcher, "") {
  return ExplainMatchResult(matcher, arg.event_level_status(), result_listener);
}

MATCHER_P(CreateReportAggregatableStatusIs, matcher, "") {
  return ExplainMatchResult(matcher, arg.aggregatable_status(),
                            result_listener);
}

MATCHER_P(ReplacedEventLevelReportIs, matcher, "") {
  return ExplainMatchResult(matcher, arg.replaced_event_level_report(),
                            result_listener);
}

MATCHER_P(DeactivatedSourceIs, matcher, "") {
  return ExplainMatchResult(matcher, arg.GetDeactivatedSource(),
                            result_listener);
}

MATCHER_P(NewEventLevelReportIs, matcher, "") {
  return ExplainMatchResult(matcher, arg.new_event_level_report(),
                            result_listener);
}

MATCHER_P(NewAggregatableReportIs, matcher, "") {
  return ExplainMatchResult(matcher, arg.new_aggregatable_report(),
                            result_listener);
}

MATCHER_P(DroppedEventLevelReportIs, matcher, "") {
  return ExplainMatchResult(matcher, arg.dropped_event_level_report(),
                            result_listener);
}

struct SourceRegistrationMatcherConfig {
  ::testing::Matcher<uint64_t> source_event_id = ::testing::_;
  ::testing::Matcher<const attribution_reporting::DestinationSet&>
      destination_set = ::testing::_;
  ::testing::Matcher<uint64_t> priority = ::testing::_;
  ::testing::Matcher<absl::optional<uint64_t>> debug_key = ::testing::_;
  ::testing::Matcher<const attribution_reporting::AggregationKeys&>
      aggregation_keys = ::testing::_;
  ::testing::Matcher<bool> debug_reporting = ::testing::_;

  SourceRegistrationMatcherConfig() = delete;
  explicit SourceRegistrationMatcherConfig(
      ::testing::Matcher<uint64_t> source_event_id = ::testing::_,
      ::testing::Matcher<const attribution_reporting::DestinationSet&>
          destination_set = ::testing::_,
      ::testing::Matcher<uint64_t> priority = ::testing::_,
      ::testing::Matcher<absl::optional<uint64_t>> debug_key = ::testing::_,
      ::testing::Matcher<const attribution_reporting::AggregationKeys&>
          aggregation_keys = ::testing::_,
      ::testing::Matcher<bool> debug_reporting = ::testing::_);
  ~SourceRegistrationMatcherConfig();
};

::testing::Matcher<const attribution_reporting::SourceRegistration&>
SourceRegistrationMatches(const SourceRegistrationMatcherConfig&);

struct EventTriggerDataMatcherConfig {
  ::testing::Matcher<uint64_t> data;
  ::testing::Matcher<int64_t> priority;
  ::testing::Matcher<absl::optional<uint64_t>> dedup_key;
  ::testing::Matcher<const attribution_reporting::FilterPair&> filters;

  EventTriggerDataMatcherConfig() = delete;
  explicit EventTriggerDataMatcherConfig(
      ::testing::Matcher<uint64_t> data = ::testing::_,
      ::testing::Matcher<int64_t> priority = ::testing::_,
      ::testing::Matcher<absl::optional<uint64_t>> dedup_key = ::testing::_,
      ::testing::Matcher<const attribution_reporting::FilterPair&> filters =
          ::testing::_);
  ~EventTriggerDataMatcherConfig();
};

::testing::Matcher<const attribution_reporting::EventTriggerData&>
EventTriggerDataMatches(const EventTriggerDataMatcherConfig&);

struct TriggerRegistrationMatcherConfig {
  ::testing::Matcher<const attribution_reporting::FilterPair&> filters =
      ::testing::_;
  ::testing::Matcher<absl::optional<uint64_t>> debug_key = ::testing::_;
  ::testing::Matcher<
      const std::vector<attribution_reporting::EventTriggerData>&>
      event_triggers = ::testing::_;
  ::testing::Matcher<
      const std::vector<attribution_reporting::AggregatableDedupKey>&>
      aggregatable_dedup_keys = ::testing::_;
  ::testing::Matcher<bool> debug_reporting = ::testing::_;
  ::testing::Matcher<
      const std::vector<attribution_reporting::AggregatableTriggerData>&>
      aggregatable_trigger_data = ::testing::_;
  ::testing::Matcher<const attribution_reporting::AggregatableValues&>
      aggregatable_values = ::testing::_;
  ::testing::Matcher<::aggregation_service::mojom::AggregationCoordinator>
      aggregation_coordinator = ::testing::_;

  TriggerRegistrationMatcherConfig() = delete;
  explicit TriggerRegistrationMatcherConfig(
      ::testing::Matcher<const attribution_reporting::FilterPair&> filters =
          ::testing::_,
      ::testing::Matcher<absl::optional<uint64_t>> debug_key = ::testing::_,
      ::testing::Matcher<
          const std::vector<attribution_reporting::EventTriggerData>&>
          event_triggers = ::testing::_,
      ::testing::Matcher<
          const std::vector<attribution_reporting::AggregatableDedupKey>&>
          aggregatable_dedup_keys = ::testing::_,
      ::testing::Matcher<bool> debug_reporting = ::testing::_,
      ::testing::Matcher<
          const std::vector<attribution_reporting::AggregatableTriggerData>&>
          aggregatable_trigger_data = ::testing::_,
      ::testing::Matcher<const attribution_reporting::AggregatableValues&>
          aggregatable_values = ::testing::_,
      ::testing::Matcher<::aggregation_service::mojom::AggregationCoordinator>
          aggregation_coordinator = ::testing::_);
  ~TriggerRegistrationMatcherConfig();
};

::testing::Matcher<const attribution_reporting::TriggerRegistration&>
TriggerRegistrationMatches(const TriggerRegistrationMatcherConfig&);

struct AttributionTriggerMatcherConfig {
  ::testing::Matcher<const attribution_reporting::SuitableOrigin&>
      reporting_origin = ::testing::_;
  ::testing::Matcher<const attribution_reporting::TriggerRegistration&>
      registration = ::testing::_;
  ::testing::Matcher<const attribution_reporting::SuitableOrigin&>
      destination_origin = ::testing::_;
  ::testing::Matcher<const absl::optional<network::TriggerAttestation>&>
      attestation = ::testing::_;

  ::testing::Matcher<bool> is_within_fenced_frame = ::testing::_;

  AttributionTriggerMatcherConfig() = delete;
  explicit AttributionTriggerMatcherConfig(
      ::testing::Matcher<const attribution_reporting::SuitableOrigin&>
          reporting_origin = ::testing::_,
      ::testing::Matcher<const attribution_reporting::TriggerRegistration&>
          registration = ::testing::_,
      ::testing::Matcher<const attribution_reporting::SuitableOrigin&>
          destination_origin = ::testing::_,
      ::testing::Matcher<bool> is_within_fenced_frame = ::testing::_);
  ~AttributionTriggerMatcherConfig();
};

::testing::Matcher<AttributionTrigger> AttributionTriggerMatches(
    const AttributionTriggerMatcherConfig&);

class TestAggregatableSourceProvider {
 public:
  explicit TestAggregatableSourceProvider(size_t size = 1);
  ~TestAggregatableSourceProvider();

  SourceBuilder GetBuilder(base::Time source_time = base::Time::Now()) const;

 private:
  attribution_reporting::AggregationKeys source_;
};

TriggerBuilder DefaultAggregatableTriggerBuilder(
    const std::vector<uint32_t>& histogram_values = {1});

std::vector<AggregatableHistogramContribution>
DefaultAggregatableHistogramContributions(
    const std::vector<uint32_t>& histogram_values = {1});

#if BUILDFLAG(IS_ANDROID)
struct OsRegistration;

bool operator==(const OsRegistration&, const OsRegistration&);

std::ostream& operator<<(std::ostream&, const OsRegistration&);
#endif

}  // namespace content

#endif  // CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_TEST_UTILS_H_
