// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/attribution_simulator_input_parser.h"

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
#include "components/attribution_reporting/test_utils.h"
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

constexpr char kAttributionSrcUrlKey[] = "attribution_src_url";
constexpr char kRegistrationRequestKey[] = "registration_request";
constexpr char kResponseKey[] = "response";
constexpr char kResponsesKey[] = "responses";
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
      ParseListOfDicts(sources, [&](base::Value::Dict source) {
        ParseSource(std::move(source));
      });
    }

    static constexpr char kKeyTriggers[] = "triggers";
    if (base::Value* triggers = input.Find(kKeyTriggers)) {
      auto context = PushContext(kKeyTriggers);
      ParseListOfDicts(triggers, [&](base::Value::Dict trigger) {
        ParseTrigger(std::move(trigger));
      });
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
    absl::optional<SuitableOrigin> origin = ParseOrigin(dict, kUrlKey);
    if (has_error()) {
      return;
    }
    if (*origin != reporting_origin) {
      auto context = PushContext(kUrlKey);
      *Error() << "must match " << reporting_origin.Serialize();
    }
  }

  void ParseSource(base::Value::Dict source_dict) {
    base::Time source_time = ParseTime(source_dict, kTimestampKey);

    absl::optional<SuitableOrigin> source_origin;
    absl::optional<SuitableOrigin> reporting_origin;
    absl::optional<AttributionSourceType> source_type;

    ParseDict(source_dict, kRegistrationRequestKey,
              [&](base::Value::Dict dict) {
                source_origin = ParseOrigin(dict, "source_origin");
                reporting_origin = ParseOrigin(dict, kAttributionSrcUrlKey);
                source_type = ParseSourceType(dict);
              });

    if (has_error()) {
      return;
    }

    auto context = PushContext(kResponsesKey);
    ParseListOfDicts(
        source_dict.Find(kResponsesKey),
        [&](base::Value::Dict dict) {
          VerifyReportingOrigin(dict, *reporting_origin);

          bool debug_permission = ParseDebugPermission(dict);

          if (has_error()) {
            return;
          }

          ParseDict(dict, kResponseKey, [&](base::Value::Dict response_dict) {
            ParseDict(
                response_dict, "Attribution-Reporting-Register-Source",
                [&](base::Value::Dict registration_dict) {
                  base::expected<
                      StorableSource,
                      attribution_reporting::mojom::SourceRegistrationError>
                      storable_source = ParseSourceRegistration(
                          std::move(registration_dict), source_time,
                          std::move(*reporting_origin),
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
          });
        },
        /*expected_size=*/1);
  }

  void ParseTrigger(base::Value::Dict trigger_dict) {
    base::Time trigger_time = ParseTime(trigger_dict, kTimestampKey);

    absl::optional<SuitableOrigin> destination_origin;
    absl::optional<SuitableOrigin> reporting_origin;

    ParseDict(trigger_dict, kRegistrationRequestKey,
              [&](base::Value::Dict dict) {
                destination_origin = ParseOrigin(dict, "destination_origin");
                reporting_origin = ParseOrigin(dict, kAttributionSrcUrlKey);
              });

    if (has_error()) {
      return;
    }

    auto context = PushContext(kResponsesKey);
    ParseListOfDicts(
        trigger_dict.Find(kResponsesKey),
        [&](base::Value::Dict dict) {
          VerifyReportingOrigin(dict, *reporting_origin);

          bool debug_permission = ParseDebugPermission(dict);

          if (has_error()) {
            return;
          }

          ParseDict(dict, kResponseKey, [&](base::Value::Dict response_dict) {
            ParseDict(response_dict, "Attribution-Reporting-Register-Trigger",
                      [&](base::Value::Dict registration_dict) {
                        auto trigger_registration =
                            attribution_reporting::TriggerRegistration::Parse(
                                std::move(registration_dict));
                        if (!trigger_registration.has_value()) {
                          *Error() << trigger_registration.error();
                          return;
                        }

                        events_.push_back(AttributionTriggerAndTime{
                            .trigger = AttributionTrigger(
                                std::move(*reporting_origin),
                                std::move(*trigger_registration),
                                std::move(*destination_origin),
                                /*attestation=*/absl::nullopt,
                                /*is_within_fenced_frame=*/false),
                            .time = trigger_time,
                            .debug_permission = debug_permission,
                        });
                      });
          });
        },
        /*expected_size=*/1);
  }

  absl::optional<SuitableOrigin> ParseOrigin(const base::Value::Dict& dict,
                                             base::StringPiece key) {
    auto context = PushContext(key);

    absl::optional<SuitableOrigin> origin;
    if (const std::string* s = dict.FindString(key)) {
      origin = SuitableOrigin::Deserialize(*s);
    }

    if (!origin.has_value()) {
      *Error() << "must be a valid, secure origin";
    }

    return origin;
  }

  base::Time ParseTime(const base::Value::Dict& dict, base::StringPiece key) {
    auto context = PushContext(key);

    const std::string* v = dict.FindString(key);
    int64_t milliseconds;

    if (v && base::StringToInt64(*v, &milliseconds)) {
      base::Time time = offset_time_ + base::Milliseconds(milliseconds);
      if (!time.is_null() && !time.is_inf()) {
        return time;
      }
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
};

}  // namespace

base::expected<AttributionSimulationEvents, std::string>
ParseAttributionSimulationInput(base::Value::Dict input,
                                const base::Time offset_time) {
  return AttributionSimulatorInputParser(offset_time).Parse(std::move(input));
}

}  // namespace content
