// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/test/attribution_simulator_input_parser.h"

#include <stdint.h>

#include <ostream>
#include <string>
#include <utility>
#include <vector>

#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/time/time.h"
#include "base/values.h"
#include "content/browser/attribution_reporting/attribution_aggregatable_sources.h"
#include "content/browser/attribution_reporting/attribution_filter_data.h"
#include "content/browser/attribution_reporting/attribution_trigger.h"
#include "content/browser/attribution_reporting/common_source_info.h"
#include "content/browser/attribution_reporting/storable_source.h"
#include "net/base/schemeful_site.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

namespace {

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

    ParseList(input, "sources", &AttributionSimulatorInputParser::ParseSource);
    ParseList(input, "triggers",
              &AttributionSimulatorInputParser::ParseTrigger);

    if (has_error_)
      return absl::nullopt;

    return std::move(events_);
  }

 private:
  const base::Time offset_time_;
  std::ostream& error_stream_;

  base::StringPiece context_ = "input root";
  absl::optional<size_t> context_index_;
  bool has_error_ = false;
  std::vector<AttributionSimulationEventAndValue> events_;

  // Returns an ostream& for callers to append detailed error information to.
  std::ostream& Error() {
    has_error_ = true;
    error_stream_ << context_;

    if (context_index_.has_value())
      error_stream_ << "[" << *context_index_ << "]";

    return error_stream_ << ": ";
  }

  using ParseListMethod =
      void (AttributionSimulatorInputParser::*)(base::Value&&);

  void ParseList(base::Value& input,
                 base::StringPiece key,
                 ParseListMethod parse) {
    context_ = key;
    context_index_ = absl::nullopt;

    base::Value* values = input.FindKey(context_);
    if (!values)
      return;

    if (!values->is_list()) {
      Error() << "must be a list" << std::endl;
      return;
    }

    context_index_ = 0;
    for (base::Value& value : values->GetListDeprecated()) {
      (this->*parse)(std::move(value));
      (*context_index_)++;
    }
  }

  void ParseSource(base::Value&& source) {
    if (!EnsureDictionary(source))
      return;

    base::Time source_time = ParseTime(source, "source_time");
    url::Origin source_origin = ParseOrigin(source, "source_origin");
    url::Origin reporting_origin = ParseOrigin(source, "reporting_origin");
    absl::optional<CommonSourceInfo::SourceType> source_type =
        ParseSourceType(source);

    const base::Value* cfg = ParseRegistrationConfig(source);
    if (!cfg)
      return;

    uint64_t source_event_id = ParseRequiredUint64(*cfg, "source_event_id");
    url::Origin destination_origin = ParseOrigin(*cfg, "destination");
    absl::optional<uint64_t> debug_key = ParseOptionalUint64(*cfg, "debug_key");
    int64_t priority = ParseOptionalInt64(*cfg, "priority").value_or(0);
    base::TimeDelta expiry = ParseSourceExpiry(*cfg).value_or(base::Days(30));

    // TODO(linnan): Support aggregatable reports in the simulator.

    if (has_error_)
      return;

    // TODO(apaseltiner): Parse filter data from `cfg`.
    events_.emplace_back(
        StorableSource(CommonSourceInfo(
            source_event_id, std::move(source_origin),
            std::move(destination_origin), std::move(reporting_origin),
            source_time,
            CommonSourceInfo::GetExpiryTime(expiry, source_time, *source_type),
            *source_type, priority, AttributionFilterData(), debug_key,
            AttributionAggregatableSources())),
        std::move(source));
  }

  void ParseTrigger(base::Value&& trigger) {
    if (!EnsureDictionary(trigger))
      return;

    base::Time trigger_time = ParseTime(trigger, "trigger_time");
    url::Origin reporting_origin = ParseOrigin(trigger, "reporting_origin");
    net::SchemefulSite destination(ParseOrigin(trigger, "destination"));

    const base::Value* cfg = ParseRegistrationConfig(trigger);
    if (!cfg)
      return;

    uint64_t trigger_data =
        ParseOptionalUint64(*cfg, "trigger_data").value_or(0);
    uint64_t event_source_trigger_data =
        ParseOptionalUint64(*cfg, "event_source_trigger_data").value_or(0);
    absl::optional<uint64_t> debug_key = ParseOptionalUint64(*cfg, "debug_key");
    absl::optional<uint64_t> dedup_key =
        ParseOptionalUint64(*cfg, "deduplication_key");
    int64_t priority = ParseOptionalInt64(*cfg, "priority").value_or(0);

    if (has_error_)
      return;

    events_.emplace_back(
        AttributionTriggerAndTime{
            .trigger = AttributionTrigger(trigger_data, std::move(destination),
                                          std::move(reporting_origin),
                                          event_source_trigger_data, priority,
                                          dedup_key, debug_key),
            .time = trigger_time,
        },
        std::move(trigger));
  }

  url::Origin ParseOrigin(const base::Value& dict, base::StringPiece key) {
    url::Origin origin;

    if (const std::string* v = dict.FindStringKey(key))
      origin = url::Origin::Create(GURL(*v));

    if (origin.opaque())
      Error() << key << " must be a valid origin" << std::endl;

    return origin;
  }

  base::Time ParseTime(const base::Value& dict, base::StringPiece key) {
    absl::optional<int> v = dict.FindIntKey(key);
    if (!v) {
      Error() << key
              << " must be an integer number of seconds since the Unix epoch"
              << std::endl;
      return base::Time();
    }

    return offset_time_ + base::Seconds(*v);
  }

  uint64_t ParseUint64(const std::string* s, base::StringPiece key) {
    uint64_t value = 0;

    if (!s || !base::StringToUint64(*s, &value)) {
      Error() << key << " must be a uint64 formatted as a base-10 string"
              << std::endl;
    }

    return value;
  }

  int64_t ParseInt64(const std::string* s, base::StringPiece key) {
    int64_t value = 0;

    if (!s || !base::StringToInt64(*s, &value)) {
      Error() << key << " must be an int64 formatted as a base-10 string"
              << std::endl;
    }

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

  absl::optional<CommonSourceInfo::SourceType> ParseSourceType(
      const base::Value& dict) {
    static constexpr char kKey[] = "source_type";
    static constexpr char kNavigation[] = "navigation";
    static constexpr char kEvent[] = "event";

    absl::optional<CommonSourceInfo::SourceType> source_type;

    if (const std::string* v = dict.FindStringKey(kKey)) {
      if (*v == kNavigation) {
        source_type = CommonSourceInfo::SourceType::kNavigation;
      } else if (*v == kEvent) {
        source_type = CommonSourceInfo::SourceType::kEvent;
      }
    }

    if (!source_type) {
      Error() << kKey << " must be either \"" << kNavigation << "\" or \""
              << kEvent << "\"" << std::endl;
    }

    return source_type;
  }

  const base::Value* ParseRegistrationConfig(const base::Value& dict) {
    static constexpr char kKey[] = "registration_config";

    const base::Value* cfg = dict.FindDictKey(kKey);
    if (!cfg)
      Error() << kKey << " must be a dictionary" << std::endl;

    return cfg;
  }

  absl::optional<base::TimeDelta> ParseSourceExpiry(const base::Value& dict) {
    static constexpr char kKey[] = "expiry";

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
      Error() << kKey
              << " must be a positive number of milliseconds formatted as a"
                 " base-10 string"
              << std::endl;
    }

    return expiry;
  }

  bool EnsureDictionary(const base::Value& value) {
    if (!value.is_dict()) {
      Error() << "must be a dictionary" << std::endl;
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
