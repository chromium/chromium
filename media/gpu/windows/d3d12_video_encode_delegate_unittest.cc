// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/windows/d3d12_video_encode_delegate_unittest.h"

#include "base/rand_util.h"
#include "media/base/win/d3d12_mocks.h"
#include "media/base/win/d3d12_video_mocks.h"
#include "media/gpu/h264_dpb.h"
#include "media/gpu/windows/format_utils.h"
#include "testing/gmock/include/gmock/gmock.h"

using testing::_;
using testing::Mock;
using testing::NiceMock;
using testing::Return;

namespace media {

namespace {

class MockD3D12VideoEncodeDelegate : public D3D12VideoEncodeDelegate {
 public:
  explicit MockD3D12VideoEncodeDelegate(
      Microsoft::WRL::ComPtr<ID3D12VideoDevice3> video_device)
      : D3D12VideoEncodeDelegate(std::move(video_device)) {}
  ~MockD3D12VideoEncodeDelegate() override = default;

  size_t GetMaxNumOfRefFrames() const override { return 8; }
  bool SupportsRateControlReconfiguration() const override { return false; }
  EncoderStatus::Or<BitstreamBufferMetadata> EncodeImpl(
      ID3D12Resource*,
      UINT,
      const VideoEncoder::EncodeOptions&) override {
    return BitstreamBufferMetadata();
  }

 private:
  EncoderStatus InitializeVideoEncoder(
      const VideoEncodeAccelerator::Config&) override {
    video_encoder_wrapper_ =
        video_encoder_wrapper_factory_.Run({}, {}, {}, {}, {}, {}, {});
    return EncoderStatus::Codes::kOk;
  }
};

}  // namespace

MockD3D12VideoProcessorWrapper::MockD3D12VideoProcessorWrapper(
    Microsoft::WRL::ComPtr<ID3D12VideoDevice> device)
    : D3D12VideoProcessorWrapper(std::move(device)) {}

MockD3D12VideoProcessorWrapper::~MockD3D12VideoProcessorWrapper() = default;

MockD3D12VideoEncoderWrapper::MockD3D12VideoEncoderWrapper()
    : D3D12VideoEncoderWrapper(nullptr, nullptr) {}

MockD3D12VideoEncoderWrapper::~MockD3D12VideoEncoderWrapper() = default;

D3D12VideoEncodeDelegateTestBase::D3D12VideoEncodeDelegateTestBase() = default;

D3D12VideoEncodeDelegateTestBase::~D3D12VideoEncodeDelegateTestBase() = default;

std::unique_ptr<D3D12VideoProcessorWrapper>
D3D12VideoEncodeDelegateTestBase::CreateVideoProcessorWrapper(
    Microsoft::WRL::ComPtr<ID3D12VideoDevice>&& video_device) {
  auto video_processor_wrapper =
      std::make_unique<NiceMock<MockD3D12VideoProcessorWrapper>>(video_device);
  ON_CALL(*video_processor_wrapper, Init).WillByDefault(Return(true));
  ON_CALL(*video_processor_wrapper, ProcessFrames).WillByDefault(Return(true));
  return std::move(video_processor_wrapper);
}

std::unique_ptr<D3D12VideoEncoderWrapper>
D3D12VideoEncodeDelegateTestBase::CreateVideoEncoderWrapper(
    ID3D12VideoDevice*,
    D3D12_VIDEO_ENCODER_CODEC,
    const D3D12_VIDEO_ENCODER_PROFILE_DESC&,
    const D3D12_VIDEO_ENCODER_LEVEL_SETTING&,
    DXGI_FORMAT,
    const D3D12_VIDEO_ENCODER_CODEC_CONFIGURATION&,
    const D3D12_VIDEO_ENCODER_PICTURE_RESOLUTION_DESC&) {
  auto video_encoder_wrapper =
      std::make_unique<NiceMock<MockD3D12VideoEncoderWrapper>>();
  ON_CALL(*video_encoder_wrapper, Initialize).WillByDefault(Return(true));
  ON_CALL(*video_encoder_wrapper, Encode)
      .WillByDefault(Return(EncoderStatus::Codes::kOk));
  ON_CALL(*video_encoder_wrapper, ReadbackBitstream)
      .WillByDefault(Return(EncoderStatus::Codes::kOk));
  return std::move(video_encoder_wrapper);
}

MockD3D12VideoProcessorWrapper*
D3D12VideoEncodeDelegateTestBase::GetVideoProcessorWrapper() const {
  return static_cast<MockD3D12VideoProcessorWrapper*>(
      encoder_delegate_->GetVideoProcessorWrapperForTesting());
}

MockD3D12VideoEncoderWrapper*
D3D12VideoEncodeDelegateTestBase::GetVideoEncoderWrapper() const {
  return static_cast<MockD3D12VideoEncoderWrapper*>(
      encoder_delegate_->GetVideoEncoderWrapperForTesting());
}

VideoEncodeAccelerator::Config
D3D12VideoEncodeDelegateTestBase::GetDefaultH264Config() const {
  VideoEncodeAccelerator::Config config;
  config.output_profile = H264PROFILE_BASELINE;
  config.input_format = PIXEL_FORMAT_NV12;
  config.input_visible_size = gfx::Size(1280, 720);
  config.bitrate = Bitrate::ConstantBitrate(300000u);
  config.framerate = 30;
  config.gop_length = 3000;
  return config;
}

Microsoft::WRL::ComPtr<ID3D12Resource>
D3D12VideoEncodeDelegateTestBase::CreateResource(
    const gfx::Size& size,
    VideoPixelFormat format) const {
  auto input_frame = MakeComPtr<NiceMock<D3D12ResourceMock>>();
  ON_CALL(*input_frame.Get(), GetDesc())
      .WillByDefault(Return(D3D12_RESOURCE_DESC{
          .Width = static_cast<UINT64>(size.width()),
          .Height = static_cast<UINT>(size.height()),
          .Format = VideoPixelFormatToDxgiFormat(format),
      }));
  return input_frame;
}

class D3D12VideoEncodeDelegateTest : public D3D12VideoEncodeDelegateTestBase {
 protected:
  void SetUp() override {
    device_ = MakeComPtr<NiceMock<D3D12DeviceMock>>();
    video_device3_ = MakeComPtr<NiceMock<D3D12VideoDevice3Mock>>();
    ON_CALL(*video_device3_.Get(), QueryInterface(IID_ID3D12Device, _))
        .WillByDefault(SetComPointeeAndReturnOk<1>(device_.Get()));
    ON_CALL(*video_device3_.Get(), QueryInterface(IID_ID3D12VideoDevice1, _))
        .WillByDefault(SetComPointeeAndReturnOk<1>(video_device3_.Get()));
    encoder_delegate_ =
        std::make_unique<MockD3D12VideoEncodeDelegate>(video_device3_);
    encoder_delegate_->SetFactoriesForTesting(
        base::BindRepeating(&CreateVideoEncoderWrapper),
        base::BindRepeating(&CreateVideoProcessorWrapper));
  }

  Microsoft::WRL::ComPtr<D3D12DeviceMock> device_;
  Microsoft::WRL::ComPtr<D3D12VideoDevice3Mock> video_device3_;
};

TEST_F(D3D12VideoEncodeDelegateTest, Initialize) {
  EXPECT_TRUE(encoder_delegate_->Initialize(GetDefaultH264Config()).is_ok());
  EXPECT_EQ(encoder_delegate_->GetFormatForTesting(), DXGI_FORMAT_NV12);
}

TEST_F(D3D12VideoEncodeDelegateTest, P010InputFormatFor10BitProfile) {
  VideoEncodeAccelerator::Config config = GetDefaultH264Config();
  config.input_format = PIXEL_FORMAT_P010LE;
  config.output_profile = H264PROFILE_HIGH10PROFILE;
  EXPECT_TRUE(encoder_delegate_->Initialize(config).is_ok());
  EXPECT_EQ(encoder_delegate_->GetFormatForTesting(), DXGI_FORMAT_P010);
}

TEST_F(D3D12VideoEncodeDelegateTest, ExternalRateControl) {
  VideoEncodeAccelerator::Config config = GetDefaultH264Config();
  config.bitrate = Bitrate::ExternalRateControl();
  EXPECT_EQ(encoder_delegate_->Initialize(config).code(),
            EncoderStatus::Codes::kOk);
}

class D3D12VideoEncodeDelegateTestWithProcessFrame
    : public D3D12VideoEncodeDelegateTest,
      public testing::WithParamInterface<bool> {};

TEST_P(D3D12VideoEncodeDelegateTestWithProcessFrame, EncodeFrame) {
  bool do_process_frame = GetParam();
  VideoEncodeAccelerator::Config config = GetDefaultH264Config();
  ASSERT_TRUE(encoder_delegate_->Initialize(config).is_ok());

  gfx::Size input_size = config.input_visible_size;
  if (do_process_frame) {
    input_size += {1, 0};
  }
  auto input_frame = CreateResource(input_size, config.input_format);
  gfx::ColorSpace color_space = gfx::ColorSpace::CreateSRGB();
  constexpr size_t kPayloadSize = 1024;
  auto shared_memory = base::UnsafeSharedMemoryRegion::Create(kPayloadSize);
  BitstreamBuffer bitstream_buffer(base::RandInt(0, H264DPB::kDPBMaxSize - 1),
                                   shared_memory.Duplicate(), kPayloadSize);
  if (do_process_frame) {
    EXPECT_CALL(*GetVideoProcessorWrapper(), ProcessFrames)
        .WillOnce([&](ID3D12Resource*, UINT, const gfx::ColorSpace&,
                      const gfx::Rect& input_rectangle, ID3D12Resource*, UINT,
                      const gfx::ColorSpace&,
                      const gfx::Rect& output_rectangle) {
          EXPECT_EQ(input_rectangle.width(), input_size.width());
          EXPECT_EQ(input_rectangle.height(), input_size.height());
          EXPECT_EQ(output_rectangle.width(),
                    config.input_visible_size.width());
          EXPECT_EQ(output_rectangle.height(),
                    config.input_visible_size.height());
          return true;
        });
  } else {
    EXPECT_CALL(*GetVideoProcessorWrapper(), ProcessFrames).Times(0);
  }
  EXPECT_CALL(*GetVideoEncoderWrapper(), GetEncodedBitstreamWrittenBytesCount)
      .WillOnce(Return(kPayloadSize));
  auto result_or_error =
      encoder_delegate_->Encode(input_frame, 0, color_space, bitstream_buffer,
                                VideoEncoder::EncodeOptions());
  Mock::VerifyAndClearExpectations(GetVideoProcessorWrapper());
  ASSERT_TRUE(result_or_error.has_value());

  auto [bitstream_buffer_id, metadata] = std::move(result_or_error).value();
  EXPECT_EQ(bitstream_buffer_id, bitstream_buffer.id());
  EXPECT_EQ(metadata.encoded_color_space, color_space);
  EXPECT_EQ(metadata.payload_size_bytes, kPayloadSize);
}

INSTANTIATE_TEST_SUITE_P(,
                         D3D12VideoEncodeDelegateTestWithProcessFrame,
                         testing::Values(true, false));

}  // namespace media
