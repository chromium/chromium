// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/windows/d3d12_fence.h"

#include "base/logging.h"
#include "base/threading/scoped_blocking_call.h"
#include "base/win/scoped_handle.h"

namespace media {

D3D12Fence::D3D12Fence(ComD3D12Fence fence) : fence_(std::move(fence)) {}

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

D3D11Status::Or<uint64_t> D3D12Fence::Signal(
    ID3D12CommandQueue& command_queue) {
  HRESULT hr = command_queue.Signal(fence_.Get(), ++fence_value_);
  if (FAILED(hr)) {
    return D3D11Status{D3D11StatusCode::kFenceSignalFailed,
                       "ID3D12CommandQueue failed to signal fence", hr};
  }
  return fence_value_;
}

D3D11Status D3D12Fence::Wait(uint64_t fence_value) const {
  if (fence_->GetCompletedValue() >= fence_value) {
    return D3D11StatusCode::kOk;
  }
  base::win::ScopedHandle fence_event{::CreateEvent(
      nullptr, /*bManualReset=*/TRUE, /*bInitialState=*/FALSE, nullptr)};
  HRESULT hr = fence_->SetEventOnCompletion(fence_value_, fence_event.get());
  if (FAILED(hr)) {
    return D3D11Status{D3D11StatusCode::kWaitForFenceFailed,
                       "Failed to SetEventOnCompletion", hr};
  }

  return WaitForSingleObject(fence_event.Get(), INFINITE) == WAIT_OBJECT_0
             ? D3D11StatusCode::kOk
             : D3D11StatusCode::kWaitForFenceFailed;
}

D3D11Status D3D12Fence::SignalAndWait(ID3D12CommandQueue& command_queue) {
  auto fence_value_or_error = Signal(command_queue);
  if (!fence_value_or_error.has_value()) {
    return std::move(fence_value_or_error).error();
  }
  return Wait(std::move(fence_value_or_error).value());
}

D3D12Fence::~D3D12Fence() = default;

}  // namespace media
