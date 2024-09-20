// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ptr_util.h"
#include "base/memory/unsafe_shared_memory_region.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/task_environment.h"
#include "gpu/config/gpu_driver_bug_workarounds.h"
#include "gpu/config/gpu_info.h"
#include "gpu/config/gpu_preferences.h"
#include "media/mojo/clients/mojo_media_log_service.h"
#include "media/mojo/mojom/video_encode_accelerator.mojom.h"
#include "media/mojo/services/mojo_video_encode_accelerator_service.h"
#include "media/video/fake_video_encode_accelerator.h"
#include "media/video/video_encode_accelerator.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/self_owned_associated_receiver.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace gpu {
struct GpuPreferences;
}  // namespace gpu

using ::testing::_;

namespace media {

static const gfx::Size kInputVisibleSize(64, 48);

std::unique_ptr<VideoEncodeAccelerator> CreateAndInitializeFakeVEA(
    bool will_initialization_succeed,
    const VideoEncodeAccelerator::Config& config,
    VideoEncodeAccelerator::Client* client,
    const gpu::GpuPreferences& gpu_preferences,
    const gpu::GpuDriverBugWorkarounds& gpu_workarounds,
    const gpu::GPUInfo::GPUDevice& gpu_device,
    std::unique_ptr<MediaLog> media_log,
    MojoVideoEncodeAcceleratorService::GetCommandBufferHelperCB
        get_command_buffer_helper_cb,
    scoped_refptr<base::SingleThreadTaskRunner> gpu_task_runner) {
  // Use FakeVEA as scoped_ptr to guarantee proper destruction via Destroy().
  auto vea = std::make_unique<FakeVideoEncodeAccelerator>(
      base::SingleThreadTaskRunner::GetCurrentDefault());
  vea->SetWillInitializationSucceed(will_initialization_succeed);
  const bool result = vea->Initialize(config, client, media_log->Clone());

  // Mimic the behaviour of GpuVideoEncodeAcceleratorFactory::CreateVEA().
  return result ? base::WrapUnique<VideoEncodeAccelerator>(vea.release())
                : nullptr;
}

class MockMojoVideoEncodeAcceleratorClient
    : public mojom::VideoEncodeAcceleratorClient {
 public:
  MockMojoVideoEncodeAcceleratorClient() = default;

  MockMojoVideoEncodeAcceleratorClient(
      const MockMojoVideoEncodeAcceleratorClient&) = delete;
  MockMojoVideoEncodeAcceleratorClient& operator=(
      const MockMojoVideoEncodeAcceleratorClient&) = delete;

  MOCK_METHOD3(RequireBitstreamBuffers,
               void(uint32_t, const gfx::Size&, uint32_t));
  MOCK_METHOD2(BitstreamBufferReady,
               void(int32_t, const media::BitstreamBufferMetadata&));
  MOCK_METHOD1(NotifyErrorStatus, void(const EncoderStatus&));
  MOCK_METHOD1(NotifyEncoderInfoChange, void(const VideoEncoderInfo& info));
};

// Test harness for a MojoVideoEncodeAcceleratorService; the tests manipulate it
// via its MojoVideoEncodeAcceleratorService interface while observing a
// "remote" mojo::VideoEncodeAcceleratorClient (that we keep inside a Mojo
// binding). The class under test uses a FakeVideoEncodeAccelerator as
// implementation.
class MojoVideoEncodeAcceleratorServiceTest : public ::testing::Test {
 public:
  MojoVideoEncodeAcceleratorServiceTest() = default;

  MojoVideoEncodeAcceleratorServiceTest(
      const MojoVideoEncodeAcceleratorServiceTest&) = delete;
  MojoVideoEncodeAcceleratorServiceTest& operator=(
      const MojoVideoEncodeAcceleratorServiceTest&) = delete;

  void TearDown() override {
    // The destruction of a mojo::SelfOwnedReceiver closes the bound message
    // pipe but does not destroy the implementation object: needs to happen
    // manually, otherwise we leak it. This only applies if BindAndInitialize()
    // has been called.
    if (mojo_vea_receiver_)
      mojo_vea_receiver_->Close();
  }

  // Creates the class under test, configuring the underlying FakeVEA to succeed
  // upon initialization (by default) or not.
  void CreateMojoVideoEncodeAccelerator(
      bool will_fake_vea_initialization_succeed = true) {
    mojo_vea_service_ = std::make_unique<MojoVideoEncodeAcceleratorService>(
        base::BindRepeating(&CreateAndInitializeFakeVEA,
                            will_fake_vea_initialization_succeed),
        gpu::GpuPreferences(), gpu::GpuDriverBugWorkarounds(),
        gpu::GPUInfo::GPUDevice(),
        MojoVideoEncodeAcceleratorService::GetCommandBufferHelperCB(),
        base::SingleThreadTaskRunner::GetCurrentDefault());
  }

  void BindAndInitialize() {
    // Create an Mojo VEA Client remote and bind it to our Mock.
    mojo::PendingAssociatedRemote<mojom::VideoEncodeAcceleratorClient>
        pending_client_remote;
    auto pending_client_receiver =
        pending_client_remote.InitWithNewEndpointAndPassReceiver();
    pending_client_receiver.EnableUnassociatedUsage();

    mojo_vea_receiver_ = mojo::MakeSelfOwnedAssociatedReceiver(
        std::make_unique<MockMojoVideoEncodeAcceleratorClient>(),
        std::move(pending_client_receiver));

    EXPECT_CALL(*mock_mojo_vea_client(),
                RequireBitstreamBuffers(_, kInputVisibleSize, _));

    constexpr media::Bitrate kInitialBitrate =
        media::Bitrate::ConstantBitrate(100000u);
    const media::VideoEncodeAccelerator::Config config(
        PIXEL_FORMAT_I420, kInputVisibleSize, H264PROFILE_MIN, kInitialBitrate,
        media::VideoEncodeAccelerator::kDefaultFramerate,
        VideoEncodeAccelerator::Config::StorageType::kGpuMemoryBuffer,
        VideoEncodeAccelerator::Config::ContentType::kCamera);

    mojo::PendingReceiver<mojom::MediaLog> media_log_pending_receiver;
    auto media_log_pending_remote =
        media_log_pending_receiver.InitWithNewPipeAndPassRemote();

    mojo_vea_service()->Initialize(
        config, std::move(pending_client_remote),
        std::move(media_log_pending_remote),
        base::BindOnce([](bool success) { ASSERT_TRUE(success); }));
    base::RunLoop().RunUntilIdle();
  }

  MojoVideoEncodeAcceleratorService* mojo_vea_service() {
    return mojo_vea_service_.get();
  }

  MockMojoVideoEncodeAcceleratorClient* mock_mojo_vea_client() const {
    return static_cast<media::MockMojoVideoEncodeAcceleratorClient*>(
        mojo_vea_receiver_->impl());
  }

  FakeVideoEncodeAccelerator* fake_vea() const {
    return static_cast<FakeVideoEncodeAccelerator*>(
        mojo_vea_service_->encoder_.get());
  }

 private:
  base::test::SingleThreadTaskEnvironment task_environment_;

  mojo::SelfOwnedAssociatedReceiverRef<mojom::VideoEncodeAcceleratorClient>
      mojo_vea_receiver_;

  // The class under test.
  std::unique_ptr<MojoVideoEncodeAcceleratorService> mojo_vea_service_;
};

// This test verifies the BindAndInitialize() communication prologue in
// isolation.
TEST_F(MojoVideoEncodeAcceleratorServiceTest,
       InitializeAndRequireBistreamBuffers) {
  CreateMojoVideoEncodeAccelerator();
  BindAndInitialize();
}

// This test verifies the BindAndInitialize() communication prologue followed by
// a sharing of a single bitstream buffer and the Encode() of one frame.
TEST_F(MojoVideoEncodeAcceleratorServiceTest, EncodeOneFrame) {
  CreateMojoVideoEncodeAccelerator();
  BindAndInitialize();

  const int32_t kBitstreamBufferId = 17;
  {
    const uint64_t kShMemSize = fake_vea()->minimum_output_buffer_size();
    auto region = base::UnsafeSharedMemoryRegion::Create(kShMemSize);

    mojo_vea_service()->UseOutputBitstreamBuffer(kBitstreamBufferId,
                                                 std::move(region));
    base::RunLoop().RunUntilIdle();
  }

  {
    const auto video_frame = VideoFrame::CreateBlackFrame(kInputVisibleSize);
    EXPECT_CALL(*mock_mojo_vea_client(),
                BitstreamBufferReady(kBitstreamBufferId, _));

    media::VideoEncoder::EncodeOptions options(/* key_frame */ true);
    mojo_vea_service()->Encode(video_frame, options, base::DoNothing());
    base::RunLoop().RunUntilIdle();
  }
}

// Tests that a RequestEncodingParametersChange() ripples through correctly.
TEST_F(MojoVideoEncodeAcceleratorServiceTest, EncodingParametersChange) {
  CreateMojoVideoEncodeAccelerator();
  BindAndInitialize();

  const uint32_t kNewBitrate = 123123u;
  const uint32_t kNewFramerate = 321321u;
  VideoBitrateAllocation bitrate_allocation;
  bitrate_allocation.SetBitrate(0, 0, kNewBitrate);
  mojo_vea_service()->RequestEncodingParametersChangeWithLayers(
      bitrate_allocation, kNewFramerate, std::nullopt);
  base::RunLoop().RunUntilIdle();

  ASSERT_TRUE(fake_vea());
  VideoBitrateAllocation expected_allocation;
  expected_allocation.SetBitrate(0, 0, kNewBitrate);
  EXPECT_EQ(expected_allocation,
            fake_vea()->stored_bitrate_allocations().back());
  EXPECT_TRUE(fake_vea()->stored_frame_sizes().empty());
}

// Tests that a RequestEncodingParametersChange() ripples through correctly.
TEST_F(MojoVideoEncodeAcceleratorServiceTest,
       EncodingParametersWithBitrateAllocation) {
  CreateMojoVideoEncodeAccelerator();
  BindAndInitialize();

  const uint32_t kNewFramerate = 321321u;
  const size_t kMaxNumBitrates = VideoBitrateAllocation::kMaxSpatialLayers *
                                 VideoBitrateAllocation::kMaxTemporalLayers;

  // Verify translation of VideoBitrateAllocation into vector of bitrates for
  // everything from empty array up to max number of layers.
  VideoBitrateAllocation bitrate_allocation;
  for (size_t i = 0; i <= kMaxNumBitrates; ++i) {
    if (i > 0) {
      uint32_t layer_bitrate = i * 1000;
      const size_t si = (i - 1) / VideoBitrateAllocation::kMaxTemporalLayers;
      const size_t ti = (i - 1) % VideoBitrateAllocation::kMaxTemporalLayers;
      bitrate_allocation.SetBitrate(si, ti, layer_bitrate);
    }

    mojo_vea_service()->RequestEncodingParametersChangeWithLayers(
        bitrate_allocation, kNewFramerate, std::nullopt);
    base::RunLoop().RunUntilIdle();

    ASSERT_TRUE(fake_vea());
    EXPECT_EQ(bitrate_allocation,
              fake_vea()->stored_bitrate_allocations().back());
    EXPECT_TRUE(fake_vea()->stored_frame_sizes().empty());
  }
}

// Tests that a RequestEncodingParametersChange() ripples through correctly.
TEST_F(MojoVideoEncodeAcceleratorServiceTest,
       EncodingParametersWithBitrateAllocationAndFrameSize) {
  CreateMojoVideoEncodeAccelerator();
  BindAndInitialize();

  const uint32_t kNewFramerate = 321321u;
  const size_t kMaxNumBitrates = VideoBitrateAllocation::kMaxSpatialLayers *
                                 VideoBitrateAllocation::kMaxTemporalLayers;

  // Verify translation of VideoBitrateAllocation into vector of bitrates for
  // everything from empty array up to max number of layers.
  VideoBitrateAllocation bitrate_allocation;
  // Verify frame size from 256 x 144 to 256*kMaxSpatialLayers x
  // 144*kMaxSpatialLayers.
  const int kFrameSizeWidthBase = 256;
  const int kFrameSizeHeightBase = 144;
  gfx::Size frame_size = gfx::Size(kFrameSizeWidthBase, kFrameSizeHeightBase);
  for (size_t i = 0; i <= kMaxNumBitrates; ++i) {
    if (i > 0) {
      uint32_t layer_bitrate = i * 1000;
      const size_t si = (i - 1) / VideoBitrateAllocation::kMaxTemporalLayers;
      const size_t ti = (i - 1) % VideoBitrateAllocation::kMaxTemporalLayers;
      bitrate_allocation.SetBitrate(si, ti, layer_bitrate);
    }

    if (i < VideoBitrateAllocation::kMaxSpatialLayers) {
      frame_size = gfx::Size(kFrameSizeWidthBase * (i + 1),
                             kFrameSizeHeightBase * (i + 1));
    }

    EXPECT_CALL(*mock_mojo_vea_client(),
                RequireBitstreamBuffers(_, frame_size, _));
    mojo_vea_service()->RequestEncodingParametersChangeWithLayers(
        bitrate_allocation, kNewFramerate, frame_size);
    base::RunLoop().RunUntilIdle();

    ASSERT_TRUE(fake_vea());
    EXPECT_EQ(bitrate_allocation,
              fake_vea()->stored_bitrate_allocations().back());
    EXPECT_EQ(frame_size, fake_vea()->stored_frame_sizes().back());
  }
}

// This test verifies that MojoVEA::Initialize() fails with an invalid |client|.
TEST_F(MojoVideoEncodeAcceleratorServiceTest,
       InitializeWithInvalidClientFails) {
  CreateMojoVideoEncodeAccelerator();

  mojo::PendingAssociatedRemote<mojom::VideoEncodeAcceleratorClient>
      invalid_mojo_vea_client;

  constexpr media::Bitrate kInitialBitrate =
      media::Bitrate::ConstantBitrate(100000u);
  const media::VideoEncodeAccelerator::Config config(
      PIXEL_FORMAT_I420, kInputVisibleSize, H264PROFILE_MIN, kInitialBitrate,
      media::VideoEncodeAccelerator::kDefaultFramerate,
      VideoEncodeAccelerator::Config::StorageType::kGpuMemoryBuffer,
      VideoEncodeAccelerator::Config::ContentType::kCamera);
  mojo::PendingReceiver<mojom::MediaLog> media_log_pending_receiver;
  auto media_log_pending_remote =
      media_log_pending_receiver.InitWithNewPipeAndPassRemote();

  mojo_vea_service()->Initialize(
      config, std::move(invalid_mojo_vea_client),
      std::move(media_log_pending_remote),
      base::BindOnce([](bool success) { ASSERT_FALSE(success); }));
  base::RunLoop().RunUntilIdle();
}

// This test verifies that when FakeVEA is configured to fail upon start,
// MojoVEA::Initialize() causes a NotifyError().
TEST_F(MojoVideoEncodeAcceleratorServiceTest, InitializeFailure) {
  CreateMojoVideoEncodeAccelerator(
      false /* will_fake_vea_initialization_succeed */);

  mojo::PendingAssociatedRemote<mojom::VideoEncodeAcceleratorClient>
      mojo_vea_client;
  auto mojo_vea_receiver = mojo::MakeSelfOwnedAssociatedReceiver(
      std::make_unique<MockMojoVideoEncodeAcceleratorClient>(),
      mojo_vea_client.InitWithNewEndpointAndPassReceiver());

  constexpr media::Bitrate kInitialBitrate =
      media::Bitrate::ConstantBitrate(100000u);
  const media::VideoEncodeAccelerator::Config config(
      PIXEL_FORMAT_I420, kInputVisibleSize, H264PROFILE_MIN, kInitialBitrate,
      media::VideoEncodeAccelerator::kDefaultFramerate,
      VideoEncodeAccelerator::Config::StorageType::kGpuMemoryBuffer,
      VideoEncodeAccelerator::Config::ContentType::kCamera);
  mojo::PendingReceiver<mojom::MediaLog> media_log_pending_receiver;
  auto media_log_pending_remote =
      media_log_pending_receiver.InitWithNewPipeAndPassRemote();

  mojo_vea_service()->Initialize(
      config, std::move(mojo_vea_client), std::move(media_log_pending_remote),
      base::BindOnce([](bool success) { ASSERT_FALSE(success); }));
  base::RunLoop().RunUntilIdle();

  mojo_vea_receiver->Close();
}

// This test verifies that UseOutputBitstreamBuffer() with a wrong ShMem size
// causes NotifyError().
TEST_F(MojoVideoEncodeAcceleratorServiceTest,
       UseOutputBitstreamBufferWithWrongSizeFails) {
  CreateMojoVideoEncodeAccelerator();
  BindAndInitialize();

  const int32_t kBitstreamBufferId = 17;
  const uint64_t wrong_size = fake_vea()->minimum_output_buffer_size() / 2;
  auto region = base::UnsafeSharedMemoryRegion::Create(wrong_size);

  EXPECT_CALL(*mock_mojo_vea_client(), NotifyErrorStatus);

  mojo_vea_service()->UseOutputBitstreamBuffer(kBitstreamBufferId,
                                               std::move(region));
  base::RunLoop().RunUntilIdle();
}

// This test verifies that Encode() with wrong coded size causes NotifyError().
TEST_F(MojoVideoEncodeAcceleratorServiceTest, EncodeWithWrongSizeFails) {
  CreateMojoVideoEncodeAccelerator();
  BindAndInitialize();

  // We should send a UseOutputBitstreamBuffer() first but in unit tests we can
  // skip that prologue.

  const gfx::Size wrong_size(kInputVisibleSize.width() / 2,
                             kInputVisibleSize.height() / 2);
  const auto video_frame = VideoFrame::CreateBlackFrame(wrong_size);

  EXPECT_CALL(*mock_mojo_vea_client(), NotifyErrorStatus);

  media::VideoEncoder::EncodeOptions options(/* key_frame */ true);
  mojo_vea_service()->Encode(video_frame, options, base::DoNothing());
  base::RunLoop().RunUntilIdle();
}

// This test verifies that an any mojom::VEA method call (e.g. Encode(),
// UseOutputBitstreamBuffer() etc) before MojoVEA::Initialize() is ignored (we
// can't expect NotifyError()s since there's no mojo client registered).
TEST_F(MojoVideoEncodeAcceleratorServiceTest, CallsBeforeInitializeAreIgnored) {
  CreateMojoVideoEncodeAccelerator();
  {
    const auto video_frame = VideoFrame::CreateBlackFrame(kInputVisibleSize);
    media::VideoEncoder::EncodeOptions options(/* key_frame */ true);
    mojo_vea_service()->Encode(video_frame, options, base::DoNothing());
    base::RunLoop().RunUntilIdle();
  }
  {
    const int32_t kBitstreamBufferId = 17;
    const uint64_t kShMemSize = 10;
    auto region = base::UnsafeSharedMemoryRegion::Create(kShMemSize);
    mojo_vea_service()->UseOutputBitstreamBuffer(kBitstreamBufferId,
                                                 std::move(region));
    base::RunLoop().RunUntilIdle();
  }
  {
    const uint32_t kNewBitrate = 123123u;
    const uint32_t kNewFramerate = 321321u;
    media::VideoBitrateAllocation bitrate_allocation;
    bitrate_allocation.SetBitrate(0, 0, kNewBitrate);
    mojo_vea_service()->RequestEncodingParametersChangeWithLayers(
        bitrate_allocation, kNewFramerate, std::nullopt);
    base::RunLoop().RunUntilIdle();
  }
}

// This test verifies that IsFlushSupported/Flush on FakeVEA.
TEST_F(MojoVideoEncodeAcceleratorServiceTest, IsFlushSupportedAndFlush) {
  CreateMojoVideoEncodeAccelerator();
  BindAndInitialize();

  ASSERT_TRUE(fake_vea());

  // media::VideoEncodeAccelerator::IsFlushSupported and Flush are return
  // false as default, so here expect false for both IsFlushSupported and
  // Flush.
  auto flush_support =
      base::BindOnce([](bool status) { EXPECT_EQ(status, false); });
  mojo_vea_service()->IsFlushSupported(std::move(flush_support));
  base::RunLoop().RunUntilIdle();

  auto flush_callback =
      base::BindOnce([](bool status) { EXPECT_EQ(status, false); });
  mojo_vea_service()->IsFlushSupported(std::move(flush_callback));
}

}  // namespace media
