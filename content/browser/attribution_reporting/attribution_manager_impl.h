// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_MANAGER_IMPL_H_
#define CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_MANAGER_IMPL_H_

#include <memory>
#include <vector>

#include "base/callback_forward.h"
#include "base/compiler_specific.h"
#include "base/containers/circular_deque.h"
#include "base/containers/flat_set.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/threading/sequence_bound.h"
#include "content/browser/aggregation_service/aggregation_service.h"
#include "content/browser/attribution_reporting/attribution_data_host_manager.h"
#include "content/browser/attribution_reporting/attribution_manager.h"
#include "content/browser/attribution_reporting/attribution_report.h"
#include "content/browser/attribution_reporting/attribution_report_scheduler.h"
#include "content/browser/attribution_reporting/attribution_storage.h"
#include "content/common/content_export.h"
#include "storage/browser/quota/special_storage_policy.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/abseil-cpp/absl/types/variant.h"

namespace base {
class FilePath;
}  // namespace base

namespace url {
class Origin;
}  // namespace url

namespace content {

class AggregatableReport;
class AttributionCookieChecker;
class AttributionReportSender;
class AttributionStorageDelegate;
class BrowserContext;
class CreateReportResult;
class StoragePartitionImpl;

struct DeactivatedSource;
struct SendResult;

// Provides access to the manager owned by the default StoragePartition.
class AttributionManagerProviderImpl : public AttributionManager::Provider {
 public:
  AttributionManagerProviderImpl() = default;
  AttributionManagerProviderImpl(const AttributionManagerProviderImpl& other) =
      delete;
  AttributionManagerProviderImpl& operator=(
      const AttributionManagerProviderImpl& other) = delete;
  AttributionManagerProviderImpl(AttributionManagerProviderImpl&& other) =
      delete;
  AttributionManagerProviderImpl& operator=(
      AttributionManagerProviderImpl&& other) = delete;
  ~AttributionManagerProviderImpl() override = default;

  // AttributionManagerProvider:
  AttributionManager* GetManager(WebContents* web_contents) const override;
};

// UI thread class that manages the lifetime of the underlying attribution
// storage and coordinates sending attribution reports. Owned by the storage
// partition.
class CONTENT_EXPORT AttributionManagerImpl : public AttributionManager {
 public:
  using IsReportAllowedCallback =
      base::RepeatingCallback<bool(const AttributionReport&)>;

  static IsReportAllowedCallback DefaultIsReportAllowedCallback(
      BrowserContext*);

  // Configures underlying storage to be setup in memory, rather than on
  // disk. This speeds up initialization to avoid timeouts in test environments.
  static void RunInMemoryForTesting();

  static std::unique_ptr<AttributionManagerImpl> CreateForTesting(
      IsReportAllowedCallback is_report_allowed_callback,
      const base::FilePath& user_data_directory,
      scoped_refptr<storage::SpecialStoragePolicy> special_storage_policy,
      std::unique_ptr<AttributionStorageDelegate> storage_delegate,
      std::unique_ptr<AttributionCookieChecker> cookie_checker,
      std::unique_ptr<AttributionReportSender> report_sender,
      StoragePartitionImpl* storage_partition = nullptr);

  AttributionManagerImpl(
      StoragePartitionImpl* storage_partition,
      const base::FilePath& user_data_directory,
      scoped_refptr<storage::SpecialStoragePolicy> special_storage_policy);
  AttributionManagerImpl(const AttributionManagerImpl& other) = delete;
  AttributionManagerImpl& operator=(const AttributionManagerImpl& other) =
      delete;
  AttributionManagerImpl(AttributionManagerImpl&& other) = delete;
  AttributionManagerImpl& operator=(AttributionManagerImpl&& other) = delete;
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
      base::OnceCallback<void(std::vector<AttributionReport>)> callback)
      override;
  void SendReportsForWebUI(
      const std::vector<AttributionReport::EventLevelData::Id>& ids,
      base::OnceClosure done) override;
  void ClearData(base::Time delete_begin,
                 base::Time delete_end,
                 base::RepeatingCallback<bool(const url::Origin&)> filter,
                 base::OnceClosure done) override;

  using SourceOrTrigger = absl::variant<StorableSource, AttributionTrigger>;

  void MaybeEnqueueEventForTesting(SourceOrTrigger event);

  void AddAggregatableAttributionForTesting(
      AggregatableAttribution aggregatable_attribution);

 private:
  friend class AttributionManagerImplTest;

  AttributionManagerImpl(
      StoragePartitionImpl* storage_partition,
      IsReportAllowedCallback is_report_allowed_callback,
      const base::FilePath& user_data_directory,
      scoped_refptr<storage::SpecialStoragePolicy> special_storage_policy,
      std::unique_ptr<AttributionStorageDelegate> storage_delegate,
      std::unique_ptr<AttributionCookieChecker> cookie_checker,
      std::unique_ptr<AttributionReportSender> report_sender,
      std::unique_ptr<AttributionDataHostManager> data_host_manager);

  void MaybeEnqueueEvent(SourceOrTrigger event);
  void ProcessEvents();
  void ProcessNextEvent(bool is_debug_cookie_set);
  void StoreSource(StorableSource source);
  void StoreTrigger(AttributionTrigger trigger);

  void GetReportsToSend();
  void OnGetReportsToSend(std::vector<AttributionReport> reports);

  void OnGetReportsToSendFromWebUI(base::OnceClosure done,
                                   std::vector<AttributionReport> reports);

  void SendReports(std::vector<AttributionReport> reports,
                   bool log_metrics,
                   base::RepeatingClosure done);
  void SendReport(AttributionReport report, base::OnceClosure done);
  void OnReportSent(base::OnceClosure done,
                    AttributionReport report,
                    SendResult info);
  void AssembleAggregateReport(AttributionReport report,
                               base::OnceClosure done);
  void OnAggregateReportAssembled(
      base::OnceClosure done,
      AttributionReport report,
      absl::optional<AggregatableReport> assembled_report,
      AggregationService::AssemblyStatus status);
  void MarkReportCompleted(AttributionReport::Id report_id);

  void OnReportStored(CreateReportResult result);

  void NotifySourcesChanged();
  void NotifyReportsChanged();
  void NotifySourceDeactivated(const DeactivatedSource& source);

  // Friend to expose the AttributionStorage for certain tests.
  friend std::vector<AttributionReport> GetAttributionReportsForTesting(
      AttributionManagerImpl* manager,
      base::Time max_report_time);

  // Might be `nullptr` for testing.
  raw_ptr<StoragePartitionImpl> storage_partition_;

  // Internally holds a non-owning pointer to `BrowserContext`.
  IsReportAllowedCallback is_report_allowed_callback_;

  // Holds pending sources and triggers in the order they were received by the
  // browser. For the time being, they must be processed in this order in order
  // to ensure that behavioral requirements are met and to ensure that
  // `AttributionObserver`s are notified in the correct order, which
  // the simulator currently depends on. We may be able to loosen this
  // requirement in the future so that there are conceptually separate queues
  // per <source origin, destination origin, reporting origin>.
  base::circular_deque<SourceOrTrigger> pending_events_;

  base::SequenceBound<AttributionStorage> attribution_storage_;

  AttributionReportScheduler scheduler_;

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

  base::WeakPtrFactory<AttributionManagerImpl> weak_factory_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_MANAGER_IMPL_H_
