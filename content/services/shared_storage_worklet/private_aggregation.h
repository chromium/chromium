// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SERVICES_SHARED_STORAGE_WORKLET_PRIVATE_AGGREGATION_H_
#define CONTENT_SERVICES_SHARED_STORAGE_WORKLET_PRIVATE_AGGREGATION_H_

#include <vector>

#include "base/memory/raw_ref.h"
#include "gin/object_template_builder.h"
#include "gin/wrappable.h"
#include "third_party/blink/public/mojom/private_aggregation/aggregatable_report.mojom-forward.h"
#include "third_party/blink/public/mojom/private_aggregation/private_aggregation_host.mojom.h"
#include "third_party/blink/public/mojom/shared_storage/shared_storage_worklet_service.mojom.h"

namespace gin {
class Arguments;
}  // namespace gin

namespace shared_storage_worklet {

class PrivateAggregation final : public gin::Wrappable<PrivateAggregation> {
 public:
  PrivateAggregation(
      blink::mojom::SharedStorageWorkletServiceClient& client,
      bool private_aggregation_permissions_policy_allowed,
      blink::mojom::PrivateAggregationHost& private_aggregation_host);
  ~PrivateAggregation() override;

  static gin::WrapperInfo kWrapperInfo;

  gin::ObjectTemplateBuilder GetObjectTemplateBuilder(
      v8::Isolate* isolate) override;

  const char* GetTypeName() override;

  // Flushes any Private Aggregation contributions and resets the local state.
  // Should be called whenever an operation finishes running.
  void FlushAndReset();

 private:
  void SendHistogramReport(gin::Arguments* args);
  void EnableDebugMode(gin::Arguments* args);

  void EnsureUseCountersAreRecorded();

  raw_ref<blink::mojom::SharedStorageWorkletServiceClient> client_;

  bool private_aggregation_permissions_policy_allowed_;

  raw_ref<blink::mojom::PrivateAggregationHost> private_aggregation_host_;

  bool has_recorded_use_counters_ = false;

  // Defaults to debug mode being disabled.
  blink::mojom::DebugModeDetails debug_mode_details_;

  std::vector<blink::mojom::AggregatableReportHistogramContributionPtr>
      private_aggregation_contributions_;
};

}  // namespace shared_storage_worklet

#endif  // CONTENT_SERVICES_SHARED_STORAGE_WORKLET_PRIVATE_AGGREGATION_H_
