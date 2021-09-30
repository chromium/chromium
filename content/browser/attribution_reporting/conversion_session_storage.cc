// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/conversion_session_storage.h"

#include "base/check.h"

namespace content {

ConversionSessionStorage::ConversionSessionStorage(size_t max_reports_to_store)
    : max_reports_to_store_(max_reports_to_store) {
  DCHECK_GT(max_reports_to_store_, 0u);
}

ConversionSessionStorage::~ConversionSessionStorage() = default;

void ConversionSessionStorage::Reset() {
  sent_reports_.clear();
  dropped_reports_.clear();
}

const base::circular_deque<SentReportInfo>&
ConversionSessionStorage::GetSentReports() const {
  return sent_reports_;
}

void ConversionSessionStorage::AddSentReport(SentReportInfo info) {
  if (sent_reports_.size() == max_reports_to_store_)
    sent_reports_.pop_front();
  sent_reports_.push_back(std::move(info));
}

const base::circular_deque<AttributionReport>&
ConversionSessionStorage::GetDroppedReports() const {
  return dropped_reports_;
}

void ConversionSessionStorage::AddDroppedReport(
    AttributionReport dropped_report) {
  if (dropped_reports_.size() == max_reports_to_store_)
    dropped_reports_.pop_front();
  dropped_reports_.push_back(std::move(dropped_report));
}

}  // namespace content
