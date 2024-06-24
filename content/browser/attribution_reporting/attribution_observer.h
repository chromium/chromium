// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_OBSERVER_H_
#define CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_OBSERVER_H_

#include <stdint.h>

#include <optional>

#include "base/observer_list_types.h"
#include "base/time/time.h"
#include "base/values.h"
#include "content/browser/attribution_reporting/attribution_reporting.mojom-forward.h"
#include "content/browser/attribution_reporting/process_aggregatable_debug_report_result.mojom-forward.h"
#include "content/browser/attribution_reporting/store_source_result.mojom-forward.h"

namespace attribution_reporting {
struct OsRegistrationItem;
}  // namespace attribution_reporting

namespace url {
class Origin;
}  // namespace url

namespace content {

class AggregatableDebugReport;
class AttributionDebugReport;
class AttributionReport;
class CreateReportResult;
class StorableSource;

struct SendAggregatableDebugReportResult;
struct SendResult;

// Observes events in the Attribution Reporting API. Observers are registered on
// `AttributionManager`.
class AttributionObserver : public base::CheckedObserver {
 public:
  ~AttributionObserver() override = default;

  // Called when sources in storage change.
  virtual void OnSourcesChanged() {}

  // Called when reports in storage change.
  virtual void OnReportsChanged() {}

  // Called when a source is registered, regardless of success.
  virtual void OnSourceHandled(
      const StorableSource& source,
      base::Time source_time,
      std::optional<uint64_t> cleared_debug_key,
      attribution_reporting::mojom::StoreSourceResult) {}

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

  // Called when an aggregatable debug report is assembled and sent, regardless
  // of success.
  virtual void OnAggregatableDebugReportSent(
      const AggregatableDebugReport&,
      base::ValueView report_body,
      attribution_reporting::mojom::ProcessAggregatableDebugReportResult,
      const SendAggregatableDebugReportResult&) {}

  // Called when a trigger is registered, regardless of success.
  virtual void OnTriggerHandled(std::optional<uint64_t> cleared_debug_key,
                                const CreateReportResult& result) {}

  // Called when an OS source or trigger registration is handled, regardless of
  // success.
  virtual void OnOsRegistration(
      base::Time time,
      const attribution_reporting::OsRegistrationItem&,
      const url::Origin& top_level_origin,
      attribution_reporting::mojom::RegistrationType,
      bool is_debug_key_allowed,
      attribution_reporting::mojom::OsRegistrationResult) {}

  virtual void OnDebugModeChanged(bool debug_mode) {}
};

}  // namespace content

#endif  // CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_OBSERVER_H_
