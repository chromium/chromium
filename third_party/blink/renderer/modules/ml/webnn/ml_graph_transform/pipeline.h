// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_ML_WEBNN_ML_GRAPH_TRANSFORM_PIPELINE_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_ML_WEBNN_ML_GRAPH_TRANSFORM_PIPELINE_H_

#include "third_party/blink/renderer/modules/ml/webnn/ml_graph_transform/ml_graph_transformer.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
namespace blink {

class MLGraphTransformPipeline
    : public GarbageCollected<MLGraphTransformPipeline> {
 public:
  explicit MLGraphTransformPipeline(MLGraphBuilder* graph_builder);

  void Trace(Visitor* visitor) const;

  void InitTransformers(MLGraphBuilder* graph_builder);

  void Run(MLNamedOperands& named_outputs);

 private:
  HeapVector<Member<MLGraphTransformer>> transformers_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_ML_WEBNN_ML_GRAPH_TRANSFORM_PIPELINE_H_
