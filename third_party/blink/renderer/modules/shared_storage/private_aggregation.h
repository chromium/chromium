// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_SHARED_STORAGE_PRIVATE_AGGREGATION_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_SHARED_STORAGE_PRIVATE_AGGREGATION_H_

#include <stdint.h>

#include "third_party/blink/public/mojom/private_aggregation/aggregatable_report.mojom-blink.h"
#include "third_party/blink/public/mojom/private_aggregation/private_aggregation_host.mojom-blink.h"
#include "third_party/blink/public/mojom/shared_storage/shared_storage_worklet_service.mojom-blink-forward.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_map.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

class ExceptionState;
class PrivateAggregationDebugModeOptions;
class PrivateAggregationHistogramContribution;
class ScriptState;
class SharedStorageWorkletGlobalScope;

class MODULES_EXPORT PrivateAggregation final : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  struct OperationState : public GarbageCollected<OperationState> {
    // Defaults to debug mode being disabled.
    mojom::blink::DebugModeDetails debug_mode_details;

    // Pending contributions
    Vector<mojom::blink::AggregatableReportHistogramContributionPtr>
        private_aggregation_contributions;

    void Trace(Visitor* visitor) const {}
  };

  explicit PrivateAggregation(SharedStorageWorkletGlobalScope* global_scope);

  ~PrivateAggregation() override;

  void Trace(Visitor*) const override;

  // PrivateAggregation IDL
  void sendHistogramReport(ScriptState*,
                           const PrivateAggregationHistogramContribution*,
                           ExceptionState&);
  void enableDebugMode(ScriptState*, ExceptionState&);
  void enableDebugMode(ScriptState*,
                       const PrivateAggregationDebugModeOptions*,
                       ExceptionState&);

  void OnOperationStarted(int64_t operation_id);
  void OnOperationFinished(int64_t operation_id);

  void OnWorkletDestroyed();

 private:
  void EnsureUseCountersAreRecorded();

  bool has_recorded_use_counters_ = false;

  Member<SharedStorageWorkletGlobalScope> global_scope_;
  HeapHashMap<int64_t,
              Member<OperationState>,
              IntWithZeroKeyHashTraits<int64_t>>
      operation_states_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_SHARED_STORAGE_PRIVATE_AGGREGATION_H_
