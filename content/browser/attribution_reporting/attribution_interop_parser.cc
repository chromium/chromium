// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/attribution_interop_parser.h"

#include <stddef.h>

#include <ostream>
#include <utility>

#include "base/callback.h"
#include "base/check.h"
#include "base/strings/string_piece.h"
#include "base/test/bind.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

AttributionInteropParser::AttributionInteropParser(std::ostream& stream)
    : error_manager_(stream) {}

AttributionInteropParser::~AttributionInteropParser() = default;

bool AttributionInteropParser::has_error() const {
  return error_manager_.has_error();
}

AttributionParserErrorManager::ScopedContext
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

  if (std::string* str = value->GetIfString())
    return std::move(*str);

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
  if (!EnsureDictionary(request))
    return absl::nullopt;

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
              if (!EnsureDictionary(&value))
                return;

              static constexpr char kKeyUrl[] = "url";
              if (absl::optional<std::string> url =
                      ExtractString(value.GetDict(), kKeyUrl);
                  url && *url != attribution_src_url) {
                auto inner_context = PushContext(kKeyUrl);
                *Error() << "must match " << attribution_src_url;
              }

              static constexpr char kKeyResponse[] = "response";
              auto inner_context = PushContext(kKeyResponse);
              base::Value* response = value.FindKey(kKeyResponse);
              if (!EnsureDictionary(response))
                return;

              MoveDictValues(response->GetDict(), out);
            }),
            /*expected_size=*/1);
}

base::Value::List AttributionInteropParser::ParseEvents(base::Value::Dict& dict,
                                                        base::StringPiece key) {
  auto context = PushContext(key);

  base::Value::List results;

  ParseList(dict.Find(key), base::BindLambdaForTesting([&](base::Value value) {
              if (!EnsureDictionary(&value))
                return;

              static constexpr char kKeyReportingOrigin[] = "reporting_origin";

              base::Value::Dict dict;
              MoveValue(value.GetDict(), "timestamp", dict);

              // Placeholder so that it errors out if request or response
              // contains this field.
              dict.Set(kKeyReportingOrigin, "");

              absl::optional<std::string> attribution_src_url =
                  ParseRequest(value.GetDict(), dict);

              if (has_error())
                return;

              DCHECK(attribution_src_url);

              ParseResponse(value.GetDict(), dict, *attribution_src_url);

              if (has_error())
                return;

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

  auto context = PushContext(kKey);

  base::Value* dict = input.Find(kKey);
  if (!EnsureDictionary(dict))
    return absl::nullopt;

  base::Value::List sources = ParseEvents(dict->GetDict(), "sources");
  base::Value::List triggers = ParseEvents(dict->GetDict(), "triggers");

  if (has_error())
    return absl::nullopt;

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
  if (!value)
    return event_level_results;

  auto context = PushContext(kKey);
  ParseList(output.Find(kKey),
            base::BindLambdaForTesting([&](base::Value value) {
              if (!EnsureDictionary(&value))
                return;

              base::Value::Dict result;

              base::Value::Dict& value_dict = value.GetDict();
              MoveValue(value_dict, "report", result, "payload");
              MoveValue(value_dict, "report_url", result);
              MoveValue(value_dict, "report_time", result);

              if (has_error())
                return;

              event_level_results.Append(std::move(result));
            }));

  return event_level_results;
}

base::Value::List AttributionInteropParser::ParseAggregatableReports(
    base::Value::Dict& output) {
  static constexpr char kKey[] = "aggregatable_reports";

  base::Value::List aggregatable_results;

  base::Value* value = output.Find(kKey);
  if (!value)
    return aggregatable_results;

  auto context = PushContext(kKey);
  ParseList(output.Find(kKey),
            base::BindLambdaForTesting([&](base::Value value) {
              if (!EnsureDictionary(&value))
                return;

              base::Value::Dict result;

              base::Value::Dict& value_dict = value.GetDict();
              MoveValue(value_dict, "report_url", result);
              MoveValue(value_dict, "report_time", result);

              static constexpr char kKeyTestInfo[] = "test_info";
              base::Value* test_info;
              {
                auto test_info_context = PushContext(kKeyTestInfo);
                test_info = value_dict.Find(kKeyTestInfo);
                if (!EnsureDictionary(test_info))
                  return;
              }

              static constexpr char kKeyReport[] = "report";
              {
                auto report_context = PushContext(kKeyReport);
                base::Value* report = value_dict.Find(kKeyReport);
                if (!EnsureDictionary(report))
                  return;

                MoveDictValues(test_info->GetDict(), report->GetDict());
              }

              MoveValue(value_dict, "report", result, "payload");

              if (has_error())
                return;

              aggregatable_results.Append(std::move(result));
            }));

  return aggregatable_results;
}

absl::optional<base::Value>
AttributionInteropParser::InteropOutputFromSimulatorOutput(base::Value output) {
  if (!EnsureDictionary(&output))
    return absl::nullopt;

  base::Value::List event_level_results =
      ParseEventLevelReports(output.GetDict());

  base::Value::List aggregatable_results =
      ParseAggregatableReports(output.GetDict());

  if (has_error())
    return absl::nullopt;

  base::Value::Dict dict;
  if (!event_level_results.empty())
    dict.Set("event_level_results", std::move(event_level_results));

  if (!aggregatable_results.empty())
    dict.Set("aggregatable_results", std::move(aggregatable_results));

  return base::Value(std::move(dict));
}

}  // namespace content
