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
#include "base/strings/abseil_string_number_conversions.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/test/bind.h"
#include "base/time/time.h"
#include "base/values.h"
#include "content/browser/attribution_reporting/attribution_aggregatable_source.h"
#include "content/browser/attribution_reporting/attribution_aggregatable_trigger.h"
#include "content/browser/attribution_reporting/attribution_filter_data.h"
#include "content/browser/attribution_reporting/attribution_reporting.pb.h"
#include "content/browser/attribution_reporting/attribution_source_type.h"
#include "content/browser/attribution_reporting/attribution_trigger.h"
#include "content/browser/attribution_reporting/common_source_info.h"
#include "content/browser/attribution_reporting/storable_source.h"
#include "services/network/public/cpp/is_potentially_trustworthy.h"
#include "third_party/abseil-cpp/absl/numeric/int128.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "third_party/blink/public/mojom/conversions/attribution_data_host.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

namespace {

using Context = absl::variant<base::StringPiece, size_t>;
using ContextPath = std::vector<Context>;

constexpr char kTimestampKey[] = "timestamp";

constexpr char kAggregatableTriggerDataKey[] =
    "Attribution-Reporting-Register-Aggregatable-Trigger-Data";
constexpr char kAggregatableValuesKey[] =
    "Attribution-Reporting-Register-Aggregatable-Values";

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

    base::Time source_time = ParseTime(source, kTimestampKey);
    url::Origin source_origin = ParseOrigin(source, "source_origin");
    url::Origin reporting_origin = ParseOrigin(source, "reporting_origin");
    absl::optional<AttributionSourceType> source_type = ParseSourceType(source);

    uint64_t source_event_id = 0;
    url::Origin destination_origin;
    absl::optional<uint64_t> debug_key;
    int64_t priority = 0;
    base::TimeDelta expiry;
    AttributionFilterData filter_data;

    if (!ParseAttributionSource(
            source, base::BindLambdaForTesting([&](const base::Value& dict) {
              source_event_id = ParseRequiredUint64(dict, "source_event_id");
              destination_origin = ParseOrigin(dict, "destination");
              debug_key = ParseOptionalUint64(dict, "debug_key");
              priority = ParseOptionalInt64(dict, "priority").value_or(0);
              expiry = ParseSourceExpiry(dict).value_or(base::Days(30));
              filter_data = ParseFilterData(
                  dict, "filter_data",
                  &AttributionFilterData::FromSourceFilterValues);
            }))) {
      return;
    }

    AttributionAggregatableSource aggregatable_source =
        ParseAggregatableSource(source);

    if (has_error_)
      return;

    events_.emplace_back(
        StorableSource(CommonSourceInfo(
            source_event_id, std::move(source_origin),
            std::move(destination_origin), std::move(reporting_origin),
            source_time,
            CommonSourceInfo::GetExpiryTime(expiry, source_time, *source_type),
            *source_type, priority, std::move(filter_data), debug_key,
            std::move(aggregatable_source))),
        std::move(source));
  }

  void ParseTrigger(base::Value&& trigger) {
    if (!EnsureDictionary(trigger))
      return;

    base::Time trigger_time = ParseTime(trigger, kTimestampKey);
    url::Origin reporting_origin = ParseOrigin(trigger, "reporting_origin");
    url::Origin destination_origin = ParseOrigin(trigger, "destination_origin");

    absl::optional<uint64_t> debug_key =
        ParseOptionalUint64(trigger, "Attribution-Reporting-Trigger-Debug-Key");
    AttributionFilterData filters =
        ParseFilterData(trigger, "Attribution-Reporting-Filters",
                        &AttributionFilterData::FromTriggerFilterValues);
    std::vector<AttributionTrigger::EventTriggerData> event_triggers =
        ParseEventTriggers(trigger);

    AttributionAggregatableTrigger aggregatable_trigger =
        ParseAggregatableTrigger(trigger);

    if (has_error_)
      return;

    events_.emplace_back(
        AttributionTriggerAndTime{
            .trigger = AttributionTrigger(
                std::move(destination_origin), std::move(reporting_origin),
                std::move(filters), debug_key, std::move(event_triggers),
                std::move(aggregatable_trigger)),
            .time = trigger_time,
        },
        std::move(trigger));
  }

  std::vector<AttributionTrigger::EventTriggerData> ParseEventTriggers(
      const base::Value& cfg) {
    std::vector<AttributionTrigger::EventTriggerData> event_triggers;

    static constexpr char kKey[] =
        "Attribution-Reporting-Register-Event-Trigger";

    const base::Value* values = cfg.FindKey(kKey);
    if (!values)
      return event_triggers;

    auto context = PushContext(kKey);
    ParseList(
        *values,
        base::BindLambdaForTesting([&](const base::Value& event_trigger) {
          if (!EnsureDictionary(event_trigger))
            return;

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

  bool ParseAttributionSource(
      const base::Value& value,
      base::OnceCallback<void(const base::Value&)> callback) {
    static constexpr char kKey[] = "Attribution-Reporting-Register-Source";

    auto context = PushContext(kKey);

    const base::Value* dict = value.FindKey(kKey);
    if (!dict) {
      *Error() << "must be present";
      return false;
    }

    if (!EnsureDictionary(*dict))
      return false;

    std::move(callback).Run(*dict);
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

  absl::uint128 ParseAggregatableKey(const base::Value& dict) {
    static constexpr char kKey[] = "key_piece";

    auto context = PushContext(kKey);

    const std::string* s = dict.FindStringKey(kKey);

    absl::uint128 value = 0;
    if (!s || !base::HexStringToUInt128(*s, &value))
      *Error() << "must be a uint128 formatted as a base-16 string";

    return value;
  }

  std::string ParseAggregatableKeyId(const base::Value& dict) {
    static constexpr char kKey[] = "id";

    auto context = PushContext(kKey);

    const std::string* s = dict.FindStringKey(kKey);
    if (!s)
      *Error() << "must be a string";

    return s ? *s : "";
  }

  AttributionAggregatableSource ParseAggregatableSource(
      const base::Value& cfg) {
    static constexpr char kKey[] =
        "Attribution-Reporting-Register-Aggregatable-Source";

    const base::Value* values = cfg.FindKey(kKey);
    if (!values)
      return AttributionAggregatableSource();

    proto::AttributionAggregatableSource proto;

    auto context = PushContext(kKey);

    ParseList(
        *values, base::BindLambdaForTesting([&](const base::Value& value) {
          if (!EnsureDictionary(value))
            return;

          std::string id = ParseAggregatableKeyId(value);
          absl::uint128 key = ParseAggregatableKey(value);

          if (has_error_)
            return;

          proto::AttributionAggregatableKey proto_key;
          proto_key.set_high_bits(absl::Uint128High64(key));
          proto_key.set_low_bits(absl::Uint128Low64(key));

          (*proto.mutable_keys())[std::move(id)] = std::move(proto_key);
        }));

    absl::optional<AttributionAggregatableSource> aggregatable_source =
        AttributionAggregatableSource::Create(proto);
    if (!aggregatable_source)
      *Error() << "invalid";

    return std::move(aggregatable_source)
        .value_or(AttributionAggregatableSource());
  }

  std::vector<std::string> ParseAggregatableTriggerDataSourceKeys(
      const base::Value& dict) {
    static constexpr char kKey[] = "source_keys";

    std::vector<std::string> source_keys;

    auto context = PushContext(kKey);

    const base::Value* values = dict.FindKey(kKey);
    if (!values) {
      *Error() << "must be present";
      return source_keys;
    }

    ParseList(*values,
              base::BindLambdaForTesting([&](const base::Value& value) {
                if (!value.is_string()) {
                  *Error() << "must be a string";
                } else {
                  source_keys.emplace_back(value.GetString());
                }
              }));

    return source_keys;
  }

  std::vector<blink::mojom::AttributionAggregatableTriggerDataPtr>
  ParseAggregatableTriggerData(const base::Value& dict) {
    std::vector<blink::mojom::AttributionAggregatableTriggerDataPtr>
        aggregatable_triggers;

    auto context = PushContext(kAggregatableTriggerDataKey);

    const base::Value* values = dict.FindKey(kAggregatableTriggerDataKey);
    if (!values) {
      *Error() << "must be present";
      return aggregatable_triggers;
    }

    ParseList(
        *values,
        base::BindLambdaForTesting(
            [&](const base::Value& aggregatable_trigger) {
              if (!EnsureDictionary(aggregatable_trigger))
                return;

              std::vector<std::string> source_keys =
                  ParseAggregatableTriggerDataSourceKeys(aggregatable_trigger);

              absl::uint128 key = ParseAggregatableKey(aggregatable_trigger);

              AttributionFilterData filters = ParseFilterData(
                  aggregatable_trigger, "filters",
                  &AttributionFilterData::FromTriggerFilterValues);

              AttributionFilterData not_filters = ParseFilterData(
                  aggregatable_trigger, "not_filters",
                  &AttributionFilterData::FromTriggerFilterValues);

              if (has_error_)
                return;

              aggregatable_triggers.push_back(
                  blink::mojom::AttributionAggregatableTriggerData::New(
                      blink::mojom::AttributionAggregatableKey::New(
                          absl::Uint128High64(key), absl::Uint128Low64(key)),
                      std::move(source_keys),
                      blink::mojom::AttributionFilterData::New(
                          std::move(filters.filter_values())),
                      blink::mojom::AttributionFilterData::New(
                          std::move(not_filters.filter_values()))));
            }));

    return aggregatable_triggers;
  }

  AttributionAggregatableTrigger::Values ParseAggregatableValues(
      const base::Value& dict) {
    AttributionAggregatableTrigger::Values aggregatable_values;

    auto context = PushContext(kAggregatableValuesKey);

    const base::Value* value = dict.FindKey(kAggregatableValuesKey);
    if (!value) {
      *Error() << "must be present";
      return aggregatable_values;
    }

    if (!EnsureDictionary(*value))
      return aggregatable_values;

    AttributionAggregatableTrigger::Values::container_type container;

    for (auto [id, key_value] : value->GetDict()) {
      auto key_context = PushContext(id);
      if (!key_value.is_int() || key_value.GetInt() <= 0) {
        *Error() << "must be a positive integer";
      } else {
        container.emplace_back(id, key_value.GetInt());
      }
    }

    return container;
  }

  AttributionAggregatableTrigger ParseAggregatableTrigger(
      const base::Value& dict) {
    if (!dict.FindKey(kAggregatableTriggerDataKey) &&
        !dict.FindKey(kAggregatableValuesKey)) {
      return AttributionAggregatableTrigger();
    }

    auto mojo = blink::mojom::AttributionAggregatableTrigger::New(
        ParseAggregatableTriggerData(dict), ParseAggregatableValues(dict));

    absl::optional<AttributionAggregatableTrigger> aggregatable_trigger =
        AttributionAggregatableTrigger::FromMojo(std::move(mojo));
    if (!aggregatable_trigger)
      *Error() << "invalid";

    return std::move(aggregatable_trigger)
        .value_or(AttributionAggregatableTrigger());
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
