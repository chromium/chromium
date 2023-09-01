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

#include "base/memory/scoped_refptr.h"
#include "services/webnn/public/mojom/webnn_graph.mojom.h"
#include "services/webnn/public/mojom/webnn_service.mojom.h"
#include "services/webnn/webnn_graph_impl.h"

namespace webnn::dml {

class CommandQueue;
class CommandRecorder;
class GraphBuilder;
struct NodeOutputInfo;

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
                             Microsoft::WRL::ComPtr<IDMLDevice> dml_device,
                             const mojom::GraphInfoPtr& graph_info,
                             mojom::WebNNContext::CreateGraphCallback callback);

  GraphImpl(const GraphImpl&) = delete;
  GraphImpl& operator=(const GraphImpl&) = delete;
  ~GraphImpl() override;

 private:
  // The members of `InputBufferBindingInfo` are used to create the buffer
  // binding (DML_BUFFER_BINDING) array for graph initialization and execution.
  struct InputBufferBindingInfo {
    InputBufferBindingInfo();
    ~InputBufferBindingInfo();

    InputBufferBindingInfo(const InputBufferBindingInfo&) = delete;
    InputBufferBindingInfo& operator=(const InputBufferBindingInfo&) = delete;

    // The key constant id is used to get the GraphInputIndex to bind a constant
    // buffer for initialization.
    std::map<uint64_t, uint32_t> constant_id_to_graph_input_index_map;
    // The key input name is used to get the GraphInputIndex to bind a input
    // buffer for inference.
    std::unordered_map<std::string, uint32_t> graph_input_name_to_index_map;
  };

  GraphImpl(std::unique_ptr<CommandRecorder> command_recorder,
            Microsoft::WRL::ComPtr<ID3D12Resource> persistent_buffer,
            Microsoft::WRL::ComPtr<IDMLCompiledOperator> compiled_operator,
            std::unique_ptr<ComputeResourceInfo> compute_resource_info);

  // The method compiles all DML operators into an IDMLCompiledOperator
  // which can be dispatched to GPU. Since IDMLDevice1::CompileGraph called in
  // this method may take long time to compile shaders (if not cached before),
  // this method should run on a background thread rather than the current GPU
  // main thread to avoid blocking.
  static Microsoft::WRL::ComPtr<IDMLCompiledOperator> CompileOnBackgroundThread(
      std::vector<NodeOutputInfo> graph_outputs,
      GraphBuilder graph_builder);

  // After the CompileOnBackgroundThread task is completed on a background
  // thread, the OnCompilationComplete method should run back on the GPU main
  // thread since graph initialization commands are submitted to GPU. Notice
  // that the compiled_operator might be nullptr if the graph compilation fails.
  static void OnCompilationComplete(
      mojom::WebNNContext::CreateGraphCallback callback,
      std::unique_ptr<CommandRecorder> command_recorder,
      base::flat_map<uint64_t, mojo_base::BigBuffer> constant_id_to_buffer_map,
      std::unique_ptr<InputBufferBindingInfo> input_buffer_binding_info,
      std::unique_ptr<ComputeResourceInfo> compute_resource_info,
      Microsoft::WRL::ComPtr<IDMLCompiledOperator> compiled_operator);

  // Create the GraphImpl instance and bind it to the mojom::WebNNGraph
  // receiver, then run callback to send the pending remote to the
  // render.
  // Notice that the persistent_buffer could be nullptr which means it isn't
  // required by the graph.
  static void OnInitializationComplete(
      std::unique_ptr<CommandRecorder> command_recorder,
      Microsoft::WRL::ComPtr<ID3D12Resource> persistent_buffer,
      Microsoft::WRL::ComPtr<IDMLCompiledOperator> compiled_operator,
      std::unique_ptr<ComputeResourceInfo> compute_resource_info,
      mojom::WebNNContext::CreateGraphCallback callback,
      HRESULT hr);

  // Execute the compiled platform graph asynchronously. The `named_inputs` was
  // validated in base class so we can use them to compute directly, the result
  // of inference will be returned to renderer process with the `callback`.
  void ComputeImpl(
      base::flat_map<std::string, mojo_base::BigBuffer> named_inputs,
      mojom::WebNNGraph::ComputeCallback callback) override;

  // The persistent buffer will be initialized after the initialization work on
  // GPU is completed and will be used for the following graph executions. It
  // could be nullptr which means it isn't required by the graph and won't need
  // to be bound for graph executions.
  Microsoft::WRL::ComPtr<ID3D12Resource> persistent_buffer_;
  std::unique_ptr<CommandRecorder> command_recorder_;
  // IDMLCompiledOperator represents a compiled and initialized DML graph to be
  // executed on GPU.
  Microsoft::WRL::ComPtr<IDMLCompiledOperator> compiled_operator_;
};

}  // namespace webnn::dml

#endif  // SERVICES_WEBNN_DML_GRAPH_IMPL_H_
