// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/windows/d3d12_copy_command_list_wrapper.h"

#include "third_party/microsoft_dxheaders/src/include/directx/d3dx12_barriers.h"

namespace media {

// static
std::unique_ptr<D3D12CopyCommandQueueWrapper>
D3D12CopyCommandQueueWrapper::Create(ID3D12Device* device) {
  Microsoft::WRL::ComPtr<ID3D12CommandQueue> command_queue;
  D3D12_COMMAND_QUEUE_DESC command_queue_desc{D3D12_COMMAND_LIST_TYPE_COPY};
  HRESULT hr = device->CreateCommandQueue(&command_queue_desc,
                                          IID_PPV_ARGS(&command_queue));
  if (FAILED(hr)) {
    LOG(ERROR) << "Failed to create d3d12 copy command queue: "
               << logging::SystemErrorCodeToString(hr);
    return nullptr;
  }

  Microsoft::WRL::ComPtr<ID3D12CommandAllocator> command_allocator;
  hr = device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_COPY,
                                      IID_PPV_ARGS(&command_allocator));
  if (FAILED(hr)) {
    LOG(ERROR) << "Failed to create d3d12 copy command allocator: "
               << logging::SystemErrorCodeToString(hr);
    return nullptr;
  }

  Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> command_list;
  hr = device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_COPY,
                                 command_allocator.Get(), nullptr,
                                 IID_PPV_ARGS(&command_list));
  if (FAILED(hr)) {
    LOG(ERROR) << "Failed to create d3d12 copy command list: "
               << logging::SystemErrorCodeToString(hr);
    return nullptr;
  }

  Microsoft::WRL::ComPtr<ID3D12Fence> fence;
  hr = device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence));
  if (FAILED(hr)) {
    LOG(ERROR) << "Failed to create d3d12 fence: "
               << logging::SystemErrorCodeToString(hr);
    return nullptr;
  }

  return std::make_unique<D3D12CopyCommandQueueWrapper>(
      command_queue, command_allocator, command_list, fence);
}

D3D12CopyCommandQueueWrapper::D3D12CopyCommandQueueWrapper(
    Microsoft::WRL::ComPtr<ID3D12CommandQueue> command_queue,
    Microsoft::WRL::ComPtr<ID3D12CommandAllocator> command_allocator,
    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> command_list,
    Microsoft::WRL::ComPtr<ID3D12Fence> fence)
    : command_queue_(std::move(command_queue)),
      command_allocator_(std::move(command_allocator)),
      command_list_(std::move(command_list)),
      fence_(base::MakeRefCounted<D3D12Fence>(fence)) {}

D3D12CopyCommandQueueWrapper::~D3D12CopyCommandQueueWrapper() = default;

void D3D12CopyCommandQueueWrapper::CopyTextureRegion(
    const D3D12_TEXTURE_COPY_LOCATION& dst_location,
    uint32_t x,
    uint32_t y,
    uint32_t z,
    const D3D12_TEXTURE_COPY_LOCATION& src_location,
    const D3D12_BOX* src_box) {
  command_list_->CopyTextureRegion(&dst_location, x, y, z, &src_location,
                                   src_box);
}

void D3D12CopyCommandQueueWrapper::CopyBufferToNV12Texture(
    ID3D12Resource* target_texture,
    ID3D12Resource* source_buffer,
    uint32_t y_offset,
    uint32_t y_stride,
    uint32_t uv_offset,
    uint32_t uv_stride) {
  CHECK_EQ(source_buffer->GetDesc().Dimension, D3D12_RESOURCE_DIMENSION_BUFFER);
  D3D12_RESOURCE_DESC target_texture_desc = target_texture->GetDesc();

  // The COPY_SOURCE/COPY_DEST states can be promote/decayed. So the resource
  // barriers are not necessary here. See
  // https://learn.microsoft.com/en-us/windows/win32/direct3d12/using-resource-barriers-to-synchronize-resource-states-in-direct3d-12#common-state-promotion
  CopyTextureRegion(
      {.pResource = target_texture,
       .Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX,
       .SubresourceIndex = 0},
      0, 0, 0,
      {.pResource = source_buffer,
       .Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT,
       .PlacedFootprint = {
           .Offset = y_offset,
           .Footprint = {.Format = DXGI_FORMAT_R8_TYPELESS,
                         .Width = static_cast<UINT>(target_texture_desc.Width),
                         .Height = target_texture_desc.Height,
                         .Depth = 1,
                         .RowPitch = y_stride},
       }});
  CopyTextureRegion(
      {.pResource = target_texture,
       .Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX,
       .SubresourceIndex = 1},
      0, 0, 0,
      {.pResource = source_buffer,
       .Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT,
       .PlacedFootprint = {
           .Offset = uv_offset,
           .Footprint = {.Format = DXGI_FORMAT_R8G8_TYPELESS,
                         .Width =
                             static_cast<UINT>(target_texture_desc.Width + 1) /
                             2,
                         .Height = (target_texture_desc.Height + 1) / 2,
                         .Depth = 1,
                         .RowPitch = uv_stride},
       }});
}

bool D3D12CopyCommandQueueWrapper::ExecuteAndWait() {
  HRESULT hr = command_list_->Close();
  if (FAILED(hr)) {
    LOG(ERROR) << "Failed to close d3d12 copy command list: "
               << logging::SystemErrorCodeToString(hr);
    return false;
  }

  ID3D12CommandList* command_lists[] = {command_list_.Get()};
  command_queue_->ExecuteCommandLists(1, command_lists);
  D3D11Status status = fence_->SignalAndWait(*command_queue_.Get());
  if (!status.is_ok()) {
    LOG(ERROR) << "Failed to SignalAndWait: " << status.message();
    return false;
  }
  return Reset();
}

bool D3D12CopyCommandQueueWrapper::Reset() {
  HRESULT hr = command_allocator_->Reset();
  if (FAILED(hr)) {
    LOG(ERROR) << "Failed to reset d3d12 copy command allocator: "
               << logging::SystemErrorCodeToString(hr);
    return false;
  }
  hr = command_list_->Reset(command_allocator_.Get(), nullptr);
  if (FAILED(hr)) {
    LOG(ERROR) << "Failed to reset d3d12 copy command list: "
               << logging::SystemErrorCodeToString(hr);
    return false;
  }
  return true;
}

}  // namespace media
