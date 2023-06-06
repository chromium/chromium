// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/webnn/dml/command_recorder.h"

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/notreached.h"
#include "services/webnn/dml/adapter.h"
#include "services/webnn/dml/error.h"

namespace webnn::dml {

// Static
std::unique_ptr<CommandRecorder> CommandRecorder::Create(
    scoped_refptr<Adapter> adapter) {
  ComPtr<ID3D12CommandAllocator> command_allocator;
  RETURN_NULL_IF_FAILED(adapter->d3d12_device()->CreateCommandAllocator(
      D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&command_allocator)));

  // The command list will be created upon the first call to `Open()` method.
  // Because the command list will be created in the open state, we won't want
  // to close it right after its creation.

  ComPtr<IDMLCommandRecorder> command_recorder;
  RETURN_NULL_IF_FAILED(adapter->dml_device()->CreateCommandRecorder(
      IID_PPV_ARGS(&command_recorder)));

  return base::WrapUnique(new CommandRecorder(std::move(adapter),
                                              std::move(command_allocator),
                                              std::move(command_recorder)));
}

CommandRecorder::CommandRecorder(
    scoped_refptr<Adapter> adapter,
    ComPtr<ID3D12CommandAllocator> command_allocator,
    ComPtr<IDMLCommandRecorder> command_recorder)
    : adapter_(std::move(adapter)),
      command_allocator_(std::move(command_allocator)),
      command_recorder_(std::move(command_recorder)) {}

CommandRecorder::~CommandRecorder() = default;

CommandQueue* CommandRecorder::GetCommandQueue() const {
  return adapter_->command_queue();
}

HRESULT CommandRecorder::Open() {
  CHECK(!is_open_);
  if (last_submitted_fence_value_ <=
      adapter_->command_queue()->GetCompletedValue()) {
    // When the execution of last submitted command list is completed, it's
    // safe to reset the command allocator.
    RETURN_IF_FAILED(command_allocator_->Reset());
  }
  if (!command_list_) {
    // `CreateCommandList()` creates a command list in the open state.
    RETURN_IF_FAILED(adapter_->d3d12_device()->CreateCommandList(
        0, D3D12_COMMAND_LIST_TYPE_DIRECT, command_allocator_.Get(), nullptr,
        IID_PPV_ARGS(&command_list_)));
  } else {
    // It's safe to reset the command list while it is still being executed.
    RETURN_IF_FAILED(command_list_->Reset(command_allocator_.Get(), nullptr));
  }
  is_open_ = true;
  return S_OK;
}

HRESULT CommandRecorder::CloseAndExecute() {
  CHECK(is_open_);
  RETURN_IF_FAILED(command_list_->Close());
  RETURN_IF_FAILED(
      adapter_->command_queue()->ExecuteCommandList(command_list_.Get()));
  last_submitted_fence_value_ = adapter_->command_queue()->GetLastFenceValue();
  is_open_ = false;
  return S_OK;
}

void CommandRecorder::ResourceBarrier(
    const std::vector<const D3D12_RESOURCE_BARRIER>& barriers) {
  CHECK(is_open_);
  command_list_->ResourceBarrier(barriers.size(), barriers.data());
}

void CommandRecorder::CopyBufferRegion(ID3D12Resource* dst_buffer,
                                       uint64_t dst_offset,
                                       ID3D12Resource* src_buffer,
                                       uint64_t src_offset,
                                       uint64_t byte_length) {
  CHECK(is_open_);
  command_list_->CopyBufferRegion(dst_buffer, dst_offset, src_buffer,
                                  src_offset, byte_length);
}

HRESULT CommandRecorder::InitializeGraph(
    GraphDMLImpl* graph,
    const DML_BINDING_DESC& input_array_binding) {
  CHECK(is_open_);
  CHECK(graph);
  // TODO(crbug.com/1273291): This method will be implemented after the
  // GraphDMLImpl class has been defined.
  NOTIMPLEMENTED();
  return S_OK;
}

HRESULT CommandRecorder::ExecuteGraph(
    GraphDMLImpl* graph,
    const std::vector<DML_BINDING_DESC>& input_bindings,
    const std::vector<DML_BINDING_DESC>& output_bindings) {
  CHECK(is_open_);
  CHECK(graph);
  // TODO(crbug.com/1273291): This method will be implemented after the
  // GraphDMLImpl class has been defined.
  NOTIMPLEMENTED();
  return S_OK;
}

}  // namespace webnn::dml
