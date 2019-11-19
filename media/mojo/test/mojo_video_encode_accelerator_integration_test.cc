// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <memory>

#include "base/bind.h"
#include "base/memory/unsafe_shared_memory_region.h"
#include "base/run_loop.h"
#include "base/test/gtest_util.h"
#include "base/test/task_environment.h"
#include "gpu/config/gpu_preferences.h"
#include "media/base/limits.h"
#include "media/mojo/clients/mojo_video_encode_accelerator.h"
#include "media/mojo/mojom/video_encode_accelerator.mojom.h"
#include "media/mojo/services/mojo_video_encode_accelerator_service.h"
#include "media/video/fake_video_encode_accelerator.h"
#include "media/video/video_encode_accelerator.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;

namespace media {

static const gfx::Size kInputVisibleSize(64, 48);
static const uint32_t kInitialBitrate = 100000u;
static const VideoCodecProfile kValidOutputProfile = H264PROFILE_MAIN;

extern std::unique_ptr<VideoEncodeAccelerator> CreateAndInitializeFakeVEA(
    const VideoEncodeAccelerator::Config& config,
    VideoEncodeAccelerator::Client* client,
    const gpu::GpuPreferences& gpu_preferences) {
  // Use FakeVEA as scoped_ptr to guarantee proper destruction via Destroy().
  auto vea = std::make_unique<FakeVideoEncodeAccelerator>(
      base::ThreadTaskRunnerHandle::Get());
  const bool result = vea->Initialize(config, client);

  // Mimic the behaviour of GpuVideoEncodeAcceleratorFactory::CreateVEA().
  return result ? base::WrapUnique<VideoEncodeAccelerator>(vea.release())
                : nullptr;
}

// Mock implementation of the client of MojoVideoEncodeAccelerator.
class MockVideoEncodeAcceleratorClient : public VideoEncodeAccelerator::Client {
 public:
  MockVideoEncodeAcceleratorClient() = default;

  MOCK_METHOD3(RequireBitstreamBuffers,
               void(unsigned int, const gfx::Size&, size_t));
  MOCK_METHOD2(BitstreamBufferReady,
               void(int32_t, const media::BitstreamBufferMetadata&));
  MOCK_METHOD1(NotifyError, void(VideoEncodeAccelerator::Error));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockVideoEncodeAcceleratorClient);
};

class MojoVideoEncodeAcceleratorIntegrationTest : public ::testing::Test {
 public:
  MojoVideoEncodeAcceleratorIntegrationTest() = default;

  void SetUp() override {
    mojo::PendingRemote<mojom::VideoEncodeAccelerator> mojo_vea;
    mojo_vea_receiver_ = mojo::MakeSelfOwnedReceiver(
        std::make_unique<MojoVideoEncodeAcceleratorService>(
            base::Bind(&CreateAndInitializeFakeVEA), gpu::GpuPreferences()),
        mojo_vea.InitWithNewPipeAndPassReceiver());

    mojo_vea_.reset(new MojoVideoEncodeAccelerator(
        std::move(mojo_vea), gpu::VideoEncodeAcceleratorSupportedProfiles()));
  }

  void TearDown() override {
    // The destruction of a mojo::SelfOwnedReceiver closes the bound message
    // pipe but does not destroy the implementation object(s): this needs to
    // happen manually by Close()ing it.
    if (mojo_vea_receiver_)
      mojo_vea_receiver_->Close();
  }

  VideoEncodeAccelerator* mojo_vea() { return mojo_vea_.get(); }

  FakeVideoEncodeAccelerator* fake_vea() const {
    const auto* mojo_vea_service =
        static_cast<MojoVideoEncodeAcceleratorService*>(
            mojo_vea_receiver_->impl());
    return static_cast<FakeVideoEncodeAccelerator*>(
        mojo_vea_service->encoder_.get());
  }

  // This method calls Initialize() with semantically correct parameters and
  // verifies that the appropriate message goes through the mojo pipe and is
  // responded by a RequireBitstreamBuffers() on |mock_vea_client|.
  void Initialize(MockVideoEncodeAcceleratorClient* mock_vea_client) {
    const uint64_t kShMemSize = fake_vea()->minimum_output_buffer_size();

    EXPECT_CALL(*mock_vea_client,
                RequireBitstreamBuffers(_, kInputVisibleSize, kShMemSize));

    const VideoEncodeAccelerator::Config config(
        PIXEL_FORMAT_I420, kInputVisibleSize, kValidOutputProfile,
        kInitialBitrate);
    EXPECT_TRUE(mojo_vea()->Initialize(config, mock_vea_client));
    base::RunLoop().RunUntilIdle();
  }

 private:
  base::test::SingleThreadTaskEnvironment task_environment_;

  // This member holds on to the implementation of the "service" side.
  mojo::SelfOwnedReceiverRef<mojom::VideoEncodeAccelerator> mojo_vea_receiver_;

  // The class under test, as a generic media::VideoEncodeAccelerator.
  std::unique_ptr<VideoEncodeAccelerator> mojo_vea_;

  DISALLOW_COPY_AND_ASSIGN(MojoVideoEncodeAcceleratorIntegrationTest);
};

TEST_F(MojoVideoEncodeAcceleratorIntegrationTest, CreateAndDestroy) {}

TEST_F(MojoVideoEncodeAcceleratorIntegrationTest, Initialize) {
  auto mock_vea_client = std::make_unique<MockVideoEncodeAcceleratorClient>();
  Initialize(mock_vea_client.get());

  // Make double sure that |kValidOutputProfile| is supported.
  ASSERT_GT(fake_vea()->GetSupportedProfiles().size(), 1u);
  EXPECT_EQ(kValidOutputProfile, fake_vea()->GetSupportedProfiles()[0].profile);
}

// This test verifies that Initialize() fails with an invalid |client|.
TEST_F(MojoVideoEncodeAcceleratorIntegrationTest,
       InitializeWithInvalidClientFails) {
  media::VideoEncodeAccelerator::Client* invalid_client = nullptr;

  const VideoEncodeAccelerator::Config config(
      PIXEL_FORMAT_I420, kInputVisibleSize, kValidOutputProfile,
      kInitialBitrate);
  EXPECT_FALSE(mojo_vea()->Initialize(config, invalid_client));
  base::RunLoop().RunUntilIdle();
}

// This test verifies that Initialize() fails when called with too large a
// visible size, and NotifyError() gets pinged.
TEST_F(MojoVideoEncodeAcceleratorIntegrationTest,
       InitializeWithInvalidDimensionsFails) {
  auto mock_vea_client = std::make_unique<MockVideoEncodeAcceleratorClient>();

  const gfx::Size kInvalidInputVisibleSize(limits::kMaxDimension + 1, 48);

  const VideoEncodeAccelerator::Config config(
      PIXEL_FORMAT_I420, kInvalidInputVisibleSize, kValidOutputProfile,
      kInitialBitrate);
  EXPECT_FALSE(mojo_vea()->Initialize(config, mock_vea_client.get()));
  base::RunLoop().RunUntilIdle();
}
// This test verifies that Initialize() fails when called with an invalid codec
// profile, and NotifyError() gets pinged.
// This test is tantamount to forcing the remote Fake VEA to fail upon init.
TEST_F(MojoVideoEncodeAcceleratorIntegrationTest,
       InitializeWithUnsupportedProfileFails) {
  auto mock_vea_client = std::make_unique<MockVideoEncodeAcceleratorClient>();

  const VideoCodecProfile kInvalidOutputProfile = VIDEO_CODEC_PROFILE_UNKNOWN;

  const VideoEncodeAccelerator::Config config(
      PIXEL_FORMAT_I420, kInputVisibleSize, kInvalidOutputProfile,
      kInitialBitrate);
  EXPECT_FALSE(mojo_vea()->Initialize(config, mock_vea_client.get()));
  base::RunLoop().RunUntilIdle();
}

// This test verifies that UseOutputBitstreamBuffer() with a different size than
// the requested in RequireBitstreamBuffers() fails.
TEST_F(MojoVideoEncodeAcceleratorIntegrationTest,
       UseOutputBitstreamBufferWithInvalidSizeFails) {
  auto mock_vea_client = std::make_unique<MockVideoEncodeAcceleratorClient>();
  Initialize(mock_vea_client.get());

  const uint64_t kInvalidShMemSize =
      fake_vea()->minimum_output_buffer_size() / 2;

  base::UnsafeSharedMemoryRegion region =
      base::UnsafeSharedMemoryRegion::Create(kInvalidShMemSize);

  EXPECT_CALL(*mock_vea_client,
              NotifyError(VideoEncodeAccelerator::kInvalidArgumentError));

  mojo_vea()->UseOutputBitstreamBuffer(
      BitstreamBuffer(17 /* id */, std::move(region), kInvalidShMemSize,
                      0 /* offset */, base::TimeDelta()));
  base::RunLoop().RunUntilIdle();
}

// This test verifies that UseOutputBitstreamBuffer() with an invalid (negative)
// buffer id fails.
TEST_F(MojoVideoEncodeAcceleratorIntegrationTest,
       UseOutputBitstreamBufferWithInvalidIdFails) {
  auto mock_vea_client = std::make_unique<MockVideoEncodeAcceleratorClient>();
  Initialize(mock_vea_client.get());

  const int32_t kInvalidBistreamBufferId = -18;

  const uint64_t kShMemSize = fake_vea()->minimum_output_buffer_size();
  base::UnsafeSharedMemoryRegion region =
      base::UnsafeSharedMemoryRegion::Create(kShMemSize);
  EXPECT_CALL(*mock_vea_client,
              NotifyError(VideoEncodeAccelerator::kInvalidArgumentError));

  mojo_vea()->UseOutputBitstreamBuffer(
      BitstreamBuffer(kInvalidBistreamBufferId, std::move(region), kShMemSize,
                      0 /* offset */, base::TimeDelta()));
  base::RunLoop().RunUntilIdle();
}

// This test verifies the sharing of a single bitstream buffer and the Encode()
// of one frame.
TEST_F(MojoVideoEncodeAcceleratorIntegrationTest, EncodeOneFrame) {
  auto mock_vea_client = std::make_unique<MockVideoEncodeAcceleratorClient>();
  Initialize(mock_vea_client.get());

  const int32_t kBistreamBufferId = 17;
  {
    const uint64_t kShMemSize = fake_vea()->minimum_output_buffer_size();
    base::UnsafeSharedMemoryRegion region =
        base::UnsafeSharedMemoryRegion::Create(kShMemSize);
    mojo_vea()->UseOutputBitstreamBuffer(
        BitstreamBuffer(kBistreamBufferId, std::move(region), kShMemSize,
                        0 /* offset */, base::TimeDelta()));
    base::RunLoop().RunUntilIdle();
  }

  {
    base::UnsafeSharedMemoryRegion shmem =
        base::UnsafeSharedMemoryRegion::Create(
            VideoFrame::AllocationSize(PIXEL_FORMAT_I420, kInputVisibleSize) *
            2);
    ASSERT_TRUE(shmem.IsValid());
    base::WritableSharedMemoryMapping mapping = shmem.Map();
    ASSERT_TRUE(mapping.IsValid());
    const scoped_refptr<VideoFrame> video_frame = VideoFrame::WrapExternalData(
        PIXEL_FORMAT_I420, kInputVisibleSize, gfx::Rect(kInputVisibleSize),
        kInputVisibleSize, mapping.GetMemoryAsSpan<uint8_t>().data(),
        mapping.size(), base::TimeDelta());
    video_frame->BackWithSharedMemory(&shmem);
    const bool is_keyframe = true;

    EXPECT_CALL(*mock_vea_client, BitstreamBufferReady(kBistreamBufferId, _))
        .WillOnce(testing::Invoke(
            [is_keyframe](int32_t, const BitstreamBufferMetadata& metadata) {
              EXPECT_EQ(is_keyframe, metadata.key_frame);
            }));

    mojo_vea()->Encode(video_frame, is_keyframe);
    base::RunLoop().RunUntilIdle();
  }
}

// This test verifies that trying to Encode() a VideoFrame with dimensions
// different than those configured in Initialize() fails.
TEST_F(MojoVideoEncodeAcceleratorIntegrationTest,
       EncodeWithInvalidDimensionsFails) {
  auto mock_vea_client = std::make_unique<MockVideoEncodeAcceleratorClient>();
  Initialize(mock_vea_client.get());

  {
    const uint64_t kShMemSize = fake_vea()->minimum_output_buffer_size();
    base::UnsafeSharedMemoryRegion region =
        base::UnsafeSharedMemoryRegion::Create(kShMemSize);
    mojo_vea()->UseOutputBitstreamBuffer(
        BitstreamBuffer(17 /* id */, std::move(region), kShMemSize,
                        0 /* offset */, base::TimeDelta()));
    base::RunLoop().RunUntilIdle();
  }

  {
    const gfx::Size kInvalidInputVisibleSize(kInputVisibleSize.width() * 2,
                                             kInputVisibleSize.height());

    base::UnsafeSharedMemoryRegion shmem =
        base::UnsafeSharedMemoryRegion::Create(
            VideoFrame::AllocationSize(PIXEL_FORMAT_I420,
                                       kInvalidInputVisibleSize) *
            2);
    ASSERT_TRUE(shmem.IsValid());
    base::WritableSharedMemoryMapping mapping = shmem.Map();
    ASSERT_TRUE(mapping.IsValid());
    const scoped_refptr<VideoFrame> video_frame = VideoFrame::WrapExternalData(
        PIXEL_FORMAT_I420, kInvalidInputVisibleSize,
        gfx::Rect(kInvalidInputVisibleSize), kInvalidInputVisibleSize,
        mapping.GetMemoryAsSpan<uint8_t>().data(), mapping.size(),
        base::TimeDelta());
    video_frame->BackWithSharedMemory(&shmem);
    const bool is_keyframe = true;

    EXPECT_CALL(*mock_vea_client,
                NotifyError(VideoEncodeAccelerator::kInvalidArgumentError));

    mojo_vea()->Encode(video_frame, is_keyframe);
    base::RunLoop().RunUntilIdle();
  }
}

// Tests that a RequestEncodingParametersChange() ripples through correctly.
TEST_F(MojoVideoEncodeAcceleratorIntegrationTest, EncodingParametersChange) {
  auto mock_vea_client = std::make_unique<MockVideoEncodeAcceleratorClient>();
  Initialize(mock_vea_client.get());

  const uint32_t kNewBitrate = 123123u;
  const uint32_t kNewFramerate = 321321u;

  mojo_vea()->RequestEncodingParametersChange(kNewBitrate, kNewFramerate);
  base::RunLoop().RunUntilIdle();
  VideoBitrateAllocation expected_bitrate_allocation;
  expected_bitrate_allocation.SetBitrate(0, 0, kNewBitrate);
  EXPECT_EQ(expected_bitrate_allocation,
            fake_vea()->stored_bitrate_allocations().back());
}

// Tests that a RequestEncodingParametersChange() ripples through correctly.
TEST_F(MojoVideoEncodeAcceleratorIntegrationTest,
       EncodingParametersWithBitrateAllocation) {
  auto mock_vea_client = std::make_unique<MockVideoEncodeAcceleratorClient>();
  Initialize(mock_vea_client.get());

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

    mojo_vea()->RequestEncodingParametersChange(bitrate_allocation,
                                                kNewFramerate);
    base::RunLoop().RunUntilIdle();
    EXPECT_EQ(bitrate_allocation,
              fake_vea()->stored_bitrate_allocations().back());
  }
}

// Tests that calls are sent nowhere when the connection has been Close()d --
// this simulates the remote end of the communication going down.
TEST_F(MojoVideoEncodeAcceleratorIntegrationTest,
       CallsAreIgnoredAfterBindingClosed) {
  auto mock_vea_client = std::make_unique<MockVideoEncodeAcceleratorClient>();
  Initialize(mock_vea_client.get());

  {
    TearDown();  // Alias for |mojo_vea_receiver_| Close().
    base::RunLoop().RunUntilIdle();
  }
  {
    // Any call to MojoVideoEncodeAccelerator here will do nothing because the
    // remote end has been torn down and needs to be re Initialize()d.
    mojo_vea()->RequestEncodingParametersChange(1234u /* bitrate */,
                                                3321 /* framerate */);
    base::RunLoop().RunUntilIdle();
  }
}

}  // namespace media
