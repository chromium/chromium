// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include <memory>

#include "base/memory/ptr_util.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "base/memory/unsafe_shared_memory_region.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "gpu/config/gpu_info.h"
#include "media/base/bitstream_buffer.h"
#include "media/base/media_util.h"
#include "media/mojo/clients/mojo_video_encode_accelerator.h"
#include "media/mojo/mojom/video_encode_accelerator.mojom.h"
#include "media/video/video_encode_accelerator.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::InSequence;
using ::testing::Invoke;

namespace media {

MATCHER_P(ExpectEncoderStatusCode, expected_code, "encoder status code") {
  return arg.code() == expected_code;
}

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

  MockMojoVideoEncodeAccelerator(const MockMojoVideoEncodeAccelerator&) =
      delete;
  MockMojoVideoEncodeAccelerator& operator=(
      const MockMojoVideoEncodeAccelerator&) = delete;

  // mojom::VideoEncodeAccelerator impl.
  void Initialize(
      const media::VideoEncodeAccelerator::Config& config,
      mojo::PendingAssociatedRemote<mojom::VideoEncodeAcceleratorClient> client,
      mojo::PendingRemote<mojom::MediaLog> media_log,
      InitializeCallback success_callback) override {
    if (initialization_success_) {
      ASSERT_TRUE(client);
      client_.Bind(std::move(client));
      const size_t allocation_size = VideoFrame::AllocationSize(
          config.input_format, config.input_visible_size);

      client_->RequireBitstreamBuffers(1, config.input_visible_size,
                                       allocation_size);

      DoInitialize(config.input_format, config.input_visible_size,
                   config.output_profile, config.bitrate, config.content_type,
                   &client_);
    }
    std::move(success_callback).Run(initialization_success_);
  }
  MOCK_METHOD6(
      DoInitialize,
      void(media::VideoPixelFormat,
           const gfx::Size&,
           media::VideoCodecProfile,
           media::Bitrate,
           media::VideoEncodeAccelerator::Config::ContentType,
           mojo::AssociatedRemote<mojom::VideoEncodeAcceleratorClient>*));

  void Encode(const scoped_refptr<VideoFrame>& frame,
              const VideoEncoder::EncodeOptions& options,
              EncodeCallback callback) override {
    EXPECT_NE(-1, configured_bitstream_buffer_id_);
    EXPECT_TRUE(client_);
    client_->BitstreamBufferReady(
        configured_bitstream_buffer_id_,
        BitstreamBufferMetadata(100, options.key_frame, frame->timestamp()));
    configured_bitstream_buffer_id_ = -1;

    DoEncode(frame, options.key_frame);
    std::move(callback).Run();
  }
  MOCK_METHOD2(DoEncode, void(const scoped_refptr<VideoFrame>&, bool));

  void UseOutputBitstreamBuffer(
      int32_t bitstream_buffer_id,
      base::UnsafeSharedMemoryRegion region) override {
    EXPECT_EQ(-1, configured_bitstream_buffer_id_);
    configured_bitstream_buffer_id_ = bitstream_buffer_id;

    DoUseOutputBitstreamBuffer(bitstream_buffer_id, &region);
  }
  MOCK_METHOD2(DoUseOutputBitstreamBuffer,
               void(int32_t, base::UnsafeSharedMemoryRegion*));

  void RequestEncodingParametersChangeWithLayers(
      const media::VideoBitrateAllocation& bitrate,
      uint32_t framerate,
      const std::optional<gfx::Size>& size) override {
    DoRequestEncodingParametersChangeWithLayers(bitrate, framerate, size);
  }
  MOCK_METHOD3(DoRequestEncodingParametersChangeWithLayers,
               void(const media::VideoBitrateAllocation&,
                    uint32_t,
                    const std::optional<gfx::Size>&));
  void RequestEncodingParametersChangeWithBitrate(
      const media::Bitrate& bitrate,
      uint32_t framerate,
      const std::optional<gfx::Size>& size) override {
    DoRequestEncodingParametersChangeWithBitrate(bitrate, framerate, size);
  }
  MOCK_METHOD3(DoRequestEncodingParametersChangeWithBitrate,
               void(const media::Bitrate&,
                    uint32_t,
                    const std::optional<gfx::Size>&));

  void IsFlushSupported(IsFlushSupportedCallback callback) override {
    DoIsFlushSupported();
    std::move(callback).Run(true);
  }
  MOCK_METHOD0(DoIsFlushSupported, void());
  void Flush(FlushCallback callback) override {
    FlushCallback mock_callback;
    DoFlush(std::move(mock_callback));
    // Actually, this callback should run on DoFlush, but in test, manally run
    // it on Flush.
    std::move(callback).Run(true);
  }
  MOCK_METHOD1(DoFlush, void(FlushCallback));

  void set_initialization_success(bool success) {
    initialization_success_ = success;
  }

 private:
  mojo::AssociatedRemote<mojom::VideoEncodeAcceleratorClient> client_;
  int32_t configured_bitstream_buffer_id_ = -1;
  bool initialization_success_ = true;
};

// Mock implementation of the client of MojoVideoEncodeAccelerator.
class MockVideoEncodeAcceleratorClient : public VideoEncodeAccelerator::Client {
 public:
  MockVideoEncodeAcceleratorClient() = default;

  MockVideoEncodeAcceleratorClient(const MockVideoEncodeAcceleratorClient&) =
      delete;
  MockVideoEncodeAcceleratorClient& operator=(
      const MockVideoEncodeAcceleratorClient&) = delete;

  MOCK_METHOD3(RequireBitstreamBuffers,
               void(unsigned int, const gfx::Size&, size_t));
  MOCK_METHOD2(BitstreamBufferReady,
               void(int32_t, const media::BitstreamBufferMetadata&));
  MOCK_METHOD1(NotifyErrorStatus, void(const media::EncoderStatus&));
  MOCK_METHOD1(NotifyEncoderInfoChange, void(const media::VideoEncoderInfo&));
};

// Test wrapper for a MojoVideoEncodeAccelerator, which translates between a
// pipe to a remote mojom::MojoVideoEncodeAccelerator, and a local
// media::VideoEncodeAccelerator::Client.
class MojoVideoEncodeAcceleratorTest : public ::testing::Test {
 public:
  MojoVideoEncodeAcceleratorTest() = default;

  MojoVideoEncodeAcceleratorTest(const MojoVideoEncodeAcceleratorTest&) =
      delete;
  MojoVideoEncodeAcceleratorTest& operator=(
      const MojoVideoEncodeAcceleratorTest&) = delete;

  void SetUp() override {
    mojo::PendingRemote<mojom::VideoEncodeAccelerator> mojo_vea;
    mojo_vea_receiver_ = mojo::MakeSelfOwnedReceiver(
        std::make_unique<MockMojoVideoEncodeAccelerator>(),
        mojo_vea.InitWithNewPipeAndPassReceiver());

    mojo_vea_ = base::WrapUnique<VideoEncodeAccelerator>(
        new MojoVideoEncodeAccelerator(std::move(mojo_vea)));
  }

  void TearDown() override {
    // The destruction of a mojo::SelfOwnedReceiver closes the bound message
    // pipe but does not destroy the implementation object(s): this needs to
    // happen manually by Close()ing it.
    if (mojo_vea_receiver_)
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
    constexpr VideoCodecProfile kOutputProfile = VIDEO_CODEC_PROFILE_UNKNOWN;
    constexpr Bitrate kInitialBitrate = Bitrate::ConstantBitrate(100000u);
    constexpr uint32_t kFramerate = 30;
    constexpr VideoEncodeAccelerator::Config::StorageType kStorageType =
        VideoEncodeAccelerator::Config::StorageType::kShmem;
    constexpr VideoEncodeAccelerator::Config::ContentType kContentType =
        VideoEncodeAccelerator::Config::ContentType::kDisplay;

    EXPECT_CALL(*mock_mojo_vea(),
                DoInitialize(PIXEL_FORMAT_I420, kInputVisibleSize,
                             kOutputProfile, kInitialBitrate, kContentType, _));
    EXPECT_CALL(
        *mock_vea_client,
        RequireBitstreamBuffers(
            _, kInputVisibleSize,
            VideoFrame::AllocationSize(PIXEL_FORMAT_I420, kInputVisibleSize)));

    VideoEncodeAccelerator::Config config(
        PIXEL_FORMAT_I420, kInputVisibleSize, kOutputProfile, kInitialBitrate,
        kFramerate, kStorageType, kContentType);

    EXPECT_TRUE(mojo_vea()->Initialize(
        config, mock_vea_client, std::make_unique<media::NullMediaLog>()));
    base::RunLoop().RunUntilIdle();
  }

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_;

  // This member holds on to the mock implementation of the "service" side.
  mojo::SelfOwnedReceiverRef<mojom::VideoEncodeAccelerator> mojo_vea_receiver_;

  // The class under test, as a generic media::VideoEncodeAccelerator.
  std::unique_ptr<VideoEncodeAccelerator> mojo_vea_;
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
    mojo_vea()->UseOutputBitstreamBuffer(
        BitstreamBuffer(kBitstreamBufferId, std::move(shmem), kShMemSize,
                        0 /* offset */, base::TimeDelta()));
    base::RunLoop().RunUntilIdle();
  }

  {
    base::MappedReadOnlyRegion shmem = base::ReadOnlySharedMemoryRegion::Create(
        VideoFrame::AllocationSize(PIXEL_FORMAT_I420, kInputVisibleSize) * 2);
    ASSERT_TRUE(shmem.IsValid());
    const scoped_refptr<VideoFrame> video_frame = VideoFrame::WrapExternalData(
        PIXEL_FORMAT_I420, kInputVisibleSize, gfx::Rect(kInputVisibleSize),
        kInputVisibleSize, static_cast<uint8_t*>(shmem.mapping.memory()),
        shmem.mapping.size(), base::TimeDelta());
    video_frame->BackWithSharedMemory(&shmem.region);
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
  Bitrate bitrate = Bitrate::ConstantBitrate(kNewBitrate);

  // In a real world scenario, we should go through an Initialize() prologue,
  // but we can skip that in unit testing.

  EXPECT_CALL(*mock_mojo_vea(),
              DoRequestEncodingParametersChangeWithBitrate(
                  bitrate, kNewFramerate, testing::Eq(std::nullopt)));
  mojo_vea()->RequestEncodingParametersChange(bitrate, kNewFramerate,
                                              std::nullopt);
  base::RunLoop().RunUntilIdle();
}

// Tests that a RequestEncodingParametersChange() works with multi-dimensional
// bitrate allocation.
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
      uint32_t layer_bitrate = i * 1000;
      const size_t si = (i - 1) / VideoBitrateAllocation::kMaxTemporalLayers;
      const size_t ti = (i - 1) % VideoBitrateAllocation::kMaxTemporalLayers;
      bitrate_allocation.SetBitrate(si, ti, layer_bitrate);
    }

    EXPECT_CALL(*mock_mojo_vea(), DoRequestEncodingParametersChangeWithLayers(
                                      bitrate_allocation, kNewFramerate,
                                      testing::Eq(std::nullopt)));
    mojo_vea()->RequestEncodingParametersChange(bitrate_allocation,
                                                kNewFramerate, std::nullopt);
    base::RunLoop().RunUntilIdle();
  }
}

// This test verifies RequestEncodingParametersChange() communication with
// updated frame size.
TEST_F(MojoVideoEncodeAcceleratorTest, EncodingParametersChangeWithFrameSize) {
  std::unique_ptr<MockVideoEncodeAcceleratorClient> mock_vea_client =
      std::make_unique<MockVideoEncodeAcceleratorClient>();
  Initialize(mock_vea_client.get());

  base::RunLoop().RunUntilIdle();
  const uint32_t kNewFramerate = 321321u;
  const uint32_t kNewBitrate = 123123u;
  const gfx::Size kNewSize = gfx::Size(1280, 720);
  Bitrate bitrate = Bitrate::ConstantBitrate(kNewBitrate);
  EXPECT_CALL(*mock_mojo_vea(),
              DoRequestEncodingParametersChangeWithBitrate(
                  bitrate, kNewFramerate, testing::Optional(kNewSize)));
  mojo_vea()->RequestEncodingParametersChange(bitrate, kNewFramerate, kNewSize);
  base::RunLoop().RunUntilIdle();
}

// Tests that a RequestEncodingParametersChange() works with multi-dimensional
// bitrate allocation and updated frame size.
TEST_F(MojoVideoEncodeAcceleratorTest,
       EncodingParametersChangeWithBitrateAllocationAndFrameSize) {
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

    EXPECT_CALL(*mock_mojo_vea(), DoRequestEncodingParametersChangeWithLayers(
                                      bitrate_allocation, kNewFramerate,
                                      testing::Optional(frame_size)));
    mojo_vea()->RequestEncodingParametersChange(bitrate_allocation,
                                                kNewFramerate, frame_size);
    base::RunLoop().RunUntilIdle();
  }
}

// This test verifies the Initialize() communication prologue fails when the
// FakeVEA is configured to do so.
TEST_F(MojoVideoEncodeAcceleratorTest, InitializeFailure) {
  std::unique_ptr<MockVideoEncodeAcceleratorClient> mock_vea_client =
      std::make_unique<MockVideoEncodeAcceleratorClient>();

  constexpr Bitrate kInitialBitrate = Bitrate::ConstantBitrate(100000u);
  constexpr uint32_t kFramerate = 30;
  constexpr VideoEncodeAccelerator::Config::StorageType kStorageType =
      VideoEncodeAccelerator::Config::StorageType::kShmem;
  constexpr VideoEncodeAccelerator::Config::ContentType kContentType =
      VideoEncodeAccelerator::Config::ContentType::kDisplay;

  mock_mojo_vea()->set_initialization_success(false);

  const VideoEncodeAccelerator::Config config(
      PIXEL_FORMAT_I420, kInputVisibleSize, VIDEO_CODEC_PROFILE_UNKNOWN,
      kInitialBitrate, kFramerate, kStorageType, kContentType);
  EXPECT_FALSE(mojo_vea()->Initialize(config, mock_vea_client.get(),
                                      std::make_unique<media::NullMediaLog>()));
  base::RunLoop().RunUntilIdle();
}

// Test that mojo disconnect before initialize is surfaced as a platform error.
TEST_F(MojoVideoEncodeAcceleratorTest, MojoDisconnectBeforeInitialize) {
  std::unique_ptr<MockVideoEncodeAcceleratorClient> mock_vea_client =
      std::make_unique<MockVideoEncodeAcceleratorClient>();

  constexpr Bitrate kInitialBitrate = Bitrate::ConstantBitrate(100000u);
  constexpr uint32_t kFramerate = 30;
  constexpr VideoEncodeAccelerator::Config::StorageType kStorageType =
      VideoEncodeAccelerator::Config::StorageType::kShmem;
  constexpr VideoEncodeAccelerator::Config::ContentType kContentType =
      VideoEncodeAccelerator::Config::ContentType::kDisplay;
  const VideoEncodeAccelerator::Config config(
      PIXEL_FORMAT_I420, kInputVisibleSize, VIDEO_CODEC_PROFILE_UNKNOWN,
      kInitialBitrate, kFramerate, kStorageType, kContentType);
  mojo_vea_receiver_->Close();
  EXPECT_FALSE(mojo_vea()->Initialize(config, mock_vea_client.get(),
                                      std::make_unique<media::NullMediaLog>()));
  base::RunLoop().RunUntilIdle();
}

// Test that mojo disconnect after initialize is surfaced as a platform error.
TEST_F(MojoVideoEncodeAcceleratorTest, MojoDisconnectAfterInitialize) {
  std::unique_ptr<MockVideoEncodeAcceleratorClient> mock_vea_client =
      std::make_unique<MockVideoEncodeAcceleratorClient>();

  constexpr Bitrate kInitialBitrate = Bitrate::ConstantBitrate(100000u);
  constexpr uint32_t kFramerate = 30;
  constexpr VideoEncodeAccelerator::Config::StorageType kStorageType =
      VideoEncodeAccelerator::Config::StorageType::kShmem;
  constexpr VideoEncodeAccelerator::Config::ContentType kContentType =
      VideoEncodeAccelerator::Config::ContentType::kDisplay;
  const VideoEncodeAccelerator::Config config(
      PIXEL_FORMAT_I420, kInputVisibleSize, VIDEO_CODEC_PROFILE_UNKNOWN,
      kInitialBitrate, kFramerate, kStorageType, kContentType);
  EXPECT_TRUE(mojo_vea()->Initialize(config, mock_vea_client.get(),
                                     std::make_unique<media::NullMediaLog>()));
  mojo_vea_receiver_->Close();
  EXPECT_CALL(*mock_vea_client,
              NotifyErrorStatus(ExpectEncoderStatusCode(
                  EncoderStatus::Codes::kEncoderMojoConnectionError)));
  base::RunLoop().RunUntilIdle();
}

// This test verifies the IsFlushSupported() and Flush() communication.
TEST_F(MojoVideoEncodeAcceleratorTest, IsFlushSupportedAndFlush) {
  std::unique_ptr<MockVideoEncodeAcceleratorClient> mock_vea_client =
      std::make_unique<MockVideoEncodeAcceleratorClient>();
  Initialize(mock_vea_client.get());

  EXPECT_CALL(*mock_mojo_vea(), DoIsFlushSupported());
  bool ret = mojo_vea()->IsFlushSupported();
  base::RunLoop().RunUntilIdle();
  if (ret) {
    EXPECT_CALL(*mock_mojo_vea(), DoFlush(_));
    auto flush_callback =
        base::BindOnce([](bool status) { EXPECT_EQ(status, true); });
    mojo_vea()->Flush(std::move(flush_callback));
    base::RunLoop().RunUntilIdle();
  }
}

}  // namespace media
