// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/attribution_debug_report.h"

#include <stdint.h>

#include <optional>
#include <string_view>
#include <utility>

#include "base/check.h"
#include "base/check_op.h"
#include "base/feature_list.h"
#include "base/functional/function_ref.h"
#include "base/functional/overloaded.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "components/attribution_reporting/constants.h"
#include "components/attribution_reporting/debug_types.h"
#include "components/attribution_reporting/debug_types.mojom.h"
#include "components/attribution_reporting/destination_set.h"
#include "components/attribution_reporting/os_registration.h"
#include "components/attribution_reporting/registration_header_error.h"
#include "components/attribution_reporting/source_registration.h"
#include "components/attribution_reporting/suitable_origin.h"
#include "components/attribution_reporting/trigger_registration.h"
#include "content/browser/attribution_reporting/attribution_constants.h"
#include "content/browser/attribution_reporting/attribution_features.h"
#include "content/browser/attribution_reporting/attribution_reporting.mojom.h"
#include "content/browser/attribution_reporting/attribution_trigger.h"
#include "content/browser/attribution_reporting/common_source_info.h"
#include "content/browser/attribution_reporting/create_report_result.h"
#include "content/browser/attribution_reporting/os_registration.h"
#include "content/browser/attribution_reporting/storable_source.h"
#include "content/browser/attribution_reporting/store_source_result.h"
#include "net/base/schemeful_site.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "url/gurl.h"

namespace content {

namespace {

using ::attribution_reporting::mojom::DebugDataType;

constexpr char kAttributionDestination[] = "attribution_destination";

struct DebugDataTypeAndBody {
  DebugDataType debug_data_type;
  base::Value limit;
  base::Value::Dict additional_fields;

  explicit DebugDataTypeAndBody(
      DebugDataType debug_data_type,
      base::Value limit = base::Value(),
      base::Value::Dict additional_fields = base::Value::Dict())
      : debug_data_type(debug_data_type),
        limit(std::move(limit)),
        additional_fields(std::move(additional_fields)) {}
};

template <typename T>
base::Value GetLimit(T limit) {
  return base::Value(base::NumberToString(limit));
}

std::optional<DebugDataTypeAndBody> GetReportDataBody(
    const StoreSourceResult& result) {
  base::Value::Dict additional_fields;
  if (result.destination_limit().has_value()) {
    additional_fields.Set("source_destination_limit",
                          GetLimit(result.destination_limit().value()));
  }

  const auto make_report_body = [&](DebugDataType type,
                                    base::Value limit = base::Value()) {
    return std::make_optional(DebugDataTypeAndBody(
        type, std::move(limit), std::move(additional_fields)));
  };

  return absl::visit(
      base::Overloaded{
          [](StoreSourceResult::ProhibitedByBrowserPolicy) {
            return std::optional<DebugDataTypeAndBody>();
          },
          [&](absl::variant<StoreSourceResult::Success,
                            // `kSourceSuccess` is sent for a few errors as well
                            // to mitigate the security concerns on reporting
                            // these errors. Because these errors are thrown
                            // based on information across reporting origins,
                            // reporting on them would violate the same-origin
                            // policy.
                            StoreSourceResult::ExcessiveReportingOrigins,
                            StoreSourceResult::DestinationGlobalLimitReached>) {
            return make_report_body(result.is_noised()
                                        ? DebugDataType::kSourceNoised
                                        : DebugDataType::kSourceSuccess);
          },
          [&](StoreSourceResult::InsufficientUniqueDestinationCapacity v) {
            return make_report_body(DebugDataType::kSourceDestinationLimit,
                                    GetLimit(v.limit));
          },
          [&](absl::variant<StoreSourceResult::DestinationReportingLimitReached,
                            StoreSourceResult::DestinationBothLimitsReached>
                  v) {
            return make_report_body(
                DebugDataType::kSourceDestinationRateLimit,
                absl::visit([](auto v) { return GetLimit(v.limit); }, v));
          },
          [&](StoreSourceResult::DestinationPerDayReportingLimitReached v) {
            return make_report_body(
                DebugDataType::kSourceDestinationPerDayRateLimit,
                GetLimit(v.limit));
          },
          [&](StoreSourceResult::InsufficientSourceCapacity v) {
            return make_report_body(DebugDataType::kSourceStorageLimit,
                                    GetLimit(v.limit));
          },
          [&](StoreSourceResult::InternalError) {
            return make_report_body(DebugDataType::kSourceUnknownError);
          },
          [&](StoreSourceResult::ReportingOriginsPerSiteLimitReached v) {
            return make_report_body(
                DebugDataType::kSourceReportingOriginPerSiteLimit,
                GetLimit(v.limit));
          },
          [&](StoreSourceResult::ExceedsMaxChannelCapacity v) {
            return make_report_body(DebugDataType::kSourceChannelCapacityLimit,
                                    base::Value(v.limit));
          },
          [&](StoreSourceResult::ExceedsMaxScopesChannelCapacity v) {
            return make_report_body(
                DebugDataType::kSourceScopesChannelCapacityLimit,
                base::Value(v.limit));
          },
          [&](StoreSourceResult::ExceedsMaxTriggerStateCardinality v) {
            return make_report_body(
                DebugDataType::kSourceTriggerStateCardinalityLimit,
                GetLimit(v.limit));
          },
          [&](StoreSourceResult::ExceedsMaxEventStatesLimit v) {
            return make_report_body(DebugDataType::kSourceMaxEventStatesLimit,
                                    GetLimit(v.limit));
          },
      },
      result.result());
}

std::optional<DebugDataTypeAndBody> GetReportDataTypeAndLimit(
    const CreateReportResult::EventLevel& result) {
  return absl::visit(
      base::Overloaded{
          [](const CreateReportResult::EventLevelSuccess&) {
            return std::optional<DebugDataTypeAndBody>();
          },
          [](CreateReportResult::ProhibitedByBrowserPolicy) {
            return std::optional<DebugDataTypeAndBody>();
          },
          [](CreateReportResult::NotRegistered) {
            return std::optional<DebugDataTypeAndBody>();
          },
          [](CreateReportResult::InternalError) {
            return std::make_optional(
                DebugDataTypeAndBody(DebugDataType::kTriggerUnknownError));
          },
          [](CreateReportResult::NoCapacityForConversionDestination v) {
            return std::make_optional(DebugDataTypeAndBody(
                DebugDataType::kTriggerEventStorageLimit, GetLimit(v.max)));
          },
          [](CreateReportResult::ExcessiveReportingOrigins v) {
            return std::make_optional(DebugDataTypeAndBody(
                DebugDataType::kTriggerReportingOriginLimit, GetLimit(v.max)));
          },
          [](CreateReportResult::NoMatchingImpressions) {
            return std::make_optional(
                DebugDataTypeAndBody(DebugDataType::kTriggerNoMatchingSource));
          },
          [](CreateReportResult::ExcessiveAttributions v) {
            return std::make_optional(DebugDataTypeAndBody(
                DebugDataType::
                    kTriggerEventAttributionsPerSourceDestinationLimit,
                GetLimit(v.max)));
          },
          [](CreateReportResult::NoMatchingSourceFilterData) {
            return std::make_optional(DebugDataTypeAndBody(
                DebugDataType::kTriggerNoMatchingFilterData));
          },
          [](CreateReportResult::Deduplicated) {
            return std::make_optional(
                DebugDataTypeAndBody(DebugDataType::kTriggerEventDeduplicated));
          },
          [](CreateReportResult::NoMatchingConfigurations) {
            return std::make_optional(DebugDataTypeAndBody(
                DebugDataType::kTriggerEventNoMatchingConfigurations));
          },
          [](CreateReportResult::NeverAttributedSource) {
            return std::make_optional(
                DebugDataTypeAndBody(DebugDataType::kTriggerEventNoise));
          },
          [](CreateReportResult::FalselyAttributedSource) {
            return std::make_optional(
                DebugDataTypeAndBody(DebugDataType::kTriggerEventNoise));
          },
          [](const CreateReportResult::PriorityTooLow&) {
            return std::make_optional(
                DebugDataTypeAndBody(DebugDataType::kTriggerEventLowPriority));
          },
          [](const CreateReportResult::ExcessiveEventLevelReports&) {
            return std::make_optional(DebugDataTypeAndBody(
                DebugDataType::kTriggerEventExcessiveReports));
          },
          [](CreateReportResult::ReportWindowNotStarted) {
            return std::make_optional(DebugDataTypeAndBody(
                DebugDataType::kTriggerEventReportWindowNotStarted));
          },
          [](CreateReportResult::ReportWindowPassed) {
            return std::make_optional(DebugDataTypeAndBody(
                DebugDataType::kTriggerEventReportWindowPassed));
          },
          [](CreateReportResult::NoMatchingTriggerData) {
            return std::make_optional(DebugDataTypeAndBody(
                DebugDataType::kTriggerEventNoMatchingTriggerData));
          },
      },
      result);
}

std::optional<DebugDataTypeAndBody> GetReportDataTypeAndLimit(
    const CreateReportResult::Aggregatable& result) {
  return absl::visit(
      base::Overloaded{
          [](const CreateReportResult::AggregatableSuccess&) {
            return std::optional<DebugDataTypeAndBody>();
          },
          [](CreateReportResult::NotRegistered) {
            return std::optional<DebugDataTypeAndBody>();
          },
          [](CreateReportResult::ProhibitedByBrowserPolicy) {
            return std::optional<DebugDataTypeAndBody>();
          },
          [](CreateReportResult::InternalError) {
            return std::make_optional(
                DebugDataTypeAndBody(DebugDataType::kTriggerUnknownError));
          },
          [](CreateReportResult::NoCapacityForConversionDestination v) {
            return std::make_optional(DebugDataTypeAndBody(
                DebugDataType::kTriggerAggregateStorageLimit, GetLimit(v.max)));
          },
          [](CreateReportResult::ExcessiveReportingOrigins v) {
            return std::make_optional(DebugDataTypeAndBody(
                DebugDataType::kTriggerReportingOriginLimit, GetLimit(v.max)));
          },
          [](CreateReportResult::NoMatchingImpressions) {
            return std::make_optional(
                DebugDataTypeAndBody(DebugDataType::kTriggerNoMatchingSource));
          },
          [](CreateReportResult::ExcessiveAttributions v) {
            return std::make_optional(DebugDataTypeAndBody(
                DebugDataType::
                    kTriggerAggregateAttributionsPerSourceDestinationLimit,
                GetLimit(v.max)));
          },
          [](CreateReportResult::NoMatchingSourceFilterData) {
            return std::make_optional(DebugDataTypeAndBody(
                DebugDataType::kTriggerNoMatchingFilterData));
          },
          [](CreateReportResult::Deduplicated) {
            return std::make_optional(DebugDataTypeAndBody(
                DebugDataType::kTriggerAggregateDeduplicated));
          },
          [](CreateReportResult::NoHistograms) {
            return std::make_optional(DebugDataTypeAndBody(
                DebugDataType::kTriggerAggregateNoContributions));
          },
          [](CreateReportResult::InsufficientBudget) {
            return std::make_optional(DebugDataTypeAndBody(
                DebugDataType::kTriggerAggregateInsufficientBudget,
                GetLimit(attribution_reporting::kMaxAggregatableValue)));
          },
          [](CreateReportResult::ReportWindowPassed) {
            return std::make_optional(DebugDataTypeAndBody(
                DebugDataType::kTriggerAggregateReportWindowPassed));
          },
          [](CreateReportResult::ExcessiveAggregatableReports v) {
            return std::make_optional(DebugDataTypeAndBody(
                DebugDataType::kTriggerAggregateExcessiveReports,
                GetLimit(v.max)));
          },
      },
      result);
}

void SetSourceData(base::Value::Dict& data_body,
                   uint64_t source_event_id,
                   const net::SchemefulSite& source_site,
                   std::optional<uint64_t> source_debug_key) {
  data_body.Set("source_event_id", base::NumberToString(source_event_id));
  data_body.Set("source_site", source_site.Serialize());
  if (source_debug_key) {
    data_body.Set("source_debug_key", base::NumberToString(*source_debug_key));
  }
}

void SetLimit(base::Value::Dict& data_body, base::Value limit) {
  data_body.Set("limit", std::move(limit));
}

base::Value::Dict GetReportDataBody(DebugDataTypeAndBody data,
                                    const CreateReportResult& result) {
  if (data.debug_data_type == DebugDataType::kTriggerEventExcessiveReports ||
      data.debug_data_type == DebugDataType::kTriggerEventLowPriority) {
    DCHECK(result.dropped_event_level_report());
    return result.dropped_event_level_report()->ReportBody();
  }

  base::Value::Dict data_body;
  data_body.Set(
      kAttributionDestination,
      net::SchemefulSite(result.trigger().destination_origin()).Serialize());
  if (std::optional<uint64_t> debug_key =
          result.trigger().registration().debug_key) {
    data_body.Set("trigger_debug_key", base::NumberToString(*debug_key));
  }

  if (const std::optional<StoredSource>& source = result.source()) {
    SetSourceData(data_body, source->source_event_id(),
                  source->common_info().source_site(), source->debug_key());
  }

  if (!data.limit.is_none()) {
    SetLimit(data_body, std::move(data.limit));
  }

  return data_body;
}

base::Value::Dict GetReportData(DebugDataType type, base::Value::Dict body) {
  base::Value::Dict dict;
  dict.Set("type", attribution_reporting::SerializeDebugDataType(type));
  dict.Set("body", std::move(body));
  return dict;
}

void RecordVerboseDebugReportType(DebugDataType type) {
  base::UmaHistogramEnumeration("Conversions.SentVerboseDebugReportType4",
                                type);
}

}  // namespace

GURL AttributionDebugReport::ReportUrl() const {
  static constexpr char kPath[] =
      "/.well-known/attribution-reporting/debug/verbose";

  GURL::Replacements replacements;
  replacements.SetPathStr(kPath);
  return reporting_origin_->GetURL().ReplaceComponents(replacements);
}

// static
std::optional<AttributionDebugReport> AttributionDebugReport::Create(
    base::FunctionRef<bool()> is_operation_allowed,
    const StoreSourceResult& result) {
  const StorableSource& source = result.source();
  if (!source.registration().debug_reporting ||
      !source.common_info().debug_cookie_set() ||
      source.is_within_fenced_frame() || !is_operation_allowed()) {
    return std::nullopt;
  }

  std::optional<DebugDataTypeAndBody> data = GetReportDataBody(result);
  if (!data) {
    return std::nullopt;
  }

  RecordVerboseDebugReportType(data->debug_data_type);

  base::Value::Dict body;
  if (!data->limit.is_none()) {
    SetLimit(body, std::move(data->limit));
  }

  const attribution_reporting::SourceRegistration& registration =
      source.registration();

  body.Set(kAttributionDestination, registration.destination_set.ToJson());
  SetSourceData(body, registration.source_event_id,
                source.common_info().source_site(), registration.debug_key);

  CHECK(base::ranges::none_of(data->additional_fields, [&](const auto& e) {
    return body.contains(e.first);
  }));
  body.Merge(std::move(data->additional_fields));

  base::Value::List report_body;
  report_body.Append(GetReportData(data->debug_data_type, std::move(body)));
  return AttributionDebugReport(std::move(report_body),
                                source.common_info().reporting_origin());
}

// static
std::optional<AttributionDebugReport> AttributionDebugReport::Create(
    base::FunctionRef<bool()> is_operation_allowed,
    bool is_debug_cookie_set,
    const CreateReportResult& result) {
  if (!result.trigger().registration().debug_reporting ||
      !is_debug_cookie_set || result.trigger().is_within_fenced_frame() ||
      !is_operation_allowed()) {
    return std::nullopt;
  }

  if (result.source() && !result.source()->common_info().debug_cookie_set()) {
    return std::nullopt;
  }

  base::Value::List report_body;

  std::optional<DebugDataType> event_level_type;
  if (std::optional<DebugDataTypeAndBody> event_level_data_type_limit =
          GetReportDataTypeAndLimit(result.event_level_result())) {
    event_level_type = event_level_data_type_limit->debug_data_type;
    report_body.Append(GetReportData(
        *event_level_type,
        GetReportDataBody(*std::move(event_level_data_type_limit), result)));
    RecordVerboseDebugReportType(*event_level_type);
  }

  if (std::optional<DebugDataTypeAndBody> aggregatable_data_type_limit =
          GetReportDataTypeAndLimit(result.aggregatable_result());
      aggregatable_data_type_limit &&
      aggregatable_data_type_limit->debug_data_type != event_level_type) {
    DebugDataType aggregatable_type =
        aggregatable_data_type_limit->debug_data_type;
    report_body.Append(GetReportData(
        aggregatable_type,
        GetReportDataBody(*std::move(aggregatable_data_type_limit), result)));
    RecordVerboseDebugReportType(aggregatable_type);
  }

  if (report_body.empty()) {
    return std::nullopt;
  }

  return AttributionDebugReport(std::move(report_body),
                                result.trigger().reporting_origin());
}

// static
std::optional<AttributionDebugReport> AttributionDebugReport::Create(
    const OsRegistration& registration,
    size_t item_index,
    base::FunctionRef<bool(const url::Origin&)> is_operation_allowed) {
  CHECK_LT(item_index, registration.registration_items.size());
  const auto& registration_item = registration.registration_items[item_index];
  if (!registration_item.debug_reporting ||
      registration.is_within_fenced_frame) {
    return std::nullopt;
  }

  auto registration_origin =
      attribution_reporting::SuitableOrigin::Create(registration_item.url);
  if (!registration_origin.has_value() ||
      !is_operation_allowed(*registration_origin)) {
    return std::nullopt;
  }

  DebugDataType data_type;
  switch (registration.GetType()) {
    case attribution_reporting::mojom::RegistrationType::kSource:
      data_type = DebugDataType::kOsSourceDelegated;
      break;
    case attribution_reporting::mojom::RegistrationType::kTrigger:
      data_type = DebugDataType::kOsTriggerDelegated;
      break;
  }

  base::Value::Dict data_body;
  data_body.Set("context_site",
                net::SchemefulSite(registration.top_level_origin).Serialize());
  data_body.Set("registration_url", registration_item.url.spec());

  base::Value::List report_body;
  report_body.Append(GetReportData(data_type, std::move(data_body)));

  RecordVerboseDebugReportType(data_type);

  return AttributionDebugReport(std::move(report_body),
                                *std::move(registration_origin));
}

std::optional<AttributionDebugReport> AttributionDebugReport::Create(
    attribution_reporting::SuitableOrigin reporting_origin,
    const attribution_reporting::RegistrationHeaderError& error,
    const attribution_reporting::SuitableOrigin& context_origin,
    bool is_within_fenced_frame,
    base::FunctionRef<bool(const url::Origin&)> is_operation_allowed) {
  if (is_within_fenced_frame || !is_operation_allowed(*reporting_origin)) {
    return std::nullopt;
  }

  base::Value::Dict data_body;
  data_body.Set("context_site", net::SchemefulSite(context_origin).Serialize());
  data_body.Set("header", error.HeaderName());
  data_body.Set("value", error.header_value);

  const DebugDataType data_type = DebugDataType::kHeaderParsingError;

  base::Value::List report_body;
  report_body.Append(GetReportData(data_type, std::move(data_body)));

  RecordVerboseDebugReportType(data_type);

  return AttributionDebugReport(std::move(report_body),
                                std::move(reporting_origin));
}

AttributionDebugReport::AttributionDebugReport(
    base::Value::List report_body,
    attribution_reporting::SuitableOrigin reporting_origin)
    : report_body_(std::move(report_body)),
      reporting_origin_(std::move(reporting_origin)) {
  DCHECK(!report_body_.empty());
}

AttributionDebugReport::~AttributionDebugReport() = default;

AttributionDebugReport::AttributionDebugReport(AttributionDebugReport&&) =
    default;

AttributionDebugReport& AttributionDebugReport::operator=(
    AttributionDebugReport&&) = default;

}  // namespace content
