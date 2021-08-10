// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_CONVERSIONS_CONVERSION_MANAGER_IMPL_H_
#define CONTENT_BROWSER_CONVERSIONS_CONVERSION_MANAGER_IMPL_H_

#include <memory>
#include <vector>

#include "base/callback_forward.h"
#include "base/compiler_specific.h"
#include "base/containers/circular_deque.h"
#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/sequence_bound.h"
#include "base/timer/timer.h"
#include "content/browser/conversions/conversion_manager.h"
#include "content/browser/conversions/sent_report_info.h"
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

class ConversionStorage;
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
  // Interface which manages the ownership, queuing, and sending of pending
  // conversion reports. Owned by |this|.
  class ConversionReporter {
   public:
    virtual ~ConversionReporter() = default;

    // Adds |reports| to a shared queue of reports that need to be sent. Runs
    // |report_sent_callback| for every report that is sent, with the associated
    // |conversion_id| of the report and the time the report was originally
    // supposed to be sent.
    virtual void AddReportsToQueue(
        std::vector<ConversionReport> reports,
        base::RepeatingCallback<void(SentReportInfo)> report_sent_callback) = 0;
  };

  // Configures underlying storage to be setup in memory, rather than on
  // disk. This speeds up initialization to avoid timeouts in test environments.
  static void RunInMemoryForTesting();

  static std::unique_ptr<ConversionManagerImpl> CreateForTesting(
      std::unique_ptr<ConversionReporter> reporter,
      std::unique_ptr<ConversionPolicy> policy,
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
  void HandleImpression(StorableImpression impression) override;
  void HandleConversion(StorableConversion conversion) override;
  void GetActiveImpressionsForWebUI(
      base::OnceCallback<void(std::vector<StorableImpression>)> callback)
      override;
  void GetPendingReportsForWebUI(
      base::OnceCallback<void(std::vector<ConversionReport>)> callback,
      base::Time max_report_time) override;
  const base::circular_deque<SentReportInfo>& GetSentReportsForWebUI()
      const override;
  void SendReportsForWebUI(base::OnceClosure done) override;
  const ConversionPolicy& GetConversionPolicy() const override;
  void ClearData(base::Time delete_begin,
                 base::Time delete_end,
                 base::RepeatingCallback<bool(const url::Origin&)> filter,
                 base::OnceClosure done) override;

 private:
  ConversionManagerImpl(
      std::unique_ptr<ConversionReporter> reporter,
      std::unique_ptr<ConversionPolicy> policy,
      const base::Clock* clock,
      const base::FilePath& user_data_directory,
      scoped_refptr<storage::SpecialStoragePolicy> special_storage_policy,
      size_t max_sent_reports_to_store);

  // Retrieves at most |limit| reports from storage whose |report_time| <=
  // |max_report_time|, and calls |handler_function| on them; use a negative
  // number for no limit.
  using ReportsHandlerFunc =
      base::OnceCallback<void(std::vector<ConversionReport>)>;
  void GetAndHandleReports(ReportsHandlerFunc handler_function,
                           base::Time max_report_time,
                           int limit = -1);

  // Get the next set of reports from storage that need to be sent before the
  // next call from |get_and_queue_reports_timer_|. Adds the reports to
  // |reporter|.
  void GetAndQueueReportsForNextInterval();

  // Queue the given |reports| on |reporter_|.
  void QueueReports(std::vector<ConversionReport> reports);

  void HandleReportsSentFromWebUI(base::OnceClosure done,
                                  std::vector<ConversionReport> reports);

  void MaybeStoreSentReportInfo(SentReportInfo info);

  // Notifies storage to delete the given |conversion_id| when its associated
  // report has been sent.
  void OnReportSent(SentReportInfo info);

  // Similar to OnReportSent, but invokes |reports_sent_barrier| when the
  // report has been removed from storage.
  void OnReportSentFromWebUI(base::OnceClosure reports_sent_barrier,
                             SentReportInfo info);

  // Friend to expose the ConversionStorage for certain tests.
  friend std::vector<ConversionReport> GetConversionsToReportForTesting(
      ConversionManagerImpl* manager,
      base::Time max_report_time);

  // Whether the API is running in debug mode, meaning that there should be
  // no delays or noise added to reports. This is used by end to end tests to
  // verify functionality without mocking out any implementations.
  const bool debug_mode_;

  const base::Clock* clock_;

  // Timer which administers calls to GetAndQueueReportsForNextInterval().
  base::RepeatingTimer get_and_queue_reports_timer_;

  // Handle keeping track of conversion reports to send. Reports are fetched
  // from |storage_| and added to |reporter_| by |get_reports_timer_|.
  std::unique_ptr<ConversionReporter> reporter_;

  base::SequenceBound<ConversionStorage> conversion_storage_;

  // Stores info for the last |max_sent_reports_to_store_| reports sent in this
  // session for display in conversion internals UI.
  base::circular_deque<SentReportInfo> sent_reports_;

  // This is needed to avoid leaking memory.
  const size_t max_sent_reports_to_store_;

  // Policy used for controlling API configurations such as reporting and
  // attribution models. Unique ptr so it can be overridden for testing.
  std::unique_ptr<ConversionPolicy> conversion_policy_;

  // Storage policy for the browser context |this| is in. May be nullptr.
  scoped_refptr<storage::SpecialStoragePolicy> special_storage_policy_;

  base::WeakPtrFactory<ConversionManagerImpl> weak_factory_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_CONVERSIONS_CONVERSION_MANAGER_IMPL_H_
