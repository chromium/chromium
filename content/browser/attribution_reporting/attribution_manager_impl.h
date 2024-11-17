// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_MANAGER_IMPL_H_
#define CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_MANAGER_IMPL_H_

#include <stdint.h>

#include <memory>
#include <optional>
#include <vector>

#include "base/containers/flat_set.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ref.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/threading/sequence_bound.h"
#include "content/browser/aggregation_service/aggregation_service.h"
#include "content/browser/aggregation_service/report_scheduler_timer.h"
#include "content/browser/attribution_reporting/attribution_manager.h"
#include "content/browser/attribution_reporting/attribution_report.h"
#include "content/browser/attribution_reporting/attribution_reporting.mojom-forward.h"
#include "content/browser/attribution_reporting/process_aggregatable_debug_report_result.mojom-forward.h"
#include "content/common/content_export.h"
#include "content/public/browser/storage_partition.h"

namespace attribution_reporting {
struct OsRegistrationItem;
}  // namespace attribution_reporting

namespace base {
class FilePath;
class Time;
class TimeDelta;
class UpdateableSequencedTaskRunner;
class ValueView;
}  // namespace base

namespace storage {
class SpecialStoragePolicy;
}  // namespace storage

namespace url {
class Origin;
}  // namespace url

namespace content {

class AggregatableDebugReport;
class AggregatableReport;
class AggregatableReportRequest;
class AttributionDataHostManager;
class AttributionDebugReport;
class AttributionOsLevelManager;
class AttributionReportSender;
class AttributionResolver;
class AttributionResolverDelegate;
class CreateReportResult;
class StoragePartitionImpl;
class StoreSourceResult;

struct GlobalRenderFrameHostId;
struct OsRegistration;
struct ProcessAggregatableDebugReportResult;
struct SendAggregatableDebugReportResult;
struct SendResult;

// UI thread class that manages the lifetime of the underlying attribution
// storage and coordinates sending attribution reports. Owned by the storage
// partition.
class CONTENT_EXPORT AttributionManagerImpl : public AttributionManager {
 public:
  // Configures underlying storage to be setup in memory, rather than on
  // disk. This speeds up initialization to avoid timeouts in test environments.
  class CONTENT_EXPORT ScopedUseInMemoryStorageForTesting {
   public:
    ScopedUseInMemoryStorageForTesting();

    ~ScopedUseInMemoryStorageForTesting();

    ScopedUseInMemoryStorageForTesting(
        const ScopedUseInMemoryStorageForTesting&) = delete;
    ScopedUseInMemoryStorageForTesting& operator=(
        const ScopedUseInMemoryStorageForTesting&) = delete;

    ScopedUseInMemoryStorageForTesting(ScopedUseInMemoryStorageForTesting&&) =
        delete;
    ScopedUseInMemoryStorageForTesting& operator=(
        ScopedUseInMemoryStorageForTesting&&) = delete;

   private:
    const bool previous_;
  };

  static std::unique_ptr<AttributionManagerImpl> CreateForTesting(
      const base::FilePath& user_data_directory,
      scoped_refptr<storage::SpecialStoragePolicy> special_storage_policy,
      std::unique_ptr<AttributionResolverDelegate> resolver_delegate,
      std::unique_ptr<AttributionReportSender> report_sender,
      std::unique_ptr<AttributionOsLevelManager> os_level_manager,
      StoragePartitionImpl* storage_partition,
      scoped_refptr<base::UpdateableSequencedTaskRunner> resolver_task_runner);

  AttributionManagerImpl(
      StoragePartitionImpl* storage_partition,
      const base::FilePath& user_data_directory,
      scoped_refptr<storage::SpecialStoragePolicy> special_storage_policy);
  AttributionManagerImpl(const AttributionManagerImpl&) = delete;
  AttributionManagerImpl& operator=(const AttributionManagerImpl&) = delete;
  AttributionManagerImpl(AttributionManagerImpl&&) = delete;
  AttributionManagerImpl& operator=(AttributionManagerImpl&&) = delete;
  ~AttributionManagerImpl() override;

  // AttributionManager:
  void AddObserver(AttributionObserver* observer) override;
  void RemoveObserver(AttributionObserver* observer) override;
  AttributionDataHostManager* GetDataHostManager() override;
  void HandleSource(StorableSource source,
                    GlobalRenderFrameHostId render_frame_id) override;
  void HandleTrigger(AttributionTrigger trigger,
                     GlobalRenderFrameHostId render_frame_id) override;
  void GetActiveSourcesForWebUI(
      base::OnceCallback<void(std::vector<StoredSource>)> callback) override;
  void GetPendingReportsForInternalUse(
      int limit,
      base::OnceCallback<void(std::vector<AttributionReport>)> callback)
      override;
  void SendReportForWebUI(AttributionReport::Id,
                          base::OnceClosure done) override;
  void ClearData(base::Time delete_begin,
                 base::Time delete_end,
                 StoragePartition::StorageKeyMatcherFunction filter,
                 BrowsingDataFilterBuilder* filter_builder,
                 bool delete_rate_limit_data,
                 base::OnceClosure done) override;
  void SetDebugMode(std::optional<bool> enabled,
                    base::OnceClosure done) override;
  void ReportRegistrationHeaderError(
      attribution_reporting::SuitableOrigin reporting_origin,
      attribution_reporting::RegistrationHeaderError,
      const attribution_reporting::SuitableOrigin& context_origin,
      bool is_within_fenced_frame,
      GlobalRenderFrameHostId render_frame_id) override;

  void GetAllDataKeys(
      base::OnceCallback<void(std::set<DataKey>)> callback) override;

  void RemoveAttributionDataByDataKey(const DataKey& data_key,
                                      base::OnceClosure callback) override;

  void HandleOsRegistration(OsRegistration) override;

 private:
  friend class AttributionManagerImplTest;

  class ReportScheduler;

  using ReportSentCallback =
      base::OnceCallback<void(const AttributionReport&, SendResult)>;

  AttributionManagerImpl(
      StoragePartitionImpl* storage_partition,
      const base::FilePath& user_data_directory,
      scoped_refptr<storage::SpecialStoragePolicy> special_storage_policy,
      std::unique_ptr<AttributionResolverDelegate> resolver_delegate,
      std::unique_ptr<AttributionReportSender> report_sender,
      std::unique_ptr<AttributionOsLevelManager> os_level_manager,
      scoped_refptr<base::UpdateableSequencedTaskRunner> resolver_task_runner,
      bool debug_mode);

  void GetReportsToSend();

  void OnGetReportToSendFromWebUI(base::OnceClosure done,
                                  std::optional<AttributionReport>);

  void SendReports(std::vector<AttributionReport>);
  void SendReport(base::OnceClosure web_ui_callback,
                  base::Time now,
                  AttributionReport);
  void PrepareToSendReport(AttributionReport report,
                           bool is_debug_report,
                           ReportSentCallback callback);
  void SendReport(AttributionReport report,
                  bool is_debug_report,
                  ReportSentCallback callback);
  void OnReportSent(base::OnceClosure done,
                    const AttributionReport&,
                    SendResult info);
  void AssembleAggregatableReport(AttributionReport report,
                                  bool is_debug_report,
                                  ReportSentCallback callback);
  void OnAggregatableReportAssembled(
      AttributionReport report,
      bool is_debug_report,
      ReportSentCallback callback,
      AggregatableReportRequest,
      std::optional<AggregatableReport> assembled_report,
      AggregationService::AssemblyStatus);
  void MarkReportCompleted(AttributionReport::Id report_id);

  void OnSourceStored(std::optional<uint64_t> cleared_debug_key,
                      StoreSourceResult result);
  void OnReportStored(std::optional<uint64_t> cleared_debug_key,
                      bool cookie_based_debug_allowed,
                      CreateReportResult result);

  void MaybeSendDebugReport(AttributionReport&&);

  void NotifySourcesChanged();
  void NotifyReportsChanged();
  void NotifyReportSent(bool is_debug_report,
                        const AttributionReport&,
                        SendResult);
  void NotifyDebugReportSent(const AttributionDebugReport&, int status);
  void NotifyOsRegistration(base::Time time,
                            const attribution_reporting::OsRegistrationItem&,
                            const url::Origin& top_level_origin,
                            bool is_debug_key_allowed,
                            attribution_reporting::mojom::RegistrationType,
                            attribution_reporting::mojom::OsRegistrationResult);

  bool IsReportAllowed(const AttributionReport&) const;

  void MaybeSendVerboseDebugReport(const StoreSourceResult& result);

  void MaybeSendVerboseDebugReport(bool cookie_based_debug_allowed,
                                   const CreateReportResult& result);

  void MaybeSendVerboseDebugReports(const OsRegistration&);

  void MaybeSendAggregatableDebugReport(const StoreSourceResult& result);
  void MaybeSendAggregatableDebugReport(const CreateReportResult& result);
  void OnAggregatableDebugReportProcessed(ProcessAggregatableDebugReportResult);
  void OnAggregatableDebugReportAssembled(ProcessAggregatableDebugReportResult,
                                          AggregatableReportRequest,
                                          std::optional<AggregatableReport>,
                                          AggregationService::AssemblyStatus);
  void NotifyAggregatableDebugReportSent(
      const AggregatableDebugReport&,
      base::ValueView report_body,
      attribution_reporting::mojom::ProcessAggregatableDebugReportResult,
      SendAggregatableDebugReportResult);

  void OnUserVisibleTaskStarted();
  void OnUserVisibleTaskComplete();

  void OnClearDataComplete(bool was_user_visible);

  void OnOsRegistration(const std::vector<bool>& is_debug_key_allowed,
                        const OsRegistration&,
                        const std::vector<bool>& success);

  const raw_ref<StoragePartitionImpl> storage_partition_;

  // The task runner for all operations on the resolver.
  // Updateable to allow for priority to be temporarily increased to
  // `USER_VISIBLE` when a user-visible storage task is queued or running.
  // Otherwise `BEST_EFFORT` is used.
  scoped_refptr<base::UpdateableSequencedTaskRunner> resolver_task_runner_;

  // How many user-visible storage tasks are queued or running currently,
  // i.e. have been posted but the reply has not been run.
  int num_pending_user_visible_tasks_ = 0;

  base::SequenceBound<AttributionResolver> attribution_resolver_;

  std::unique_ptr<ReportSchedulerTimer> scheduler_timer_;

  std::unique_ptr<AttributionDataHostManager> data_host_manager_;

  // Storage policy for the browser context |this| is in. May be nullptr.
  scoped_refptr<storage::SpecialStoragePolicy> special_storage_policy_;

  std::unique_ptr<AttributionReportSender> report_sender_;

  // Set of all conversion IDs that are currently being sent, deleted, or
  // updated. The number of concurrent conversion reports being sent at any time
  // is expected to be small, so a `flat_set` is used.
  base::flat_set<AttributionReport::Id> reports_being_sent_;

  base::ObserverList<AttributionObserver> observers_;

  const std::unique_ptr<AttributionOsLevelManager> os_level_manager_;

  // Technically redundant with fields in the `AttributionResolverDelegate` but
  // duplicated here to avoid an async call to retrieve them.
  bool debug_mode_ = false;

  // Caches the
  // `FeatureList::IsEnabled(kAttributionReportDeliveryThirdRetryAttempt` check
  // as to reduce large map lookups.
  bool third_retry_enabled_ = false;

  base::WeakPtrFactory<AttributionManagerImpl> weak_factory_{this};
};

// Gets the delay for a report that has failed to be sent
// `failed_send_attempts` times.
// Returns `std::nullopt` to indicate that no more attempts should be made.
// Otherwise, the return value must be positive. `failed_send_attempts` is
// guaranteed to be positive.
//
// Exposed here for testing.
CONTENT_EXPORT
std::optional<base::TimeDelta> GetFailedReportDelay(int failed_send_attempts,
                                                    bool third_retry_enabled);

}  // namespace content

#endif  // CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_MANAGER_IMPL_H_
