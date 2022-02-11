// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/test/attribution_simulator_input_parser.h"

#include <stdint.h>

#include <string>
#include <utility>

#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "base/values.h"
#include "content/browser/attribution_reporting/attribution_policy.h"
#include "content/browser/attribution_reporting/attribution_trigger.h"
#include "content/browser/attribution_reporting/common_source_info.h"
#include "content/browser/attribution_reporting/storable_source.h"
#include "net/base/schemeful_site.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

namespace {

std::string FindStringKeyOrExit(const base::Value& dict, const char* key) {
  const std::string* v = dict.FindStringKey(key);
  LOG_IF(FATAL, !v) << "key not found: " << key;
  return *v;
}

url::Origin FindOriginKeyOrExit(const base::Value& dict, const char* key) {
  std::string v = FindStringKeyOrExit(dict, key);
  auto origin = url::Origin::Create(GURL(v));
  LOG_IF(FATAL, origin.opaque()) << "opaque origin: " << v;
  return origin;
}

base::Time FindTimeKeyOrExit(const base::Value& dict,
                             const char* key,
                             base::Time offset_time) {
  absl::optional<int> v = dict.FindIntKey(key);
  LOG_IF(FATAL, !v) << "key not found: " << key;
  LOG_IF(FATAL, *v < 0) << "negative time not allowed: " << *v;
  return offset_time + base::Seconds(*v);
}

uint64_t ParseUint64OrExit(const std::string& s) {
  uint64_t v = 0;
  LOG_IF(FATAL, !base::StringToUint64(s, &v)) << "invalid uint64: " << s;
  return v;
}

int64_t ParseInt64OrExit(const std::string& s) {
  int64_t v = 0;
  LOG_IF(FATAL, !base::StringToInt64(s, &v)) << "invalid int64: " << s;
  return v;
}

uint64_t FindUint64KeyOrDefault(const base::Value& dict,
                                const char* key,
                                uint64_t default_val) {
  const std::string* s = dict.FindStringKey(key);
  if (!s)
    return default_val;

  return ParseUint64OrExit(*s);
}

int64_t FindInt64KeyOrDefault(const base::Value& dict,
                              const char* key,
                              int64_t default_val) {
  const std::string* s = dict.FindStringKey(key);
  if (!s)
    return default_val;

  return ParseInt64OrExit(*s);
}

absl::optional<int64_t> FindInt64KeyOrNull(const base::Value& dict,
                                           const char* key) {
  const std::string* s = dict.FindStringKey(key);
  if (!s)
    return absl::nullopt;

  return ParseInt64OrExit(*s);
}

uint64_t FindUint64KeyOrExit(const base::Value& dict, const char* key) {
  return ParseUint64OrExit(FindStringKeyOrExit(dict, key));
}

absl::optional<uint64_t> FindUint64KeyOrNull(const base::Value& dict,
                                             const char* key) {
  const std::string* s = dict.FindStringKey(key);
  if (!s)
    return absl::nullopt;

  return ParseUint64OrExit(*s);
}

CommonSourceInfo::SourceType FindSourceTypeKeyOrExit(const base::Value& dict,
                                                     const char* key) {
  std::string v = FindStringKeyOrExit(dict, key);

  if (v == "navigation")
    return CommonSourceInfo::SourceType::kNavigation;

  if (v == "event")
    return CommonSourceInfo::SourceType::kEvent;

  LOG(FATAL) << "invalid source type: " << v;
  return CommonSourceInfo::SourceType::kNavigation;
}

const base::Value& FindValueOrExit(const base::Value& dict, const char* key) {
  const base::Value* v = dict.FindKey(key);
  LOG_IF(FATAL, !v) << "key not found: " << key;
  return *v;
}

StorableSource ParseSource(const base::Value& dict, base::Time offset_time) {
  const base::Value& cfg = FindValueOrExit(dict, "registration_config");

  base::Time source_time = FindTimeKeyOrExit(dict, "source_time", offset_time);

  base::TimeDelta expiry = base::Days(30);
  if (absl::optional<int64_t> v = FindInt64KeyOrNull(cfg, "expiry")) {
    LOG_IF(FATAL, *v < 0) << "expiry must be >= 0: " << *v;
    expiry = base::Milliseconds(*v);
  }

  CommonSourceInfo::SourceType source_type =
      FindSourceTypeKeyOrExit(dict, "source_type");

  return StorableSource(CommonSourceInfo(
      FindUint64KeyOrExit(cfg, "source_event_id"),
      FindOriginKeyOrExit(dict, "source_origin"),
      FindOriginKeyOrExit(cfg, "destination"),
      FindOriginKeyOrExit(dict, "reporting_origin"), source_time,
      GetExpiryTimeForImpression(expiry, source_time, source_type), source_type,
      FindInt64KeyOrDefault(cfg, "priority", 0),
      FindUint64KeyOrNull(cfg, "debug_key")));
}

AttributionTriggerAndTime ParseTrigger(const base::Value& dict,
                                       base::Time offset_time) {
  const base::Value& cfg = FindValueOrExit(dict, "registration_config");

  return AttributionTriggerAndTime{
      .trigger = AttributionTrigger(
          SanitizeTriggerData(FindUint64KeyOrDefault(cfg, "trigger_data", 0),
                              CommonSourceInfo::SourceType::kNavigation),
          net::SchemefulSite(FindOriginKeyOrExit(dict, "destination")),
          FindOriginKeyOrExit(dict, "reporting_origin"),
          SanitizeTriggerData(
              FindUint64KeyOrDefault(cfg, "event_source_trigger_data", 0),
              CommonSourceInfo::SourceType::kEvent),
          FindInt64KeyOrDefault(cfg, "priority", 0),
          FindInt64KeyOrNull(cfg, "dedup_key"),
          FindUint64KeyOrNull(cfg, "debug_key")),
      .time = FindTimeKeyOrExit(dict, "trigger_time", offset_time),
  };
}

}  // namespace

std::vector<AttributionSimulationEventAndValue>
ParseAttributionSimulationInputOrExit(base::Value input,
                                      base::Time offset_time) {
  std::vector<AttributionSimulationEventAndValue> events;

  if (base::Value* items = input.FindListKey("sources")) {
    for (base::Value& item : items->GetListDeprecated()) {
      StorableSource source = ParseSource(item, offset_time);
      events.emplace_back(std::move(source), std::move(item));
    }
  }

  if (base::Value* items = input.FindListKey("triggers")) {
    for (base::Value& item : items->GetListDeprecated()) {
      AttributionTriggerAndTime trigger = ParseTrigger(item, offset_time);
      events.emplace_back(std::move(trigger), std::move(item));
    }
  }

  return events;
}

}  // namespace content
