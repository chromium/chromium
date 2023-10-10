// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_WEBNN_DML_GRAPH_IMPL_H_
#define SERVICES_WEBNN_DML_GRAPH_IMPL_H_

#include <DirectML.h>
#include <wrl.h>
#include <map>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "services/webnn/public/mojom/webnn_context_provider.mojom.h"
#include "services/webnn/public/mojom/webnn_graph.mojom.h"
#include "services/webnn/webnn_graph_impl.h"

namespace webnn::dml {

using Microsoft::WRL::ComPtr;

class CommandQueue;
class CommandRecorder;
class GraphBuilder;

// Record the total byte length of buffers and the D3D12_RANGE for each
// buffer, all with the required alignment.
template <typename Key>
struct AlignedByteLength {
  size_t total_byte_length = 0;
  std::map<Key, D3D12_RANGE> key_to_d3d12_range_map;
};

// GraphImpl inherits WebNNGraphImpl to represent a DML graph implementation. It
// is mainly responsible for building and compiling a DML graph from
// mojom::GraphInfo via GraphBuilder, then initializing and executing the graph
// represented by an IDMLCompiledOperator.
class GraphImpl final : public WebNNGraphImpl {
 public:
  // This method builds and compiles a DML graph from mojom::GraphInfo via
  // GraphBuilder, and then call CommandRecorder::InitializeOperator method to
  // initialize the DML graph. Next, it calls CommandQueue::WaitAsync method to
  // wait for the initialization work to be completed on GPU, the GraphImpl
  // instance will only be created and bound to the mojom receiver in
  // GraphImpl::OnInitializationComplete method.
  static void CreateAndBuild(scoped_refptr<CommandQueue> command_queue,
                             ComPtr<IDMLDevice> dml_device,
                             const mojom::GraphInfoPtr& graph_info,
                             mojom::WebNNContext::CreateGraphCallback callback);

  GraphImpl(const GraphImpl&) = delete;
  GraphImpl& operator=(const GraphImpl&) = delete;
  ~GraphImpl() override;

 private:
  // It records the graph's buffer binding info to create the buffer binding
  // (DML_BUFFER_BINDING) for the graph execution.
  struct GraphBufferBindingInfo {
    GraphBufferBindingInfo();
    ~GraphBufferBindingInfo();

    GraphBufferBindingInfo(const GraphBufferBindingInfo&) = delete;
    GraphBufferBindingInfo& operator=(const GraphBufferBindingInfo&) = delete;

    GraphBufferBindingInfo(GraphBufferBindingInfo&&);
    GraphBufferBindingInfo& operator=(GraphBufferBindingInfo&&);

    // The count of input buffer bindings for the graph execution should equal
    // to the the number of both constants and inputs.
    size_t input_buffer_binding_count = 0;
    // The map is used to bind input buffers for the graph execution in
    // order.
    // The index is the DML_INPUT_GRAPH_EDGE_DESC::GraphInputIndex when
    // creating the DML_GRAPH_DESC.
    std::unordered_map<std::string, uint32_t> graph_input_name_to_index_map;
    // The map is used to bind output buffers for the graph execution in
    // order.
    // The index is the DML_OUTPUT_GRAPH_EDGE_DESC::GraphOutputIndex when
    // creating the DML_GRAPH_DESC.
    std::unordered_map<std::string, uint32_t> graph_output_name_to_index_map;
  };

  // Contains the GPU resources for a graph execution, including the descriptor
  // heap, upload buffer, input buffer, output buffer, read-back buffer and
  // temporary buffer if the graph needs. These resources should be kept alive
  // until the GPU has completed the execution. After that, the resources could
  // be reused for next graph execution or be released.
  struct ComputeResources {
    ComputeResources(ComPtr<ID3D12DescriptorHeap> descriptor_heap,
                     AlignedByteLength<std::string> input_aligned_byte_length,
                     ComPtr<ID3D12Resource> upload_buffer,
                     ComPtr<ID3D12Resource> input_buffer,
                     AlignedByteLength<std::string> output_aligned_byte_length,
                     ComPtr<ID3D12Resource> output_buffer,
                     ComPtr<ID3D12Resource> readback_buffer,
                     uint64_t temporary_buffer_byte_length,
                     ComPtr<ID3D12Resource> temporary_buffer);
    ~ComputeResources();
    ComputeResources(const ComputeResources&) = delete;
    ComputeResources& operator=(const ComputeResources&) = delete;
    ComputeResources(ComputeResources&&) = delete;
    ComputeResources& operator=(ComputeResources&&) = delete;

    ComPtr<ID3D12DescriptorHeap> descriptor_heap;

    AlignedByteLength<std::string> input_aligned_byte_length;
    ComPtr<ID3D12Resource> upload_buffer;
    ComPtr<ID3D12Resource> input_buffer;

    AlignedByteLength<std::string> output_aligned_byte_length;
    ComPtr<ID3D12Resource> output_buffer;
    ComPtr<ID3D12Resource> readback_buffer;

    ComPtr<ID3D12Resource> temporary_buffer;
    absl::optional<DML_BUFFER_BINDING> temporary_buffer_binding;
    absl::optional<DML_BINDING_DESC> temporary_buffer_binding_desc;
  };

  static std::unique_ptr<ComputeResources> AllocateComputeResources(
      CommandRecorder* command_recorder,
      IDMLCompiledOperator* compiled_operator,
      const ComputeResourceInfo& compute_resource_info);

  GraphImpl(std::unique_ptr<CommandRecorder> command_recorder,
            ComPtr<ID3D12Resource> persistent_buffer,
            ComPtr<IDMLCompiledOperator> compiled_operator,
            ComputeResourceInfo compute_resource_info,
            GraphBufferBindingInfo graph_buffer_binding_info,
            std::unique_ptr<ComputeResources> compute_resources);

  // The method compiles all DML operators into an IDMLCompiledOperator
  // which can be dispatched to GPU. Since IDMLDevice1::CompileGraph called in
  // this method may take long time to compile shaders (if not cached before),
  // this method should run on a background thread rather than the current GPU
  // main thread to avoid blocking.
  static ComPtr<IDMLCompiledOperator> CompileOnBackgroundThread(
      GraphBuilder graph_builder);

  // After the CompileOnBackgroundThread task is completed on a background
  // thread, the OnCompilationComplete method should run back on the GPU main
  // thread since graph initialization commands are submitted to GPU. Notice
  // that the compiled_operator might be nullptr if the graph compilation fails.
  //
  // The `constant_id_to_input_index_map` is used to bind constant buffers
  // for the graph initialization in order. The constant id is the key for
  // `id_to_operand_map` of `mojom::GraphInfo` interface, the input index is the
  // DML_INPUT_GRAPH_EDGE_DESC::GraphInputIndex when creating the
  // DML_GRAPH_DESC. DirectML graph treats both input tensors and constant
  // tensors to be graph inputs. The difference is the data of the constant
  // tensor is owned by DirectML and should be uploaded during the graph
  // initialization, while the data of the input tensor is uploaded for every
  // graph execution.
  static void OnCompilationComplete(
      mojom::WebNNContext::CreateGraphCallback callback,
      std::unique_ptr<CommandRecorder> command_recorder,
      base::flat_map<uint64_t, mojo_base::BigBuffer> constant_id_to_buffer_map,
      std::unordered_map<uint64_t, uint32_t> constant_id_to_input_index_map,
      GraphBufferBindingInfo graph_buffer_binding_info,
      ComputeResourceInfo compute_resource_info,
      ComPtr<IDMLCompiledOperator> compiled_operator);

  // Create the GraphImpl instance and bind it to the mojom::WebNNGraph
  // receiver, then run callback to send the pending remote to the
  // render.
  // Notice that the persistent_buffer could be nullptr which means it isn't
  // required by the graph.
  static void OnInitializationComplete(
      std::unique_ptr<CommandRecorder> command_recorder,
      ComPtr<ID3D12Resource> persistent_buffer,
      ComPtr<IDMLCompiledOperator> compiled_operator,
      ComputeResourceInfo compute_resource_info,
      GraphBufferBindingInfo graph_buffer_binding_info,
      mojom::WebNNContext::CreateGraphCallback callback,
      HRESULT hr);

  // After the computation is completed, copy the output data from GPU readback
  // buffer and then run the callback to send it to the render process.
  //
  // The ranges in the value of the `graph_output_name_to_d3d12_range_map` are
  // the ranges in the readback output buffer and the default output buffer,
  // which indicate the aligned offset for each output of the graph.
  void OnComputationComplete(
      mojom::WebNNGraph::ComputeCallback callback,
      std::unique_ptr<ComputeResources> compute_resources,
      HRESULT hr);

  // If GraphImpl::ComputeImpl fails, report an error and release the command
  // recorder since it may haven't been closed normally by
  // CommandRecorder::CloseAndExecute.
  void HandleComputationFailure(mojom::WebNNGraph::ComputeCallback callback);
  // It will call the
  // `HandleComputationFailure(mojom::WebNNGraph::ComputeCallback callback)`
  // method and also log the message from the `error`.
  void HandleComputationFailure(const char* error,
                                mojom::WebNNGraph::ComputeCallback callback);
  // It will call the
  // `HandleComputationFailure(mojom::WebNNGraph::ComputeCallback callback)`
  // method and log the message from the `error` and the system error code `hr`.
  void HandleComputationFailure(const char* error,
                                HRESULT hr,
                                mojom::WebNNGraph::ComputeCallback callback);

  // Execute the compiled platform graph asynchronously. The `named_inputs` was
  // validated in base class so we can use them to compute directly, the result
  // of execution will be returned to renderer process with the `callback`.
  void ComputeImpl(
      base::flat_map<std::string, mojo_base::BigBuffer> named_inputs,
      mojom::WebNNGraph::ComputeCallback callback) override;

  // The persistent buffer will be initialized after the initialization work on
  // GPU is completed and will be used for the following graph executions. It
  // could be nullptr which means it isn't required by the graph and won't need
  // to be bound for graph executions.
  ComPtr<ID3D12Resource> persistent_buffer_;
  absl::optional<DML_BUFFER_BINDING> persistent_buffer_binding_;
  absl::optional<DML_BINDING_DESC> persistent_buffer_binding_desc_;
  scoped_refptr<CommandQueue> command_queue_;
  ComPtr<IDMLDevice> dml_device_;
  std::unique_ptr<CommandRecorder> command_recorder_;
  // IDMLCompiledOperator represents a compiled and initialized DML graph to be
  // executed on GPU.
  ComPtr<IDMLCompiledOperator> compiled_operator_;
  GraphBufferBindingInfo graph_buffer_binding_info_;

  // The compute resource is pre-allocated after graph initialization and
  // recycled after graph execution has completed. It avoids the resource
  // allocation overhead for the first execution and following executions when
  // it is available. A graph execution takes its ownership during the execution
  // and returns the ownership once the GPU has completed the execution. If it
  // is unavailable, e.g., being taken by previous uncompleted execution, a
  // graph execution will allocate a new one and release it after the execution
  // is done.
  std::unique_ptr<ComputeResources> compute_resources_;

  base::WeakPtrFactory<GraphImpl> weak_factory_{this};
};

}  // namespace webnn::dml

#endif  // SERVICES_WEBNN_DML_GRAPH_IMPL_H_
