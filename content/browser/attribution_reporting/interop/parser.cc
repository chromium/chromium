// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/interop/parser.h"

#include <stddef.h>
#include <stdint.h>

#include <optional>
#include <ostream>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/functional/function_ref.h"
#include "base/functional/overloaded.h"
#include "base/json/json_writer.h"
#include "base/memory/raw_ref.h"
#include "base/memory/scoped_refptr.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/abseil_string_number_conversions.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "base/types/expected_macros.h"
#include "base/values.h"
#include "components/attribution_reporting/parsing_utils.h"
#include "components/attribution_reporting/privacy_math.h"
#include "components/attribution_reporting/suitable_origin.h"
#include "components/attribution_reporting/test_utils.h"
#include "content/browser/attribution_reporting/attribution_config.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_version.h"
#include "net/http/structured_headers.h"
#include "services/network/public/mojom/attribution.mojom.h"
#include "third_party/abseil-cpp/absl/numeric/int128.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "url/gurl.h"

namespace content {

namespace {

using ::attribution_reporting::SuitableOrigin;
using ::network::mojom::AttributionReportingEligibility;

constexpr char kEligibleKey[] = "Attribution-Reporting-Eligible";
constexpr char kPayloadKey[] = "payload";
constexpr char kRegistrationRequestKey[] = "registration_request";
constexpr char kReportTimeKey[] = "report_time";
constexpr char kReportUrlKey[] = "report_url";
constexpr char kReportsKey[] = "reports";
constexpr char kResponseKey[] = "response";
constexpr char kResponsesKey[] = "responses";
constexpr char kTimestampKey[] = "timestamp";

using Context = absl::variant<std::string_view, size_t>;
using ContextPath = std::vector<Context>;

std::string TimeAsUnixMillisecondString(base::Time time) {
  return base::NumberToString(
      (time - base::Time::UnixEpoch()).InMilliseconds());
}

class ScopedContext {
 public:
  ScopedContext(ContextPath& path, Context context) : path_(path) {
    path_->push_back(context);
  }

  ~ScopedContext() { path_->pop_back(); }

  ScopedContext(const ScopedContext&) = delete;
  ScopedContext(ScopedContext&&) = delete;

  ScopedContext& operator=(const ScopedContext&) = delete;
  ScopedContext& operator=(ScopedContext&&) = delete;

 private:
  const raw_ref<ContextPath> path_;
};

std::ostream& operator<<(std::ostream& out, const ContextPath& path) {
  if (path.empty()) {
    return out << "input root";
  }

  for (Context context : path) {
    absl::visit(base::Overloaded{
                    [&](std::string_view key) { out << "[\"" << key << "\"]"; },
                    [&](size_t index) { out << '[' << index << ']'; },
                },
                context);
  }
  return out;
}

// Writes a newline on destruction.
class ErrorWriter {
 public:
  explicit ErrorWriter(std::ostringstream& stream) : stream_(stream) {}

  ~ErrorWriter() { *stream_ << std::endl; }

  ErrorWriter(const ErrorWriter&) = delete;
  ErrorWriter(ErrorWriter&&) = default;

  ErrorWriter& operator=(const ErrorWriter&) = delete;
  ErrorWriter& operator=(ErrorWriter&&) = delete;

  std::ostringstream& operator*() { return *stream_; }

 private:
  const raw_ref<std::ostringstream> stream_;
};

class AttributionInteropParser {
 public:
  AttributionInteropParser() = default;

  ~AttributionInteropParser() = default;

  AttributionInteropParser(const AttributionInteropParser&) = delete;
  AttributionInteropParser(AttributionInteropParser&&) = delete;

  AttributionInteropParser& operator=(const AttributionInteropParser&) = delete;
  AttributionInteropParser& operator=(AttributionInteropParser&&) = delete;

  base::expected<std::vector<AttributionSimulationEvent>, std::string>
  ParseInput(base::Value::Dict input) && {
    std::vector<AttributionSimulationEvent> events;

    static constexpr char kKeyRegistrations[] = "registrations";
    if (base::Value* registrations = input.Find(kKeyRegistrations)) {
      auto context = PushContext(kKeyRegistrations);
      int64_t request_id = 0;
      ParseListOfDicts(registrations, [&](base::Value::Dict registration) {
        ParseRegistration(std::move(registration), events, request_id++);
      });
    }

    if (has_error_) {
      return base::unexpected(error_stream_.str());
    }

    return events;
  }

  base::expected<AttributionInteropOutput, std::string> ParseOutput(
      base::Value::Dict dict) && {
    AttributionInteropOutput output;

    {
      auto context = PushContext(kReportsKey);
      ParseListOfDicts(dict.Find(kReportsKey), [&](base::Value::Dict report) {
        ParseReport(std::move(report), output.reports);
      });
    }

    if (has_error_) {
      return base::unexpected(error_stream_.str());
    }
    return output;
  }

  base::expected<void, std::string> ParseConfig(
      base::Value::Dict& dict,
      AttributionInteropConfig& interop_config,
      bool required) && {
    interop_config.needs_cross_app_web =
        ParseBool(dict, "needs_cross_app_web").value_or(false);
    interop_config.needs_aggregatable_debug =
        ParseBool(dict, "needs_aggregatable_debug").value_or(false);
    interop_config.needs_source_destination_limit =
        ParseBool(dict, "needs_source_destination_limit").value_or(false);
    interop_config.needs_aggregatable_filtering_ids =
        ParseBool(dict, "needs_aggregatable_filtering_ids").value_or(false);
    interop_config.needs_attribution_scopes =
        ParseBool(dict, "needs_attribution_scopes").value_or(false);

    AttributionConfig& config = interop_config.attribution_config;

    ParseInt(dict, "max_sources_per_origin", config.max_sources_per_origin,
             required);

    ParseInt(dict, "max_destinations_per_source_site_reporting_site",
             config.max_destinations_per_source_site_reporting_site, required);

    ParseInt(dict, "max_destinations_per_rate_limit_window_reporting_site",
             config.destination_rate_limit.max_per_reporting_site, required);

    ParseInt(dict, "max_destinations_per_rate_limit_window",
             config.destination_rate_limit.max_total, required);

    int destination_rate_limit_window_in_minutes;
    if (ParseInt(dict, "destination_rate_limit_window_in_minutes",
                 destination_rate_limit_window_in_minutes, required)) {
      config.destination_rate_limit.rate_limit_window =
          base::Minutes(destination_rate_limit_window_in_minutes);
    }

    ParseInt(dict, "max_destinations_per_reporting_site_per_day",
             config.destination_rate_limit.max_per_reporting_site_per_day,
             required);

    ParseDouble(dict, "max_event_level_channel_capacity_navigation",
                config.privacy_math_config.max_channel_capacity_navigation,
                required);
    ParseDouble(dict, "max_event_level_channel_capacity_event",
                config.privacy_math_config.max_channel_capacity_event,
                required);
    ParseDouble(
        dict, "max_event_level_channel_capacity_scopes_navigation",
        config.privacy_math_config.max_channel_capacity_scopes_navigation,
        required);
    ParseDouble(dict, "max_event_level_channel_capacity_scopes_event",
                config.privacy_math_config.max_channel_capacity_scopes_event,
                required);

    ParseUInt32(dict, "max_trigger_state_cardinality",
                interop_config.max_trigger_state_cardinality, required);

    int rate_limit_time_window_in_days;
    if (ParseInt(dict, "rate_limit_time_window_in_days",
                 rate_limit_time_window_in_days, required)) {
      config.rate_limit.time_window =
          base::Days(rate_limit_time_window_in_days);
    }

    ParseInt64(dict, "rate_limit_max_source_registration_reporting_origins",
               config.rate_limit.max_source_registration_reporting_origins,
               required);
    ParseInt64(dict, "rate_limit_max_attribution_reporting_origins",
               config.rate_limit.max_attribution_reporting_origins, required);
    ParseInt64(dict, "rate_limit_max_attributions",
               config.rate_limit.max_attributions, required);
    ParseInt(dict, "rate_limit_max_reporting_origins_per_source_reporting_site",
             config.rate_limit.max_reporting_origins_per_source_reporting_site,
             required);

    int rate_limit_origins_per_site_window_in_days;
    if (ParseInt(dict, "rate_limit_origins_per_site_window_in_days",
                 rate_limit_origins_per_site_window_in_days, required)) {
      config.rate_limit.origins_per_site_window =
          base::Days(rate_limit_origins_per_site_window_in_days);
    }

    ParseInt(dict, "max_event_level_reports_per_destination",
             config.event_level_limit.max_reports_per_destination, required);
    ParseDouble(dict, "max_settable_event_level_epsilon",
                interop_config.max_event_level_epsilon, required);
    ParseInt(dict, "max_aggregatable_reports_per_destination",
             config.aggregate_limit.max_reports_per_destination, required);

    int aggregatable_report_min_delay;
    if (ParseInt(dict, "aggregatable_report_min_delay",
                 aggregatable_report_min_delay, required,
                 /*allow_zero=*/true)) {
      config.aggregate_limit.min_delay =
          base::Minutes(aggregatable_report_min_delay);
    }

    int aggregatable_report_delay_span;
    if (ParseInt(dict, "aggregatable_report_delay_span",
                 aggregatable_report_delay_span, required,
                 /*allow_zero=*/true)) {
      config.aggregate_limit.delay_span =
          base::Minutes(aggregatable_report_delay_span);
    }

    int max_aggregatable_debug_budget_per_context_site;
    if (ParseInt(dict, "max_aggregatable_debug_budget_per_context_site",
                 max_aggregatable_debug_budget_per_context_site, required,
                 /*allow_zero=*/false)) {
      config.aggregatable_debug_rate_limit.max_budget_per_context_site =
          max_aggregatable_debug_budget_per_context_site;
    }

    int max_aggregatable_debug_reports_per_source;
    if (ParseInt(dict, "max_aggregatable_debug_reports_per_source",
                 max_aggregatable_debug_reports_per_source, required,
                 /*allow_zero=*/false)) {
      config.aggregatable_debug_rate_limit.max_reports_per_source =
          max_aggregatable_debug_reports_per_source;
    }

    {
      static constexpr char kAggregationCoordinatorOrigins[] =
          "aggregation_coordinator_origins";
      auto context = PushContext(kAggregationCoordinatorOrigins);
      base::Value* values = dict.Find(kAggregationCoordinatorOrigins);
      if (values) {
        // Ensure that the list is replaced, not unioned, when being merged.
        interop_config.aggregation_coordinator_origins.clear();
      }

      ParseList(
          values,
          [&](base::Value v) {
            if (std::optional<SuitableOrigin> origin = ParseOrigin(&v)) {
              interop_config.aggregation_coordinator_origins.emplace_back(
                  *std::move(origin));
            }
          },
          required,
          /*allow_empty=*/false);
    }

    if (has_error_) {
      return base::unexpected(error_stream_.str());
    }
    return base::ok();
  }

 private:
  std::ostringstream error_stream_;

  ContextPath context_path_;
  bool has_error_ = false;

  [[nodiscard]] ScopedContext PushContext(Context context) {
    return ScopedContext(context_path_, context);
  }

  ErrorWriter Error() {
    has_error_ = true;
    error_stream_ << context_path_ << ": ";
    return ErrorWriter(error_stream_);
  }

  void ParseList(base::Value* values,
                 base::FunctionRef<void(base::Value)> parse_element,
                 bool required = true,
                 bool allow_empty = true) {
    if (!values) {
      if (required) {
        *Error() << "must be present";
      }
      return;
    }

    base::Value::List* list = values->GetIfList();
    if (!list) {
      *Error() << "must be a list";
      return;
    }

    if (list->empty() && !allow_empty) {
      *Error() << "must be non-empty";
    }

    for (size_t index = 0; auto& value : *list) {
      auto index_context = PushContext(index);
      parse_element(std::move(value));
      index++;
    }
  }

  void ParseListOfDicts(
      base::Value* values,
      base::FunctionRef<void(base::Value::Dict)> parse_element) {
    ParseList(values, [&](base::Value value) {
      if (!EnsureDictionary(&value)) {
        return;
      }
      parse_element(std::move(value).TakeDict());
    });
  }

  void ParseRegistration(base::Value::Dict dict,
                         std::vector<AttributionSimulationEvent>& events,
                         int64_t request_id) {
    const base::Time time =
        ParseTime(dict, kTimestampKey,
                  /*previous_time=*/events.empty() ? base::Time::Min()
                                                   : events.back().time,
                  /*strictly_greater=*/true);

    std::optional<SuitableOrigin> context_origin;
    AttributionReportingEligibility eligibility;
    bool fenced = false;

    ParseDict(dict, kRegistrationRequestKey, [&](base::Value::Dict reg_req) {
      context_origin = ParseOrigin(reg_req, "context_origin");
      eligibility = ParseEligibility(reg_req);
      fenced = ParseBool(reg_req, "fenced").value_or(false);
    });

    if (has_error_) {
      return;
    }

    events.emplace_back(
        time, AttributionSimulationEvent::StartRequest(
                  request_id, *std::move(context_origin), eligibility, fenced));

    std::optional<base::Time> default_response_time = time;

    {
      auto context = PushContext(kResponsesKey);
      ParseListOfDicts(
          dict.Find(kResponsesKey), [&](base::Value::Dict response) {
            auto url = ParseUrl(response, "url");

            const bool debug_permission = ParseDebugPermission(response);

            attribution_reporting::RandomizedResponse randomized_response =
                ParseRandomizedResponse(response);

            base::flat_set<int> null_aggregatable_reports_days =
                ParseNullAggregatableReportsDays(response);

            // The timestamp is required for all but the first response. If
            // omitted on the first response, it defaults to the registration
            // time.
            base::Time response_time = ParseTime(
                response, kTimestampKey, events.back().time,
                /*strictly_greater=*/!default_response_time.has_value(),
                default_response_time);
            default_response_time = std::nullopt;

            if (has_error_) {
              return;
            }

            ParseDict(
                response, kResponseKey, [&](base::Value::Dict response_dict) {
                  net::HttpResponseHeaders::Builder builder(
                      net::HttpVersion(1, 1),
                      /*status=*/"200 OK");

                  for (auto [header, value] : response_dict) {
                    if (const std::string* str = value.GetIfString()) {
                      builder.AddHeader(header, *str);
                    } else {
                      std::optional<std::string> json = base::WriteJson(value);
                      CHECK(json.has_value());
                      // The string must outlive the call to
                      // `net::HttpResponseHeaders::Build()`, so put it back in
                      // the dict.
                      value = base::Value(*std::move(json));
                      builder.AddHeader(header, value.GetString());
                    }
                  }

                  events.emplace_back(
                      response_time,
                      AttributionSimulationEvent::Response(
                          request_id, std::move(url), builder.Build(),
                          std::move(randomized_response),
                          std::move(null_aggregatable_reports_days),
                          debug_permission));

                });
          });
    }

    // The request ends at the time of the last response, if any; otherwise it
    // ends at the registration time.
    events.emplace_back(events.back().time,
                        AttributionSimulationEvent::EndRequest(request_id));
  }

  void ParseReport(base::Value::Dict dict,
                   std::vector<AttributionInteropOutput::Report>& reports) {
    AttributionInteropOutput::Report report;

    report.time =
        ParseTime(dict, kReportTimeKey,
                  /*previous_time=*/reports.empty() ? base::Time::Min()
                                                    : reports.back().time,
                  /*strictly_greater=*/false);

    if (const std::string* url = dict.FindString(kReportUrlKey)) {
      report.url = GURL(*url);
    }
    if (!report.url.is_valid()) {
      auto context = PushContext(kReportUrlKey);
      *Error() << "must be a valid URL";
    }

    if (std::optional<base::Value> payload = dict.Extract(kPayloadKey)) {
      report.payload = *std::move(payload);
    } else {
      auto context = PushContext(kPayloadKey);
      *Error() << "required";
    }

    if (!has_error_) {
      reports.push_back(std::move(report));
    }
  }

  std::optional<SuitableOrigin> ParseOrigin(const base::Value::Dict& dict,
                                            std::string_view key) {
    auto context = PushContext(key);
    return ParseOrigin(dict.Find(key));
  }

  std::optional<SuitableOrigin> ParseOrigin(const base::Value* v) {
    std::optional<SuitableOrigin> origin;
    if (v) {
      if (const std::string* s = v->GetIfString()) {
        origin = SuitableOrigin::Deserialize(*s);
      }
    }

    if (!origin.has_value()) {
      *Error() << "must be a valid, secure origin";
    }

    return origin;
  }

  GURL ParseUrl(const base::Value::Dict& dict, std::string_view key) {
    auto context = PushContext(key);

    GURL url;
    if (const std::string* s = dict.FindString(key)) {
      url = GURL(*s);
    }

    if (!url.is_valid()) {
      *Error() << "must be a valid URL";
    }

    return url;
  }

  base::Time ParseTime(const base::Value::Dict& dict,
                       std::string_view key,
                       base::Time previous_time,
                       bool strictly_greater,
                       std::optional<base::Time> if_absent = std::nullopt) {
    auto context = PushContext(key);
    base::Time time;

    if (const std::string* v = dict.FindString(key)) {
      if (int64_t milliseconds; base::StringToInt64(*v, &milliseconds)) {
        time = base::Time::UnixEpoch() + base::Milliseconds(milliseconds);
      }
    } else if (if_absent.has_value()) {
      time = *if_absent;
    }

    if (!time.is_null() && !time.is_inf()) {
      if (strictly_greater && time <= previous_time) {
        *Error() << "must be greater than previous time";
      } else if (!strictly_greater && time < previous_time) {
        *Error() << "must be greater than or equal to previous time";
      }
      return time;
    }

    *Error() << "must be an integer number of milliseconds since the Unix "
                "epoch formatted as a base-10 string";
    return base::Time();
  }

  std::optional<bool> ParseBool(const base::Value::Dict& dict,
                                std::string_view key) {
    auto context = PushContext(key);

    const base::Value* v = dict.Find(key);
    if (!v) {
      return std::nullopt;
    }

    if (!v->is_bool()) {
      *Error() << "must be a bool";
      return std::nullopt;
    }

    return v->GetBool();
  }

  bool ParseDebugPermission(const base::Value::Dict& dict) {
    return ParseBool(dict, "debug_permission").value_or(false);
  }

  // TODO(apaseltiner): Consider moving this for general use to
  // services/network/attribution/request_headers_internal.h.
  AttributionReportingEligibility ParseEligibility(
      const base::Value::Dict& dict) {
    static constexpr char kNavigationSource[] = "navigation-source";
    static constexpr char kEventSource[] = "event-source";
    static constexpr char kTrigger[] = "trigger";

    const std::string* v = dict.FindString(kEligibleKey);
    if (!v) {
      return AttributionReportingEligibility::kUnset;
    }

    auto context = PushContext(kEligibleKey);

    auto structured_dict = net::structured_headers::ParseDictionary(*v);
    if (!structured_dict.has_value()) {
      *Error() << "must be a structured dictionary";
      return AttributionReportingEligibility::kEmpty;
    }

    const bool navigation_source = structured_dict->contains(kNavigationSource);
    const bool event_source = structured_dict->contains(kEventSource);
    const bool trigger = structured_dict->contains(kTrigger);

    if (navigation_source && (event_source || trigger)) {
      *Error() << kNavigationSource << " is mutually exclusive with "
               << kEventSource << " and " << kTrigger;
      return AttributionReportingEligibility::kEmpty;
    }

    if (event_source && trigger) {
      return AttributionReportingEligibility::kEventSourceOrTrigger;
    }
    if (event_source) {
      return AttributionReportingEligibility::kEventSource;
    }
    if (trigger) {
      return AttributionReportingEligibility::kTrigger;
    }
    if (navigation_source) {
      return AttributionReportingEligibility::kNavigationSource;
    }
    return AttributionReportingEligibility::kEmpty;
  }

  void ParseFakeReport(
      const base::Value::Dict& dict,
      std::vector<attribution_reporting::FakeEventLevelReport>& fake_reports) {
    std::optional<uint32_t> trigger_data;
    {
      static constexpr char kTriggerData[] = "trigger_data";
      auto context = PushContext(kTriggerData);
      if (const base::Value* v = dict.Find(kTriggerData)) {
        auto result = attribution_reporting::ParseUint32(*v);
        if (result.has_value()) {
          trigger_data = *result;
        }
      }

      if (!trigger_data.has_value()) {
        *Error() << "must be a uint32";
      }
    }

    int report_window_index = -1;
    {
      static constexpr char kReportWindowIndex[] = "report_window_index";
      auto context = PushContext(kReportWindowIndex);
      report_window_index = dict.FindInt(kReportWindowIndex).value_or(-1);
      if (report_window_index < 0) {
        *Error() << "must be a non-negative integer";
      }
    }

    if (!has_error_) {
      fake_reports.emplace_back(*trigger_data, report_window_index);
    }
  }

  attribution_reporting::RandomizedResponse ParseRandomizedResponse(
      base::Value::Dict& dict) {
    attribution_reporting::RandomizedResponse randomized_response;

    static constexpr char kRandomizedResponse[] = "randomized_response";
    if (base::Value* v = dict.Find(kRandomizedResponse); v && !v->is_none()) {
      auto context = PushContext(kRandomizedResponse);
      std::vector<attribution_reporting::FakeEventLevelReport>& fake_reports =
          randomized_response.emplace();
      ParseListOfDicts(v, [&](base::Value::Dict dict) {
        ParseFakeReport(dict, fake_reports);
      });
    }

    return randomized_response;
  }

  base::flat_set<int> ParseNullAggregatableReportsDays(
      base::Value::Dict& dict) {
    base::flat_set<int> null_aggregatable_reports_days;

    static constexpr char kNullAggregatableReports[] =
        "null_aggregatable_reports_days";
    if (base::Value* v = dict.Find(kNullAggregatableReports);
        v && !v->is_none()) {
      auto context = PushContext(kNullAggregatableReports);
      ParseList(v, [&](base::Value value) {
        std::optional<int> int_value = value.GetIfInt();
        if (!int_value || *int_value < 0) {
          *Error() << "must be a non-negative integer";
        } else {
          null_aggregatable_reports_days.emplace(*int_value);
        }
      });
    }

    return null_aggregatable_reports_days;
  }

  bool ParseDict(base::Value::Dict& value,
                 std::string_view key,
                 base::FunctionRef<void(base::Value::Dict)> parse_dict) {
    auto context = PushContext(key);

    base::Value* dict = value.Find(key);
    if (!EnsureDictionary(dict)) {
      return false;
    }

    parse_dict(std::move(*dict).TakeDict());
    return true;
  }

  bool EnsureDictionary(const base::Value* value) {
    if (!value) {
      *Error() << "must be present";
      return false;
    }

    if (!value->is_dict()) {
      *Error() << "must be a dictionary";
      return false;
    }
    return true;
  }

  // Returns true if `key` is present in `dict` and the integer is parsed
  // successfully.
  template <typename T>
  bool ParseInteger(const base::Value::Dict& dict,
                    std::string_view key,
                    T& result,
                    bool (*convert_func)(std::string_view, T*),
                    bool required,
                    bool allow_zero) {
    auto context = PushContext(key);

    const base::Value* value = dict.Find(key);
    if (value) {
      const std::string* s = value->GetIfString();
      if (s && convert_func(*s, &result) &&
          (result > 0 || (result == 0 && allow_zero))) {
        return true;
      }
    } else if (!required) {
      return false;
    }

    if (allow_zero) {
      *Error() << "must be a non-negative integer formatted as base-10 string";
    } else {
      *Error() << "must be a positive integer formatted as base-10 string";
    }

    return false;
  }

  bool ParseInt(const base::Value::Dict& dict,
                std::string_view key,
                int& result,
                bool required,
                bool allow_zero = false) {
    return ParseInteger(dict, key, result, &base::StringToInt, required,
                        allow_zero);
  }

  bool ParseInt64(const base::Value::Dict& dict,
                  std::string_view key,
                  int64_t& result,
                  bool required,
                  bool allow_zero = false) {
    return ParseInteger(dict, key, result, &base::StringToInt64, required,
                        allow_zero);
  }

  bool ParseUInt32(const base::Value::Dict& dict,
                   std::string_view key,
                   uint32_t& result,
                   bool required,
                   bool allow_zero = false) {
    int64_t result_64;
    // This works because `ParseInteger()` only accepts positive values, and
    // uint32 and [0, INT64_MAX] encompasses the same values.
    if (ParseInteger(dict, key, result_64, &base::StringToInt64, required,
                     allow_zero)) {
      if (base::internal::IsValueInRangeForNumericType<uint32_t>(result_64)) {
        result = static_cast<uint32_t>(result_64);
        return true;
      } else {
        auto context = PushContext(key);
        *Error() << "must be representable by an unsigned 32-bit integer";
      }
    }
    return false;
  }

  void ParseDouble(const base::Value::Dict& dict,
                   std::string_view key,
                   double& result,
                   bool required) {
    auto context = PushContext(key);
    const base::Value* value = dict.Find(key);

    if (value) {
      const std::string* s = value->GetIfString();
      if (s) {
        if (*s == "inf") {
          result = std::numeric_limits<double>::infinity();
          return;
        }
        if (base::StringToDouble(*s, &result) && result >= 0) {
          return;
        }
      }
    } else if (!required) {
      return;
    }

    *Error() << "must be \"inf\" or a non-negative double formated as a "
                "base-10 string";
  }
};

}  // namespace

AttributionSimulationEvent::AttributionSimulationEvent(base::Time time,
                                                       Data data)
    : time(time), data(std::move(data)) {}

AttributionSimulationEvent::~AttributionSimulationEvent() = default;

AttributionSimulationEvent::AttributionSimulationEvent(
    AttributionSimulationEvent&&) = default;

AttributionSimulationEvent& AttributionSimulationEvent::operator=(
    AttributionSimulationEvent&&) = default;

AttributionSimulationEvent::Response::Response(
    int64_t request_id,
    GURL url,
    scoped_refptr<net::HttpResponseHeaders> response_headers,
    attribution_reporting::RandomizedResponse randomized_response,
    base::flat_set<int> null_aggregatable_reports_days,
    bool debug_permission)
    : request_id(request_id),
      url(std::move(url)),
      response_headers(std::move(response_headers)),
      randomized_response(std::move(randomized_response)),
      null_aggregatable_reports_days(std::move(null_aggregatable_reports_days)),
      debug_permission(debug_permission) {}

AttributionSimulationEvent::Response::~Response() = default;

AttributionSimulationEvent::Response::Response(Response&&) = default;

AttributionSimulationEvent::Response&
AttributionSimulationEvent::Response::operator=(Response&&) = default;

base::expected<std::vector<AttributionSimulationEvent>, std::string>
ParseAttributionInteropInput(base::Value::Dict input) {
  return AttributionInteropParser().ParseInput(std::move(input));
}

base::expected<AttributionInteropConfig, std::string>
ParseAttributionInteropConfig(base::Value::Dict dict) {
  AttributionInteropConfig config;
  RETURN_IF_ERROR(
      AttributionInteropParser().ParseConfig(dict, config, /*required=*/true));
  return config;
}

base::expected<void, std::string> MergeAttributionInteropConfig(
    base::Value::Dict dict,
    AttributionInteropConfig& config) {
  return AttributionInteropParser().ParseConfig(dict, config,
                                                /*required=*/false);
}

// static
base::expected<AttributionInteropOutput, std::string>
AttributionInteropOutput::Parse(base::Value::Dict dict) {
  return AttributionInteropParser().ParseOutput(std::move(dict));
}

AttributionInteropOutput::AttributionInteropOutput() = default;

AttributionInteropOutput::~AttributionInteropOutput() = default;

AttributionInteropOutput::AttributionInteropOutput(AttributionInteropOutput&&) =
    default;

AttributionInteropOutput& AttributionInteropOutput::operator=(
    AttributionInteropOutput&&) = default;

AttributionInteropOutput::Report::Report() = default;

AttributionInteropOutput::Report::Report(base::Time time,
                                         GURL url,
                                         base::Value payload)
    : time(time), url(std::move(url)), payload(std::move(payload)) {}

AttributionInteropOutput::Report::Report(const Report& other)
    : Report(other.time, other.url, other.payload.Clone()) {}

base::Value::Dict AttributionInteropOutput::Report::ToJson() const {
  return base::Value::Dict()
      .Set(kReportTimeKey, TimeAsUnixMillisecondString(time))
      .Set(kReportUrlKey, url.spec())
      .Set(kPayloadKey, payload.Clone());
}

base::Value::Dict AttributionInteropOutput::ToJson() const {
  base::Value::List report_list;
  for (const auto& report : reports) {
    report_list.Append(report.ToJson());
  }

  return base::Value::Dict().Set(kReportsKey, std::move(report_list));
}

AttributionInteropOutput::Report& AttributionInteropOutput::Report::operator=(
    const Report& other) {
  time = other.time;
  url = other.url;
  payload = other.payload.Clone();
  return *this;
}

std::ostream& operator<<(std::ostream& out,
                         const AttributionInteropOutput::Report& report) {
  return out << report.ToJson();
}

std::ostream& operator<<(std::ostream& out,
                         const AttributionInteropOutput& output) {
  return out << output.ToJson();
}

// static
base::expected<AttributionInteropRun, std::string> AttributionInteropRun::Parse(
    base::Value::Dict dict,
    const AttributionInteropConfig& default_config) {
  AttributionInteropRun run;
  run.config = default_config;

  if (base::Value* api_config = dict.Find("api_config")) {
    base::Value::Dict* config_dict = api_config->GetIfDict();
    if (!config_dict) {
      return base::unexpected("api_config must be a dict");
    }
    RETURN_IF_ERROR(
        MergeAttributionInteropConfig(std::move(*config_dict), run.config));
  }

  std::optional<base::Value> input = dict.Extract("input");
  if (!input.has_value() || !input->is_dict()) {
    return base::unexpected("input must be a dict");
  }

  ASSIGN_OR_RETURN(run.events,
                   ParseAttributionInteropInput(std::move(*input).TakeDict()));

  return run;
}

AttributionInteropRun::AttributionInteropRun() = default;

AttributionInteropRun::~AttributionInteropRun() = default;

AttributionInteropRun::AttributionInteropRun(AttributionInteropRun&&) = default;

AttributionInteropRun& AttributionInteropRun::operator=(
    AttributionInteropRun&&) = default;

AttributionInteropConfig::AttributionInteropConfig() = default;

AttributionInteropConfig::~AttributionInteropConfig() = default;

AttributionInteropConfig::AttributionInteropConfig(
    const AttributionInteropConfig&) = default;

AttributionInteropConfig& AttributionInteropConfig::operator=(
    const AttributionInteropConfig&) = default;

AttributionInteropConfig::AttributionInteropConfig(AttributionInteropConfig&&) =
    default;

AttributionInteropConfig& AttributionInteropConfig::operator=(
    AttributionInteropConfig&&) = default;

}  // namespace content
