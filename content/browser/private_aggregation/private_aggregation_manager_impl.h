// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_PRIVATE_AGGREGATION_PRIVATE_AGGREGATION_MANAGER_IMPL_H_
#define CONTENT_BROWSER_PRIVATE_AGGREGATION_PRIVATE_AGGREGATION_MANAGER_IMPL_H_

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <memory>
#include <optional>
#include <string>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "content/browser/private_aggregation/private_aggregation_budget_key.h"
#include "content/browser/private_aggregation/private_aggregation_budgeter.h"
#include "content/browser/private_aggregation/private_aggregation_caller_api.h"
#include "content/browser/private_aggregation/private_aggregation_host.h"
#include "content/browser/private_aggregation/private_aggregation_manager.h"
#include "content/browser/private_aggregation/private_aggregation_pending_contributions.h"
#include "content/common/content_export.h"
#include "content/public/browser/private_aggregation_data_model.h"
#include "content/public/browser/storage_partition.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "third_party/blink/public/mojom/private_aggregation/private_aggregation_host.mojom.h"

namespace base {
class FilePath;
}

namespace url {
class Origin;
}

namespace content {

class AggregationService;
class PrivateAggregationHost;
class StoragePartitionImpl;

// UI thread class that manages the lifetime of the other classes,
// coordinates report requests, and interfaces with other directories. Lifetime
// is bound to lifetime of the `StoragePartitionImpl`.
class CONTENT_EXPORT PrivateAggregationManagerImpl
    : public PrivateAggregationManager,
      public PrivateAggregationDataModel {
 public:
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum class RequestResult {
    kSentWithContributions = 0,
    kSentWithoutContributions = 1,
    kSentButContributionsClearedDueToBudgetDenial = 2,
    kNotSent = 3,
    kMaxValue = kNotSent,
  };

  // `storage_partition` must outlive this.
  PrivateAggregationManagerImpl(bool exclusively_run_in_memory,
                                const base::FilePath& user_data_directory,
                                StoragePartitionImpl* storage_partition);
  PrivateAggregationManagerImpl(const PrivateAggregationManagerImpl&) = delete;
  PrivateAggregationManagerImpl& operator=(
      const PrivateAggregationManagerImpl&) = delete;
  ~PrivateAggregationManagerImpl() override;

  // PrivateAggregationManager:
  [[nodiscard]] bool BindNewReceiver(
      url::Origin worklet_origin,
      url::Origin top_frame_origin,
      PrivateAggregationCallerApi caller_api,
      std::optional<std::string> context_id,
      std::optional<base::TimeDelta> timeout,
      std::optional<url::Origin> aggregation_coordinator_origin,
      size_t filtering_id_max_bytes,
      std::optional<size_t> max_contributions,
      mojo::PendingReceiver<blink::mojom::PrivateAggregationHost>
          pending_receiver) override;
  void ClearBudgetData(base::Time delete_begin,
                       base::Time delete_end,
                       StoragePartition::StorageKeyMatcherFunction filter,
                       base::OnceClosure done) override;
  bool IsDebugModeAllowed(const url::Origin& top_frame_origin,
                          const url::Origin& reporting_origin) override;

  // PrivateAggregationDataModel:
  void GetAllDataKeys(
      base::OnceCallback<void(std::set<DataKey>)> callback) override;
  void RemovePendingDataKey(const DataKey& data_key,
                            base::OnceClosure callback) override;

 protected:
  // Protected for testing.
  PrivateAggregationManagerImpl(
      std::unique_ptr<PrivateAggregationBudgeter> budgeter,
      std::unique_ptr<PrivateAggregationHost> host,
      StoragePartitionImpl* storage_partition);

  // Virtual for testing.
  virtual AggregationService* GetAggregationService();

  // Called when the `host_` has received and validated the information needed
  // for report generation from a completed mojo pipe.
  void OnReportRequestDetailsReceivedFromHost(
      PrivateAggregationHost::ReportRequestGenerator report_request_generator,
      PrivateAggregationPendingContributions::Wrapper contributions,
      PrivateAggregationBudgetKey budget_key,
      PrivateAggregationHost::NullReportBehavior null_report_behavior);

 private:
  struct InProgressBudgetRequest;
  using BudgetRequestId = base::StrongAlias<class BudgetRequestIdTag, int64_t>;

  // Called when the `budgeter_` has responded to a `ConsumeBudget()` call.
  // Virtual for testing.
  virtual void OnConsumeBudgetReturned(
      PrivateAggregationHost::ReportRequestGenerator report_request_generator,
      std::vector<blink::mojom::AggregatableReportHistogramContribution>
          contributions,
      PrivateAggregationCallerApi caller_api,
      PrivateAggregationHost::NullReportBehavior null_report_behavior,
      PrivateAggregationBudgeter::RequestResult request_result);

  void OnTestBudgetAndLockReturned(
      BudgetRequestId budget_request_id,
      PrivateAggregationBudgeter::InspectBudgetCallResult result);

  // TODO(crbug.com/381788013): Remove `WithLock` naming once
  // `kPrivateAggregationApiErrorReporting` is fully launched and the flag is
  // removed.
  void OnConsumeBudgetWithLockReturned(
      BudgetRequestId budget_request_id,
      PrivateAggregationBudgeter::BudgetQueryResult result);

  virtual void OnContributionsFinalized(
      PrivateAggregationHost::ReportRequestGenerator report_request_generator,
      std::vector<blink::mojom::AggregatableReportHistogramContribution>
          contributions,
      PrivateAggregationCallerApi caller_api);

  virtual void OnBudgeterGetAllDataKeysReturned(
      base::OnceCallback<void(std::set<DataKey>)> callback,
      std::set<DataKey> all_keys);

  std::unique_ptr<PrivateAggregationBudgeter> budgeter_;
  std::unique_ptr<PrivateAggregationHost> host_;

  // Used to track associated information for requests to the `budgeter_` that
  // have not had their callbacks called yet. Only populated if
  // `kPrivateAggregationApiErrorReporting` is enabled.
  std::map<BudgetRequestId, InProgressBudgetRequest>
      in_progress_budget_requests_;

  // Used to vend keys for `in_progress_budget_requests_`. Only used if
  // `kPrivateAggregationApiErrorReporting` is enabled.
  int64_t num_requests_processed_ = 0;

  // Can be nullptr in unit tests.
  raw_ptr<StoragePartitionImpl> storage_partition_;

  base::WeakPtrFactory<PrivateAggregationManagerImpl> weak_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_PRIVATE_AGGREGATION_PRIVATE_AGGREGATION_MANAGER_IMPL_H_
