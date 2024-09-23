// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_DXGI_SHARED_HANDLE_MANAGER_H_
#define GPU_COMMAND_BUFFER_SERVICE_DXGI_SHARED_HANDLE_MANAGER_H_

#include <d3d11.h>
#include <wrl/client.h>

// clang-format off
#include <webgpu/webgpu_cpp.h>
// clang-format on

#include "base/containers/flat_map.h"
#include "base/memory/ref_counted.h"
#include "base/synchronization/lock.h"
#include "base/thread_annotations.h"
#include "base/types/pass_key.h"
#include "base/win/scoped_handle.h"
#include "gpu/gpu_gles2_export.h"
#include "ui/gfx/gpu_memory_buffer.h"
#include "ui/gl/buildflags.h"

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

  bool has_keyed_mutex() const { return has_keyed_mutex_; }

  // Returns D3D11 texture for given device. Imports shared handle and caches
  // texture and keyed mutex if not present in |d3d11_texture_state_map_|.
  Microsoft::WRL::ComPtr<ID3D11Texture2D> GetOrCreateD3D11Texture(
      Microsoft::WRL::ComPtr<ID3D11Device> d3d11_device);

  // Acquires keyed mutex if necessary for given device. Disallows concurrent
  // access on different devices due to the possibility of deadlock.
  bool AcquireKeyedMutex(Microsoft::WRL::ComPtr<ID3D11Device> d3d11_device);

  // Releases keyed mutex if all pending access for given device are ended.
  void ReleaseKeyedMutex(Microsoft::WRL::ComPtr<ID3D11Device> d3d11_device);

  // Returns the cached Dawn SharedTextureMemory associated with given device.
  wgpu::SharedTextureMemory GetSharedTextureMemory(const wgpu::Device& device);
  // Inserts the SharedTextureMemory for this device, if not already present.
  void MaybeCacheSharedTextureMemory(const wgpu::Device& device,
                                     wgpu::SharedTextureMemory memory);
  void EraseDawnSharedTextureMemory(const wgpu::Device& device);

 private:
  struct D3D11TextureState {
    D3D11TextureState(Microsoft::WRL::ComPtr<ID3D11Texture2D> d3d11_texture);
    ~D3D11TextureState();
    D3D11TextureState(D3D11TextureState&&);
    D3D11TextureState& operator=(D3D11TextureState&&);

    Microsoft::WRL::ComPtr<ID3D11Texture2D> d3d11_texture;
    Microsoft::WRL::ComPtr<IDXGIKeyedMutex> dxgi_keyed_mutex;
    int keyed_mutex_acquired_count = 0;
  };

  ~DXGISharedHandleState();

  mutable base::Lock lock_;

  const scoped_refptr<DXGISharedHandleManager> manager_;

  const gfx::DXGIHandleToken token_;

  const base::win::ScopedHandle shared_handle_;

  using D3D11TextureStateMap =
      base::flat_map<Microsoft::WRL::ComPtr<ID3D11Device>, D3D11TextureState>;
  D3D11TextureStateMap d3d11_texture_state_map_ GUARDED_BY(lock_);

  // When Dawn uses keyed mutex for synchronization with the D3D11 backend, we
  // want a single instance of SharedTextureMemory (per device) for each unique
  // texture even if we have multiple duplicated handles (and shared images)
  // pointing to the texture. Caching the SharedTextureMemory here enables this.
  // Note that it's ok to use raw WGPUDevice pointers here since the shared
  // texture memory acts like a weak pointer to the device, and we can detect if
  // the entry is valid by checking SharedTextureMemory::IsDeviceLost().
  using DawnSharedTextureMemoryCache =
      base::flat_map<WGPUDevice, wgpu::SharedTextureMemory>;
  DawnSharedTextureMemoryCache dawn_shared_texture_memory_cache_
      GUARDED_BY(lock_);

  // True if the texture has an underlying keyed mutex.
  const bool has_keyed_mutex_ = false;

  // True if the keyed mutex is acquired on any device.
  bool keyed_mutex_acquired_ GUARDED_BY(lock_) = false;
};

class GPU_GLES2_EXPORT DXGISharedHandleManager
    : public base::RefCountedThreadSafe<DXGISharedHandleManager> {
 public:
  DXGISharedHandleManager();

  // Retrieves an existing state associated with |token| or creates a new one if
  // none exists. Note that the returned state won't not have |shared_handle| as
  // its handle if |token| was registered previously, but the state's handle
  // will refer to the same D3D11 texture. |d3d11_device| is used for opening
  // the shared handle if a state is not found. Returns a nullptr on error.
  scoped_refptr<DXGISharedHandleState> GetOrCreateSharedHandleState(
      gfx::DXGIHandleToken token,
      base::win::ScopedHandle shared_handle,
      Microsoft::WRL::ComPtr<ID3D11Device> d3d11_device);

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

  mutable base::Lock lock_;

  using SharedHandleMap =
      base::flat_map<gfx::DXGIHandleToken, DXGISharedHandleState*>;
  SharedHandleMap shared_handle_state_map_ GUARDED_BY(lock_);
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_DXGI_SHARED_HANDLE_MANAGER_H_
