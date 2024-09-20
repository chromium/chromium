// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_WEBNN_DML_GRAPH_IMPL_DML_H_
#define SERVICES_WEBNN_DML_GRAPH_IMPL_DML_H_

#include <map>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/types/expected.h"
#include "services/webnn/public/mojom/webnn_context_provider.mojom.h"
#include "services/webnn/public/mojom/webnn_graph.mojom.h"
#include "services/webnn/webnn_context_impl.h"
#include "services/webnn/webnn_graph_impl.h"
#include "third_party/microsoft_dxheaders/include/directml.h"

// Windows SDK headers should be included after DirectX headers.
#include <wrl.h>

namespace webnn::dml {

class Adapter;
class CommandRecorder;
class ContextImplDml;
class GraphBuilderDml;

// Record the total byte length of buffers and the D3D12_RANGE for each
// buffer, all with the required alignment.
template <typename Key>
struct AlignedByteLength {
  size_t total_byte_length = 0;
  std::map<Key, D3D12_RANGE> key_to_d3d12_range_map;
};

// GraphImplDml inherits WebNNGraphImpl to represent a DML graph implementation.
// It is mainly responsible for building and compiling a DML graph from
// mojom::GraphInfo via GraphBuilderDml, then initializing and executing the
// graph represented by an IDMLCompiledOperator.
class GraphImplDml final : public WebNNGraphImpl {
 public:
  // It records the graph's buffer binding info to create the buffer binding
  // (DML_BUFFER_BINDING) for the graph execution.
  struct GraphBufferBindingInfo {
    GraphBufferBindingInfo();
    ~GraphBufferBindingInfo();

    GraphBufferBindingInfo(const GraphBufferBindingInfo&);
    GraphBufferBindingInfo& operator=(const GraphBufferBindingInfo&);

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
  static base::expected<void, mojom::ErrorPtr> CreateAndBuildInternal(
      const ContextProperties& context_properties,
      scoped_refptr<Adapter> adapter,
      mojom::GraphInfoPtr& graph_info,
      base::flat_map<uint64_t, std::unique_ptr<WebNNConstantOperand>>&
          constant_operands,
      GraphBuilderDml& graph_builder,
      std::unordered_map<uint64_t, uint32_t>& constant_id_to_input_index_map,
      GraphBufferBindingInfo& graph_buffer_binding_info);

  // This method builds and compiles a DML graph from mojom::GraphInfo via
  // GraphBuilderDml, and then calls the CommandRecorder::InitializeOperator
  // method to initialize the DML graph. Next, it calls CommandQueue::WaitAsync
  // method to wait for the initialization work to be completed on GPU. The
  // GraphImplDml instance will only be created and bound to the mojom receiver
  // in GraphImplDml::OnInitializationComplete method.
  static void CreateAndBuild(
      scoped_refptr<Adapter> adapter,
      base::WeakPtr<ContextImplDml> context,
      mojom::GraphInfoPtr graph_info,
      ComputeResourceInfo compute_resource_info,
      base::flat_map<uint64_t, std::unique_ptr<WebNNConstantOperand>>
          constant_operands,
      WebNNContextImpl::CreateGraphImplCallback callback,
      bool pass_dml_execution_disable_meta_commands);

  GraphImplDml(const GraphImplDml&) = delete;
  GraphImplDml& operator=(const GraphImplDml&) = delete;
  ~GraphImplDml() override;

 private:
  // Contains the persistent resource for the graph initialization and execution
  // if the graph needs it. The resource should be kept alive until the GPU has
  // completed the execution.
  class PersistentResource final
      : public base::RefCountedThreadSafe<PersistentResource> {
   public:
    static scoped_refptr<PersistentResource> Create(
        uint64_t persistent_buffer_byte_length,
        Microsoft::WRL::ComPtr<ID3D12Resource> persistent_buffer);

    PersistentResource(const PersistentResource&) = delete;
    PersistentResource& operator=(const PersistentResource&) = delete;

    DML_BINDING_DESC persistent_buffer_binding_desc() const {
      return persistent_buffer_binding_desc_;
    }

   private:
    friend class base::RefCountedThreadSafe<PersistentResource>;
    PersistentResource(
        uint64_t persistent_buffer_byte_length,
        Microsoft::WRL::ComPtr<ID3D12Resource> persistent_buffer);
    ~PersistentResource();

    Microsoft::WRL::ComPtr<ID3D12Resource> persistent_buffer_;
    DML_BUFFER_BINDING persistent_buffer_binding_;
    DML_BINDING_DESC persistent_buffer_binding_desc_;
  };

  // Contains the GPU descriptor heap and temporary buffer for graph
  // execution. These resources should be kept alive until the GPU has completed
  // the execution. After that, the resources could be reused for next graph
  // execution or be released.
  struct GraphResources {
    GraphResources(Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> descriptor_heap,
                   uint64_t temporary_buffer_byte_length,
                   Microsoft::WRL::ComPtr<ID3D12Resource> temporary_resource);
    ~GraphResources();
    GraphResources(const GraphResources&) = delete;
    GraphResources& operator=(const GraphResources&) = delete;
    GraphResources(GraphResources&&) = delete;
    GraphResources& operator=(GraphResources&&) = delete;

    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> descriptor_heap;

    // Temporary buffers can be reused between DML dispatches. However,
    // they cannot be used between multiple queues at a time.
    // https://learn.microsoft.com/en-us/windows/ai/directml/dml-binding
    Microsoft::WRL::ComPtr<ID3D12Resource> temporary_buffer;
    std::optional<DML_BUFFER_BINDING> temporary_buffer_binding;
    std::optional<DML_BINDING_DESC> temporary_buffer_binding_desc;
  };

  static base::expected<std::unique_ptr<GraphResources>, HRESULT>
  AllocateGraphResources(Adapter* adapter,
                         IDMLCompiledOperator* compiled_operator);

  // Contains the GPU resources for a graph execution, including the descriptor
  // heap, upload buffer, input buffer, output buffer, read-back buffer and
  // temporary buffer if the graph needs. These resources should be kept alive
  // until the GPU has completed the execution. After that, the resources could
  // be reused for next graph execution or be released.
  struct ComputeResources {
    ComputeResources(
        Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> descriptor_heap,
        AlignedByteLength<std::string> input_aligned_byte_length,
        Microsoft::WRL::ComPtr<ID3D12Resource> upload_buffer,
        Microsoft::WRL::ComPtr<ID3D12Resource> input_buffer,
        AlignedByteLength<std::string> output_aligned_byte_length,
        Microsoft::WRL::ComPtr<ID3D12Resource> output_buffer,
        Microsoft::WRL::ComPtr<ID3D12Resource> readback_buffer,
        uint64_t temporary_buffer_byte_length,
        Microsoft::WRL::ComPtr<ID3D12Resource> temporary_buffer,
        std::unique_ptr<CommandRecorder> command_recorder);
    ~ComputeResources();
    ComputeResources(const ComputeResources&) = delete;
    ComputeResources& operator=(const ComputeResources&) = delete;
    ComputeResources(ComputeResources&&) = delete;
    ComputeResources& operator=(ComputeResources&&) = delete;

    AlignedByteLength<std::string> input_aligned_byte_length;
    Microsoft::WRL::ComPtr<ID3D12Resource> upload_buffer;
    Microsoft::WRL::ComPtr<ID3D12Resource> input_buffer;

    AlignedByteLength<std::string> output_aligned_byte_length;
    Microsoft::WRL::ComPtr<ID3D12Resource> output_buffer;
    Microsoft::WRL::ComPtr<ID3D12Resource> readback_buffer;

    GraphResources graph_resources;
    std::unique_ptr<CommandRecorder> command_recorder;
  };

  static base::expected<std::unique_ptr<ComputeResources>, HRESULT>
  AllocateComputeResources(Adapter* adapter,
                           IDMLCompiledOperator* compiled_operator,
                           const ComputeResourceInfo& compute_resource_info);

  // `ExecuteAndWaitSyncOnBackgroundThread` accepts a `CommandRecorder` which
  // keeps a reference to the `init_command_queue_for_npu_` in `Adapter`. The
  // method submits the command list for execution and synchronously wait for
  // initialization to complete. Since `ID3D12CommandQueue::ExecuteCommandLists`
  // called in this method may take long time on some adapters e.g. NPU, this
  // method should run on non-gpuMain threads to avoid blocking the compositor.
  //
  // CommandQueue is not a thread-safe object and should only be used by one
  // task runner at a time to avoid race conditions with its member variables.
  static HRESULT ExecuteAndWaitSyncOnBackgroundThread(
      std::unique_ptr<CommandRecorder> init_command_recorder_for_npu);

  // This method mainly records the graph execution onto the command list, binds
  // all required resources and closes the command list.
  //
  // This method is called firstly after the graph initialization has been
  // completed to prepare for the first graph execution. For following graph
  // executions, the method only needs to be called if we need to record
  // commands and bind resources again. Thus, it avoids re-calling the
  // `IDMLCommandRecorder::RecordDispatch` and
  // `ID3D12GraphicsCommandList::Close` methods which may be time-consuming for
  // some devices during the first execution and following executions of a graph
  // if not needed.
  static HRESULT RecordGraphExecution(
      Adapter* adapter,
      IDMLCompiledOperator* compiled_operator,
      const ComputeResources* compute_resources,
      const PersistentResource* persistent_resource,
      const GraphBufferBindingInfo& graph_buffer_binding_info);

  // `RecordGraphExecutionOnBackgroundThread` calls the `RecordGraphExecution`
  // method above, but runs on a background thread. The `compute_resources` is
  // passed to this method and will be returned to the caller after the graph
  // execution is recorded. Since `IDMLCommandRecorder::RecordDispatch` and
  // `ID3D12GraphicsCommandList::Close` called in this method may take long time
  // on some adapters e.g. NPU, this method should run on non-gpuMain threads to
  // avoid blocking the compositor.
  static base::expected<std::unique_ptr<GraphImplDml::ComputeResources>,
                        HRESULT>
  RecordGraphExecutionOnBackgroundThread(
      scoped_refptr<Adapter> adapter,
      scoped_refptr<PersistentResource> persistent_resource,
      Microsoft::WRL::ComPtr<IDMLCompiledOperator> compiled_operator,
      std::unique_ptr<ComputeResources> compute_resources,
      GraphBufferBindingInfo graph_buffer_binding_info);

  // After the `RecordGraphExecutionOnBackgroundThread` task or
  // `RecordGraphExecution` task is completed, the `CreateWebNNGraphImpl`
  // method runs back on the gpuMain thread to create the `GraphImplDml`
  // instance.
  static void CreateWebNNGraphImpl(
      scoped_refptr<Adapter> adapter,
      base::WeakPtr<ContextImplDml> context,
      scoped_refptr<PersistentResource> persistent_resource,
      Microsoft::WRL::ComPtr<IDMLCompiledOperator> compiled_operator,
      ComputeResourceInfo compute_resource_info,
      GraphBufferBindingInfo graph_buffer_binding_info,
      WebNNContextImpl::CreateGraphImplCallback callback,
      base::expected<std::unique_ptr<ComputeResources>, HRESULT>
          recording_result);

  // After the `RecordGraphExecutionOnBackgroundThread` task or
  // `RecordGraphExecution` task is completed, the `ExecuteAndWaitAsync`
  // method runs back on the gpuMain thread to copy the input data and submit
  // the command list for execution.
  void ExecuteAndWaitAsync(
      scoped_refptr<Adapter> adapter,
      base::flat_map<std::string, mojo_base::BigBuffer> named_inputs,
      mojom::WebNNGraph::ComputeCallback callback,
      base::expected<std::unique_ptr<ComputeResources>, HRESULT>
          recording_result);

  GraphImplDml(scoped_refptr<Adapter> adapter,
               ContextImplDml* context,
               std::unique_ptr<CommandRecorder> command_recorder,
               scoped_refptr<PersistentResource> persistent_resource,
               Microsoft::WRL::ComPtr<IDMLCompiledOperator> compiled_operator,
               ComputeResourceInfo compute_resource_info,
               GraphBufferBindingInfo graph_buffer_binding_info,
               std::unique_ptr<ComputeResources> compute_resources);

  // The method compiles all DML operators into an IDMLCompiledOperator
  // which can be dispatched to GPU. Since IDMLDevice1::CompileGraph called in
  // this method may take long time to compile shaders (if not cached before),
  // this method should run on a background thread rather than the current GPU
  // main thread to avoid blocking.
  static base::expected<Microsoft::WRL::ComPtr<IDMLCompiledOperator>, HRESULT>
  CompileOnBackgroundThread(GraphBuilderDml graph_builder,
                            bool pass_dml_execution_disable_meta_commands);

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
      scoped_refptr<Adapter> adapter,
      base::WeakPtr<ContextImplDml> context,
      WebNNContextImpl::CreateGraphImplCallback callback,
      std::unordered_map<uint64_t, uint32_t> constant_id_to_input_index_map,
      GraphBufferBindingInfo graph_buffer_binding_info,
      ComputeResourceInfo compute_resource_info,
      base::flat_map<uint64_t, std::unique_ptr<WebNNConstantOperand>>
          constant_operands,
      base::expected<Microsoft::WRL::ComPtr<IDMLCompiledOperator>, HRESULT>
          compilation_result);

  // This method calls `RecordGraphExecution` to record the graph execution,
  // create the GraphImplDml instance and bind it to the mojom::WebNNGraph
  // receiver, then run callback to send the pending remote to the renderer
  // process.
  // Notice that the `persistent_resource` could be nullptr which means
  // it isn't required by the graph.
  static void OnInitializationComplete(
      scoped_refptr<Adapter> adapter,
      base::WeakPtr<ContextImplDml> context,
      scoped_refptr<PersistentResource> persistent_resource,
      Microsoft::WRL::ComPtr<IDMLCompiledOperator> compiled_operator,
      ComputeResourceInfo compute_resource_info,
      GraphBufferBindingInfo graph_buffer_binding_info,
      WebNNContextImpl::CreateGraphImplCallback callback,
      HRESULT hr);

  // After the computation is completed, copy the output data from GPU readback
  // buffer and then run the callback to send it to the renderer process.
  //
  // The ranges in the value of the `graph_output_name_to_d3d12_range_map` are
  // the ranges in the readback output buffer and the default output buffer,
  // which indicate the aligned offset for each output of the graph.
  void OnComputationComplete(
      mojom::WebNNGraph::ComputeCallback callback,
      std::unique_ptr<ComputeResources> compute_resources,
      HRESULT hr);

  // After the dispatch is completed, recycle the graph resources for another
  // dispatch.
  void OnDispatchComplete(std::unique_ptr<GraphResources> graph_resources,
                          HRESULT hr);

  // If GraphImplDml::ComputeImpl fails, release the `compute_resources_`,
  // report the error message via `callback` and let `context_` handle the
  // error.
  void HandleComputationFailure(const std::string& error_message,
                                HRESULT hr,
                                mojom::WebNNGraph::ComputeCallback callback);

  // If GraphImplDml::DispatchImpl fails, report and log an error message and
  // release the command recorder since it may haven't been closed normally by
  // CommandRecorder::CloseAndExecute.
  void HandleDispatchFailure(std::string_view error_message, HRESULT hr);

  // Execute the compiled platform graph asynchronously. The `named_inputs` was
  // validated in base class so we can use them to compute directly, the result
  // of execution will be returned to renderer process with the `callback`.
  void ComputeImpl(
      base::flat_map<std::string, mojo_base::BigBuffer> named_inputs,
      mojom::WebNNGraph::ComputeCallback callback) override;

  void DispatchImpl(
      const base::flat_map<std::string_view, WebNNTensorImpl*>& named_inputs,
      const base::flat_map<std::string_view, WebNNTensorImpl*>& named_outputs)
      override;

  // The persistent resource is allocated after the compilation work is
  // completed for the graph initialization and will be used for the following
  // graph executions. It could be nullptr which means it isn't required by the
  // graph and won't need to be bound for graph executions.
  scoped_refptr<PersistentResource> persistent_resource_;

  // Adapter used to create the built graph.
  scoped_refptr<Adapter> adapter_;

  // ContextImplDml owns this object.
  raw_ptr<ContextImplDml> context_;

  // The command_recorder is created for the graph execution and recycled
  // after graph execution has completed. It avoids the resource allocation
  // overhead for the first execution and following executions when it is
  // available. A graph execution takes its ownership during the execution and
  // returns the ownership once the GPU has completed the execution. If it is
  // unavailable, e.g., being taken by previous uncompleted execution, a graph
  // execution will create a new one and release it after the execution is
  // done.
  std::unique_ptr<CommandRecorder> command_recorder_;
  // IDMLCompiledOperator represents a compiled and initialized DML graph to be
  // executed on GPU.
  Microsoft::WRL::ComPtr<IDMLCompiledOperator> compiled_operator_;
  GraphBufferBindingInfo graph_buffer_binding_info_;

  // Compute resources are allocated upon graph execution and
  // recycled after graph execution has completed. It avoids the resource
  // allocation overhead for the following executions when
  // it is available. A graph execution takes its ownership during the execution
  // and returns the ownership once the GPU has completed the execution. If it
  // is unavailable, e.g., being taken by previous uncompleted execution, a
  // graph execution will allocate a new one and release it after the execution
  // is done.
  std::unique_ptr<ComputeResources> compute_resources_;

  // Graph resources are allocated after graph initialization and
  // recycled after graph execution has completed. It avoids the resource
  // allocation overhead for the first execution and following executions when
  // it is available. A graph execution takes its ownership during the execution
  // and returns the ownership once the GPU has completed the execution. If it
  // is unavailable, e.g., being taken by previous uncompleted execution, a
  // graph execution will allocate a new one and release it after the execution
  // is done.
  std::unique_ptr<GraphResources> graph_resources_;

  base::flat_map<std::string, base::WeakPtr<const WebNNTensorImpl>>
      previous_input_tensors_;
  base::flat_map<std::string, base::WeakPtr<const WebNNTensorImpl>>
      previous_output_tensors_;

  base::WeakPtrFactory<GraphImplDml> weak_factory_{this};
};

}  // namespace webnn::dml

#endif  // SERVICES_WEBNN_DML_GRAPH_IMPL_DML_H_
