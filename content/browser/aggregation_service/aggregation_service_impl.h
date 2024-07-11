// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_AGGREGATION_SERVICE_AGGREGATION_SERVICE_IMPL_H_
#define CONTENT_BROWSER_AGGREGATION_SERVICE_AGGREGATION_SERVICE_IMPL_H_

#include <memory>
#include <optional>
#include <set>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/threading/sequence_bound.h"
#include "content/browser/aggregation_service/aggregatable_report_assembler.h"
#include "content/browser/aggregation_service/aggregatable_report_scheduler.h"
#include "content/browser/aggregation_service/aggregatable_report_sender.h"
#include "content/browser/aggregation_service/aggregation_service.h"
#include "content/browser/aggregation_service/aggregation_service_observer.h"
#include "content/browser/aggregation_service/aggregation_service_storage.h"
#include "content/browser/aggregation_service/aggregation_service_storage_context.h"
#include "content/common/content_export.h"
#include "content/public/browser/storage_partition.h"

class GURL;

namespace base {
class Clock;
class ElapsedTimer;
class FilePath;
class UpdateableSequencedTaskRunner;
}  // namespace base

namespace url {
class Origin;
}  // namespace url

namespace content {

struct PublicKeyset;
class AggregatableReport;
class AggregatableReportRequest;
class AggregationServiceStorage;
class AggregatableReportScheduler;
class StoragePartitionImpl;

// UI thread class that manages the lifetime of the underlying storage. Owned by
// the StoragePartitionImpl. Lifetime is bound to lifetime of the
// StoragePartitionImpl.
class CONTENT_EXPORT AggregationServiceImpl
    : public AggregationService,
      public AggregationServiceStorageContext {
 public:
  static std::unique_ptr<AggregationServiceImpl> CreateForTesting(
      bool run_in_memory,
      const base::FilePath& user_data_directory,
      const base::Clock* clock,
      std::unique_ptr<AggregatableReportScheduler> scheduler,
      std::unique_ptr<AggregatableReportAssembler> assembler,
      std::unique_ptr<AggregatableReportSender> sender);

  AggregationServiceImpl(bool run_in_memory,
                         const base::FilePath& user_data_directory,
                         StoragePartitionImpl* storage_partition);
  AggregationServiceImpl(const AggregationServiceImpl& other) = delete;
  AggregationServiceImpl& operator=(const AggregationServiceImpl& other) =
      delete;
  AggregationServiceImpl(AggregationServiceImpl&& other) = delete;
  AggregationServiceImpl& operator=(AggregationServiceImpl&& other) = delete;
  ~AggregationServiceImpl() override;

  // AggregationService:
  void AssembleReport(AggregatableReportRequest report_request,
                      AssemblyCallback callback) override;
  void SendReport(
      const GURL& url,
      const AggregatableReport& report,
      std::optional<AggregatableReportRequest::DelayType> delay_type,
      SendCallback callback) override;
  void SendReport(
      const GURL& url,
      const base::Value& contents,
      std::optional<AggregatableReportRequest::DelayType> delay_type,
      SendCallback callback) override;
  void ClearData(base::Time delete_begin,
                 base::Time delete_end,
                 StoragePartition::StorageKeyMatcherFunction filter,
                 base::OnceClosure done) override;
  void ScheduleReport(AggregatableReportRequest report_request) override;
  void AssembleAndSendReport(AggregatableReportRequest report_request) override;
  void GetPendingReportRequestsForWebUI(
      base::OnceCallback<
          void(std::vector<AggregationServiceStorage::RequestAndId>)> callback)
      override;
  void SendReportsForWebUI(
      const std::vector<AggregationServiceStorage::RequestId>& ids,
      base::OnceClosure reports_sent_callback) override;
  void GetPendingReportReportingOrigins(
      base::OnceCallback<void(std::set<url::Origin>)> callback) override;
  void AddObserver(AggregationServiceObserver* observer) override;
  void RemoveObserver(AggregationServiceObserver* observer) override;

  // AggregationServiceStorageContext:
  const base::SequenceBound<AggregationServiceStorage>& GetStorage() override;

  // Sets the public keys for `url` in storage to allow testing without network.
  void SetPublicKeysForTesting(const GURL& url, const PublicKeyset& keyset);

 private:
  // Allows access to `OnScheduledReportTimeReached()`.
  friend class AggregationServiceImplTest;

  AggregationServiceImpl(bool run_in_memory,
                         const base::FilePath& user_data_directory,
                         const base::Clock* clock,
                         std::unique_ptr<AggregatableReportScheduler> scheduler,
                         std::unique_ptr<AggregatableReportAssembler> assembler,
                         std::unique_ptr<AggregatableReportSender> sender);

  void OnScheduledReportTimeReached(
      std::vector<AggregationServiceStorage::RequestAndId> requests_and_ids);

  void AssembleAndSendReports(
      std::vector<AggregationServiceStorage::RequestAndId> requests_and_ids,
      base::RepeatingClosure done);

  // `request_id` is `std::nullopt` iff `report_request` was not
  // stored/scheduled.
  void AssembleAndSendReportImpl(
      AggregatableReportRequest report_request,
      std::optional<AggregationServiceStorage::RequestId> request_id,
      base::OnceClosure done);
  void OnReportAssemblyComplete(
      base::OnceClosure done,
      std::optional<AggregationServiceStorage::RequestId> request_id,
      GURL reporting_url,
      base::ElapsedTimer elapsed_timer,
      AggregatableReportRequest report_request,
      std::optional<AggregatableReport> report,
      AggregatableReportAssembler::AssemblyStatus status);
  void OnReportSendingComplete(
      base::OnceClosure done,
      AggregatableReportRequest report_request,
      std::optional<AggregationServiceStorage::RequestId> request_id,
      AggregatableReport report,
      base::ElapsedTimer elapsed_timer,
      AggregatableReportSender::RequestStatus status);
  void OnUserVisibleTaskStarted();
  void OnUserVisibleTaskComplete();
  void OnClearDataComplete();

  void OnGetRequestsToSendFromWebUI(
      base::OnceClosure reports_sent_callback,
      std::vector<AggregationServiceStorage::RequestAndId> requests_and_ids);

  void NotifyReportHandled(
      const AggregatableReportRequest& request,
      std::optional<AggregationServiceStorage::RequestId> request_id,
      const std::optional<AggregatableReport>& report,
      AggregationServiceObserver::ReportStatus status);

  void NotifyRequestStorageModified();

  // The task runner for all aggregation service storage operations. Updateable
  // to allow for priority to be temporarily increased to `USER_VISIBLE` when a
  // clear data task is queued or running. Otherwise `BEST_EFFORT` is used.
  scoped_refptr<base::UpdateableSequencedTaskRunner> storage_task_runner_;

  // How many user visible storage tasks are queued or running currently, i.e.
  // have been posted but the reply has not been run.
  int num_pending_user_visible_tasks_ = 0;

  base::SequenceBound<AggregationServiceStorage> storage_;
  std::unique_ptr<AggregatableReportScheduler> scheduler_;
  std::unique_ptr<AggregatableReportAssembler> assembler_;
  std::unique_ptr<AggregatableReportSender> sender_;

  base::ObserverList<AggregationServiceObserver> observers_;

  base::WeakPtrFactory<AggregationServiceImpl> weak_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_AGGREGATION_SERVICE_AGGREGATION_SERVICE_IMPL_H_
