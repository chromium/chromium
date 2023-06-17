// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_WEBNN_DML_COMMAND_RECORDER_H_
#define SERVICES_WEBNN_DML_COMMAND_RECORDER_H_

#include <DirectML.h>
#include <wrl.h>
#include <vector>

#include "base/memory/scoped_refptr.h"
#include "services/webnn/dml/command_queue.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace webnn::dml {

using Microsoft::WRL::ComPtr;

class Adapter;

// CommandRecorder is mainly responsible for the initialization and execution of
// a DirectML graph. It wraps a DirectML command recorder, and manages the
// Direct3D 12 command list and command allocator for GPU work recording and
// submission.
class CommandRecorder final {
 public:
  static std::unique_ptr<CommandRecorder> Create(
      scoped_refptr<Adapter> adapter);

  ~CommandRecorder();
  CommandRecorder(const CommandRecorder&) = delete;
  CommandRecorder& operator=(const CommandRecorder&) = delete;

  // Get the command queue that this command recorder submits command list to.
  CommandQueue* GetCommandQueue() const;

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
  HRESULT Open();
  HRESULT CloseAndExecute();

  void ResourceBarrier(base::span<const D3D12_RESOURCE_BARRIER> barriers);

  void CopyBufferRegion(ID3D12Resource* dst_buffer,
                        uint64_t dst_offset,
                        ID3D12Resource* src_buffer,
                        uint64_t src_offset,
                        uint64_t byte_length);

  // Initialize a compiled DirectML operator, which may also represent a
  // DirectML graph, on the GPU, before it can be executed. For a compiled
  // operator, this method should be called only once.
  //
  // If the compiled operator has any input tensors flagged with
  // `DML_TENSOR_FLAG_OWNED_BY_DML`, their corresponding resources binding
  // should be created by the caller and supplied via `input_array_binding` of
  // `DML_BINDING_TYPE_BUFFER_ARRAY` type. It's the caller's responsibility to
  // keep these input resources alive until the GPU work is completed, e.g. by
  // calling `CommandQueue::ReferenceUntilCompleted()`.
  //
  // If the compiled operator requires any persistent resources, their resource
  // binding should be created by the caller and supplied via
  // `persistent_resource_binding` of `DML_BINDING_TYPE_BUFFER` type. The
  // persistent resource will be initialized after the GPU work is completed and
  // it will be used for the following operator executions.
  //
  // Internally, this method will create necessary temporary resources for the
  // operator initializer and these temporary resources will be kept alive until
  // the GPU work is done.
  HRESULT InitializeOperator(
      IDMLCompiledOperator* compiled_operator,
      const absl::optional<DML_BINDING_DESC>& input_array_binding,
      const absl::optional<DML_BINDING_DESC>& persistent_resource_binding);

  // Execute a compiled DirectML operator after it is initialized. The caller is
  // allowed to call this method multiple times to record operator executions
  // with different inputs. The caller should wait for the operator execution to
  // complete on the GPU before reading back the results.
  //
  // The input and output resources are supplied by the caller via
  // `input_bindings` and `output_bindings`. The input and output resources will
  // be bound to the operator's binding table. The number of bindings should
  // exactly match the number of input and output tensors of this operator. All
  // bound resources need to be in the D3D12_RESOURCE_STATE_UNORDERED_ACCESS
  // state before calling this method. It's the caller's responsibility to keep
  // these resources alive until the operator execution work completes on the
  // GPU.
  //
  // If the compiled operator also requires any persistent resources, they
  // should be initialized by `InitializeOperator()` and be supplied via
  // `persistent_resource_binding`. The lifecycle of the persistent resource
  // should be the same as other input and output resources.
  //
  // This method will create necessary temporary resources for the operator
  // execution and these temporary resources will be kept alive until the GPU
  // work is done.
  HRESULT ExecuteOperator(
      IDMLCompiledOperator* compiled_operator,
      base::span<const DML_BINDING_DESC> input_bindings,
      base::span<const DML_BINDING_DESC> output_bindings,
      const absl::optional<DML_BINDING_DESC>& persistent_resource_binding);

 private:
  CommandRecorder(scoped_refptr<Adapter> adapter,
                  ComPtr<ID3D12CommandAllocator> command_allocator,
                  ComPtr<IDMLCommandRecorder> command_recorder);

  bool is_open_ = false;
  // The first call to `CloseAndExecute()` sets the first submitted fence value.
  uint64_t last_submitted_fence_value_ = UINT64_MAX;

  scoped_refptr<Adapter> adapter_;
  ComPtr<ID3D12CommandAllocator> command_allocator_;
  ComPtr<ID3D12GraphicsCommandList> command_list_;
  ComPtr<IDMLOperatorInitializer> operator_initializer_;
  ComPtr<IDMLCommandRecorder> command_recorder_;
};

}  // namespace webnn::dml

#endif  // SERVICES_WEBNN_DML_COMMAND_RECORDER_H_
