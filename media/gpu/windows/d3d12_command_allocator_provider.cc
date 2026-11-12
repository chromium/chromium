// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/windows/d3d12_command_allocator_provider.h"

#include <utility>

namespace media {

D3D12CommandAllocatorProvider::D3D12CommandAllocatorProvider(
    ComD3D12Device device,
    scoped_refptr<D3D12Fence> fence)
    : device_(std::move(device)), fence_(std::move(fence)) {}

D3D12CommandAllocatorProvider::~D3D12CommandAllocatorProvider() = default;

D3DStatus D3D12CommandAllocatorProvider::CreateAllocator(Allocator& allocator) {
  HRESULT hr = device_->CreateCommandAllocator(
      D3D12_COMMAND_LIST_TYPE_VIDEO_DECODE,
      IID_PPV_ARGS(&allocator.command_allocator));
  if (FAILED(hr)) {
    return D3DStatus{D3DStatusCode::kDecoderBeginFrameFailed,
                     "Failed to create command allocator", hr};
  }
  return D3DStatusCode::kOk;
}

D3DStatus D3D12CommandAllocatorProvider::ResetAllocator(Allocator& allocator) {
  D3DStatus status = fence_->WaitCPU(allocator.last_submitted_fence_value);
  if (!status.is_ok()) {
    return status;
  }
  HRESULT hr = allocator.command_allocator->Reset();
  if (FAILED(hr)) {
    return D3DStatus{D3DStatusCode::kDecoderBeginFrameFailed,
                     "Failed to reset command allocator", hr};
  }
  allocator.num_frames = 0;
  return D3DStatusCode::kOk;
}

base::expected<ID3D12CommandAllocator*, D3DStatus>
D3D12CommandAllocatorProvider::PrepareCommandAllocator() {
  D3DStatus status = D3DStatusCode::kOk;
  if (!allocators_[index_].command_allocator) {
    status = CreateAllocator(allocators_[index_]);
  } else if (fence_->GetCompletedValue() >=
             allocators_[index_].last_submitted_fence_value) {
    // The GPU has completed all the commands in the allocator.
    status = ResetAllocator(allocators_[index_]);
  } else if (allocators_[index_].num_frames >= kMaxFramesPerAllocator) {
    // The GPU does not catch up. Switch to the next allocator, whose commands
    // were submitted long ago so that normally no waiting is needed.
    size_t next_index = (index_ + 1) % allocators_.size();
    status = allocators_[next_index].command_allocator
                 ? ResetAllocator(allocators_[next_index])
                 : CreateAllocator(allocators_[next_index]);
    if (status.is_ok()) {
      index_ = next_index;
    }
  }
  if (!status.is_ok()) {
    return base::unexpected(std::move(status));
  }

  allocators_[index_].num_frames++;
  return allocators_[index_].command_allocator.Get();
}

void D3D12CommandAllocatorProvider::OnSubmitted(uint64_t fence_value) {
  allocators_[index_].last_submitted_fence_value = fence_value;
}

}  // namespace media
