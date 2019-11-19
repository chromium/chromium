// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "gpu/config/gpu_preferences.h"
#include "media/mojo/mojom/video_encode_accelerator.mojom.h"
#include "media/mojo/services/mojo_video_encode_accelerator_service.h"
#include "media/video/fake_video_encode_accelerator.h"
#include "media/video/video_encode_accelerator.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
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
    const gpu::GpuPreferences& gpu_preferences) {
  // Use FakeVEA as scoped_ptr to guarantee proper destruction via Destroy().
  auto vea = std::make_unique<FakeVideoEncodeAccelerator>(
      base::ThreadTaskRunnerHandle::Get());
  vea->SetWillInitializationSucceed(will_initialization_succeed);
  const bool result = vea->Initialize(config, client);

  // Mimic the behaviour of GpuVideoEncodeAcceleratorFactory::CreateVEA().
  return result ? base::WrapUnique<VideoEncodeAccelerator>(vea.release())
                : nullptr;
}

class MockMojoVideoEncodeAcceleratorClient
    : public mojom::VideoEncodeAcceleratorClient {
 public:
  MockMojoVideoEncodeAcceleratorClient() = default;

  MOCK_METHOD3(RequireBitstreamBuffers,
               void(uint32_t, const gfx::Size&, uint32_t));
  MOCK_METHOD2(BitstreamBufferReady,
               void(int32_t, const media::BitstreamBufferMetadata&));
  MOCK_METHOD1(NotifyError, void(VideoEncodeAccelerator::Error));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockMojoVideoEncodeAcceleratorClient);
};

// Test harness for a MojoVideoEncodeAcceleratorService; the tests manipulate it
// via its MojoVideoEncodeAcceleratorService interface while observing a
// "remote" mojo::VideoEncodeAcceleratorClient (that we keep inside a Mojo
// binding). The class under test uses a FakeVideoEncodeAccelerator as
// implementation.
class MojoVideoEncodeAcceleratorServiceTest : public ::testing::Test {
 public:
  MojoVideoEncodeAcceleratorServiceTest() = default;

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
        base::Bind(&CreateAndInitializeFakeVEA,
                   will_fake_vea_initialization_succeed),
        gpu::GpuPreferences());
  }

  void BindAndInitialize() {
    // Create an Mojo VEA Client remote and bind it to our Mock.
    mojo::PendingRemote<mojom::VideoEncodeAcceleratorClient> mojo_vea_client;
    mojo_vea_receiver_ = mojo::MakeSelfOwnedReceiver(
        std::make_unique<MockMojoVideoEncodeAcceleratorClient>(),
        mojo_vea_client.InitWithNewPipeAndPassReceiver());

    EXPECT_CALL(*mock_mojo_vea_client(),
                RequireBitstreamBuffers(_, kInputVisibleSize, _));

    const uint32_t kInitialBitrate = 100000u;
    const media::VideoEncodeAccelerator::Config config(
        PIXEL_FORMAT_I420, kInputVisibleSize, H264PROFILE_MIN, kInitialBitrate);
    mojo_vea_service()->Initialize(
        config, std::move(mojo_vea_client),
        base::Bind([](bool success) { ASSERT_TRUE(success); }));
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

  mojo::SelfOwnedReceiverRef<mojom::VideoEncodeAcceleratorClient>
      mojo_vea_receiver_;

  // The class under test.
  std::unique_ptr<MojoVideoEncodeAcceleratorService> mojo_vea_service_;

  DISALLOW_COPY_AND_ASSIGN(MojoVideoEncodeAcceleratorServiceTest);
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
    auto handle = mojo::SharedBufferHandle::Create(kShMemSize);

    mojo_vea_service()->UseOutputBitstreamBuffer(kBitstreamBufferId,
                                                 std::move(handle));
    base::RunLoop().RunUntilIdle();
  }

  {
    const auto video_frame = VideoFrame::CreateBlackFrame(kInputVisibleSize);
    EXPECT_CALL(*mock_mojo_vea_client(),
                BitstreamBufferReady(kBitstreamBufferId, _));

    mojo_vea_service()->Encode(video_frame, true /* is_keyframe */,
                               base::DoNothing());
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
  mojo_vea_service()->RequestEncodingParametersChange(bitrate_allocation,
                                                      kNewFramerate);
  base::RunLoop().RunUntilIdle();

  ASSERT_TRUE(fake_vea());
  VideoBitrateAllocation expected_allocation;
  expected_allocation.SetBitrate(0, 0, kNewBitrate);
  EXPECT_EQ(expected_allocation,
            fake_vea()->stored_bitrate_allocations().back());
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
      int layer_bitrate = i * 1000;
      const size_t si = (i - 1) / VideoBitrateAllocation::kMaxTemporalLayers;
      const size_t ti = (i - 1) % VideoBitrateAllocation::kMaxTemporalLayers;
      bitrate_allocation.SetBitrate(si, ti, layer_bitrate);
    }

    mojo_vea_service()->RequestEncodingParametersChange(bitrate_allocation,
                                                        kNewFramerate);
    base::RunLoop().RunUntilIdle();

    ASSERT_TRUE(fake_vea());
    EXPECT_EQ(bitrate_allocation,
              fake_vea()->stored_bitrate_allocations().back());
  }
}

// This test verifies that MojoVEA::Initialize() fails with an invalid |client|.
TEST_F(MojoVideoEncodeAcceleratorServiceTest,
       InitializeWithInvalidClientFails) {
  CreateMojoVideoEncodeAccelerator();

  mojo::PendingRemote<mojom::VideoEncodeAcceleratorClient>
      invalid_mojo_vea_client;

  const uint32_t kInitialBitrate = 100000u;
  const media::VideoEncodeAccelerator::Config config(
      PIXEL_FORMAT_I420, kInputVisibleSize, H264PROFILE_MIN, kInitialBitrate);
  mojo_vea_service()->Initialize(
      config, std::move(invalid_mojo_vea_client),
      base::Bind([](bool success) { ASSERT_FALSE(success); }));
  base::RunLoop().RunUntilIdle();
}

// This test verifies that when FakeVEA is configured to fail upon start,
// MojoVEA::Initialize() causes a NotifyError().
TEST_F(MojoVideoEncodeAcceleratorServiceTest, InitializeFailure) {
  CreateMojoVideoEncodeAccelerator(
      false /* will_fake_vea_initialization_succeed */);

  mojo::PendingRemote<mojom::VideoEncodeAcceleratorClient> mojo_vea_client;
  auto mojo_vea_receiver = mojo::MakeSelfOwnedReceiver(
      std::make_unique<MockMojoVideoEncodeAcceleratorClient>(),
      mojo_vea_client.InitWithNewPipeAndPassReceiver());

  const uint32_t kInitialBitrate = 100000u;
  const media::VideoEncodeAccelerator::Config config(
      PIXEL_FORMAT_I420, kInputVisibleSize, H264PROFILE_MIN, kInitialBitrate);
  mojo_vea_service()->Initialize(
      config, std::move(mojo_vea_client),
      base::Bind([](bool success) { ASSERT_FALSE(success); }));
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
  auto handle = mojo::SharedBufferHandle::Create(wrong_size);

  EXPECT_CALL(*mock_mojo_vea_client(),
              NotifyError(VideoEncodeAccelerator::kInvalidArgumentError));

  mojo_vea_service()->UseOutputBitstreamBuffer(kBitstreamBufferId,
                                               std::move(handle));
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

  EXPECT_CALL(*mock_mojo_vea_client(),
              NotifyError(VideoEncodeAccelerator::kInvalidArgumentError));

  mojo_vea_service()->Encode(video_frame, true /* is_keyframe */,
                             base::DoNothing());
  base::RunLoop().RunUntilIdle();
}

// This test verifies that an any mojom::VEA method call (e.g. Encode(),
// UseOutputBitstreamBuffer() etc) before MojoVEA::Initialize() is ignored (we
// can't expect NotifyError()s since there's no mojo client registered).
TEST_F(MojoVideoEncodeAcceleratorServiceTest, CallsBeforeInitializeAreIgnored) {
  CreateMojoVideoEncodeAccelerator();
  {
    const auto video_frame = VideoFrame::CreateBlackFrame(kInputVisibleSize);
    mojo_vea_service()->Encode(video_frame, true /* is_keyframe */,
                               base::DoNothing());
    base::RunLoop().RunUntilIdle();
  }
  {
    const int32_t kBitstreamBufferId = 17;
    const uint64_t kShMemSize = 10;
    auto handle = mojo::SharedBufferHandle::Create(kShMemSize);
    mojo_vea_service()->UseOutputBitstreamBuffer(kBitstreamBufferId,
                                                 std::move(handle));
    base::RunLoop().RunUntilIdle();
  }
  {
    const uint32_t kNewBitrate = 123123u;
    const uint32_t kNewFramerate = 321321u;
    media::VideoBitrateAllocation bitrate_allocation;
    bitrate_allocation.SetBitrate(0, 0, kNewBitrate);
    mojo_vea_service()->RequestEncodingParametersChange(bitrate_allocation,
                                                        kNewFramerate);
    base::RunLoop().RunUntilIdle();
  }
}

}  // namespace media
