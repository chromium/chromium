// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_DXGI_SHARED_HANDLE_MANAGER_H_
#define GPU_COMMAND_BUFFER_SERVICE_DXGI_SHARED_HANDLE_MANAGER_H_

#include <map>

#include <d3d11.h>
#include <wrl/client.h>

#include "base/memory/ref_counted.h"
#include "base/synchronization/lock.h"
#include "base/thread_annotations.h"
#include "base/types/pass_key.h"
#include "base/win/scoped_handle.h"
#include "gpu/gpu_gles2_export.h"
#include "ui/gfx/gpu_memory_buffer.h"

namespace gpu {

// DXGISharedHandleManager caches the state associated with DXGI shared handles
// using gfx::DXGIHandleToken as the key. These tokens are used to uniquely
// identify the texture associated with the shared handle even after the handle
// is duplicated. See |dxgi_token| in GpuMemoryBufferHandle.
//
// DXGISharedHandleManager is safe to call from any thread and is guaranteed to
// outlive any scoped_refptrs it hands out. Currently, the manager is only used
// on the GPU main thread, but it is expected that in the near future, the
// scoped_refptrs could be released on other threads e.g. on DrDC thread.
//
// DXGISharedHandleState holds the shared handle and its associated state like
// D3D texture, keyed mutex state, etc. Its lifetime is managed exclusively by
// scoped_refptrs handed out by the DXGISharedHandleManager.
//
// DXGISharedHandleState is implemented as a ref-counted type with custom AddRef
// and Release methods. The manager only holds raw pointers to state instances
// for lookup by token, so DXGISharedHandleState ensures that the raw pointers
// are cleaned up when the refcount goes to zero.

class DXGISharedHandleManager;

class GPU_GLES2_EXPORT DXGISharedHandleState
    : public base::subtle::RefCountedThreadSafeBase {
 public:
  REQUIRE_ADOPTION_FOR_REFCOUNTED_TYPE();

  DXGISharedHandleState(base::PassKey<DXGISharedHandleManager>,
                        scoped_refptr<DXGISharedHandleManager> manager,
                        gfx::DXGIHandleToken token,
                        base::win::ScopedHandle shared_handle,
                        Microsoft::WRL::ComPtr<ID3D11Texture2D> d3d11_texture);

  DXGISharedHandleState(const DXGISharedHandleState&) = delete;
  DXGISharedHandleState& operator=(const DXGISharedHandleState&) = delete;

  void AddRef() const;
  void Release() const;

  HANDLE GetSharedHandle() const { return shared_handle_.Get(); }

  Microsoft::WRL::ComPtr<ID3D11Texture2D> d3d11_texture() const {
    return d3d11_texture_;
  }

  bool has_keyed_mutex() const { return !!dxgi_keyed_mutex_; }

  // The following only have an effect if a keyed mutex is present.
  bool BeginAccessD3D11();
  void EndAccessD3D11();

  bool BeginAccessD3D12();
  void EndAccessD3D12();

 private:
  ~DXGISharedHandleState();

  scoped_refptr<DXGISharedHandleManager> manager_;
  const gfx::DXGIHandleToken token_;

  // If |d3d11_texture_| has a keyed mutex, it will be stored in
  // |dxgi_keyed_mutex_|. The keyed mutex is used to synchronize D3D11 and
  // D3D12 Chromium components. |dxgi_keyed_mutex_| is the D3D11 side of the
  // keyed mutex. To create the corresponding D3D12 interface, pass the handle
  // stored in |shared_handle_| to ID3D12Device::OpenSharedHandle. Only one
  // component is allowed to read/write to the texture at a time.
  base::win::ScopedHandle shared_handle_;
  Microsoft::WRL::ComPtr<ID3D11Texture2D> d3d11_texture_;
  Microsoft::WRL::ComPtr<IDXGIKeyedMutex> dxgi_keyed_mutex_;
  bool acquired_for_d3d12_ = false;
  int acquired_for_d3d11_count_ = 0;
};

class GPU_GLES2_EXPORT DXGISharedHandleManager
    : public base::RefCountedThreadSafe<DXGISharedHandleManager> {
 public:
  explicit DXGISharedHandleManager(
      Microsoft::WRL::ComPtr<ID3D11Device> d3d11_device);

  // Retrieves an existing state associated with |token| or creates a new one if
  // none exists. Note that the returned state won't not have |shared_handle| as
  // its handle if |token| was registered previously, but the state's handle
  // will refer to the same D3D11 texture. Returns a nullptr on error.
  scoped_refptr<DXGISharedHandleState> GetOrCreateSharedHandleState(
      gfx::DXGIHandleToken token,
      base::win::ScopedHandle shared_handle);

  // Creates a new unique state for given |shared_handle| and |d3d11_texture|.
  // No other state will have references to the same shared handle and texture.
  // Useful when creating handles which are guaranteed to never be duplicated
  // e.g. WebGPU usage shared image that only needs a handle for Dawn interop.
  scoped_refptr<DXGISharedHandleState> CreateAnonymousSharedHandleState(
      base::win::ScopedHandle shared_handle,
      Microsoft::WRL::ComPtr<ID3D11Texture2D> d3d11_texture);

  size_t GetSharedHandleMapSizeForTesting() const;

 private:
  friend class base::RefCountedThreadSafe<DXGISharedHandleManager>;
  friend class DXGISharedHandleState;

  ~DXGISharedHandleManager();

  Microsoft::WRL::ComPtr<ID3D11Device> d3d11_device_;

  mutable base::Lock lock_;

  using SharedHandleMap =
      std::map<gfx::DXGIHandleToken, DXGISharedHandleState*>;
  SharedHandleMap shared_handle_state_map_ GUARDED_BY(lock_);
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_DXGI_SHARED_HANDLE_MANAGER_H_
