// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/attribution_interop_parser.h"

#include <stdint.h>

#include <memory>
#include <string>
#include <utility>

#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "base/values.h"
#include "content/browser/attribution_reporting/attribution_config.h"
#include "content/browser/attribution_reporting/attribution_parser_test_utils.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace content {
namespace {

class AttributionInteropParser {
 public:
  [[nodiscard]] std::string ParseConfig(const base::Value::Dict&,
                                        AttributionConfig&,
                                        bool required) &&;

 private:
  [[nodiscard]] std::unique_ptr<AttributionParserErrorManager::ScopedContext>
  PushContext(AttributionParserErrorManager::Context context);

  AttributionParserErrorManager::ErrorWriter Error();

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

  bool ParseUint64(const base::Value::Dict& dict,
                   base::StringPiece key,
                   uint64_t& result,
                   bool required,
                   bool allow_zero = false) {
    return ParseInteger(dict, key, result, &base::StringToUint64, required,
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

  void ParseRandomizedResponseRate(const base::Value::Dict& dict,
                                   base::StringPiece key,
                                   double& result,
                                   bool required);

  AttributionParserErrorManager error_manager_;
};

std::unique_ptr<AttributionParserErrorManager::ScopedContext>
AttributionInteropParser::PushContext(
    AttributionParserErrorManager::Context context) {
  return error_manager_.PushContext(context);
}

AttributionParserErrorManager::ErrorWriter AttributionInteropParser::Error() {
  return error_manager_.Error();
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

std::string AttributionInteropParser::ParseConfig(const base::Value::Dict& dict,
                                                  AttributionConfig& config,
                                                  bool required) && {
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

  return std::move(error_manager_).TakeError();
}

}  // namespace

base::expected<AttributionConfig, std::string> ParseAttributionConfig(
    const base::Value::Dict& dict) {
  AttributionConfig config;
  std::string error =
      AttributionInteropParser().ParseConfig(dict, config, /*required=*/true);
  if (!error.empty()) {
    return base::unexpected(std::move(error));
  }
  return config;
}

std::string MergeAttributionConfig(const base::Value::Dict& dict,
                                   AttributionConfig& config) {
  return AttributionInteropParser().ParseConfig(dict, config,
                                                /*required=*/false);
}

}  // namespace content
