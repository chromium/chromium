// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/attribution_test_utils.h"

#include <stdint.h>

#include <algorithm>
#include <tuple>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/bind.h"
#include "base/time/time.h"
#include "components/attribution_reporting/aggregatable_dedup_key.h"
#include "components/attribution_reporting/aggregatable_trigger_data.h"
#include "components/attribution_reporting/constants.h"
#include "components/attribution_reporting/destination_set.h"
#include "components/attribution_reporting/event_report_windows.h"
#include "components/attribution_reporting/event_trigger_data.h"
#include "components/attribution_reporting/filters.h"
#include "components/attribution_reporting/source_type.mojom.h"
#include "components/attribution_reporting/suitable_origin.h"
#include "components/attribution_reporting/test_utils.h"
#include "components/attribution_reporting/trigger_registration.h"
#include "content/browser/attribution_reporting/attribution_manager.h"
#include "content/browser/attribution_reporting/attribution_observer.h"
#include "content/browser/attribution_reporting/attribution_reporting.mojom.h"
#include "content/browser/attribution_reporting/attribution_trigger.h"
#include "content/browser/attribution_reporting/os_registration.h"
#include "content/browser/attribution_reporting/rate_limit_result.h"
#include "content/public/browser/attribution_data_model.h"
#include "net/base/net_errors.h"
#include "net/base/schemeful_site.h"
#include "net/http/structured_headers.h"
#include "services/network/public/cpp/trigger_verification.h"
#include "services/network/public/cpp/trigger_verification_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/numeric/int128.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

namespace {

using ::attribution_reporting::FilterPair;
using ::attribution_reporting::SuitableOrigin;
using ::attribution_reporting::mojom::SourceType;

using ::testing::AllOf;
using ::testing::Field;
using ::testing::Property;

const char kDefaultSourceOrigin[] = "https://impression.test/";
const char kDefaultDestinationOrigin[] = "https://sub.conversion.test/";
const char kDefaultReportOrigin[] = "https://report.test/";

}  // namespace

base::Uuid DefaultExternalReportID() {
  return base::Uuid::ParseLowercase("21abd97f-73e8-4b88-9389-a9fee6abda5e");
}

absl::optional<base::Time> GetReportWindowTimeForTesting(
    absl::optional<base::TimeDelta> declared_window,
    base::Time source_time) {
  if (!declared_window) {
    return absl::nullopt;
  }
  return source_time + *declared_window;
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
  registration_.max_event_level_reports =
      attribution_reporting::kMaxSettableEventLevelAttributions;
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

SourceBuilder& SourceBuilder::SetDebugKey(absl::optional<uint64_t> debug_key) {
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

SourceBuilder& SourceBuilder::SetAggregatableBudgetConsumed(
    int64_t aggregatable_budget_consumed) {
  aggregatable_budget_consumed_ = aggregatable_budget_consumed;
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

SourceBuilder& SourceBuilder::SetEventReportWindows(
    attribution_reporting::EventReportWindows event_report_windows) {
  registration_.event_report_windows = std::move(event_report_windows);
  return *this;
}

SourceBuilder& SourceBuilder::SetMaxEventLevelReports(
    int max_event_level_reports) {
  registration_.max_event_level_reports = max_event_level_reports;
  return *this;
}

CommonSourceInfo SourceBuilder::BuildCommonInfo() const {
  return CommonSourceInfo(source_origin_, reporting_origin_, source_type_);
}

StorableSource SourceBuilder::Build() const {
  return StorableSource(reporting_origin_, registration_, source_origin_,
                        source_type_, is_within_fenced_frame_);
}

StoredSource SourceBuilder::BuildStored() const {
  base::Time expiry_time = source_time_ + registration_.expiry;
  StoredSource source(
      BuildCommonInfo(), registration_.source_event_id,
      registration_.destination_set, source_time_, expiry_time,
      registration_.event_report_windows.value_or(
          *attribution_reporting::EventReportWindows::CreateWindows(
              base::Milliseconds(0), {registration_.expiry})),
      source_time_ + registration_.aggregatable_report_window,
      registration_.max_event_level_reports, registration_.priority,
      registration_.filter_data, registration_.debug_key,
      registration_.aggregation_keys, attribution_logic_, active_state_,
      source_id_, aggregatable_budget_consumed_, randomized_response_rate_);
  source.SetDedupKeys(dedup_keys_);
  source.SetAggregatableDedupKeys(aggregatable_dedup_keys_);
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

TriggerBuilder& TriggerBuilder::SetDedupKey(
    absl::optional<uint64_t> dedup_key) {
  dedup_key_ = dedup_key;
  return *this;
}

TriggerBuilder& TriggerBuilder::SetDebugKey(
    absl::optional<uint64_t> debug_key) {
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
    attribution_reporting::AggregatableValues aggregatable_values) {
  aggregatable_values_ = std::move(aggregatable_values);
  return *this;
}

TriggerBuilder& TriggerBuilder::SetAggregatableDedupKey(
    absl::optional<uint64_t> aggregatable_dedup_key) {
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

TriggerBuilder& TriggerBuilder::SetVerifications(
    std::vector<network::TriggerVerification> verifications) {
  verifications_ = std::move(verifications);
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

AttributionTrigger TriggerBuilder::Build(
    bool generate_event_trigger_data) const {
  std::vector<attribution_reporting::EventTriggerData> event_triggers;

  if (generate_event_trigger_data) {
    event_triggers.emplace_back(trigger_data_, priority_, dedup_key_,
                                FilterPair());
  }

  return AttributionTrigger(
      reporting_origin_,
      attribution_reporting::TriggerRegistration(
          filter_pair_, debug_key_,
          {attribution_reporting::AggregatableDedupKey(
              /*dedup_key=*/aggregatable_dedup_key_,
              aggregatable_dedup_key_filter_pair_)},
          std::move(event_triggers), aggregatable_trigger_data_,
          aggregatable_values_, debug_reporting_,
          aggregation_coordinator_origin_, source_registration_time_config_),
      destination_origin_, verifications_, is_within_fenced_frame_);
}

AttributionInfoBuilder::AttributionInfoBuilder(SuitableOrigin context_origin)
    : context_origin_(std::move(context_origin)) {}

AttributionInfoBuilder::~AttributionInfoBuilder() = default;

AttributionInfoBuilder& AttributionInfoBuilder::SetTime(base::Time time) {
  time_ = time;
  return *this;
}

AttributionInfoBuilder& AttributionInfoBuilder::SetDebugKey(
    absl::optional<uint64_t> debug_key) {
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
    std::vector<AggregatableHistogramContribution> contributions) {
  DCHECK(!contributions.empty());
  contributions_ = std::move(contributions);
  return *this;
}

ReportBuilder& ReportBuilder::SetAggregationCoordinatorOrigin(
    SuitableOrigin aggregation_coordinator_origin) {
  aggregation_coordinator_origin_ = std::move(aggregation_coordinator_origin);
  return *this;
}

ReportBuilder& ReportBuilder::SetVerificationToken(
    absl::optional<std::string> verification_token) {
  verification_token_ = std::move(verification_token);
  return *this;
}

ReportBuilder& ReportBuilder::SetSourceRegistrationTimeConfig(
    attribution_reporting::mojom::SourceRegistrationTimeConfig
        source_registration_time_config) {
  source_registration_time_config_ = source_registration_time_config;
  return *this;
}

AttributionReport ReportBuilder::Build() const {
  return AttributionReport(
      attribution_info_, report_id_, report_time_,
      /*initial_report_time=*/report_time_, external_report_id_,
      /*failed_send_attempts=*/0,
      AttributionReport::EventLevelData(trigger_data_, priority_, source_));
}

AttributionReport ReportBuilder::BuildAggregatableAttribution() const {
  return AttributionReport(
      attribution_info_, report_id_, report_time_,
      /*initial_report_time=*/report_time_, external_report_id_,
      /*failed_send_attempts=*/0,
      AttributionReport::AggregatableAttributionData(
          AttributionReport::CommonAggregatableData(
              aggregation_coordinator_origin_, verification_token_,
              source_registration_time_config_),
          contributions_, source_));
}

AttributionReport ReportBuilder::BuildNullAggregatable() const {
  return AttributionReport(
      attribution_info_, report_id_, report_time_,
      /*initial_report_time=*/report_time_, external_report_id_,
      /*failed_send_attempts=*/0,
      AttributionReport::NullAggregatableData(
          AttributionReport::CommonAggregatableData(
              aggregation_coordinator_origin_, verification_token_,
              source_registration_time_config_),
          source_.common_info().reporting_origin(), source_.source_time()));
}

bool operator==(const AttributionTrigger& a, const AttributionTrigger& b) {
  const auto tie = [](const AttributionTrigger& t) {
    return std::make_tuple(t.registration(), t.destination_origin(),
                           t.is_within_fenced_frame());
  };
  return tie(a) == tie(b);
}

bool operator==(const CommonSourceInfo& a, const CommonSourceInfo& b) {
  const auto tie = [](const CommonSourceInfo& source) {
    return std::make_tuple(source.source_origin(), source.reporting_origin(),
                           source.source_type());
  };
  return tie(a) == tie(b);
}

bool operator==(const AttributionInfo& a, const AttributionInfo& b) {
  const auto tie = [](const AttributionInfo& attribution_info) {
    return std::make_tuple(attribution_info.debug_key,
                           attribution_info.context_origin);
  };
  return tie(a) == tie(b);
}

bool operator==(const AttributionStorageDelegate::FakeReport& a,
                const AttributionStorageDelegate::FakeReport& b) {
  const auto tie = [](const AttributionStorageDelegate::FakeReport& r) {
    return std::make_tuple(r.trigger_data, r.trigger_time, r.report_time);
  };
  return tie(a) == tie(b);
}

bool operator<(const AttributionStorageDelegate::FakeReport& a,
               const AttributionStorageDelegate::FakeReport& b) {
  const auto tie = [](const AttributionStorageDelegate::FakeReport& r) {
    return std::make_tuple(r.trigger_data, r.trigger_time, r.report_time);
  };
  return tie(a) < tie(b);
}

bool operator==(const StorableSource& a, const StorableSource& b) {
  const auto tie = [](const StorableSource& source) {
    return std::make_tuple(source.registration(), source.common_info(),
                           source.is_within_fenced_frame());
  };
  return tie(a) == tie(b);
}

// Does not compare source IDs, as they are set by the underlying sqlite DB and
// should not be tested.
bool operator==(const StoredSource& a, const StoredSource& b) {
  const auto tie = [](const StoredSource& source) {
    return std::make_tuple(
        source.common_info(), source.source_event_id(),
        source.destination_sites(), source.source_time(), source.expiry_time(),
        source.event_report_windows(), source.aggregatable_report_window_time(),
        source.max_event_level_reports(), source.priority(),
        source.filter_data(), source.debug_key(), source.aggregation_keys(),
        source.attribution_logic(), source.active_state(), source.dedup_keys(),
        source.aggregatable_budget_consumed(), source.aggregatable_dedup_keys(),
        source.randomized_response_rate());
  };
  return tie(a) == tie(b);
}

bool operator==(const AggregatableHistogramContribution& a,
                const AggregatableHistogramContribution& b) {
  const auto tie = [](const AggregatableHistogramContribution& contribution) {
    return std::make_tuple(contribution.key(), contribution.value());
  };
  return tie(a) == tie(b);
}

bool operator==(const AttributionReport::EventLevelData& a,
                const AttributionReport::EventLevelData& b) {
  const auto tie = [](const AttributionReport::EventLevelData& data) {
    return std::make_tuple(data.trigger_data, data.priority, data.source);
  };
  return tie(a) == tie(b);
}

// Does not compare the assembled report as it is returned by the
// aggregation service from all the other data.
bool operator==(const AttributionReport::CommonAggregatableData& a,
                const AttributionReport::CommonAggregatableData& b) {
  const auto tie = [](const AttributionReport::CommonAggregatableData& data) {
    return std::make_tuple(data.verification_token,
                           data.aggregation_coordinator_origin,
                           data.source_registration_time_config);
  };
  return tie(a) == tie(b);
}

bool operator==(const AttributionReport::AggregatableAttributionData& a,
                const AttributionReport::AggregatableAttributionData& b) {
  const auto tie =
      [](const AttributionReport::AggregatableAttributionData& data) {
        return std::make_tuple(data.common_data, data.contributions,
                               data.source);
      };
  return tie(a) == tie(b);
}

bool operator==(const AttributionReport::NullAggregatableData& a,
                const AttributionReport::NullAggregatableData& b) {
  const auto tie = [](const AttributionReport::NullAggregatableData& data) {
    return std::make_tuple(data.common_data, data.reporting_origin,
                           data.fake_source_time);
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
                           report.failed_send_attempts(), report.data());
  };
  return tie(a) == tie(b);
}

bool operator==(const SendResult& a, const SendResult& b) {
  const auto tie = [](const SendResult& info) {
    return std::make_tuple(info.status, info.network_error,
                           info.http_response_code);
  };
  return tie(a) == tie(b);
}

std::ostream& operator<<(std::ostream& out,
                         AttributionTrigger::EventLevelResult status) {
  switch (status) {
    case AttributionTrigger::EventLevelResult::kSuccess:
      return out << "success";
    case AttributionTrigger::EventLevelResult::kSuccessDroppedLowerPriority:
      return out << "successDroppedLowerPriority";
    case AttributionTrigger::EventLevelResult::kInternalError:
      return out << "internalError";
    case AttributionTrigger::EventLevelResult::
        kNoCapacityForConversionDestination:
      return out << "insufficientDestinationCapacity";
    case AttributionTrigger::EventLevelResult::kNoMatchingImpressions:
      return out << "noMatchingSources";
    case AttributionTrigger::EventLevelResult::kDeduplicated:
      return out << "deduplicated";
    case AttributionTrigger::EventLevelResult::kExcessiveAttributions:
      return out << "excessiveAttributions";
    case AttributionTrigger::EventLevelResult::kPriorityTooLow:
      return out << "priorityTooLow";
    case AttributionTrigger::EventLevelResult::kNeverAttributedSource:
      return out << "neverAttributedSource";
    case AttributionTrigger::EventLevelResult::kExcessiveReportingOrigins:
      return out << "excessiveReportingOrigins";
    case AttributionTrigger::EventLevelResult::kNoMatchingSourceFilterData:
      return out << "noMatchingSourceFilterData";
    case AttributionTrigger::EventLevelResult::kProhibitedByBrowserPolicy:
      return out << "prohibitedByBrowserPolicy";
    case AttributionTrigger::EventLevelResult::kNoMatchingConfigurations:
      return out << "noMatchingConfigurations";
    case AttributionTrigger::EventLevelResult::kExcessiveReports:
      return out << "excessiveReports";
    case AttributionTrigger::EventLevelResult::kFalselyAttributedSource:
      return out << "falselyAttributedSource";
    case AttributionTrigger::EventLevelResult::kReportWindowNotStarted:
      return out << "reportWindowNotStarted";
    case AttributionTrigger::EventLevelResult::kReportWindowPassed:
      return out << "reportWindowPassed";
    case AttributionTrigger::EventLevelResult::kNotRegistered:
      return out << "notRegistered";
  }
}

std::ostream& operator<<(std::ostream& out,
                         AttributionTrigger::AggregatableResult status) {
  switch (status) {
    case AttributionTrigger::AggregatableResult::kSuccess:
      return out << "success";
    case AttributionTrigger::AggregatableResult::kInternalError:
      return out << "internalError";
    case AttributionTrigger::AggregatableResult::
        kNoCapacityForConversionDestination:
      return out << "insufficientDestinationCapacity";
    case AttributionTrigger::AggregatableResult::kNoMatchingImpressions:
      return out << "noMatchingSources";
    case AttributionTrigger::AggregatableResult::kExcessiveAttributions:
      return out << "excessiveAttributions";
    case AttributionTrigger::AggregatableResult::kExcessiveReportingOrigins:
      return out << "excessiveReportingOrigins";
    case AttributionTrigger::AggregatableResult::kNoHistograms:
      return out << "noHistograms";
    case AttributionTrigger::AggregatableResult::kInsufficientBudget:
      return out << "insufficientBudget";
    case AttributionTrigger::AggregatableResult::kNoMatchingSourceFilterData:
      return out << "noMatchingSourceFilterData";
    case AttributionTrigger::AggregatableResult::kNotRegistered:
      return out << "notRegistered";
    case AttributionTrigger::AggregatableResult::kProhibitedByBrowserPolicy:
      return out << "prohibitedByBrowserPolicy";
    case AttributionTrigger::AggregatableResult::kDeduplicated:
      return out << "deduplicated";
    case AttributionTrigger::AggregatableResult::kReportWindowPassed:
      return out << "reportWindowPassed";
    case AttributionTrigger::AggregatableResult::kExcessiveReports:
      return out << "excessiveReports";
  }
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
  out << "{registration=" << conversion.registration()
      << ",destination_origin=" << conversion.destination_origin()
      << ",is_within_fenced_frame=" << conversion.is_within_fenced_frame()
      << ",verifications=[";
  const char* separator = "";
  for (const auto& verification : conversion.verifications()) {
    out << separator << verification;
    separator = ", ";
  }
  out << "]";

  return out << "}";
}

std::ostream& operator<<(std::ostream& out, const CommonSourceInfo& source) {
  return out << "{source_origin=" << source.source_origin()
             << "reporting_origin=" << source.reporting_origin()
             << ",source_type=" << source.source_type() << "}";
}

std::ostream& operator<<(std::ostream& out,
                         const AttributionInfo& attribution_info) {
  return out << "{time=" << attribution_info.time << ",debug_key="
             << (attribution_info.debug_key
                     ? base::NumberToString(*attribution_info.debug_key)
                     : "null")
             << ",context_origin=" << attribution_info.context_origin << "}";
}

std::ostream& operator<<(std::ostream& out,
                         const AttributionStorageDelegate::FakeReport& r) {
  return out << "{trigger_data=" << r.trigger_data
             << ",trigger_time=" << r.trigger_time
             << ",report_time=" << r.report_time << "}";
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
      << ",event_report_windows=" << source.event_report_windows()
      << ",aggregatable_report_window_time="
      << source.aggregatable_report_window_time()
      << ",max_event_level_reports=" << source.max_event_level_reports()
      << ",priority=" << source.priority()
      << ",filter_data=" << source.filter_data() << ",debug_key="
      << (source.debug_key() ? base::NumberToString(*source.debug_key())
                             : "null")
      << ",aggregation_keys=" << source.aggregation_keys()
      << ",attribution_logic=" << source.attribution_logic()
      << ",active_state=" << source.active_state()
      << ",source_id=" << *source.source_id()
      << ",aggregatable_budget_consumed="
      << source.aggregatable_budget_consumed()
      << ",randomized_response_rate=" << source.randomized_response_rate()
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
    const AggregatableHistogramContribution& contribution) {
  return out << "{key=" << contribution.key()
             << ",value=" << contribution.value() << "}";
}

std::ostream& operator<<(std::ostream& out,
                         const AttributionReport::EventLevelData& data) {
  return out << "{trigger_data=" << data.trigger_data
             << ",priority=" << data.priority
             << ",source=" << data.source << "}";
}

std::ostream& operator<<(
    std::ostream& out,
    const AttributionReport::CommonAggregatableData& data) {
  out << "{aggregation_coordinator_origin="
      << (data.aggregation_coordinator_origin.has_value()
              ? data.aggregation_coordinator_origin->Serialize()
              : "null")
      << ",verification_token=";

  if (const auto& verification_token = data.verification_token;
      verification_token.has_value()) {
    out << *verification_token;
  } else {
    out << "(null)";
  }

  return out << ",source_registration_time_config="
             << data.source_registration_time_config << "}";
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

  out << "]";

  return out << ",source=" << data.source << "}";
}

std::ostream& operator<<(std::ostream& out,
                         const AttributionReport::NullAggregatableData& data) {
  return out << "{common_data=" << data.common_data
             << ",reporting_origin=" << data.reporting_origin
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
             << ",data=" << report.data() << "}";
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
  return out << "{status=" << info.status
             << ",network_error=" << net::ErrorToShortString(info.network_error)
             << ",http_response_code=" << info.http_response_code << "}";
}

std::ostream& operator<<(std::ostream& out,
                         const AttributionDataModel::DataKey& key) {
  return out << "{reporting_origin=" << key.reporting_origin() << "}";
}

SourceRegistrationMatcherConfig::SourceRegistrationMatcherConfig(
    ::testing::Matcher<uint64_t> source_event_id,
    ::testing::Matcher<const attribution_reporting::DestinationSet&>
        destination_set,
    ::testing::Matcher<uint64_t> priority,
    ::testing::Matcher<absl::optional<uint64_t>> debug_key,
    ::testing::Matcher<const attribution_reporting::AggregationKeys&>
        aggregation_keys,
    ::testing::Matcher<bool> debug_reporting)
    : source_event_id(std::move(source_event_id)),
      destination_set(std::move(destination_set)),
      priority(std::move(priority)),
      debug_key(std::move(debug_key)),
      aggregation_keys(std::move(aggregation_keys)),
      debug_reporting(std::move(debug_reporting)) {}

SourceRegistrationMatcherConfig::~SourceRegistrationMatcherConfig() = default;

::testing::Matcher<const attribution_reporting::SourceRegistration&>
SourceRegistrationMatches(const SourceRegistrationMatcherConfig& cfg) {
  return AllOf(
      Field("source_event_id",
            &attribution_reporting::SourceRegistration::source_event_id,
            cfg.source_event_id),
      Field("destination_set",
            &attribution_reporting::SourceRegistration::destination_set,
            cfg.destination_set),
      Field("priority", &attribution_reporting::SourceRegistration::priority,
            cfg.priority),
      Field("debug_key", &attribution_reporting::SourceRegistration::debug_key,
            cfg.debug_key),
      Field("aggregation_keys",
            &attribution_reporting::SourceRegistration::aggregation_keys,
            cfg.aggregation_keys),
      Field("debug_reporting",
            &attribution_reporting::SourceRegistration::debug_reporting,
            cfg.debug_reporting));
}

EventTriggerDataMatcherConfig::EventTriggerDataMatcherConfig(
    ::testing::Matcher<uint64_t> data,
    ::testing::Matcher<int64_t> priority,
    ::testing::Matcher<absl::optional<uint64_t>> dedup_key,
    ::testing::Matcher<const FilterPair&> filters)
    : data(std::move(data)),
      priority(std::move(priority)),
      dedup_key(std::move(dedup_key)),
      filters(std::move(filters)) {}

EventTriggerDataMatcherConfig::~EventTriggerDataMatcherConfig() = default;

::testing::Matcher<const attribution_reporting::EventTriggerData&>
EventTriggerDataMatches(const EventTriggerDataMatcherConfig& cfg) {
  return ::testing::AllOf(
      Field("data", &attribution_reporting::EventTriggerData::data, cfg.data),
      Field("priority", &attribution_reporting::EventTriggerData::priority,
            cfg.priority),
      Field("dedup_key", &attribution_reporting::EventTriggerData::dedup_key,
            cfg.dedup_key),
      Field("filters", &attribution_reporting::EventTriggerData::filters,
            cfg.filters));
}

TriggerRegistrationMatcherConfig::TriggerRegistrationMatcherConfig(
    ::testing::Matcher<const FilterPair&> filters,
    ::testing::Matcher<absl::optional<uint64_t>> debug_key,
    ::testing::Matcher<
        const std::vector<attribution_reporting::EventTriggerData>&>
        event_triggers,
    ::testing::Matcher<
        const std::vector<attribution_reporting::AggregatableDedupKey>&>
        aggregatable_dedup_keys,
    ::testing::Matcher<bool> debug_reporting,
    ::testing::Matcher<
        const std::vector<attribution_reporting::AggregatableTriggerData>&>
        aggregatable_trigger_data,
    ::testing::Matcher<const attribution_reporting::AggregatableValues&>
        aggregatable_values,
    ::testing::Matcher<const absl::optional<SuitableOrigin>&>
        aggregation_coordinator_origin,
    ::testing::Matcher<
        attribution_reporting::mojom::SourceRegistrationTimeConfig>
        source_registration_time_config)
    : filters(std::move(filters)),
      debug_key(std::move(debug_key)),
      event_triggers(std::move(event_triggers)),
      aggregatable_dedup_keys(std::move(aggregatable_dedup_keys)),
      debug_reporting(std::move(debug_reporting)),
      aggregatable_trigger_data(std::move(aggregatable_trigger_data)),
      aggregatable_values(std::move(aggregatable_values)),
      aggregation_coordinator_origin(std::move(aggregation_coordinator_origin)),
      source_registration_time_config(
          std::move(source_registration_time_config)) {}

TriggerRegistrationMatcherConfig::~TriggerRegistrationMatcherConfig() = default;

::testing::Matcher<const attribution_reporting::TriggerRegistration&>
TriggerRegistrationMatches(const TriggerRegistrationMatcherConfig& cfg) {
  return AllOf(
      Field("filters", &attribution_reporting::TriggerRegistration::filters,
            cfg.filters),
      Field("debug_key", &attribution_reporting::TriggerRegistration::debug_key,
            cfg.debug_key),
      Field("event_triggers",
            &attribution_reporting::TriggerRegistration::event_triggers,
            cfg.event_triggers),
      Field(
          "aggregatable_dedup_keys",
          &attribution_reporting::TriggerRegistration::aggregatable_dedup_keys,
          cfg.aggregatable_dedup_keys),
      Field("debug_reporting",
            &attribution_reporting::TriggerRegistration::debug_reporting,
            cfg.debug_reporting),
      Field("aggregatable_trigger_data",
            &attribution_reporting::TriggerRegistration::
                aggregatable_trigger_data,
            cfg.aggregatable_trigger_data),
      Field("aggregatable_values",
            &attribution_reporting::TriggerRegistration::aggregatable_values,
            cfg.aggregatable_values),
      Field("aggregation_coordinator_origin",
            &attribution_reporting::TriggerRegistration::
                aggregation_coordinator_origin,
            cfg.aggregation_coordinator_origin),
      Field("source_registration_time_config",
            &attribution_reporting::TriggerRegistration::
                source_registration_time_config,
            cfg.source_registration_time_config));
}

AttributionTriggerMatcherConfig::AttributionTriggerMatcherConfig(
    ::testing::Matcher<const SuitableOrigin&> reporting_origin,
    ::testing::Matcher<const attribution_reporting::TriggerRegistration&>
        registration,
    ::testing::Matcher<const SuitableOrigin&> destination_origin,
    ::testing::Matcher<bool> is_within_fenced_frame)
    : reporting_origin(std::move(reporting_origin)),
      registration(std::move(registration)),
      destination_origin(std::move(destination_origin)),
      is_within_fenced_frame(std::move(is_within_fenced_frame)) {}

AttributionTriggerMatcherConfig::~AttributionTriggerMatcherConfig() = default;

::testing::Matcher<AttributionTrigger> AttributionTriggerMatches(
    const AttributionTriggerMatcherConfig& cfg) {
  return AllOf(
      Property("reporting_origin", &AttributionTrigger::reporting_origin,
               cfg.reporting_origin),
      Property("registration", &AttributionTrigger::registration,
               cfg.registration),
      Property("destination_origin", &AttributionTrigger::destination_origin,
               cfg.destination_origin),
      Property("is_within_fenced_frame",
               &AttributionTrigger::is_within_fenced_frame,
               cfg.is_within_fenced_frame),
      Property("verifications", &AttributionTrigger::verifications,
               cfg.verifications));
}

std::vector<AttributionReport> GetAttributionReportsForTesting(
    AttributionManager* manager) {
  base::RunLoop run_loop;
  std::vector<AttributionReport> attribution_reports;
  manager->GetPendingReportsForInternalUse(
      /*limit=*/-1,
      base::BindLambdaForTesting([&](std::vector<AttributionReport> reports) {
        attribution_reports = std::move(reports);
        run_loop.Quit();
      }));
  run_loop.Run();
  return attribution_reports;
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
  source_ = std::move(*source);
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
        *attribution_reporting::AggregatableTriggerData::Create(
            absl::MakeUint128(/*high=*/i, /*low=*/0),
            /*source_keys=*/{key_id}, FilterPair()));
    aggregatable_values.emplace(std::move(key_id), histogram_values[i]);
  }

  return TriggerBuilder()
      .SetAggregatableTriggerData(std::move(aggregatable_trigger_data))
      .SetAggregatableValues(*attribution_reporting::AggregatableValues::Create(
          std::move(aggregatable_values)));
}

std::vector<AggregatableHistogramContribution>
DefaultAggregatableHistogramContributions(
    const std::vector<uint32_t>& histogram_values) {
  std::vector<AggregatableHistogramContribution> contributions;
  for (size_t i = 0; i < histogram_values.size(); ++i) {
    contributions.emplace_back(absl::MakeUint128(i, i), histogram_values[i]);
  }
  return contributions;
}

bool operator==(const OsRegistration& a, const OsRegistration& b) {
  const auto tie = [](const OsRegistration& r) {
    return std::make_tuple(r.registration_url, r.top_level_origin, r.GetType());
  };
  return tie(a) == tie(b);
}

std::ostream& operator<<(std::ostream& out, const OsRegistration& r) {
  return out << "{registration_url=" << r.registration_url
             << ",top_level_origin=" << r.top_level_origin
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
