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
// a DirectML graph. It's a wrapper of D3D12 command recorder, and own's the
// D3D12 command list, D3D12 command allocator, DirectML operator initializer
// and so on. CommandRecorder will be owned and called by an execution context
// class which performs GPU work, and manages command list recording and
// submission to queues.
class CommandRecorder final {
 public:
  static std::unique_ptr<CommandRecorder> Create(
      scoped_refptr<Adapter> adapter);

  ~CommandRecorder();
  CommandRecorder(const CommandRecorder&) = delete;
  CommandRecorder& operator=(const CommandRecorder&) = delete;

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

  HRESULT CloseAndExecute() const;
  // TODO(crbug.com/1273291): The command allocator can't be reset while a
  // command list is still executing, so reset the command allocator when
  // opening a new command recorder.
  HRESULT ResetCommandList() const;

 private:
  CommandRecorder(scoped_refptr<Adapter> adapter,
                  ComPtr<ID3D12CommandAllocator> command_allocator,
                  ComPtr<ID3D12GraphicsCommandList> command_list,
                  ComPtr<IDMLOperatorInitializer> operator_initializer,
                  ComPtr<IDMLCommandRecorder> command_recorder);

  scoped_refptr<Adapter> adapter_;
  ComPtr<ID3D12CommandAllocator> command_allocator_;
  ComPtr<ID3D12GraphicsCommandList> command_list_;
  ComPtr<IDMLOperatorInitializer> operator_initializer_;
  ComPtr<IDMLCommandRecorder> command_recorder_;
};

}  // namespace webnn::dml

#endif  // SERVICES_WEBNN_DML_COMMAND_RECORDER_H_
