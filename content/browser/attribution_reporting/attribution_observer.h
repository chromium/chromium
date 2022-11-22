// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_OBSERVER_H_
#define CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_OBSERVER_H_

#include <string>

#include "base/observer_list_types.h"
#include "base/time/time.h"
#include "components/attribution_reporting/source_registration_error.mojom.h"
#include "content/browser/attribution_reporting/attribution_report.h"
#include "content/browser/attribution_reporting/storable_source.h"

namespace attribution_reporting {
class SuitableOrigin;
}  // namespace attribution_reporting

namespace content {

class AttributionDebugReport;
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
  virtual void OnReportsChanged(AttributionReport::Type report_type) {}

  // Called when a source is registered, regardless of success.
  virtual void OnSourceHandled(const StorableSource& source,
                               absl::optional<uint64_t> cleared_debug_key,
                               StorableSource::Result result) {}

  // Called when a report is sent, regardless of success, but not for attempts
  // that will be retried.
  virtual void OnReportSent(const AttributionReport& report,
                            bool is_debug_report,
                            const SendResult& info) {}

  // Called when a verbose debug report is sent, regardless of success.
  // If `status` is positive, it is the HTTP response code. Otherwise, it is the
  // network error.
  virtual void OnDebugReportSent(const AttributionDebugReport&,
                                 int status,
                                 base::Time) {}

  // Called when a trigger is registered, regardless of success.
  virtual void OnTriggerHandled(const AttributionTrigger& trigger,
                                absl::optional<uint64_t> cleared_debug_key,
                                const CreateReportResult& result) {}

  // Called when the source header registration json parser fails.
  virtual void OnFailedSourceRegistration(
      const std::string& header_value,
      base::Time source_time,
      const attribution_reporting::SuitableOrigin& reporting_origin,
      attribution_reporting::mojom::SourceRegistrationError) {}
};

}  // namespace content

#endif  // CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_OBSERVER_H_
