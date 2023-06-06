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

namespace webnn::dml {

using Microsoft::WRL::ComPtr;

class Adapter;
class GraphDMLImpl;

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

  void ResourceBarrier(
      const std::vector<const D3D12_RESOURCE_BARRIER>& barriers);

  void CopyBufferRegion(ID3D12Resource* dst_buffer,
                        uint64_t dst_offset,
                        ID3D12Resource* src_buffer,
                        uint64_t src_offset,
                        uint64_t byte_length);

  HRESULT InitializeGraph(GraphDMLImpl* graph,
                          const DML_BINDING_DESC& input_array_binding);

  HRESULT ExecuteGraph(GraphDMLImpl* graph,
                       const std::vector<DML_BINDING_DESC>& input_bindings,
                       const std::vector<DML_BINDING_DESC>& output_bindings);

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
