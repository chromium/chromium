// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_ML_WEBNN_ML_GRAPH_TRANSFORM_TRANSPOSE_ELIMINATION_TRANSFORMER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_ML_WEBNN_ML_GRAPH_TRANSFORM_TRANSPOSE_ELIMINATION_TRANSFORMER_H_

#include "third_party/blink/renderer/modules/ml/webnn/ml_graph_transform/ml_graph_transformer.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_set.h"

namespace blink {

// This applies a transpose elimination optimization. The algorithm traverses
// the operation sequence, bypassing transpose agnostic operations to locate and
// remove inverse transpose pairs.
class MODULES_EXPORT TransposeEliminationTransformer
    : public MLGraphTransformer {
 public:
  explicit TransposeEliminationTransformer(MLGraphBuilder* graph_builder)
      : MLGraphTransformer(graph_builder) {}
  void Transform(MLNamedOperands& named_outputs) override;

 private:
  void HandleTranspose(
      MLOperator* transpose,
      HeapHashSet<Member<const MLOperator>>& graph_output_operators,
      MLNamedOperands& named_outputs);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_ML_WEBNN_ML_GRAPH_TRANSFORM_TRANSPOSE_ELIMINATION_TRANSFORMER_H_
