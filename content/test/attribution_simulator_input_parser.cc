// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/test/attribution_simulator_input_parser.h"

#include <stddef.h>
#include <stdint.h>

#include <ostream>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/test/bind.h"
#include "base/time/time.h"
#include "base/values.h"
#include "content/browser/attribution_reporting/attribution_aggregatable_source.h"
#include "content/browser/attribution_reporting/attribution_aggregatable_trigger.h"
#include "content/browser/attribution_reporting/attribution_filter_data.h"
#include "content/browser/attribution_reporting/attribution_source_type.h"
#include "content/browser/attribution_reporting/attribution_trigger.h"
#include "content/browser/attribution_reporting/common_source_info.h"
#include "content/browser/attribution_reporting/storable_source.h"
#include "services/network/public/cpp/is_potentially_trustworthy.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

namespace {

using Context = absl::variant<base::StringPiece, size_t>;
using ContextPath = std::vector<Context>;

class ScopedContext {
 public:
  ScopedContext(ContextPath& path, Context context) : path_(path) {
    path_.push_back(context);
  }

  ~ScopedContext() { path_.pop_back(); }

  ScopedContext(const ScopedContext&) = delete;
  ScopedContext(ScopedContext&&) = delete;

  ScopedContext& operator=(const ScopedContext&) = delete;
  ScopedContext& operator=(ScopedContext&&) = delete;

 private:
  ContextPath& path_;
};

// Writes a newline on destruction.
class ErrorWriter {
 public:
  explicit ErrorWriter(std::ostream& stream) : stream_(stream) {}

  ~ErrorWriter() { stream_ << std::endl; }

  ErrorWriter(const ErrorWriter&) = delete;
  ErrorWriter(ErrorWriter&&) = default;

  ErrorWriter& operator=(const ErrorWriter&) = delete;
  ErrorWriter& operator=(ErrorWriter&&) = delete;

  std::ostream& operator*() { return stream_; }

  void operator()(base::StringPiece key) { stream_ << "[\"" << key << "\"]"; }

  void operator()(size_t index) { stream_ << '[' << index << ']'; }

 private:
  std::ostream& stream_;
};

class AttributionSimulatorInputParser {
 public:
  AttributionSimulatorInputParser(base::Time offset_time,
                                  std::ostream& error_stream)
      : offset_time_(offset_time), error_stream_(error_stream) {}

  ~AttributionSimulatorInputParser() = default;

  AttributionSimulatorInputParser(const AttributionSimulatorInputParser&) =
      delete;
  AttributionSimulatorInputParser(AttributionSimulatorInputParser&&) = delete;

  AttributionSimulatorInputParser& operator=(
      const AttributionSimulatorInputParser&) = delete;
  AttributionSimulatorInputParser& operator=(
      AttributionSimulatorInputParser&&) = delete;

  absl::optional<AttributionSimulationEventAndValues> Parse(
      base::Value input) && {
    if (!EnsureDictionary(input))
      return absl::nullopt;

    static constexpr char kKeySources[] = "sources";
    if (base::Value* sources = input.FindKey(kKeySources)) {
      auto context = PushContext(kKeySources);
      ParseList(
          std::move(*sources),
          base::BindRepeating(&AttributionSimulatorInputParser::ParseSource,
                              base::Unretained(this)));
    }

    static constexpr char kKeyTriggers[] = "triggers";
    if (base::Value* triggers = input.FindKey(kKeyTriggers)) {
      auto context = PushContext(kKeyTriggers);
      ParseList(
          std::move(*triggers),
          base::BindRepeating(&AttributionSimulatorInputParser::ParseTrigger,
                              base::Unretained(this)));
    }

    if (has_error_)
      return absl::nullopt;

    return std::move(events_);
  }

 private:
  const base::Time offset_time_;
  std::ostream& error_stream_;

  ContextPath context_path_;
  bool has_error_ = false;
  std::vector<AttributionSimulationEventAndValue> events_;

  ScopedContext PushContext(Context context) {
    return ScopedContext(context_path_, context);
  }

  ErrorWriter Error() {
    has_error_ = true;

    if (context_path_.empty())
      error_stream_ << "input root";

    ErrorWriter writer(error_stream_);
    for (Context context : context_path_) {
      absl::visit(writer, context);
    }

    error_stream_ << ": ";
    return writer;
  }

  template <typename T>
  void ParseList(T&& values,
                 base::RepeatingCallback<void(decltype(values))> callback) {
    if (!values.is_list()) {
      *Error() << "must be a list";
      return;
    }

    size_t index = 0;
    for (auto&& value : values.GetList()) {
      auto index_context = PushContext(index);
      callback.Run(std::forward<T>(value));
      index++;
    }
  }

  void ParseSource(base::Value&& source) {
    if (!EnsureDictionary(source))
      return;

    base::Time source_time = ParseTime(source, "source_time");
    url::Origin source_origin = ParseOrigin(source, "source_origin");
    url::Origin reporting_origin = ParseOrigin(source, "reporting_origin");
    absl::optional<AttributionSourceType> source_type = ParseSourceType(source);

    uint64_t source_event_id = 0;
    url::Origin destination_origin;
    absl::optional<uint64_t> debug_key;
    int64_t priority = 0;
    base::TimeDelta expiry;
    AttributionFilterData filter_data;

    if (!ParseRegistrationConfig(
            source, base::BindLambdaForTesting([&](const base::Value& cfg) {
              source_event_id = ParseRequiredUint64(cfg, "source_event_id");
              destination_origin = ParseOrigin(cfg, "destination");
              debug_key = ParseOptionalUint64(cfg, "debug_key");
              priority = ParseOptionalInt64(cfg, "priority").value_or(0);
              expiry = ParseSourceExpiry(cfg).value_or(base::Days(30));
              filter_data = ParseFilterData(
                  cfg, "filter_data",
                  &AttributionFilterData::FromSourceFilterValues);
            }))) {
      return;
    }

    // TODO(linnan): Support aggregatable reports in the simulator.

    if (has_error_)
      return;

    events_.emplace_back(
        StorableSource(CommonSourceInfo(
            source_event_id, std::move(source_origin),
            std::move(destination_origin), std::move(reporting_origin),
            source_time,
            CommonSourceInfo::GetExpiryTime(expiry, source_time, *source_type),
            *source_type, priority, std::move(filter_data), debug_key,
            AttributionAggregatableSource())),
        std::move(source));
  }

  void ParseTrigger(base::Value&& trigger) {
    if (!EnsureDictionary(trigger))
      return;

    base::Time trigger_time = ParseTime(trigger, "trigger_time");
    url::Origin reporting_origin = ParseOrigin(trigger, "reporting_origin");
    url::Origin destination_origin = ParseOrigin(trigger, "destination");

    absl::optional<uint64_t> debug_key;
    AttributionFilterData filters;
    std::vector<AttributionTrigger::EventTriggerData> event_triggers;

    if (!ParseRegistrationConfig(
            trigger, base::BindLambdaForTesting([&](const base::Value& cfg) {
              debug_key = ParseOptionalUint64(cfg, "debug_key");

              filters = ParseFilterData(
                  cfg, "filters",
                  &AttributionFilterData::FromTriggerFilterValues);

              event_triggers = ParseEventTriggers(cfg);
            }))) {
      return;
    }

    if (has_error_)
      return;

    // TODO(linnan): Support aggregatable reports in the simulator.

    events_.emplace_back(
        AttributionTriggerAndTime{
            .trigger = AttributionTrigger(
                std::move(destination_origin), std::move(reporting_origin),
                std::move(filters), debug_key, std::move(event_triggers),
                AttributionAggregatableTrigger()),
            .time = trigger_time,
        },
        std::move(trigger));
  }

  std::vector<AttributionTrigger::EventTriggerData> ParseEventTriggers(
      const base::Value& cfg) {
    std::vector<AttributionTrigger::EventTriggerData> event_triggers;

    static constexpr char kKey[] = "event_triggers";

    const base::Value* values = cfg.FindKey(kKey);
    if (!values)
      return event_triggers;

    auto context = PushContext(kKey);
    ParseList(
        *values,
        base::BindLambdaForTesting([&](const base::Value& event_trigger) {
          uint64_t trigger_data =
              ParseOptionalUint64(event_trigger, "trigger_data").value_or(0);

          int64_t priority =
              ParseOptionalInt64(event_trigger, "priority").value_or(0);

          absl::optional<uint64_t> dedup_key =
              ParseOptionalUint64(event_trigger, "deduplication_key");

          AttributionFilterData filters =
              ParseFilterData(event_trigger, "filters",
                              &AttributionFilterData::FromTriggerFilterValues);

          AttributionFilterData not_filters =
              ParseFilterData(event_trigger, "not_filters",
                              &AttributionFilterData::FromTriggerFilterValues);

          if (has_error_)
            return;

          event_triggers.emplace_back(trigger_data, priority, dedup_key,
                                      std::move(filters),
                                      std::move(not_filters));
        }));

    return event_triggers;
  }

  url::Origin ParseOrigin(const base::Value& dict, base::StringPiece key) {
    auto context = PushContext(key);

    url::Origin origin;

    if (const std::string* v = dict.FindStringKey(key))
      origin = url::Origin::Create(GURL(*v));

    if (!network::IsOriginPotentiallyTrustworthy(origin))
      *Error() << "must be a valid, secure origin";

    return origin;
  }

  base::Time ParseTime(const base::Value& dict, base::StringPiece key) {
    auto context = PushContext(key);

    absl::optional<int> v = dict.FindIntKey(key);
    if (!v) {
      *Error() << "must be an integer number of seconds since the Unix epoch";
      return base::Time();
    }

    return offset_time_ + base::Seconds(*v);
  }

  uint64_t ParseUint64(const std::string* s, base::StringPiece key) {
    auto context = PushContext(key);

    uint64_t value = 0;

    if (!s || !base::StringToUint64(*s, &value))
      *Error() << "must be a uint64 formatted as a base-10 string";

    return value;
  }

  int64_t ParseInt64(const std::string* s, base::StringPiece key) {
    auto context = PushContext(key);

    int64_t value = 0;

    if (!s || !base::StringToInt64(*s, &value))
      *Error() << "must be an int64 formatted as a base-10 string";

    return value;
  }

  uint64_t ParseRequiredUint64(const base::Value& dict, base::StringPiece key) {
    return ParseUint64(dict.FindStringKey(key), key);
  }

  absl::optional<uint64_t> ParseOptionalUint64(const base::Value& dict,
                                               base::StringPiece key) {
    const base::Value* value = dict.FindKey(key);
    if (!value)
      return absl::nullopt;

    return ParseUint64(value->GetIfString(), key);
  }

  absl::optional<int64_t> ParseOptionalInt64(const base::Value& dict,
                                             base::StringPiece key) {
    const base::Value* value = dict.FindKey(key);
    if (!value)
      return absl::nullopt;

    return ParseInt64(value->GetIfString(), key);
  }

  absl::optional<AttributionSourceType> ParseSourceType(
      const base::Value& dict) {
    static constexpr char kKey[] = "source_type";
    static constexpr char kNavigation[] = "navigation";
    static constexpr char kEvent[] = "event";

    auto context = PushContext(kKey);

    absl::optional<AttributionSourceType> source_type;

    if (const std::string* v = dict.FindStringKey(kKey)) {
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

  bool ParseRegistrationConfig(
      base::Value& dict,
      base::OnceCallback<void(const base::Value&)> callback) {
    static constexpr char kKey[] = "registration_config";

    auto context = PushContext(kKey);

    const base::Value* cfg = dict.FindKey(kKey);
    if (!cfg) {
      *Error() << "must be present";
      return false;
    }

    if (!EnsureDictionary(*cfg))
      return false;

    std::move(callback).Run(*cfg);
    return true;
  }

  using FromFilterValuesFunc = absl::optional<AttributionFilterData>(
      AttributionFilterData::FilterValues&&);

  AttributionFilterData ParseFilterData(
      const base::Value& dict,
      base::StringPiece key,
      FromFilterValuesFunc from_filter_values) {
    auto context = PushContext(key);

    const base::Value* value = dict.FindKey(key);
    if (!value)
      return AttributionFilterData();

    if (!EnsureDictionary(*value))
      return AttributionFilterData();

    AttributionFilterData::FilterValues::container_type container;
    for (auto [filter, values_list] : value->GetDict()) {
      auto filter_context = PushContext(filter);
      std::vector<std::string> values;

      ParseList(values_list,
                base::BindLambdaForTesting([&](const base::Value& value) {
                  if (!value.is_string()) {
                    *Error() << "must be a string";
                  } else {
                    values.emplace_back(value.GetString());
                  }
                }));

      container.emplace_back(filter, std::move(values));
    }

    absl::optional<AttributionFilterData> filter_data =
        from_filter_values(std::move(container));
    // TODO(apaseltiner): Provide more detailed information.
    if (!filter_data)
      *Error() << "invalid";

    return std::move(filter_data).value_or(AttributionFilterData());
  }

  absl::optional<base::TimeDelta> ParseSourceExpiry(const base::Value& dict) {
    static constexpr char kKey[] = "expiry";

    auto context = PushContext(kKey);

    const base::Value* value = dict.FindKey(kKey);
    if (!value)
      return absl::nullopt;

    absl::optional<base::TimeDelta> expiry;

    if (const std::string* s = value->GetIfString()) {
      int64_t milliseconds = 0;
      if (base::StringToInt64(*s, &milliseconds))
        expiry = base::Milliseconds(milliseconds);
    }

    if (!expiry || *expiry < base::TimeDelta()) {
      *Error() << "must be a positive number of milliseconds formatted as a "
                  "base-10 string";
    }

    return expiry;
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

absl::optional<AttributionSimulationEventAndValues>
ParseAttributionSimulationInput(base::Value input,
                                const base::Time offset_time,
                                std::ostream& error_stream) {
  return AttributionSimulatorInputParser(offset_time, error_stream)
      .Parse(std::move(input));
}

}  // namespace content
