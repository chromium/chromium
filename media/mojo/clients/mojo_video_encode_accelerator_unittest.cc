// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "gpu/config/gpu_info.h"
#include "media/mojo/clients/mojo_video_encode_accelerator.h"
#include "media/mojo/mojom/video_encode_accelerator.mojom.h"
#include "media/video/video_encode_accelerator.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::InSequence;
using ::testing::Invoke;

namespace media {

static const gfx::Size kInputVisibleSize(64, 48);

// Mock implementation of the Mojo "service" side of the VEA dialogue. Upon an
// Initialize() call, checks |initialization_success_| and responds to |client|
// with a RequireBitstreamBuffers() if so configured; upon Encode(), responds
// with a BitstreamBufferReady() with the bitstream buffer id previously
// configured by a call to UseOutputBitstreamBuffer(). This mock class only
// allows for one bitstream buffer in flight.
class MockMojoVideoEncodeAccelerator : public mojom::VideoEncodeAccelerator {
 public:
  MockMojoVideoEncodeAccelerator() = default;

  // mojom::VideoEncodeAccelerator impl.
  void Initialize(
      const media::VideoEncodeAccelerator::Config& config,
      mojo::PendingRemote<mojom::VideoEncodeAcceleratorClient> client,
      InitializeCallback success_callback) override {
    if (initialization_success_) {
      ASSERT_TRUE(client);
      client_.Bind(std::move(client));
      const size_t allocation_size = VideoFrame::AllocationSize(
          config.input_format, config.input_visible_size);

      client_->RequireBitstreamBuffers(1, config.input_visible_size,
                                       allocation_size);

      DoInitialize(config.input_format, config.input_visible_size,
                   config.output_profile, config.initial_bitrate,
                   config.content_type, &client_);
    }
    std::move(success_callback).Run(initialization_success_);
  }
  MOCK_METHOD6(DoInitialize,
               void(media::VideoPixelFormat,
                    const gfx::Size&,
                    media::VideoCodecProfile,
                    uint32_t,
                    media::VideoEncodeAccelerator::Config::ContentType,
                    mojo::Remote<mojom::VideoEncodeAcceleratorClient>*));

  void Encode(const scoped_refptr<VideoFrame>& frame,
              bool keyframe,
              EncodeCallback callback) override {
    EXPECT_NE(-1, configured_bitstream_buffer_id_);
    EXPECT_TRUE(client_);
    client_->BitstreamBufferReady(
        configured_bitstream_buffer_id_,
        BitstreamBufferMetadata(0, keyframe, frame->timestamp()));
    configured_bitstream_buffer_id_ = -1;

    DoEncode(frame, keyframe);
    std::move(callback).Run();
  }
  MOCK_METHOD2(DoEncode, void(const scoped_refptr<VideoFrame>&, bool));

  void UseOutputBitstreamBuffer(
      int32_t bitstream_buffer_id,
      mojo::ScopedSharedBufferHandle buffer) override {
    EXPECT_EQ(-1, configured_bitstream_buffer_id_);
    configured_bitstream_buffer_id_ = bitstream_buffer_id;

    DoUseOutputBitstreamBuffer(bitstream_buffer_id, &buffer);
  }
  MOCK_METHOD2(DoUseOutputBitstreamBuffer,
               void(int32_t, mojo::ScopedSharedBufferHandle*));

  MOCK_METHOD2(RequestEncodingParametersChange,
               void(const media::VideoBitrateAllocation&, uint32_t));

  void set_initialization_success(bool success) {
    initialization_success_ = success;
  }

 private:
  mojo::Remote<mojom::VideoEncodeAcceleratorClient> client_;
  int32_t configured_bitstream_buffer_id_ = -1;
  bool initialization_success_ = true;

  DISALLOW_COPY_AND_ASSIGN(MockMojoVideoEncodeAccelerator);
};

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

// Test wrapper for a MojoVideoEncodeAccelerator, which translates between a
// pipe to a remote mojom::MojoVideoEncodeAccelerator, and a local
// media::VideoEncodeAccelerator::Client.
class MojoVideoEncodeAcceleratorTest : public ::testing::Test {
 public:
  MojoVideoEncodeAcceleratorTest() = default;

  void SetUp() override {
    mojo::PendingRemote<mojom::VideoEncodeAccelerator> mojo_vea;
    mojo_vea_receiver_ = mojo::MakeSelfOwnedReceiver(
        std::make_unique<MockMojoVideoEncodeAccelerator>(),
        mojo_vea.InitWithNewPipeAndPassReceiver());

    mojo_vea_.reset(new MojoVideoEncodeAccelerator(
        std::move(mojo_vea), gpu::VideoEncodeAcceleratorSupportedProfiles()));
  }

  void TearDown() override {
    // The destruction of a mojo::SelfOwnedReceiver closes the bound message
    // pipe but does not destroy the implementation object(s): this needs to
    // happen manually by Close()ing it.
    mojo_vea_receiver_->Close();
  }

  MockMojoVideoEncodeAccelerator* mock_mojo_vea() {
    return static_cast<media::MockMojoVideoEncodeAccelerator*>(
        mojo_vea_receiver_->impl());
  }
  VideoEncodeAccelerator* mojo_vea() { return mojo_vea_.get(); }

  // This method calls Initialize() with semantically correct parameters and
  // verifies that the appropriate message goes through the mojo pipe and is
  // responded by a RequireBitstreamBuffers() on |mock_vea_client|.
  void Initialize(MockVideoEncodeAcceleratorClient* mock_vea_client) {
    const VideoCodecProfile kOutputProfile = VIDEO_CODEC_PROFILE_UNKNOWN;
    const uint32_t kInitialBitrate = 100000u;
    const VideoEncodeAccelerator::Config::ContentType kContentType =
        VideoEncodeAccelerator::Config::ContentType::kDisplay;

    EXPECT_CALL(*mock_mojo_vea(),
                DoInitialize(PIXEL_FORMAT_I420, kInputVisibleSize,
                             kOutputProfile, kInitialBitrate, kContentType, _));
    EXPECT_CALL(
        *mock_vea_client,
        RequireBitstreamBuffers(
            _, kInputVisibleSize,
            VideoFrame::AllocationSize(PIXEL_FORMAT_I420, kInputVisibleSize)));

    const VideoEncodeAccelerator::Config config(
        PIXEL_FORMAT_I420, kInputVisibleSize, kOutputProfile, kInitialBitrate,
        base::nullopt, base::nullopt, base::nullopt, base::nullopt,
        kContentType);
    EXPECT_TRUE(mojo_vea()->Initialize(config, mock_vea_client));
    base::RunLoop().RunUntilIdle();
  }

 private:
  base::test::SingleThreadTaskEnvironment task_environment_;

  // This member holds on to the mock implementation of the "service" side.
  mojo::SelfOwnedReceiverRef<mojom::VideoEncodeAccelerator> mojo_vea_receiver_;

  // The class under test, as a generic media::VideoEncodeAccelerator.
  std::unique_ptr<VideoEncodeAccelerator> mojo_vea_;

  DISALLOW_COPY_AND_ASSIGN(MojoVideoEncodeAcceleratorTest);
};

TEST_F(MojoVideoEncodeAcceleratorTest, CreateAndDestroy) {}

// This test verifies the Initialize() communication prologue in isolation.
TEST_F(MojoVideoEncodeAcceleratorTest, InitializeAndRequireBistreamBuffers) {
  std::unique_ptr<MockVideoEncodeAcceleratorClient> mock_vea_client =
      std::make_unique<MockVideoEncodeAcceleratorClient>();
  Initialize(mock_vea_client.get());
}

// This test verifies the Initialize() communication prologue followed by a
// sharing of a single bitstream buffer and the Encode() of one frame.
TEST_F(MojoVideoEncodeAcceleratorTest, EncodeOneFrame) {
  std::unique_ptr<MockVideoEncodeAcceleratorClient> mock_vea_client =
      std::make_unique<MockVideoEncodeAcceleratorClient>();
  Initialize(mock_vea_client.get());

  const int32_t kBitstreamBufferId = 17;
  {
    const int32_t kShMemSize = 10;
    auto shmem = base::UnsafeSharedMemoryRegion::Create(kShMemSize);
    EXPECT_CALL(*mock_mojo_vea(),
                DoUseOutputBitstreamBuffer(kBitstreamBufferId, _));
    mojo_vea()->UseOutputBitstreamBuffer(BitstreamBuffer(
        kBitstreamBufferId,
        base::UnsafeSharedMemoryRegion::TakeHandleForSerialization(
            std::move(shmem)),
        kShMemSize, 0 /* offset */, base::TimeDelta()));
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

    // The remote end of the mojo Pipe doesn't receive |video_frame| itself.
    EXPECT_CALL(*mock_mojo_vea(), DoEncode(_, is_keyframe));
    EXPECT_CALL(*mock_vea_client, BitstreamBufferReady(kBitstreamBufferId, _))
        .WillOnce(Invoke([is_keyframe, &video_frame](
                             int32_t, const BitstreamBufferMetadata& metadata) {
          EXPECT_EQ(is_keyframe, metadata.key_frame);
          EXPECT_EQ(metadata.timestamp, video_frame->timestamp());
        }));

    mojo_vea()->Encode(video_frame, is_keyframe);
    base::RunLoop().RunUntilIdle();
  }
}

// Tests that a RequestEncodingParametersChange() ripples through correctly.
TEST_F(MojoVideoEncodeAcceleratorTest, EncodingParametersChange) {
  const uint32_t kNewFramerate = 321321u;
  const uint32_t kNewBitrate = 123123u;
  VideoBitrateAllocation bitrate_allocation;
  bitrate_allocation.SetBitrate(0, 0, kNewBitrate);

  // In a real world scenario, we should go through an Initialize() prologue,
  // but we can skip that in unit testing.

  EXPECT_CALL(*mock_mojo_vea(), RequestEncodingParametersChange(
                                    bitrate_allocation, kNewFramerate));
  mojo_vea()->RequestEncodingParametersChange(kNewBitrate, kNewFramerate);
  base::RunLoop().RunUntilIdle();
}

// Tests that a RequestEncodingParametersChange() works with multi-dimensional
// bitrate allocatio.
TEST_F(MojoVideoEncodeAcceleratorTest,
       EncodingParametersWithBitrateAllocation) {
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

    EXPECT_CALL(*mock_mojo_vea(), RequestEncodingParametersChange(
                                      bitrate_allocation, kNewFramerate));
    mojo_vea()->RequestEncodingParametersChange(bitrate_allocation,
                                                kNewFramerate);
    base::RunLoop().RunUntilIdle();
  }
}

// This test verifies the Initialize() communication prologue fails when the
// FakeVEA is configured to do so.
TEST_F(MojoVideoEncodeAcceleratorTest, InitializeFailure) {
  std::unique_ptr<MockVideoEncodeAcceleratorClient> mock_vea_client =
      std::make_unique<MockVideoEncodeAcceleratorClient>();

  const uint32_t kInitialBitrate = 100000u;

  mock_mojo_vea()->set_initialization_success(false);

  const VideoEncodeAccelerator::Config config(
      PIXEL_FORMAT_I420, kInputVisibleSize, VIDEO_CODEC_PROFILE_UNKNOWN,
      kInitialBitrate);
  EXPECT_FALSE(mojo_vea()->Initialize(config, mock_vea_client.get()));
  base::RunLoop().RunUntilIdle();
}

}  // namespace media
