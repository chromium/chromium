// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_WINDOWS_D3D12_VIDEO_DECODE_TASK_H_
#define MEDIA_GPU_WINDOWS_D3D12_VIDEO_DECODE_TASK_H_

#include <d3d12.h>
#include <wrl.h>

#include <utility>
#include <vector>

#include "base/containers/span.h"
#include "media/gpu/windows/d3d12_fence.h"

namespace media {

// Stores the referenced memory and fence event of a submitted D3D12 video
// decode task.
class D3D12VideoDecoderTask {
 public:
  D3D12VideoDecoderTask();
  ~D3D12VideoDecoderTask();

  void SetFenceAndValue(scoped_refptr<D3D12Fence> fence, uint64_t value);

  // Wait for previous task to complete. After |SetFenceAndValue()| is called,
  // all other methods should be called after this method returns true.
  bool WaitForCompletion();

  // Reset the picture parameters, inverse quantization matrix, and slice
  // control buffers. The compressed bitstream buffer is managed in
  // |GetBitstreamBuffer()|.
  void ResetBuffers();

  // Create or reset the command allocator for the task and return it.
  Microsoft::WRL::ComPtr<ID3D12CommandAllocator> ResetAndGetCommandAllocator(
      ID3D12Device* device);

  std::vector<uint8_t>& GetPictureParametersBuffer();
  std::vector<uint8_t>& GetInverseQuantizationMatrixBuffer();
  std::vector<uint8_t>& GetSliceControlBuffer();

  Microsoft::WRL::ComPtr<ID3D12Resource> GetBitstreamBuffer(
      ID3D12Device* device,
      size_t size);

 private:
  Microsoft::WRL::ComPtr<ID3D12CommandAllocator> command_allocator_;
  std::vector<uint8_t> picture_parameters_buffer_;
  std::vector<uint8_t> inverse_quantization_matrix_buffer_;
  std::vector<uint8_t> slice_control_buffer_;
  Microsoft::WRL::ComPtr<ID3D12Resource> compressed_bitstream_;
  std::pair<scoped_refptr<D3D12Fence>, uint64_t> fence_and_value_;
};

}  // namespace media

#endif  // MEDIA_GPU_WINDOWS_D3D12_VIDEO_DECODE_TASK_H_
