// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_TEST_UTILS_H_
#define CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_TEST_UTILS_H_

#include <stdint.h>

#include <iosfwd>
#include <optional>
#include <string>
#include <vector>

#include "base/containers/flat_set.h"
#include "base/time/time.h"
#include "base/uuid.h"
#include "components/attribution_reporting/aggregatable_debug_reporting_config.h"
#include "components/attribution_reporting/aggregatable_filtering_id_max_bytes.h"
#include "components/attribution_reporting/constants.h"
#include "components/attribution_reporting/filters.h"
#include "components/attribution_reporting/source_registration.h"
#include "components/attribution_reporting/source_registration_time_config.mojom.h"
#include "components/attribution_reporting/source_type.mojom.h"
#include "components/attribution_reporting/suitable_origin.h"
#include "components/attribution_reporting/test_utils.h"
#include "components/attribution_reporting/trigger_data_matching.mojom-forward.h"
#include "content/browser/attribution_reporting/attribution_info.h"
#include "content/browser/attribution_reporting/attribution_report.h"
#include "content/browser/attribution_reporting/create_report_result.h"
#include "content/browser/attribution_reporting/send_result.h"
#include "content/browser/attribution_reporting/storable_source.h"
#include "content/browser/attribution_reporting/stored_source.h"
#include "content/public/browser/attribution_data_model.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/blink/public/mojom/aggregation_service/aggregatable_report.mojom-forward.h"

namespace attribution_reporting {
class AggregatableValues;
class AggregationKeys;
class AttributionScopesData;
class TriggerSpecs;
}  // namespace attribution_reporting

namespace net {
class SchemefulSite;
}  // namespace net

namespace content {

class AttributionTrigger;
class CommonSourceInfo;

enum class RateLimitResult : int;

base::Uuid DefaultExternalReportID();

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

  SourceBuilder& SetDebugKey(std::optional<uint64_t> debug_key);

  SourceBuilder& SetSourceId(StoredSource::Id source_id);

  SourceBuilder& SetDedupKeys(std::vector<uint64_t> dedup_keys);

  SourceBuilder& SetAggregationKeys(
      attribution_reporting::AggregationKeys aggregation_keys);

  SourceBuilder& SetRemainingAggregatableAttributionBudget(
      int remaining_aggregatable_attribution_budget);

  SourceBuilder& SetRemainingAggregatableDebugBudget(
      int remaining_aggregatable_debug_budget);

  SourceBuilder& SetRandomizedResponseRate(double randomized_response_rate);

  SourceBuilder& SetAggregatableDedupKeys(
      std::vector<uint64_t> aggregatable_dedup_keys);

  SourceBuilder& SetIsWithinFencedFrame(bool is_within_fenced_frame);

  SourceBuilder& SetDebugReporting(bool debug_reporting);

  SourceBuilder& SetTriggerSpecs(attribution_reporting::TriggerSpecs);

  SourceBuilder& SetMaxEventLevelReports(int max_event_level_reports);

  SourceBuilder& SetTriggerDataMatching(
      attribution_reporting::mojom::TriggerDataMatching);

  SourceBuilder& SetDebugCookieSet(bool debug_cookie_set);

  SourceBuilder& SetAggregatableDebugReportingConfig(
      attribution_reporting::SourceAggregatableDebugReportingConfig);

  SourceBuilder& SetDestinationLimitPriority(int64_t priority);

  SourceBuilder& SetAttributionScopesData(
      attribution_reporting::AttributionScopesData);

  StorableSource Build() const;

  StoredSource BuildStored() const;

 private:
  base::Time source_time_;
  attribution_reporting::SuitableOrigin source_origin_;
  attribution_reporting::SourceRegistration registration_;
  attribution_reporting::SuitableOrigin reporting_origin_;
  attribution_reporting::mojom::SourceType source_type_ =
      attribution_reporting::mojom::SourceType::kNavigation;
  StoredSource::AttributionLogic attribution_logic_ =
      StoredSource::AttributionLogic::kTruthfully;
  StoredSource::ActiveState active_state_ = StoredSource::ActiveState::kActive;
  // `base::StrongAlias` does not automatically initialize the value here.
  // Ensure that we don't use uninitialized memory.
  StoredSource::Id source_id_{0};
  std::vector<uint64_t> dedup_keys_;
  int remaining_aggregatable_attribution_budget_ =
      attribution_reporting::kMaxAggregatableValue;
  double randomized_response_rate_ = 0;
  std::vector<uint64_t> aggregatable_dedup_keys_;
  bool is_within_fenced_frame_ = false;
  bool debug_cookie_set_ = false;
  int remaining_aggregatable_debug_budget_ = 0;
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

  TriggerBuilder& SetDestinationOrigin(attribution_reporting::SuitableOrigin);

  TriggerBuilder& SetReportingOrigin(attribution_reporting::SuitableOrigin);

  TriggerBuilder& SetPriority(int64_t priority);

  TriggerBuilder& SetDedupKey(std::optional<uint64_t> dedup_key);

  TriggerBuilder& SetDebugKey(std::optional<uint64_t> debug_key);

  TriggerBuilder& SetAggregatableTriggerData(
      std::vector<attribution_reporting::AggregatableTriggerData>);

  TriggerBuilder& SetAggregatableValues(
      std::vector<attribution_reporting::AggregatableValues>);

  TriggerBuilder& SetAggregatableDedupKey(
      std::optional<uint64_t> aggregatable_dedup_key);

  TriggerBuilder& SetAggregatableDedupKeyFilterPair(
      attribution_reporting::FilterPair filter_pair);

  TriggerBuilder& SetIsWithinFencedFrame(bool is_within_fenced_frame);

  TriggerBuilder& SetDebugReporting(bool debug_reporting);

  TriggerBuilder& SetAggregationCoordinatorOrigin(
      attribution_reporting::SuitableOrigin);

  TriggerBuilder& SetSourceRegistrationTimeConfig(
      attribution_reporting::mojom::SourceRegistrationTimeConfig);

  TriggerBuilder& SetFilterPair(attribution_reporting::FilterPair filter_pair);

  TriggerBuilder& SetTriggerContextId(std::string trigger_context_id);

  TriggerBuilder& SetAggregatableDebugReportingConfig(
      attribution_reporting::AggregatableDebugReportingConfig);

  TriggerBuilder& SetAttributionScopes(
      attribution_reporting::AttributionScopesSet);

  TriggerBuilder& SetAggregatableFilteringIdMaxBytes(
      attribution_reporting::AggregatableFilteringIdsMaxBytes);

  AttributionTrigger Build(bool generate_event_trigger_data = true) const;

 private:
  uint64_t trigger_data_ = 111;
  attribution_reporting::SuitableOrigin destination_origin_;
  attribution_reporting::SuitableOrigin reporting_origin_;
  attribution_reporting::FilterPair filter_pair_;
  int64_t priority_ = 0;
  std::optional<uint64_t> dedup_key_;
  std::optional<uint64_t> debug_key_;
  std::vector<attribution_reporting::AggregatableTriggerData>
      aggregatable_trigger_data_;
  std::vector<attribution_reporting::AggregatableValues> aggregatable_values_;
  std::optional<uint64_t> aggregatable_dedup_key_;
  attribution_reporting::FilterPair aggregatable_dedup_key_filter_pair_;
  bool is_within_fenced_frame_ = false;
  bool debug_reporting_ = false;
  std::optional<attribution_reporting::SuitableOrigin>
      aggregation_coordinator_origin_;
  attribution_reporting::mojom::SourceRegistrationTimeConfig
      source_registration_time_config_ =
          attribution_reporting::mojom::SourceRegistrationTimeConfig::kInclude;
  std::optional<std::string> trigger_context_id_;
  attribution_reporting::AggregatableFilteringIdsMaxBytes
      aggregatable_filtering_id_max_bytes_;
  attribution_reporting::AggregatableDebugReportingConfig
      aggregatable_debug_reporting_config_;
  attribution_reporting::AttributionScopesSet attribution_scopes_;
};

// Helper class to construct an `AttributionInfo` for tests using default data.
class AttributionInfoBuilder {
 public:
  explicit AttributionInfoBuilder(
      // For most tests, the context origin is irrelevant.
      attribution_reporting::SuitableOrigin context_origin =
          *attribution_reporting::SuitableOrigin::Deserialize(
              "https://conversion.test"));
  ~AttributionInfoBuilder();

  AttributionInfoBuilder& SetTime(base::Time time);

  AttributionInfoBuilder& SetDebugKey(std::optional<uint64_t> debug_key);

  AttributionInfo Build() const;

 private:
  base::Time time_;
  std::optional<uint64_t> debug_key_;
  attribution_reporting::SuitableOrigin context_origin_;
};

// Helper class to construct an `AttributionReport` for tests using default
// data.
class ReportBuilder {
 public:
  explicit ReportBuilder(AttributionInfo attribution_info, StoredSource);
  ~ReportBuilder();

  ReportBuilder& SetTriggerData(uint64_t trigger_data);

  ReportBuilder& SetReportTime(base::Time time);

  ReportBuilder& SetPriority(int64_t priority);

  ReportBuilder& SetExternalReportId(base::Uuid external_report_id);

  ReportBuilder& SetReportId(AttributionReport::Id id);

  ReportBuilder& SetAggregatableHistogramContributions(
      std::vector<blink::mojom::AggregatableReportHistogramContribution>
          contributions);

  ReportBuilder& SetAggregationCoordinatorOrigin(
      attribution_reporting::SuitableOrigin);

  ReportBuilder& SetSourceRegistrationTimeConfig(
      attribution_reporting::mojom::SourceRegistrationTimeConfig);

  ReportBuilder& SetAggregatableFilteringIdsMaxBytes(
      attribution_reporting::AggregatableFilteringIdsMaxBytes);

  ReportBuilder& SetTriggerContextId(std::string trigger_context_id);

  AttributionReport Build() const;

  AttributionReport BuildAggregatableAttribution() const;

  AttributionReport BuildNullAggregatable() const;

 private:
  AttributionInfo attribution_info_;
  StoredSource source_;
  uint64_t trigger_data_ = 0;
  base::Time report_time_;
  int64_t priority_ = 0;
  base::Uuid external_report_id_;
  AttributionReport::Id report_id_{0};
  attribution_reporting::AggregatableFilteringIdsMaxBytes
      aggregatable_filtering_ids_max_bytes_;
  std::vector<blink::mojom::AggregatableReportHistogramContribution>
      contributions_;
  std::optional<attribution_reporting::SuitableOrigin>
      aggregation_coordinator_origin_;

  attribution_reporting::mojom::SourceRegistrationTimeConfig
      source_registration_time_config_ =
          attribution_reporting::mojom::SourceRegistrationTimeConfig::kInclude;
  std::optional<std::string> trigger_context_id_;
};

bool operator==(const StoredSource&, const StoredSource&);

bool operator==(const AttributionReport::CommonAggregatableData&,
                const AttributionReport::CommonAggregatableData&);

bool operator==(const AttributionReport::AggregatableAttributionData&,
                const AttributionReport::AggregatableAttributionData&);

bool operator==(const AttributionReport::NullAggregatableData&,
                const AttributionReport::NullAggregatableData&);

bool operator==(const AttributionReport&, const AttributionReport&);

std::ostream& operator<<(std::ostream& out, RateLimitResult result);

std::ostream& operator<<(std::ostream& out,
                         const AttributionTrigger& conversion);

std::ostream& operator<<(std::ostream& out, const CommonSourceInfo& source);

std::ostream& operator<<(std::ostream& out,
                         const AttributionInfo& attribution_info);

std::ostream& operator<<(std::ostream& out, const StorableSource& source);

std::ostream& operator<<(std::ostream& out, const StoredSource& source);

std::ostream& operator<<(
    std::ostream& out,
    const blink::mojom::AggregatableReportHistogramContribution& contribution);

std::ostream& operator<<(std::ostream& out,
                         const AttributionReport::EventLevelData& data);

std::ostream& operator<<(std::ostream& out,
                         const AttributionReport::CommonAggregatableData&);

std::ostream& operator<<(
    std::ostream& out,
    const AttributionReport::AggregatableAttributionData& data);

std::ostream& operator<<(std::ostream& out,
                         const AttributionReport::NullAggregatableData&);

std::ostream& operator<<(std::ostream& out, const AttributionReport& report);

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

// Source matchers

MATCHER_P(SourceRegistrationIs, matcher, "") {
  return ExplainMatchResult(matcher, arg.registration(), result_listener);
}

MATCHER_P(RegistrationAttributionScopesDataIs, matcher, "") {
  return ExplainMatchResult(matcher, arg.registration().attribution_scopes_data,
                            result_listener);
}

MATCHER_P(RegistrationSourceEventIdIs, matcher, "") {
  return ExplainMatchResult(matcher, arg.registration().source_event_id,
                            result_listener);
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

MATCHER_P(SourceDebugCookieSetIs, matcher, "") {
  return ExplainMatchResult(matcher, arg.common_info().debug_cookie_set(),
                            result_listener);
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

MATCHER_P(RemainingAggregatableAttributionBudgetIs, matcher, "") {
  return ExplainMatchResult(matcher,
                            arg.remaining_aggregatable_attribution_budget(),
                            result_listener);
}

MATCHER_P(AttributionScopesDataIs, matcher, "") {
  return ExplainMatchResult(matcher, arg.attribution_scopes_data(),
                            result_listener);
}

MATCHER_P(AttributionScopesSetIs, matcher, "") {
  return ExplainMatchResult(matcher, arg->attribution_scopes_set(),
                            result_listener);
}

MATCHER_P(RandomizedResponseRateIs, matcher, "") {
  return ExplainMatchResult(matcher, arg.randomized_response_rate(),
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

MATCHER_P(ReportTimeIs, matcher, "") {
  return ExplainMatchResult(matcher, arg.report_time(), result_listener);
}

MATCHER_P(InitialReportTimeIs, matcher, "") {
  return ExplainMatchResult(matcher, arg.initial_report_time(),
                            result_listener);
}

MATCHER_P(FailedSendAttemptsIs, matcher, "") {
  return ExplainMatchResult(matcher, arg.failed_send_attempts(),
                            result_listener);
}

MATCHER_P(TriggerDebugKeyIs, matcher, "") {
  return ExplainMatchResult(matcher, arg.attribution_info().debug_key,
                            result_listener);
}

MATCHER_P(ReportSourceDebugKeyIs, matcher, "") {
  return ExplainMatchResult(matcher, arg.GetSourceDebugKey(), result_listener);
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

MATCHER_P(ReportURLIs, matcher, "") {
  return ExplainMatchResult(matcher, arg.ReportURL(), result_listener);
}

MATCHER_P(ReportOriginIs, matcher, "") {
  return ExplainMatchResult(matcher, arg.reporting_origin(), result_listener);
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

MATCHER_P(NullAggregatableDataIs, matcher, "") {
  return ExplainMatchResult(
      ::testing::VariantWith<AttributionReport::NullAggregatableData>(matcher),
      arg.data(), result_listener);
}

MATCHER_P(AggregatableHistogramContributionsAre, matcher, "") {
  return ExplainMatchResult(matcher, arg.contributions, result_listener);
}

MATCHER_P(AggregationCoordinatorOriginIs, matcher, "") {
  return ExplainMatchResult(
      matcher, arg.common_data.aggregation_coordinator_origin, result_listener);
}

MATCHER_P(SourceRegistrationTimeConfigIs, matcher, "") {
  return ExplainMatchResult(matcher,
                            arg.common_data.aggregatable_trigger_config
                                .source_registration_time_config(),
                            result_listener);
}

MATCHER_P(TriggerContextIdIs, matcher, "") {
  return ExplainMatchResult(
      matcher, arg.common_data.aggregatable_trigger_config.trigger_context_id(),
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

std::vector<blink::mojom::AggregatableReportHistogramContribution>
DefaultAggregatableHistogramContributions(
    const std::vector<int32_t>& histogram_values = {1});

struct OsRegistration;

bool operator==(const OsRegistration&, const OsRegistration&);

std::ostream& operator<<(std::ostream&, const OsRegistration&);

void ExpectValidAttributionReportingEligibleHeaderForEventBeacon(
    const std::string& header);
void ExpectValidAttributionReportingEligibleHeaderForImg(
    const std::string& header);
void ExpectValidAttributionReportingEligibleHeaderForNavigation(
    const std::string& header);
void ExpectEmptyAttributionReportingEligibleHeader(const std::string& header);

void ExpectValidAttributionReportingSupportHeader(const std::string& header,
                                                  bool web_expected,
                                                  bool os_expected);

}  // namespace content

#endif  // CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_TEST_UTILS_H_
