// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/attribution_interop_parser.h"

#include <stddef.h>
#include <stdint.h>

#include <ostream>
#include <utility>

#include "base/check.h"
#include "base/functional/callback.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/test/bind.h"
#include "base/time/time.h"
#include "content/public/browser/attribution_config.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

AttributionInteropParser::AttributionInteropParser(std::ostream& stream)
    : error_manager_(stream) {}

AttributionInteropParser::~AttributionInteropParser() = default;

bool AttributionInteropParser::has_error() const {
  return error_manager_.has_error();
}

std::unique_ptr<AttributionParserErrorManager::ScopedContext>
AttributionInteropParser::PushContext(
    AttributionParserErrorManager::Context context) {
  return error_manager_.PushContext(context);
}

AttributionParserErrorManager::ErrorWriter AttributionInteropParser::Error() {
  return error_manager_.Error();
}

void AttributionInteropParser::MoveDictValues(base::Value::Dict& in,
                                              base::Value::Dict& out) {
  for (auto [key, value] : in) {
    auto context = PushContext(key);
    if (out.contains(key)) {
      *Error() << "must not be present";
      return;
    }
    out.Set(key, std::move(value));
  }
}

void AttributionInteropParser::MoveValue(
    base::Value::Dict& in,
    base::StringPiece in_key,
    base::Value::Dict& out,
    absl::optional<base::StringPiece> out_key_opt) {
  auto context = PushContext(in_key);

  base::Value* value = in.Find(in_key);
  if (!value) {
    *Error() << "must be present";
    return;
  }

  base::StringPiece out_key = out_key_opt.value_or(in_key);
  DCHECK(!out.contains(out_key));
  out.Set(out_key, std::move(*value));
}

bool AttributionInteropParser::EnsureDictionary(const base::Value* value) {
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

absl::optional<std::string> AttributionInteropParser::ExtractString(
    base::Value::Dict& dict,
    base::StringPiece key) {
  auto context = PushContext(key);

  absl::optional<base::Value> value = dict.Extract(key);
  if (!value) {
    *Error() << "must be present";
    return absl::nullopt;
  }

  if (std::string* str = value->GetIfString()) {
    return std::move(*str);
  }

  *Error() << "must be a string";
  return absl::nullopt;
}

void AttributionInteropParser::ParseList(
    base::Value* values,
    base::RepeatingCallback<void(base::Value)> callback,
    size_t expected_size) {
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
  for (auto& value : values->GetList()) {
    auto context = PushContext(index);
    callback.Run(std::move(value));
    index++;
  }
}

absl::optional<std::string> AttributionInteropParser::ParseRequest(
    base::Value::Dict& in,
    base::Value::Dict& out) {
  static constexpr char kKey[] = "registration_request";

  auto context = PushContext(kKey);

  base::Value* request = in.Find(kKey);
  if (!EnsureDictionary(request)) {
    return absl::nullopt;
  }

  absl::optional<std::string> str =
      ExtractString(request->GetDict(), "attribution_src_url");

  MoveDictValues(request->GetDict(), out);

  return str;
}

void AttributionInteropParser::ParseResponse(
    base::Value::Dict& in,
    base::Value::Dict& out,
    const std::string& attribution_src_url) {
  static constexpr char kKey[] = "responses";

  auto context = PushContext(kKey);

  ParseList(in.Find(kKey), base::BindLambdaForTesting([&](base::Value value) {
              if (!EnsureDictionary(&value)) {
                return;
              }

              static constexpr char kKeyUrl[] = "url";
              if (absl::optional<std::string> url =
                      ExtractString(value.GetDict(), kKeyUrl);
                  url && *url != attribution_src_url) {
                auto inner_context = PushContext(kKeyUrl);
                *Error() << "must match " << attribution_src_url;
              }

              static constexpr char kKeyResponse[] = "response";
              auto inner_context = PushContext(kKeyResponse);
              base::Value* response = value.GetDict().Find(kKeyResponse);
              if (!EnsureDictionary(response)) {
                return;
              }

              MoveDictValues(response->GetDict(), out);
            }),
            /*expected_size=*/1);
}

base::Value::List AttributionInteropParser::ParseEvents(base::Value::Dict& dict,
                                                        base::StringPiece key) {
  auto context = PushContext(key);

  base::Value::List results;

  ParseList(dict.Find(key), base::BindLambdaForTesting([&](base::Value value) {
              if (!EnsureDictionary(&value)) {
                return;
              }

              static constexpr char kKeyReportingOrigin[] = "reporting_origin";

              base::Value::Dict dict;
              MoveValue(value.GetDict(), "timestamp", dict);

              // Placeholder so that it errors out if request or response
              // contains this field.
              dict.Set(kKeyReportingOrigin, "");

              absl::optional<std::string> attribution_src_url =
                  ParseRequest(value.GetDict(), dict);

              if (has_error()) {
                return;
              }

              DCHECK(attribution_src_url);

              ParseResponse(value.GetDict(), dict, *attribution_src_url);

              if (has_error()) {
                return;
              }

              dict.Set(
                  kKeyReportingOrigin,
                  url::Origin::Create(GURL(std::move(*attribution_src_url)))
                      .Serialize());

              results.Append(std::move(dict));
            }));

  return results;
}

absl::optional<base::Value>
AttributionInteropParser::SimulatorInputFromInteropInput(
    base::Value::Dict& input) {
  static constexpr char kKey[] = "input";

  error_manager_.ResetErrorState();

  auto context = PushContext(kKey);

  base::Value* dict = input.Find(kKey);
  if (!EnsureDictionary(dict)) {
    return absl::nullopt;
  }

  base::Value::List sources = ParseEvents(dict->GetDict(), "sources");
  base::Value::List triggers = ParseEvents(dict->GetDict(), "triggers");

  if (has_error()) {
    return absl::nullopt;
  }

  base::Value::Dict result;
  result.Set("sources", std::move(sources));
  result.Set("triggers", std::move(triggers));
  return base::Value(std::move(result));
}

base::Value::List AttributionInteropParser::ParseEventLevelReports(
    base::Value::Dict& output) {
  static constexpr char kKey[] = "event_level_reports";

  base::Value::List event_level_results;

  base::Value* value = output.Find(kKey);
  if (!value) {
    return event_level_results;
  }

  auto context = PushContext(kKey);
  ParseList(
      output.Find(kKey), base::BindLambdaForTesting([&](base::Value value) {
        if (!EnsureDictionary(&value)) {
          return;
        }

        base::Value::Dict result;

        base::Value::Dict& value_dict = value.GetDict();
        MoveValue(value_dict, "report", result, "payload");
        MoveValue(value_dict, "report_url", result);
        MoveValue(value_dict, "intended_report_time", result, "report_time");

        if (has_error()) {
          return;
        }

        event_level_results.Append(std::move(result));
      }));

  return event_level_results;
}

base::Value::List AttributionInteropParser::ParseAggregatableReports(
    base::Value::Dict& output) {
  static constexpr char kKey[] = "aggregatable_reports";

  base::Value::List aggregatable_results;

  base::Value* value = output.Find(kKey);
  if (!value) {
    return aggregatable_results;
  }

  auto context = PushContext(kKey);
  ParseList(
      output.Find(kKey), base::BindLambdaForTesting([&](base::Value value) {
        if (!EnsureDictionary(&value)) {
          return;
        }

        base::Value::Dict result;

        base::Value::Dict& value_dict = value.GetDict();
        MoveValue(value_dict, "report_url", result);
        MoveValue(value_dict, "intended_report_time", result, "report_time");

        static constexpr char kKeyTestInfo[] = "test_info";
        base::Value* test_info;
        {
          auto test_info_context = PushContext(kKeyTestInfo);
          test_info = value_dict.Find(kKeyTestInfo);
          if (!EnsureDictionary(test_info)) {
            return;
          }
        }

        static constexpr char kKeyReport[] = "report";
        {
          auto report_context = PushContext(kKeyReport);
          base::Value* report = value_dict.Find(kKeyReport);
          if (!EnsureDictionary(report)) {
            return;
          }

          MoveDictValues(test_info->GetDict(), report->GetDict());
        }

        MoveValue(value_dict, "report", result, "payload");

        if (has_error()) {
          return;
        }

        aggregatable_results.Append(std::move(result));
      }));

  return aggregatable_results;
}

base::Value::List AttributionInteropParser::ParseVerboseDebugReports(
    base::Value::Dict& output) {
  static constexpr char kKey[] = "verbose_debug_reports";

  base::Value::List reports;

  base::Value* value = output.Find(kKey);
  if (!value) {
    return reports;
  }

  auto context = PushContext(kKey);
  ParseList(output.Find(kKey),
            base::BindLambdaForTesting([&](base::Value value) {
              if (!EnsureDictionary(&value)) {
                return;
              }

              base::Value::Dict report;

              base::Value::Dict& value_dict = value.GetDict();
              MoveValue(value_dict, "report", report, "payload");
              MoveValue(value_dict, "report_url", report);
              MoveValue(value_dict, "report_time", report);

              if (has_error()) {
                return;
              }

              reports.Append(std::move(report));
            }));

  return reports;
}

absl::optional<base::Value>
AttributionInteropParser::InteropOutputFromSimulatorOutput(base::Value output) {
  error_manager_.ResetErrorState();

  if (!EnsureDictionary(&output)) {
    return absl::nullopt;
  }

  base::Value::List event_level_results =
      ParseEventLevelReports(output.GetDict());

  base::Value::List aggregatable_results =
      ParseAggregatableReports(output.GetDict());

  base::Value::List verbose_debug_reports =
      ParseVerboseDebugReports(output.GetDict());

  if (has_error()) {
    return absl::nullopt;
  }

  base::Value::Dict dict;
  if (!event_level_results.empty()) {
    dict.Set("event_level_results", std::move(event_level_results));
  }

  if (!aggregatable_results.empty()) {
    dict.Set("aggregatable_results", std::move(aggregatable_results));
  }

  if (!verbose_debug_reports.empty()) {
    dict.Set("verbose_debug_reports", std::move(verbose_debug_reports));
  }

  return base::Value(std::move(dict));
}

bool AttributionInteropParser::ParseInt(const base::Value::Dict& dict,
                                        base::StringPiece key,
                                        int& result,
                                        bool required,
                                        bool allow_zero) {
  return ParseInteger(dict, key, result, &base::StringToInt, required,
                      allow_zero);
}

bool AttributionInteropParser::ParseUint64(const base::Value::Dict& dict,
                                           base::StringPiece key,
                                           uint64_t& result,
                                           bool required,
                                           bool allow_zero) {
  return ParseInteger(dict, key, result, &base::StringToUint64, required,
                      allow_zero);
}

bool AttributionInteropParser::ParseInt64(const base::Value::Dict& dict,
                                          base::StringPiece key,
                                          int64_t& result,
                                          bool required,
                                          bool allow_zero) {
  return ParseInteger(dict, key, result, &base::StringToInt64, required,
                      allow_zero);
}

void AttributionInteropParser::ParseRandomizedResponseRate(
    const base::Value::Dict& dict,
    base::StringPiece key,
    double& result,
    bool required) {
  auto context = PushContext(key);

  const base::Value* value = dict.Find(key);

  if (value) {
    absl::optional<double> d = value->GetIfDouble();
    if (d && *d >= 0 && *d <= 1) {
      result = *d;
      return;
    }
  } else if (!required) {
    return;
  }

  *Error() << "must be a double between 0 and 1 formatted as string";
}

bool AttributionInteropParser::ParseConfig(const base::Value& value,
                                           AttributionConfig& config,
                                           bool required,
                                           base::StringPiece key) {
  error_manager_.ResetErrorState();

  std::unique_ptr<AttributionParserErrorManager::ScopedContext> context;
  if (!key.empty()) {
    context = PushContext(key);
  }

  if (!EnsureDictionary(&value)) {
    return false;
  }

  const base::Value::Dict& dict = value.GetDict();

  ParseInt(dict, "max_sources_per_origin", config.max_sources_per_origin,
           required);

  ParseInt(dict, "max_destinations_per_source_site_reporting_origin",
           config.max_destinations_per_source_site_reporting_origin, required);

  uint64_t source_event_id_cardinality;
  if (ParseUint64(dict, "source_event_id_cardinality",
                  source_event_id_cardinality, required,
                  /*allow_zero=*/true)) {
    if (source_event_id_cardinality == 0u) {
      config.source_event_id_cardinality = absl::nullopt;
    } else {
      config.source_event_id_cardinality = source_event_id_cardinality;
    }
  }

  int rate_limit_time_window;
  if (ParseInt(dict, "rate_limit_time_window", rate_limit_time_window,
               required)) {
    config.rate_limit.time_window = base::Days(rate_limit_time_window);
  }

  ParseInt64(dict, "rate_limit_max_source_registration_reporting_origins",
             config.rate_limit.max_source_registration_reporting_origins,
             required);
  ParseInt64(dict, "rate_limit_max_attribution_reporting_origins",
             config.rate_limit.max_attribution_reporting_origins, required);
  ParseInt64(dict, "rate_limit_max_attributions",
             config.rate_limit.max_attributions, required);

  ParseInt(dict, "max_event_level_reports_per_destination",
           config.event_level_limit.max_reports_per_destination, required);
  ParseInt(dict, "max_attributions_per_navigation_source",
           config.event_level_limit.max_attributions_per_navigation_source,
           required);
  ParseInt(dict, "max_attributions_per_event_source",
           config.event_level_limit.max_attributions_per_event_source,
           required);
  ParseUint64(
      dict, "navigation_source_trigger_data_cardinality",
      config.event_level_limit.navigation_source_trigger_data_cardinality,
      required);
  ParseUint64(dict, "event_source_trigger_data_cardinality",
              config.event_level_limit.event_source_trigger_data_cardinality,
              required);
  ParseRandomizedResponseRate(
      dict, "navigation_source_randomized_response_rate",
      config.event_level_limit.navigation_source_randomized_response_rate,
      required);
  ParseRandomizedResponseRate(
      dict, "event_source_randomized_response_rate",
      config.event_level_limit.event_source_randomized_response_rate, required);

  ParseInt(dict, "max_aggregatable_reports_per_destination",
           config.aggregate_limit.max_reports_per_destination, required);
  ParseInt64(dict, "aggregatable_budget_per_source",
             config.aggregate_limit.aggregatable_budget_per_source, required);

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

  return !has_error();
}

}  // namespace content
