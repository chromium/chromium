// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/ipc/service/vda_video_decoder.h"

#include <stdint.h>

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/memory/scoped_refptr.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "base/threading/thread.h"
#include "base/time/time.h"
#include "gpu/command_buffer/common/sync_token.h"
#include "media/base/decode_status.h"
#include "media/base/decoder_buffer.h"
#include "media/base/media_util.h"
#include "media/base/mock_media_log.h"
#include "media/base/simple_sync_token_client.h"
#include "media/base/video_codecs.h"
#include "media/base/video_frame.h"
#include "media/base/video_transformation.h"
#include "media/base/video_types.h"
#include "media/gpu/ipc/service/picture_buffer_manager.h"
#include "media/gpu/test/fake_command_buffer_helper.h"
#include "media/video/mock_video_decode_accelerator.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

using ::testing::_;
using ::testing::DoAll;
using ::testing::Invoke;
using ::testing::Return;
using ::testing::SaveArg;

namespace media {

namespace {

constexpr uint8_t kData[] = "foo";
constexpr size_t kDataSize = base::size(kData);

scoped_refptr<DecoderBuffer> CreateDecoderBuffer(base::TimeDelta timestamp) {
  scoped_refptr<DecoderBuffer> buffer =
      DecoderBuffer::CopyFrom(kData, kDataSize);
  buffer->set_timestamp(timestamp);
  return buffer;
}

VideoDecodeAccelerator::SupportedProfiles GetSupportedProfiles() {
  VideoDecodeAccelerator::SupportedProfiles profiles;
  {
    VideoDecodeAccelerator::SupportedProfile profile;
    profile.profile = VP9PROFILE_PROFILE0;
    profile.max_resolution = gfx::Size(1920, 1088);
    profile.min_resolution = gfx::Size(640, 480);
    profile.encrypted_only = false;
    profiles.push_back(std::move(profile));
  }
  return profiles;
}

VideoDecodeAccelerator::Capabilities GetCapabilities() {
  VideoDecodeAccelerator::Capabilities capabilities;
  capabilities.supported_profiles = GetSupportedProfiles();
  capabilities.flags = 0;
  return capabilities;
}

}  // namespace

// Parameterized by |decode_on_parent_thread|.
class VdaVideoDecoderTest : public testing::TestWithParam<bool> {
 public:
  explicit VdaVideoDecoderTest() : gpu_thread_("GPU Thread") {
    gpu_thread_.Start();
    scoped_refptr<base::SingleThreadTaskRunner> parent_task_runner =
        environment_.GetMainThreadTaskRunner();
    scoped_refptr<base::SingleThreadTaskRunner> gpu_task_runner =
        gpu_thread_.task_runner();
    cbh_ = base::MakeRefCounted<FakeCommandBufferHelper>(gpu_task_runner);

    // |owned_vda_| exists to delete |vda_| when |this| is destructed. Ownership
    // is passed to |vdavd_| by CreateVda(), but |vda_| remains to be used for
    // configuring mock expectations.
    vda_ = new testing::StrictMock<MockVideoDecodeAccelerator>();
    owned_vda_.reset(vda_);

    // In either case, vda_->Destroy() should be called once.
    EXPECT_CALL(*vda_, Destroy());

    vdavd_.reset(new VdaVideoDecoder(
        parent_task_runner, gpu_task_runner, media_log_.Clone(),
        gfx::ColorSpace(),
        base::BindOnce(&VdaVideoDecoderTest::CreatePictureBufferManager,
                       base::Unretained(this)),
        base::BindOnce(&VdaVideoDecoderTest::CreateCommandBufferHelper,
                       base::Unretained(this)),
        base::BindRepeating(&VdaVideoDecoderTest::CreateAndInitializeVda,
                            base::Unretained(this)),
        GetCapabilities()));
    client_ = vdavd_.get();
  }

  ~VdaVideoDecoderTest() override {
    // Drop ownership of anything that may have an async destruction process,
    // then allow destruction to complete.
    gpu_thread_.task_runner()->PostTask(
        FROM_HERE, base::BindOnce(&FakeCommandBufferHelper::StubLost, cbh_));
    cbh_ = nullptr;
    owned_vda_ = nullptr;
    pbm_ = nullptr;
    vdavd_ = nullptr;
    RunUntilIdle();
  }

 protected:
  void RunUntilIdle() {
    // FlushForTesting() only runs tasks that have already been posted.
    // TODO(sandersd): Find a more reliable way to run all tasks.
    gpu_thread_.FlushForTesting();
    gpu_thread_.FlushForTesting();
    environment_.RunUntilIdle();
  }

  void InitializeWithConfig(const VideoDecoderConfig& config) {
    vdavd_->Initialize(config, false, nullptr, init_cb_.Get(), output_cb_.Get(),
                       waiting_cb_.Get());
  }

  void Initialize() {
    EXPECT_CALL(*vda_, Initialize(_, vdavd_.get())).WillOnce(Return(true));
    EXPECT_CALL(*vda_, TryToSetupDecodeOnSeparateThread(_, _))
        .WillOnce(Return(GetParam()));
    EXPECT_CALL(init_cb_, Run(true));
    InitializeWithConfig(VideoDecoderConfig(
        kCodecVP9, VP9PROFILE_PROFILE0,
        VideoDecoderConfig::AlphaMode::kIsOpaque, VideoColorSpace::REC709(),
        kNoTransformation, gfx::Size(1920, 1088), gfx::Rect(1920, 1080),
        gfx::Size(1920, 1080), EmptyExtraData(),
        EncryptionScheme::kUnencrypted));
    RunUntilIdle();
  }

  int32_t ProvidePictureBuffer() {
    std::vector<PictureBuffer> picture_buffers;
    EXPECT_CALL(*vda_, AssignPictureBuffers(_))
        .WillOnce(SaveArg<0>(&picture_buffers));
    gpu_thread_.task_runner()->PostTask(
        FROM_HERE,
        base::BindOnce(&VideoDecodeAccelerator::Client::ProvidePictureBuffers,
                       base::Unretained(client_), 1, PIXEL_FORMAT_XRGB, 1,
                       gfx::Size(1920, 1088), GL_TEXTURE_2D));
    RunUntilIdle();
    DCHECK_EQ(picture_buffers.size(), 1U);
    return picture_buffers[0].id();
  }

  int32_t Decode(base::TimeDelta timestamp) {
    int32_t bitstream_id = 0;
    EXPECT_CALL(*vda_, Decode(_, _)).WillOnce(SaveArg<1>(&bitstream_id));
    vdavd_->Decode(CreateDecoderBuffer(timestamp), decode_cb_.Get());
    RunUntilIdle();
    return bitstream_id;
  }

  void NotifyEndOfBitstreamBuffer(int32_t bitstream_id) {
    EXPECT_CALL(decode_cb_, Run(DecodeStatus::OK));
    if (GetParam()) {
      // TODO(sandersd): The VDA could notify on either thread. Test both.
      client_->NotifyEndOfBitstreamBuffer(bitstream_id);
    } else {
      gpu_thread_.task_runner()->PostTask(
          FROM_HERE,
          base::BindOnce(
              &VideoDecodeAccelerator::Client::NotifyEndOfBitstreamBuffer,
              base::Unretained(client_), bitstream_id));
    }
    RunUntilIdle();
  }

  scoped_refptr<VideoFrame> PictureReady_NoRunUntilIdle(
      int32_t bitstream_buffer_id,
      int32_t picture_buffer_id,
      gfx::Rect visible_rect = gfx::Rect(1920, 1080)) {
    scoped_refptr<VideoFrame> frame;
    Picture picture(picture_buffer_id, bitstream_buffer_id, visible_rect,
                    gfx::ColorSpace::CreateSRGB(), true);
    EXPECT_CALL(output_cb_, Run(_)).WillOnce(SaveArg<0>(&frame));
    if (GetParam()) {
      // TODO(sandersd): The first time a picture is output, VDAs will do so on
      // the GPU thread (because GpuVideoDecodeAccelerator required that). Test
      // both.
      client_->PictureReady(picture);
    } else {
      gpu_thread_.task_runner()->PostTask(
          FROM_HERE,
          base::BindOnce(&VideoDecodeAccelerator::Client::PictureReady,
                         base::Unretained(client_), picture));
    }
    return frame;
  }

  scoped_refptr<VideoFrame> PictureReady(
      int32_t bitstream_buffer_id,
      int32_t picture_buffer_id,
      gfx::Rect visible_rect = gfx::Rect(1920, 1080)) {
    scoped_refptr<VideoFrame> frame = PictureReady_NoRunUntilIdle(
        bitstream_buffer_id, picture_buffer_id, visible_rect);
    RunUntilIdle();
    return frame;
  }

  void DismissPictureBuffer(int32_t picture_buffer_id) {
    gpu_thread_.task_runner()->PostTask(
        FROM_HERE,
        base::BindOnce(&VideoDecodeAccelerator::Client::DismissPictureBuffer,
                       base::Unretained(client_), picture_buffer_id));
    RunUntilIdle();
  }

  void NotifyFlushDone() {
    gpu_thread_.task_runner()->PostTask(
        FROM_HERE,
        base::BindOnce(&VideoDecodeAccelerator::Client::NotifyFlushDone,
                       base::Unretained(client_)));
    RunUntilIdle();
  }

  void NotifyResetDone() {
    gpu_thread_.task_runner()->PostTask(
        FROM_HERE,
        base::BindOnce(&VideoDecodeAccelerator::Client::NotifyResetDone,
                       base::Unretained(client_)));
    RunUntilIdle();
  }

  void NotifyError(VideoDecodeAccelerator::Error error) {
    gpu_thread_.task_runner()->PostTask(
        FROM_HERE, base::BindOnce(&VideoDecodeAccelerator::Client::NotifyError,
                                  base::Unretained(client_), error));
    RunUntilIdle();
  }

  void ReleaseSyncToken(gpu::SyncToken sync_token) {
    gpu_thread_.task_runner()->PostTask(
        FROM_HERE, base::BindOnce(&FakeCommandBufferHelper::ReleaseSyncToken,
                                  cbh_, sync_token));
    RunUntilIdle();
  }

  // TODO(sandersd): This exact code is also used in
  // PictureBufferManagerImplTest. Share the implementation.
  gpu::SyncToken GenerateSyncToken(scoped_refptr<VideoFrame> video_frame) {
    gpu::SyncToken sync_token(gpu::GPU_IO,
                              gpu::CommandBufferId::FromUnsafeValue(1),
                              next_release_count_++);
    SimpleSyncTokenClient sync_token_client(sync_token);
    video_frame->UpdateReleaseSyncToken(&sync_token_client);
    return sync_token;
  }

  scoped_refptr<CommandBufferHelper> CreateCommandBufferHelper() {
    return cbh_;
  }

  scoped_refptr<PictureBufferManager> CreatePictureBufferManager(
      PictureBufferManager::ReusePictureBufferCB reuse_cb) {
    DCHECK(!pbm_);
    pbm_ = PictureBufferManager::Create(std::move(reuse_cb));
    return pbm_;
  }

  std::unique_ptr<VideoDecodeAccelerator> CreateAndInitializeVda(
      scoped_refptr<CommandBufferHelper> command_buffer_helper,
      VideoDecodeAccelerator::Client* client,
      MediaLog* media_log,
      const VideoDecodeAccelerator::Config& config) {
    DCHECK(owned_vda_);
    if (!owned_vda_->Initialize(config, client))
      return nullptr;
    return std::move(owned_vda_);
  }

  base::test::TaskEnvironment environment_;
  base::Thread gpu_thread_;

  testing::NiceMock<MockMediaLog> media_log_;
  testing::StrictMock<base::MockCallback<VideoDecoder::InitCB>> init_cb_;
  testing::StrictMock<base::MockCallback<VideoDecoder::OutputCB>> output_cb_;
  testing::StrictMock<base::MockCallback<WaitingCB>> waiting_cb_;
  testing::StrictMock<base::MockCallback<VideoDecoder::DecodeCB>> decode_cb_;
  testing::StrictMock<base::MockCallback<base::RepeatingClosure>> reset_cb_;

  scoped_refptr<FakeCommandBufferHelper> cbh_;
  testing::StrictMock<MockVideoDecodeAccelerator>* vda_;
  std::unique_ptr<VideoDecodeAccelerator> owned_vda_;
  scoped_refptr<PictureBufferManager> pbm_;
  std::unique_ptr<VdaVideoDecoder, std::default_delete<VideoDecoder>> vdavd_;

  VideoDecodeAccelerator::Client* client_;
  uint64_t next_release_count_ = 1;

  DISALLOW_COPY_AND_ASSIGN(VdaVideoDecoderTest);
};

TEST_P(VdaVideoDecoderTest, CreateAndDestroy) {}

TEST_P(VdaVideoDecoderTest, Initialize) {
  Initialize();
}

TEST_P(VdaVideoDecoderTest, Initialize_UnsupportedSize) {
  InitializeWithConfig(VideoDecoderConfig(
      kCodecVP9, VP9PROFILE_PROFILE0, VideoDecoderConfig::AlphaMode::kIsOpaque,
      VideoColorSpace::REC601(), kNoTransformation, gfx::Size(320, 240),
      gfx::Rect(320, 240), gfx::Size(320, 240), EmptyExtraData(),
      EncryptionScheme::kUnencrypted));
  EXPECT_CALL(init_cb_, Run(false));
  RunUntilIdle();
}

TEST_P(VdaVideoDecoderTest, Initialize_UnsupportedCodec) {
  InitializeWithConfig(VideoDecoderConfig(
      kCodecH264, H264PROFILE_BASELINE,
      VideoDecoderConfig::AlphaMode::kIsOpaque, VideoColorSpace::REC709(),
      kNoTransformation, gfx::Size(1920, 1088), gfx::Rect(1920, 1080),
      gfx::Size(1920, 1080), EmptyExtraData(), EncryptionScheme::kUnencrypted));
  EXPECT_CALL(init_cb_, Run(false));
  RunUntilIdle();
}

TEST_P(VdaVideoDecoderTest, Initialize_RejectedByVda) {
  EXPECT_CALL(*vda_, Initialize(_, vdavd_.get())).WillOnce(Return(false));
  InitializeWithConfig(VideoDecoderConfig(
      kCodecVP9, VP9PROFILE_PROFILE0, VideoDecoderConfig::AlphaMode::kIsOpaque,
      VideoColorSpace::REC709(), kNoTransformation, gfx::Size(1920, 1088),
      gfx::Rect(1920, 1080), gfx::Size(1920, 1080), EmptyExtraData(),
      EncryptionScheme::kUnencrypted));
  EXPECT_CALL(init_cb_, Run(false));
  RunUntilIdle();
}

TEST_P(VdaVideoDecoderTest, ProvideAndDismissPictureBuffer) {
  Initialize();
  int32_t id = ProvidePictureBuffer();
  DismissPictureBuffer(id);
}

TEST_P(VdaVideoDecoderTest, Decode) {
  Initialize();
  int32_t bitstream_id = Decode(base::TimeDelta());
  NotifyEndOfBitstreamBuffer(bitstream_id);
}

TEST_P(VdaVideoDecoderTest, Decode_Reset) {
  Initialize();
  Decode(base::TimeDelta());

  EXPECT_CALL(*vda_, Reset());
  vdavd_->Reset(reset_cb_.Get());
  RunUntilIdle();

  EXPECT_CALL(decode_cb_, Run(DecodeStatus::ABORTED));
  EXPECT_CALL(reset_cb_, Run());
  NotifyResetDone();
}

TEST_P(VdaVideoDecoderTest, Decode_NotifyError) {
  Initialize();
  Decode(base::TimeDelta());

  EXPECT_CALL(decode_cb_, Run(DecodeStatus::DECODE_ERROR));
  NotifyError(VideoDecodeAccelerator::PLATFORM_FAILURE);
}

TEST_P(VdaVideoDecoderTest, Decode_OutputAndReuse) {
  Initialize();
  int32_t bitstream_id = Decode(base::TimeDelta());
  NotifyEndOfBitstreamBuffer(bitstream_id);
  int32_t picture_buffer_id = ProvidePictureBuffer();
  scoped_refptr<VideoFrame> frame =
      PictureReady(bitstream_id, picture_buffer_id);

  // Dropping the frame triggers reuse, which will wait on the SyncPoint.
  gpu::SyncToken sync_token = GenerateSyncToken(frame);
  frame = nullptr;
  RunUntilIdle();

  // But the VDA won't be notified until the SyncPoint wait completes.
  EXPECT_CALL(*vda_, ReusePictureBuffer(picture_buffer_id));
  ReleaseSyncToken(sync_token);
}

TEST_P(VdaVideoDecoderTest, Decode_OutputAndDismiss) {
  Initialize();
  int32_t bitstream_id = Decode(base::TimeDelta());
  NotifyEndOfBitstreamBuffer(bitstream_id);
  int32_t picture_buffer_id = ProvidePictureBuffer();
  scoped_refptr<VideoFrame> frame =
      PictureReady_NoRunUntilIdle(bitstream_id, picture_buffer_id);
  DismissPictureBuffer(picture_buffer_id);

  // Dropping the frame still requires a SyncPoint to wait on.
  gpu::SyncToken sync_token = GenerateSyncToken(frame);
  frame = nullptr;
  RunUntilIdle();

  // But the VDA should not be notified when it completes.
  ReleaseSyncToken(sync_token);
}

TEST_P(VdaVideoDecoderTest, Decode_Output_MaintainsAspect) {
  // Initialize with a config that has a 2:1 pixel aspect ratio.
  EXPECT_CALL(*vda_, Initialize(_, vdavd_.get())).WillOnce(Return(true));
  EXPECT_CALL(*vda_, TryToSetupDecodeOnSeparateThread(_, _))
      .WillOnce(Return(GetParam()));
  InitializeWithConfig(VideoDecoderConfig(
      kCodecVP9, VP9PROFILE_PROFILE0, VideoDecoderConfig::AlphaMode::kIsOpaque,
      VideoColorSpace::REC709(), kNoTransformation, gfx::Size(640, 480),
      gfx::Rect(640, 480), gfx::Size(1280, 480), EmptyExtraData(),
      EncryptionScheme::kUnencrypted));
  EXPECT_CALL(init_cb_, Run(true));
  RunUntilIdle();

  // Assign a picture buffer that has size 1920x1088.
  int32_t picture_buffer_id = ProvidePictureBuffer();

  // Produce a frame that has visible size 320x240.
  int32_t bitstream_id = Decode(base::TimeDelta());
  NotifyEndOfBitstreamBuffer(bitstream_id);

  scoped_refptr<VideoFrame> frame =
      PictureReady(bitstream_id, picture_buffer_id, gfx::Rect(320, 240));

  // The frame should have |natural_size| 640x240 (pixel aspect ratio
  // preserved).
  ASSERT_TRUE(frame);
  EXPECT_EQ(frame->natural_size(), gfx::Size(640, 240));
  EXPECT_EQ(frame->coded_size(), gfx::Size(1920, 1088));
  EXPECT_EQ(frame->visible_rect(), gfx::Rect(320, 240));
}

TEST_P(VdaVideoDecoderTest, Flush) {
  Initialize();
  EXPECT_CALL(*vda_, Flush());
  vdavd_->Decode(DecoderBuffer::CreateEOSBuffer(), decode_cb_.Get());
  RunUntilIdle();

  EXPECT_CALL(decode_cb_, Run(DecodeStatus::OK));
  NotifyFlushDone();
}

INSTANTIATE_TEST_SUITE_P(VdaVideoDecoder,
                         VdaVideoDecoderTest,
                         ::testing::Values(false, true));

}  // namespace media
