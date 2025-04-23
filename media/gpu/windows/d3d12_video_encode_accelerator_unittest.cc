// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/windows/d3d12_video_encode_accelerator.h"

#include "base/test/task_environment.h"
#include "components/viz/common/resources/shared_image_format.h"
#include "gpu/command_buffer/client/test_shared_image_interface.h"
#include "media/base/media_util.h"
#include "media/base/win/d3d12_mocks.h"
#include "media/base/win/d3d12_video_mocks.h"
#include "media/gpu/windows/d3d12_video_encode_delegate.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/gpu_memory_buffer.h"

using media::SetComPointeeAndReturnOk;
using testing::_;
using testing::Invoke;
using testing::Mock;
using testing::NiceMock;
using testing::Return;

namespace media {
namespace {

constexpr VideoCodecProfile kSupportedProfile = H264PROFILE_MAIN;
constexpr gfx::Size kSupportedSize{1920, 1080};

class MockVideoEncodeAcceleratorClient : public VideoEncodeAccelerator::Client {
 public:
  MockVideoEncodeAcceleratorClient() = default;
  ~MockVideoEncodeAcceleratorClient() override = default;

  MOCK_METHOD3(RequireBitstreamBuffers,
               void(unsigned int, const gfx::Size&, size_t));
  MOCK_METHOD2(BitstreamBufferReady,
               void(int32_t, const BitstreamBufferMetadata&));
  MOCK_METHOD1(NotifyErrorStatus, void(const EncoderStatus&));
  MOCK_METHOD1(NotifyEncoderInfoChange, void(const VideoEncoderInfo&));
};

class MockVideoEncoderDelegate : public D3D12VideoEncodeDelegate {
 public:
  MockVideoEncoderDelegate(ID3D12VideoDevice3* video_device,
                           VideoCodecProfile profile)
      : D3D12VideoEncodeDelegate(video_device) {}

  MOCK_METHOD1(Initialize, EncoderStatus(VideoEncodeAccelerator::Config));
  MOCK_METHOD(size_t, GetMaxNumOfRefFrames, (), (const override));
  MOCK_METHOD(bool, SupportsRateControlReconfiguration, (), (const override));
  MOCK_METHOD5(
      Encode,
      EncoderStatus::Or<EncodeResult>(Microsoft::WRL::ComPtr<ID3D12Resource>,
                                      UINT,
                                      const gfx::ColorSpace&,
                                      const BitstreamBuffer&,
                                      const VideoEncoder::EncodeOptions&));
  MOCK_METHOD(EncoderStatus::Or<BitstreamBufferMetadata>,
              EncodeImpl,
              (ID3D12Resource*, UINT, const VideoEncoder::EncodeOptions&),
              (override));

 protected:
  MOCK_METHOD(EncoderStatus,
              InitializeVideoEncoder,
              (const VideoEncodeAccelerator::Config&));
};

class MockVideoEncoderDelegateFactory
    : public D3D12VideoEncodeAccelerator::VideoEncodeDelegateFactoryInterface {
 public:
  ~MockVideoEncoderDelegateFactory() override = default;

  std::unique_ptr<D3D12VideoEncodeDelegate> CreateVideoEncodeDelegate(
      ID3D12VideoDevice3* video_device,
      VideoCodecProfile profile) override {
    auto encoder_delegate =
        std::make_unique<NiceMock<MockVideoEncoderDelegate>>(video_device,
                                                             profile);
    ON_CALL(*encoder_delegate, Initialize(_))
        .WillByDefault(Return(EncoderStatus::Codes::kOk));
    ON_CALL(*encoder_delegate, GetMaxNumOfRefFrames())
        .WillByDefault(Return(16));
    ON_CALL(*encoder_delegate, Encode(_, _, _, _, _))
        .WillByDefault(Invoke([](Microsoft::WRL::ComPtr<ID3D12Resource>, UINT,
                                 const gfx::ColorSpace&,
                                 const BitstreamBuffer& bitstream_buffer,
                                 const VideoEncoder::EncodeOptions&)
                                  -> D3D12VideoEncodeDelegate::EncodeResult {
          return {bitstream_buffer.id()};
        }));
    return std::move(encoder_delegate);
  }

  VideoEncodeAccelerator::SupportedProfiles GetSupportedProfiles(
      ID3D12VideoDevice3* video_device) override {
    EXPECT_TRUE(video_device);
    VideoEncodeAccelerator::SupportedProfile profile(kSupportedProfile,
                                                     kSupportedSize, 30, 1);
    profile.gpu_supported_pixel_formats.push_back(PIXEL_FORMAT_NV12);
    return {profile};
  }
};

}  // namespace

class D3D12VideoEncodeAcceleratorTest : public testing::Test {
 public:
  ~D3D12VideoEncodeAcceleratorTest() override = default;

 protected:
  void SetUp() override {
    mock_device_ = MakeComPtr<NiceMock<D3D12DeviceMock>>();
    mock_video_device3_ = MakeComPtr<NiceMock<D3D12VideoDevice3Mock>>();
    mock_command_list_ = MakeComPtr<NiceMock<D3D12GraphicsCommandListMock>>();
    mock_resource_ = MakeComPtr<NiceMock<D3D12ResourceMock>>();
    COM_ON_CALL(mock_device_, QueryInterface(IID_ID3D12VideoDevice3, _))
        .WillByDefault(SetComPointeeAndReturnOk<1>(mock_video_device3_.Get()));
    COM_ON_CALL(mock_device_, CreateCommandList(_, _, _, _, _, _))
        .WillByDefault(SetComPointeeAndReturnOk<5>(mock_command_list_.Get()));
    COM_ON_CALL(mock_device_, OpenSharedHandle(_, IID_ID3D12Resource, _))
        .WillByDefault(SetComPointeeAndReturnOk<2>(mock_resource_.Get()));

    video_encode_accelerator_.reset(
        new D3D12VideoEncodeAccelerator(mock_device_));
    client_ = std::make_unique<NiceMock<MockVideoEncodeAcceleratorClient>>();
    static_cast<D3D12VideoEncodeAccelerator*>(video_encode_accelerator_.get())
        ->SetEncoderFactoryForTesting(
            std::make_unique<MockVideoEncoderDelegateFactory>());
    test_sii_ = base::MakeRefCounted<gpu::TestSharedImageInterface>();
  }

  VideoEncodeAccelerator::Config SupportedProfileToConfig(
      const VideoEncodeAccelerator::SupportedProfile& profile) {
    return VideoEncodeAccelerator::Config(
        profile.gpu_supported_pixel_formats.front(), profile.max_resolution,
        profile.profile, Bitrate::ConstantBitrate(300000u),
        profile.max_framerate_numerator,
        VideoEncodeAccelerator::Config::StorageType::kShmem,
        VideoEncodeAccelerator::Config::ContentType::kCamera);
  }

  scoped_refptr<VideoFrame> CreateTestVideoFrame() {
    gfx::GpuMemoryBufferHandle fake_handle(
        gfx::DXGIHandle::CreateFakeForTest());

    const auto si_usage = gpu::SHARED_IMAGE_USAGE_CPU_WRITE_ONLY |
                          gpu::SHARED_IMAGE_USAGE_DISPLAY_READ;
    auto shared_image = test_sii_->CreateSharedImage(
        {viz::MultiPlaneFormat::kNV12, kSupportedSize, gfx::ColorSpace(),
         gpu::SharedImageUsageSet(si_usage), "D3D12VideoEncodeAcceleratorTest"},
        gpu::kNullSurfaceHandle, gfx::BufferUsage::GPU_READ,
        std::move(fake_handle));
    return media::VideoFrame::WrapMappableSharedImage(
        std::move(shared_image), test_sii_->GenVerifiedSyncToken(),
        base::NullCallback(), gfx::Rect(kSupportedSize), kSupportedSize,
        base::TimeDelta{});
  }

  void WaitForEncoderTasksToComplete() const {
    base::RunLoop run_loop;
    auto* d3d12_video_encode_accelerator =
        static_cast<D3D12VideoEncodeAccelerator*>(
            video_encode_accelerator_.get());
    d3d12_video_encode_accelerator->GetEncoderTaskRunnerForTesting()->PostTask(
        FROM_HERE, run_loop.QuitClosure());
    run_loop.Run();
  }

  void CheckInputFramesQueueAndBitstreamBuffersAreEitherEmpty() const {
    auto* d3d12_video_encode_accelerator =
        static_cast<D3D12VideoEncodeAccelerator*>(
            video_encode_accelerator_.get());
    base::RunLoop run_loop;
    d3d12_video_encode_accelerator->GetEncoderTaskRunnerForTesting()->PostTask(
        FROM_HERE,
        base::BindOnce(
            [](const D3D12VideoEncodeAccelerator* encoder,
               base::OnceClosure quit_closure) {
              EXPECT_TRUE(encoder->GetInputFramesQueueSizeForTesting() == 0 ||
                          encoder->GetBitstreamBuffersSizeForTesting() == 0);
              std::move(quit_closure).Run();
            },
            d3d12_video_encode_accelerator, run_loop.QuitClosure()));
    run_loop.Run();
  }

  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<MediaLog> media_log_ = std::make_unique<NullMediaLog>();
  Microsoft::WRL::ComPtr<D3D12DeviceMock> mock_device_;
  Microsoft::WRL::ComPtr<D3D12VideoDevice3Mock> mock_video_device3_;
  Microsoft::WRL::ComPtr<D3D12GraphicsCommandListMock> mock_command_list_;
  Microsoft::WRL::ComPtr<D3D12ResourceMock> mock_resource_;
  std::unique_ptr<VideoEncodeAccelerator> video_encode_accelerator_;
  std::unique_ptr<MockVideoEncodeAcceleratorClient> client_;
  scoped_refptr<gpu::TestSharedImageInterface> test_sii_;
};

TEST_F(D3D12VideoEncodeAcceleratorTest, SupportedProfilesCanBeInitialized) {
  auto* d3d12_video_encode_accelerator =
      static_cast<D3D12VideoEncodeAccelerator*>(
          video_encode_accelerator_.get());
  for (const auto& profile :
       d3d12_video_encode_accelerator->GetSupportedProfiles()) {
    auto config = SupportedProfileToConfig(profile);
    EXPECT_TRUE(d3d12_video_encode_accelerator
                    ->Initialize(config, client_.get(), media_log_->Clone())
                    .is_ok());
    EXPECT_CALL(*client_, NotifyEncoderInfoChange(_)).Times(1);
    EXPECT_CALL(*client_, NotifyErrorStatus(_)).Times(0);
    WaitForEncoderTasksToComplete();
    Mock::VerifyAndClearExpectations(&client_);
  }
}

TEST_F(D3D12VideoEncodeAcceleratorTest, RejectsUnsupportedConfig) {
  auto* d3d12_video_encode_accelerator =
      static_cast<D3D12VideoEncodeAccelerator*>(
          video_encode_accelerator_.get());
  auto supported_profiles =
      d3d12_video_encode_accelerator->GetSupportedProfiles();
  EXPECT_FALSE(supported_profiles.empty());
  auto profile = supported_profiles.front();
  auto supported_config = SupportedProfileToConfig(profile);

  constexpr std::pair<VideoCodecProfile, gfx::Size> kUnsupportedConfigs[]{
      {VIDEO_CODEC_PROFILE_UNKNOWN, kSupportedSize},
      {kSupportedProfile,
       gfx::Size(kSupportedSize.width(), kSupportedSize.height() + 1)},
      {kSupportedProfile,
       gfx::Size(kSupportedSize.width() + 1, kSupportedSize.height())},
  };
  for (const auto& [video_codec_profile, size] : kUnsupportedConfigs) {
    auto bad_config = supported_config;
    bad_config.output_profile = video_codec_profile;
    bad_config.input_visible_size = size;
    EXPECT_FALSE(
        d3d12_video_encode_accelerator
            ->Initialize(bad_config, client_.get(), media_log_->Clone())
            .is_ok());
    // Errors should be returned early and InitializeTask() should not be
    // called.
    EXPECT_CALL(*client_, NotifyEncoderInfoChange(_)).Times(0);
    EXPECT_CALL(*client_, NotifyErrorStatus(_)).Times(0);
    WaitForEncoderTasksToComplete();
    Mock::VerifyAndClearExpectations(&client_);
  }
}

TEST_F(D3D12VideoEncodeAcceleratorTest,
       InputFramesQueueAndBitstreamBuffersAreEitherEmpty) {
  auto* d3d12_video_encode_accelerator =
      static_cast<D3D12VideoEncodeAccelerator*>(
          video_encode_accelerator_.get());
  auto supported_profiles =
      d3d12_video_encode_accelerator->GetSupportedProfiles();
  EXPECT_FALSE(supported_profiles.empty());
  auto profile = supported_profiles.front();
  auto supported_config = SupportedProfileToConfig(profile);

  unsigned bitstream_buffer_count = 0;
  size_t bitstream_buffer_size = 0;
  EXPECT_CALL(*client_, RequireBitstreamBuffers(_, _, _))
      .WillOnce(Invoke(
          [&](unsigned int count, const gfx::Size& size, size_t size_in_bytes) {
            bitstream_buffer_count = count;
            bitstream_buffer_size = size_in_bytes;
          }));
  EXPECT_TRUE(
      d3d12_video_encode_accelerator
          ->Initialize(supported_config, client_.get(), media_log_->Clone())
          .is_ok());
  WaitForEncoderTasksToComplete();
  Mock::VerifyAndClearExpectations(&client_);

  std::vector<std::pair<int, base::UnsafeSharedMemoryRegion>> bitstream_buffers;
  for (unsigned i = 0; i < bitstream_buffer_count; ++i) {
    BitstreamBuffer bitstream_buffer(
        i, base::UnsafeSharedMemoryRegion::Create(bitstream_buffer_size),
        bitstream_buffer_size);
    bitstream_buffers.emplace_back(i, bitstream_buffer.DuplicateRegion());
    d3d12_video_encode_accelerator->UseOutputBitstreamBuffer(
        std::move(bitstream_buffer));
  }
  WaitForEncoderTasksToComplete();
  CheckInputFramesQueueAndBitstreamBuffersAreEitherEmpty();

  unsigned frame_count = bitstream_buffer_count * 2;
  for (unsigned i = 0; i < frame_count; ++i) {
    d3d12_video_encode_accelerator->Encode(CreateTestVideoFrame(), false);
    WaitForEncoderTasksToComplete();
    CheckInputFramesQueueAndBitstreamBuffersAreEitherEmpty();
  }
}

}  // namespace media
