// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_REPORT_SENDER_H_
#define CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_REPORT_SENDER_H_

#include "base/callback_forward.h"

namespace content {

class AttributionDebugReport;
class AttributionReport;

struct SendResult;

// This class is responsible for sending attribution reports to their
// configured endpoints.
class AttributionReportSender {
 public:
  virtual ~AttributionReportSender() = default;

  // Callback used to notify caller that the requested report has been sent.
  using ReportSentCallback =
      base::OnceCallback<void(AttributionReport, SendResult)>;

  // If `status` is positive, it is the HTTP response code. Otherwise, it is the
  // network error.
  using DebugReportSentCallback =
      base::OnceCallback<void(AttributionDebugReport, int status)>;

  // Sends `report` and runs `sent_callback` when done.
  virtual void SendReport(AttributionReport report,
                          bool is_debug_report,
                          ReportSentCallback sent_callback) = 0;

  virtual void SendReport(AttributionDebugReport, DebugReportSentCallback) = 0;
};

}  // namespace content

#endif  // CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_REPORT_SENDER_H_
