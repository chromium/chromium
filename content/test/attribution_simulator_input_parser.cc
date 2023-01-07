// Copyright 2022 The Chromium Authors
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
#include "base/strings/string_util.h"
#include "base/test/bind.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "base/values.h"
#include "content/browser/attribution_reporting/attribution_aggregatable_trigger_data.h"
#include "content/browser/attribution_reporting/attribution_aggregatable_values.h"
#include "content/browser/attribution_reporting/attribution_filter_data.h"
#include "content/browser/attribution_reporting/attribution_header_utils.h"
#include "content/browser/attribution_reporting/attribution_parser_test_utils.h"
#include "content/browser/attribution_reporting/attribution_reporting.mojom.h"
#include "content/browser/attribution_reporting/attribution_source_type.h"
#include "content/browser/attribution_reporting/attribution_trigger.h"
#include "content/browser/attribution_reporting/storable_source.h"
#include "net/cookies/canonical_cookie.h"
#include "services/network/public/cpp/is_potentially_trustworthy.h"
#include "third_party/abseil-cpp/absl/numeric/int128.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "third_party/blink/public/common/attribution_reporting/constants.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

namespace {

constexpr char kTimestampKey[] = "timestamp";

class AttributionSimulatorInputParser {
 public:
  AttributionSimulatorInputParser(base::Time offset_time,
                                  std::ostream& error_stream)
      : offset_time_(offset_time), error_manager_(error_stream) {}

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

    static constexpr char kKeyCookies[] = "cookies";
    if (base::Value* cookies = input.FindKey(kKeyCookies)) {
      auto context = PushContext(kKeyCookies);
      ParseList(
          std::move(*cookies),
          base::BindRepeating(&AttributionSimulatorInputParser::ParseCookie,
                              base::Unretained(this)));
    }

    static constexpr char kKeyDataClears[] = "data_clears";
    if (base::Value* data_clears = input.FindKey(kKeyDataClears)) {
      auto context = PushContext(kKeyDataClears);
      ParseList(
          std::move(*data_clears),
          base::BindRepeating(&AttributionSimulatorInputParser::ParseDataClear,
                              base::Unretained(this)));
    }

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

    if (has_error())
      return absl::nullopt;

    return std::move(events_);
  }

 private:
  const base::Time offset_time_;
  AttributionParserErrorManager error_manager_;

  std::vector<AttributionSimulationEventAndValue> events_;

  [[nodiscard]] std::unique_ptr<AttributionParserErrorManager::ScopedContext>
  PushContext(AttributionParserErrorManager::Context context) {
    return error_manager_.PushContext(context);
  }

  AttributionParserErrorManager::ErrorWriter Error() {
    return error_manager_.Error();
  }

  bool has_error() const { return error_manager_.has_error(); }

  template <typename T>
  void ParseList(T&& values,
                 base::RepeatingCallback<void(decltype(values))> callback,
                 size_t max_size = 0) {
    if (!values.is_list()) {
      *Error() << "must be a list";
      return;
    }

    if (max_size > 0 && values.GetList().size() > max_size) {
      *Error() << "too many elements";
      return;
    }

    size_t index = 0;
    for (auto&& value : values.GetList()) {
      auto index_context = PushContext(index);
      callback.Run(std::forward<T>(value));
      index++;
    }
  }

  void ParseCookie(base::Value&& cookie) {
    if (!EnsureDictionary(cookie))
      return;

    const base::Value::Dict& dict = cookie.GetDict();

    base::Time time = ParseTime(dict, kTimestampKey);

    static constexpr char kKeyUrl[] = "url";
    GURL url = ParseURL(dict, kKeyUrl);
    if (!url.is_valid()) {
      auto context = PushContext(kKeyUrl);
      *Error() << "must be a valid URL";
    }

    static constexpr char kKeySetCookie[] = "Set-Cookie";
    const std::string* line = dict.FindString(kKeySetCookie);
    if (!line) {
      auto context = PushContext(kKeySetCookie);
      *Error() << "must be present";
      return;
    }

    // `CanonicalCookie::Create()` will DCHECK.
    if (time.is_null())
      return;

    std::unique_ptr<net::CanonicalCookie> canonical_cookie =
        net::CanonicalCookie::Create(url, *line, time,
                                     /*server_time=*/absl::nullopt,
                                     /*cookie_partition_key=*/absl::nullopt);
    if (!canonical_cookie)
      *Error() << "invalid cookie";

    if (has_error())
      return;

    events_.emplace_back(
        AttributionSimulatorCookie{
            .cookie = std::move(*canonical_cookie),
            .source_url = std::move(url),
        },
        std::move(cookie));
  }

  void ParseDataClear(base::Value&& data_clear) {
    if (!EnsureDictionary(data_clear))
      return;

    const base::Value::Dict& dict = data_clear.GetDict();

    base::Time time = ParseTime(dict, kTimestampKey);

    static constexpr char kKeyDeleteBegin[] = "delete_begin";
    base::Time delete_begin = base::Time::Min();
    if (dict.contains(kKeyDeleteBegin))
      delete_begin = ParseTime(dict, kKeyDeleteBegin);

    static constexpr char kKeyDeleteEnd[] = "delete_end";
    base::Time delete_end = base::Time::Max();
    if (dict.contains(kKeyDeleteEnd))
      delete_end = ParseTime(dict, kKeyDeleteEnd);

    absl::optional<base::flat_set<url::Origin>> origin_set;

    static constexpr char kKeyOrigins[] = "origins";
    if (const base::Value* origins = dict.Find(kKeyOrigins)) {
      auto context = PushContext(kKeyOrigins);
      origin_set.emplace();

      ParseList(
          *origins, base::BindLambdaForTesting([&](const base::Value& value) {
            if (!value.is_string()) {
              *Error() << "must be a string";
            } else {
              origin_set->emplace(url::Origin::Create(GURL(value.GetString())));
            }
          }));
    }

    if (has_error())
      return;

    events_.emplace_back(AttributionDataClear(time, delete_begin, delete_end,
                                              std::move(origin_set)),
                         std::move(data_clear));
  }

  void ParseSource(base::Value&& source) {
    if (!EnsureDictionary(source))
      return;

    const base::Value::Dict& source_dict = source.GetDict();

    base::Time source_time = ParseTime(source_dict, kTimestampKey);
    url::Origin source_origin = ParseOrigin(source_dict, "source_origin");
    url::Origin reporting_origin = ParseOrigin(source_dict, "reporting_origin");
    absl::optional<AttributionSourceType> source_type =
        ParseSourceType(source_dict);

    if (has_error())
      return;

    ParseAttributionEvent(
        source_dict, "Attribution-Reporting-Register-Source",
        base::BindLambdaForTesting([&](const base::Value::Dict& dict) {
          base::expected<StorableSource,
                         attribution_reporting::mojom::SourceRegistrationError>
              storable_source = ParseSourceRegistration(
                  dict.Clone(), source_time, std::move(reporting_origin),
                  std::move(source_origin), *source_type);

          if (!storable_source.has_value()) {
            *Error() << storable_source.error();
            return;
          }

          events_.emplace_back(std::move(*storable_source), std::move(source));
        }));
  }

  void ParseTrigger(base::Value&& trigger) {
    if (!EnsureDictionary(trigger))
      return;

    const base::Value::Dict& trigger_dict = trigger.GetDict();

    base::Time trigger_time = ParseTime(trigger_dict, kTimestampKey);
    url::Origin reporting_origin =
        ParseOrigin(trigger_dict, "reporting_origin");
    url::Origin destination_origin =
        ParseOrigin(trigger_dict, "destination_origin");

    absl::optional<uint64_t> debug_key;
    AttributionFilterData filters;
    AttributionFilterData not_filters;
    std::vector<AttributionTrigger::EventTriggerData> event_triggers;
    std::vector<AttributionAggregatableTriggerData> aggregatable_trigger_data;
    AttributionAggregatableValues aggregatable_values;
    absl::optional<uint64_t> aggregatable_dedup_key;

    if (!ParseAttributionEvent(
            trigger_dict,
            "Attribution-Reporting-Register-Trigger",
            base::BindLambdaForTesting(
                [&](const base::Value::Dict& dict) {
                  debug_key = ParseOptionalUint64(dict, "debug_key");
                  filters = ParseFilterData(
                      dict, "filters",
                      &AttributionFilterData::FromTriggerFilterValues);
                  not_filters = ParseFilterData(
                      dict, "not_filters",
                      &AttributionFilterData::FromTriggerFilterValues);
                  event_triggers = ParseEventTriggers(dict);

                  aggregatable_trigger_data =
                      ParseAggregatableTriggerData(dict);

                  aggregatable_values = ParseAggregatableValues(dict);

                  aggregatable_dedup_key = ParseOptionalUint64(
                      dict, "aggregatable_deduplication_key");
                }))) {
      return;
    }

    if (has_error())
      return;

    events_.emplace_back(
        AttributionTriggerAndTime{
            .trigger = AttributionTrigger(
                std::move(destination_origin), std::move(reporting_origin),
                std::move(filters), std::move(not_filters), debug_key,
                aggregatable_dedup_key, std::move(event_triggers),
                std::move(aggregatable_trigger_data),
                std::move(aggregatable_values)),
            .time = trigger_time,
        },
        std::move(trigger));
  }

  std::vector<AttributionTrigger::EventTriggerData> ParseEventTriggers(
      const base::Value::Dict& cfg) {
    std::vector<AttributionTrigger::EventTriggerData> event_triggers;

    static constexpr char kKey[] = "event_trigger_data";

    const base::Value* values = cfg.Find(kKey);
    if (!values)
      return event_triggers;

    auto context = PushContext(kKey);
    ParseList(
        *values,
        base::BindLambdaForTesting([&](const base::Value& event_trigger) {
          if (!EnsureDictionary(event_trigger))
            return;

          const base::Value::Dict& dict = event_trigger.GetDict();

          uint64_t trigger_data =
              ParseOptionalUint64(dict, "trigger_data").value_or(0);

          int64_t priority = ParseOptionalInt64(dict, "priority").value_or(0);

          absl::optional<uint64_t> dedup_key =
              ParseOptionalUint64(dict, "deduplication_key");

          AttributionFilterData filters = ParseFilterData(
              dict, "filters", &AttributionFilterData::FromTriggerFilterValues);

          AttributionFilterData not_filters =
              ParseFilterData(dict, "not_filters",
                              &AttributionFilterData::FromTriggerFilterValues);

          if (has_error())
            return;

          event_triggers.emplace_back(trigger_data, priority, dedup_key,
                                      std::move(filters),
                                      std::move(not_filters));
        }),
        /*max_size=*/blink::kMaxAttributionEventTriggerData);

    return event_triggers;
  }

  GURL ParseURL(const base::Value::Dict& dict, base::StringPiece key) const {
    if (const std::string* v = dict.FindString(key))
      return GURL(*v);

    return GURL();
  }

  url::Origin ParseOrigin(const base::Value::Dict& dict,
                          base::StringPiece key) {
    auto context = PushContext(key);

    auto origin = url::Origin::Create(ParseURL(dict, key));

    if (!network::IsOriginPotentiallyTrustworthy(origin))
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

  absl::optional<uint64_t> ParseOptionalUint64(const base::Value::Dict& dict,
                                               base::StringPiece key) {
    const base::Value* value = dict.Find(key);
    if (!value)
      return absl::nullopt;

    return ParseUint64(value->GetIfString(), key);
  }

  absl::optional<int64_t> ParseOptionalInt64(const base::Value::Dict& dict,
                                             base::StringPiece key) {
    const base::Value* value = dict.Find(key);
    if (!value)
      return absl::nullopt;

    return ParseInt64(value->GetIfString(), key);
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
      const base::Value::Dict& value,
      base::StringPiece key,
      base::OnceCallback<void(const base::Value::Dict&)> callback) {
    auto context = PushContext(key);

    const base::Value* dict = value.Find(key);
    if (!dict) {
      *Error() << "must be present";
      return false;
    }

    if (!EnsureDictionary(*dict))
      return false;

    std::move(callback).Run(dict->GetDict());
    return true;
  }

  using FromFilterValuesFunc = absl::optional<AttributionFilterData>(
      AttributionFilterData::FilterValues&&);

  AttributionFilterData ParseFilterData(
      const base::Value::Dict& dict,
      base::StringPiece key,
      FromFilterValuesFunc from_filter_values) {
    auto context = PushContext(key);

    const base::Value* value = dict.Find(key);
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

  absl::uint128 ParseAggregationKey(const base::Value& key_value) {
    const std::string* s = key_value.GetIfString();

    absl::uint128 value = 0;
    if (!s ||
        !base::StartsWith(*s, "0x", base::CompareCase::INSENSITIVE_ASCII) ||
        !base::HexStringToUInt128(*s, &value)) {
      *Error() << "must be a uint128 formatted as a base-16 string";
    }

    return value;
  }

  base::flat_set<std::string> ParseAggregatableTriggerDataSourceKeys(
      const base::Value::Dict& dict) {
    static constexpr char kKey[] = "source_keys";

    base::flat_set<std::string> source_keys;

    auto context = PushContext(kKey);

    const base::Value* values = dict.Find(kKey);
    if (!values) {
      *Error() << "must be present";
      return source_keys;
    }

    ParseList(*values,
              base::BindLambdaForTesting([&](const base::Value& value) {
                if (!value.is_string()) {
                  *Error() << "must be a string";
                } else {
                  source_keys.emplace(value.GetString());
                }
              }));

    return source_keys;
  }

  std::vector<AttributionAggregatableTriggerData> ParseAggregatableTriggerData(
      const base::Value::Dict& dict) {
    static constexpr char kKey[] = "aggregatable_trigger_data";

    std::vector<AttributionAggregatableTriggerData> aggregatable_triggers;

    const base::Value* values = dict.Find(kKey);
    if (!values)
      return aggregatable_triggers;

    auto context = PushContext(kKey);
    ParseList(
        *values,
        base::BindLambdaForTesting(
            [&](const base::Value& aggregatable_trigger) {
              if (!EnsureDictionary(aggregatable_trigger))
                return;

              const base::Value::Dict& trigger_dict =
                  aggregatable_trigger.GetDict();

              base::flat_set<std::string> source_keys =
                  ParseAggregatableTriggerDataSourceKeys(trigger_dict);

              absl::uint128 key;
              {
                static constexpr char kKeyKeyPiece[] = "key_piece";
                auto key_context = PushContext(kKeyKeyPiece);
                const base::Value* key_piece = trigger_dict.Find(kKeyKeyPiece);
                if (key_piece) {
                  key = ParseAggregationKey(*key_piece);
                } else {
                  *Error() << "must be present";
                }
              }

              AttributionFilterData filters = ParseFilterData(
                  trigger_dict, "filters",
                  &AttributionFilterData::FromTriggerFilterValues);

              AttributionFilterData not_filters = ParseFilterData(
                  trigger_dict, "not_filters",
                  &AttributionFilterData::FromTriggerFilterValues);

              auto trigger_data = AttributionAggregatableTriggerData::Create(
                  key, std::move(source_keys), std::move(filters),
                  std::move(not_filters));
              if (!trigger_data)
                *Error() << "invalid";

              if (has_error())
                return;

              aggregatable_triggers.push_back(std::move(*trigger_data));
            }),
        blink::kMaxAttributionAggregatableTriggerDataPerTrigger);

    return aggregatable_triggers;
  }

  AttributionAggregatableValues ParseAggregatableValues(
      const base::Value::Dict& dict) {
    static constexpr char kKey[] = "aggregatable_values";

    const base::Value* value = dict.Find(kKey);
    if (!value)
      return AttributionAggregatableValues();

    auto context = PushContext(kKey);

    if (!EnsureDictionary(*value))
      return AttributionAggregatableValues();

    AttributionAggregatableValues::Values::container_type container;

    for (auto [id, key_value] : value->GetDict()) {
      auto key_context = PushContext(id);
      if (!key_value.is_int() || key_value.GetInt() <= 0) {
        *Error() << "must be a positive integer";
      } else {
        container.emplace_back(id, key_value.GetInt());
      }
    }

    absl::optional<AttributionAggregatableValues> aggregatable_values =
        AttributionAggregatableValues::FromValues(std::move(container));
    if (!aggregatable_values.has_value())
      *Error() << "invalid";

    return aggregatable_values.value_or(AttributionAggregatableValues());
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

AttributionDataClear::AttributionDataClear(
    base::Time time,
    base::Time delete_begin,
    base::Time delete_end,
    absl::optional<base::flat_set<url::Origin>> origins)
    : time(time),
      delete_begin(delete_begin),
      delete_end(delete_end),
      origins(std::move(origins)) {}

AttributionDataClear::~AttributionDataClear() = default;

AttributionDataClear::AttributionDataClear(const AttributionDataClear&) =
    default;

AttributionDataClear::AttributionDataClear(AttributionDataClear&&) = default;

AttributionDataClear& AttributionDataClear::operator=(
    const AttributionDataClear&) = default;

AttributionDataClear& AttributionDataClear::operator=(AttributionDataClear&&) =
    default;

absl::optional<AttributionSimulationEventAndValues>
ParseAttributionSimulationInput(base::Value input,
                                const base::Time offset_time,
                                std::ostream& error_stream) {
  return AttributionSimulatorInputParser(offset_time, error_stream)
      .Parse(std::move(input));
}

}  // namespace content
