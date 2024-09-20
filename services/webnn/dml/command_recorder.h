// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_WEBNN_DML_COMMAND_RECORDER_H_
#define SERVICES_WEBNN_DML_COMMAND_RECORDER_H_

#include <map>
#include <optional>
#include <vector>

#include "base/component_export.h"
#include "base/containers/span.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/types/expected.h"
#include "third_party/microsoft_dxheaders/include/directml.h"

// Windows SDK headers should be included after DirectX headers.
#include <wrl.h>

namespace webnn::dml {

class CommandQueue;
class TensorImplDml;

// CommandRecorder is mainly responsible for the initialization and execution of
// a DirectML graph. It wraps a DirectML command recorder, and manages the
// Direct3D 12 command list and command allocator for GPU work recording and
// submission.
class COMPONENT_EXPORT(WEBNN_SERVICE) CommandRecorder final {
 public:
  static base::expected<std::unique_ptr<CommandRecorder>, HRESULT> Create(
      scoped_refptr<CommandQueue> queue,
      Microsoft::WRL::ComPtr<IDMLDevice1> dml_device);

  ~CommandRecorder();
  CommandRecorder(const CommandRecorder&) = delete;
  CommandRecorder& operator=(const CommandRecorder&) = delete;

  // Indicates whether this recorder is ready to record new commands.
  bool IsOpen() const { return is_open_; }

  // Call the `Open()` method before recording any new commands. The `Open()`
  // method would prepare the underlying command list and command allocator.
  // After recording the commands, call the `CloseAndExecute()` method to submit
  // the recorded command list to the command queue for GPU execution. The
  // caller may need to call the `CommandQueue::WaitAsync()` method on the
  // command queue to wait for the GPU execution to complete.
  //
  // The caller is allowed to open the command recorder without waiting for the
  // GPU to complete execution of previous recorded commands. The `Open()`
  // method would ensure the command allocator is not reset while the previous
  // command list is still being used by the GPU.
  //
  // If there are any failures during the command recording, the caller should
  // delete this command recorder that ensures to release the references of all
  // recorded commands and their resources.
  HRESULT Open();

  // Close the command list.
  HRESULT Close();
  // Submit the command list for execution and reference all resources required
  // by this execution.
  HRESULT Execute();
  // This method will call the above `Close()` and `Execute()` methods.
  HRESULT CloseAndExecute();

  void ResourceBarrier(base::span<const D3D12_RESOURCE_BARRIER> barriers);

  // Record the buffer copy command. The destination and source buffers will be
  // referenced until the GPU work has completed.
  void CopyBufferRegion(Microsoft::WRL::ComPtr<ID3D12Resource> dst_buffer,
                        uint64_t dst_offset,
                        Microsoft::WRL::ComPtr<ID3D12Resource> src_buffer,
                        uint64_t src_offset,
                        uint64_t byte_length);

  // Helper function to upload tensor data from GPU to CPU.
  void UploadTensorWithBarrier(
      TensorImplDml* dst_tensor,
      Microsoft::WRL::ComPtr<ID3D12Resource> src_buffer,
      size_t buffer_size);

  // Helper function to readback tensor data from GPU to CPU.
  void ReadbackTensorWithBarrier(
      Microsoft::WRL::ComPtr<ID3D12Resource> dst_buffer,
      TensorImplDml* src_tensor,
      size_t buffer_size);

  // Initialize a compiled DirectML operator, which may also represent a
  // DirectML graph, on the GPU, before it can be executed. For a compiled
  // operator, this method should be called only once.
  //
  // If the compiled operator has any input tensors flagged with
  // `DML_TENSOR_FLAG_OWNED_BY_DML`, their corresponding resources binding
  // should be created by the caller and supplied via `input_array_binding` of
  // `DML_BINDING_TYPE_BUFFER_ARRAY` type.
  //
  // If the compiled operator requires any persistent resources, their resource
  // binding should be created by the caller and supplied via
  // `persistent_resource_binding` of `DML_BINDING_TYPE_BUFFER` type. The
  // persistent resource will be initialized after the GPU work is completed and
  // it will be used for the following operator executions.
  //
  // Internally, this method will create necessary temporary resources for the
  // operator initializer.
  //
  // This method ensures that all the required GPU resources will be kept alive
  // until the operator initialization has completed on the GPU.
  HRESULT InitializeOperator(
      IDMLCompiledOperator* compiled_operator,
      const std::optional<DML_BINDING_DESC>& input_array_binding,
      const std::optional<DML_BINDING_DESC>& persistent_resource_binding);

  // Execute a compiled DirectML operator after it is initialized. The caller is
  // allowed to call this method multiple times to record operator executions
  // with different inputs. The caller should wait for the operator execution to
  // complete on the GPU before reading back the results.
  //
  // The caller should create the descriptor heap large enough for the number of
  // descriptors that the compiled operator needs and supply it via
  // `descriptor_heap`.
  //
  // The input and output resources are supplied by the caller via
  // `input_bindings` and `output_bindings`. The input and output resources will
  // be bound to the operator's binding table. The number of bindings should
  // exactly match the number of input and output tensors of this operator. All
  // bound resources need to be in the D3D12_RESOURCE_STATE_UNORDERED_ACCESS
  // state before calling this method.
  //
  // If the compiled operator also requires any persistent resources, they
  // should be initialized by `InitializeOperator()` and be supplied via
  // `persistent_resource_binding`.
  //
  // If the compiled operator also requires any temporary resources, they should
  // be supplied via `temporary_resource_binding`.
  //
  // This method ensures that all the required GPU resources will be kept alive
  // until the operator execution has completed on the GPU.
  HRESULT ExecuteOperator(
      Microsoft::WRL::ComPtr<IDMLCompiledOperator> compiled_operator,
      Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> descriptor_heap,
      base::span<const DML_BINDING_DESC> input_bindings,
      base::span<const DML_BINDING_DESC> output_bindings,
      const std::optional<DML_BINDING_DESC>& persistent_resource_binding,
      const std::optional<DML_BINDING_DESC>& temporary_resource_binding);

  CommandQueue* command_queue() const { return command_queue_.get(); }

  // Called when a WebNNTensor requires tracking of GPU progress
  // because a recorded command will modify the data which could be accessed
  // by the CPU. The last submission fence will be updated during
  // recording to ensure the CPU can safely use the buffer.
  void OnTensorAccessed(TensorImplDml* tensor);

  void ReferenceCommandResources(Microsoft::WRL::ComPtr<IUnknown> object);

 private:
  CommandRecorder(
      scoped_refptr<CommandQueue> command_queue,
      Microsoft::WRL::ComPtr<IDMLDevice1> dml_device,
      Microsoft::WRL::ComPtr<ID3D12CommandAllocator> command_allocator,
      Microsoft::WRL::ComPtr<IDMLCommandRecorder> command_recorder);

  // Records execution of a dispatchable object (an operator initializer, or a
  // compiled operator) onto a command list.
  void RecordDispatch(IDMLDispatchable* dispatchable,
                      IDMLBindingTable* binding_table);

  bool is_open_ = false;
  // The first call to `CloseAndExecute()` sets the first submitted fence value.
  uint64_t last_submitted_fence_value_ = UINT64_MAX;

  scoped_refptr<CommandQueue> command_queue_;
  Microsoft::WRL::ComPtr<IDMLDevice1> dml_device_;
  Microsoft::WRL::ComPtr<ID3D12Device> d3d12_device_;
  Microsoft::WRL::ComPtr<ID3D12CommandAllocator> command_allocator_;
  Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> command_list_;
  Microsoft::WRL::ComPtr<IDMLCommandRecorder> command_recorder_;

  // Keep the resources used by recorded commands. After commands submission,
  // these resources would be kept alive until the command queue has completed
  // the execution of these commands on GPU.
  std::vector<Microsoft::WRL::ComPtr<IUnknown>> command_resources_;

  // Keep WebNNTensors used in recorded commands pending execution. The key is
  // a strong pointer to the underlying ID3D12Resource to ensure the recorded
  // buffer entry will always remain valid until Open() is called again to reset
  // it.
  std::map<Microsoft::WRL::ComPtr<ID3D12Resource>, base::WeakPtr<TensorImplDml>>
      command_tensor_impls_;
};

}  // namespace webnn::dml

#endif  // SERVICES_WEBNN_DML_COMMAND_RECORDER_H_
