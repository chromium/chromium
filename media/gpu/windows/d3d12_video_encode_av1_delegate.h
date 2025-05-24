// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_WINDOWS_D3D12_VIDEO_ENCODE_AV1_DELEGATE_H_
#define MEDIA_GPU_WINDOWS_D3D12_VIDEO_ENCODE_AV1_DELEGATE_H_

#include "third_party/microsoft_dxheaders/src/include/directx/d3d12video.h"
// Windows SDK headers should be included after DirectX headers.

#include "media/gpu/av1_builder.h"
#include "media/gpu/windows/d3d12_video_encode_delegate.h"

namespace aom {
class AV1RateControlRTC;
}  // namespace aom

namespace media {

class MEDIA_GPU_EXPORT D3D12VideoEncodeAV1Delegate
    : public D3D12VideoEncodeDelegate {
 public:
  struct PictureControlFlags {
    bool allow_screen_content_tools = false;
    bool allow_intrabc = false;
  };

  static std::vector<
      std::pair<VideoCodecProfile, std::vector<VideoPixelFormat>>>
  GetSupportedProfiles(ID3D12VideoDevice3* video_device);

  explicit D3D12VideoEncodeAV1Delegate(
      Microsoft::WRL::ComPtr<ID3D12VideoDevice3> video_device);
  ~D3D12VideoEncodeAV1Delegate() override;

  size_t GetMaxNumOfRefFrames() const override;

  EncoderStatus::Or<BitstreamBufferMetadata> EncodeImpl(
      ID3D12Resource* input_frame,
      UINT input_frame_subresource,
      const VideoEncoder::EncodeOptions& options) override;

  bool SupportsRateControlReconfiguration() const override;

  bool UpdateRateControl(const Bitrate& bitrate, uint32_t framerate) override;

 private:
  EncoderStatus InitializeVideoEncoder(
      const VideoEncodeAccelerator::Config& config) override;

  EncoderStatus::Or<size_t> ReadbackBitstream(
      base::span<uint8_t> bitstream_buffer) override;

  void FillPictureControlParams(const VideoEncoder::EncodeOptions& options);

  D3D12_VIDEO_ENCODER_ENCODEFRAME_INPUT_ARGUMENTS input_arguments_{};

  // input_arguments_.SequenceControlDesc.CodecGopSequence
  D3D12_VIDEO_ENCODER_AV1_SEQUENCE_STRUCTURE gop_sequence_{};

  // input_arguments_.PictureControlDesc.PictureControlCodecData
  D3D12_VIDEO_ENCODER_AV1_PICTURE_CONTROL_CODEC_DATA picture_params_{};

  // Bitrate controller for CBR encoding.
  std::unique_ptr<aom::AV1RateControlRTC> software_brc_;

  // TODO: move out of av1 delegate.
  D3D12_VIDEO_ENCODER_RATE_CONTROL_CQP cqp_pramas_;

  // Bitrate allocation in bps.
  VideoBitrateAllocation bitrate_allocation_{Bitrate::Mode::kConstant};

  // Enabled features for creating D3D12 AV1 encoder.
  D3D12_VIDEO_ENCODER_AV1_FEATURE_FLAGS enabled_features_{};

  // Picture control flags for each Encoding frame.
  PictureControlFlags picture_ctrl_{};

  // The encoding content is a screen content.
  bool is_screen_ = false;

  uint32_t framerate_ = 30;
  AV1BitstreamBuilder::SequenceHeader sequence_header_;
  D3D12VideoEncodeDecodedPictureBuffers<kAV1DPBMaxSize> dpb_;
  int picture_id_ = -1;
};

}  // namespace media

#endif  // MEDIA_GPU_WINDOWS_D3D12_VIDEO_ENCODE_AV1_DELEGATE_H_
