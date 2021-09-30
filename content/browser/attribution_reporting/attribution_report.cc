// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/attribution_report.h"

#include "base/check.h"
#include "base/json/json_writer.h"
#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "url/gurl.h"
#include "url/url_canon.h"

namespace content {

AttributionReport::AttributionReport(StorableSource impression,
                                     uint64_t conversion_data,
                                     base::Time conversion_time,
                                     base::Time report_time,
                                     int64_t priority,
                                     absl::optional<Id> conversion_id)
    : impression(std::move(impression)),
      conversion_data(conversion_data),
      conversion_time(conversion_time),
      report_time(report_time),
      priority(priority),
      conversion_id(conversion_id) {}

AttributionReport::AttributionReport(const AttributionReport& other) = default;

AttributionReport& AttributionReport::operator=(
    const AttributionReport& other) = default;

AttributionReport::AttributionReport(AttributionReport&& other) = default;

AttributionReport& AttributionReport::operator=(AttributionReport&& other) =
    default;

AttributionReport::~AttributionReport() = default;

GURL AttributionReport::ReportURL() const {
  url::Replacements<char> replacements;
  static constexpr char kEndpointPath[] =
      "/.well-known/attribution-reporting/report-attribution";
  replacements.SetPath(kEndpointPath, url::Component(0, strlen(kEndpointPath)));
  return impression.reporting_origin().GetURL().ReplaceComponents(replacements);
}

std::string AttributionReport::ReportBody(bool pretty_print) const {
  base::Value dict(base::Value::Type::DICTIONARY);

  // The API denotes these values as strings; a `uint64_t` cannot be put in
  // a dict as an integer in order to be opaque to various API configurations.
  dict.SetStringKey("source_event_id",
                    base::NumberToString(impression.impression_data()));

  dict.SetStringKey("trigger_data", base::NumberToString(conversion_data));

  const char* source_type = nullptr;
  switch (impression.source_type()) {
    case StorableSource::SourceType::kNavigation:
      source_type = "navigation";
      break;
    case StorableSource::SourceType::kEvent:
      source_type = "event";
      break;
  }
  dict.SetStringKey("source_type", source_type);

  // Write the dict to json;
  std::string output_json;
  bool success = base::JSONWriter::WriteWithOptions(
      dict, pretty_print ? base::JSONWriter::OPTIONS_PRETTY_PRINT : 0,
      &output_json);
  DCHECK(success);
  return output_json;
}

}  // namespace content
