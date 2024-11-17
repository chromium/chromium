// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_WINDOWS_D3D12_VIDEO_ENCODE_DELEGATE_H_
#define MEDIA_GPU_WINDOWS_D3D12_VIDEO_ENCODE_DELEGATE_H_

#include <d3d12.h>
#include <wrl.h>

#include "media/base/bitstream_buffer.h"
#include "media/base/encoder_status.h"
#include "media/gpu/media_gpu_export.h"
#include "media/gpu/windows/d3d12_video_encoder_wrapper.h"
#include "media/gpu/windows/d3d12_video_processor_wrapper.h"
#include "media/video/video_encode_accelerator.h"
#include "third_party/abseil-cpp/absl/container/inlined_vector.h"

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

  Microsoft::WRL::ComPtr<ID3D12Device> device_;
  Microsoft::WRL::ComPtr<ID3D12VideoDevice3> video_device_;
  size_t max_num_ref_frames_ = 0;

  // The the size and format for the input of the D3D12VideoEncoder. The format
  // may be different to input frame, in which case we do internal conversion.
  D3D12_VIDEO_ENCODER_PICTURE_RESOLUTION_DESC input_size_{};
  DXGI_FORMAT input_format_ = DXGI_FORMAT_UNKNOWN;

  union {
    D3D12_VIDEO_ENCODER_RATE_CONTROL_CQP cqp;
    D3D12_VIDEO_ENCODER_RATE_CONTROL_CBR cbr;
    D3D12_VIDEO_ENCODER_RATE_CONTROL_VBR vbr;
  } rate_control_params_;
  D3D12_VIDEO_ENCODER_RATE_CONTROL rate_control_{};

  // Output profile requested by |config| in |Initialize()|.
  // The implementation may use a different profile for compatibility.
  VideoCodecProfile output_profile_ = VIDEO_CODEC_PROFILE_UNKNOWN;

  std::unique_ptr<D3D12VideoEncoderWrapper> video_encoder_wrapper_;

 private:
  // The video processor used for possible resolution, format, or color space
  // conversion.
  std::unique_ptr<D3D12VideoProcessorWrapper> video_processor_wrapper_;
  Microsoft::WRL::ComPtr<ID3D12Resource> processed_input_frame_;
};

// Records an ID3D12Resource pointer and a subresource index for a specific
// subresource.
struct D3D12PictureBuffer {
  raw_ptr<ID3D12Resource> resource_ = nullptr;
  UINT subresource_ = 0;
};

// A class to manage the decoded picture buffers for D3D12 video encode and
// returns the |D3D12_VIDEO_ENCODE_REFERENCE_FRAMES| that D3D12 video encode API
// expects. When initialized, it creates the textures that may be needed during
// the encoding. Before encoding a frame, the client should call
// |GetCurrentFrame()| to get an unused buffer. After encoding a frame, depends
// on the codec, call |InsertCurrentFrame()| or |ReplaceWithCurrentFrame()| to
// update the decoded picture buffers.
template <size_t maxDpbSize>
class D3D12VideoEncodeDecodedPictureBuffers {
 public:
  explicit D3D12VideoEncodeDecodedPictureBuffers(size_t size);
  ~D3D12VideoEncodeDecodedPictureBuffers();

  // Initialize the texture array with the given size and format.
  bool InitializeTextureArray(ID3D12Device* device,
                              gfx::Size texture_size,
                              DXGI_FORMAT format);

  // Get the unused buffer for current frame.
  D3D12PictureBuffer GetCurrentFrame() const;
  // Insert the last picture buffer returned by |GetCurrentFrame()| into the
  // given index and move the old buffers with index no less than |position| to
  // the next index.
  void InsertCurrentFrame(size_t position);
  // Replace the picture buffer at |position| with the last picture buffer
  // returned by |GetCurrentFrame()|.
  void ReplaceWithCurrentFrame(size_t position);

  // Return the |D3D12_VIDEO_ENCODE_REFERENCE_FRAMES| structure that D3D12 video
  // encode API expects.
  D3D12_VIDEO_ENCODE_REFERENCE_FRAMES ToD3D12VideoEncodeReferenceFrames();

 private:
  size_t size_;
  absl::InlinedVector<Microsoft::WRL::ComPtr<ID3D12Resource>, maxDpbSize + 1>
      resources_;
  absl::InlinedVector<ID3D12Resource*, maxDpbSize + 1> raw_resources_;
  absl::InlinedVector<UINT, maxDpbSize + 1> subresources_;
};

}  // namespace media

#endif  // MEDIA_GPU_WINDOWS_D3D12_VIDEO_ENCODE_DELEGATE_H_
