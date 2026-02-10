// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/ml/webnn/ml_graph_transform/pipeline.h"

#include "services/webnn/public/cpp/webnn_buildflags.h"
#include "third_party/blink/renderer/modules/ml/webnn/ml_graph_builder.h"
#include "third_party/blink/renderer/modules/ml/webnn/ml_graph_transform/constant_folding_transformer.h"
#include "third_party/blink/renderer/modules/ml/webnn/ml_graph_transform/layout_transformer.h"
#include "third_party/blink/renderer/modules/ml/webnn/ml_graph_transform/qdq_detection_transformer.h"
#include "third_party/blink/renderer/modules/ml/webnn/ml_graph_transform/transpose_elimination_transformer.h"
#include "third_party/blink/renderer/modules/ml/webnn/webnn_introspection_impl.h"

#if BUILDFLAG(WEBNN_ENABLE_GRAPH_DUMP)
#include "third_party/blink/renderer/modules/ml/webnn/ml_graph_transform/utils/ml_graph_dump.h"
#endif

namespace blink {
MLGraphTransformPipeline::MLGraphTransformPipeline(
    MLGraphBuilder* graph_builder)
    : graph_builder_(graph_builder) {
  InitTransformers(graph_builder);
}

void MLGraphTransformPipeline::Trace(Visitor* visitor) const {
  visitor->Trace(graph_builder_);
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
#if BUILDFLAG(WEBNN_ENABLE_GRAPH_DUMP)
  const WebNNIntrospectionImpl& introspection =
      WebNNIntrospectionImpl::GetInstance();

  bool is_recording_enabled = introspection.IsGraphRecordingEnabled();

  MLGraphDumper* dumper = nullptr;
  if (is_recording_enabled) {
    dumper = MakeGarbageCollected<MLGraphDumper>();
    dumper->RecordGraph("0_before_transform", named_outputs);
  }

  int transform_index = 0;
#endif

  for (auto& transformer : transformers_) {
    transformer->Transform(named_outputs);

#if BUILDFLAG(WEBNN_ENABLE_GRAPH_DUMP)
    if (is_recording_enabled) {
      dumper->RecordGraph(
          StrCat({String::Number(++transform_index), "_", transformer->Name()})
              .Utf8(),
          named_outputs);
    }
#endif
  }

#if BUILDFLAG(WEBNN_ENABLE_GRAPH_DUMP)
  if (is_recording_enabled) {
    std::string utf8_json = dumper->GetJson();
    mojo_base::BigBuffer json_data(base::as_bytes(base::span(utf8_json)));
    introspection.OnGraphRecorded(std::move(json_data));
  }
#endif
}

}  // namespace blink
