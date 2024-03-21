// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/attribution_interop_parser.h"

#include <stddef.h>
#include <stdint.h>

#include <optional>
#include <ostream>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/functional/function_ref.h"
#include "base/functional/overloaded.h"
#include "base/json/json_writer.h"
#include "base/memory/raw_ref.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "base/types/expected_macros.h"
#include "base/types/optional_util.h"
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
#include "third_party/abseil-cpp/absl/types/variant.h"

namespace content {

namespace {

using ::attribution_reporting::SuitableOrigin;
using ::network::mojom::AttributionReportingEligibility;

constexpr char kAttributionSrcUrlKey[] = "attribution_src_url";
constexpr char kEligibleKey[] = "Attribution-Reporting-Eligible";
constexpr char kPayloadKey[] = "payload";
constexpr char kRegistrationRequestKey[] = "registration_request";
constexpr char kReportTimeKey[] = "report_time";
constexpr char kReportUrlKey[] = "report_url";
constexpr char kReportsKey[] = "reports";
constexpr char kResponseKey[] = "response";
constexpr char kResponsesKey[] = "responses";

using Context = absl::variant<base::StringPiece, size_t>;
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
    absl::visit(
        base::Overloaded{
            [&](base::StringPiece key) { out << "[\"" << key << "\"]"; },
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
  explicit AttributionInteropParser(
      base::Time offset_time = base::Time::UnixEpoch())
      : offset_time_(offset_time) {}

  ~AttributionInteropParser() = default;

  AttributionInteropParser(const AttributionInteropParser&) = delete;
  AttributionInteropParser(AttributionInteropParser&&) = delete;

  AttributionInteropParser& operator=(const AttributionInteropParser&) = delete;
  AttributionInteropParser& operator=(AttributionInteropParser&&) = delete;

  base::expected<AttributionSimulationEvents, std::string> ParseInput(
      base::Value::Dict input) && {
    std::vector<AttributionSimulationEvent> events;

    static constexpr char kKeyRegistrations[] = "registrations";
    if (base::Value* registrations = input.Find(kKeyRegistrations)) {
      auto context = PushContext(kKeyRegistrations);
      ParseListOfDicts(registrations, [&](base::Value::Dict registration) {
        ParseRegistration(std::move(registration), events);
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
      std::optional<base::Value> reports = dict.Extract(kReportsKey);
      auto context = PushContext(kReportsKey);
      ParseListOfDicts(base::OptionalToPtr(reports),
                       [&](base::Value::Dict report) {
                         ParseReport(std::move(report), output.reports);
                       });
    }

    CheckUnknown(dict);

    if (has_error_) {
      return base::unexpected(error_stream_.str());
    }
    return output;
  }

  [[nodiscard]] std::string ParseConfig(
      const base::Value::Dict& dict,
      AttributionInteropConfig& interop_config,
      bool required) && {
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

    ParseDouble(dict, "max_navigation_info_gain",
                config.event_level_limit.max_navigation_info_gain, required);
    ParseDouble(dict, "max_event_info_gain",
                config.event_level_limit.max_event_info_gain, required);

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

    // TODO(linnan): Parse null reports rate if it's supported in interop tests.

    return error_stream_.str();
  }

 private:
  const base::Time offset_time_;

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

  void ParseListOfDicts(
      base::Value* values,
      base::FunctionRef<void(base::Value::Dict)> parse_element,
      size_t expected_size = 0) {
    if (!values) {
      *Error() << "must be present";
      return;
    }

    base::Value::List* list = values->GetIfList();
    if (!list) {
      *Error() << "must be a list";
      return;
    }

    if (expected_size > 0 && list->size() != expected_size) {
      *Error() << "must have size " << expected_size;
      return;
    }

    size_t index = 0;
    for (auto& value : *list) {
      auto index_context = PushContext(index);
      if (!EnsureDictionary(&value)) {
        return;
      }
      parse_element(std::move(value).TakeDict());
      index++;
    }
  }

  void VerifyReportingOrigin(const base::Value::Dict& dict,
                             const SuitableOrigin& reporting_origin) {
    static constexpr char kUrlKey[] = "url";
    std::optional<SuitableOrigin> origin = ParseOrigin(dict, kUrlKey);
    if (has_error_) {
      return;
    }
    if (*origin != reporting_origin) {
      auto context = PushContext(kUrlKey);
      *Error() << "must match " << reporting_origin.Serialize();
    }
  }

  void ParseRegistration(base::Value::Dict dict,
                         std::vector<AttributionSimulationEvent>& events) {
    const base::Time time =
        ParseTime(dict, /*key=*/"timestamp",
                  /*previous_time=*/events.empty() ? base::Time::Min()
                                                   : events.back().time,
                  /*strictly_greater=*/true);

    std::optional<SuitableOrigin> context_origin;
    std::optional<SuitableOrigin> reporting_origin;
    AttributionReportingEligibility eligibility;

    ParseDict(dict, kRegistrationRequestKey, [&](base::Value::Dict reg_req) {
      context_origin = ParseOrigin(reg_req, "context_origin");
      reporting_origin = ParseOrigin(reg_req, kAttributionSrcUrlKey);
      eligibility = ParseEligibility(reg_req);
    });

    if (has_error_) {
      return;
    }

    {
      auto context = PushContext(kResponsesKey);
      ParseListOfDicts(
          dict.Find(kResponsesKey),
          [&](base::Value::Dict response) {
            VerifyReportingOrigin(response, *reporting_origin);

            const bool debug_permission = ParseDebugPermission(response);

            attribution_reporting::RandomizedResponse randomized_response =
                ParseRandomizedResponse(response);

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
                      value = base::Value(std::move(*json));
                      builder.AddHeader(header, value.GetString());
                    }
                  }

                  auto& event = events.emplace_back(
                      std::move(*reporting_origin), std::move(*context_origin));
                  event.eligibility = eligibility;
                  event.response_headers = builder.Build();
                  event.time = time;
                  event.debug_permission = debug_permission;
                  event.randomized_response = std::move(randomized_response);
                });
          },
          /*expected_size=*/1);
    }
  }

  void ParseReport(base::Value::Dict dict,
                   std::vector<AttributionInteropOutput::Report>& reports) {
    AttributionInteropOutput::Report report;

    report.time =
        ParseTime(dict, kReportTimeKey,
                  /*previous_time=*/reports.empty() ? base::Time::Min()
                                                    : reports.back().time,
                  /*strictly_greater=*/false);
    dict.Remove(kReportTimeKey);

    if (std::optional<base::Value> url = dict.Extract(kReportUrlKey);
        const std::string* str = url ? url->GetIfString() : nullptr) {
      report.url = GURL(*str);
    }
    if (!report.url.is_valid()) {
      auto context = PushContext(kReportUrlKey);
      *Error() << "must be a valid URL";
    }

    if (std::optional<base::Value> payload = dict.Extract(kPayloadKey)) {
      report.payload = std::move(*payload);
    } else {
      auto context = PushContext(kPayloadKey);
      *Error() << "required";
    }

    CheckUnknown(dict);

    if (!has_error_) {
      reports.push_back(std::move(report));
    }
  }

  void CheckUnknown(const base::Value::Dict& dict) {
    for (auto [key, value] : dict) {
      auto context = PushContext(key);
      *Error() << "unknown field";
    }
  }

  std::optional<SuitableOrigin> ParseOrigin(const base::Value::Dict& dict,
                                            base::StringPiece key) {
    auto context = PushContext(key);

    std::optional<SuitableOrigin> origin;
    if (const std::string* s = dict.FindString(key)) {
      origin = SuitableOrigin::Deserialize(*s);
    }

    if (!origin.has_value()) {
      *Error() << "must be a valid, secure origin";
    }

    return origin;
  }

  base::Time ParseTime(const base::Value::Dict& dict,
                       base::StringPiece key,
                       base::Time previous_time,
                       bool strictly_greater) {
    auto context = PushContext(key);

    const std::string* v = dict.FindString(key);
    int64_t milliseconds;

    if (v && base::StringToInt64(*v, &milliseconds)) {
      base::Time time = offset_time_ + base::Milliseconds(milliseconds);
      if (!time.is_null() && !time.is_inf()) {
        if (strictly_greater && time <= previous_time) {
          *Error() << "must be greater than previous time";
        } else if (!strictly_greater && time < previous_time) {
          *Error() << "must be greater than or equal to previous time";
        }
        return time;
      }
    }

    *Error() << "must be an integer number of milliseconds since the Unix "
                "epoch formatted as a base-10 string";
    return base::Time();
  }

  std::optional<bool> ParseBool(const base::Value::Dict& dict,
                                base::StringPiece key) {
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

  bool ParseDict(base::Value::Dict& value,
                 base::StringPiece key,
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
                    base::StringPiece key,
                    T& result,
                    bool (*convert_func)(base::StringPiece, T*),
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
                base::StringPiece key,
                int& result,
                bool required,
                bool allow_zero = false) {
    return ParseInteger(dict, key, result, &base::StringToInt, required,
                        allow_zero);
  }

  bool ParseInt64(const base::Value::Dict& dict,
                  base::StringPiece key,
                  int64_t& result,
                  bool required,
                  bool allow_zero = false) {
    return ParseInteger(dict, key, result, &base::StringToInt64, required,
                        allow_zero);
  }

  void ParseDouble(const base::Value::Dict& dict,
                   base::StringPiece key,
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

AttributionSimulationEvent::AttributionSimulationEvent(
    SuitableOrigin reporting_origin,
    SuitableOrigin context_origin)
    : reporting_origin(std::move(reporting_origin)),
      context_origin(std::move(context_origin)) {}

AttributionSimulationEvent::~AttributionSimulationEvent() = default;

AttributionSimulationEvent::AttributionSimulationEvent(
    AttributionSimulationEvent&&) = default;

AttributionSimulationEvent& AttributionSimulationEvent::operator=(
    AttributionSimulationEvent&&) = default;

base::expected<AttributionSimulationEvents, std::string>
ParseAttributionInteropInput(base::Value::Dict input,
                             const base::Time offset_time) {
  return AttributionInteropParser(offset_time).ParseInput(std::move(input));
}

base::expected<AttributionInteropConfig, std::string>
ParseAttributionInteropConfig(const base::Value::Dict& dict) {
  AttributionInteropConfig config;
  std::string error =
      AttributionInteropParser().ParseConfig(dict, config, /*required=*/true);
  if (!error.empty()) {
    return base::unexpected(std::move(error));
  }
  return config;
}

std::string MergeAttributionInteropConfig(const base::Value::Dict& dict,
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

}  // namespace content
