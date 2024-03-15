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
#include "base/functional/function_ref.h"
#include "base/functional/overloaded.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "components/attribution_reporting/constants.h"
#include "components/attribution_reporting/destination_set.h"
#include "components/attribution_reporting/os_registration.h"
#include "components/attribution_reporting/registration_header_error.h"
#include "components/attribution_reporting/registration_header_type.mojom.h"
#include "components/attribution_reporting/source_registration.h"
#include "components/attribution_reporting/suitable_origin.h"
#include "components/attribution_reporting/trigger_registration.h"
#include "content/browser/attribution_reporting/attribution_constants.h"
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

using EventLevelResult = ::content::AttributionTrigger::EventLevelResult;
using AggregatableResult = ::content::AttributionTrigger::AggregatableResult;

using ::attribution_reporting::mojom::RegistrationHeaderType;

constexpr char kAttributionDestination[] = "attribution_destination";

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class DebugDataType {
  kSourceDestinationLimit = 0,
  kSourceNoised = 1,
  kSourceStorageLimit = 2,
  kSourceSuccess = 3,
  kSourceUnknownError = 4,
  kSourceDestinationRateLimit = 5,
  kTriggerNoMatchingSource = 6,
  kTriggerAttributionsPerSourceDestinationLimit = 7,
  kTriggerNoMatchingFilterData = 8,
  kTriggerReportingOriginLimit = 9,
  kTriggerEventDeduplicated = 10,
  kTriggerEventNoMatchingConfigurations = 11,
  kTriggerEventNoise = 12,
  kTriggerEventLowPriority = 13,
  kTriggerEventExcessiveReports = 14,
  kTriggerEventStorageLimit = 15,
  kTriggerEventReportWindowPassed = 16,
  kTriggerAggregateDeduplicated = 17,
  kTriggerAggregateNoContributions = 18,
  kTriggerAggregateInsufficientBudget = 19,
  kTriggerAggregateStorageLimit = 20,
  kTriggerAggregateReportWindowPassed = 21,
  kTriggerAggregateExcessiveReports = 22,
  kTriggerUnknownError = 23,
  kOsSourceDelegated = 24,
  kOsTriggerDelegated = 25,
  kTriggerEventReportWindowNotStarted = 26,
  kTriggerEventNoMatchingTriggerData = 27,
  kHeaderParsingError = 28,
  kMaxValue = kHeaderParsingError,
};

std::optional<DebugDataType> DataTypeIfCookieSet(DebugDataType data_type,
                                                 bool is_debug_cookie_set) {
  return is_debug_cookie_set ? std::make_optional(data_type) : std::nullopt;
}

struct DebugDataTypeAndBody {
  DebugDataType debug_data_type;
  int limit;

  explicit DebugDataTypeAndBody(DebugDataType debug_data_type, int limit = -1)
      : debug_data_type(debug_data_type), limit(limit) {}
};

std::optional<DebugDataTypeAndBody> GetReportDataBody(
    bool is_debug_cookie_set,
    const StoreSourceResult& result) {
  return absl::visit(
      base::Overloaded{
          [](absl::variant<StoreSourceResult::ProhibitedByBrowserPolicy,
                           StoreSourceResult::ExceedsMaxChannelCapacity>) {
            return std::optional<DebugDataTypeAndBody>();
          },
          [&](absl::variant<
              StoreSourceResult::Success,
              // `kSourceSuccess` is sent for a few errors as well to mitigate
              // the security concerns on reporting these errors. Because these
              // errors are thrown based on information across reporting
              // origins, reporting on them would violate the same-origin
              // policy.
              StoreSourceResult::ExcessiveReportingOrigins,
              StoreSourceResult::DestinationGlobalLimitReached,
              StoreSourceResult::ReportingOriginsPerSiteLimitReached>) {
            return is_debug_cookie_set
                       ? std::make_optional<DebugDataTypeAndBody>(
                             DebugDataType::kSourceSuccess)
                       : std::nullopt;
          },
          [](StoreSourceResult::InsufficientUniqueDestinationCapacity v) {
            return std::make_optional<DebugDataTypeAndBody>(
                DebugDataType::kSourceDestinationLimit, v.limit);
          },
          [](absl::variant<StoreSourceResult::DestinationReportingLimitReached,
                           StoreSourceResult::DestinationBothLimitsReached> v) {
            return std::make_optional<DebugDataTypeAndBody>(
                DebugDataType::kSourceDestinationRateLimit,
                absl::visit([](auto v) { return v.limit; }, v));
          },
          [&](StoreSourceResult::SuccessNoised) {
            return is_debug_cookie_set
                       ? std::make_optional<DebugDataTypeAndBody>(
                             DebugDataType::kSourceNoised)
                       : std::nullopt;
          },
          [&](StoreSourceResult::InsufficientSourceCapacity v) {
            return is_debug_cookie_set
                       ? std::make_optional<DebugDataTypeAndBody>(
                             DebugDataType::kSourceStorageLimit, v.limit)
                       : std::nullopt;
          },
          [&](StoreSourceResult::InternalError) {
            return is_debug_cookie_set
                       ? std::make_optional<DebugDataTypeAndBody>(
                             DebugDataType::kSourceUnknownError)
                       : std::nullopt;
          },
      },
      result.result());
}

std::optional<DebugDataType> GetReportDataType(EventLevelResult result,
                                               bool is_debug_cookie_set) {
  switch (result) {
    case EventLevelResult::kSuccess:
    case EventLevelResult::kProhibitedByBrowserPolicy:
    case EventLevelResult::kSuccessDroppedLowerPriority:
    case EventLevelResult::kNotRegistered:
      return std::nullopt;
    case EventLevelResult::kInternalError:
      return DataTypeIfCookieSet(DebugDataType::kTriggerUnknownError,
                                 is_debug_cookie_set);
    case EventLevelResult::kNoCapacityForConversionDestination:
      return DataTypeIfCookieSet(DebugDataType::kTriggerEventStorageLimit,
                                 is_debug_cookie_set);
    case EventLevelResult::kExcessiveReportingOrigins:
      return DataTypeIfCookieSet(DebugDataType::kTriggerReportingOriginLimit,
                                 is_debug_cookie_set);
    case EventLevelResult::kNoMatchingImpressions:
      return DataTypeIfCookieSet(DebugDataType::kTriggerNoMatchingSource,
                                 is_debug_cookie_set);
    case EventLevelResult::kExcessiveAttributions:
      return DataTypeIfCookieSet(
          DebugDataType::kTriggerAttributionsPerSourceDestinationLimit,
          is_debug_cookie_set);
    case EventLevelResult::kNoMatchingSourceFilterData:
      return DataTypeIfCookieSet(DebugDataType::kTriggerNoMatchingFilterData,
                                 is_debug_cookie_set);
    case EventLevelResult::kDeduplicated:
      return DataTypeIfCookieSet(DebugDataType::kTriggerEventDeduplicated,
                                 is_debug_cookie_set);
    case EventLevelResult::kNoMatchingConfigurations:
      return DataTypeIfCookieSet(
          DebugDataType::kTriggerEventNoMatchingConfigurations,
          is_debug_cookie_set);
    case EventLevelResult::kNeverAttributedSource:
    case EventLevelResult::kFalselyAttributedSource:
      return DataTypeIfCookieSet(DebugDataType::kTriggerEventNoise,
                                 is_debug_cookie_set);
    case EventLevelResult::kPriorityTooLow:
      return DataTypeIfCookieSet(DebugDataType::kTriggerEventLowPriority,
                                 is_debug_cookie_set);
    case EventLevelResult::kExcessiveReports:
      return DataTypeIfCookieSet(DebugDataType::kTriggerEventExcessiveReports,
                                 is_debug_cookie_set);
    case EventLevelResult::kReportWindowNotStarted:
      return DataTypeIfCookieSet(
          DebugDataType::kTriggerEventReportWindowNotStarted,
          is_debug_cookie_set);
    case EventLevelResult::kReportWindowPassed:
      return DataTypeIfCookieSet(DebugDataType::kTriggerEventReportWindowPassed,
                                 is_debug_cookie_set);
    case EventLevelResult::kNoMatchingTriggerData:
      return DataTypeIfCookieSet(
          DebugDataType::kTriggerEventNoMatchingTriggerData,
          is_debug_cookie_set);
  }
}

std::optional<DebugDataType> GetReportDataType(AggregatableResult result,
                                               bool is_debug_cookie_set) {
  switch (result) {
    case AggregatableResult::kSuccess:
    case AggregatableResult::kNotRegistered:
    case AggregatableResult::kProhibitedByBrowserPolicy:
      return std::nullopt;
    case AggregatableResult::kInternalError:
      return DataTypeIfCookieSet(DebugDataType::kTriggerUnknownError,
                                 is_debug_cookie_set);
    case AggregatableResult::kNoCapacityForConversionDestination:
      return DataTypeIfCookieSet(DebugDataType::kTriggerAggregateStorageLimit,
                                 is_debug_cookie_set);
    case AggregatableResult::kExcessiveReportingOrigins:
      return DataTypeIfCookieSet(DebugDataType::kTriggerReportingOriginLimit,
                                 is_debug_cookie_set);
    case AggregatableResult::kNoMatchingImpressions:
      return DataTypeIfCookieSet(DebugDataType::kTriggerNoMatchingSource,
                                 is_debug_cookie_set);
    case AggregatableResult::kExcessiveAttributions:
      return DataTypeIfCookieSet(
          DebugDataType::kTriggerAttributionsPerSourceDestinationLimit,
          is_debug_cookie_set);
    case AggregatableResult::kNoMatchingSourceFilterData:
      return DataTypeIfCookieSet(DebugDataType::kTriggerNoMatchingFilterData,
                                 is_debug_cookie_set);
    case AggregatableResult::kDeduplicated:
      return DataTypeIfCookieSet(DebugDataType::kTriggerAggregateDeduplicated,
                                 is_debug_cookie_set);
    case AggregatableResult::kNoHistograms:
      return DataTypeIfCookieSet(
          DebugDataType::kTriggerAggregateNoContributions, is_debug_cookie_set);
    case AggregatableResult::kInsufficientBudget:
      return DataTypeIfCookieSet(
          DebugDataType::kTriggerAggregateInsufficientBudget,
          is_debug_cookie_set);
    case AggregatableResult::kReportWindowPassed:
      return DataTypeIfCookieSet(
          DebugDataType::kTriggerAggregateReportWindowPassed,
          is_debug_cookie_set);
    case AggregatableResult::kExcessiveReports:
      return DataTypeIfCookieSet(
          DebugDataType::kTriggerAggregateExcessiveReports,
          is_debug_cookie_set);
  }
}

std::string_view SerializeReportDataType(DebugDataType data_type) {
  switch (data_type) {
    case DebugDataType::kSourceDestinationLimit:
      return "source-destination-limit";
    case DebugDataType::kSourceNoised:
      return "source-noised";
    case DebugDataType::kSourceStorageLimit:
      return "source-storage-limit";
    case DebugDataType::kSourceSuccess:
      return "source-success";
    case DebugDataType::kSourceDestinationRateLimit:
      return "source-destination-rate-limit";
    case DebugDataType::kSourceUnknownError:
      return "source-unknown-error";
    case DebugDataType::kTriggerNoMatchingSource:
      return "trigger-no-matching-source";
    case DebugDataType::kTriggerAttributionsPerSourceDestinationLimit:
      return "trigger-attributions-per-source-destination-limit";
    case DebugDataType::kTriggerNoMatchingFilterData:
      return "trigger-no-matching-filter-data";
    case DebugDataType::kTriggerReportingOriginLimit:
      return "trigger-reporting-origin-limit";
    case DebugDataType::kTriggerEventDeduplicated:
      return "trigger-event-deduplicated";
    case DebugDataType::kTriggerEventNoMatchingConfigurations:
      return "trigger-event-no-matching-configurations";
    case DebugDataType::kTriggerEventNoise:
      return "trigger-event-noise";
    case DebugDataType::kTriggerEventLowPriority:
      return "trigger-event-low-priority";
    case DebugDataType::kTriggerEventExcessiveReports:
      return "trigger-event-excessive-reports";
    case DebugDataType::kTriggerEventStorageLimit:
      return "trigger-event-storage-limit";
    case DebugDataType::kTriggerEventReportWindowNotStarted:
      return "trigger-event-report-window-not-started";
    case DebugDataType::kTriggerEventReportWindowPassed:
      return "trigger-event-report-window-passed";
    case DebugDataType::kTriggerEventNoMatchingTriggerData:
      return "trigger-event-no-matching-trigger-data";
    case DebugDataType::kTriggerAggregateDeduplicated:
      return "trigger-aggregate-deduplicated";
    case DebugDataType::kTriggerAggregateNoContributions:
      return "trigger-aggregate-no-contributions";
    case DebugDataType::kTriggerAggregateInsufficientBudget:
      return "trigger-aggregate-insufficient-budget";
    case DebugDataType::kTriggerAggregateStorageLimit:
      return "trigger-aggregate-storage-limit";
    case DebugDataType::kTriggerAggregateReportWindowPassed:
      return "trigger-aggregate-report-window-passed";
    case DebugDataType::kTriggerAggregateExcessiveReports:
      return "trigger-aggregate-excessive-reports";
    case DebugDataType::kTriggerUnknownError:
      return "trigger-unknown-error";
    case DebugDataType::kOsSourceDelegated:
      return "os-source-delegated";
    case DebugDataType::kOsTriggerDelegated:
      return "os-trigger-delegated";
    case DebugDataType::kHeaderParsingError:
      return "header-parsing-error";
  }
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

template <typename T>
void SetLimit(base::Value::Dict& data_body, T limit) {
  data_body.Set("limit", base::NumberToString(limit));
}

template <typename T>
void SetLimit(base::Value::Dict& data_body, std::optional<T> limit) {
  DCHECK(limit.has_value());
  SetLimit(data_body, *limit);
}

base::Value::Dict GetReportDataBody(DebugDataType data_type,
                                    const CreateReportResult& result) {
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

  switch (data_type) {
    case DebugDataType::kTriggerNoMatchingSource:
    case DebugDataType::kTriggerNoMatchingFilterData:
    case DebugDataType::kTriggerEventDeduplicated:
    case DebugDataType::kTriggerEventNoMatchingConfigurations:
    case DebugDataType::kTriggerEventNoise:
    case DebugDataType::kTriggerEventReportWindowNotStarted:
    case DebugDataType::kTriggerEventReportWindowPassed:
    case DebugDataType::kTriggerEventNoMatchingTriggerData:
    case DebugDataType::kTriggerAggregateDeduplicated:
    case DebugDataType::kTriggerAggregateNoContributions:
    case DebugDataType::kTriggerAggregateReportWindowPassed:
    case DebugDataType::kTriggerUnknownError:
      break;
    case DebugDataType::kTriggerAttributionsPerSourceDestinationLimit:
      SetLimit(data_body, result.limits().rate_limits_max_attributions);
      break;
    case DebugDataType::kTriggerAggregateInsufficientBudget:
      SetLimit<int>(data_body, attribution_reporting::kMaxAggregatableValue);
      break;
    case DebugDataType::kTriggerAggregateExcessiveReports:
      SetLimit(data_body, result.limits().max_aggregatable_reports_per_source);
      break;
    case DebugDataType::kTriggerReportingOriginLimit:
      SetLimit(data_body,
               result.limits().rate_limits_max_attribution_reporting_origins);
      break;
    case DebugDataType::kTriggerEventStorageLimit:
      SetLimit(data_body,
               result.limits().max_event_level_reports_per_destination);
      break;
    case DebugDataType::kTriggerAggregateStorageLimit:
      SetLimit(data_body,
               result.limits().max_aggregatable_reports_per_destination);
      break;
    case DebugDataType::kTriggerEventLowPriority:
    case DebugDataType::kTriggerEventExcessiveReports:
      DCHECK(result.dropped_event_level_report());
      return result.dropped_event_level_report()->ReportBody();
    case DebugDataType::kSourceDestinationLimit:
    case DebugDataType::kSourceNoised:
    case DebugDataType::kSourceStorageLimit:
    case DebugDataType::kSourceSuccess:
    case DebugDataType::kSourceUnknownError:
    case DebugDataType::kSourceDestinationRateLimit:
    case DebugDataType::kOsSourceDelegated:
    case DebugDataType::kOsTriggerDelegated:
    case DebugDataType::kHeaderParsingError:
      NOTREACHED_NORETURN();
  }

  return data_body;
}

base::Value::Dict GetReportData(DebugDataType type, base::Value::Dict body) {
  base::Value::Dict dict;
  dict.Set("type", SerializeReportDataType(type));
  dict.Set("body", std::move(body));
  return dict;
}

void RecordVerboseDebugReportType(DebugDataType type) {
  static_assert(DebugDataType::kMaxValue == DebugDataType::kHeaderParsingError,
                "Update ConversionVerboseDebugReportType enum.");
  base::UmaHistogramEnumeration("Conversions.SentVerboseDebugReportType4",
                                type);
}

std::string_view GetHeaderName(RegistrationHeaderType type) {
  switch (type) {
    case RegistrationHeaderType::kSource:
      return kAttributionReportingRegisterSourceHeader;
    case RegistrationHeaderType::kTrigger:
      return kAttributionReportingRegisterTriggerHeader;
    case RegistrationHeaderType::kOsSource:
      return kAttributionReportingRegisterOsSourceHeader;
    case RegistrationHeaderType::kOsTrigger:
      return kAttributionReportingRegisterOsTriggerHeader;
    default:
      // Should only be possible with compromised renderers.
      return "";
  }
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
    const StorableSource& source,
    base::FunctionRef<bool()> is_operation_allowed,
    bool is_debug_cookie_set,
    const StoreSourceResult& result) {
  if (!source.registration().debug_reporting ||
      source.is_within_fenced_frame() || !is_operation_allowed()) {
    return std::nullopt;
  }

  std::optional<DebugDataTypeAndBody> data =
      GetReportDataBody(is_debug_cookie_set, result);
  if (!data) {
    return std::nullopt;
  }

  RecordVerboseDebugReportType(data->debug_data_type);

  base::Value::Dict body;
  if (data->limit >= 0) {
    SetLimit(body, data->limit);
  }

  const attribution_reporting::SourceRegistration& registration =
      source.registration();

  body.Set(kAttributionDestination, registration.destination_set.ToJson());
  SetSourceData(body, registration.source_event_id,
                source.common_info().source_site(), registration.debug_key);

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
      result.trigger().is_within_fenced_frame() || !is_operation_allowed()) {
    return std::nullopt;
  }

  if (is_debug_cookie_set && result.source()) {
    is_debug_cookie_set = result.source()->debug_cookie_set();
  }

  base::Value::List report_body;

  std::optional<DebugDataType> event_level_data_type =
      GetReportDataType(result.event_level_status(), is_debug_cookie_set);
  if (event_level_data_type) {
    report_body.Append(
        GetReportData(*event_level_data_type,
                      GetReportDataBody(*event_level_data_type, result)));
    RecordVerboseDebugReportType(*event_level_data_type);
  }

  if (std::optional<DebugDataType> aggregatable_data_type =
          GetReportDataType(result.aggregatable_status(), is_debug_cookie_set);
      aggregatable_data_type &&
      aggregatable_data_type != event_level_data_type) {
    report_body.Append(
        GetReportData(*aggregatable_data_type,
                      GetReportDataBody(*aggregatable_data_type, result)));
    RecordVerboseDebugReportType(*aggregatable_data_type);
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
                                std::move(*registration_origin));
}

std::optional<AttributionDebugReport> AttributionDebugReport::Create(
    attribution_reporting::SuitableOrigin reporting_origin,
    const attribution_reporting::RegistrationHeaderError& error,
    const attribution_reporting::SuitableOrigin& context_origin,
    bool is_within_fenced_frame,
    base::FunctionRef<bool(const url::Origin&)> is_operation_allowed) {
  if (is_within_fenced_frame) {
    return std::nullopt;
  }

  std::string_view header_type = GetHeaderName(error.header_type);
  if (header_type.empty() || !is_operation_allowed(*reporting_origin)) {
    return std::nullopt;
  }

  base::Value::Dict data_body;
  data_body.Set("context_site", net::SchemefulSite(context_origin).Serialize());
  data_body.Set("header", header_type);
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
