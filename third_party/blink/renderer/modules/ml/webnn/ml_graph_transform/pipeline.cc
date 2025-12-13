// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/ml/webnn/ml_graph_transform/pipeline.h"

#include "third_party/blink/renderer/modules/ml/webnn/ml_graph_transform/constant_folding_transformer.h"
#include "third_party/blink/renderer/modules/ml/webnn/ml_graph_transform/layout_transformer.h"
#include "third_party/blink/renderer/modules/ml/webnn/ml_graph_transform/qdq_detection_transformer.h"
#include "third_party/blink/renderer/modules/ml/webnn/ml_graph_transform/transpose_elimination_transformer.h"

namespace blink {
MLGraphTransformPipeline::MLGraphTransformPipeline(
    MLGraphBuilder* graph_builder) {
  InitTransformers(graph_builder);
}

void MLGraphTransformPipeline::Trace(Visitor* visitor) const {
  visitor->Trace(transformers_);
}

void MLGraphTransformPipeline::InitTransformers(MLGraphBuilder* graph_builder) {
  // Required transformers for backends to consume the graph.
  transformers_.push_back(
      MakeGarbageCollected<LayoutTransformer>(graph_builder));

  // Non-essential transformers. For better performance.
  transformers_.push_back(
      MakeGarbageCollected<QDQDetectionTransformer>(graph_builder));
  // The QDQDetectionTransformer might shuffle transposes up the graph that can
  // be constant folded.
  transformers_.push_back(
      MakeGarbageCollected<ConstantFoldingTransformer>(graph_builder));
  transformers_.push_back(
      MakeGarbageCollected<TransposeEliminationTransformer>(graph_builder));
}

void MLGraphTransformPipeline::Run(MLNamedOperands& named_outputs) {
  for (auto& transformer : transformers_) {
    transformer->Transform(named_outputs);
  }
}

}  // namespace blink
