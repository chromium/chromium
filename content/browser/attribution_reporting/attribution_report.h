// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_REPORT_H_
#define CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_REPORT_H_

#include <stdint.h>

#include <string>

#include "base/compiler_specific.h"
#include "base/guid.h"
#include "base/time/time.h"
#include "base/types/strong_alias.h"
#include "content/browser/attribution_reporting/storable_source.h"
#include "content/common/content_export.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

class GURL;

namespace content {

// Struct that contains all the data needed to serialize and send a conversion
// report. This represents the report for a conversion event and its associated
// impression.
struct CONTENT_EXPORT AttributionReport {
  using Id = base::StrongAlias<AttributionReport, int64_t>;

  // The conversion_id may not be set for a conversion report.
  AttributionReport(StorableSource impression,
                    uint64_t trigger_data,
                    base::Time conversion_time,
                    base::Time report_time,
                    int64_t priority,
                    base::GUID external_report_id,
                    absl::optional<Id> conversion_id);
  AttributionReport(const AttributionReport& other);
  AttributionReport& operator=(const AttributionReport& other);
  AttributionReport(AttributionReport&& other);
  AttributionReport& operator=(AttributionReport&& other);
  ~AttributionReport();

  // Returns the URL to which the report will be sent.
  GURL ReportURL() const WARN_UNUSED_RESULT;

  // Returns the JSON for the report body.
  std::string ReportBody(bool pretty_print = false) const WARN_UNUSED_RESULT;

  // Impression associated with this conversion report.
  StorableSource impression;

  // Data provided at trigger time by the attribution destination. Depending on
  // the source type, this contains the associated data in the trigger redirect.
  uint64_t trigger_data;

  // The time the conversion occurred.
  base::Time conversion_time;

  // The time this conversion report should be sent.
  base::Time report_time;

  // Priority specified in conversion redirect.
  int64_t priority;

  // External report ID for deduplicating reports received by the reporting
  // origin.
  base::GUID external_report_id;

  // Id assigned by storage to uniquely identify a completed conversion. If
  // null, an ID has not been assigned yet.
  absl::optional<Id> conversion_id;

  // Number of times the browser has tried and failed to send this report.
  int failed_send_attempts = 0;

  // When adding new members, the corresponding `operator==()` definition in
  // `attribution_test_utils.h` should also be updated.
};

}  // namespace content

#endif  // CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_REPORT_H_
