// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_WINDOWS_D3D12_VIDEO_ENCODE_DELEGATE_H_
#define MEDIA_GPU_WINDOWS_D3D12_VIDEO_ENCODE_DELEGATE_H_

#include <d3d12.h>
#include <d3d12video.h>
#include <wrl.h>

#include "media/base/bitstream_buffer.h"
#include "media/base/encoder_status.h"
#include "media/gpu/media_gpu_export.h"
#include "media/video/video_encode_accelerator.h"

namespace media {

class MEDIA_GPU_EXPORT D3D12VideoEncodeDelegate {
 public:
  struct EncodeResult {
    int32_t bitstream_buffer_id_;
    BitstreamBufferMetadata metadata_;
  };

  // Returns the supported profiles for all available codecs.
  static VideoEncodeAccelerator::SupportedProfiles GetSupportedProfiles(
      ID3D12VideoDevice3* video_device);

  explicit D3D12VideoEncodeDelegate(
      Microsoft::WRL::ComPtr<ID3D12VideoDevice3> video_device);
  virtual ~D3D12VideoEncodeDelegate();

  virtual EncoderStatus Initialize(VideoEncodeAccelerator::Config config);
  virtual size_t GetMaxNumOfRefFrames() const = 0;

  // Do video processing if the input frame format or resolution is not
  // expected and then call |EncodeImpl()|.
  virtual EncoderStatus::Or<EncodeResult> Encode(
      Microsoft::WRL::ComPtr<ID3D12Resource> input_frame,
      UINT input_frame_subresource,
      const gfx::ColorSpace& input_frame_color_space,
      const BitstreamBuffer& bitstream_buffer,
      bool force_keyframe);

  // Do the codec specific encoding.
  virtual EncoderStatus::Or<BitstreamBufferMetadata> EncodeImpl(
      ID3D12Resource* input_frame,
      UINT input_frame_subresource,
      bool force_keyframe) = 0;

 protected:
  virtual EncoderStatus InitializeVideoEncoder(
      const VideoEncodeAccelerator::Config& config) = 0;

  virtual EncoderStatus::Or<size_t> ReadbackBitstream(
      base::span<uint8_t> bitstream_buffer);
};

}  // namespace media

#endif  // MEDIA_GPU_WINDOWS_D3D12_VIDEO_ENCODE_DELEGATE_H_
