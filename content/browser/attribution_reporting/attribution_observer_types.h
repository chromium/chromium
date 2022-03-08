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
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace content {

struct CONTENT_EXPORT DeactivatedSource {
  enum class Reason {
    kReplacedByNewerSource,
    kReachedAttributionLimit,
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
      std::vector<AttributionReport> dropped_reports = {},
      absl::optional<DeactivatedSource::Reason>
          dropped_report_source_deactivation_reason = absl::nullopt,
      std::vector<AttributionReport> new_reports = {});
  ~CreateReportResult();

  CreateReportResult(const CreateReportResult&);
  CreateReportResult(CreateReportResult&&);

  CreateReportResult& operator=(const CreateReportResult&);
  CreateReportResult& operator=(CreateReportResult&&);

  AttributionTrigger::EventLevelResult event_level_status() const {
    return event_level_status_;
  }

  const std::vector<AttributionReport>& dropped_reports() const {
    return dropped_reports_;
  }

  const std::vector<AttributionReport>& new_reports() const {
    return new_reports_;
  }

  std::vector<AttributionReport>& new_reports() { return new_reports_; }

  absl::optional<DeactivatedSource> GetDeactivatedSource() const;

 private:
  AttributionTrigger::EventLevelResult event_level_status_;

  // `AttributionTrigger::EventLevelResult::kInternalError` is only associated
  // with a dropped report if the browser succeeded in running the
  // source-to-attribute logic.
  std::vector<AttributionReport> dropped_reports_;

  // Null unless `dropped_report_`'s source was deactivated.
  absl::optional<DeactivatedSource::Reason>
      dropped_report_source_deactivation_reason_;

  // Empty unless `event_level_status` is `kSuccess` or
  // `kSuccessDroppedLowerPriority`.
  std::vector<AttributionReport> new_reports_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_OBSERVER_TYPES_H_
