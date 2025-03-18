// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_WINDOWS_D3D12_FENCE_H_
#define MEDIA_GPU_WINDOWS_D3D12_FENCE_H_

#include <wrl.h>

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "media/gpu/windows/d3d11_status.h"
#include "media/gpu/windows/d3d_com_defs.h"

namespace media {

// D3D12Fence wraps a ID3D12Fence pointer and its last signaled fence value.
class D3D12Fence : public base::RefCountedThreadSafe<D3D12Fence> {
 public:
  explicit D3D12Fence(ComD3D12Fence fence);

  static scoped_refptr<D3D12Fence> Create(
      ID3D12Device* device,
      D3D12_FENCE_FLAGS flags = D3D12_FENCE_FLAG_NONE);

  // Let |command_queue| signal the fence and return the corresponding fence
  // value to be waited for elsewhere.
  D3D11Status::Or<uint64_t> Signal(ID3D12CommandQueue& command_queue);

  // Wait on CPU until the |fence_value| is signaled.
  D3D11Status Wait(uint64_t fence_value) const;

  // Signal the fence and wait on CPU until the fence is signaled.
  D3D11Status SignalAndWait(ID3D12CommandQueue& command_queue);

 private:
  friend class RefCountedThreadSafe;
  ~D3D12Fence();

  ComD3D12Fence fence_;
  uint64_t fence_value_ = 0;
};

}  // namespace media

#endif  // MEDIA_GPU_WINDOWS_D3D12_FENCE_H_
