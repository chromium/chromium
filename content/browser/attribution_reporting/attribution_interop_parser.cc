// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/attribution_interop_parser.h"

#include <stddef.h>
#include <stdint.h>

#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/function_ref.h"
#include "base/functional/overloaded.h"
#include "base/memory/raw_ref.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "base/types/optional_util.h"
#include "base/values.h"
#include "components/attribution_reporting/source_registration.h"
#include "components/attribution_reporting/source_registration_error.mojom.h"
#include "components/attribution_reporting/source_type.mojom.h"
#include "components/attribution_reporting/suitable_origin.h"
#include "components/attribution_reporting/test_utils.h"
#include "components/attribution_reporting/trigger_registration.h"
#include "components/attribution_reporting/trigger_registration_error.mojom.h"
#include "content/browser/attribution_reporting/attribution_config.h"
#include "content/browser/attribution_reporting/attribution_trigger.h"
#include "content/browser/attribution_reporting/storable_source.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/abseil-cpp/absl/types/variant.h"

namespace content {

namespace {

using ::attribution_reporting::SuitableOrigin;
using ::attribution_reporting::mojom::SourceType;

constexpr char kAttributionSrcUrlKey[] = "attribution_src_url";
constexpr char kRegistrationRequestKey[] = "registration_request";
constexpr char kResponseKey[] = "response";
constexpr char kResponsesKey[] = "responses";

using Context = absl::variant<base::StringPiece, size_t>;
using ContextPath = std::vector<Context>;

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

  void operator()(base::StringPiece key) { *stream_ << "[\"" << key << "\"]"; }

  void operator()(size_t index) { *stream_ << '[' << index << ']'; }

 private:
  const raw_ref<std::ostringstream> stream_;
};

class AttributionInteropParser {
 public:
  explicit AttributionInteropParser(base::Time offset_time = base::Time())
      : offset_time_(offset_time) {}

  ~AttributionInteropParser() = default;

  AttributionInteropParser(const AttributionInteropParser&) = delete;
  AttributionInteropParser(AttributionInteropParser&&) = delete;

  AttributionInteropParser& operator=(const AttributionInteropParser&) = delete;
  AttributionInteropParser& operator=(AttributionInteropParser&&) = delete;

  base::expected<AttributionSimulationEvents, std::string> ParseInput(
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

    if (has_error_) {
      return base::unexpected(error_stream_.str());
    }

    base::ranges::sort(events_, /*comp=*/{}, &GetEventTime);
    return std::move(events_);
  }

  [[nodiscard]] std::string ParseConfig(const base::Value::Dict& dict,
                                        AttributionConfig& config,
                                        bool required) && {
    ParseInt(dict, "max_sources_per_origin", config.max_sources_per_origin,
             required);

    ParseInt(dict, "max_destinations_per_source_site_reporting_origin",
             config.max_destinations_per_source_site_reporting_origin,
             required);

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
        config.event_level_limit.event_source_randomized_response_rate,
        required);

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

    return error_stream_.str();
  }

 private:
  const base::Time offset_time_;

  std::ostringstream error_stream_;

  ContextPath context_path_;
  bool has_error_ = false;

  std::vector<AttributionSimulationEvent> events_;

  [[nodiscard]] ScopedContext PushContext(Context context) {
    return ScopedContext(context_path_, context);
  }

  ErrorWriter Error() {
    has_error_ = true;

    if (context_path_.empty()) {
      error_stream_ << "input root";
    }

    ErrorWriter writer(error_stream_);
    for (Context context : context_path_) {
      absl::visit(writer, context);
    }

    error_stream_ << ": ";
    return writer;
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
    absl::optional<SuitableOrigin> origin = ParseOrigin(dict, kUrlKey);
    if (has_error_) {
      return;
    }
    if (*origin != reporting_origin) {
      auto context = PushContext(kUrlKey);
      *Error() << "must match " << reporting_origin.Serialize();
    }
  }

  void ParseSource(base::Value::Dict source_dict) {
    base::Time source_time = ParseDistinctTime(source_dict);

    absl::optional<SuitableOrigin> source_origin;
    absl::optional<SuitableOrigin> reporting_origin;
    absl::optional<SourceType> source_type;

    ParseDict(source_dict, kRegistrationRequestKey,
              [&](base::Value::Dict dict) {
                source_origin = ParseOrigin(dict, "source_origin");
                reporting_origin = ParseOrigin(dict, kAttributionSrcUrlKey);
                source_type = ParseSourceType(dict);
              });

    if (has_error_) {
      return;
    }

    auto context = PushContext(kResponsesKey);
    ParseListOfDicts(
        source_dict.Find(kResponsesKey),
        [&](base::Value::Dict dict) {
          VerifyReportingOrigin(dict, *reporting_origin);

          bool debug_permission = ParseDebugPermission(dict);

          if (has_error_) {
            return;
          }

          ParseDict(dict, kResponseKey, [&](base::Value::Dict response_dict) {
            ParseDict(
                response_dict, "Attribution-Reporting-Register-Source",
                [&](base::Value::Dict registration_dict) {
                  auto registration =
                      attribution_reporting::SourceRegistration::Parse(
                          std::move(registration_dict));
                  if (!registration.has_value()) {
                    *Error() << registration.error();
                    return;
                  }

                  events_.emplace_back(
                      StorableSource(std::move(*reporting_origin),
                                     std::move(*registration), source_time,
                                     std::move(*source_origin), *source_type,
                                     /*is_within_fenced_frame=*/false),
                      debug_permission);
                });
          });
        },
        /*expected_size=*/1);
  }

  void ParseTrigger(base::Value::Dict trigger_dict) {
    base::Time trigger_time = ParseDistinctTime(trigger_dict);

    absl::optional<SuitableOrigin> destination_origin;
    absl::optional<SuitableOrigin> reporting_origin;

    ParseDict(trigger_dict, kRegistrationRequestKey,
              [&](base::Value::Dict dict) {
                destination_origin = ParseOrigin(dict, "destination_origin");
                reporting_origin = ParseOrigin(dict, kAttributionSrcUrlKey);
              });

    if (has_error_) {
      return;
    }

    auto context = PushContext(kResponsesKey);
    ParseListOfDicts(
        trigger_dict.Find(kResponsesKey),
        [&](base::Value::Dict dict) {
          VerifyReportingOrigin(dict, *reporting_origin);

          bool debug_permission = ParseDebugPermission(dict);

          if (has_error_) {
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

                        events_.emplace_back(
                            AttributionTriggerAndTime{
                                .trigger = AttributionTrigger(
                                    std::move(*reporting_origin),
                                    std::move(*trigger_registration),
                                    std::move(*destination_origin),
                                    /*attestation=*/absl::nullopt,
                                    /*is_within_fenced_frame=*/false),
                                .time = trigger_time,
                            },
                            debug_permission);
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

  base::Time ParseDistinctTime(const base::Value::Dict& dict) {
    static constexpr char kTimestampKey[] = "timestamp";

    auto context = PushContext(kTimestampKey);

    const std::string* v = dict.FindString(kTimestampKey);
    int64_t milliseconds;

    if (v && base::StringToInt64(*v, &milliseconds)) {
      base::Time time = offset_time_ + base::Milliseconds(milliseconds);
      if (!time.is_null() && !time.is_inf()) {
        if (base::ranges::find(events_, time, &GetEventTime) != events_.end()) {
          *Error() << "must be distinct from all others: " << milliseconds;
        }
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

  absl::optional<SourceType> ParseSourceType(const base::Value::Dict& dict) {
    static constexpr char kKey[] = "source_type";
    static constexpr char kNavigation[] = "navigation";
    static constexpr char kEvent[] = "event";

    auto context = PushContext(kKey);

    absl::optional<SourceType> source_type;

    if (const std::string* v = dict.FindString(kKey)) {
      if (*v == kNavigation) {
        source_type = SourceType::kNavigation;
      } else if (*v == kEvent) {
        source_type = SourceType::kEvent;
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
};

}  // namespace

AttributionSimulationEvent::AttributionSimulationEvent(
    absl::variant<StorableSource, AttributionTriggerAndTime> event,
    bool debug_permission)
    : event(std::move(event)), debug_permission(debug_permission) {}

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

base::Time GetEventTime(const AttributionSimulationEvent& event) {
  return absl::visit(
      base::Overloaded{
          [](const StorableSource& source) {
            return source.common_info().source_time();
          },
          [](const AttributionTriggerAndTime& trigger) { return trigger.time; },
      },
      event.event);
}

}  // namespace content
