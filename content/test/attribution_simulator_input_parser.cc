// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/test/attribution_simulator_input_parser.h"

#include <stddef.h>
#include <stdint.h>

#include <string>
#include <utility>
#include <vector>

#include "base/functional/function_ref.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "base/types/optional_util.h"
#include "base/values.h"
#include "components/attribution_reporting/source_registration_error.mojom.h"
#include "components/attribution_reporting/suitable_origin.h"
#include "components/attribution_reporting/trigger_registration.h"
#include "components/attribution_reporting/trigger_registration_error.mojom.h"
#include "content/browser/attribution_reporting/attribution_header_utils.h"
#include "content/browser/attribution_reporting/attribution_parser_test_utils.h"
#include "content/browser/attribution_reporting/attribution_source_type.h"
#include "content/browser/attribution_reporting/attribution_trigger.h"
#include "content/browser/attribution_reporting/storable_source.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/abseil-cpp/absl/types/variant.h"

namespace content {

namespace {

using ::attribution_reporting::SuitableOrigin;

constexpr char kTimestampKey[] = "timestamp";

class AttributionSimulatorInputParser {
 public:
  explicit AttributionSimulatorInputParser(base::Time offset_time)
      : offset_time_(offset_time) {}

  ~AttributionSimulatorInputParser() = default;

  AttributionSimulatorInputParser(const AttributionSimulatorInputParser&) =
      delete;
  AttributionSimulatorInputParser(AttributionSimulatorInputParser&&) = delete;

  AttributionSimulatorInputParser& operator=(
      const AttributionSimulatorInputParser&) = delete;
  AttributionSimulatorInputParser& operator=(
      AttributionSimulatorInputParser&&) = delete;

  base::expected<AttributionSimulationEvents, std::string> Parse(
      base::Value::Dict input) && {
    static constexpr char kKeySources[] = "sources";
    if (base::Value* sources = input.Find(kKeySources)) {
      auto context = PushContext(kKeySources);
      ParseList(std::move(*sources),
                [&](base::Value source) { ParseSource(std::move(source)); });
    }

    static constexpr char kKeyTriggers[] = "triggers";
    if (base::Value* triggers = input.Find(kKeyTriggers)) {
      auto context = PushContext(kKeyTriggers);
      ParseList(std::move(*triggers),
                [&](base::Value trigger) { ParseTrigger(std::move(trigger)); });
    }

    if (has_error()) {
      return base::unexpected(std::move(error_manager_).TakeError());
    }

    return std::move(events_);
  }

 private:
  const base::Time offset_time_;
  AttributionParserErrorManager error_manager_;

  std::vector<AttributionSimulationEvent> events_;

  [[nodiscard]] std::unique_ptr<AttributionParserErrorManager::ScopedContext>
  PushContext(AttributionParserErrorManager::Context context) {
    return error_manager_.PushContext(context);
  }

  AttributionParserErrorManager::ErrorWriter Error() {
    return error_manager_.Error();
  }

  bool has_error() const { return error_manager_.has_error(); }

  void ParseList(base::Value values,
                 base::FunctionRef<void(base::Value)> parse_element) {
    if (!values.is_list()) {
      *Error() << "must be a list";
      return;
    }

    size_t index = 0;
    for (auto& value : values.GetList()) {
      auto index_context = PushContext(index);
      parse_element(std::move(value));
      index++;
    }
  }

  void ParseSource(base::Value source) {
    if (!EnsureDictionary(source))
      return;

    base::Value::Dict& source_dict = source.GetDict();

    base::Time source_time = ParseTime(source_dict, kTimestampKey);
    absl::optional<SuitableOrigin> source_origin =
        ParseOrigin(source_dict, "source_origin");
    absl::optional<SuitableOrigin> reporting_origin =
        ParseOrigin(source_dict, "reporting_origin");
    absl::optional<AttributionSourceType> source_type =
        ParseSourceType(source_dict);
    bool debug_permission = ParseDebugPermission(source_dict);

    if (has_error())
      return;

    ParseAttributionEvent(
        source_dict, "Attribution-Reporting-Register-Source",
        [&](base::Value::Dict dict) {
          base::expected<StorableSource,
                         attribution_reporting::mojom::SourceRegistrationError>
              storable_source = ParseSourceRegistration(
                  std::move(dict), source_time, std::move(*reporting_origin),
                  std::move(*source_origin), *source_type,
                  /*is_within_fenced_frame=*/false);

          if (!storable_source.has_value()) {
            *Error() << storable_source.error();
            return;
          }

          events_.push_back(AttributionSource{
              .source = std::move(*storable_source),
              .debug_permission = debug_permission,
          });
        });
  }

  void ParseTrigger(base::Value trigger) {
    if (!EnsureDictionary(trigger))
      return;

    base::Value::Dict& trigger_dict = trigger.GetDict();

    base::Time trigger_time = ParseTime(trigger_dict, kTimestampKey);
    absl::optional<SuitableOrigin> reporting_origin =
        ParseOrigin(trigger_dict, "reporting_origin");
    absl::optional<SuitableOrigin> destination_origin =
        ParseOrigin(trigger_dict, "destination_origin");
    bool debug_permission = ParseDebugPermission(trigger_dict);

    if (has_error())
      return;

    ParseAttributionEvent(
        trigger_dict, "Attribution-Reporting-Register-Trigger",
        [&](base::Value::Dict dict) {
          auto trigger_registration =
              attribution_reporting::TriggerRegistration::Parse(
                  std::move(dict));
          if (!trigger_registration.has_value()) {
            *Error() << trigger_registration.error();
            return;
          }

          events_.push_back(AttributionTriggerAndTime{
              .trigger = AttributionTrigger(std::move(*reporting_origin),
                                            std::move(*trigger_registration),
                                            std::move(*destination_origin),
                                            /*attestation=*/absl::nullopt,
                                            /*is_within_fenced_frame=*/false),
              .time = trigger_time,
              .debug_permission = debug_permission,
          });
        });
  }

  absl::optional<SuitableOrigin> ParseOrigin(const base::Value::Dict& dict,
                                             base::StringPiece key) {
    auto context = PushContext(key);

    absl::optional<SuitableOrigin> origin;
    if (const std::string* s = dict.FindString(key))
      origin = SuitableOrigin::Deserialize(*s);

    if (!origin.has_value())
      *Error() << "must be a valid, secure origin";

    return origin;
  }

  base::Time ParseTime(const base::Value::Dict& dict, base::StringPiece key) {
    auto context = PushContext(key);

    const std::string* v = dict.FindString(key);
    int64_t milliseconds;

    if (v && base::StringToInt64(*v, &milliseconds)) {
      base::Time time = offset_time_ + base::Milliseconds(milliseconds);
      if (!time.is_null() && !time.is_inf())
        return time;
    }

    *Error() << "must be an integer number of milliseconds since the Unix "
                "epoch formatted as a base-10 string";
    return base::Time();
  }

  absl::optional<bool> ParseBool(const base::Value::Dict& dict,
                                 base::StringPiece key) {
    auto context = PushContext(key);

    const base::Value* v = dict.Find(key);
    if (!v) {
      return absl::nullopt;
    }

    if (!v->is_bool()) {
      *Error() << "must be a bool";
      return absl::nullopt;
    }

    return v->GetBool();
  }

  bool ParseDebugPermission(const base::Value::Dict& dict) {
    return ParseBool(dict, "debug_permission").value_or(false);
  }

  absl::optional<AttributionSourceType> ParseSourceType(
      const base::Value::Dict& dict) {
    static constexpr char kKey[] = "source_type";
    static constexpr char kNavigation[] = "navigation";
    static constexpr char kEvent[] = "event";

    auto context = PushContext(kKey);

    absl::optional<AttributionSourceType> source_type;

    if (const std::string* v = dict.FindString(kKey)) {
      if (*v == kNavigation) {
        source_type = AttributionSourceType::kNavigation;
      } else if (*v == kEvent) {
        source_type = AttributionSourceType::kEvent;
      }
    }

    if (!source_type) {
      *Error() << "must be either \"" << kNavigation << "\" or \"" << kEvent
               << "\"";
    }

    return source_type;
  }

  bool ParseAttributionEvent(
      base::Value::Dict& value,
      base::StringPiece key,
      base::FunctionRef<void(base::Value::Dict)> parse_dict) {
    auto context = PushContext(key);

    base::Value* dict = value.Find(key);
    if (!dict) {
      *Error() << "must be present";
      return false;
    }

    if (!EnsureDictionary(*dict))
      return false;

    parse_dict(std::move(*dict).TakeDict());
    return true;
  }

  bool EnsureDictionary(const base::Value& value) {
    if (!value.is_dict()) {
      *Error() << "must be a dictionary";
      return false;
    }
    return true;
  }
};

}  // namespace

base::expected<AttributionSimulationEvents, std::string>
ParseAttributionSimulationInput(base::Value::Dict input,
                                const base::Time offset_time) {
  return AttributionSimulatorInputParser(offset_time).Parse(std::move(input));
}

}  // namespace content
