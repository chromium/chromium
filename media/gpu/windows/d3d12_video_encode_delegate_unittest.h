// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_WINDOWS_D3D12_VIDEO_ENCODE_DELEGATE_UNITTEST_H_
#define MEDIA_GPU_WINDOWS_D3D12_VIDEO_ENCODE_DELEGATE_UNITTEST_H_

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

  MOCK_METHOD(bool, Initialize, ());
  MOCK_METHOD2(
      Encode,
      EncoderStatus(const D3D12_VIDEO_ENCODER_ENCODEFRAME_INPUT_ARGUMENTS&,
                    const D3D12_VIDEO_ENCODER_RECONSTRUCTED_PICTURE&));
  MOCK_METHOD(EncoderStatus::Or<uint64_t>,
              GetEncodedBitstreamWrittenBytesCount,
              (),
              (const override));
  MOCK_METHOD(EncoderStatus,
              ReadbackBitstream,
              (base::span<uint8_t>),
              (const override));
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

  Microsoft::WRL::ComPtr<ID3D12Resource> CreateResource(
      const gfx::Size& size,
      VideoPixelFormat format) const;

  std::unique_ptr<D3D12VideoEncodeDelegate> encoder_delegate_;
};

}  // namespace media

#endif  // MEDIA_GPU_WINDOWS_D3D12_VIDEO_ENCODE_DELEGATE_UNITTEST_H_
