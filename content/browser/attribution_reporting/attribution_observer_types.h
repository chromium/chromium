// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_OBSERVER_TYPES_H_
#define CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_OBSERVER_TYPES_H_

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
      AttributionTrigger::EventLevelResult status,
      absl::optional<AttributionReport> dropped_report = absl::nullopt,
      absl::optional<DeactivatedSource::Reason>
          dropped_report_source_deactivation_reason = absl::nullopt,
      absl::optional<AttributionReport> new_report = absl::nullopt);
  ~CreateReportResult();

  CreateReportResult(const CreateReportResult&);
  CreateReportResult(CreateReportResult&&);

  CreateReportResult& operator=(const CreateReportResult&);
  CreateReportResult& operator=(CreateReportResult&&);

  AttributionTrigger::EventLevelResult status() const { return status_; }

  const absl::optional<AttributionReport>& dropped_report() const {
    return dropped_report_;
  }

  const absl::optional<AttributionReport>& new_report() const {
    return new_report_;
  }

  absl::optional<AttributionReport>& new_report() { return new_report_; }

  absl::optional<DeactivatedSource> GetDeactivatedSource() const;

 private:
  AttributionTrigger::EventLevelResult status_;

  // `AttributionTrigger::EventLevelResult::kInternalError` is only associated
  // with a dropped report if the browser succeeded in running the
  // source-to-attribute logic.
  absl::optional<AttributionReport> dropped_report_;

  // Null unless `dropped_report_`'s source was deactivated.
  absl::optional<DeactivatedSource::Reason>
      dropped_report_source_deactivation_reason_;

  // Null unless `status` is `kSuccess` or `kSuccessDroppedLowerPriority`.
  absl::optional<AttributionReport> new_report_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_OBSERVER_TYPES_H_
