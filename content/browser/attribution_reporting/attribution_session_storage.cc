// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/attribution_session_storage.h"

#include "base/check.h"

namespace content {

AttributionSessionStorage::AttributionSessionStorage(
    size_t max_reports_to_store)
    : max_reports_to_store_(max_reports_to_store) {
  DCHECK_GT(max_reports_to_store_, 0u);
}

AttributionSessionStorage::~AttributionSessionStorage() = default;

void AttributionSessionStorage::Reset() {
  sent_reports_.clear();
  dropped_reports_.clear();
}

const base::circular_deque<SentReportInfo>&
AttributionSessionStorage::GetSentReports() const {
  return sent_reports_;
}

void AttributionSessionStorage::AddSentReport(SentReportInfo info) {
  if (sent_reports_.size() == max_reports_to_store_)
    sent_reports_.pop_front();
  sent_reports_.push_back(std::move(info));
}

const base::circular_deque<AttributionStorage::CreateReportResult>&
AttributionSessionStorage::GetDroppedReports() const {
  return dropped_reports_;
}

void AttributionSessionStorage::AddDroppedReport(
    AttributionStorage::CreateReportResult result) {
  if (dropped_reports_.size() == max_reports_to_store_)
    dropped_reports_.pop_front();
  dropped_reports_.push_back(std::move(result));
}

}  // namespace content
