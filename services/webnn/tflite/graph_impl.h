// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_WEBNN_TFLITE_GRAPH_IMPL_H_
#define SERVICES_WEBNN_TFLITE_GRAPH_IMPL_H_

#include "services/webnn/public/mojom/webnn_graph.mojom.h"
#include "services/webnn/webnn_graph_impl.h"
#include "third_party/flatbuffers/src/include/flatbuffers/flatbuffers.h"
#include "third_party/tflite/src/tensorflow/lite/core/interpreter.h"
#include "third_party/tflite/src/tensorflow/lite/model_builder.h"

namespace webnn::tflite {

// GraphImpl inherits from WebNNGraphImpl to represent a TFLite graph
// implementation. It is mainly responsible for building a TFLite flatbuffer
// model from mojom::GraphInfo via tflite::GraphBuilder, then initializing and
// executing the graph.
class GraphImpl final : public WebNNGraphImpl {
 public:
  static void CreateAndBuild(mojom::GraphInfoPtr graph_info,
                             mojom::WebNNContext::CreateGraphCallback callback);

  GraphImpl(const GraphImpl&) = delete;
  GraphImpl& operator=(const GraphImpl&) = delete;
  ~GraphImpl() override;

 private:
  GraphImpl(ComputeResourceInfo compute_resource_info,
            flatbuffers::DetachedBuffer model_content,
            std::unique_ptr<::tflite::FlatBufferModel> model,
            std::unique_ptr<::tflite::Interpreter> intepreter);

  // Execute the compiled platform graph asynchronously. The `named_inputs` were
  // validated in base class so we can use them to compute directly, the result
  // of execution will be returned to renderer process with the `callback`.
  void ComputeImpl(
      base::flat_map<std::string, mojo_base::BigBuffer> named_inputs,
      mojom::WebNNGraph::ComputeCallback callback) override;

  // `interpreter_` depends on `model_` and `model_content_` outliving it.
  flatbuffers::DetachedBuffer model_content_;
  std::unique_ptr<::tflite::FlatBufferModel> model_;
  std::unique_ptr<::tflite::Interpreter> interpreter_;
};

}  // namespace webnn::tflite

#endif  // SERVICES_WEBNN_TFLITE_GRAPH_IMPL_H_
