// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/shared_image/d3d_shared_fence.h"

#include "base/logging.h"
#include "base/notreached.h"

namespace gpu {
namespace {
Microsoft::WRL::ComPtr<ID3D11DeviceContext4> GetDeviceContext4(
    ID3D11Device* d3d11_device) {
  Microsoft::WRL::ComPtr<ID3D11DeviceContext> context;
  d3d11_device->GetImmediateContext(&context);

  Microsoft::WRL::ComPtr<ID3D11DeviceContext4> context4;
  HRESULT hr = context.As(&context4);
  if (FAILED(hr)) {
    DLOG(ERROR) << "Failed to get ID3D11DeviceContext4: 0x" << std::hex << hr;
    return nullptr;
  }
  return context4;
}
}  // namespace

// static
scoped_refptr<D3DSharedFence> D3DSharedFence::CreateForD3D11(
    Microsoft::WRL::ComPtr<ID3D11Device> d3d11_device) {
  Microsoft::WRL::ComPtr<ID3D11Device5> d3d11_device5;
  HRESULT hr = d3d11_device.As(&d3d11_device5);
  if (FAILED(hr)) {
    DLOG(ERROR) << "Failed to get ID3D11Device5: 0x" << std::hex << hr;
    return nullptr;
  }

  Microsoft::WRL::ComPtr<ID3D11Fence> d3d11_fence;
  hr = d3d11_device5->CreateFence(0, D3D11_FENCE_FLAG_SHARED,
                                  IID_PPV_ARGS(&d3d11_fence));
  if (FAILED(hr)) {
    DLOG(ERROR) << "CreateFence failed with error 0x" << std::hex << hr;
    return nullptr;
  }

  HANDLE shared_handle = nullptr;
  hr = d3d11_fence->CreateSharedHandle(nullptr, GENERIC_ALL, nullptr,
                                       &shared_handle);
  if (FAILED(hr)) {
    DLOG(ERROR) << "Unable to create shared handle for D3D11Fence: 0x"
                << std::hex << hr;
    return nullptr;
  }
  auto fence = base::WrapRefCounted(
      new D3DSharedFence(base::win::ScopedHandle(shared_handle)));
  fence->d3d11_device_ = std::move(d3d11_device);
  fence->d3d11_signal_fence_ = std::move(d3d11_fence);
  return fence;
}

// static
bool D3DSharedFence::IsSupported(ID3D11Device* d3d11_device) {
  DCHECK(d3d11_device);
  Microsoft::WRL::ComPtr<ID3D11Device5> d3d11_device5;
  HRESULT hr = d3d11_device->QueryInterface(IID_PPV_ARGS(&d3d11_device5));
  if (FAILED(hr)) {
    DVLOG(1) << "Failed to get ID3D11Device5: 0x" << std::hex << hr;
    return false;
  }
  return true;
}

// static
scoped_refptr<D3DSharedFence> D3DSharedFence::CreateFromHandle(
    HANDLE shared_handle) {
  HANDLE dup_handle = nullptr;
  if (!::DuplicateHandle(::GetCurrentProcess(), shared_handle,
                         ::GetCurrentProcess(), &dup_handle, 0, FALSE,
                         DUPLICATE_SAME_ACCESS)) {
    DLOG(ERROR) << "DuplicateHandle failed: 0x" << std::hex << ::GetLastError();
    return nullptr;
  }
  return base::WrapRefCounted(
      new D3DSharedFence(base::win::ScopedHandle(dup_handle)));
}

D3DSharedFence::D3DSharedFence(base::win::ScopedHandle shared_handle)
    : shared_handle_(std::move(shared_handle)),
      d3d11_wait_fence_map_(kMaxD3D11FenceMapSize) {}

D3DSharedFence::~D3DSharedFence() = default;

HANDLE D3DSharedFence::GetSharedHandle() const {
  return shared_handle_.get();
}

uint64_t D3DSharedFence::GetFenceValue() const {
  return fence_value_;
}

bool D3DSharedFence::IsSameFenceAsHandle(HANDLE shared_handle) const {
  using PFN_COMPARE_OBJECT_HANDLES =
      BOOL(WINAPI*)(HANDLE hFirstObjectHandle, HANDLE hSecondObjectHandle);
  static PFN_COMPARE_OBJECT_HANDLES compare_object_handles_fn =
      []() -> PFN_COMPARE_OBJECT_HANDLES {
    HMODULE kernelbase_module = ::GetModuleHandle(L"kernelbase.dll");
    if (!kernelbase_module) {
      DVLOG(1) << "kernelbase.dll not found";
      return nullptr;
    }
    PFN_COMPARE_OBJECT_HANDLES fn =
        reinterpret_cast<PFN_COMPARE_OBJECT_HANDLES>(
            ::GetProcAddress(kernelbase_module, "CompareObjectHandles"));
    if (!fn)
      DVLOG(1) << "CompareObjectHandles not found";
    return fn;
  }();
  if (compare_object_handles_fn)
    return compare_object_handles_fn(shared_handle_.Get(), shared_handle);
  // CompareObjectHandles isn't available before Windows 10 but we shouldn't be
  // using fences in that case either.
  NOTREACHED();
  return false;
}

void D3DSharedFence::Update(uint64_t fence_value) {
  if (fence_value > fence_value_)
    fence_value_ = fence_value;
}

bool D3DSharedFence::WaitD3D11(
    Microsoft::WRL::ComPtr<ID3D11Device> d3d11_device) {
  // Skip wait if passed in device is the same as signaling device.
  if (d3d11_device == d3d11_device_)
    return true;

  auto it = d3d11_wait_fence_map_.Get(d3d11_device);
  if (it == d3d11_wait_fence_map_.end()) {
    Microsoft::WRL::ComPtr<ID3D11Device5> d3d11_device5;
    HRESULT hr = d3d11_device.As(&d3d11_device5);
    if (FAILED(hr)) {
      DLOG(ERROR) << "Failed to get ID3D11Device5: 0x" << std::hex << hr;
      return false;
    }
    Microsoft::WRL::ComPtr<ID3D11Fence> d3d11_fence;
    hr = d3d11_device5->OpenSharedFence(shared_handle_.get(),
                                        IID_PPV_ARGS(&d3d11_fence));
    if (FAILED(hr)) {
      DLOG(ERROR) << "OpenSharedFence failed: 0x" << std::hex << hr;
      return false;
    }
    it = d3d11_wait_fence_map_.Put(d3d11_device, d3d11_fence);
  }

  Microsoft::WRL::ComPtr<ID3D11DeviceContext4> context4 =
      GetDeviceContext4(d3d11_device.Get());
  if (!context4)
    return false;

  const Microsoft::WRL::ComPtr<ID3D11Fence>& fence = it->second;
  HRESULT hr = context4->Wait(fence.Get(), fence_value_);
  if (FAILED(hr)) {
    DLOG(ERROR) << "D3D11 fence wait failed: 0x" << std::hex << hr;
    return false;
  }
  return true;
}

bool D3DSharedFence::IncrementAndSignalD3D11() {
  DCHECK(d3d11_device_);
  DCHECK(d3d11_signal_fence_);

  Microsoft::WRL::ComPtr<ID3D11DeviceContext4> context4 =
      GetDeviceContext4(d3d11_device_.Get());
  if (!context4)
    return false;

  HRESULT hr = context4->Signal(d3d11_signal_fence_.Get(), fence_value_ + 1);
  if (FAILED(hr)) {
    DLOG(ERROR) << "D3D11 fence signal failed: 0x" << std::hex << hr;
    return false;
  }
  fence_value_++;
  return true;
}

}  // namespace gpu
