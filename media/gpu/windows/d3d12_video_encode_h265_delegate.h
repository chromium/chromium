// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_WINDOWS_D3D12_VIDEO_ENCODE_H265_DELEGATE_H_
#define MEDIA_GPU_WINDOWS_D3D12_VIDEO_ENCODE_H265_DELEGATE_H_

#include "third_party/microsoft_dxheaders/src/include/directx/d3d12video.h"
// Windows SDK headers should be included after DirectX headers.

#include <wrl.h>

#include <vector>

#include "media/base/encoder_status.h"
#include "media/base/video_codecs.h"
#include "media/filters/h26x_annex_b_bitstream_builder.h"
#include "media/gpu/media_gpu_export.h"
#include "media/gpu/windows/d3d12_video_encode_delegate.h"
#include "media/parsers/h265_parser.h"
#include "media/video/video_encode_accelerator.h"
#include "third_party/abseil-cpp/absl/container/inlined_vector.h"

namespace media {

class D3D12VideoEncodeH265ReferenceFrameManager {
 public:
  explicit D3D12VideoEncodeH265ReferenceFrameManager(size_t max_num_ref_frames);
  ~D3D12VideoEncodeH265ReferenceFrameManager();

  void EndFrame(uint32_t pic_order_count, uint32_t temporal_layer_id);

  // Write the reference picture descriptors to |pic_params| according to the
  // ListxReferenceFrames variables.
  void WriteReferencePictureDescriptorsToPictureParameters(
      D3D12_VIDEO_ENCODER_PICTURE_CONTROL_CODEC_DATA_HEVC* pic_params,
      base::span<uint32_t> list0_reference_frames);

 private:
  size_t max_num_ref_frames_ = 0;
  absl::InlinedVector<D3D12_VIDEO_ENCODER_REFERENCE_PICTURE_DESCRIPTOR_HEVC,
                      kMaxDpbSize>
      descriptors_;
};

class MEDIA_GPU_EXPORT D3D12VideoEncodeH265Delegate
    : public D3D12VideoEncodeDelegate {
 public:
  static std::vector<
      std::pair<VideoCodecProfile, std::vector<VideoPixelFormat>>>
  GetSupportedProfiles(ID3D12VideoDevice3* video_device);

  explicit D3D12VideoEncodeH265Delegate(
      Microsoft::WRL::ComPtr<ID3D12VideoDevice3> video_device);
  ~D3D12VideoEncodeH265Delegate() override;

  size_t GetMaxNumOfRefFrames() const override;

  bool SupportsRateControlReconfiguration() const override;

  EncoderStatus::Or<BitstreamBufferMetadata> EncodeImpl(
      ID3D12Resource* input_frame,
      UINT input_frame_subresource,
      bool force_keyframe) override;

 private:
  EncoderStatus InitializeVideoEncoder(
      const VideoEncodeAccelerator::Config& config) override;

  // Readback the bitstream from the encoder. Also prepend the SPS/PPS header.
  EncoderStatus::Or<size_t> ReadbackBitstream(
      base::span<uint8_t> bitstream_buffer) override;

  H265VPS ToVPS() const;
  H265SPS ToSPS(const H265VPS& vps) const;
  H265PPS ToPPS(const H265SPS& sps) const;

  D3D12_VIDEO_ENCODER_SUPPORT_FLAGS encoder_support_flags_{};

  // Codec information, saved for building VPS/SPS/PPS.
  D3D12_VIDEO_ENCODER_PROFILE_HEVC h265_profile_{};
  D3D12_VIDEO_ENCODER_LEVEL_TIER_CONSTRAINTS_HEVC h265_level_{};
  D3D12_VIDEO_ENCODER_CODEC_CONFIGURATION_HEVC codec_config_hevc_{};
  D3D12_FEATURE_DATA_VIDEO_ENCODER_RESOLUTION_SUPPORT_LIMITS
  resolution_support_limits_{};

  // Input arguments.
  D3D12_VIDEO_ENCODER_SEQUENCE_GOP_STRUCTURE_HEVC gop_structure_{};
  D3D12_VIDEO_ENCODER_PICTURE_CONTROL_CODEC_DATA_HEVC pic_params_{};
  D3D12VideoEncoderRateControl current_rate_control_;
  D3D12_VIDEO_ENCODER_ENCODEFRAME_INPUT_ARGUMENTS input_arguments_{};
  std::array<UINT, 16> list0_reference_frames_{};

  std::optional<D3D12VideoEncodeDecodedPictureBuffers<kMaxDpbSize>> dpb_;
  std::optional<D3D12VideoEncodeH265ReferenceFrameManager>
      reference_frame_manager_;

  H26xAnnexBBitstreamBuilder packed_header_{
      /*insert_emulation_prevention_bytes=*/true};
};

}  // namespace media

#endif  // MEDIA_GPU_WINDOWS_D3D12_VIDEO_ENCODE_H265_DELEGATE_H_
