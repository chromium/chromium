// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_PRIVATE_AGGREGATION_PRIVATE_AGGREGATION_MANAGER_H_
#define CONTENT_BROWSER_PRIVATE_AGGREGATION_PRIVATE_AGGREGATION_MANAGER_H_

#include "base/functional/callback_forward.h"
#include "content/browser/private_aggregation/private_aggregation_budget_key.h"
#include "content/common/content_export.h"
#include "content/common/private_aggregation_host.mojom-forward.h"
#include "content/public/browser/storage_partition.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"

namespace base {
class Time;
}

namespace url {
class Origin;
}

namespace content {

class BrowserContext;

// Interface that mediates data flow between the Private Aggregation API
// component and other APIs using it.
class CONTENT_EXPORT PrivateAggregationManager {
 public:
  virtual ~PrivateAggregationManager() = default;

  static PrivateAggregationManager* GetManager(BrowserContext& browser_context);

  // Binds a new pending receiver for a worklet, allowing messages to be sent
  // and processed. However, the receiver is not bound if the `worklet_origin`
  // is not potentially trustworthy. The return value indicates whether the
  // receiver was accepted.
  [[nodiscard]] virtual bool BindNewReceiver(
      url::Origin worklet_origin,
      url::Origin top_frame_origin,
      PrivateAggregationBudgetKey::Api api_for_budgeting,
      mojo::PendingReceiver<mojom::PrivateAggregationHost>
          pending_receiver) = 0;

  // Deletes all data in storage for any budgets that could have been set
  // between `delete_begin` and `delete_end` time (inclusive). Note that the
  // discrete time windows used in the budgeter may lead to more data being
  // deleted than strictly necessary. Null times are treated as unbounded lower
  // or upper range. If `!filter.is_null()`, budget keys with an origin that
  // does *not* match the `filter` are retained (i.e. not cleared).
  virtual void ClearBudgetData(
      base::Time delete_begin,
      base::Time delete_end,
      StoragePartition::StorageKeyMatcherFunction filter,
      base::OnceClosure done) = 0;
};

}  // namespace content

#endif  // CONTENT_BROWSER_PRIVATE_AGGREGATION_PRIVATE_AGGREGATION_MANAGER_H_
