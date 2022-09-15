// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/attribution_observer_types.h"

#include <utility>

#include "base/check.h"
#include "base/check_op.h"

namespace content {

CreateReportResult::CreateReportResult(
    base::Time trigger_time,
    AttributionTrigger::EventLevelResult event_level_status,
    AttributionTrigger::AggregatableResult aggregatable_status,
    absl::optional<AttributionReport> replaced_event_level_report,
    absl::optional<AttributionReport> new_event_level_report,
    absl::optional<AttributionReport> new_aggregatable_report)
    : trigger_time_(trigger_time),
      event_level_status_(event_level_status),
      aggregatable_status_(aggregatable_status),
      replaced_event_level_report_(std::move(replaced_event_level_report)),
      new_event_level_report_(std::move(new_event_level_report)),
      new_aggregatable_report_(std::move(new_aggregatable_report)) {
  DCHECK_EQ(
      event_level_status_ == AttributionTrigger::EventLevelResult::kSuccess ||
          event_level_status_ == AttributionTrigger::EventLevelResult::
                                     kSuccessDroppedLowerPriority,
      new_event_level_report_.has_value());

  DCHECK(!new_event_level_report_.has_value() ||
         new_event_level_report_->GetReportType() ==
             AttributionReport::ReportType::kEventLevel);

  DCHECK_EQ(
      aggregatable_status_ == AttributionTrigger::AggregatableResult::kSuccess,
      new_aggregatable_report_.has_value());

  DCHECK(!new_aggregatable_report_.has_value() ||
         new_aggregatable_report_->GetReportType() ==
             AttributionReport::ReportType::kAggregatableAttribution);

  DCHECK_EQ(
      replaced_event_level_report_.has_value(),
      event_level_status_ ==
          AttributionTrigger::EventLevelResult::kSuccessDroppedLowerPriority);
}

CreateReportResult::~CreateReportResult() = default;

CreateReportResult::CreateReportResult(const CreateReportResult&) = default;
CreateReportResult::CreateReportResult(CreateReportResult&&) = default;

CreateReportResult& CreateReportResult::operator=(const CreateReportResult&) =
    default;
CreateReportResult& CreateReportResult::operator=(CreateReportResult&&) =
    default;

}  // namespace content
