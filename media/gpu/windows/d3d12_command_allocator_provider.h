// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_WINDOWS_D3D12_COMMAND_ALLOCATOR_PROVIDER_H_
#define MEDIA_GPU_WINDOWS_D3D12_COMMAND_ALLOCATOR_PROVIDER_H_

#include "third_party/microsoft_dxheaders/src/include/directx/d3d12.h"
// Windows SDK headers should be included after DirectX headers.

#include <stddef.h>
#include <stdint.h>

#include <array>

#include "base/memory/scoped_refptr.h"
#include "base/types/expected.h"
#include "media/gpu/media_gpu_export.h"
#include "media/gpu/windows/d3d12_fence.h"
#include "media/gpu/windows/d3d_com_defs.h"
#include "media/gpu/windows/d3d_status.h"

namespace media {

// Provides the command allocator for recording the D3D12 video decode commands
// of the next frame. Resetting a command allocator is costly, so instead of
// resetting it for every frame, keep recording into the same one and only reset
// it when the GPU has completed all the commands recorded in it. When the GPU
// does not catch up, switch to another allocator instead of waiting.
class MEDIA_GPU_EXPORT D3D12CommandAllocatorProvider {
 public:
  // The maximum number of frames recorded into a command allocator before
  // switching to another one. This bounds the memory held by each allocator
  // when the GPU does not catch up. The value is chosen to reset as
  // rarely as possible without holding much extra memory. It is deliberately
  // larger than the number of frames in flight (see
  // `D3DVideoDecoder::GetMaxDecodeRequests()`), since the GPU is normally
  // caught up long before it is reached.
  static constexpr size_t kMaxFramesPerAllocator = 64;
  // The number of command allocators to switch between.
  static constexpr size_t kNumAllocators = 2;

  // `fence` is the fence that is signaled after the commands of each frame are
  // submitted.
  D3D12CommandAllocatorProvider(ComD3D12Device device,
                                scoped_refptr<D3D12Fence> fence);
  ~D3D12CommandAllocatorProvider();

  D3D12CommandAllocatorProvider(const D3D12CommandAllocatorProvider&) = delete;
  D3D12CommandAllocatorProvider& operator=(
      const D3D12CommandAllocatorProvider&) = delete;

  // Returns the command allocator for recording the next frame, or the error
  // on failure. It may create or reset an allocator, or wait for the GPU. The
  // caller must not release the returned allocator.
  base::expected<ID3D12CommandAllocator*, D3DStatus> PrepareCommandAllocator();

  // Must be called after the commands recorded in the allocator returned by
  // `PrepareCommandAllocator()` are submitted, with the value that the fence is
  // signaled with after them.
  void OnSubmitted(uint64_t fence_value);

 private:
  struct Allocator {
    ComD3D12CommandAllocator command_allocator;
    // The fence value signaled after the last commands recorded in it.
    uint64_t last_submitted_fence_value = 0;
    size_t num_frames = 0;
  };

  // Creates the command allocator of `allocator`.
  D3DStatus CreateAllocator(Allocator& allocator);

  // Resets `allocator`, waiting for the GPU to complete its commands first if
  // needed.
  D3DStatus ResetAllocator(Allocator& allocator);

  ComD3D12Device device_;
  scoped_refptr<D3D12Fence> fence_;
  std::array<Allocator, kNumAllocators> allocators_;
  // The index of the allocator that the next frame is recorded into.
  size_t index_ = 0;
};

}  // namespace media

#endif  // MEDIA_GPU_WINDOWS_D3D12_COMMAND_ALLOCATOR_PROVIDER_H_
