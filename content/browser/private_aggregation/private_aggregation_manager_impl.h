// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_PRIVATE_AGGREGATION_PRIVATE_AGGREGATION_MANAGER_IMPL_H_
#define CONTENT_BROWSER_PRIVATE_AGGREGATION_PRIVATE_AGGREGATION_MANAGER_IMPL_H_

#include <memory>

#include "base/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "content/browser/private_aggregation/private_aggregation_budget_key.h"
#include "content/browser/private_aggregation/private_aggregation_budgeter.h"
#include "content/browser/private_aggregation/private_aggregation_manager.h"
#include "content/common/content_export.h"
#include "content/common/private_aggregation_host.mojom.h"
#include "content/public/browser/storage_partition.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"

namespace base {
class FilePath;
class Time;
}

namespace url {
class Origin;
}

namespace content {

class AggregatableReportRequest;
class AggregationService;
class PrivateAggregationHost;
class StoragePartitionImpl;

// UI thread class that manages the lifetime of the other classes,
// coordinates report requests, and interfaces with other directories. Lifetime
// is bound to lifetime of the `StoragePartitionImpl`.
class CONTENT_EXPORT PrivateAggregationManagerImpl
    : public PrivateAggregationManager {
 public:
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
      PrivateAggregationBudgetKey::Api api_for_budgeting,
      mojo::PendingReceiver<mojom::PrivateAggregationHost> pending_receiver)
      override;
  void ClearBudgetData(base::Time delete_begin,
                       base::Time delete_end,
                       StoragePartition::StorageKeyMatcherFunction filter,
                       base::OnceClosure done) override;

 protected:
  // Protected for testing.
  PrivateAggregationManagerImpl(
      std::unique_ptr<PrivateAggregationBudgeter> budgeter,
      std::unique_ptr<PrivateAggregationHost> host,
      StoragePartitionImpl* storage_partition);

  // Virtual for testing.
  virtual AggregationService* GetAggregationService();

  // Called when the `host_` has received and validated a report request.
  void OnReportRequestReceivedFromHost(AggregatableReportRequest report_request,
                                       PrivateAggregationBudgetKey budget_key);

 private:
  // Called when the `budgeter_` has responded to a `ConsumeBudget()` call.
  // Virtual for testing.
  virtual void OnConsumeBudgetReturned(
      AggregatableReportRequest report_request,
      PrivateAggregationBudgetKey::Api api_for_budgeting,
      PrivateAggregationBudgeter::RequestResult request_result);

  std::unique_ptr<PrivateAggregationBudgeter> budgeter_;
  std::unique_ptr<PrivateAggregationHost> host_;

  // Can be nullptr in unit tests.
  raw_ptr<StoragePartitionImpl> storage_partition_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_PRIVATE_AGGREGATION_PRIVATE_AGGREGATION_MANAGER_IMPL_H_
