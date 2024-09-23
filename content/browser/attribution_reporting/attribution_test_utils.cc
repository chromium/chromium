// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/attribution_test_utils.h"

#include <stdint.h>

#include <algorithm>
#include <optional>
#include <tuple>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/functional/overloaded.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "components/attribution_reporting/aggregatable_debug_reporting_config.h"
#include "components/attribution_reporting/aggregatable_dedup_key.h"
#include "components/attribution_reporting/aggregatable_filtering_id_max_bytes.h"
#include "components/attribution_reporting/aggregatable_trigger_config.h"
#include "components/attribution_reporting/aggregatable_trigger_data.h"
#include "components/attribution_reporting/aggregatable_values.h"
#include "components/attribution_reporting/attribution_scopes_data.h"
#include "components/attribution_reporting/constants.h"
#include "components/attribution_reporting/destination_set.h"
#include "components/attribution_reporting/event_report_windows.h"
#include "components/attribution_reporting/event_trigger_data.h"
#include "components/attribution_reporting/filters.h"
#include "components/attribution_reporting/max_event_level_reports.h"
#include "components/attribution_reporting/os_registration.h"
#include "components/attribution_reporting/source_type.mojom.h"
#include "components/attribution_reporting/suitable_origin.h"
#include "components/attribution_reporting/test_utils.h"
#include "components/attribution_reporting/trigger_config.h"
#include "components/attribution_reporting/trigger_data_matching.mojom-forward.h"
#include "components/attribution_reporting/trigger_registration.h"
#include "content/browser/attribution_reporting/attribution_observer.h"
#include "content/browser/attribution_reporting/attribution_reporting.mojom.h"
#include "content/browser/attribution_reporting/attribution_trigger.h"
#include "content/browser/attribution_reporting/attribution_utils.h"
#include "content/browser/attribution_reporting/os_registration.h"
#include "content/browser/attribution_reporting/rate_limit_result.h"
#include "content/browser/attribution_reporting/stored_source.h"
#include "content/public/browser/attribution_data_model.h"
#include "net/base/net_errors.h"
#include "net/base/schemeful_site.h"
#include "net/http/structured_headers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/numeric/int128.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

namespace {

using ::attribution_reporting::FilterPair;
using ::attribution_reporting::OsRegistrationItem;
using ::attribution_reporting::SuitableOrigin;
using ::attribution_reporting::mojom::SourceType;

const char kDefaultSourceOrigin[] = "https://impression.test/";
const char kDefaultDestinationOrigin[] = "https://sub.conversion.test/";
const char kDefaultReportOrigin[] = "https://report.test/";

}  // namespace

base::Uuid DefaultExternalReportID() {
  return base::Uuid::ParseLowercase("21abd97f-73e8-4b88-9389-a9fee6abda5e");
}

// Builds an impression with default values. This is done as a builder because
// all values needed to be provided at construction time.
SourceBuilder::SourceBuilder(base::Time time)
    : source_time_(time),
      source_origin_(*SuitableOrigin::Deserialize(kDefaultSourceOrigin)),
      registration_(*attribution_reporting::DestinationSet::Create(
          {net::SchemefulSite::Deserialize(kDefaultDestinationOrigin)})),
      reporting_origin_(*SuitableOrigin::Deserialize(kDefaultReportOrigin)) {
  registration_.source_event_id = 123;
  registration_.trigger_specs = attribution_reporting::TriggerSpecs(
      source_type_, attribution_reporting::EventReportWindows(),
      attribution_reporting::MaxEventLevelReports::Max());
}

SourceBuilder::~SourceBuilder() = default;

SourceBuilder::SourceBuilder(const SourceBuilder&) = default;

SourceBuilder::SourceBuilder(SourceBuilder&&) = default;

SourceBuilder& SourceBuilder::operator=(const SourceBuilder&) = default;

SourceBuilder& SourceBuilder::operator=(SourceBuilder&&) = default;

SourceBuilder& SourceBuilder::SetExpiry(base::TimeDelta delta) {
  registration_.expiry = delta;
  return *this;
}

SourceBuilder& SourceBuilder::SetAggregatableReportWindow(
    base::TimeDelta delta) {
  registration_.aggregatable_report_window = delta;
  return *this;
}

SourceBuilder& SourceBuilder::SetSourceEventId(uint64_t source_event_id) {
  registration_.source_event_id = source_event_id;
  return *this;
}

SourceBuilder& SourceBuilder::SetSourceOrigin(SuitableOrigin origin) {
  source_origin_ = std::move(origin);
  return *this;
}

SourceBuilder& SourceBuilder::SetDestinationSites(
    base::flat_set<net::SchemefulSite> sites) {
  registration_.destination_set =
      *attribution_reporting::DestinationSet::Create(std::move(sites));
  return *this;
}

SourceBuilder& SourceBuilder::SetReportingOrigin(SuitableOrigin origin) {
  reporting_origin_ = std::move(origin);
  return *this;
}

SourceBuilder& SourceBuilder::SetSourceType(SourceType source_type) {
  source_type_ = source_type;
  registration_.trigger_specs = attribution_reporting::TriggerSpecs(
      source_type_, attribution_reporting::EventReportWindows(),
      attribution_reporting::MaxEventLevelReports(source_type));
  return *this;
}

SourceBuilder& SourceBuilder::SetPriority(int64_t priority) {
  registration_.priority = priority;
  return *this;
}

SourceBuilder& SourceBuilder::SetFilterData(
    attribution_reporting::FilterData filter_data) {
  registration_.filter_data = std::move(filter_data);
  return *this;
}

SourceBuilder& SourceBuilder::SetDebugKey(std::optional<uint64_t> debug_key) {
  registration_.debug_key = debug_key;
  return *this;
}

SourceBuilder& SourceBuilder::SetAttributionLogic(
    StoredSource::AttributionLogic attribution_logic) {
  attribution_logic_ = attribution_logic;
  return *this;
}

SourceBuilder& SourceBuilder::SetActiveState(
    StoredSource::ActiveState active_state) {
  active_state_ = active_state;
  return *this;
}

SourceBuilder& SourceBuilder::SetSourceId(StoredSource::Id source_id) {
  source_id_ = source_id;
  return *this;
}

SourceBuilder& SourceBuilder::SetDedupKeys(std::vector<uint64_t> dedup_keys) {
  dedup_keys_ = std::move(dedup_keys);
  return *this;
}

SourceBuilder& SourceBuilder::SetAggregationKeys(
    attribution_reporting::AggregationKeys aggregation_keys) {
  registration_.aggregation_keys = std::move(aggregation_keys);
  return *this;
}

SourceBuilder& SourceBuilder::SetRemainingAggregatableAttributionBudget(
    int remaining_aggregatable_attribution_budget) {
  remaining_aggregatable_attribution_budget_ =
      remaining_aggregatable_attribution_budget;
  return *this;
}

SourceBuilder& SourceBuilder::SetRemainingAggregatableDebugBudget(
    int remaining_aggregatable_debug_budget) {
  remaining_aggregatable_debug_budget_ = remaining_aggregatable_debug_budget;
  return *this;
}

SourceBuilder& SourceBuilder::SetRandomizedResponseRate(
    double randomized_response_rate) {
  randomized_response_rate_ = randomized_response_rate;
  return *this;
}

SourceBuilder& SourceBuilder::SetAggregatableDedupKeys(
    std::vector<uint64_t> dedup_keys) {
  aggregatable_dedup_keys_ = std::move(dedup_keys);
  return *this;
}

SourceBuilder& SourceBuilder::SetIsWithinFencedFrame(
    bool is_within_fenced_frame) {
  is_within_fenced_frame_ = is_within_fenced_frame;
  return *this;
}

SourceBuilder& SourceBuilder::SetDebugReporting(bool debug_reporting) {
  registration_.debug_reporting = debug_reporting;
  return *this;
}

SourceBuilder& SourceBuilder::SetTriggerSpecs(
    attribution_reporting::TriggerSpecs trigger_specs) {
  registration_.trigger_specs = std::move(trigger_specs);
  return *this;
}

SourceBuilder& SourceBuilder::SetMaxEventLevelReports(
    int max_event_level_reports) {
  registration_.trigger_specs.SetMaxEventLevelReportsForTesting(
      attribution_reporting::MaxEventLevelReports(max_event_level_reports));
  return *this;
}

SourceBuilder& SourceBuilder::SetTriggerDataMatching(
    attribution_reporting::mojom::TriggerDataMatching trigger_data_matching) {
  registration_.trigger_data_matching = trigger_data_matching;
  return *this;
}

SourceBuilder& SourceBuilder::SetDebugCookieSet(bool debug_cookie_set) {
  debug_cookie_set_ = debug_cookie_set;
  return *this;
}

SourceBuilder& SourceBuilder::SetAggregatableDebugReportingConfig(
    attribution_reporting::SourceAggregatableDebugReportingConfig
        aggregatable_debug_reporting_config) {
  registration_.aggregatable_debug_reporting_config =
      std::move(aggregatable_debug_reporting_config);
  return *this;
}

SourceBuilder& SourceBuilder::SetDestinationLimitPriority(int64_t priority) {
  registration_.destination_limit_priority = priority;
  return *this;
}

SourceBuilder& SourceBuilder::SetAttributionScopesData(
    attribution_reporting::AttributionScopesData attribution_scopes) {
  registration_.attribution_scopes_data = std::move(attribution_scopes);
  return *this;
}

StorableSource SourceBuilder::Build() const {
  StorableSource source(reporting_origin_, registration_, source_origin_,
                        source_type_, is_within_fenced_frame_);
  source.set_debug_cookie_set(debug_cookie_set_);
  return source;
}

StoredSource SourceBuilder::BuildStored() const {
  base::Time expiry_time = source_time_ + registration_.expiry;
  StoredSource source = *StoredSource::Create(
      CommonSourceInfo(source_origin_, reporting_origin_, source_type_,
                       debug_cookie_set_),
      registration_.source_event_id, registration_.destination_set,
      source_time_, expiry_time, registration_.trigger_specs,
      source_time_ + registration_.aggregatable_report_window,
      registration_.priority, registration_.filter_data,
      registration_.debug_key, registration_.aggregation_keys,
      attribution_logic_, active_state_, source_id_,
      remaining_aggregatable_attribution_budget_, randomized_response_rate_,
      registration_.trigger_data_matching, registration_.event_level_epsilon,
      registration_.aggregatable_debug_reporting_config.config().key_piece,
      remaining_aggregatable_debug_budget_,
      registration_.attribution_scopes_data);
  source.dedup_keys() = dedup_keys_;
  source.aggregatable_dedup_keys() = aggregatable_dedup_keys_;
  return source;
}

AttributionTrigger DefaultTrigger() {
  return TriggerBuilder().Build();
}

TriggerBuilder::TriggerBuilder()
    : destination_origin_(
          *SuitableOrigin::Deserialize(kDefaultDestinationOrigin)),
      reporting_origin_(*SuitableOrigin::Deserialize(kDefaultReportOrigin)) {}

TriggerBuilder::~TriggerBuilder() = default;

TriggerBuilder::TriggerBuilder(const TriggerBuilder&) = default;

TriggerBuilder::TriggerBuilder(TriggerBuilder&&) = default;

TriggerBuilder& TriggerBuilder::operator=(const TriggerBuilder&) = default;

TriggerBuilder& TriggerBuilder::operator=(TriggerBuilder&&) = default;

TriggerBuilder& TriggerBuilder::SetTriggerData(uint64_t trigger_data) {
  trigger_data_ = trigger_data;
  return *this;
}

TriggerBuilder& TriggerBuilder::SetDestinationOrigin(SuitableOrigin origin) {
  destination_origin_ = std::move(origin);
  return *this;
}

TriggerBuilder& TriggerBuilder::SetReportingOrigin(SuitableOrigin origin) {
  reporting_origin_ = std::move(origin);
  return *this;
}

TriggerBuilder& TriggerBuilder::SetPriority(int64_t priority) {
  priority_ = priority;
  return *this;
}

TriggerBuilder& TriggerBuilder::SetDedupKey(std::optional<uint64_t> dedup_key) {
  dedup_key_ = dedup_key;
  return *this;
}

TriggerBuilder& TriggerBuilder::SetDebugKey(std::optional<uint64_t> debug_key) {
  debug_key_ = debug_key;
  return *this;
}

TriggerBuilder& TriggerBuilder::SetAggregatableTriggerData(
    std::vector<attribution_reporting::AggregatableTriggerData>
        aggregatable_trigger_data) {
  aggregatable_trigger_data_ = std::move(aggregatable_trigger_data);
  return *this;
}

TriggerBuilder& TriggerBuilder::SetAggregatableValues(
    std::vector<attribution_reporting::AggregatableValues>
        aggregatable_values) {
  aggregatable_values_ = std::move(aggregatable_values);
  return *this;
}

TriggerBuilder& TriggerBuilder::SetAggregatableDedupKey(
    std::optional<uint64_t> aggregatable_dedup_key) {
  aggregatable_dedup_key_ = aggregatable_dedup_key;
  return *this;
}

TriggerBuilder& TriggerBuilder::SetIsWithinFencedFrame(
    bool is_within_fenced_frame) {
  is_within_fenced_frame_ = is_within_fenced_frame;
  return *this;
}

TriggerBuilder& TriggerBuilder::SetDebugReporting(bool debug_reporting) {
  debug_reporting_ = debug_reporting;
  return *this;
}

TriggerBuilder& TriggerBuilder::SetAggregationCoordinatorOrigin(
    SuitableOrigin aggregation_coordinator_origin) {
  aggregation_coordinator_origin_ = std::move(aggregation_coordinator_origin);
  return *this;
}

TriggerBuilder& TriggerBuilder::SetSourceRegistrationTimeConfig(
    attribution_reporting::mojom::SourceRegistrationTimeConfig
        source_registration_time_config) {
  source_registration_time_config_ = source_registration_time_config;
  return *this;
}

TriggerBuilder& TriggerBuilder::SetFilterPair(
    attribution_reporting::FilterPair filter_pair) {
  filter_pair_ = std::move(filter_pair);
  return *this;
}

TriggerBuilder& TriggerBuilder::SetAggregatableDedupKeyFilterPair(
    attribution_reporting::FilterPair filter_pair) {
  aggregatable_dedup_key_filter_pair_ = std::move(filter_pair);
  return *this;
}

TriggerBuilder& TriggerBuilder::SetTriggerContextId(
    std::string trigger_context_id) {
  trigger_context_id_ = std::move(trigger_context_id);
  return *this;
}

TriggerBuilder& TriggerBuilder::SetAggregatableDebugReportingConfig(
    attribution_reporting::AggregatableDebugReportingConfig
        aggregatable_trigger_config) {
  aggregatable_debug_reporting_config_ = std::move(aggregatable_trigger_config);
  return *this;
}

TriggerBuilder& TriggerBuilder::SetAggregatableFilteringIdMaxBytes(
    attribution_reporting::AggregatableFilteringIdsMaxBytes max_bytes) {
  aggregatable_filtering_id_max_bytes_ = max_bytes;
  return *this;
}

TriggerBuilder& TriggerBuilder::SetAttributionScopes(
    attribution_reporting::AttributionScopesSet attribution_scopes) {
  attribution_scopes_ = std::move(attribution_scopes);
  return *this;
}

AttributionTrigger TriggerBuilder::Build(
    bool generate_event_trigger_data) const {
  attribution_reporting::TriggerRegistration reg;
  reg.filters = filter_pair_;
  reg.debug_key = debug_key_;
  reg.aggregatable_dedup_keys.emplace_back(
      /*dedup_key=*/aggregatable_dedup_key_,
      aggregatable_dedup_key_filter_pair_);

  if (generate_event_trigger_data) {
    reg.event_triggers.emplace_back(trigger_data_, priority_, dedup_key_,
                                    FilterPair());
  }

  reg.aggregatable_trigger_data = aggregatable_trigger_data_;
  reg.aggregatable_values = aggregatable_values_;
  reg.debug_reporting = debug_reporting_;
  reg.aggregation_coordinator_origin = aggregation_coordinator_origin_;
  reg.aggregatable_trigger_config =
      *attribution_reporting::AggregatableTriggerConfig::Create(
          source_registration_time_config_, trigger_context_id_,
          aggregatable_filtering_id_max_bytes_);
  reg.aggregatable_debug_reporting_config =
      aggregatable_debug_reporting_config_;
  reg.attribution_scopes = attribution_scopes_;

  return AttributionTrigger(reporting_origin_, std::move(reg),
                            destination_origin_, is_within_fenced_frame_);
}

AttributionInfoBuilder::AttributionInfoBuilder(SuitableOrigin context_origin)
    : context_origin_(std::move(context_origin)) {}

AttributionInfoBuilder::~AttributionInfoBuilder() = default;

AttributionInfoBuilder& AttributionInfoBuilder::SetTime(base::Time time) {
  time_ = time;
  return *this;
}

AttributionInfoBuilder& AttributionInfoBuilder::SetDebugKey(
    std::optional<uint64_t> debug_key) {
  debug_key_ = debug_key;
  return *this;
}

AttributionInfo AttributionInfoBuilder::Build() const {
  return AttributionInfo(time_, debug_key_, context_origin_);
}

ReportBuilder::ReportBuilder(AttributionInfo attribution_info,
                             StoredSource source)
    : attribution_info_(std::move(attribution_info)),
      source_(std::move(source)),
      external_report_id_(DefaultExternalReportID()) {}

ReportBuilder::~ReportBuilder() = default;

ReportBuilder& ReportBuilder::SetTriggerData(uint64_t trigger_data) {
  trigger_data_ = trigger_data;
  return *this;
}

ReportBuilder& ReportBuilder::SetReportTime(base::Time time) {
  report_time_ = time;
  return *this;
}

ReportBuilder& ReportBuilder::SetPriority(int64_t priority) {
  priority_ = priority;
  return *this;
}

ReportBuilder& ReportBuilder::SetExternalReportId(
    base::Uuid external_report_id) {
  external_report_id_ = std::move(external_report_id);
  return *this;
}

ReportBuilder& ReportBuilder::SetReportId(AttributionReport::Id id) {
  report_id_ = id;
  return *this;
}

ReportBuilder& ReportBuilder::SetAggregatableHistogramContributions(
    std::vector<blink::mojom::AggregatableReportHistogramContribution>
        contributions) {
  DCHECK(!contributions.empty());
  contributions_ = std::move(contributions);
  return *this;
}

ReportBuilder& ReportBuilder::SetAggregationCoordinatorOrigin(
    SuitableOrigin aggregation_coordinator_origin) {
  aggregation_coordinator_origin_ = std::move(aggregation_coordinator_origin);
  return *this;
}

ReportBuilder& ReportBuilder::SetSourceRegistrationTimeConfig(
    attribution_reporting::mojom::SourceRegistrationTimeConfig
        source_registration_time_config) {
  source_registration_time_config_ = source_registration_time_config;
  return *this;
}

ReportBuilder& ReportBuilder::SetAggregatableFilteringIdsMaxBytes(
    attribution_reporting::AggregatableFilteringIdsMaxBytes max_bytes) {
  aggregatable_filtering_ids_max_bytes_ = max_bytes;
  return *this;
}

ReportBuilder& ReportBuilder::SetTriggerContextId(
    std::string trigger_context_id) {
  trigger_context_id_ = std::move(trigger_context_id);
  return *this;
}

AttributionReport ReportBuilder::Build() const {
  return AttributionReport(
      attribution_info_, report_id_, report_time_,
      /*initial_report_time=*/report_time_, external_report_id_,
      /*failed_send_attempts=*/0,
      AttributionReport::EventLevelData(trigger_data_, priority_, source_),
      source_.common_info().reporting_origin());
}

AttributionReport ReportBuilder::BuildAggregatableAttribution() const {
  return AttributionReport(
      attribution_info_, report_id_, report_time_,
      /*initial_report_time=*/report_time_, external_report_id_,
      /*failed_send_attempts=*/0,
      AttributionReport::AggregatableAttributionData(
          AttributionReport::CommonAggregatableData(
              aggregation_coordinator_origin_,
              *attribution_reporting::AggregatableTriggerConfig::Create(
                  source_registration_time_config_, trigger_context_id_,
                  aggregatable_filtering_ids_max_bytes_)),
          contributions_, source_),
      source_.common_info().reporting_origin());
}

AttributionReport ReportBuilder::BuildNullAggregatable() const {
  return AttributionReport(
      attribution_info_, report_id_, report_time_,
      /*initial_report_time=*/report_time_, external_report_id_,
      /*failed_send_attempts=*/0,
      AttributionReport::NullAggregatableData(
          AttributionReport::CommonAggregatableData(
              aggregation_coordinator_origin_,
              *attribution_reporting::AggregatableTriggerConfig::Create(
                  source_registration_time_config_, trigger_context_id_,
                  attribution_reporting::AggregatableFilteringIdsMaxBytes())),
          source_.source_time()),
      source_.common_info().reporting_origin());
}

// Does not compare source IDs, as they are set by the underlying sqlite DB and
// should not be tested.
bool operator==(const StoredSource& a, const StoredSource& b) {
  const auto tie = [](const StoredSource& source) {
    return std::make_tuple(
        source.common_info(), source.source_event_id(),
        source.destination_sites(), source.source_time(), source.expiry_time(),
        source.trigger_specs(), source.aggregatable_report_window_time(),
        source.priority(), source.filter_data(), source.debug_key(),
        source.aggregation_keys(), source.attribution_logic(),
        source.active_state(), source.dedup_keys(),
        source.remaining_aggregatable_attribution_budget(),
        source.aggregatable_dedup_keys(), source.randomized_response_rate(),
        source.trigger_data_matching(), source.event_level_epsilon(),
        source.aggregatable_debug_key_piece(),
        source.remaining_aggregatable_debug_budget(),
        source.attribution_scopes_data());
  };
  return tie(a) == tie(b);
}

// Does not compare the assembled report as it is returned by the
// aggregation service from all the other data.
bool operator==(const AttributionReport::CommonAggregatableData& a,
                const AttributionReport::CommonAggregatableData& b) {
  const auto tie = [](const AttributionReport::CommonAggregatableData& data) {
    return std::make_tuple(data.aggregation_coordinator_origin,
                           data.aggregatable_trigger_config);
  };
  return tie(a) == tie(b);
}

bool operator==(const AttributionReport::AggregatableAttributionData& a,
                const AttributionReport::AggregatableAttributionData& b) {
  const auto tie =
      [](const AttributionReport::AggregatableAttributionData& data) {
        return std::make_tuple(data.common_data, data.contributions,
                               data.source_time, data.source_debug_key,
                               data.source_origin);
      };
  return tie(a) == tie(b);
}

bool operator==(const AttributionReport::NullAggregatableData& a,
                const AttributionReport::NullAggregatableData& b) {
  const auto tie = [](const AttributionReport::NullAggregatableData& data) {
    return std::make_tuple(data.common_data, data.fake_source_time);
  };
  return tie(a) == tie(b);
}

// Does not compare source or report IDs, as they are set by the underlying
// sqlite DB and should not be tested.
bool operator==(const AttributionReport& a, const AttributionReport& b) {
  const auto tie = [](const AttributionReport& report) {
    return std::make_tuple(report.attribution_info(), report.report_time(),
                           report.initial_report_time(),
                           report.external_report_id(),
                           report.failed_send_attempts(), report.data(),
                           report.reporting_origin());
  };
  return tie(a) == tie(b);
}

std::ostream& operator<<(std::ostream& out, RateLimitResult result) {
  switch (result) {
    case RateLimitResult::kAllowed:
      return out << "kAllowed";
    case RateLimitResult::kNotAllowed:
      return out << "kNotAllowed";
    case RateLimitResult::kError:
      return out << "kError";
  }
}

std::ostream& operator<<(std::ostream& out,
                         StoredSource::AttributionLogic attribution_logic) {
  switch (attribution_logic) {
    case StoredSource::AttributionLogic::kNever:
      return out << "kNever";
    case StoredSource::AttributionLogic::kTruthfully:
      return out << "kTruthfully";
    case StoredSource::AttributionLogic::kFalsely:
      return out << "kFalsely";
  }
}

std::ostream& operator<<(std::ostream& out,
                         StoredSource::ActiveState active_state) {
  switch (active_state) {
    case StoredSource::ActiveState::kActive:
      return out << "kActive";
    case StoredSource::ActiveState::kInactive:
      return out << "kInactive";
    case StoredSource::ActiveState::kReachedEventLevelAttributionLimit:
      return out << "kReachedEventLevelAttributionLimit";
  }
}

std::ostream& operator<<(std::ostream& out,
                         const AttributionTrigger& conversion) {
  return out << "{registration=" << conversion.registration()
             << ",destination_origin=" << conversion.destination_origin()
             << ",is_within_fenced_frame="
             << conversion.is_within_fenced_frame() << "}";
}

std::ostream& operator<<(std::ostream& out, const CommonSourceInfo& source) {
  return out << "{source_origin=" << source.source_origin()
             << "reporting_origin=" << source.reporting_origin()
             << ",source_type=" << source.source_type()
             << ",debug_cookie_set=" << source.debug_cookie_set() << "}";
}

std::ostream& operator<<(std::ostream& out,
                         const AttributionInfo& attribution_info) {
  return out << "{time=" << attribution_info.time << ",debug_key="
             << (attribution_info.debug_key
                     ? base::NumberToString(*attribution_info.debug_key)
                     : "null")
             << ",context_origin=" << attribution_info.context_origin << "}";
}

std::ostream& operator<<(std::ostream& out, const StorableSource& source) {
  return out << "{registration=" << source.registration().ToJson()
             << ",common_info=" << source.common_info()
             << ",is_within_fenced_frame=" << source.is_within_fenced_frame()
             << "}";
}

std::ostream& operator<<(std::ostream& out, const StoredSource& source) {
  out << "{common_info=" << source.common_info()
      << ",source_event_id=" << source.source_event_id()
      << ",destination_sites=" << source.destination_sites()
      << ",source_time=" << source.source_time()
      << ",expiry_time=" << source.expiry_time()
      << ",trigger_specs=" << source.trigger_specs()
      << ",aggregatable_report_window_time="
      << source.aggregatable_report_window_time()
      << ",priority=" << source.priority()
      << ",filter_data=" << source.filter_data() << ",debug_key="
      << (source.debug_key() ? base::NumberToString(*source.debug_key())
                             : "null")
      << ",aggregation_keys=" << source.aggregation_keys()
      << ",attribution_logic=" << source.attribution_logic()
      << ",active_state=" << source.active_state()
      << ",source_id=" << *source.source_id()
      << ",remaining_aggregatable_attribution_budget="
      << source.remaining_aggregatable_attribution_budget()
      << ",randomized_response_rate=" << source.randomized_response_rate()
      << ",event_level_epsilon=" << source.event_level_epsilon()
      << ",trigger_data_matching=" << source.trigger_data_matching()
      << ",aggregatable_debug_key_piece="
      << source.aggregatable_debug_key_piece()
      << ",remaining_aggregatable_debug_budget="
      << source.remaining_aggregatable_debug_budget()
      << ",attribution_scopes_data="
      << (source.attribution_scopes_data().has_value()
              ? SerializeAttributionJson(
                    source.attribution_scopes_data()->ToJson())
              : "null")
      << ",dedup_keys=[";

  const char* separator = "";
  for (int64_t dedup_key : source.dedup_keys()) {
    out << separator << dedup_key;
    separator = ", ";
  }

  out << "],aggregatable_dedup_keys=[";

  separator = "";
  for (int64_t dedup_key : source.aggregatable_dedup_keys()) {
    out << separator << dedup_key;
    separator = ",";
  }

  return out << "]}";
}

std::ostream& operator<<(
    std::ostream& out,
    const blink::mojom::AggregatableReportHistogramContribution& contribution) {
  return out << "{bucket=" << contribution.bucket
             << ",value=" << contribution.value << "}";
}

std::ostream& operator<<(std::ostream& out,
                         const AttributionReport::EventLevelData& data) {
  out << "{trigger_data=" << data.trigger_data << ",priority=" << data.priority
      << ",source_origin=" << data.source_origin
      << ",destinations=" << data.destinations
      << ",source_event_id=" << data.source_event_id
      << ",source_type=" << data.source_type << ",source_debug_key=";

  if (data.source_debug_key.has_value()) {
    out << *data.source_debug_key;
  } else {
    out << "null";
  }

  return out << ",randomized_response_rate=" << data.randomized_response_rate
             << ",attributed_truthfully=" << data.attributed_truthfully << "}";
}

std::ostream& operator<<(
    std::ostream& out,
    const AttributionReport::CommonAggregatableData& data) {
  return out << "{aggregation_coordinator_origin="
             << (data.aggregation_coordinator_origin.has_value()
                     ? data.aggregation_coordinator_origin->Serialize()
                     : "null")
             << ",aggregatable_trigger_config="
             << data.aggregatable_trigger_config << "}";
}

std::ostream& operator<<(
    std::ostream& out,
    const AttributionReport::AggregatableAttributionData& data) {
  out << "{common_data=" << data.common_data << ",contributions=[";

  const char* separator = "";
  for (const auto& contribution : data.contributions) {
    out << separator << contribution;
    separator = ", ";
  }

  out << "],source_time=" << data.source_time << ",source_debug_key=";

  if (data.source_debug_key.has_value()) {
    out << *data.source_debug_key;
  } else {
    out << "null";
  }

  return out << ",source_origin=" << data.source_origin << "}";
}

std::ostream& operator<<(std::ostream& out,
                         const AttributionReport::NullAggregatableData& data) {
  return out << "{common_data=" << data.common_data
             << ",fake_source_time=" << data.fake_source_time << "}";
}

namespace {
std::ostream& operator<<(std::ostream& out,
                         const AttributionReport::Data& data) {
  absl::visit([&out](const auto& v) { out << v; }, data);
  return out;
}
}  // namespace

std::ostream& operator<<(std::ostream& out, const AttributionReport& report) {
  return out << "{attribution_info=" << report.attribution_info()
             << ",id=" << *report.id()
             << ",report_time=" << report.report_time()
             << ",initial_report_time=" << report.initial_report_time()
             << ",external_report_id=" << report.external_report_id()
             << ",failed_send_attempts=" << report.failed_send_attempts()
             << ",data=" << report.data()
             << ",reporting_origin=" << report.reporting_origin() << "}";
}

std::ostream& operator<<(std::ostream& out, SendResult::Status status) {
  switch (status) {
    case SendResult::Status::kSent:
      return out << "kSent";
    case SendResult::Status::kTransientFailure:
      return out << "kTransientFailure";
    case SendResult::Status::kFailure:
      return out << "kFailure";
    case SendResult::Status::kDropped:
      return out << "kDropped";
    case SendResult::Status::kAssemblyFailure:
      return out << "kAssemblyFailure";
    case SendResult::Status::kTransientAssemblyFailure:
      return out << "kTransientAssemblyFailure";
  }
}

std::ostream& operator<<(std::ostream& out, const SendResult& info) {
  absl::visit(base::Overloaded{
                  [&](SendResult::Sent sent) {
                    out << "{Sent={result=";
                    switch (sent.result) {
                      case SendResult::Sent::Result::kSent:
                        out << "kSent";
                        break;
                      case SendResult::Sent::Result::kTransientFailure:
                        out << "kTransientFailure";
                        break;
                      case SendResult::Sent::Result::kFailure:
                        out << "kFailure";
                        break;
                    }
                    out << ",status=" << sent.status << "}}";
                  },
                  [&](SendResult::Dropped) { out << "{Dropped={}}"; },
                  [&](SendResult::AssemblyFailure failure) {
                    out << "{AssemblyFailure={transient=" << failure.transient
                        << "}}";
                  },
              },
              info.result);
  return out;
}

std::ostream& operator<<(std::ostream& out,
                         const AttributionDataModel::DataKey& key) {
  return out << "{reporting_origin=" << key.reporting_origin() << "}";
}

TestAggregatableSourceProvider::TestAggregatableSourceProvider(size_t size) {
  attribution_reporting::AggregationKeys::Keys::container_type keys;
  keys.reserve(size);
  for (size_t i = 0; i < size; ++i) {
    keys.emplace_back(base::NumberToString(i), i);
  }

  auto source =
      attribution_reporting::AggregationKeys::FromKeys(std::move(keys));
  DCHECK(source.has_value());
  source_ = *std::move(source);
}

TestAggregatableSourceProvider::~TestAggregatableSourceProvider() = default;

SourceBuilder TestAggregatableSourceProvider::GetBuilder(
    base::Time source_time) const {
  return SourceBuilder(source_time).SetAggregationKeys(source_);
}

TriggerBuilder DefaultAggregatableTriggerBuilder(
    const std::vector<uint32_t>& histogram_values) {
  std::vector<attribution_reporting::AggregatableTriggerData>
      aggregatable_trigger_data;

  attribution_reporting::AggregatableValues::Values aggregatable_values;

  for (size_t i = 0; i < histogram_values.size(); ++i) {
    std::string key_id = base::NumberToString(i);
    aggregatable_trigger_data.push_back(
        attribution_reporting::AggregatableTriggerData(
            absl::MakeUint128(/*high=*/i, /*low=*/0),
            /*source_keys=*/{key_id}, FilterPair()));
    aggregatable_values.emplace(
        std::move(key_id),
        *attribution_reporting::AggregatableValuesValue::Create(
            histogram_values[i], attribution_reporting::kDefaultFilteringId));
  }

  return TriggerBuilder()
      .SetAggregatableTriggerData(std::move(aggregatable_trigger_data))
      .SetAggregatableValues(
          {*attribution_reporting::AggregatableValues::Create(
              std::move(aggregatable_values), FilterPair())});
}

std::vector<blink::mojom::AggregatableReportHistogramContribution>
DefaultAggregatableHistogramContributions(
    const std::vector<int32_t>& histogram_values) {
  std::vector<blink::mojom::AggregatableReportHistogramContribution>
      contributions;
  for (size_t i = 0; i < histogram_values.size(); ++i) {
    contributions.emplace_back(absl::MakeUint128(i, i), histogram_values[i],
                               attribution_reporting::kDefaultFilteringId);
  }
  return contributions;
}

bool operator==(const OsRegistration& a, const OsRegistration& b) {
  const auto tie = [](const OsRegistration& r) {
    return std::make_tuple(r.registration_items, r.top_level_origin,
                           r.GetType());
  };
  return tie(a) == tie(b);
}

std::ostream& operator<<(std::ostream& out, const OsRegistration& r) {
  out << "{registration_items=[";
  const char* separator = "";
  for (const OsRegistrationItem& item : r.registration_items) {
    out << separator << item;
    separator = ",";
  }
  return out << "],top_level_origin=" << r.top_level_origin
             << ",type=" << r.GetType() << "}";
}

namespace {

void CheckAttributionReportingHeader(
    const std::string& header,
    const std::vector<std::string>& required_keys,
    const std::vector<std::string>& prohibited_keys) {
  auto dict = net::structured_headers::ParseDictionary(header);
  EXPECT_TRUE(dict.has_value());
  if (!dict.has_value()) {
    return;
  }

  for (const auto& key : required_keys) {
    EXPECT_TRUE(dict->contains(key)) << key;
  }

  for (const auto& key : prohibited_keys) {
    EXPECT_FALSE(dict->contains(key)) << key;
  }
}

}  // namespace

void ExpectValidAttributionReportingEligibleHeaderForEventBeacon(
    const std::string& header) {
  CheckAttributionReportingHeader(
      header,
      /*required_keys=*/{"event-source"},
      /*prohibited_keys=*/{"navigation-source", "trigger"});
}

void ExpectValidAttributionReportingEligibleHeaderForImg(
    const std::string& header) {
  CheckAttributionReportingHeader(header,
                                  /*required_keys=*/{"event-source", "trigger"},
                                  /*prohibited_keys=*/{"navigation-source"});
}

void ExpectValidAttributionReportingEligibleHeaderForNavigation(
    const std::string& header) {
  CheckAttributionReportingHeader(
      header,
      /*required_keys=*/{"navigation-source"},
      /*prohibited_keys=*/{"event-source", "trigger"});
}

void ExpectEmptyAttributionReportingEligibleHeader(const std::string& header) {
  CheckAttributionReportingHeader(
      header,
      /*required_keys=*/{},
      /*prohibited_keys=*/{"navigation-source", "event-source", "trigger"});
}

void ExpectValidAttributionReportingSupportHeader(const std::string& header,
                                                  bool web_expected,
                                                  bool os_expected) {
  std::vector<std::string> required_keys;
  std::vector<std::string> prohibited_keys;

  if (web_expected) {
    required_keys.emplace_back("web");
  } else {
    prohibited_keys.emplace_back("web");
  }

  if (os_expected) {
    required_keys.emplace_back("os");
  } else {
    prohibited_keys.emplace_back("os");
  }

  CheckAttributionReportingHeader(header, required_keys, prohibited_keys);
}

}  // namespace content
