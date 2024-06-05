// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ATTRIBUTION_REPORTING_AGGREGATABLE_DEBUG_REPORT_H_
#define CONTENT_BROWSER_ATTRIBUTION_REPORTING_AGGREGATABLE_DEBUG_REPORT_H_

#include <optional>
#include <utility>
#include <vector>

#include "base/functional/function_ref.h"
#include "base/gtest_prod_util.h"
#include "base/time/time.h"
#include "base/uuid.h"
#include "components/attribution_reporting/suitable_origin.h"
#include "content/browser/attribution_reporting/process_aggregatable_debug_report_result.mojom-forward.h"
#include "content/common/content_export.h"
#include "net/base/schemeful_site.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "third_party/blink/public/mojom/aggregation_service/aggregatable_report.mojom-forward.h"

class GURL;

namespace content {

class AggregatableReportRequest;
class CreateReportResult;
class StoreSourceResult;

class CONTENT_EXPORT AggregatableDebugReport {
 public:
  // Returns `std::nullopt` if operation is not allowed, or the source
  // registration occurs within a fenced frame, or aggregatable debug
  // reporting is not enabled. A report may be created with no contributions if
  // there is no debug data registered for the corresponding debug type.
  static std::optional<AggregatableDebugReport> Create(
      base::FunctionRef<bool()> is_operation_allowed,
      const StoreSourceResult&);

  // Returns `std::nullopt` if operation is not allowed, or the trigger
  // registration occurs within a fenced frame, or aggregatable debug
  // reporting is not enabled. A report may be created with no contributions if
  // there is no debug data registered for the corresponding debug type.
  static std::optional<AggregatableDebugReport> Create(
      base::FunctionRef<bool()> is_operation_allowed,
      const CreateReportResult&);

  static AggregatableDebugReport CreateForTesting(
      std::vector<blink::mojom::AggregatableReportHistogramContribution>,
      net::SchemefulSite context_site,
      attribution_reporting::SuitableOrigin reporting_origin,
      net::SchemefulSite effective_destination,
      std::optional<attribution_reporting::SuitableOrigin>
          aggregation_coordinator_origin,
      base::Time scheduled_report_time);

  AggregatableDebugReport(const AggregatableDebugReport&) = delete;
  AggregatableDebugReport& operator=(const AggregatableDebugReport&) = delete;

  AggregatableDebugReport(AggregatableDebugReport&&);
  AggregatableDebugReport& operator=(AggregatableDebugReport&&);

  ~AggregatableDebugReport();

  const std::vector<blink::mojom::AggregatableReportHistogramContribution>&
  contributions() const {
    return contributions_;
  }

  const attribution_reporting::SuitableOrigin& reporting_origin() const {
    return reporting_origin_;
  }

  const net::SchemefulSite& context_site() const { return context_site_; }

  base::Time scheduled_report_time() const { return scheduled_report_time_; }

  void set_report_id(base::Uuid report_id) {
    report_id_ = std::move(report_id);
  }

  int BudgetRequired() const;

  net::SchemefulSite ReportingSite() const;

  void ToNull();

  GURL ReportUrl() const;

  std::optional<AggregatableReportRequest> CreateAggregatableReportRequest()
      const;

 private:
  FRIEND_TEST_ALL_PREFIXES(AggregatableDebugReportTest, SourceDebugReport_Data);
  FRIEND_TEST_ALL_PREFIXES(AggregatableDebugReportTest,
                           TriggerDebugReport_Data);

  AggregatableDebugReport(
      std::vector<blink::mojom::AggregatableReportHistogramContribution>,
      net::SchemefulSite context_site,
      attribution_reporting::SuitableOrigin reporting_origin,
      net::SchemefulSite effective_destination,
      std::optional<attribution_reporting::SuitableOrigin>
          aggregation_coordinator_origin,
      base::Time scheduled_report_time);

  std::vector<blink::mojom::AggregatableReportHistogramContribution>
      contributions_;
  net::SchemefulSite context_site_;
  attribution_reporting::SuitableOrigin reporting_origin_;
  net::SchemefulSite effective_destination_;
  std::optional<attribution_reporting::SuitableOrigin>
      aggregation_coordinator_origin_;
  base::Time scheduled_report_time_;
  base::Uuid report_id_;
};

struct ProcessAggregatableDebugReportResult {
  AggregatableDebugReport report;
  attribution_reporting::mojom::ProcessAggregatableDebugReportResult result;
};

struct SendAggregatableDebugReportResult {
  struct Sent {
    // If `status` is positive, it is the HTTP response code. Otherwise, it is
    // the network error.
    int status;
    explicit Sent(int status) : status(status) {}
  };

  struct AssemblyFailed {};

  using Result = absl::variant<Sent, AssemblyFailed>;

  Result result;
};

}  // namespace content

#endif  // CONTENT_BROWSER_ATTRIBUTION_REPORTING_AGGREGATABLE_DEBUG_REPORT_H_
