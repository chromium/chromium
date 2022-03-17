// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_OBSERVER_TYPES_H_
#define CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_OBSERVER_TYPES_H_

#include <vector>

#include "content/browser/attribution_reporting/attribution_report.h"
#include "content/browser/attribution_reporting/attribution_trigger.h"
#include "content/browser/attribution_reporting/stored_source.h"
#include "content/common/content_export.h"

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
  explicit CreateReportResult(
      AttributionTrigger::EventLevelResult event_level_status,
      AttributionTrigger::AggregatableResult aggregatable_status,
      std::vector<AttributionReport> dropped_reports = {},
      std::vector<AttributionReport> new_reports = {});
  ~CreateReportResult();

  CreateReportResult(const CreateReportResult&);
  CreateReportResult(CreateReportResult&&);

  CreateReportResult& operator=(const CreateReportResult&);
  CreateReportResult& operator=(CreateReportResult&&);

  AttributionTrigger::EventLevelResult event_level_status() const {
    return event_level_status_;
  }

  AttributionTrigger::AggregatableResult aggregatable_status() const {
    return aggregatable_status_;
  }

  const std::vector<AttributionReport>& dropped_reports() const {
    return dropped_reports_;
  }

  const std::vector<AttributionReport>& new_reports() const {
    return new_reports_;
  }

  std::vector<AttributionReport>& new_reports() { return new_reports_; }

 private:
  AttributionTrigger::EventLevelResult event_level_status_;

  AttributionTrigger::AggregatableResult aggregatable_status_;

  // `AttributionTrigger::EventLevelResult::kInternalError` and
  // `AttributionTrigger::AggregatableResult::kInternalError` are only
  // associated with a dropped report if the browser succeeded in running the
  // source-to-attribute logic.
  std::vector<AttributionReport> dropped_reports_;

  // Empty unless `event_level_status` is `kSuccess` or
  // `kSuccessDroppedLowerPriority` or `aggregatable_status` is
  // `kSuccess`.
  std::vector<AttributionReport> new_reports_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_OBSERVER_TYPES_H_
