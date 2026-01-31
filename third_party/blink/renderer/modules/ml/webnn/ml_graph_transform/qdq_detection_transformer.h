// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_ML_WEBNN_ML_GRAPH_TRANSFORM_QDQ_DETECTION_TRANSFORMER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_ML_WEBNN_ML_GRAPH_TRANSFORM_QDQ_DETECTION_TRANSFORMER_H_

#include "third_party/blink/renderer/modules/ml/webnn/ml_graph_transform/ml_graph_transformer.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_set.h"

namespace blink {

// This detects potential QDQ (Quantize-Dequantize) patterns in the graph and
// transform it to suitable form for backend fusion.
// For example, here is the original graph:
//
//    ...               ...
//      \               /
//       DQ(0)         DQ(1)
//        \           /
//    transpose(0)  transpose(1)
//          \      /
//           conv2d(center operator)
//             \
//          transpose(2)
//               \
//                Q
//
// The TFlite(LiteRT) backend can fuse conv2d with DQ inputs and Q outputs,
// so we should swap the position of DQ(Q) and transpose to make it
// suitable for backend fusion. Like this:
//
//    ...               ...
//      \               /
//   transpose(0)     transpose(1)
//        \           /
//        DQ(0)      DQ(1)
//          \      /
//           conv2d(center operator)
//             \
//              Q
//               \
//            transpose(2)
class MODULES_EXPORT QDQDetectionTransformer : public MLGraphTransformer {
 public:
  explicit QDQDetectionTransformer(MLGraphBuilder* graph_builder)
      : MLGraphTransformer(graph_builder) {}

  void Transform(MLNamedOperands& named_outputs) override;

  const StringView Name() const override { return "QDQDetectionTransformer"; }

 private:
  void HandleQuantize(
      MLOperator* quantize,
      HeapHashSet<Member<const MLOperator>>& graph_output_operators,
      MLNamedOperands& named_outputs);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_ML_WEBNN_ML_GRAPH_TRANSFORM_QDQ_DETECTION_TRANSFORMER_H_
