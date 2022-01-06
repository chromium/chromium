// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ATTRIBUTION_REPORTING_EVENT_ATTRIBUTION_REPORT_H_
#define CONTENT_BROWSER_ATTRIBUTION_REPORTING_EVENT_ATTRIBUTION_REPORT_H_

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

// Class that contains all the data needed to serialize and send a conversion
// report. This represents the report for a conversion event and its associated
// source.
class CONTENT_EXPORT EventAttributionReport {
 public:
  using Id = base::StrongAlias<EventAttributionReport, int64_t>;

  // The conversion_id may not be set for a conversion report.
  EventAttributionReport(StorableSource source,
                         uint64_t trigger_data,
                         base::Time conversion_time,
                         base::Time report_time,
                         int64_t priority,
                         base::GUID external_report_id,
                         absl::optional<Id> report_id);
  EventAttributionReport(const EventAttributionReport& other);
  EventAttributionReport& operator=(const EventAttributionReport& other);
  EventAttributionReport(EventAttributionReport&& other);
  EventAttributionReport& operator=(EventAttributionReport&& other);
  ~EventAttributionReport();

  // Returns the URL to which the report will be sent.
  GURL ReportURL() const WARN_UNUSED_RESULT;

  // Returns the JSON for the report body.
  std::string ReportBody(bool pretty_print = false) const WARN_UNUSED_RESULT;

  const StorableSource& source() const { return source_; }

  uint64_t trigger_data() const { return trigger_data_; }

  base::Time conversion_time() const { return conversion_time_; }

  base::Time report_time() const { return report_time_; }

  int64_t priority() const { return priority_; }

  const base::GUID& external_report_id() const { return external_report_id_; }

  absl::optional<Id> report_id() const { return report_id_; }

  int failed_send_attempts() const { return failed_send_attempts_; }

  void set_report_time(base::Time report_time);

  void set_failed_send_attempts(int failed_send_attempts);

  void SetExternalReportIdForTesting(base::GUID external_report_id);

 private:
  // Source associated with this conversion report.
  StorableSource source_;

  // Data provided at trigger time by the attribution destination. Depending on
  // the source type, this contains the associated data in the trigger redirect.
  uint64_t trigger_data_;

  // The time the conversion occurred.
  base::Time conversion_time_;

  // The time this conversion report should be sent.
  base::Time report_time_;

  // Priority specified in conversion redirect.
  int64_t priority_;

  // External report ID for deduplicating reports received by the reporting
  // origin.
  base::GUID external_report_id_;

  // Id assigned by storage to uniquely identify a completed conversion. If
  // null, an ID has not been assigned yet.
  absl::optional<Id> report_id_;

  // Number of times the browser has tried and failed to send this report.
  int failed_send_attempts_ = 0;

  // When adding new members, the corresponding `operator==()` definition in
  // `attribution_test_utils.h` should also be updated.
};

}  // namespace content

#endif  // CONTENT_BROWSER_ATTRIBUTION_REPORTING_EVENT_ATTRIBUTION_REPORT_H_
