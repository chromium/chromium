// Copyright 2022 The Chromium Authors. All rights reserved.
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

// Class to manage bindings for the Private Aggregation API. Expected to be used
// for a context managed by `ContextRecycler`. Throws exceptions when invalid
// arguments are detected.
class CONTENT_EXPORT PrivateAggregationBindings : public Bindings {
 public:
  explicit PrivateAggregationBindings(AuctionV8Helper* v8_helper);
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
  static void EnableDebugMode(const v8::FunctionCallbackInfo<v8::Value>& args);

  const raw_ptr<AuctionV8Helper> v8_helper_;

  // Defaults to debug mode being disabled.
  content::mojom::DebugModeDetails debug_mode_details_;

  std::vector<content::mojom::AggregatableReportHistogramContributionPtr>
      private_aggregation_contributions_;
};

}  // namespace auction_worklet

#endif  // CONTENT_SERVICES_AUCTION_WORKLET_PRIVATE_AGGREGATION_BINDINGS_H_
