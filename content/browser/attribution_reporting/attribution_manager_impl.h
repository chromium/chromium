// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_MANAGER_IMPL_H_
#define CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_MANAGER_IMPL_H_

#include <memory>
#include <vector>

#include "base/callback_forward.h"
#include "base/compiler_specific.h"
#include "base/containers/flat_set.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/threading/sequence_bound.h"
#include "base/timer/wall_clock_timer.h"
#include "content/browser/attribution_reporting/attribution_manager.h"
#include "content/browser/attribution_reporting/attribution_report.h"
#include "content/browser/attribution_reporting/attribution_storage.h"
#include "content/common/content_export.h"
#include "services/network/public/cpp/network_connection_tracker.h"
#include "storage/browser/quota/special_storage_policy.h"

class GURL;

namespace base {
class FilePath;
class Value;
}  // namespace base

namespace content {

class BrowserContext;
class StoragePartitionImpl;

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
class CONTENT_EXPORT AttributionManagerImpl
    : public AttributionManager,
      public network::NetworkConnectionTracker::NetworkConnectionObserver {
 public:
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum class DeleteEvent {
    kStarted = 0,
    kSucceeded = 1,
    kFailed = 2,
    kMaxValue = kFailed,
  };

  // This class is responsible for sending conversion reports to their
  // configured endpoints over the network.
  class NetworkSender {
   public:
    virtual ~NetworkSender() = default;

    // Callback used to notify caller that the requested report has been sent.
    using ReportSentCallback = base::OnceCallback<void(SendResult)>;

    // Generates and sends a conversion report matching |report|. This should
    // generate a secure POST request with no-credentials.
    virtual void SendReport(GURL report_url,
                            base::Value report_body,
                            ReportSentCallback sent_callback) = 0;
  };

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
      std::unique_ptr<AttributionStorage::Delegate> storage_delegate,
      std::unique_ptr<NetworkSender> network_sender);

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
  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;
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

  void HandleSourceInternalForTesting(StorableSource);
  void HandleTriggerInternalForTesting(AttributionTrigger);

 private:
  friend class AttributionManagerImplTest;

  AttributionManagerImpl(
      IsReportAllowedCallback is_report_allowed_callback,
      const base::FilePath& user_data_directory,
      scoped_refptr<storage::SpecialStoragePolicy> special_storage_policy,
      std::unique_ptr<AttributionStorage::Delegate> storage_delegate,
      std::unique_ptr<NetworkSender> network_sender);

  // network::NetworkConnectionTracker::NetworkConnectionObserver:
  void OnConnectionChanged(
      network::mojom::ConnectionType connection_type) override;

  // Retrieves at most |limit| reports from storage whose |report_time| <=
  // |max_report_time|, and calls |handler_function| on them; use a negative
  // number for no limit.
  using ReportsHandlerFunc =
      base::OnceCallback<void(std::vector<AttributionReport>)>;
  void GetAndHandleReports(ReportsHandlerFunc handler_function,
                           base::Time max_report_time,
                           int limit);

  void UpdateGetReportsToSendTimer(absl::optional<base::Time> time);
  void StartGetReportsToSendTimer();
  void GetReportsToSend();
  void OnGetReportsToSend(std::vector<AttributionReport> reports);

  void OnGetReportsToSendFromWebUI(base::OnceClosure done,
                                   std::vector<AttributionReport> reports);

  void SendReports(std::vector<AttributionReport> reports,
                   bool log_metrics,
                   base::RepeatingClosure done);
  void OnReportSent(base::OnceClosure done,
                    AttributionReport report,
                    SendResult info);
  void MarkReportCompleted(AttributionReport::EventLevelData::Id report_id);

  void OnReportStored(AttributionStorage::CreateReportResult result);

  void NotifySourcesChanged();
  void NotifyReportsChanged();
  void NotifySourceDeactivated(
      const AttributionStorage::DeactivatedSource& source);

  void HandleSourceInternal(StorableSource source);
  void HandleTriggerInternal(AttributionTrigger trigger);

  // Friend to expose the AttributionStorage for certain tests.
  friend std::vector<AttributionReport> GetAttributionsToReportForTesting(
      AttributionManagerImpl* manager,
      base::Time max_report_time);

  // Internally holds a non-owning pointer to `BrowserContext`.
  IsReportAllowedCallback is_report_allowed_callback_;

  base::SequenceBound<AttributionStorage> attribution_storage_;

  // Storage policy for the browser context |this| is in. May be nullptr.
  scoped_refptr<storage::SpecialStoragePolicy> special_storage_policy_;

  std::unique_ptr<NetworkSender> network_sender_;

  base::WallClockTimer get_reports_to_send_timer_;

  // Set of all conversion IDs that are currently being sent, deleted, or
  // updated. The number of concurrent conversion reports being sent at any time
  // is expected to be small, so a `flat_set` is used.
  base::flat_set<AttributionReport::EventLevelData::Id> reports_being_sent_;

  base::ObserverList<Observer> observers_;

  base::WeakPtrFactory<AttributionManagerImpl> weak_factory_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_MANAGER_IMPL_H_
