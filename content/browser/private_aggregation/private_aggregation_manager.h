// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_PRIVATE_AGGREGATION_PRIVATE_AGGREGATION_MANAGER_H_
#define CONTENT_BROWSER_PRIVATE_AGGREGATION_PRIVATE_AGGREGATION_MANAGER_H_

#include <stddef.h>
#include <stdint.h>

#include <optional>
#include <string>

#include "base/functional/callback_forward.h"
#include "base/numerics/safe_conversions.h"
#include "base/time/time.h"
#include "content/browser/private_aggregation/private_aggregation_caller_api.h"
#include "content/common/content_export.h"
#include "content/public/browser/storage_partition.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "third_party/blink/public/mojom/private_aggregation/private_aggregation_host.mojom-forward.h"

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

  // Attempts to bind a new pending receiver for a worklet, allowing messages to
  // be sent and processed. The return value indicates whether the receiver was
  // accepted. Virtual for testing.
  //
  // The receiver will only be bound when all of these conditions are met:
  // * `worklet_origin` is potentially trustworthy.
  // * `context_id`, if set, is not too long.
  // * `aggregation_coordinator_origin`, if set, is on the allowlist.
  // * `filtering_id_max_bytes` is positive and no greater than
  //   `AggregationServicePayloadContents::kMaximumFilteringIdMaxBytes`.
  // * `max_contributions`, if set, is positive.
  // * `timeout` is set iff a report should be sent deterministically, i.e.
  //   `PrivateAggregationManager::ShouldSendReportDeterministically(caller_api,
  //   context_id, filtering_id_max_bytes, max_contributions)` is true.
  //
  // When `timeout` is set and developer mode is not enabled, this host will
  // send a report after the given duration of time has passed, regardless of
  // when the receiver is actually disconnected. It is a fatal error for
  // `timeout` to be zero or negative.
  [[nodiscard]] virtual bool BindNewReceiver(
      url::Origin worklet_origin,
      url::Origin top_frame_origin,
      PrivateAggregationCallerApi caller_api,
      std::optional<std::string> context_id,
      std::optional<base::TimeDelta> timeout,
      std::optional<url::Origin> aggregation_coordinator_origin,
      size_t filtering_id_max_bytes,
      std::optional<size_t> max_contributions,
      mojo::PendingReceiver<blink::mojom::PrivateAggregationHost>
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

  // Returns whether debug mode is allowed for a context with the given
  // parameters. If disallowed, any debug mode details specified over the
  // PrivateAggregationHost mojo pipe will be ignored.
  virtual bool IsDebugModeAllowed(const url::Origin& top_frame_origin,
                                  const url::Origin& reporting_origin) = 0;

  // Returns true iff an isolated context with the given parameters requires
  // deterministic report counts, i.e. sending a null report when a real report
  // has no approved contributions. Such contexts also qualify for "reduced
  // delay", meaning they may be sent after a fixed duration of time relative to
  // an event outside of the isolated context.
  [[nodiscard]] static bool ShouldSendReportDeterministically(
      PrivateAggregationCallerApi caller_api,
      const std::optional<std::string>& context_id,
      base::StrictNumeric<size_t> filtering_id_max_bytes,
      std::optional<size_t> requested_max_contributions);
};

}  // namespace content

#endif  // CONTENT_BROWSER_PRIVATE_AGGREGATION_PRIVATE_AGGREGATION_MANAGER_H_
