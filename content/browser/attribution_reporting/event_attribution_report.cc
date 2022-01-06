// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/event_attribution_report.h"

#include <utility>

#include "base/check.h"
#include "base/json/json_writer.h"
#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "net/base/schemeful_site.h"
#include "url/gurl.h"
#include "url/url_canon.h"

namespace content {

EventAttributionReport::EventAttributionReport(StorableSource source,
                                               uint64_t trigger_data,
                                               base::Time conversion_time,
                                               base::Time report_time,
                                               int64_t priority,
                                               base::GUID external_report_id,
                                               absl::optional<Id> report_id)
    : source_(std::move(source)),
      trigger_data_(trigger_data),
      conversion_time_(conversion_time),
      report_time_(report_time),
      priority_(priority),
      external_report_id_(std::move(external_report_id)),
      report_id_(report_id) {
  DCHECK(external_report_id_.is_valid());
}

EventAttributionReport::EventAttributionReport(
    const EventAttributionReport& other) = default;

EventAttributionReport& EventAttributionReport::operator=(
    const EventAttributionReport& other) = default;

EventAttributionReport::EventAttributionReport(EventAttributionReport&& other) =
    default;

EventAttributionReport& EventAttributionReport::operator=(
    EventAttributionReport&& other) = default;

EventAttributionReport::~EventAttributionReport() = default;

GURL EventAttributionReport::ReportURL() const {
  url::Replacements<char> replacements;
  static constexpr char kEndpointPath[] =
      "/.well-known/attribution-reporting/report-attribution";
  replacements.SetPath(kEndpointPath, url::Component(0, strlen(kEndpointPath)));
  return source_.reporting_origin().GetURL().ReplaceComponents(replacements);
}

std::string EventAttributionReport::ReportBody(bool pretty_print) const {
  base::Value dict(base::Value::Type::DICTIONARY);

  dict.SetStringKey("attribution_destination",
                    source_.ConversionDestination().Serialize());

  // The API denotes these values as strings; a `uint64_t` cannot be put in
  // a dict as an integer in order to be opaque to various API configurations.
  dict.SetStringKey("source_event_id",
                    base::NumberToString(source_.source_event_id()));

  dict.SetStringKey("trigger_data", base::NumberToString(trigger_data_));

  const char* source_type = nullptr;
  switch (source_.source_type()) {
    case StorableSource::SourceType::kNavigation:
      source_type = "navigation";
      break;
    case StorableSource::SourceType::kEvent:
      source_type = "event";
      break;
  }
  dict.SetStringKey("source_type", source_type);

  dict.SetStringKey("report_id", external_report_id_.AsLowercaseString());

  // Write the dict to json;
  std::string output_json;
  bool success = base::JSONWriter::WriteWithOptions(
      dict, pretty_print ? base::JSONWriter::OPTIONS_PRETTY_PRINT : 0,
      &output_json);
  DCHECK(success);
  return output_json;
}

void EventAttributionReport::set_report_time(base::Time report_time) {
  report_time_ = report_time;
}

void EventAttributionReport::set_failed_send_attempts(
    int failed_send_attempts) {
  DCHECK_GE(failed_send_attempts, 0);
  failed_send_attempts_ = failed_send_attempts;
}

void EventAttributionReport::SetExternalReportIdForTesting(
    base::GUID external_report_id) {
  DCHECK(external_report_id.is_valid());
  external_report_id_ = std::move(external_report_id);
}

}  // namespace content
