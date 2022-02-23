// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/attribution_storage.h"

#include "base/check_op.h"

namespace content {

namespace {
using CreateReportResult = ::content::AttributionStorage::CreateReportResult;
using DeactivatedSource = ::content::AttributionStorage::DeactivatedSource;
using StoreSourceResult = ::content::AttributionStorage::StoreSourceResult;
}  // namespace

CreateReportResult::CreateReportResult(
    AttributionTrigger::Result status,
    absl::optional<AttributionReport> dropped_report,
    absl::optional<DeactivatedSource::Reason>
        dropped_report_source_deactivation_reason,
    absl::optional<base::Time> report_time)
    : status_(status),
      dropped_report_(std::move(dropped_report)),
      dropped_report_source_deactivation_reason_(
          dropped_report_source_deactivation_reason),
      report_time_(report_time) {
  DCHECK((status_ == AttributionTrigger::Result::kSuccess &&
          !dropped_report_.has_value()) ||
         status_ == AttributionTrigger::Result::kNoMatchingImpressions ||
         status_ == AttributionTrigger::Result::kInternalError ||
         dropped_report_.has_value());

  DCHECK(dropped_report_.has_value() ||
         !dropped_report_source_deactivation_reason_);

  DCHECK_EQ(
      status_ == AttributionTrigger::Result::kSuccess ||
          status_ == AttributionTrigger::Result::kSuccessDroppedLowerPriority,
      report_time_.has_value());
}

CreateReportResult::~CreateReportResult() = default;

CreateReportResult::CreateReportResult(const CreateReportResult&) = default;
CreateReportResult::CreateReportResult(CreateReportResult&&) = default;

CreateReportResult& CreateReportResult::operator=(const CreateReportResult&) =
    default;
CreateReportResult& CreateReportResult::operator=(CreateReportResult&&) =
    default;

AttributionTrigger::Result CreateReportResult::status() const {
  return status_;
}

const absl::optional<AttributionReport>& CreateReportResult::dropped_report()
    const {
  return dropped_report_;
}

absl::optional<base::Time> CreateReportResult::report_time() const {
  return report_time_;
}

absl::optional<DeactivatedSource> CreateReportResult::GetDeactivatedSource()
    const {
  if (dropped_report_source_deactivation_reason_) {
    return DeactivatedSource(dropped_report_->attribution_info().source,
                             *dropped_report_source_deactivation_reason_);
  }
  return absl::nullopt;
}

DeactivatedSource::DeactivatedSource(StoredSource source, Reason reason)
    : source(std::move(source)), reason(reason) {}

DeactivatedSource::~DeactivatedSource() = default;

DeactivatedSource::DeactivatedSource(const DeactivatedSource&) = default;

DeactivatedSource::DeactivatedSource(DeactivatedSource&&) = default;

DeactivatedSource& DeactivatedSource::operator=(const DeactivatedSource&) =
    default;

DeactivatedSource& DeactivatedSource::operator=(DeactivatedSource&&) = default;

StoreSourceResult::StoreSourceResult(
    StorableSource::Result status,
    std::vector<DeactivatedSource> deactivated_sources,
    absl::optional<base::Time> min_fake_report_time)
    : status(status),
      deactivated_sources(std::move(deactivated_sources)),
      min_fake_report_time(min_fake_report_time) {}

StoreSourceResult::~StoreSourceResult() = default;

StoreSourceResult::StoreSourceResult(const StoreSourceResult&) = default;

StoreSourceResult::StoreSourceResult(StoreSourceResult&&) = default;

StoreSourceResult& StoreSourceResult::operator=(const StoreSourceResult&) =
    default;

StoreSourceResult& StoreSourceResult::operator=(StoreSourceResult&&) = default;

}  // namespace content
