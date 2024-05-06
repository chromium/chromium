// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_WEBNN_TFLITE_GRAPH_IMPL_H_
#define SERVICES_WEBNN_TFLITE_GRAPH_IMPL_H_

#include <memory>
#include <string>
#include <utility>

#include "base/containers/flat_map.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/types/expected.h"
#include "mojo/public/cpp/base/big_buffer.h"
#include "services/webnn/public/mojom/webnn_graph.mojom-forward.h"
#include "services/webnn/webnn_graph_impl.h"

namespace webnn::tflite {

class ContextImpl;

// GraphImpl inherits from WebNNGraphImpl to represent a TFLite graph
// implementation. It is mainly responsible for building a TFLite flatbuffer
// model from mojom::GraphInfo via tflite::GraphBuilder, then initializing and
// executing the graph.
class GraphImpl final : public WebNNGraphImpl {
 public:
  static base::expected<std::unique_ptr<GraphImpl>, mojom::ErrorPtr>
  CreateAndBuild(mojom::GraphInfoPtr graph_info, ContextImpl* context);

  GraphImpl(const GraphImpl&) = delete;
  GraphImpl& operator=(const GraphImpl&) = delete;
  ~GraphImpl() override;

 private:
  class GraphResources;
  class ComputeResources;

  using NamedBuffers = base::flat_map<std::string, mojo_base::BigBuffer>;
  using AsyncComputeResult =
      std::pair<mojom::ComputeResultPtr, std::unique_ptr<ComputeResources>>;

  GraphImpl(ComputeResourceInfo compute_resource_info,
            scoped_refptr<GraphResources> graph_resources,
            std::unique_ptr<ComputeResources> compute_resources,
            ContextImpl* context);

  // Execute the compiled platform graph asynchronously. The `named_inputs` were
  // validated in base class so we can use them to compute directly, the result
  // of execution will be returned to renderer process with the `callback`.
  void ComputeImpl(NamedBuffers named_inputs,
                   mojom::WebNNGraph::ComputeCallback callback) override;

  void OnComputeComplete(ComputeCallback callback, AsyncComputeResult result);

  void DispatchImpl(
      const base::flat_map<std::string_view, WebNNBufferImpl*>& named_inputs,
      const base::flat_map<std::string_view, WebNNBufferImpl*>& named_outputs)
      override;

  // This class is owned by the `UniqueAssociatedReceiverSet` in `ContextImpl`.
  raw_ptr<ContextImpl> context_;

  scoped_refptr<GraphResources> graph_resources_;
  std::unique_ptr<ComputeResources> compute_resources_;
  base::WeakPtrFactory<GraphImpl> weak_factory_{this};
};

}  // namespace webnn::tflite

#endif  // SERVICES_WEBNN_TFLITE_GRAPH_IMPL_H_
