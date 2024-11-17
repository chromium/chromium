// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_WINDOWS_D3D12_VIDEO_ENCODER_WRAPPER_H_
#define MEDIA_GPU_WINDOWS_D3D12_VIDEO_ENCODER_WRAPPER_H_

#include <d3d12.h>
#include <d3d12video.h>
#include <wrl.h>

#include "base/containers/span.h"
#include "media/base/encoder_status.h"
#include "media/gpu/windows/d3d12_fence.h"
#include "media/gpu/windows/d3d12_helpers.h"

namespace media {

class D3D12VideoEncoderWrapper {
 public:
  D3D12VideoEncoderWrapper(
      Microsoft::WRL::ComPtr<ID3D12VideoEncoder> video_encoder,
      Microsoft::WRL::ComPtr<ID3D12VideoEncoderHeap> video_encoder_heap);
  ~D3D12VideoEncoderWrapper();

  bool Initialize();

  // Do the encode and wait for the completion of the encoding.
  EncoderStatus Encode(
      const D3D12_VIDEO_ENCODER_ENCODEFRAME_INPUT_ARGUMENTS& input_arguments,
      const D3D12_VIDEO_ENCODER_RECONSTRUCTED_PICTURE& reconstructed_picture);

  // Get the number of bytes written to the bitstream buffer or log the encoding
  // error.
  EncoderStatus::Or<uint64_t> GetEncodedBitstreamWrittenBytesCount() const;

  // Readback bitstream from GPU memory to |data|. The |size| must be no greater
  // than the value returned by |GetEncodedBitstreamWrittenBytesCount()|.
  EncoderStatus ReadbackBitstream(base::span<uint8_t> data) const;

 private:
  Microsoft::WRL::ComPtr<ID3D12VideoEncoder> video_encoder_;
  Microsoft::WRL::ComPtr<ID3D12VideoEncoderHeap> video_encoder_heap_;

  Microsoft::WRL::ComPtr<ID3D12CommandQueue> command_queue_;
  Microsoft::WRL::ComPtr<ID3D12CommandAllocator> command_allocator_;
  Microsoft::WRL::ComPtr<ID3D12VideoEncodeCommandList2> command_list_;
  scoped_refptr<D3D12Fence> fence_;

  Microsoft::WRL::ComPtr<ID3D12Resource> bitstream_buffer_;
  Microsoft::WRL::ComPtr<ID3D12Resource> opaque_metadata_buffer_;
  Microsoft::WRL::ComPtr<ID3D12Resource> metadata_buffer_;

  D3D12_VIDEO_ENCODER_ENCODEFRAME_OUTPUT_ARGUMENTS output_arguments_{};

  D3D12_VIDEO_ENCODER_RESOLVE_METADATA_INPUT_ARGUMENTS
  resolve_metadata_input_arguments_{};
  D3D12_VIDEO_ENCODER_RESOLVE_METADATA_OUTPUT_ARGUMENTS
  resolve_metadata_output_arguments_{};
};

}  // namespace media

#endif  // MEDIA_GPU_WINDOWS_D3D12_VIDEO_ENCODER_WRAPPER_H_
