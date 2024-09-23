// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/shared_storage/private_aggregation.h"

#include <stdint.h>

#include <bit>
#include <iterator>
#include <memory>
#include <optional>
#include <utility>

#include "base/check.h"
#include "base/feature_list.h"
#include "base/ranges/algorithm.h"
#include "third_party/abseil-cpp/absl/numeric/int128.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/shared_storage/shared_storage_worklet_service.mojom-blink.h"
#include "third_party/blink/public/mojom/use_counter/metrics/web_feature.mojom-blink.h"
#include "third_party/blink/public/platform/cross_variant_mojo_util.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_throw_dom_exception.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_binding_for_modules.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_private_aggregation_debug_mode_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_private_aggregation_histogram_contribution.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/modules/shared_storage/shared_storage_worklet_global_scope.h"
#include "third_party/blink/renderer/modules/shared_storage/util.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/context_lifecycle_notifier.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/heap/visitor.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

namespace {

constexpr char kPermissionsPolicyErrorMessage[] =
    "The \"private-aggregation\" Permissions Policy denied the method on "
    "privateAggregation";

constexpr size_t kBitsPerByte = 8;

}  // namespace

PrivateAggregation::PrivateAggregation(
    SharedStorageWorkletGlobalScope* global_scope)
    : global_scope_(global_scope) {}

PrivateAggregation::~PrivateAggregation() = default;

void PrivateAggregation::Trace(Visitor* visitor) const {
  visitor->Trace(global_scope_);
  visitor->Trace(operation_states_);
  ScriptWrappable::Trace(visitor);
}

// TODO(alexmt): Consider merging parsing logic with FLEDGE worklet.
void PrivateAggregation::contributeToHistogram(
    ScriptState* script_state,
    const PrivateAggregationHistogramContribution* contribution,
    ExceptionState& exception_state) {
  if (!CheckBrowsingContextIsValid(*script_state, exception_state)) {
    return;
  }

  ExecutionContext* execution_context = ExecutionContext::From(script_state);
  CHECK(execution_context->IsSharedStorageWorkletGlobalScope());

  EnsureGeneralUseCountersAreRecorded();

  if (!global_scope_->private_aggregation_permissions_policy_allowed()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidAccessError,
                                      kPermissionsPolicyErrorMessage);
    return;
  }

  // TODO(alexmt): Align error types with Protected Audience implementation.
  std::optional<absl::uint128> bucket = contribution->bucket().ToUInt128();
  if (!bucket) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kDataError,
        "contribution['bucket'] is negative or does not fit in 128 bits");
    return;
  }

  int32_t value = contribution->value();
  if (value < 0) {
    exception_state.ThrowDOMException(DOMExceptionCode::kDataError,
                                      "contribution['value'] is negative");
    return;
  }

  std::optional<uint64_t> filtering_id;
  if (contribution->hasFilteringId() &&
      base::FeatureList::IsEnabled(
          features::kPrivateAggregationApiFilteringIds)) {
    EnsureFilteringIdUseCounterIsRecorded();
    std::optional<absl::uint128> filtering_id_128 =
        contribution->filteringId().ToUInt128();
    if (!filtering_id_128 || absl::Uint128High64(*filtering_id_128) != 0) {
      exception_state.ThrowDOMException(
          DOMExceptionCode::kDataError,
          "contribution['filteringId'] is negative or does not fit in byte "
          "size");
      return;
    }
    filtering_id = absl::Uint128Low64(*filtering_id_128);

    int64_t operation_id = global_scope_->GetCurrentOperationId();
    CHECK(base::Contains(operation_states_, operation_id));
    OperationState* operation_state = operation_states_.at(operation_id);

    if (static_cast<size_t>(std::bit_width(*filtering_id)) >
        kBitsPerByte * operation_state->filtering_id_max_bytes) {
      exception_state.ThrowDOMException(
          DOMExceptionCode::kDataError,
          "contribution['filteringId'] is negative or does not fit in byte "
          "size");
      return;
    }
  }

  // TODO(crbug.com/330744610): Allow filtering ID to be set.
  Vector<mojom::blink::AggregatableReportHistogramContributionPtr>
      mojom_contribution_vector;
  mojom_contribution_vector.push_back(
      mojom::blink::AggregatableReportHistogramContribution::New(
          bucket.value(), value, filtering_id));

  int64_t operation_id = global_scope_->GetCurrentOperationId();
  CHECK(operation_states_.Contains(operation_id));
  OperationState* operation_state = operation_states_.at(operation_id);

  operation_state->private_aggregation_host->ContributeToHistogram(
      std::move(mojom_contribution_vector));
}

void PrivateAggregation::enableDebugMode(ScriptState* script_state,
                                         ExceptionState& exception_state) {
  enableDebugMode(script_state, /*options=*/nullptr, exception_state);
}

void PrivateAggregation::enableDebugMode(
    ScriptState* script_state,
    const PrivateAggregationDebugModeOptions* options,
    ExceptionState& exception_state) {
  if (!CheckBrowsingContextIsValid(*script_state, exception_state)) {
    return;
  }

  ExecutionContext* execution_context = ExecutionContext::From(script_state);
  CHECK(execution_context->IsSharedStorageWorkletGlobalScope());

  EnsureGeneralUseCountersAreRecorded();
  EnsureEnableDebugModeUseCounterIsRecorded();

  if (!global_scope_->private_aggregation_permissions_policy_allowed()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidAccessError,
                                      kPermissionsPolicyErrorMessage);
    return;
  }

  int64_t operation_id = global_scope_->GetCurrentOperationId();
  CHECK(base::Contains(operation_states_, operation_id));
  OperationState* operation_state = operation_states_.at(operation_id);

  if (operation_state->enable_debug_mode_called) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kDataError,
        "enableDebugMode may be called at most once");
    return;
  }
  operation_state->enable_debug_mode_called = true;

  mojom::blink::DebugKeyPtr debug_key;

  // If `options` is not provided, no debug key is set.
  if (options) {
    std::optional<absl::uint128> maybe_debug_key =
        options->debugKey().ToUInt128();

    if (!maybe_debug_key || absl::Uint128High64(maybe_debug_key.value()) != 0) {
      exception_state.ThrowDOMException(
          DOMExceptionCode::kDataError,
          "options['debugKey'] is negative or does not fit in 64 bits");
      return;
    }

    debug_key = mojom::blink::DebugKey::New(
        absl::Uint128Low64(maybe_debug_key.value()));
  }

  operation_state->private_aggregation_host->EnableDebugMode(
      std::move(debug_key));
}

void PrivateAggregation::OnOperationStarted(
    int64_t operation_id,
    mojom::blink::PrivateAggregationOperationDetailsPtr pa_operation_details) {
  CHECK(!operation_states_.Contains(operation_id));
  auto map_it = operation_states_.insert(
      operation_id,
      MakeGarbageCollected<OperationState>(
          global_scope_, pa_operation_details->filtering_id_max_bytes));
  map_it.stored_value->value->private_aggregation_host.Bind(
      std::move(pa_operation_details->pa_host),
      global_scope_->GetTaskRunner(blink::TaskType::kMiscPlatformAPI));
}

void PrivateAggregation::OnOperationFinished(int64_t operation_id) {
  CHECK(operation_states_.Contains(operation_id));
  operation_states_.at(operation_id)->private_aggregation_host.reset();
  operation_states_.erase(operation_id);
}

void PrivateAggregation::OnWorkletDestroyed() {
  // Ensure any unfinished operations are properly handled.
  Vector<int64_t> remaining_operation_ids;
  remaining_operation_ids.reserve(operation_states_.size());
  base::ranges::transform(operation_states_,
                          std::back_inserter(remaining_operation_ids),
                          [](auto& elem) { return elem.key; });

  base::ranges::for_each(remaining_operation_ids, [this](int64_t operation_id) {
    OnOperationFinished(operation_id);
  });

  CHECK(operation_states_.empty());
}

void PrivateAggregation::EnsureGeneralUseCountersAreRecorded() {
  if (!has_recorded_general_use_counters_) {
    has_recorded_general_use_counters_ = true;
    global_scope_->GetSharedStorageWorkletServiceClient()->RecordUseCounters(
        {mojom::blink::WebFeature::kPrivateAggregationApiAll,
         mojom::blink::WebFeature::kPrivateAggregationApiSharedStorage});
  }
}

void PrivateAggregation::EnsureEnableDebugModeUseCounterIsRecorded() {
  if (!has_recorded_enable_debug_mode_use_counter_) {
    has_recorded_enable_debug_mode_use_counter_ = true;
    global_scope_->GetSharedStorageWorkletServiceClient()->RecordUseCounters(
        {mojom::blink::WebFeature::kPrivateAggregationApiEnableDebugMode});
  }
}

void PrivateAggregation::EnsureFilteringIdUseCounterIsRecorded() {
  if (!has_recorded_filtering_id_use_counter_) {
    has_recorded_filtering_id_use_counter_ = true;
    global_scope_->GetSharedStorageWorkletServiceClient()->RecordUseCounters(
        {mojom::blink::WebFeature::kPrivateAggregationApiFilteringIds});
  }
}

}  // namespace blink
