// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_MANAGER_IMPL_H_
#define CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_MANAGER_IMPL_H_

#include <stddef.h>
#include <memory>
#include <vector>

#include "base/containers/circular_deque.h"
#include "base/containers/flat_set.h"
#include "base/feature_list.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/threading/sequence_bound.h"
#include "components/attribution_reporting/os_support.mojom.h"
#include "components/attribution_reporting/source_registration_error.mojom-forward.h"
#include "content/browser/aggregation_service/aggregation_service.h"
#include "content/browser/aggregation_service/report_scheduler_timer.h"
#include "content/browser/attribution_reporting/attribution_manager.h"
#include "content/browser/attribution_reporting/attribution_report.h"
#include "content/browser/attribution_reporting/attribution_report_sender.h"
#include "content/browser/attribution_reporting/attribution_storage.h"
#include "content/common/content_export.h"
#include "content/public/browser/storage_partition.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/abseil-cpp/absl/types/variant.h"

namespace attribution_reporting {
class SuitableOrigin;
}  // namespace attribution_reporting

namespace base {
class FilePath;
class TimeDelta;
class UpdateableSequencedTaskRunner;
}  // namespace base

namespace storage {
class SpecialStoragePolicy;
}  // namespace storage

namespace content {

class AggregatableReport;
class AggregatableReportRequest;
class AttributionCookieChecker;
class AttributionDataHostManager;
class AttributionDebugReport;
class AttributionStorage;
class AttributionStorageDelegate;
class CreateReportResult;
class StoragePartitionImpl;
class StoredSource;

struct SendResult;

CONTENT_EXPORT BASE_DECLARE_FEATURE(kAttributionVerboseDebugReporting);

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

  class CONTENT_EXPORT ScopedOsSupportForTesting {
   public:
    explicit ScopedOsSupportForTesting(attribution_reporting::mojom::OsSupport);
    ~ScopedOsSupportForTesting();

    ScopedOsSupportForTesting(const ScopedOsSupportForTesting&) = delete;
    ScopedOsSupportForTesting& operator=(const ScopedOsSupportForTesting&) =
        delete;

    ScopedOsSupportForTesting(ScopedOsSupportForTesting&&) = delete;
    ScopedOsSupportForTesting& operator=(ScopedOsSupportForTesting&&) = delete;

   private:
    const attribution_reporting::mojom::OsSupport previous_;
  };

  static std::unique_ptr<AttributionManagerImpl> CreateForTesting(
      const base::FilePath& user_data_directory,
      size_t max_pending_events,
      scoped_refptr<storage::SpecialStoragePolicy> special_storage_policy,
      std::unique_ptr<AttributionStorageDelegate> storage_delegate,
      std::unique_ptr<AttributionCookieChecker> cookie_checker,
      std::unique_ptr<AttributionReportSender> report_sender,
      StoragePartitionImpl* storage_partition,
      scoped_refptr<base::UpdateableSequencedTaskRunner> storage_task_runner);

  static std::unique_ptr<AttributionManagerImpl> CreateWithNewDbForTesting(
      StoragePartitionImpl* storage_partition,
      const base::FilePath& user_data_directory,
      scoped_refptr<storage::SpecialStoragePolicy> special_storage_policy);

  // Returns whether OS-level attribution is enabled. `kDisabled` is returned
  // before the result is returned from the underlying platform (e.g. Android).
  static attribution_reporting::mojom::OsSupport GetOsSupport() {
    return g_os_support_;
  }

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
  void HandleSource(StorableSource source) override;
  void HandleTrigger(AttributionTrigger trigger) override;
  void GetActiveSourcesForWebUI(
      base::OnceCallback<void(std::vector<StoredSource>)> callback) override;
  void GetPendingReportsForInternalUse(
      AttributionReport::Types report_types,
      int limit,
      base::OnceCallback<void(std::vector<AttributionReport>)> callback)
      override;
  void SendReportsForWebUI(const std::vector<AttributionReport::Id>& ids,
                           base::OnceClosure done) override;
  void ClearData(base::Time delete_begin,
                 base::Time delete_end,
                 StoragePartition::StorageKeyMatcherFunction filter,
                 BrowsingDataFilterBuilder* filter_builder,
                 bool delete_rate_limit_data,
                 base::OnceClosure done) override;
  void NotifyFailedSourceRegistration(
      const std::string& header_value,
      const attribution_reporting::SuitableOrigin& source_origin,
      const attribution_reporting::SuitableOrigin& reporting_origin,
      attribution_reporting::mojom::SourceRegistrationError) override;

  void GetAllDataKeys(
      base::OnceCallback<void(std::vector<DataKey>)> callback) override;

  void RemoveAttributionDataByDataKey(const DataKey& data_key,
                                      base::OnceClosure callback) override;

 private:
  friend class AttributionManagerImplTest;

  static void SetOsSupportForTesting(
      attribution_reporting::mojom::OsSupport os_support);

  // TODO(crbug.com/1373536): The OS-level support should be derived from the
  // underlying platform (e.g. Android).
  static attribution_reporting::mojom::OsSupport g_os_support_;

  using ReportSentCallback = AttributionReportSender::ReportSentCallback;
  using SourceOrTrigger = absl::variant<StorableSource, AttributionTrigger>;

  AttributionManagerImpl(
      StoragePartitionImpl* storage_partition,
      const base::FilePath& user_data_directory,
      size_t max_pending_events,
      scoped_refptr<storage::SpecialStoragePolicy> special_storage_policy,
      std::unique_ptr<AttributionStorageDelegate> storage_delegate,
      std::unique_ptr<AttributionCookieChecker> cookie_checker,
      std::unique_ptr<AttributionReportSender> report_sender,
      std::unique_ptr<AttributionDataHostManager> data_host_manager,
      scoped_refptr<base::UpdateableSequencedTaskRunner> storage_task_runner);

  void MaybeEnqueueEvent(SourceOrTrigger event);
  void ProcessEvents();
  void ProcessNextEvent(bool is_debug_cookie_set);
  void StoreSource(StorableSource source,
                   absl::optional<uint64_t> cleared_debug_key,
                   bool is_debug_cookie_set);
  void StoreTrigger(AttributionTrigger trigger,
                    absl::optional<uint64_t> cleared_debug_key,
                    bool is_debug_cookie_set);

  void GetReportsToSend();

  void OnGetReportsToSendFromWebUI(base::OnceClosure done,
                                   std::vector<AttributionReport> reports);

  void SendReports(base::RepeatingClosure web_ui_callback,
                   std::vector<AttributionReport> reports);
  void PrepareToSendReport(AttributionReport report,
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
      absl::optional<AggregatableReport> assembled_report,
      AggregationService::AssemblyStatus);
  void MarkReportCompleted(AttributionReport::Id report_id);

  void OnSourceStored(const StorableSource& source,
                      absl::optional<uint64_t> cleared_debug_key,
                      bool is_debug_cookie_set,
                      AttributionStorage::StoreSourceResult result);
  void OnReportStored(const AttributionTrigger& trigger,
                      absl::optional<uint64_t> cleared_debug_key,
                      bool is_debug_cookie_set,
                      CreateReportResult result);

  void MaybeSendDebugReport(AttributionReport&&);

  void NotifySourcesChanged();
  void NotifyReportsChanged(AttributionReport::Type report_type);
  void NotifyReportSent(bool is_debug_report,
                        const AttributionReport&,
                        SendResult);
  void NotifyDebugReportSent(const AttributionDebugReport&, int status);

  bool IsReportAllowed(const AttributionReport&) const;

  void MaybeSendVerboseDebugReport(
      const StorableSource& source,
      bool is_debug_cookie_set,
      const AttributionStorage::StoreSourceResult& result);

  void MaybeSendVerboseDebugReport(const AttributionTrigger& trigger,
                                   bool is_debug_cookie_set,
                                   const CreateReportResult& result);

  void OnClearDataComplete();

  // Never null.
  const raw_ptr<StoragePartitionImpl> storage_partition_;

  // Holds pending sources and triggers in the order they were received by the
  // browser. For the time being, they must be processed in this order in order
  // to ensure that behavioral requirements are met and to ensure that
  // `AttributionObserver`s are notified in the correct order, which
  // the simulator currently depends on. We may be able to loosen this
  // requirement in the future so that there are conceptually separate queues
  // per <source origin, destination origin, reporting origin>.
  base::circular_deque<SourceOrTrigger> pending_events_;

  // Controls the maximum size of `pending_events_` to avoid unbounded memory
  // growth with adversarial input.
  size_t max_pending_events_;

  // The task runner for all attribution reporting storage operations.
  // Updateable to allow for priority to be temporarily increased to
  // `USER_VISIBLE` when a clear data task is queued or running. Otherwise
  // `BEST_EFFORT` is used.
  scoped_refptr<base::UpdateableSequencedTaskRunner> storage_task_runner_;

  // How many clear data storage tasks are queued or running currently, i.e.
  // have been posted but the reply has not been run.
  int num_pending_clear_data_tasks_ = 0;

  base::SequenceBound<AttributionStorage> attribution_storage_;

  ReportSchedulerTimer scheduler_timer_;

  std::unique_ptr<AttributionDataHostManager> data_host_manager_;

  // Storage policy for the browser context |this| is in. May be nullptr.
  scoped_refptr<storage::SpecialStoragePolicy> special_storage_policy_;

  std::unique_ptr<AttributionCookieChecker> cookie_checker_;

  std::unique_ptr<AttributionReportSender> report_sender_;

  // Set of all conversion IDs that are currently being sent, deleted, or
  // updated. The number of concurrent conversion reports being sent at any time
  // is expected to be small, so a `flat_set` is used.
  base::flat_set<AttributionReport::Id> reports_being_sent_;

  base::ObserverList<AttributionObserver> observers_;

  base::WeakPtrFactory<AttributionManagerImpl> weak_factory_{this};
};

// Gets the delay for a report that has failed to be sent
// `failed_send_attempts` times.
// Returns `absl::nullopt` to indicate that no more attempts should be made.
// Otherwise, the return value must be positive. `failed_send_attempts` is
// guaranteed to be positive.
//
// Exposed here for testing.
CONTENT_EXPORT
absl::optional<base::TimeDelta> GetFailedReportDelay(int failed_send_attempts);

}  // namespace content

#endif  // CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_MANAGER_IMPL_H_
