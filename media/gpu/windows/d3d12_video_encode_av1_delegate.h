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
    bool enable_auto_segmentation = false;
  };

  struct D3D12EncodingCapabilities {
    D3D12_VIDEO_ENCODER_AV1_INTERPOLATION_FILTERS interpolation_filter;
    D3D12_VIDEO_ENCODER_AV1_RESTORATION_CONFIG loop_restoration;
    std::array<D3D12_VIDEO_ENCODER_AV1_TX_MODE, 2> tx_modes;
    D3D12_VIDEO_ENCODER_AV1_POST_ENCODE_VALUES_FLAGS post_value_flags;
  };

  static std::vector<
      std::pair<VideoCodecProfile, std::vector<VideoPixelFormat>>>
  GetSupportedProfiles(ID3D12VideoDevice3* video_device);

  explicit D3D12VideoEncodeAV1Delegate(
      Microsoft::WRL::ComPtr<ID3D12VideoDevice3> video_device);
  ~D3D12VideoEncodeAV1Delegate() override;

  size_t GetMaxNumOfRefFrames() const override;
  size_t GetMaxNumOfManualRefBuffers() const override;

  EncoderStatus EncodeImpl(ID3D12Resource* input_frame,
                           UINT input_frame_subresource,
                           const VideoEncoder::EncodeOptions& options,
                           const gfx::ColorSpace& input_color_space) override;

  bool SupportsRateControlReconfiguration() const override;

  bool UpdateRateControl(const VideoBitrateAllocation& bitrate_allocation,
                         uint32_t framerate) override;

  bool ReportsAverageQp() const override;

 private:
  friend class D3D12VideoEncodeAV1DelegateTest;

  EncoderStatus InitializeVideoEncoder(
      const VideoEncodeAccelerator::Config& config) override;

  EncoderStatus::Or<size_t> GetEncodedBitstreamWrittenBytesCount(
      const ScopedD3D12ResourceMap& metadata) override;

  void RefreshDPBAndDescriptors();

  size_t PackAV1BitstreamHeader(
      const AV1BitstreamBuilder::FrameHeader& frame_header,
      size_t compressed_size,
      base::span<uint8_t> bitstream_buffer);
  EncoderStatus::Or<size_t> ReadbackBitstream(
      base::span<uint8_t> bitstream_buffer) override;

  void FillPictureControlParams(const VideoEncoder::EncodeOptions& options);

  // Updates frame header to be packed into the encoder output bitstream,
  // according to the post-encode metadata from driver.
  bool UpdateFrameHeaderPostEncode(
      const D3D12_VIDEO_ENCODER_AV1_POST_ENCODE_VALUES_FLAGS& post_encode_flags,
      const D3D12_VIDEO_ENCODER_AV1_POST_ENCODE_VALUES& post_encode_values,
      AV1BitstreamBuilder::FrameHeader& frame_header);

  // Returns true if the current picture is a key frame, should be guranteed
  // to be called after `FillPictureControlParams()`.
  bool IsKeyFrame() const { return picture_id_ == 0; }

  uint32_t max_num_ref_frames_ = 0;

  D3D12_VIDEO_ENCODER_ENCODEFRAME_INPUT_ARGUMENTS input_arguments_{};

  // input_arguments_.SequenceControlDesc.CodecGopSequence
  D3D12_VIDEO_ENCODER_AV1_SEQUENCE_STRUCTURE gop_sequence_{};

  // input_arguments_.PictureControlDesc.PictureControlCodecData
  D3D12_VIDEO_ENCODER_AV1_PICTURE_CONTROL_CODEC_DATA picture_params_{};

  // picture_params_.ReferenceFramesReconPictureDescriptors
  std::array<D3D12_VIDEO_ENCODER_AV1_REFERENCE_PICTURE_DESCRIPTOR, 8>
      reference_descriptors_{};

  // Bitrate controller for CBR encoding.
  std::unique_ptr<aom::AV1RateControlRTC> software_brc_;

  // The `enc_caps_` is populated based on the capability of d3d12
  // driver in `InitializeVideoEncoder()` and remains constant afterwards.
  D3D12EncodingCapabilities enc_caps_{};

  // Enabled features for creating D3D12 AV1 encoder.
  D3D12_VIDEO_ENCODER_AV1_FEATURE_FLAGS enabled_features_{};

  // Picture control flags for each Encoding frame.
  PictureControlFlags picture_ctrl_{};

  // Subregion layout settings.
  D3D12_VIDEO_ENCODER_AV1_PICTURE_CONTROL_SUBREGIONS_LAYOUT_DATA_TILES
  sub_layout_{};

  // The encoding content is a screen content.
  bool is_screen_ = false;

  AV1BitstreamBuilder::SequenceHeader sequence_header_;
  D3D12VideoEncodeDecodedPictureBuffers<kAV1DPBMaxSize> dpb_;
  int picture_id_ = -1;

  D3D12VideoEncoderRateControl current_rate_control_;
};

}  // namespace media

#endif  // MEDIA_GPU_WINDOWS_D3D12_VIDEO_ENCODE_AV1_DELEGATE_H_
