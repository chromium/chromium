// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_PRIVATE_AGGREGATION_PRIVATE_AGGREGATION_MANAGER_H_
#define CONTENT_BROWSER_PRIVATE_AGGREGATION_PRIVATE_AGGREGATION_MANAGER_H_

#include "content/browser/private_aggregation/private_aggregation_budget_key.h"
#include "content/common/private_aggregation_host.mojom-forward.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"

namespace url {
class Origin;
}

namespace content {

class BrowserContext;

// Interface that mediates data flow between the Private Aggregation API
// component and other APIs using it.
class PrivateAggregationManager {
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
};

}  // namespace content

#endif  // CONTENT_BROWSER_PRIVATE_AGGREGATION_PRIVATE_AGGREGATION_MANAGER_H_
