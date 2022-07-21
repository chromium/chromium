// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_PRIVATE_AGGREGATION_PRIVATE_AGGREGATION_MANAGER_IMPL_H_
#define CONTENT_BROWSER_PRIVATE_AGGREGATION_PRIVATE_AGGREGATION_MANAGER_IMPL_H_

#include <memory>

#include "content/browser/private_aggregation/private_aggregation_budget_key.h"
#include "content/browser/private_aggregation/private_aggregation_manager.h"
#include "content/common/content_export.h"
#include "content/common/private_aggregation_host.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"

namespace base {
class FilePath;
}

namespace url {
class Origin;
}

namespace content {

class AggregatableReportRequest;
class PrivateAggregationBudgeter;
class PrivateAggregationHost;

// UI thread class that manages the lifetime of the other classes,
// coordinates report requests, and interfaces with other directories. Lifetime
// is bound to lifetime of the `StoragePartitionImpl`.
// TODO(crbug.com/1323325): Integrate with aggregation service.
class CONTENT_EXPORT PrivateAggregationManagerImpl
    : public PrivateAggregationManager {
 public:
  PrivateAggregationManagerImpl(bool exclusively_run_in_memory,
                                const base::FilePath& user_data_directory);
  PrivateAggregationManagerImpl(const PrivateAggregationManagerImpl&) = delete;
  PrivateAggregationManagerImpl& operator=(
      const PrivateAggregationManagerImpl&) = delete;
  ~PrivateAggregationManagerImpl() override;

  // PrivateAggregationManager:
  [[nodiscard]] bool BindNewReceiver(
      url::Origin worklet_origin,
      PrivateAggregationBudgetKey::Api api_for_budgeting,
      mojo::PendingReceiver<mojom::PrivateAggregationHost> pending_receiver)
      override;

 protected:
  // Protected for testing
  PrivateAggregationManagerImpl(
      std::unique_ptr<PrivateAggregationBudgeter> budgeter,
      std::unique_ptr<PrivateAggregationHost> host);

  // Called when the `host_` has received and validated a report request.
  void OnReportRequestReceivedFromHost(AggregatableReportRequest report_request,
                                       PrivateAggregationBudgetKey budget_key);

  // Called when the `budgeter_` has responded to a `ConsumeBudget()` call.
  // Virtual for testing.
  virtual void OnConsumeBudgetReturned(AggregatableReportRequest report_request,
                                       bool was_budget_use_approved);

 private:
  std::unique_ptr<PrivateAggregationBudgeter> budgeter_;
  std::unique_ptr<PrivateAggregationHost> host_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_PRIVATE_AGGREGATION_PRIVATE_AGGREGATION_MANAGER_IMPL_H_
