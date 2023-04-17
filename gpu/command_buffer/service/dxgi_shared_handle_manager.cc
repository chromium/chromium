// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/dxgi_shared_handle_manager.h"

#include <d3d11_1.h>
#include <windows.h>

#include "base/atomic_ref_count.h"
#include "base/logging.h"
#include "gpu/command_buffer/common/constants.h"
#include "ui/gl/gl_angle_util_win.h"

namespace gpu {

namespace {

bool IsSameHandle(HANDLE handle, HANDLE other) {
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
    return compare_object_handles_fn(handle, other);
  // CompareObjectHandles isn't available before Windows 10. Return true in that
  // case since there's no other way to check if the handles refer to the same
  // D3D11 texture and IsSameHandle is only used as a sanity test.
  return true;
}

}  // namespace

DXGISharedHandleState::DXGISharedHandleState(
    base::PassKey<DXGISharedHandleManager>,
    scoped_refptr<DXGISharedHandleManager> manager,
    gfx::DXGIHandleToken token,
    base::win::ScopedHandle shared_handle,
    Microsoft::WRL::ComPtr<ID3D11Texture2D> d3d11_texture)
    : base::subtle::RefCountedThreadSafeBase(
          base::subtle::GetRefCountPreference<DXGISharedHandleState>()),
      manager_(std::move(manager)),
      token_(std::move(token)),
      shared_handle_(std::move(shared_handle)),
      d3d11_texture_(std::move(d3d11_texture)) {
  d3d11_texture_.As(&dxgi_keyed_mutex_);
}

DXGISharedHandleState::~DXGISharedHandleState() {
  if (acquired_for_d3d11_count_ > 0) {
    EndAccessD3D11();
  }
  if (acquired_for_d3d12_) {
    EndAccessD3D12();
  }
}

void DXGISharedHandleState::AddRef() const {
  base::subtle::RefCountedThreadSafeBase::AddRef();
}

void DXGISharedHandleState::Release() const {
  // Hold the lock to prevent a race between erasing the state from the map and
  // adding another reference to it. If GetOrCreateStateByToken runs before we
  // erase the state from the map, we would return a scoped_refptr that will
  // have a dangling pointer to the state after the call to delete causing a
  // use-after-free when the scoped_refptr is later dereferenced.
  base::AutoLock auto_lock(manager_->lock_);
  if (base::subtle::RefCountedThreadSafeBase::Release()) {
    // Prune the code paths which the static analyzer may take to simulate
    // object destruction. Use-after-free errors aren't possible given the
    // lifetime guarantees of the refcounting system.
    ANALYZER_SKIP_THIS_PATH();
    manager_->shared_handle_state_map_.erase(token_);
    delete this;
  }
}

bool DXGISharedHandleState::BeginAccessD3D11() {
  if (!dxgi_keyed_mutex_)
    return true;

  if (acquired_for_d3d12_) {
    DLOG(ERROR) << "Recursive BeginAccess not supported";
    return false;
  }
  if (acquired_for_d3d11_count_ > 0) {
    acquired_for_d3d11_count_++;
    return true;
  }
  const HRESULT hr =
      dxgi_keyed_mutex_->AcquireSync(kDXGIKeyedMutexAcquireKey, INFINITE);
  if (FAILED(hr)) {
    DLOG(ERROR) << "Unable to acquire the keyed mutex " << std::hex << hr;
    return false;
  }
  acquired_for_d3d11_count_++;
  return true;
}

void DXGISharedHandleState::EndAccessD3D11() {
  if (!dxgi_keyed_mutex_)
    return;

  DCHECK_GT(acquired_for_d3d11_count_, 0);
  acquired_for_d3d11_count_--;
  if (acquired_for_d3d11_count_ == 0) {
    const HRESULT hr =
        dxgi_keyed_mutex_->ReleaseSync(kDXGIKeyedMutexAcquireKey);
    if (FAILED(hr))
      DLOG(ERROR) << "Unable to release the keyed mutex " << std::hex << hr;
  }
}

bool DXGISharedHandleState::BeginAccessD3D12() {
  if (!dxgi_keyed_mutex_)
    return true;

  if (acquired_for_d3d12_ || acquired_for_d3d11_count_ > 0) {
    DLOG(ERROR) << "Recursive BeginAccess not supported";
    return false;
  }
  acquired_for_d3d12_ = true;
  return true;
}

void DXGISharedHandleState::EndAccessD3D12() {
  if (!dxgi_keyed_mutex_)
    return;
  acquired_for_d3d12_ = false;
}

DXGISharedHandleManager::DXGISharedHandleManager(
    Microsoft::WRL::ComPtr<ID3D11Device> d3d11_device)
    : d3d11_device_(std::move(d3d11_device)) {
  DCHECK(d3d11_device_);
}

DXGISharedHandleManager::~DXGISharedHandleManager() {
#if DCHECK_IS_ON()
  base::AutoLock auto_lock(lock_);
  DCHECK(shared_handle_state_map_.empty());
#endif
}

scoped_refptr<DXGISharedHandleState>
DXGISharedHandleManager::GetOrCreateSharedHandleState(
    gfx::DXGIHandleToken token,
    base::win::ScopedHandle shared_handle) {
  DCHECK(shared_handle.IsValid());

  base::AutoLock auto_lock(lock_);

  auto it = shared_handle_state_map_.find(token);
  if (it != shared_handle_state_map_.end()) {
    DXGISharedHandleState* state = it->second;
    DCHECK(state);
    // If there's already a shared handle associated with the token, it should
    // refer to the same D3D11 texture (or kernel object).
    if (!IsSameHandle(shared_handle.Get(), state->GetSharedHandle())) {
      DLOG(ERROR) << "Existing shared handle for token doesn't match";
      return nullptr;
    }
    return base::WrapRefCounted(state);
  }

  Microsoft::WRL::ComPtr<ID3D11Device1> d3d11_device1;
  HRESULT hr = d3d11_device_.As(&d3d11_device1);
  if (FAILED(hr)) {
    DLOG(ERROR) << "Failed to query for ID3D11Device1. Error: "
                << logging::SystemErrorCodeToString(hr);
    return nullptr;
  }

  Microsoft::WRL::ComPtr<ID3D11Texture2D> d3d11_texture;
  hr = d3d11_device1->OpenSharedResource1(shared_handle.Get(),
                                          IID_PPV_ARGS(&d3d11_texture));
  if (FAILED(hr)) {
    DLOG(ERROR) << "Unable to open shared resource from DXGI handle. Error: "
                << logging::SystemErrorCodeToString(hr);
    return nullptr;
  }

  auto state = base::MakeRefCounted<DXGISharedHandleState>(
      base::PassKey<DXGISharedHandleManager>(), base::WrapRefCounted(this),
      token, std::move(shared_handle), std::move(d3d11_texture));

  shared_handle_state_map_.insert(
      std::make_pair(std::move(token), state.get()));

  return state;
}

scoped_refptr<DXGISharedHandleState>
DXGISharedHandleManager::CreateAnonymousSharedHandleState(
    base::win::ScopedHandle shared_handle,
    Microsoft::WRL::ComPtr<ID3D11Texture2D> d3d11_texture) {
  DCHECK(shared_handle.IsValid());
  DCHECK(d3d11_texture);

  base::AutoLock auto_lock(lock_);

  auto token = gfx::DXGIHandleToken();

  auto state = base::MakeRefCounted<DXGISharedHandleState>(
      base::PassKey<DXGISharedHandleManager>(), base::WrapRefCounted(this),
      token, std::move(shared_handle), std::move(d3d11_texture));

  std::pair<SharedHandleMap::iterator, bool> inserted =
      shared_handle_state_map_.insert(
          std::make_pair(std::move(token), state.get()));
  DCHECK(inserted.second);

  return state;
}

size_t DXGISharedHandleManager::GetSharedHandleMapSizeForTesting() const {
  base::AutoLock auto_lock(lock_);
  return shared_handle_state_map_.size();
}

}  // namespace gpu
