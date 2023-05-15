// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/webnn/dml/command_recorder.h"

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/notreached.h"
#include "services/webnn/dml/adapter.h"

namespace webnn::dml {

// Static
std::unique_ptr<CommandRecorder> CommandRecorder::Create(
    scoped_refptr<Adapter> adapter) {
  ID3D12Device* d3d12_device = adapter->d3d12_device();
  IDMLDevice* dml_device = adapter->dml_device();

  ComPtr<ID3D12CommandAllocator> command_allocator;
  HRESULT hr = d3d12_device->CreateCommandAllocator(
      D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&command_allocator));
  if (FAILED(hr)) {
    DLOG(ERROR) << "Failed to create command allocator : "
                << logging::SystemErrorCodeToString(hr);
    return nullptr;
  }

  ComPtr<ID3D12GraphicsCommandList> command_list;
  hr = d3d12_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT,
                                       command_allocator.Get(), nullptr,
                                       IID_PPV_ARGS(&command_list));
  if (FAILED(hr)) {
    DLOG(ERROR) << "Failed to create command list : "
                << logging::SystemErrorCodeToString(hr);
    return nullptr;
  }

  ComPtr<IDMLOperatorInitializer> operator_initializer;
  hr = dml_device->CreateOperatorInitializer(
      0, nullptr, IID_PPV_ARGS(&operator_initializer));
  if (FAILED(hr)) {
    DLOG(ERROR) << "Failed to create dml operator initializer : "
                << logging::SystemErrorCodeToString(hr);
    return nullptr;
  }

  ComPtr<IDMLCommandRecorder> command_recorder;
  hr = dml_device->CreateCommandRecorder(IID_PPV_ARGS(&command_recorder));
  if (FAILED(hr)) {
    DLOG(ERROR) << "Failed to create command recorder : "
                << logging::SystemErrorCodeToString(hr);
    return nullptr;
  }

  return base::WrapUnique(new CommandRecorder(
      std::move(adapter), std::move(command_allocator), std::move(command_list),
      std::move(operator_initializer), std::move(command_recorder)));
}

CommandRecorder::CommandRecorder(
    scoped_refptr<Adapter> adapter,
    ComPtr<ID3D12CommandAllocator> command_allocator,
    ComPtr<ID3D12GraphicsCommandList> command_list,
    ComPtr<IDMLOperatorInitializer> operator_initializer,
    ComPtr<IDMLCommandRecorder> command_recorder)
    : adapter_(std::move(adapter)),
      command_allocator_(std::move(command_allocator)),
      command_list_(std::move(command_list)),
      operator_initializer_(std::move(operator_initializer)),
      command_recorder_(std::move(command_recorder)) {}

CommandRecorder::~CommandRecorder() = default;

void CommandRecorder::ResourceBarrier(
    const std::vector<const D3D12_RESOURCE_BARRIER>& barriers) {
  command_list_->ResourceBarrier(barriers.size(), barriers.data());
}

void CommandRecorder::CopyBufferRegion(ID3D12Resource* dst_buffer,
                                       uint64_t dst_offset,
                                       ID3D12Resource* src_buffer,
                                       uint64_t src_offset,
                                       uint64_t byte_length) {
  command_list_->CopyBufferRegion(dst_buffer, dst_offset, src_buffer,
                                  src_offset, byte_length);
}

HRESULT CommandRecorder::InitializeGraph(
    GraphDMLImpl* graph,
    const DML_BINDING_DESC& input_array_binding) {
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
  CHECK(graph);
  // TODO(crbug.com/1273291): This method will be implemented after the
  // GraphDMLImpl class has been defined.
  NOTIMPLEMENTED();
  return S_OK;
}

HRESULT CommandRecorder::CloseAndExecute() const {
  HRESULT hr = command_list_->Close();
  if (FAILED(hr)) {
    DLOG(ERROR) << "Failed to close commandlist : "
                << logging::SystemErrorCodeToString(hr);
    return hr;
  }

  hr = adapter_->command_queue()->ExecuteCommandLists({command_list_.Get()});
  if (FAILED(hr)) {
    DLOG(ERROR) << "Failed to execute commandlist : "
                << logging::SystemErrorCodeToString(hr);
    return hr;
  }
  return S_OK;
}

HRESULT CommandRecorder::ResetCommandList() const {
  HRESULT hr = command_allocator_->Reset();
  if (FAILED(hr)) {
    return hr;
  }
  return command_list_->Reset(command_allocator_.Get(), nullptr);
}

}  // namespace webnn::dml
