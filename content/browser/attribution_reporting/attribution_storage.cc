// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/attribution_storage.h"

#include "base/check_op.h"

namespace content {

namespace {
using CreateReportResult = ::content::AttributionStorage::CreateReportResult;
}  // namespace

CreateReportResult::CreateReportResult(
    Status status,
    absl::optional<AttributionReport> dropped_report)
    : status_(status), dropped_report_(std::move(dropped_report)) {
  DCHECK_EQ(status_ == Status::kSuccessDroppedLowerPriority ||
                status_ == Status::kPriorityTooLow ||
                status_ == Status::kDroppedForNoise,
            dropped_report_.has_value());
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

const absl::optional<AttributionReport>& CreateReportResult::dropped_report()
    const {
  return dropped_report_;
}

}  // namespace content
