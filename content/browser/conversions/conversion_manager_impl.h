// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_CONVERSIONS_CONVERSION_MANAGER_IMPL_H_
#define CONTENT_BROWSER_CONVERSIONS_CONVERSION_MANAGER_IMPL_H_

#include <memory>
#include <vector>

#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "content/browser/conversions/conversion_manager.h"
#include "content/browser/conversions/conversion_policy.h"
#include "content/browser/conversions/conversion_storage_context.h"
#include "storage/browser/quota/special_storage_policy.h"

namespace base {

class Clock;
class FilePath;

}  // namespace base

namespace content {

// Frequency we pull ConversionReports from storage and queue them to be
// reported.
extern CONTENT_EXPORT const base::TimeDelta
    kConversionManagerQueueReportsInterval;

class StoragePartition;

// Provides access to the manager owned by the default StoragePartition.
class ConversionManagerProviderImpl : public ConversionManager::Provider {
 public:
  ConversionManagerProviderImpl() = default;
  ConversionManagerProviderImpl(const ConversionManagerProviderImpl& other) =
      delete;
  ConversionManagerProviderImpl& operator=(
      const ConversionManagerProviderImpl& other) = delete;
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
    // |conversion_id| of the report.
    virtual void AddReportsToQueue(
        std::vector<ConversionReport> reports,
        base::RepeatingCallback<void(int64_t)> report_sent_callback) = 0;
  };

  // Configures underlying storage to be setup in memory, rather than on
  // disk. This speeds up initialization to avoid timeouts in test environments.
  static void RunInMemoryForTesting();

  static std::unique_ptr<ConversionManagerImpl> CreateForTesting(
      std::unique_ptr<ConversionReporter> reporter,
      std::unique_ptr<ConversionPolicy> policy,
      const base::Clock* clock,
      const base::FilePath& user_data_directory,
      scoped_refptr<storage::SpecialStoragePolicy> special_storage_policy);

  ConversionManagerImpl(
      StoragePartition* storage_partition,
      const base::FilePath& user_data_directory,
      scoped_refptr<storage::SpecialStoragePolicy> special_storage_policy);
  ConversionManagerImpl(const ConversionManagerImpl& other) = delete;
  ConversionManagerImpl& operator=(const ConversionManagerImpl& other) = delete;
  ~ConversionManagerImpl() override;

  // ConversionManager:
  void HandleImpression(const StorableImpression& impression) override;
  void HandleConversion(const StorableConversion& conversion) override;
  void GetActiveImpressionsForWebUI(
      base::OnceCallback<void(std::vector<StorableImpression>)> callback)
      override;
  void GetReportsForWebUI(
      base::OnceCallback<void(std::vector<ConversionReport>)> callback,
      base::Time max_report_time) override;
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
      scoped_refptr<storage::SpecialStoragePolicy> special_storage_policy);

  // Retrieves reports from storage whose |report_time| <= |max_report_time|,
  // and calls |handler_function| on them.
  using ReportsHandlerFunc =
      base::OnceCallback<void(std::vector<ConversionReport>)>;
  void GetAndHandleReports(ReportsHandlerFunc handler_function,
                           base::Time max_report_time);

  // Get the next set of reports from storage that need to be sent before the
  // next call from |get_and_queue_reports_timer_|. Adds the reports to
  // |reporter|.
  void GetAndQueueReportsForNextInterval();

  // Queue the given |reports| on |reporter_|.
  void QueueReports(std::vector<ConversionReport> reports);

  void HandleReportsExpiredAtStartup(std::vector<ConversionReport> reports);

  void HandleReportsSentFromWebUI(base::OnceClosure done,
                                  std::vector<ConversionReport> reports);

  // Notify storage to delete the given |conversion_id| when its associated
  // report has been sent.
  void OnReportSent(int64_t conversion_id);

  // Similar to OnReportSent, but invokes |reports_sent_barrier| when the
  // report has been removed from storage.
  void OnReportSentFromWebUI(base::OnceClosure reports_sent_barrier,
                             int64_t conversion_id);

  // Friend to expose the ConversionStorageContext for certain tests.
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

  // Cross sequence storage context that is created alongside the manager. The
  // ref count is held for the entire lifetime of |this|, but may outlive
  // |this|. Can be accessed at any point in |this|'s lifetime.
  scoped_refptr<ConversionStorageContext> conversion_storage_context_;

  // Policy used for controlling API configurations such as reporting and
  // attribution models. Unique ptr so it can be overridden for testing.
  std::unique_ptr<ConversionPolicy> conversion_policy_;

  // Storage policy for the browser context |this| is in. May be nullptr.
  scoped_refptr<storage::SpecialStoragePolicy> special_storage_policy_;

  base::WeakPtrFactory<ConversionManagerImpl> weak_factory_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_CONVERSIONS_CONVERSION_MANAGER_IMPL_H_
