// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/ml/webnn/ml_graph_transform/pipeline.h"

#include "third_party/blink/renderer/modules/ml/webnn/ml_graph_transform/layout_transformer.h"

namespace blink {
MLGraphTransformPipeline::MLGraphTransformPipeline(
    MLGraphBuilder* graph_builder) {
  InitTransformers(graph_builder);
}

void MLGraphTransformPipeline::Trace(Visitor* visitor) const {
  visitor->Trace(transformers_);
}

void MLGraphTransformPipeline::InitTransformers(MLGraphBuilder* graph_builder) {
  transformers_.push_back(
      MakeGarbageCollected<LayoutTransformer>(graph_builder));
}

void MLGraphTransformPipeline::Run(MLNamedOperands& named_outputs) {
  for (auto& transformer : transformers_) {
    transformer->Transform(named_outputs);
  }
}

}  // namespace blink
