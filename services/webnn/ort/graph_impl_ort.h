// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_WEBNN_ORT_GRAPH_IMPL_ORT_H_
#define SERVICES_WEBNN_ORT_GRAPH_IMPL_ORT_H_

#include <memory>
#include <string>

#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/types/expected.h"
#include "mojo/public/cpp/base/big_buffer.h"
#include "services/webnn/ort/allocator_ort.h"
#include "services/webnn/ort/graph_builder_ort.h"
#include "services/webnn/public/mojom/webnn_error.mojom-forward.h"
#include "services/webnn/public/mojom/webnn_graph.mojom-forward.h"
#include "services/webnn/queueable_resource_state.h"
#include "services/webnn/webnn_context_impl.h"
#include "services/webnn/webnn_graph_impl.h"
#include "third_party/microsoft_dxheaders/include/onnxruntime_c_api.h"
#include "base/memory/scoped_refptr.h"

namespace webnn {

class WebNNConstantOperand;

namespace ort {

class ContextImplOrt;
class BufferContentOrt;

// GraphImplOrt inherits from WebNNGraphImpl to represent a ort graph
// implementation. It is mainly responsible for building a ort
// model from mojom::GraphInfo via ort::GraphBuilderOrt, then executing the
// graph.
class GraphImplOrt final : public WebNNGraphImpl {
 public:
  static void CreateAndBuild(
      mojom::GraphInfoPtr graph_info,
      ComputeResourceInfo compute_resource_info,
      base::flat_map<uint64_t, std::unique_ptr<WebNNConstantOperand>>
          constant_operands,
      ContextImplOrt* context,
      WebNNContextImpl::CreateGraphImplCallback callback);

  GraphImplOrt(const GraphImplOrt&) = delete;
  GraphImplOrt& operator=(const GraphImplOrt&) = delete;
  ~GraphImplOrt() override;

 private:
  GraphImplOrt(ComputeResourceInfo compute_resource_info,
               OrtSession* session,
               ContextImplOrt* context);

  static base::expected<OrtSession*, mojom::ErrorPtr>
  CreateAndBuildOnBackgroundThread(
      mojom::GraphInfoPtr graph_info,
      mojom::CreateContextOptionsPtr context_options,
      ContextProperties context_properties,
      base::flat_map<uint64_t, std::unique_ptr<WebNNConstantOperand>>
          constant_operands,
      scoped_refptr<AllocatorOrt> allocator);

  static void DidCreateAndBuild(
      base::WeakPtr<WebNNContextImpl> context,
      ComputeResourceInfo compute_resource_info,
      WebNNContextImpl::CreateGraphImplCallback callback,
      base::expected<OrtSession*, mojom::ErrorPtr> result);

  class ComputeResources;

  // Execute the compiled platform graph asynchronously. The inputs were
  // validated in base class so we can use them to compute directly.
  void DispatchImpl(const base::flat_map<std::string_view, WebNNTensorImpl*>&
                        named_input_tensors,
                    const base::flat_map<std::string_view, WebNNTensorImpl*>&
                        named_output_tensors) override;

  // std::map<uint64_t, GraphBuilderOrt::OperandInfo> operand_infos_;
  scoped_refptr<QueueableResourceState<ComputeResources>>
      compute_resources_state_;
  base::WeakPtrFactory<GraphImplOrt> weak_factory_{this};
};

}  // namespace ort
}  // namespace webnn

#endif  // SERVICES_WEBNN_ORT_GRAPH_IMPL_ORT_H_
