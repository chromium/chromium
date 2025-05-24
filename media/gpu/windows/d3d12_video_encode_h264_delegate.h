// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_WINDOWS_D3D12_VIDEO_ENCODE_H264_DELEGATE_H_
#define MEDIA_GPU_WINDOWS_D3D12_VIDEO_ENCODE_H264_DELEGATE_H_

#include "third_party/microsoft_dxheaders/src/include/directx/d3d12video.h"
// Windows SDK headers should be included after DirectX headers.

#include <wrl.h>

#include <utility>
#include <vector>

#include "media/base/encoder_status.h"
#include "media/base/video_codecs.h"
#include "media/filters/h26x_annex_b_bitstream_builder.h"
#include "media/gpu/h264_dpb.h"
#include "media/gpu/h264_rate_controller.h"
#include "media/gpu/media_gpu_export.h"
#include "media/gpu/windows/d3d12_video_encode_delegate.h"
#include "media/video/video_encode_accelerator.h"
#include "third_party/abseil-cpp/absl/container/inlined_vector.h"

namespace media {

class D3D12VideoEncodeH264ReferenceFrameManager
    : public D3D12VideoEncodeDecodedPictureBuffers<H264DPB::kDPBMaxSize> {
 public:
  D3D12VideoEncodeH264ReferenceFrameManager();
  ~D3D12VideoEncodeH264ReferenceFrameManager() override;

  void EndFrame(uint32_t frame_num,
                uint32_t pic_order_cnt,
                uint32_t temporal_layer_id);

  base::span<D3D12_VIDEO_ENCODER_REFERENCE_PICTURE_DESCRIPTOR_H264>
  ToReferencePictureDescriptors();

 private:
  using D3D12VideoEncodeDecodedPictureBuffers::InsertCurrentFrame;
  using D3D12VideoEncodeDecodedPictureBuffers::ReplaceWithCurrentFrame;

  absl::InlinedVector<D3D12_VIDEO_ENCODER_REFERENCE_PICTURE_DESCRIPTOR_H264,
                      H264DPB::kDPBMaxSize>
      descriptors_;
};

class MEDIA_GPU_EXPORT D3D12VideoEncodeH264Delegate
    : public D3D12VideoEncodeDelegate {
 public:
  static std::vector<
      std::pair<VideoCodecProfile, std::vector<VideoPixelFormat>>>
  GetSupportedProfiles(ID3D12VideoDevice3* video_device);

  explicit D3D12VideoEncodeH264Delegate(
      Microsoft::WRL::ComPtr<ID3D12VideoDevice3> video_device);
  ~D3D12VideoEncodeH264Delegate() override;

  size_t GetMaxNumOfRefFrames() const override;
  bool ReportsAverageQp() const override;

  bool UpdateRateControl(const Bitrate& bitrate, uint32_t framerate) override;

  bool SupportsRateControlReconfiguration() const override;

  EncoderStatus::Or<BitstreamBufferMetadata> EncodeImpl(
      ID3D12Resource* input_frame,
      UINT input_frame_subresource,
      const VideoEncoder::EncodeOptions& options) override;

 private:
  friend class D3D12VideoEncodeH264DelegateTest;

  EncoderStatus InitializeVideoEncoder(
      const VideoEncodeAccelerator::Config& config) override;

  // Readback the bitstream from the encoder. Also prepend the SPS/PPS header.
  EncoderStatus::Or<size_t> ReadbackBitstream(
      base::span<uint8_t> bitstream_buffer) override;

  H264SPS ToSPS() const;
  H264PPS ToPPS(const H264SPS& sps) const;

  D3D12_VIDEO_ENCODER_SUPPORT_FLAGS encoder_support_flags_{};

  // Codec information, saved for building SPS/PPS.
  D3D12_VIDEO_ENCODER_PROFILE_H264 h264_profile_{};
  D3D12_VIDEO_ENCODER_LEVELS_H264 h264_level_{};
  D3D12_VIDEO_ENCODER_CODEC_CONFIGURATION_H264 codec_config_h264_{};

  // Input arguments.
  D3D12_VIDEO_ENCODER_SEQUENCE_GOP_STRUCTURE_H264 gop_structure_{};
  D3D12_VIDEO_ENCODER_PICTURE_CONTROL_CODEC_DATA_H264 pic_params_{};
  D3D12VideoEncoderRateControl current_rate_control_;

  std::optional<H264RateController> software_rate_controller_;
  H264RateControllerSettings rate_controller_settings_;
  // The timestamp of the next frame in the encoded video, to be used for the
  // rate controller. The value stands for the time delta relative to the
  // beginning of the video when the frame should be decoded.
  base::TimeDelta rate_controller_timestamp_;

  D3D12_VIDEO_ENCODER_ENCODEFRAME_INPUT_ARGUMENTS input_arguments_{};
  std::array<UINT, 16> list0_reference_frames_{};

  D3D12VideoEncodeH264ReferenceFrameManager reference_frame_manager_;

  H26xAnnexBBitstreamBuilder packed_header_{
      /*insert_emulation_prevention_bytes=*/true};

  // The metadata of the bitstream buffer for the last encode request.
  BitstreamBufferMetadata metadata_;
};

}  // namespace media

#endif  // MEDIA_GPU_WINDOWS_D3D12_VIDEO_ENCODE_H264_DELEGATE_H_
