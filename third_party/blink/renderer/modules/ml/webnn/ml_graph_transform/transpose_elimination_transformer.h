// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_ML_WEBNN_ML_GRAPH_TRANSFORM_TRANSPOSE_ELIMINATION_TRANSFORMER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_ML_WEBNN_ML_GRAPH_TRANSFORM_TRANSPOSE_ELIMINATION_TRANSFORMER_H_

#include "third_party/blink/renderer/modules/ml/webnn/ml_graph_transform/ml_graph_transformer.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_set.h"

namespace blink {

// This applies a transpose elimination optimization. The algorithm will match
// subgraphs whose inputs and outputs are all transposes, and eliminate all
// these transposes if they are valid and safe to do so.
class MODULES_EXPORT TransposeEliminationTransformer
    : public MLGraphTransformer {
 public:
  explicit TransposeEliminationTransformer(MLGraphBuilder* graph_builder)
      : MLGraphTransformer(graph_builder) {}
  void Transform(MLNamedOperands& named_outputs) override;
  void Trace(Visitor* visitor) const override;

  const StringView Name() const override {
    return "TransposeEliminationTransformer";
  }

 private:
  void HandleTranspose(
      MLOperator* transpose,
      HeapHashSet<Member<const MLOperator>>& graph_output_operators,
      MLNamedOperands& named_outputs);

  HeapHashSet<Member<const MLOperator>> visited_transposes_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_ML_WEBNN_ML_GRAPH_TRANSFORM_TRANSPOSE_ELIMINATION_TRANSFORMER_H_
