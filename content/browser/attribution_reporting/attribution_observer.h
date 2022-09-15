// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_OBSERVER_H_
#define CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_OBSERVER_H_

#include "base/observer_list_types.h"
#include "content/browser/attribution_reporting/attribution_report.h"
#include "content/browser/attribution_reporting/storable_source.h"

namespace content {

class AttributionTrigger;
class CreateReportResult;

struct SendResult;

// Observes events in the Attribution Reporting API. Observers are registered on
// `AttributionManager`.
class AttributionObserver : public base::CheckedObserver {
 public:
  ~AttributionObserver() override = default;

  // Called when sources in storage change.
  virtual void OnSourcesChanged() {}

  // Called when reports in storage change.
  virtual void OnReportsChanged(AttributionReport::ReportType report_type) {}

  // Called when a source is registered, regardless of success.
  virtual void OnSourceHandled(const StorableSource& source,
                               StorableSource::Result result) {}

  // Called when a report is sent, regardless of success, but not for attempts
  // that will be retried.
  virtual void OnReportSent(const AttributionReport& report,
                            bool is_debug_report,
                            const SendResult& info) {}

  // Called when a trigger is registered, regardless of success.
  virtual void OnTriggerHandled(const AttributionTrigger& trigger,
                                const CreateReportResult& result) {}
};

}  // namespace content

#endif  // CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_OBSERVER_H_
