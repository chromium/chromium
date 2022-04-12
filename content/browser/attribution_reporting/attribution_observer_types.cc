// Copyright 2022 The Chromium Authors. All rights reserved.
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
    std::vector<AttributionReport> new_reports)
    : trigger_time_(trigger_time),
      event_level_status_(event_level_status),
      aggregatable_status_(aggregatable_status),
      replaced_event_level_report_(std::move(replaced_event_level_report)),
      new_reports_(std::move(new_reports)) {
  DCHECK_EQ(
      event_level_status_ == AttributionTrigger::EventLevelResult::kSuccess ||
          event_level_status_ == AttributionTrigger::EventLevelResult::
                                     kSuccessDroppedLowerPriority ||
          aggregatable_status_ ==
              AttributionTrigger::AggregatableResult::kSuccess,
      !new_reports_.empty());

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

DeactivatedSource::DeactivatedSource(StoredSource source, Reason reason)
    : source(std::move(source)), reason(reason) {}

DeactivatedSource::~DeactivatedSource() = default;

DeactivatedSource::DeactivatedSource(const DeactivatedSource&) = default;

DeactivatedSource::DeactivatedSource(DeactivatedSource&&) = default;

DeactivatedSource& DeactivatedSource::operator=(const DeactivatedSource&) =
    default;

DeactivatedSource& DeactivatedSource::operator=(DeactivatedSource&&) = default;

}  // namespace content
