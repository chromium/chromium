// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_WINDOWS_D3D12_COPY_COMMAND_LIST_WRAPPER_H_
#define MEDIA_GPU_WINDOWS_D3D12_COPY_COMMAND_LIST_WRAPPER_H_

#include <d3d12.h>
#include <wrl.h>

#include <memory>

#include "base/containers/span.h"
#include "base/memory/scoped_refptr.h"
#include "media/gpu/windows/d3d12_fence.h"
#include "media/gpu/windows/d3d12_helpers.h"

namespace media {

class D3D12CopyCommandQueueWrapper {
 public:
  static std::unique_ptr<D3D12CopyCommandQueueWrapper> Create(
      ID3D12Device* device);

  D3D12CopyCommandQueueWrapper(
      Microsoft::WRL::ComPtr<ID3D12CommandQueue> command_queue,
      Microsoft::WRL::ComPtr<ID3D12CommandAllocator> command_allocator,
      Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> command_list,
      Microsoft::WRL::ComPtr<ID3D12Fence> fence);
  ~D3D12CopyCommandQueueWrapper();

  bool CopyTextureRegion(const D3D12_TEXTURE_COPY_LOCATION& dst_location,
                         uint32_t x,
                         uint32_t y,
                         uint32_t z,
                         const D3D12_TEXTURE_COPY_LOCATION& src_location,
                         const D3D12_BOX* src_box = nullptr);

  bool CopyBufferToNV12Texture(ID3D12Resource* target_texture,
                               ID3D12Resource* source_buffer,
                               uint32_t y_offset,
                               uint32_t y_stride,
                               uint32_t uv_offset,
                               uint32_t uv_stride);

  D3D12FenceAndValue Execute();

 private:
  // This will be automatically called before CopyTextureRegion().
  bool Reset();

  Microsoft::WRL::ComPtr<ID3D12CommandQueue> command_queue_;
  Microsoft::WRL::ComPtr<ID3D12CommandAllocator> command_allocator_;
  Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> command_list_;
  bool is_open_ = true;
  scoped_refptr<D3D12Fence> fence_;
};

}  // namespace media

#endif  // MEDIA_GPU_WINDOWS_D3D12_COPY_COMMAND_LIST_WRAPPER_H_
