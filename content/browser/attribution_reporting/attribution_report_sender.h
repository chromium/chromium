// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_REPORT_SENDER_H_
#define CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_REPORT_SENDER_H_

#include "base/functional/callback_forward.h"
#include "base/values.h"
#include "content/browser/attribution_reporting/send_result.h"

namespace content {

class AggregatableDebugReport;
class AttributionDebugReport;
class AttributionReport;

// This class is responsible for sending attribution reports to their
// configured endpoints.
class AttributionReportSender {
 public:
  virtual ~AttributionReportSender() = default;

  // Callback used to notify caller that the requested report has been sent.
  using ReportSentCallback =
      base::OnceCallback<void(const AttributionReport&, SendResult::Sent)>;

  // If `status` is positive, it is the HTTP response code. Otherwise, it is the
  // network error.
  using DebugReportSentCallback =
      base::OnceCallback<void(const AttributionDebugReport&, int status)>;

  // If `status` is positive, it is the HTTP response code. Otherwise, it is the
  // network error.
  using AggregatableDebugReportSentCallback = base::OnceCallback<
      void(const AggregatableDebugReport&, base::ValueView, int status)>;

  // Sends `report` and runs `sent_callback` when done.
  virtual void SendReport(AttributionReport report,
                          bool is_debug_report,
                          ReportSentCallback sent_callback) = 0;

  virtual void SendReport(AttributionDebugReport, DebugReportSentCallback) = 0;

  virtual void SendReport(AggregatableDebugReport,
                          base::Value::Dict report_body,
                          AggregatableDebugReportSentCallback) = 0;
};

}  // namespace content

#endif  // CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_REPORT_SENDER_H_
