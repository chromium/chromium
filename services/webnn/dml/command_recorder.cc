// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/webnn/dml/command_recorder.h"

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/notreached.h"
#include "base/numerics/safe_conversions.h"
#include "services/webnn/dml/command_queue.h"
#include "services/webnn/dml/error.h"
#include "services/webnn/dml/utils.h"

namespace webnn::dml {

namespace {

D3D12_RESOURCE_BARRIER CreateUAVBarrier(ID3D12Resource* resource) {
  return {.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV,
          .Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE,
          .UAV = {.pResource = resource}};
}

D3D12_HEAP_PROPERTIES CreateHeapProperties(D3D12_HEAP_TYPE type) {
  return {.Type = type,
          .CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN,
          .MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN,
          .CreationNodeMask = 1,
          .VisibleNodeMask = 1};
}

D3D12_RESOURCE_DESC CreateResourceDesc(
    uint64_t size,
    D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE) {
  return {.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER,
          .Alignment = 0,
          .Width = size,
          .Height = 1,
          .DepthOrArraySize = 1,
          .MipLevels = 1,
          .Format = DXGI_FORMAT_UNKNOWN,
          .SampleDesc = {1, 0},
          .Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR,
          .Flags = flags};
}

}  // namespace

// Static
std::unique_ptr<CommandRecorder> CommandRecorder::Create(
    scoped_refptr<CommandQueue> queue,
    ComPtr<IDMLDevice> dml_device) {
  ComPtr<ID3D12CommandAllocator> command_allocator;
  RETURN_NULL_IF_FAILED(
      GetD3D12Device(dml_device.Get())
          ->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,
                                   IID_PPV_ARGS(&command_allocator)));

  // The command list will be created upon the first call to `Open()` method.
  // Because the command list will be created in the open state, we won't want
  // to close it right after its creation.

  ComPtr<IDMLCommandRecorder> command_recorder;
  RETURN_NULL_IF_FAILED(
      dml_device->CreateCommandRecorder(IID_PPV_ARGS(&command_recorder)));

  return base::WrapUnique(new CommandRecorder(
      std::move(queue), std::move(dml_device), std::move(command_allocator),
      std::move(command_recorder)));
}

CommandRecorder::CommandRecorder(
    scoped_refptr<CommandQueue> command_queue,
    ComPtr<IDMLDevice> dml_device,
    ComPtr<ID3D12CommandAllocator> command_allocator,
    ComPtr<IDMLCommandRecorder> command_recorder)
    : command_queue_(std::move(command_queue)),
      dml_device_(std::move(dml_device)),
      d3d12_device_(GetD3D12Device(dml_device_.Get())),
      command_allocator_(std::move(command_allocator)),
      command_recorder_(std::move(command_recorder)) {}

CommandRecorder::~CommandRecorder() = default;

CommandQueue* CommandRecorder::GetCommandQueue() const {
  return command_queue_.get();
}

HRESULT CommandRecorder::Open() {
  CHECK(!is_open_);
  if (last_submitted_fence_value_ <= command_queue_->GetCompletedValue()) {
    // When the execution of last submitted command list is completed, it's
    // safe to reset the command allocator.
    RETURN_IF_FAILED(command_allocator_->Reset());
  }
  if (!command_list_) {
    // `CreateCommandList()` creates a command list in the open state.
    RETURN_IF_FAILED(d3d12_device_->CreateCommandList(
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
  RETURN_IF_FAILED(command_queue_->ExecuteCommandList(command_list_.Get()));
  last_submitted_fence_value_ = command_queue_->GetLastFenceValue();
  is_open_ = false;
  return S_OK;
}

void CommandRecorder::ResourceBarrier(
    base::span<const D3D12_RESOURCE_BARRIER> barriers) {
  CHECK(is_open_);
  command_list_->ResourceBarrier(base::checked_cast<uint32_t>(barriers.size()),
                                 barriers.data());
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

HRESULT CommandRecorder::InitializeOperator(
    IDMLCompiledOperator* compiled_operator,
    const absl::optional<DML_BINDING_DESC>& input_array_binding,
    const absl::optional<DML_BINDING_DESC>& persistent_resource_binding) {
  CHECK(is_open_);
  CHECK(compiled_operator);

  ComPtr<IDMLOperatorInitializer> initializer;
  IDMLCompiledOperator* compiled_operators[] = {compiled_operator};
  RETURN_IF_FAILED(dml_device_->CreateOperatorInitializer(
      /* operatorCount */ 1, compiled_operators, IID_PPV_ARGS(&initializer)));

  DML_BINDING_PROPERTIES initialization_binding_properties =
      initializer->GetBindingProperties();

  ComPtr<ID3D12DescriptorHeap> descriptor_heap;
  // Some operator initializers, such as Relu, requires 0 descriptors. However,
  // the DirectML binding table requires valid CPU and GPU descriptor handles.
  // So create a descriptor heap with at least 1 descriptor.
  const uint32_t num_descriptors_in_heap =
      std::max(1u, initialization_binding_properties.RequiredDescriptorCount);
  D3D12_DESCRIPTOR_HEAP_DESC descriptor_heap_desc{
      .Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
      .NumDescriptors = num_descriptors_in_heap,
      .Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE};
  RETURN_IF_FAILED(d3d12_device_->CreateDescriptorHeap(
      &descriptor_heap_desc, IID_PPV_ARGS(&descriptor_heap)));

  ID3D12DescriptorHeap* descriptor_heaps[] = {descriptor_heap.Get()};
  command_list_->SetDescriptorHeaps(/* NumDescriptorHeaps */ 1,
                                    descriptor_heaps);

  DML_BINDING_TABLE_DESC binding_table_desc = {
      .Dispatchable = initializer.Get(),
      .CPUDescriptorHandle =
          descriptor_heap->GetCPUDescriptorHandleForHeapStart(),
      .GPUDescriptorHandle =
          descriptor_heap->GetGPUDescriptorHandleForHeapStart(),
      .SizeInDescriptors =
          initialization_binding_properties.RequiredDescriptorCount};
  ComPtr<IDMLBindingTable> binding_table;
  RETURN_IF_FAILED(dml_device_->CreateBindingTable(
      &binding_table_desc, IID_PPV_ARGS(&binding_table)));

  // Create and bind the temporary resource if the operator initializer
  // requires.
  auto temp_resource_size =
      initialization_binding_properties.TemporaryResourceSize;
  if (temp_resource_size > 0) {
    ComPtr<ID3D12Resource> temp_resource;
    RETURN_IF_FAILED(CreateDefaultBuffer(temp_resource_size, temp_resource));
    DML_BUFFER_BINDING temp_buffer_binding{.Buffer = temp_resource.Get(),
                                           .Offset = 0,
                                           .SizeInBytes = temp_resource_size};
    DML_BINDING_DESC temp_binding_desc{.Type = DML_BINDING_TYPE_BUFFER,
                                       .Desc = &temp_buffer_binding};
    binding_table->BindTemporaryResource(&temp_binding_desc);
    command_queue_->ReferenceUntilCompleted(std::move(temp_resource));
  }

  // The input resources with DML_TENSOR_FLAG_OWNED_BY_DML flag (e.g. weights)
  // should be bound as input during operator initialization.
  if (input_array_binding.has_value()) {
    CHECK_EQ(input_array_binding.value().Type, DML_BINDING_TYPE_BUFFER_ARRAY);
    binding_table->BindInputs(/* bindingCount */ 1,
                              &input_array_binding.value());
  }

  // The persistent resource should be bound as output during operator
  // initialization.
  if (persistent_resource_binding.has_value()) {
    CHECK_EQ(persistent_resource_binding.value().Type, DML_BINDING_TYPE_BUFFER);
    binding_table->BindOutputs(/* bindingCount */ 1,
                               &persistent_resource_binding.value());
  }

  command_recorder_->RecordDispatch(command_list_.Get(), initializer.Get(),
                                    binding_table.Get());

  // The operator initializer owns GPU resources, it should be kept alive until
  // the dispatch using it have completed execution on the GPU.
  command_queue_->ReferenceUntilCompleted(std::move(initializer));

  // It's safe to release the binding table right after the dispatch has been
  // recorded into the command list. However, the heap which is referred to by
  // the GPU descriptor handle should be kept alive until all work referencing
  // it has completed execution on the GPU.
  command_queue_->ReferenceUntilCompleted(std::move(descriptor_heap));

  // Record a UAV barrier when the persistent is used, because the following
  // operator dispatches may depend on it.
  if (persistent_resource_binding.has_value()) {
    auto uav = CreateUAVBarrier(nullptr);
    command_list_->ResourceBarrier(/* NumBarriers */ 1, &uav);
  }

  return S_OK;
}

HRESULT CommandRecorder::ExecuteOperator(
    IDMLCompiledOperator* compiled_operator,
    base::span<const DML_BINDING_DESC> input_bindings,
    base::span<const DML_BINDING_DESC> output_bindings,
    const absl::optional<DML_BINDING_DESC>& persistent_resource_binding) {
  CHECK(is_open_);
  CHECK(compiled_operator);

  DML_BINDING_PROPERTIES execution_binding_properties =
      compiled_operator->GetBindingProperties();

  // TODO(crbug.com/1455278): Consider maintaining a descriptors pool for better
  // resource reuse.
  ComPtr<ID3D12DescriptorHeap> descriptor_heap;
  CHECK_GT(execution_binding_properties.RequiredDescriptorCount, 0u);
  D3D12_DESCRIPTOR_HEAP_DESC descriptor_heap_desc{
      .Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
      .NumDescriptors = execution_binding_properties.RequiredDescriptorCount,
      .Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE};
  RETURN_IF_FAILED(d3d12_device_->CreateDescriptorHeap(
      &descriptor_heap_desc, IID_PPV_ARGS(&descriptor_heap)));

  ID3D12DescriptorHeap* descriptor_heaps[] = {descriptor_heap.Get()};
  command_list_->SetDescriptorHeaps(/* NumDescriptorHeaps */ 1,
                                    descriptor_heaps);

  DML_BINDING_TABLE_DESC binding_table_desc = {
      .Dispatchable = compiled_operator,
      .CPUDescriptorHandle =
          descriptor_heap->GetCPUDescriptorHandleForHeapStart(),
      .GPUDescriptorHandle =
          descriptor_heap->GetGPUDescriptorHandleForHeapStart(),
      .SizeInDescriptors =
          execution_binding_properties.RequiredDescriptorCount};
  // TODO(crbug.com/1455278): Consider reusing the binding table.
  ComPtr<IDMLBindingTable> binding_table;
  RETURN_IF_FAILED(dml_device_->CreateBindingTable(
      &binding_table_desc, IID_PPV_ARGS(&binding_table)));

  // Create and bind the temporary resource if the operator execution requires.
  auto temp_resource_size = execution_binding_properties.TemporaryResourceSize;
  if (temp_resource_size > 0) {
    ComPtr<ID3D12Resource> temp_resource;
    RETURN_IF_FAILED(CreateDefaultBuffer(temp_resource_size, temp_resource));
    DML_BUFFER_BINDING temp_buffer_binding{.Buffer = temp_resource.Get(),
                                           .Offset = 0,
                                           .SizeInBytes = temp_resource_size};
    DML_BINDING_DESC temp_binding_desc{.Type = DML_BINDING_TYPE_BUFFER,
                                       .Desc = &temp_buffer_binding};
    binding_table->BindTemporaryResource(&temp_binding_desc);
    command_queue_->ReferenceUntilCompleted(std::move(temp_resource));
  }

  // The persistent resource should be bound if the operator execution requires.
  auto persistent_buffer_size =
      execution_binding_properties.PersistentResourceSize;
  if (persistent_buffer_size > 0) {
    CHECK_EQ(persistent_resource_binding.has_value(), true);
    CHECK_EQ(persistent_resource_binding.value().Type, DML_BINDING_TYPE_BUFFER);
    binding_table->BindPersistentResource(&persistent_resource_binding.value());
  }

  // Bind the input and output resources.
  binding_table->BindInputs(base::checked_cast<uint32_t>(input_bindings.size()),
                            input_bindings.data());
  binding_table->BindOutputs(
      base::checked_cast<uint32_t>(output_bindings.size()),
      output_bindings.data());

  // Dispatch the execution of the compiled operator.
  command_recorder_->RecordDispatch(command_list_.Get(), compiled_operator,
                                    binding_table.Get());

  // It's safe to release the binding table right after the dispatch has been
  // recorded into the command list. However, the heap which is referred to by
  // the GPU descriptor handle should be kept alive until all work referencing
  // it has completed execution on the GPU.
  command_queue_->ReferenceUntilCompleted(std::move(descriptor_heap));

  return S_OK;
}

HRESULT CommandRecorder::CreateDefaultBuffer(uint64_t size,
                                             ComPtr<ID3D12Resource>& resource) {
  auto heap_properties = CreateHeapProperties(D3D12_HEAP_TYPE_DEFAULT);
  auto resource_desc =
      CreateResourceDesc(size, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
  RETURN_IF_FAILED(d3d12_device_->CreateCommittedResource(
      &heap_properties, D3D12_HEAP_FLAG_NONE, &resource_desc,
      D3D12_RESOURCE_STATE_UNORDERED_ACCESS, nullptr, IID_PPV_ARGS(&resource)));
  CHECK(resource.Get());
  return S_OK;
}

HRESULT CommandRecorder::CreateUploadBuffer(uint64_t size,
                                            ComPtr<ID3D12Resource>& resource) {
  auto heap_properties = CreateHeapProperties(D3D12_HEAP_TYPE_UPLOAD);
  auto resource_desc = CreateResourceDesc(size, D3D12_RESOURCE_FLAG_NONE);
  RETURN_IF_FAILED(d3d12_device_->CreateCommittedResource(
      &heap_properties, D3D12_HEAP_FLAG_NONE, &resource_desc,
      D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&resource)));
  CHECK(resource.Get());
  return S_OK;
}

HRESULT CommandRecorder::CreateReadbackBuffer(
    uint64_t size,
    ComPtr<ID3D12Resource>& resource) {
  auto heap_properties = CreateHeapProperties(D3D12_HEAP_TYPE_READBACK);
  auto resource_desc = CreateResourceDesc(size, D3D12_RESOURCE_FLAG_NONE);
  RETURN_IF_FAILED(d3d12_device_->CreateCommittedResource(
      &heap_properties, D3D12_HEAP_FLAG_NONE, &resource_desc,
      D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&resource)));
  CHECK(resource.Get());
  return S_OK;
}

}  // namespace webnn::dml
