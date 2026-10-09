// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/windows/d3d12_fence.h"

#include "base/check_is_test.h"
#include "base/logging.h"
#include "base/threading/scoped_blocking_call.h"
#include "base/win/scoped_handle.h"
#include "ui/gfx/win/d3d_shared_fence.h"

namespace media {

D3D12Fence::D3D12Fence(ComD3D12Fence fence) : fence_(std::move(fence)) {
  CHECK(fence_);
}

// static
scoped_refptr<D3D12Fence> D3D12Fence::Create(ID3D12Device* device,
                                             D3D12_FENCE_FLAGS flags) {
  ComD3D12Fence d3d12_fence;
  HRESULT hr = device->CreateFence(0, flags, IID_PPV_ARGS(&d3d12_fence));
  if (FAILED(hr)) {
    DLOG(ERROR) << "Failed to create D3D12Fence: "
                << logging::SystemErrorCodeToString(hr);
    return nullptr;
  }
  return base::MakeRefCounted<D3D12Fence>(std::move(d3d12_fence));
}

ID3D12Fence* D3D12Fence::Get() const {
  return fence_.Get();
}

scoped_refptr<gfx::D3DSharedFence> D3D12Fence::CreateSharedFence(
    uint64_t fence_value) const {
  return gfx::D3DSharedFence::CreateFromD3D12Fence(fence_, fence_value);
}

uint64_t D3D12Fence::Value() const {
  return fence_value_;
}

uint64_t D3D12Fence::GetCompletedValue() const {
  return fence_->GetCompletedValue();
}

D3DStatus::Or<uint64_t> D3D12Fence::Signal(ID3D12CommandQueue& command_queue) {
  uint64_t next_value = fence_value_ + 1;
  HRESULT hr = command_queue.Signal(fence_.Get(), next_value);
  if (FAILED(hr)) {
    return D3DStatus{D3DStatusCode::kFenceSignalFailed,
                     "ID3D12CommandQueue failed to signal fence", hr};
  }
  fence_value_ = next_value;
  return fence_value_;
}

D3DStatus D3D12Fence::WaitCPU(uint64_t fence_value) const {
  if (fence_->GetCompletedValue() >= fence_value) {
    return D3DStatusCode::kOk;
  }
  base::win::ScopedHandle fence_event{::CreateEvent(
      nullptr, /*bManualReset=*/TRUE, /*bInitialState=*/FALSE, nullptr)};
  HRESULT hr = fence_->SetEventOnCompletion(fence_value, fence_event.get());
  if (FAILED(hr)) {
    return D3DStatus{D3DStatusCode::kWaitForFenceFailed,
                     "Failed to SetEventOnCompletion", hr};
  }

  return WaitForSingleObject(fence_event.Get(), INFINITE) == WAIT_OBJECT_0
             ? D3DStatusCode::kOk
             : D3DStatusCode::kWaitForFenceFailed;
}

D3DStatus D3D12Fence::SignalAndWaitCPU(ID3D12CommandQueue& command_queue) {
  auto fence_value_or_error = Signal(command_queue);
  if (!fence_value_or_error.has_value()) {
    return std::move(fence_value_or_error).error();
  }
  return WaitCPU(std::move(fence_value_or_error).value());
}

D3D12Fence::~D3D12Fence() = default;

}  // namespace media
