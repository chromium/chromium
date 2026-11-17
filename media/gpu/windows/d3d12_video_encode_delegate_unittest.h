// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_WINDOWS_D3D12_VIDEO_ENCODE_DELEGATE_UNITTEST_H_
#define MEDIA_GPU_WINDOWS_D3D12_VIDEO_ENCODE_DELEGATE_UNITTEST_H_

#include <optional>
#include <utility>
#include <vector>

#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "media/gpu/windows/d3d12_video_encode_delegate.h"
#include "media/gpu/windows/d3d12_video_encoder_wrapper.h"
#include "media/gpu/windows/d3d12_video_processor_wrapper.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace media {

class MockD3D12VideoProcessorWrapper : public D3D12VideoProcessorWrapper {
 public:
  explicit MockD3D12VideoProcessorWrapper(
      Microsoft::WRL::ComPtr<ID3D12VideoDevice> device);
  ~MockD3D12VideoProcessorWrapper() override;

  MOCK_METHOD(bool, Init, ());
  MOCK_METHOD(bool,
              CheckVideoProcessorSupport,
              (UINT,
               UINT,
               DXGI_FORMAT,
               const gfx::ColorSpace&,
               DXGI_FORMAT,
               const gfx::ColorSpace&));
  MOCK_METHOD(D3D12FenceAndValue,
              ProcessFrames,
              (ID3D12Resource*,
               UINT,
               const gfx::ColorSpace&,
               const gfx::Rect&,
               ID3D12Resource*,
               UINT,
               const gfx::ColorSpace&,
               const gfx::Rect&));
};

class MockD3D12VideoEncoderWrapper : public D3D12VideoEncoderWrapper {
 public:
  MockD3D12VideoEncoderWrapper();
  ~MockD3D12VideoEncoderWrapper() override;

  MOCK_METHOD(bool, Initialize, (uint32_t, uint64_t), (override));
  MOCK_METHOD(bool, Wait, (D3D12FenceAndValue));
  MOCK_METHOD2(
      Encode,
      EncoderStatus(const D3D12_VIDEO_ENCODER_ENCODEFRAME_INPUT_ARGUMENTS&,
                    const D3D12_VIDEO_ENCODER_RECONSTRUCTED_PICTURE&));
  MOCK_METHOD((base::expected<ScopedD3D12ResourceMap, EncoderStatus>),
              GetEncoderOutputMetadata,
              (),
              (const, override));
  MOCK_METHOD(EncoderStatus,
              ReadbackBitstream,
              (base::span<uint8_t>),
              (const, override));
};

class D3D12VideoEncodeDelegateTestBase : public ::testing::Test {
 public:
  D3D12VideoEncodeDelegateTestBase();
  ~D3D12VideoEncodeDelegateTestBase() override;

 protected:
  static std::unique_ptr<D3D12VideoProcessorWrapper>
  CreateVideoProcessorWrapper(
      Microsoft::WRL::ComPtr<ID3D12VideoDevice>&& video_device);

  static std::unique_ptr<D3D12VideoEncoderWrapper> CreateVideoEncoderWrapper(
      ID3D12VideoDevice*,
      D3D12_VIDEO_ENCODER_CODEC,
      const D3D12_VIDEO_ENCODER_PROFILE_DESC&,
      const D3D12_VIDEO_ENCODER_LEVEL_SETTING&,
      DXGI_FORMAT,
      const D3D12_VIDEO_ENCODER_CODEC_CONFIGURATION&,
      const D3D12_VIDEO_ENCODER_PICTURE_RESOLUTION_DESC&);

  MockD3D12VideoProcessorWrapper* GetVideoProcessorWrapper() const;

  MockD3D12VideoEncoderWrapper* GetVideoEncoderWrapper() const;

  VideoEncodeAccelerator::Config GetDefaultH264Config() const;

  ScopedD3D12ResourceMap GetEncoderOutputMetadataResourceMap(
      size_t bitstream_size) const;

  Microsoft::WRL::ComPtr<ID3D12Resource> CreateResource(
      const gfx::Size& size,
      VideoPixelFormat format) const;

  void EnableFeature(const base::Feature& feature);

  void DisableFeature(const base::Feature& feature);

  // Initializes `encoder_delegate_` with `config` and encodes one frame per
  // entry of `timestamps`. The mocked driver reports every frame as
  // `frame_size` bytes. Returns the QP of each encoded frame.
  // `encoder_delegate_` must be newly created and not yet initialized.
  std::vector<int> InitializeAndEncodeFrames(
      const VideoEncodeAccelerator::Config& config,
      size_t frame_size,
      const std::vector<base::TimeDelta>& timestamps);

  // Encodes the same `num_frames` frames of `frame_size` bytes twice on newly
  // created delegates: once spaced `interval` apart and once at
  // `config.framerate`. Returns the QP of the last frame of each run as
  // `{qp_at_interval, qp_at_config_framerate}`.
  std::pair<int, int> GetLastQpAtIntervalAndAtConfigFramerate(
      const VideoEncodeAccelerator::Config& config,
      size_t frame_size,
      int num_frames,
      base::TimeDelta interval);

  // Replaces `encoder_delegate_` with a new, uninitialized delegate. Codec
  // fixtures that use `GetLastQpAtIntervalAndAtConfigFramerate()` override
  // this.
  virtual void CreateEncoderDelegate();

  std::unique_ptr<D3D12VideoEncodeDelegate> encoder_delegate_;
  std::optional<base::test::ScopedFeatureList> scoped_feature_list_;
};

}  // namespace media

#endif  // MEDIA_GPU_WINDOWS_D3D12_VIDEO_ENCODE_DELEGATE_UNITTEST_H_
