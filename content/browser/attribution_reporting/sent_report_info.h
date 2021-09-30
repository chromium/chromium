// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ATTRIBUTION_REPORTING_SENT_REPORT_INFO_H_
#define CONTENT_BROWSER_ATTRIBUTION_REPORTING_SENT_REPORT_INFO_H_

#include "base/time/time.h"
#include "content/browser/attribution_reporting/attribution_report.h"
#include "content/common/content_export.h"

namespace content {

// Struct that contains data about sent reports. Some info is displayed in the
// Conversion Internals WebUI.
struct CONTENT_EXPORT SentReportInfo {
  enum class Status {
    kSent,
    // The report failed without receiving response headers.
    kTransientFailure,
    // The report failed due to other cause and would not be retried.
    kFailure,
    // The report was dropped without ever being sent, e.g. due to embedder
    // disabling the API.
    kDropped,
    // The report should be retried because the browser is offline. This
    // shouldn't contribute to the number of report failures.
    kOffline,
    // The report was removed from the queue before being sent.
    kRemovedFromQueue,
  };

  SentReportInfo(AttributionReport report,
                 Status status,
                 int http_response_code);
  SentReportInfo(const SentReportInfo& other);
  SentReportInfo& operator=(const SentReportInfo& other);
  SentReportInfo(SentReportInfo&& other);
  SentReportInfo& operator=(SentReportInfo&& other);
  ~SentReportInfo();

  AttributionReport report;

  Status status;

  // Information on the network request that was sent.
  int http_response_code;

  // When adding new members, the corresponding `operator==()` definition in
  // `conversion_test_utils.h` should also be updated.
};

}  // namespace content

#endif  // CONTENT_BROWSER_ATTRIBUTION_REPORTING_SENT_REPORT_INFO_H_
