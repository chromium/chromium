// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SERVICES_AUCTION_WORKLET_PRIVATE_AGGREGATION_BINDINGS_H_
#define CONTENT_SERVICES_AUCTION_WORKLET_PRIVATE_AGGREGATION_BINDINGS_H_

#include <vector>

#include "base/memory/raw_ptr.h"
#include "content/common/content_export.h"
#include "content/common/private_aggregation_host.mojom.h"
#include "content/services/auction_worklet/context_recycler.h"
#include "content/services/auction_worklet/public/mojom/private_aggregation_request.mojom-forward.h"
#include "v8/include/v8-forward.h"

namespace auction_worklet {

class AuctionV8Helper;

// Reserved event types for aggregatable report's for-event contribution.
CONTENT_EXPORT extern const char kReservedAlways[];
CONTENT_EXPORT extern const char kReservedWin[];
CONTENT_EXPORT extern const char kReservedLoss[];

// Class to manage bindings for the Private Aggregation API. Expected to be used
// for a context managed by `ContextRecycler`. Throws exceptions when invalid
// arguments are detected.
class CONTENT_EXPORT PrivateAggregationBindings : public Bindings {
 public:
  explicit PrivateAggregationBindings(
      AuctionV8Helper* v8_helper,
      bool private_aggregation_permissions_policy_allowed);
  PrivateAggregationBindings(const PrivateAggregationBindings&) = delete;
  PrivateAggregationBindings& operator=(const PrivateAggregationBindings&) =
      delete;
  ~PrivateAggregationBindings() override;

  // Add privateAggregation object to `global_template`. `this` must outlive the
  // template.
  void FillInGlobalTemplate(
      v8::Local<v8::ObjectTemplate> global_template) override;
  void Reset() override;

  std::vector<auction_worklet::mojom::PrivateAggregationRequestPtr>
  TakePrivateAggregationRequests();

 private:
  static void SendHistogramReport(
      const v8::FunctionCallbackInfo<v8::Value>& args);
  static void ReportContributionForEvent(
      const v8::FunctionCallbackInfo<v8::Value>& args);
  static void EnableDebugMode(const v8::FunctionCallbackInfo<v8::Value>& args);

  const raw_ptr<AuctionV8Helper> v8_helper_;

  bool private_aggregation_permissions_policy_allowed_;

  // Defaults to debug mode being disabled.
  content::mojom::DebugModeDetails debug_mode_details_;

  // Contributions from calling Private Aggregation APIs.
  std::vector<auction_worklet::mojom::AggregatableReportContributionPtr>
      private_aggregation_contributions_;
};

}  // namespace auction_worklet

#endif  // CONTENT_SERVICES_AUCTION_WORKLET_PRIVATE_AGGREGATION_BINDINGS_H_
