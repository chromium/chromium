// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_D3D_SHARED_FENCE_H_
#define GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_D3D_SHARED_FENCE_H_

#include <d3d11.h>
#include <d3d11_4.h>
#include <wrl/client.h>

#include "base/containers/lru_cache.h"
#include "base/memory/ref_counted.h"
#include "base/win/scoped_handle.h"
#include "gpu/gpu_gles2_export.h"

namespace gpu {

// A ref-counted wrapper around D3D11Fence and its shared handle. Fences are
// opened for each D3D11 device the fence is waited on. Ref-counting is used so
// that the same fence object can be referred to in multiple places e.g. as the
// signaling fence or in list of fences to wait for next access.
class GPU_GLES2_EXPORT D3DSharedFence
    : public base::RefCounted<D3DSharedFence> {
 public:
  // Create a new ID3D11Fence with initial value 0 on given |d3d11_device|.
  static scoped_refptr<D3DSharedFence> CreateForD3D11(
      Microsoft::WRL::ComPtr<ID3D11Device> d3d11_device);

  // Create from existing shared handle e.g. from Dawn. Doesn't take ownership
  // of |shared_handle| and duplicates it instead. The ID3D11Fence is lazily
  // created on Wait or Signal for the device provided to those calls.
  static scoped_refptr<D3DSharedFence> CreateFromHandle(HANDLE shared_handle);

  // Returns true if fences are supported i.e. if ID3D11Device5 is supported.
  static bool IsSupported(ID3D11Device* d3d11_device);

  // Return the shared handle for the fence.
  HANDLE GetSharedHandle() const;

  // Returns true if the underlying fence object is the same as |shared_handle|.
  bool IsSameFenceAsHandle(HANDLE shared_handle) const;

  // Issue a wait for the fence on the immediate context of |d3d11_device| using
  // |wait_value|. The wait is skipped if the passed in device is the same as
  // |d3d11_device_|. Returns true on success.
  bool WaitD3D11(Microsoft::WRL::ComPtr<ID3D11Device> d3d11_device,
                 uint64_t wait_value);

  // Issue a signal for the fence on the immediate context of |d3d11_device_|
  // using |signal_value|. Returns true on success.
  bool SignalD3D11(uint64_t signal_value);

 private:
  friend class base::RefCounted<D3DSharedFence>;

  // 5 D3D11 devices ought to be enough for anybody.
  static constexpr size_t kMaxD3D11FenceMapSize = 5;

  explicit D3DSharedFence(base::win::ScopedHandle shared_handle);
  ~D3DSharedFence();

  // Owned shared handle corresponding to the fence.
  base::win::ScopedHandle shared_handle_;

  // If present, this is the D3D11 device that the fence was created on, and
  // used to signal |d3d11_fence_|.
  Microsoft::WRL::ComPtr<ID3D11Device> d3d11_device_;

  // If present, this is the D3D11 fence object this fence was created with and
  // used for signaling.
  Microsoft::WRL::ComPtr<ID3D11Fence> d3d11_signal_fence_;

  // Map of D3D11 device to D3D11 fence objects used by WaitD3D11().
  base::LRUCache<Microsoft::WRL::ComPtr<ID3D11Device>,
                 Microsoft::WRL::ComPtr<ID3D11Fence>>
      d3d11_wait_fence_map_;
};

}  // namespace gpu

#endif
