// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_ML_WEBNN_ML_GRAPH_TRANSFORM_CONSTANT_FOLDING_TRANSFORMER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_ML_WEBNN_ML_GRAPH_TRANSFORM_CONSTANT_FOLDING_TRANSFORMER_H_

#include "third_party/blink/renderer/modules/ml/webnn/ml_graph_transform/ml_graph_transformer.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_map.h"

namespace blink {

class MLConstantOperand;

// Fold reshape and transpose operations on constants so that only output of
// these operations are left as constants.
class MODULES_EXPORT ConstantFoldingTransformer : public MLGraphTransformer {
 public:
  explicit ConstantFoldingTransformer(MLGraphBuilder* graph_builder)
      : MLGraphTransformer(graph_builder) {}
  void Transform(MLNamedOperands& named_outputs) override;

  const StringView Name() const override {
    return "ConstantFoldingTransformer";
  }

 private:
  void TryFoldConstant(MLOperator& op);
  void ApplyPermutation(MLConstantOperand* old_constant,
                        MLConstantOperand* new_constant,
                        Vector<uint32_t> permutation);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_ML_WEBNN_ML_GRAPH_TRANSFORM_CONSTANT_FOLDING_TRANSFORMER_H_
