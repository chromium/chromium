// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/conversion_storage.h"

namespace content {

namespace {
using CreateReportResult = ::content::ConversionStorage::CreateReportResult;
}  // namespace

CreateReportResult::CreateReportResult(
    Status status,
    absl::optional<AttributionReport> dropped_report)
    : status(status), dropped_report(std::move(dropped_report)) {}

CreateReportResult::~CreateReportResult() = default;

CreateReportResult::CreateReportResult(const CreateReportResult&) = default;
CreateReportResult::CreateReportResult(CreateReportResult&&) = default;

CreateReportResult& CreateReportResult::operator=(const CreateReportResult&) =
    default;
CreateReportResult& CreateReportResult::operator=(CreateReportResult&&) =
    default;

}  // namespace content
