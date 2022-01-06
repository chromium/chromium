// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/attribution_storage.h"

#include "base/check_op.h"

namespace content {

namespace {
using CreateReportResult = ::content::AttributionStorage::CreateReportResult;
using DeactivatedSource = ::content::AttributionStorage::DeactivatedSource;
}  // namespace

CreateReportResult::CreateReportResult(
    Status status,
    absl::optional<EventAttributionReport> dropped_report,
    absl::optional<DeactivatedSource::Reason>
        dropped_report_source_deactivation_reason,
    absl::optional<base::Time> report_time)
    : status_(status),
      dropped_report_(std::move(dropped_report)),
      dropped_report_source_deactivation_reason_(
          dropped_report_source_deactivation_reason),
      report_time_(report_time) {
  DCHECK_EQ(status_ == Status::kSuccessDroppedLowerPriority ||
                status_ == Status::kRateLimited ||
                status_ == Status::kPriorityTooLow ||
                status_ == Status::kDroppedForNoise,
            dropped_report_.has_value());
  DCHECK(dropped_report_.has_value() ||
         !dropped_report_source_deactivation_reason_);
  DCHECK_EQ(status_ == Status::kSuccess ||
                status_ == Status::kSuccessDroppedLowerPriority,
            report_time_.has_value());
}

CreateReportResult::~CreateReportResult() = default;

CreateReportResult::CreateReportResult(const CreateReportResult&) = default;
CreateReportResult::CreateReportResult(CreateReportResult&&) = default;

CreateReportResult& CreateReportResult::operator=(const CreateReportResult&) =
    default;
CreateReportResult& CreateReportResult::operator=(CreateReportResult&&) =
    default;

CreateReportResult::Status CreateReportResult::status() const {
  return status_;
}

const absl::optional<EventAttributionReport>&
CreateReportResult::dropped_report() const {
  return dropped_report_;
}

absl::optional<base::Time> CreateReportResult::report_time() const {
  return report_time_;
}

absl::optional<DeactivatedSource> CreateReportResult::GetDeactivatedSource()
    const {
  if (dropped_report_source_deactivation_reason_) {
    return DeactivatedSource(dropped_report_->source(),
                             *dropped_report_source_deactivation_reason_);
  }
  return absl::nullopt;
}

DeactivatedSource::DeactivatedSource(StorableSource source, Reason reason)
    : source(std::move(source)), reason(reason) {}

DeactivatedSource::~DeactivatedSource() = default;

DeactivatedSource::DeactivatedSource(const DeactivatedSource&) = default;

DeactivatedSource::DeactivatedSource(DeactivatedSource&&) = default;

DeactivatedSource& DeactivatedSource::operator=(const DeactivatedSource&) =
    default;

DeactivatedSource& DeactivatedSource::operator=(DeactivatedSource&&) = default;

}  // namespace content
