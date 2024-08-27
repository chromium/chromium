// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/aggregatable_debug_report.h"

#include <stddef.h>
#include <stdint.h>

#include <optional>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/check_op.h"
#include "base/containers/enum_set.h"
#include "base/feature_list.h"
#include "base/functional/function_ref.h"
#include "base/numerics/checked_math.h"
#include "base/numerics/safe_conversions.h"
#include "base/ranges/algorithm.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/attribution_reporting/aggregatable_debug_reporting_config.h"
#include "components/attribution_reporting/aggregatable_filtering_id_max_bytes.h"
#include "components/attribution_reporting/aggregatable_utils.h"
#include "components/attribution_reporting/debug_types.h"
#include "components/attribution_reporting/debug_types.mojom.h"
#include "components/attribution_reporting/features.h"
#include "components/attribution_reporting/source_registration.h"
#include "components/attribution_reporting/suitable_origin.h"
#include "components/attribution_reporting/trigger_registration.h"
#include "content/browser/aggregation_service/aggregatable_report.h"
#include "content/browser/aggregation_service/aggregation_service_features.h"
#include "content/browser/attribution_reporting/aggregatable_attribution_utils.h"
#include "content/browser/attribution_reporting/aggregatable_result.mojom.h"
#include "content/browser/attribution_reporting/attribution_trigger.h"
#include "content/browser/attribution_reporting/create_report_result.h"
#include "content/browser/attribution_reporting/event_level_result.mojom.h"
#include "content/browser/attribution_reporting/storable_source.h"
#include "content/browser/attribution_reporting/store_source_result.h"
#include "content/browser/attribution_reporting/store_source_result.mojom.h"
#include "content/browser/attribution_reporting/stored_source.h"
#include "net/base/schemeful_site.h"
#include "third_party/abseil-cpp/absl/numeric/int128.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "third_party/blink/public/mojom/aggregation_service/aggregatable_report.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

namespace {

using ::attribution_reporting::DebugDataTypes;
using ::attribution_reporting::mojom::AggregatableResult;
using ::attribution_reporting::mojom::DebugDataType;
using ::attribution_reporting::mojom::EventLevelResult;
using ::blink::mojom::AggregatableReportHistogramContribution;

using StoreSourceStatus = ::attribution_reporting::mojom::StoreSourceResult;

constexpr size_t kMaxContributions = 2;

constexpr char kApiIdentifier[] = "attribution-reporting-debug";
constexpr char kVersion[] = "0.1";
constexpr char kVersionWithFlexibleContributionFiltering[] = "1.0";

std::optional<DebugDataType> GetDebugType(const StoreSourceResult& result) {
  switch (result.status()) {
    case StoreSourceStatus::kSuccess:
      return DebugDataType::kSourceSuccess;
    case StoreSourceStatus::kInternalError:
      return DebugDataType::kSourceUnknownError;
    case StoreSourceStatus::kInsufficientSourceCapacity:
      return DebugDataType::kSourceStorageLimit;
    case StoreSourceStatus::kInsufficientUniqueDestinationCapacity:
      return DebugDataType::kSourceDestinationLimit;
    case StoreSourceStatus::kExcessiveReportingOrigins:
      return DebugDataType::kSourceReportingOriginLimit;
    case StoreSourceStatus::kProhibitedByBrowserPolicy:
      return std::nullopt;
    case StoreSourceStatus::kSuccessNoised:
      return DebugDataType::kSourceNoised;
    case StoreSourceStatus::kDestinationReportingLimitReached:
    case StoreSourceStatus::kDestinationBothLimitsReached:
      return DebugDataType::kSourceDestinationRateLimit;
    case StoreSourceStatus::kDestinationGlobalLimitReached:
      return DebugDataType::kSourceDestinationGlobalRateLimit;
    case StoreSourceStatus::kReportingOriginsPerSiteLimitReached:
      return DebugDataType::kSourceReportingOriginPerSiteLimit;
    case StoreSourceStatus::kExceedsMaxChannelCapacity:
      return DebugDataType::kSourceChannelCapacityLimit;
    case StoreSourceStatus::kExceedsMaxTriggerStateCardinality:
      return DebugDataType::kSourceTriggerStateCardinalityLimit;
    case StoreSourceStatus::kDestinationPerDayReportingLimitReached:
      return DebugDataType::kSourceDestinationPerDayRateLimit;
    case StoreSourceStatus::kExceedsMaxScopesChannelCapacity:
      return DebugDataType::kSourceScopesChannelCapacityLimit;
    case StoreSourceStatus::kExceedsMaxEventStatesLimit:
      return DebugDataType::kSourceMaxEventStatesLimit;
  }
}

std::optional<DebugDataType> GetDebugType(EventLevelResult result) {
  switch (result) {
    case EventLevelResult::kSuccess:
    case EventLevelResult::kProhibitedByBrowserPolicy:
    case EventLevelResult::kSuccessDroppedLowerPriority:
    case EventLevelResult::kNotRegistered:
      return std::nullopt;
    case EventLevelResult::kInternalError:
      return DebugDataType::kTriggerUnknownError;
    case EventLevelResult::kNoCapacityForConversionDestination:
      return DebugDataType::kTriggerEventStorageLimit;
    case EventLevelResult::kExcessiveReportingOrigins:
      return DebugDataType::kTriggerReportingOriginLimit;
    case EventLevelResult::kNoMatchingImpressions:
      return DebugDataType::kTriggerNoMatchingSource;
    case EventLevelResult::kExcessiveAttributions:
      return DebugDataType::kTriggerEventAttributionsPerSourceDestinationLimit;
    case EventLevelResult::kNoMatchingSourceFilterData:
      return DebugDataType::kTriggerNoMatchingFilterData;
    case EventLevelResult::kDeduplicated:
      return DebugDataType::kTriggerEventDeduplicated;
    case EventLevelResult::kNoMatchingConfigurations:
      return DebugDataType::kTriggerEventNoMatchingConfigurations;
    case EventLevelResult::kNeverAttributedSource:
    case EventLevelResult::kFalselyAttributedSource:
      return DebugDataType::kTriggerEventNoise;
    case EventLevelResult::kPriorityTooLow:
      return DebugDataType::kTriggerEventLowPriority;
    case EventLevelResult::kExcessiveReports:
      return DebugDataType::kTriggerEventExcessiveReports;
    case EventLevelResult::kReportWindowNotStarted:
      return DebugDataType::kTriggerEventReportWindowNotStarted;
    case EventLevelResult::kReportWindowPassed:
      return DebugDataType::kTriggerEventReportWindowPassed;
    case EventLevelResult::kNoMatchingTriggerData:
      return DebugDataType::kTriggerEventNoMatchingTriggerData;
  }
}

std::optional<DebugDataType> GetDebugType(AggregatableResult result) {
  switch (result) {
    case AggregatableResult::kSuccess:
    case AggregatableResult::kNotRegistered:
    case AggregatableResult::kProhibitedByBrowserPolicy:
      return std::nullopt;
    case AggregatableResult::kInternalError:
      return DebugDataType::kTriggerUnknownError;
    case AggregatableResult::kNoCapacityForConversionDestination:
      return DebugDataType::kTriggerAggregateStorageLimit;
    case AggregatableResult::kExcessiveReportingOrigins:
      return DebugDataType::kTriggerReportingOriginLimit;
    case AggregatableResult::kNoMatchingImpressions:
      return DebugDataType::kTriggerNoMatchingSource;
    case AggregatableResult::kExcessiveAttributions:
      return DebugDataType::
          kTriggerAggregateAttributionsPerSourceDestinationLimit;
    case AggregatableResult::kNoMatchingSourceFilterData:
      return DebugDataType::kTriggerNoMatchingFilterData;
    case AggregatableResult::kDeduplicated:
      return DebugDataType::kTriggerAggregateDeduplicated;
    case AggregatableResult::kNoHistograms:
      return DebugDataType::kTriggerAggregateNoContributions;
    case AggregatableResult::kInsufficientBudget:
      return DebugDataType::kTriggerAggregateInsufficientBudget;
    case AggregatableResult::kReportWindowPassed:
      return DebugDataType::kTriggerAggregateReportWindowPassed;
    case AggregatableResult::kExcessiveReports:
      return DebugDataType::kTriggerAggregateExcessiveReports;
  }
}

std::vector<AggregatableReportHistogramContribution>
GetAggregatableContributions(
    absl::uint128 context_key_piece,
    const attribution_reporting::AggregatableDebugReportingConfig::DebugData&
        debug_data,
    const DebugDataTypes& debug_types) {
  std::vector<AggregatableReportHistogramContribution> contributions;
  for (DebugDataType type : debug_types) {
    if (auto iter = debug_data.find(type); iter != debug_data.end()) {
      contributions.emplace_back(
          iter->second.key_piece() | context_key_piece,
          base::checked_cast<int32_t>(iter->second.value()),
          /*filtering_id=*/std::nullopt);
    }
  }
  return contributions;
}

bool IsAggregatableFilteringIdsEnabled() {
  return base::FeatureList::IsEnabled(
             attribution_reporting::features::
                 kAttributionReportingAggregatableFilteringIds) &&
         base::FeatureList::IsEnabled(
             kPrivacySandboxAggregationServiceFilteringIds);
}

}  // namespace

// static
std::optional<AggregatableDebugReport> AggregatableDebugReport::Create(
    base::FunctionRef<bool()> is_operation_allowed,
    const StoreSourceResult& result) {
  if (!base::FeatureList::IsEnabled(
          attribution_reporting::features::
              kAttributionAggregatableDebugReporting)) {
    return std::nullopt;
  }

  const StorableSource& source = result.source();
  const attribution_reporting::SourceAggregatableDebugReportingConfig& config =
      source.registration().aggregatable_debug_reporting_config;
  if (config.config().debug_data.empty() || source.is_within_fenced_frame() ||
      !is_operation_allowed()) {
    return std::nullopt;
  }

  CHECK(!source.registration().destination_set.destinations().empty());

  DebugDataTypes types;
  if (std::optional<DebugDataType> type = GetDebugType(result)) {
    types.Put(*type);
  }
  if (result.destination_limit().has_value()) {
    types.Put(DebugDataType::kSourceDestinationLimitReplaced);
  }

  return AggregatableDebugReport(
      GetAggregatableContributions(config.config().key_piece,
                                   config.config().debug_data, types),
      source.common_info().source_site(),
      source.common_info().reporting_origin(),
      *source.registration().destination_set.destinations().begin(),
      config.config().aggregation_coordinator_origin, result.source_time());
}

// static
std::optional<AggregatableDebugReport> AggregatableDebugReport::Create(
    base::FunctionRef<bool()> is_operation_allowed,
    const CreateReportResult& result) {
  if (!base::FeatureList::IsEnabled(
          attribution_reporting::features::
              kAttributionAggregatableDebugReporting)) {
    return std::nullopt;
  }

  if (absl::holds_alternative<CreateReportResult::NotRegistered>(
          result.event_level_result()) &&
      absl::holds_alternative<CreateReportResult::NotRegistered>(
          result.aggregatable_result())) {
    return std::nullopt;
  }

  const AttributionTrigger& trigger = result.trigger();
  const attribution_reporting::AggregatableDebugReportingConfig& config =
      trigger.registration().aggregatable_debug_reporting_config;
  if (config.debug_data.empty() || trigger.is_within_fenced_frame() ||
      !is_operation_allowed()) {
    return std::nullopt;
  }

  DebugDataTypes types;
  if (std::optional<DebugDataType> event_level_type =
          GetDebugType(result.event_level_status())) {
    types.Put(*event_level_type);
  }
  if (std::optional<DebugDataType> aggregatable_type =
          GetDebugType(result.aggregatable_status())) {
    types.Put(*aggregatable_type);
  }

  absl::uint128 context_key_piece = config.key_piece;
  if (result.source()) {
    context_key_piece |= result.source()->aggregatable_debug_key_piece();
  }

  return AggregatableDebugReport(
      GetAggregatableContributions(context_key_piece, config.debug_data, types),
      net::SchemefulSite(trigger.destination_origin()),
      trigger.reporting_origin(),
      net::SchemefulSite(trigger.destination_origin()),
      config.aggregation_coordinator_origin, result.trigger_time());
}

// static
AggregatableDebugReport AggregatableDebugReport::CreateForTesting(
    std::vector<AggregatableReportHistogramContribution> contributions,
    net::SchemefulSite context_site,
    attribution_reporting::SuitableOrigin reporting_origin,
    net::SchemefulSite effective_destination,
    std::optional<attribution_reporting::SuitableOrigin>
        aggregation_coordinator_origin,
    base::Time scheduled_report_time) {
  return AggregatableDebugReport(
      std::move(contributions), std::move(context_site),
      std::move(reporting_origin), std::move(effective_destination),
      std::move(aggregation_coordinator_origin), scheduled_report_time);
}

AggregatableDebugReport::AggregatableDebugReport(
    std::vector<AggregatableReportHistogramContribution> contributions,
    net::SchemefulSite context_site,
    attribution_reporting::SuitableOrigin reporting_origin,
    net::SchemefulSite effective_destination,
    std::optional<attribution_reporting::SuitableOrigin>
        aggregation_coordinator_origin,
    base::Time scheduled_report_time)
    : contributions_(std::move(contributions)),
      context_site_(std::move(context_site)),
      reporting_origin_(std::move(reporting_origin)),
      effective_destination_(std::move(effective_destination)),
      aggregation_coordinator_origin_(
          std::move(aggregation_coordinator_origin)),
      scheduled_report_time_(scheduled_report_time) {
  CHECK_LE(contributions_.size(), kMaxContributions);
  CHECK(base::ranges::all_of(contributions_, [](const auto& contribution) {
    return attribution_reporting::IsAggregatableValueInRange(
        contribution.value);
  }));
}

AggregatableDebugReport::AggregatableDebugReport(AggregatableDebugReport&&) =
    default;

AggregatableDebugReport& AggregatableDebugReport::operator=(
    AggregatableDebugReport&&) = default;

AggregatableDebugReport::~AggregatableDebugReport() = default;

int AggregatableDebugReport::BudgetRequired() const {
  base::CheckedNumeric<int64_t> budget_required =
      GetTotalAggregatableValues(contributions_);
  CHECK(budget_required.IsValid());
  int64_t budget_required_value = budget_required.ValueOrDie();
  CHECK(base::IsValueInRangeForNumericType<int>(budget_required_value));
  return budget_required_value;
}

net::SchemefulSite AggregatableDebugReport::ReportingSite() const {
  return net::SchemefulSite(reporting_origin_);
}

void AggregatableDebugReport::ToNull() {
  // Null contributions will be padded in
  // `ConstructUnencryptedTeeBasedPayload()`.
  contributions_.clear();
}

GURL AggregatableDebugReport::ReportUrl() const {
  static constexpr char kPath[] =
      "/.well-known/attribution-reporting/debug/report-aggregate-debug";

  GURL::Replacements replacements;
  replacements.SetPathStr(kPath);
  return reporting_origin_->GetURL().ReplaceComponents(replacements);
}

std::optional<AggregatableReportRequest>
AggregatableDebugReport::CreateAggregatableReportRequest() const {
  CHECK(report_id_.is_valid());

  std::optional<size_t> filtering_id_max_bytes;
  if (IsAggregatableFilteringIdsEnabled()) {
    filtering_id_max_bytes =
        attribution_reporting::AggregatableFilteringIdsMaxBytes().value();
  }

  base::Value::Dict additional_fields;
  SetAttributionDestination(additional_fields, effective_destination_);
  return AggregatableReportRequest::Create(
      AggregationServicePayloadContents(
          AggregationServicePayloadContents::Operation::kHistogram,
          contributions_, blink::mojom::AggregationServiceMode::kDefault,
          aggregation_coordinator_origin_
              ? std::make_optional(**aggregation_coordinator_origin_)
              : std::nullopt,
          kMaxContributions, filtering_id_max_bytes),
      AggregatableReportSharedInfo(
          scheduled_report_time_, report_id_, reporting_origin_,
          AggregatableReportSharedInfo::DebugMode::kDisabled,
          std::move(additional_fields),
          filtering_id_max_bytes.has_value()
              ? kVersionWithFlexibleContributionFiltering
              : kVersion,
          kApiIdentifier),
      // The returned request cannot be serialized due to the null `delay_type`.
      /*delay_type=*/std::nullopt);
}

}  // namespace content
