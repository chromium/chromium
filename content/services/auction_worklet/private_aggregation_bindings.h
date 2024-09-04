// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SERVICES_AUCTION_WORKLET_PRIVATE_AGGREGATION_BINDINGS_H_
#define CONTENT_SERVICES_AUCTION_WORKLET_PRIVATE_AGGREGATION_BINDINGS_H_

#include <vector>

#include "base/memory/raw_ptr.h"
#include "content/common/content_export.h"
#include "content/services/auction_worklet/context_recycler.h"
#include "content/services/auction_worklet/public/mojom/private_aggregation_request.mojom-forward.h"
#include "third_party/blink/public/mojom/private_aggregation/private_aggregation_host.mojom.h"
#include "v8/include/v8-forward.h"

namespace auction_worklet {

class AuctionV8Helper;
class AuctionV8Logger;

// Class to manage bindings for the Private Aggregation API. Expected to be used
// for a context managed by `ContextRecycler`. Throws exceptions when invalid
// arguments are detected.
class CONTENT_EXPORT PrivateAggregationBindings : public Bindings {
 public:
  explicit PrivateAggregationBindings(
      AuctionV8Helper* v8_helper,
      AuctionV8Logger* v8_logger,
      bool private_aggregation_permissions_policy_allowed,
      bool reserved_once_allowed);
  PrivateAggregationBindings(const PrivateAggregationBindings&) = delete;
  PrivateAggregationBindings& operator=(const PrivateAggregationBindings&) =
      delete;
  ~PrivateAggregationBindings() override;

  // Add privateAggregation object to global context. `this` must outlive the
  // context.
  void AttachToContext(v8::Local<v8::Context> context) override;
  void Reset() override;

  std::vector<auction_worklet::mojom::PrivateAggregationRequestPtr>
  TakePrivateAggregationRequests();

 private:
  static void ContributeToHistogram(
      const v8::FunctionCallbackInfo<v8::Value>& args);
  static void ContributeToHistogramOnEvent(
      const v8::FunctionCallbackInfo<v8::Value>& args);
  static void EnableDebugMode(const v8::FunctionCallbackInfo<v8::Value>& args);

  const raw_ptr<AuctionV8Helper> v8_helper_;
  const raw_ptr<AuctionV8Logger> v8_logger_;

  const bool private_aggregation_permissions_policy_allowed_;
  const bool enforce_permission_policy_for_on_event_;
  const bool additional_extensions_allowed_;

  // This is true if the binding is used for functions where reserved.once is
  // permitted; it's irrelevant if reserved.once is turned off by
  // `additional_extensions_allowed_` being false.
  const bool reserved_once_allowed_;

  // Defaults to debug mode being disabled.
  blink::mojom::DebugModeDetails debug_mode_details_;

  // Contributions from calling Private Aggregation APIs.
  std::vector<auction_worklet::mojom::AggregatableReportContributionPtr>
      private_aggregation_contributions_;
};

}  // namespace auction_worklet

#endif  // CONTENT_SERVICES_AUCTION_WORKLET_PRIVATE_AGGREGATION_BINDINGS_H_
