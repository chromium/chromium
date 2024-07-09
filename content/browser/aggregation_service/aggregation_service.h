// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_AGGREGATION_SERVICE_AGGREGATION_SERVICE_H_
#define CONTENT_BROWSER_AGGREGATION_SERVICE_AGGREGATION_SERVICE_H_

#include <set>
#include <vector>

#include "base/functional/callback_forward.h"
#include "content/browser/aggregation_service/aggregatable_report_assembler.h"
#include "content/browser/aggregation_service/aggregatable_report_sender.h"
#include "content/browser/aggregation_service/aggregation_service_storage.h"
#include "content/common/content_export.h"
#include "content/public/browser/storage_partition.h"

class GURL;

namespace base {
class Time;
class Value;
}  // namespace base

namespace url {
class Origin;
}  // namespace url

namespace content {

class AggregationServiceObserver;
class AggregatableReport;
class AggregatableReportRequest;
class BrowserContext;

// External interface for the aggregation service.
class CONTENT_EXPORT AggregationService {
 public:
  using AssemblyStatus = AggregatableReportAssembler::AssemblyStatus;
  using AssemblyCallback = AggregatableReportAssembler::AssemblyCallback;

  using SendStatus = AggregatableReportSender::RequestStatus;
  using SendCallback = AggregatableReportSender::ReportSentCallback;

  // No more report requests can be scheduled and not yet sent than this. Any
  // additional requests will silently be dropped until there is more capacity.
  // This ensures malicious actors cannot use unbounded memory or disk space.
  static constexpr int kMaxStoredReportsPerReportingOrigin = 1000;

  virtual ~AggregationService() = default;

  // Gets the AggregationService that should be used for handling aggregations
  // in the given `browser_context`. Returns nullptr if aggregation service is
  // not enabled.
  static AggregationService* GetService(BrowserContext* browser_context);

  // Constructs an AggregatableReport from the information in `report_request`.
  // `callback` will be run once completed which returns the assembled report
  // if successful, otherwise `std::nullopt` will be returned.
  virtual void AssembleReport(AggregatableReportRequest report_request,
                              AssemblyCallback callback) = 0;

  // TODO(alexmt): Consider removing `SendReport()`.

  // Sends an aggregatable report to the reporting endpoint `url`.
  virtual void SendReport(
      const GURL& url,
      const AggregatableReport& report,
      std::optional<AggregatableReportRequest::DelayType> delay_type,
      SendCallback callback) = 0;

  // Sends the contents of an aggregatable report to the reporting endpoint
  // `url`. This allows a caller to modify the report's JSON serialization as
  // needed.
  virtual void SendReport(
      const GURL& url,
      const base::Value& contents,
      std::optional<AggregatableReportRequest::DelayType> delay_type,
      SendCallback callback) = 0;

  // Deletes all data in storage that were fetched/stored between `delete_begin`
  // and `delete_end` time (inclusive). Null times are treated as unbounded
  // lower or upper range. If `!filter.is_null()`, requests with a reporting
  // origin that does *not* match the `filter` are retained (i.e. not cleared);
  // `filter` does not affect public key deletion.
  virtual void ClearData(base::Time delete_begin,
                         base::Time delete_end,
                         StoragePartition::StorageKeyMatcherFunction filter,
                         base::OnceClosure done) = 0;

  // Schedules `report_request` to be assembled and sent at its scheduled report
  // time. It is stored on disk (unless in incognito) until then. See the
  // `AggregatableReportScheduler` for details.
  virtual void ScheduleReport(AggregatableReportRequest report_request) = 0;

  // Immediately assembles and then sends `report_request`.
  virtual void AssembleAndSendReport(
      AggregatableReportRequest report_request) = 0;

  // Gets all pending report requests that are currently stored. Used for
  // populating WebUI.
  // TODO(linnan): Consider enforcing a limit on the number of requests
  // returned.
  virtual void GetPendingReportRequestsForWebUI(
      base::OnceCallback<void(
          std::vector<AggregationServiceStorage::RequestAndId>)> callback) = 0;

  // Sends the given reports immediately, and runs `reports_sent_callback` once
  // they have all been sent.
  virtual void SendReportsForWebUI(
      const std::vector<AggregationServiceStorage::RequestId>& ids,
      base::OnceClosure reports_sent_callback) = 0;

  // Runs `callback` with a set containing all the distinct reporting origins
  // stored in the report request table.
  virtual void GetPendingReportReportingOrigins(
      base::OnceCallback<void(std::set<url::Origin>)> callback) = 0;

  virtual void AddObserver(AggregationServiceObserver* observer) = 0;

  virtual void RemoveObserver(AggregationServiceObserver* observer) = 0;
};

}  // namespace content

#endif  // CONTENT_BROWSER_AGGREGATION_SERVICE_AGGREGATION_SERVICE_H_
