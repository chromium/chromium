// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_OBSERVER_TYPES_H_
#define CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_OBSERVER_TYPES_H_

#include <vector>

#include "base/time/time.h"
#include "content/browser/attribution_reporting/attribution_report.h"
#include "content/browser/attribution_reporting/attribution_trigger.h"
#include "content/browser/attribution_reporting/stored_source.h"
#include "content/common/content_export.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace content {

struct CONTENT_EXPORT DeactivatedSource {
  enum class Reason {
    kReplacedByNewerSource,
  };

  DeactivatedSource(StoredSource source, Reason reason);
  ~DeactivatedSource();

  DeactivatedSource(const DeactivatedSource&);
  DeactivatedSource(DeactivatedSource&&);

  DeactivatedSource& operator=(const DeactivatedSource&);
  DeactivatedSource& operator=(DeactivatedSource&&);

  StoredSource source;
  Reason reason;
};

class CONTENT_EXPORT CreateReportResult {
 public:
  CreateReportResult(base::Time trigger_time,
                     AttributionTrigger::EventLevelResult event_level_status,
                     AttributionTrigger::AggregatableResult aggregatable_status,
                     absl::optional<AttributionReport>
                         replaced_event_level_report = absl::nullopt,
                     std::vector<AttributionReport> new_reports = {});
  ~CreateReportResult();

  CreateReportResult(const CreateReportResult&);
  CreateReportResult(CreateReportResult&&);

  CreateReportResult& operator=(const CreateReportResult&);
  CreateReportResult& operator=(CreateReportResult&&);

  base::Time trigger_time() const { return trigger_time_; }

  AttributionTrigger::EventLevelResult event_level_status() const {
    return event_level_status_;
  }

  AttributionTrigger::AggregatableResult aggregatable_status() const {
    return aggregatable_status_;
  }

  const absl::optional<AttributionReport>& replaced_event_level_report() const {
    return replaced_event_level_report_;
  }

  const std::vector<AttributionReport>& new_reports() const {
    return new_reports_;
  }

  std::vector<AttributionReport>& new_reports() { return new_reports_; }

 private:
  base::Time trigger_time_;

  AttributionTrigger::EventLevelResult event_level_status_;

  AttributionTrigger::AggregatableResult aggregatable_status_;

  absl::optional<AttributionReport> replaced_event_level_report_;

  // Empty unless `event_level_status` is `kSuccess` or
  // `kSuccessDroppedLowerPriority` or `aggregatable_status` is
  // `kSuccess`.
  std::vector<AttributionReport> new_reports_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_OBSERVER_TYPES_H_
