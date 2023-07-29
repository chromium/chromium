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

Microsoft::WRL::ComPtr<ID3D11Texture2D> OpenSharedHandleTexture(
    Microsoft::WRL::ComPtr<ID3D11Device> d3d11_device,
    HANDLE shared_handle) {
  Microsoft::WRL::ComPtr<ID3D11Device1> d3d11_device1;
  HRESULT hr = d3d11_device.As(&d3d11_device1);
  if (FAILED(hr)) {
    LOG(ERROR) << "Failed to query for ID3D11Device1. Error: "
               << logging::SystemErrorCodeToString(hr);
    return nullptr;
  }

  Microsoft::WRL::ComPtr<ID3D11Texture2D> d3d11_texture;
  hr = d3d11_device1->OpenSharedResource1(shared_handle,
                                          IID_PPV_ARGS(&d3d11_texture));
  if (FAILED(hr)) {
    LOG(ERROR) << "Unable to open shared resource from DXGI handle. Error: "
               << logging::SystemErrorCodeToString(hr);
    return nullptr;
  }

  return d3d11_texture;
}

}  // namespace

DXGISharedHandleState::D3D11TextureState::D3D11TextureState(
    Microsoft::WRL::ComPtr<ID3D11Texture2D> texture)
    : d3d11_texture(std::move(texture)) {
  d3d11_texture.As(&dxgi_keyed_mutex);
}
DXGISharedHandleState::D3D11TextureState::~D3D11TextureState() {
  CHECK_EQ(keyed_mutex_acquired_count, 0);
}
DXGISharedHandleState::D3D11TextureState::D3D11TextureState(
    D3D11TextureState&&) = default;
DXGISharedHandleState::D3D11TextureState&
DXGISharedHandleState::D3D11TextureState::operator=(D3D11TextureState&&) =
    default;

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
      shared_handle_(std::move(shared_handle)) {
  CHECK(d3d11_texture);
  Microsoft::WRL::ComPtr<ID3D11Device> d3d11_device;
  d3d11_texture->GetDevice(&d3d11_device);
  CHECK(d3d11_device);

  D3D11TextureState texture_state(std::move(d3d11_texture));
  has_keyed_mutex_ = !!texture_state.dxgi_keyed_mutex;
  d3d11_texture_state_map_.emplace(d3d11_device, std::move(texture_state));
}

DXGISharedHandleState::~DXGISharedHandleState() {
  for (auto& [_, d3d11_state] : d3d11_texture_state_map_) {
    if (d3d11_state.keyed_mutex_acquired_count > 0) {
      CHECK(d3d11_state.dxgi_keyed_mutex);
      const HRESULT hr =
          d3d11_state.dxgi_keyed_mutex->ReleaseSync(kDXGIKeyedMutexAcquireKey);
      if (FAILED(hr)) {
        LOG(ERROR) << "Unable to release the keyed mutex " << std::hex << hr;
      }
    }
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

Microsoft::WRL::ComPtr<ID3D11Texture2D>
DXGISharedHandleState::GetOrCreateD3D11Texture(
    Microsoft::WRL::ComPtr<ID3D11Device> d3d11_device) {
  auto it = d3d11_texture_state_map_.find(d3d11_device);
  if (it == d3d11_texture_state_map_.end()) {
    auto d3d11_texture =
        OpenSharedHandleTexture(d3d11_device, shared_handle_.Get());
    if (!d3d11_texture) {
      LOG(ERROR) << "Failed to open DXGI shared handle";
      return nullptr;
    }
    // TODO(sunnyps): Consider adding a method to cleanup entries in the map.
    d3d11_texture_state_map_.emplace(std::move(d3d11_device),
                                     D3D11TextureState(d3d11_texture));
    return d3d11_texture;
  }
  return it->second.d3d11_texture;
}

bool DXGISharedHandleState::BeginAccessD3D11(
    Microsoft::WRL::ComPtr<ID3D11Device> d3d11_device) {
  if (!has_keyed_mutex_) {
    return true;
  }
  auto& d3d11_state = d3d11_texture_state_map_.at(d3d11_device);
  CHECK(d3d11_state.dxgi_keyed_mutex);
  CHECK_GE(d3d11_state.keyed_mutex_acquired_count, 0);

  // Keyed mutex is acquired on |d3d11_device|. Simply increment acquired count.
  if (d3d11_state.keyed_mutex_acquired_count > 0) {
    d3d11_state.keyed_mutex_acquired_count++;
    return true;
  }

  // Keyed mutex is acquired on another device which is not permitted.
  if (keyed_mutex_acquired_) {
    LOG(ERROR) << "Concurrent keyed mutex access not supported";
    return false;
  }

  const HRESULT hr = d3d11_state.dxgi_keyed_mutex->AcquireSync(
      kDXGIKeyedMutexAcquireKey, INFINITE);
  if (FAILED(hr)) {
    LOG(ERROR) << "Unable to acquire the keyed mutex " << std::hex << hr;
    return false;
  }
  d3d11_state.keyed_mutex_acquired_count++;
  keyed_mutex_acquired_ = true;
  return true;
}

void DXGISharedHandleState::EndAccessD3D11(
    Microsoft::WRL::ComPtr<ID3D11Device> d3d11_device) {
  if (!has_keyed_mutex_) {
    return;
  }
  CHECK(keyed_mutex_acquired_);

  auto& d3d11_state = d3d11_texture_state_map_.at(d3d11_device);
  CHECK(d3d11_state.dxgi_keyed_mutex);
  CHECK_GT(d3d11_state.keyed_mutex_acquired_count, 0);

  d3d11_state.keyed_mutex_acquired_count--;

  if (d3d11_state.keyed_mutex_acquired_count == 0) {
    const HRESULT hr =
        d3d11_state.dxgi_keyed_mutex->ReleaseSync(kDXGIKeyedMutexAcquireKey);
    if (SUCCEEDED(hr)) {
      keyed_mutex_acquired_ = false;
    } else {
      LOG(ERROR) << "Unable to release the keyed mutex " << std::hex << hr;
    }
  }
}

#if BUILDFLAG(USE_DAWN)
DXGISharedHandleState::DawnExternalImageState::DawnExternalImageState() =
    default;
DXGISharedHandleState::DawnExternalImageState::~DawnExternalImageState() =
    default;
DXGISharedHandleState::DawnExternalImageState::DawnExternalImageState(
    DawnExternalImageState&&) = default;
DXGISharedHandleState::DawnExternalImageState&
DXGISharedHandleState::DawnExternalImageState::operator=(
    DawnExternalImageState&&) = default;

bool DXGISharedHandleState::BeginAccessDawn(WGPUDevice device) {
  auto& dawn_state = dawn_external_image_cache_.at(device);
  CHECK_GE(dawn_state.access_count, 0);
  // If a keyed mutex is present it's already acquired on |device| so just
  // increment the access count.
  if (dawn_state.access_count > 0) {
    dawn_state.access_count++;
    return true;
  }
  // Keyed mutex is acquired on another device which is not permitted.
  if (keyed_mutex_acquired_) {
    LOG(ERROR) << "Concurrent keyed mutex access not supported";
    return false;
  }
  dawn_state.access_count++;
  // The keyed mutex is actually acquired internally in Dawn, but we do extra
  // tracking here for preventing concurrent access.
  keyed_mutex_acquired_ = has_keyed_mutex_;
  return true;
}

void DXGISharedHandleState::EndAccessDawn(WGPUDevice device) {
  auto& dawn_state = dawn_external_image_cache_.at(device);
  CHECK_GT(dawn_state.access_count, 0);
  CHECK_EQ(has_keyed_mutex_, keyed_mutex_acquired_);

  dawn_state.access_count--;

  if (dawn_state.access_count == 0) {
    if (!dawn_state.external_image) {
      // Clear entry from the map if the backing has already reset the external
      // image e.g. due to device destruction or loss.
      dawn_external_image_cache_.erase(device);
    }
    // The keyed mutex is actually released internally in Dawn, but we do extra
    // tracking here for preventing concurrent access.
    keyed_mutex_acquired_ = false;
  }
}

std::unique_ptr<ExternalImageDXGI>& DXGISharedHandleState::GetDawnExternalImage(
    WGPUDevice device) {
  return dawn_external_image_cache_[device].external_image;
}
#endif  // BUILDFLAG(USE_DAWN)

DXGISharedHandleManager::DXGISharedHandleManager() = default;

DXGISharedHandleManager::~DXGISharedHandleManager() {
#if DCHECK_IS_ON()
  base::AutoLock auto_lock(lock_);
  DCHECK(shared_handle_state_map_.empty());
#endif
}

scoped_refptr<DXGISharedHandleState>
DXGISharedHandleManager::GetOrCreateSharedHandleState(
    gfx::DXGIHandleToken token,
    base::win::ScopedHandle shared_handle,
    Microsoft::WRL::ComPtr<ID3D11Device> d3d11_device) {
  DCHECK(shared_handle.IsValid());

  base::AutoLock auto_lock(lock_);

  auto it = shared_handle_state_map_.find(token);
  if (it != shared_handle_state_map_.end()) {
    DXGISharedHandleState* state = it->second;
    DCHECK(state);
    // If there's already a shared handle associated with the token, it should
    // refer to the same D3D11 texture (or kernel object).
    if (!IsSameHandle(shared_handle.Get(), state->GetSharedHandle())) {
      LOG(ERROR) << "Existing shared handle for token doesn't match";
      return nullptr;
    }
    return base::WrapRefCounted(state);
  }

  auto d3d11_texture =
      OpenSharedHandleTexture(d3d11_device, shared_handle.Get());
  if (!d3d11_texture) {
    LOG(ERROR) << "Failed to open DXGI shared handle";
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
