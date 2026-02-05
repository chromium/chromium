// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/windows/d3d12_fence.h"

#include <d3d11_4.h>

#include "base/check_is_test.h"
#include "base/logging.h"
#include "base/threading/scoped_blocking_call.h"
#include "base/win/scoped_handle.h"

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

uint64_t D3D12Fence::Value() const {
  return fence_value_;
}

uint64_t D3D12Fence::GetCompletedValue() const {
  return fence_->GetCompletedValue();
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

D3D11Status D3D12Fence::WaitCPU(uint64_t fence_value) const {
  if (fence_->GetCompletedValue() >= fence_value) {
    return D3D11StatusCode::kOk;
  }
  base::win::ScopedHandle fence_event{::CreateEvent(
      nullptr, /*bManualReset=*/TRUE, /*bInitialState=*/FALSE, nullptr)};
  HRESULT hr = fence_->SetEventOnCompletion(fence_value, fence_event.get());
  if (FAILED(hr)) {
    return D3D11Status{D3D11StatusCode::kWaitForFenceFailed,
                       "Failed to SetEventOnCompletion", hr};
  }

  return WaitForSingleObject(fence_event.Get(), INFINITE) == WAIT_OBJECT_0
             ? D3D11StatusCode::kOk
             : D3D11StatusCode::kWaitForFenceFailed;
}

D3D11Status D3D12Fence::WaitGPU(ID3D11DeviceContext& device_context,
                                uint64_t fence_value) {
  HRESULT hr;
  if (!d3d11_fence_) {
    Microsoft::WRL::ComPtr<ID3D12Device> d3d12_device;
    CHECK_EQ(fence_->GetDevice(IID_PPV_ARGS(&d3d12_device)), S_OK);

    HANDLE handle;
    hr = d3d12_device->CreateSharedHandle(fence_.Get(), nullptr, GENERIC_ALL,
                                          nullptr, &handle);
    if (FAILED(hr)) {
      LOG(ERROR) << "Failed to create shared handle for fence: "
                 << logging::SystemErrorCodeToString(hr);
      return D3D11StatusCode::kCreateSharedHandleFailed;
    }
    base::win::ScopedHandle scoped_handle(handle);

    Microsoft::WRL::ComPtr<ID3D11Device> device;
    device_context.GetDevice(&device);
    Microsoft::WRL::ComPtr<ID3D11Device5> device5;
    // We have checked that D3D11Fence is supported in d3d11_video_decoder.cc
    CHECK_EQ(device.As(&device5), S_OK);

    hr = device5->OpenSharedFence(scoped_handle.get(),
                                  IID_PPV_ARGS(&d3d11_fence_));
    if (FAILED(hr)) {
      LOG(ERROR) << "Failed to open shared fence: "
                 << logging::SystemErrorCodeToString(hr);
      return D3D11StatusCode::kCreateFenceFailed;
    }
  }
  CHECK(d3d11_fence_);

  Microsoft::WRL::ComPtr<ID3D11DeviceContext4> device_context4;
  // We have checked that D3D11Fence is supported in d3d11_video_decoder.cc
  CHECK_EQ(device_context.QueryInterface(IID_PPV_ARGS(&device_context4)), S_OK);
  hr = device_context4->Wait(d3d11_fence_.Get(), fence_value);
  if (FAILED(hr)) {
    LOG(ERROR) << "ID3D11DeviceContext4 failed to wait for fence: "
               << logging::SystemErrorCodeToString(hr);
    return D3D11StatusCode::kWaitForFenceFailed;
  }
  return D3D11StatusCode::kOk;
}

D3D11Status D3D12Fence::SignalAndWaitCPU(ID3D12CommandQueue& command_queue) {
  auto fence_value_or_error = Signal(command_queue);
  if (!fence_value_or_error.has_value()) {
    return std::move(fence_value_or_error).error();
  }
  return WaitCPU(std::move(fence_value_or_error).value());
}

D3D12Fence::~D3D12Fence() = default;

}  // namespace media
