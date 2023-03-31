// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/services/shared_storage_worklet/private_aggregation.h"

#include <stdint.h>

#include <map>
#include <string>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/containers/contains.h"
#include "content/services/shared_storage_worklet/shared_storage_worklet_global_scope.h"
#include "content/services/worklet_utils/private_aggregation_utils.h"
#include "gin/arguments.h"
#include "third_party/blink/public/mojom/private_aggregation/aggregatable_report.mojom.h"
#include "third_party/blink/public/mojom/private_aggregation/private_aggregation_host.mojom.h"
#include "third_party/blink/public/mojom/use_counter/metrics/web_feature.mojom-shared.h"
#include "v8/include/v8-isolate.h"
#include "v8/include/v8-primitive.h"

namespace shared_storage_worklet {

struct PrivateAggregation::OperationState {
  // Defaults to debug mode being disabled.
  blink::mojom::DebugModeDetails debug_mode_details;

  // Pending contributions
  std::vector<blink::mojom::AggregatableReportHistogramContributionPtr>
      private_aggregation_contributions;
};

PrivateAggregation::PrivateAggregation(
    blink::mojom::SharedStorageWorkletServiceClient& client,
    bool private_aggregation_permissions_policy_allowed,
    blink::mojom::PrivateAggregationHost& private_aggregation_host,
    SharedStorageWorkletGlobalScope& global_scope)
    : client_(client),
      private_aggregation_permissions_policy_allowed_(
          private_aggregation_permissions_policy_allowed),
      private_aggregation_host_(private_aggregation_host),
      global_scope_(global_scope) {}

PrivateAggregation::~PrivateAggregation() {
  CHECK(operation_states_.empty());
}

gin::WrapperInfo PrivateAggregation::kWrapperInfo = {gin::kEmbedderNativeGin};

gin::ObjectTemplateBuilder PrivateAggregation::GetObjectTemplateBuilder(
    v8::Isolate* isolate) {
  return Wrappable<PrivateAggregation>::GetObjectTemplateBuilder(isolate)
      .SetMethod("sendHistogramReport",
                 &PrivateAggregation::SendHistogramReport)
      .SetMethod("enableDebugMode", &PrivateAggregation::EnableDebugMode);
}

const char* PrivateAggregation::GetTypeName() {
  return "PrivateAggregation";
}

void PrivateAggregation::OnOperationStarted(int64_t operation_id) {
  CHECK(!base::Contains(operation_states_, operation_id));
  operation_states_[operation_id];
}

void PrivateAggregation::OnOperationFinished(int64_t operation_id) {
  CHECK(base::Contains(operation_states_, operation_id));
  OperationState& operation_state = operation_states_[operation_id];

  if (!operation_state.private_aggregation_contributions.empty()) {
    private_aggregation_host_->SendHistogramReport(
        std::move(operation_state.private_aggregation_contributions),
        // TODO(alexmt): consider allowing this to be set
        blink::mojom::AggregationServiceMode::kDefault,
        operation_state.debug_mode_details.Clone());
  }

  operation_states_.erase(operation_id);
}

void PrivateAggregation::SendHistogramReport(gin::Arguments* args) {
  EnsureUseCountersAreRecorded();

  blink::mojom::AggregatableReportHistogramContributionPtr contribution =
      worklet_utils::ParseSendHistogramReportArguments(
          *args, private_aggregation_permissions_policy_allowed_);
  if (contribution.is_null()) {
    // Indicates an exception was thrown.
    return;
  }

  int64_t operation_id = global_scope_->GetCurrentOperationId();
  CHECK(base::Contains(operation_states_, operation_id));
  OperationState& operation_state = operation_states_[operation_id];

  operation_state.private_aggregation_contributions.push_back(
      std::move(contribution));
}

void PrivateAggregation::EnableDebugMode(gin::Arguments* args) {
  EnsureUseCountersAreRecorded();

  int64_t operation_id = global_scope_->GetCurrentOperationId();
  CHECK(base::Contains(operation_states_, operation_id));
  OperationState& operation_state = operation_states_[operation_id];

  worklet_utils::ParseAndApplyEnableDebugModeArguments(
      *args, private_aggregation_permissions_policy_allowed_,
      operation_state.debug_mode_details);
}

void PrivateAggregation::EnsureUseCountersAreRecorded() {
  if (!has_recorded_use_counters_) {
    has_recorded_use_counters_ = true;
    client_->RecordUseCounters(
        {blink::mojom::WebFeature::kPrivateAggregationApiAll,
         blink::mojom::WebFeature::kPrivateAggregationApiSharedStorage});
  }
}

}  // namespace shared_storage_worklet
