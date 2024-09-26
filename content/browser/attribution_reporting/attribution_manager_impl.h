// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_MANAGER_IMPL_H_
#define CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_MANAGER_IMPL_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <optional>
#include <vector>

#include "base/containers/circular_deque.h"
#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ref.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/threading/sequence_bound.h"
#include "base/timer/elapsed_timer.h"
#include "base/timer/timer.h"
#include "content/browser/aggregation_service/aggregation_service.h"
#include "content/browser/aggregation_service/report_scheduler_timer.h"
#include "content/browser/attribution_reporting/attribution_manager.h"
#include "content/browser/attribution_reporting/attribution_report.h"
#include "content/browser/attribution_reporting/attribution_reporting.mojom-forward.h"
#include "content/browser/attribution_reporting/process_aggregatable_debug_report_result.mojom-forward.h"
#include "content/common/content_export.h"
#include "content/public/browser/privacy_sandbox_attestations_observer.h"
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
class AttributionCookieChecker;
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
class CONTENT_EXPORT AttributionManagerImpl
    : public AttributionManager,
      public PrivacySandboxAttestationsObserver {
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
      size_t max_pending_events,
      scoped_refptr<storage::SpecialStoragePolicy> special_storage_policy,
      std::unique_ptr<AttributionResolverDelegate> resolver_delegate,
      std::unique_ptr<AttributionCookieChecker> cookie_checker,
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
      const attribution_reporting::RegistrationHeaderError&,
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

  struct SourceOrTriggerRFH;

  struct PendingReportTimings;

  AttributionManagerImpl(
      StoragePartitionImpl* storage_partition,
      const base::FilePath& user_data_directory,
      size_t max_pending_events,
      scoped_refptr<storage::SpecialStoragePolicy> special_storage_policy,
      std::unique_ptr<AttributionResolverDelegate> resolver_delegate,
      std::unique_ptr<AttributionCookieChecker> cookie_checker,
      std::unique_ptr<AttributionReportSender> report_sender,
      std::unique_ptr<AttributionOsLevelManager> os_level_manager,
      scoped_refptr<base::UpdateableSequencedTaskRunner> resolver_task_runner,
      bool debug_mode);

  void MaybeEnqueueEvent(SourceOrTriggerRFH);
  void PrepareNextEvent();
  void ProcessNextEvent(bool registration_allowed, bool is_debug_cookie_set);
  void StoreSource(StorableSource source);
  void StoreTrigger(AttributionTrigger trigger, bool is_debug_cookie_set);

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
                      bool is_debug_cookie_set,
                      CreateReportResult result);

  void MaybeSendDebugReport(AttributionReport&&);

  void NotifySourcesChanged();
  void NotifyReportsChanged();
  void NotifyReportSent(bool is_debug_report,
                        const AttributionReport&,
                        SendResult);
  void NotifyDebugReportSent(const AttributionDebugReport&, int status);
  void NotifyTotalOsRegistrationFailure(
      const OsRegistration&,
      attribution_reporting::mojom::OsRegistrationResult);
  void NotifyOsRegistration(base::Time time,
                            const attribution_reporting::OsRegistrationItem&,
                            const url::Origin& top_level_origin,
                            bool is_debug_key_allowed,
                            attribution_reporting::mojom::RegistrationType,
                            attribution_reporting::mojom::OsRegistrationResult);

  bool IsReportAllowed(const AttributionReport&) const;

  void MaybeSendVerboseDebugReport(const StoreSourceResult& result);

  void MaybeSendVerboseDebugReport(bool is_debug_cookie_set,
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

  void AddPendingAggregatableReportTiming(const AttributionReport&);
  void RecordPendingAggregatableReportsTimings();

  void OnUserVisibleTaskStarted();
  void OnUserVisibleTaskComplete();

  void OnClearDataComplete(bool was_user_visible);

  void PrepareNextOsEvent();
  void ProcessNextOsEvent(const std::vector<bool>& is_debug_key_allowed);
  void OnOsRegistration(const std::vector<bool>& is_debug_key_allowed,
                        const OsRegistration&,
                        const std::vector<bool>& success);

  // PrivacySandboxAttestationsObserver:
  void OnAttestationsLoaded() override;

  // The manager may not be ready to process attribution events when
  // attestations are not loaded yet. Returns whether the manager is ready upon
  // `OnAttestationsLoaded()`.
  bool IsReady() const;

  const raw_ref<StoragePartitionImpl> storage_partition_;

  // Holds pending sources and triggers in the order they were received by the
  // browser. For the time being, they must be processed in this order in order
  // to ensure that behavioral requirements are met. We may be able to loosen
  // this requirement in the future so that there are conceptually separate
  // queues per <source origin, destination origin, reporting origin>.
  base::circular_deque<SourceOrTriggerRFH> pending_events_;

  // Controls the maximum size of `pending_events_` to avoid unbounded memory
  // growth with adversarial input.
  size_t max_pending_events_;

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

  std::unique_ptr<AttributionCookieChecker> cookie_checker_;

  std::unique_ptr<AttributionReportSender> report_sender_;

  // Set of all conversion IDs that are currently being sent, deleted, or
  // updated. The number of concurrent conversion reports being sent at any time
  // is expected to be small, so a `flat_set` is used.
  base::flat_set<AttributionReport::Id> reports_being_sent_;

  // We keep track of pending reports timings in memory to record metrics
  // when the browser becomes unavailable to send reports due to becoming
  // offline or being shutdown.
  base::flat_map<AttributionReport::Id, PendingReportTimings>
      pending_aggregatable_reports_;

  base::ObserverList<AttributionObserver> observers_;

  const std::unique_ptr<AttributionOsLevelManager> os_level_manager_;

  base::circular_deque<OsRegistration> pending_os_events_;

  // Guardrail to ensure `OnAttestationsLoaded()` is always called to avoid
  // waiting indefinitely.
  base::OneShotTimer privacy_sandbox_attestations_timer_;

  // Timer to record the time elapsed since the construction. Used to measure
  // the delay due to privacy sandbox attestations loading.
  base::ElapsedTimer time_since_construction_;

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
