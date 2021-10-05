// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ATTRIBUTION_REPORTING_CONVERSION_MANAGER_IMPL_H_
#define CONTENT_BROWSER_ATTRIBUTION_REPORTING_CONVERSION_MANAGER_IMPL_H_

#include <memory>
#include <vector>

#include "base/callback_forward.h"
#include "base/compiler_specific.h"
#include "base/containers/circular_deque.h"
#include "base/containers/flat_set.h"
#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/sequence_bound.h"
#include "base/timer/timer.h"
#include "content/browser/attribution_reporting/attribution_report.h"
#include "content/browser/attribution_reporting/attribution_session_storage.h"
#include "content/browser/attribution_reporting/attribution_storage.h"
#include "content/browser/attribution_reporting/conversion_manager.h"
#include "content/browser/attribution_reporting/sent_report_info.h"
#include "storage/browser/quota/special_storage_policy.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace base {
class Clock;
class FilePath;
}  // namespace base

namespace content {

// Frequency we pull ConversionReports from storage and queue them to be
// reported.
extern CONTENT_EXPORT const base::TimeDelta
    kConversionManagerQueueReportsInterval;

class StoragePartitionImpl;

// Provides access to the manager owned by the default StoragePartition.
class ConversionManagerProviderImpl : public ConversionManager::Provider {
 public:
  ConversionManagerProviderImpl() = default;
  ConversionManagerProviderImpl(const ConversionManagerProviderImpl& other) =
      delete;
  ConversionManagerProviderImpl& operator=(
      const ConversionManagerProviderImpl& other) = delete;
  ConversionManagerProviderImpl(ConversionManagerProviderImpl&& other) = delete;
  ConversionManagerProviderImpl& operator=(
      ConversionManagerProviderImpl&& other) = delete;
  ~ConversionManagerProviderImpl() override = default;

  // ConversionManagerProvider:
  ConversionManager* GetManager(WebContents* web_contents) const override;
};

// UI thread class that manages the lifetime of the underlying conversion
// storage. Owned by the storage partition.
class CONTENT_EXPORT ConversionManagerImpl : public ConversionManager {
 public:
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum class DeleteEvent {
    kStarted = 0,
    kSucceeded = 1,
    kFailed = 2,
    kMaxValue = kFailed,
  };

  // Interface which manages the ownership, queuing, and sending of pending
  // conversion reports. Owned by |this|.
  class AttributionReporter {
   public:
    virtual ~AttributionReporter() = default;

    // Adds |reports| to a shared queue of reports that need to be sent.
    virtual void AddReportsToQueue(std::vector<AttributionReport> reports) = 0;

    // Called by `ConversionManagerImpl::ClearData()` to prevent outstanding
    // reports from being sent. This is best-effort, as a network request may
    // already have been triggered.
    virtual void RemoveAllReportsFromQueue() = 0;
  };

  // Configures underlying storage to be setup in memory, rather than on
  // disk. This speeds up initialization to avoid timeouts in test environments.
  static void RunInMemoryForTesting();

  static std::unique_ptr<ConversionManagerImpl> CreateForTesting(
      std::unique_ptr<AttributionReporter> reporter,
      std::unique_ptr<AttributionPolicy> policy,
      const base::Clock* clock,
      const base::FilePath& user_data_directory,
      scoped_refptr<storage::SpecialStoragePolicy> special_storage_policy,
      size_t max_sent_reports_to_store) WARN_UNUSED_RESULT;

  ConversionManagerImpl(
      StoragePartitionImpl* storage_partition,
      const base::FilePath& user_data_directory,
      scoped_refptr<storage::SpecialStoragePolicy> special_storage_policy);
  ConversionManagerImpl(const ConversionManagerImpl& other) = delete;
  ConversionManagerImpl& operator=(const ConversionManagerImpl& other) = delete;
  ConversionManagerImpl(ConversionManagerImpl&& other) = delete;
  ConversionManagerImpl& operator=(ConversionManagerImpl&& other) = delete;
  ~ConversionManagerImpl() override;

  // ConversionManager:
  void HandleImpression(StorableSource impression) override;
  void HandleConversion(StorableTrigger conversion) override;
  void GetActiveImpressionsForWebUI(
      base::OnceCallback<void(std::vector<StorableSource>)> callback) override;
  void GetPendingReportsForWebUI(
      base::OnceCallback<void(std::vector<AttributionReport>)> callback,
      base::Time max_report_time) override;
  const AttributionSessionStorage& GetSessionStorage() const override;
  void SendReportsForWebUI(base::OnceClosure done) override;
  const AttributionPolicy& GetAttributionPolicy() const override;
  void ClearData(base::Time delete_begin,
                 base::Time delete_end,
                 base::RepeatingCallback<bool(const url::Origin&)> filter,
                 base::OnceClosure done) override;

 private:
  friend class ConversionManagerImplTest;

  ConversionManagerImpl(
      std::unique_ptr<AttributionReporter> reporter,
      std::unique_ptr<AttributionPolicy> policy,
      const base::Clock* clock,
      const base::FilePath& user_data_directory,
      scoped_refptr<storage::SpecialStoragePolicy> special_storage_policy,
      size_t max_sent_reports_to_store);

  // Retrieves at most |limit| reports from storage whose |report_time| <=
  // |max_report_time|, and calls |handler_function| on them; use a negative
  // number for no limit.
  using ReportsHandlerFunc =
      base::OnceCallback<void(std::vector<AttributionReport>)>;
  void GetAndHandleReports(ReportsHandlerFunc handler_function,
                           base::Time max_report_time,
                           int limit = -1);

  // Get the next set of reports from storage that need to be sent before the
  // next call from |get_and_queue_reports_timer_|. Adds the reports to
  // |reporter|.
  void GetAndQueueReportsForNextInterval();

  // Queue the given |reports| on |reporter_|.
  void QueueReports(std::vector<AttributionReport> reports);

  void HandleReportsSentFromWebUI(base::OnceClosure done,
                                  std::vector<AttributionReport> reports);

  // Notifies storage to delete the given |conversion_id| when its associated
  // report has been sent.
  void OnReportSent(SentReportInfo info);

  void OnReportStored(AttributionStorage::CreateReportResult result);

  // Friend to expose the AttributionStorage for certain tests.
  friend std::vector<AttributionReport> GetConversionsToReportForTesting(
      ConversionManagerImpl* manager,
      base::Time max_report_time);

  // Whether the API is running in debug mode, meaning that there should be
  // no delays or noise added to reports. This is used by end to end tests to
  // verify functionality without mocking out any implementations.
  const bool debug_mode_;

  const base::Clock* clock_;

  // Timer which administers calls to `GetAndQueueReportsForNextInterval()`.
  base::RepeatingTimer get_and_queue_reports_timer_;

  // Handle keeping track of conversion reports to send. Reports are fetched
  // from |storage_| and added to |reporter_| by |get_reports_timer_|.
  std::unique_ptr<AttributionReporter> reporter_;

  base::SequenceBound<AttributionStorage> attribution_storage_;

  AttributionSessionStorage session_storage_;

  // Stores the set of conversion IDs whose reports are being sent by
  // `SendReportsForWebUI()`. Once empty, `send_reports_for_web_ui_callback_` is
  // invoked if non-null.
  base::flat_set<AttributionReport::Id>
      pending_conversion_ids_for_internals_ui_;
  base::OnceClosure send_reports_for_web_ui_callback_;

  // Policy used for controlling API configurations such as reporting and
  // attribution models. Unique ptr so it can be overridden for testing.
  std::unique_ptr<AttributionPolicy> attribution_policy_;

  // Storage policy for the browser context |this| is in. May be nullptr.
  scoped_refptr<storage::SpecialStoragePolicy> special_storage_policy_;

  base::WeakPtrFactory<ConversionManagerImpl> weak_factory_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_ATTRIBUTION_REPORTING_CONVERSION_MANAGER_IMPL_H_
