// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ATTRIBUTION_REPORTING_CONVERSION_SESSION_STORAGE_H_
#define CONTENT_BROWSER_ATTRIBUTION_REPORTING_CONVERSION_SESSION_STORAGE_H_

#include "base/compiler_specific.h"
#include "base/containers/circular_deque.h"
#include "content/browser/attribution_reporting/attribution_report.h"
#include "content/browser/attribution_reporting/conversion_storage.h"
#include "content/browser/attribution_reporting/sent_report_info.h"
#include "content/common/content_export.h"

namespace content {

// Provides access to session-only data useful for debugging in the internals
// UI.
class CONTENT_EXPORT ConversionSessionStorage {
 public:
  explicit ConversionSessionStorage(size_t max_reports_to_store);

  ~ConversionSessionStorage();

  void Reset();

  const base::circular_deque<SentReportInfo>& GetSentReports() const
      WARN_UNUSED_RESULT;

  void AddSentReport(SentReportInfo info);

  const base::circular_deque<ConversionStorage::CreateReportResult>&
  GetDroppedReports() const WARN_UNUSED_RESULT;

  void AddDroppedReport(ConversionStorage::CreateReportResult result);

 private:
  // Stores info for the last |max_reports_to_store_| reports sent in this
  // session.
  base::circular_deque<SentReportInfo> sent_reports_;

  // Stores info for the last |max_reports_to_store_| reports dropped in
  // favor of higher-priority ones in this session.
  base::circular_deque<ConversionStorage::CreateReportResult> dropped_reports_;

  // This is needed to avoid leaking memory.
  const size_t max_reports_to_store_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_ATTRIBUTION_REPORTING_CONVERSION_SESSION_STORAGE_H_
