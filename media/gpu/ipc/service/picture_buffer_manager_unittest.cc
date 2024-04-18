// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include "media/gpu/ipc/service/picture_buffer_manager.h"

#include "base/memory/scoped_refptr.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "media/base/simple_sync_token_client.h"
#include "media/gpu/test/fake_command_buffer_helper.h"
#include "media/video/video_decode_accelerator.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

class PictureBufferManagerImplTest : public testing::Test {
 public:
  explicit PictureBufferManagerImplTest() {
    // TODO(sandersd): Use a separate thread for the GPU task runner.
    cbh_ = base::MakeRefCounted<FakeCommandBufferHelper>(
        environment_.GetMainThreadTaskRunner());
    pbm_ = PictureBufferManager::Create(/*allocate_gpu_memory_buffers=*/false,
                                        reuse_cb_.Get());
  }

  PictureBufferManagerImplTest(const PictureBufferManagerImplTest&) = delete;
  PictureBufferManagerImplTest& operator=(const PictureBufferManagerImplTest&) =
      delete;

  ~PictureBufferManagerImplTest() override {
    // Drop ownership of anything that may have an async destruction process,
    // then allow destruction to complete.
    cbh_->StubLost();
    cbh_ = nullptr;
    pbm_ = nullptr;
    environment_.RunUntilIdle();
  }

 protected:
  void Initialize() {
    pbm_->Initialize(environment_.GetMainThreadTaskRunner(), cbh_);
  }

  std::vector<PictureBuffer> CreateARGBPictureBuffers(uint32_t count) {
    std::vector<std::pair<PictureBuffer, gfx::GpuMemoryBufferHandle>>
        picture_buffers_and_gmbs = pbm_->CreatePictureBuffers(
            count, PIXEL_FORMAT_ARGB, gfx::Size(320, 240));
    std::vector<PictureBuffer> picture_buffers;
    for (const auto& picture_buffer_and_gmb : picture_buffers_and_gmbs) {
      picture_buffers.push_back(picture_buffer_and_gmb.first);
    }
    return picture_buffers;
  }

  PictureBuffer CreateARGBPictureBuffer() {
    std::vector<PictureBuffer> picture_buffers = CreateARGBPictureBuffers(1);
    DCHECK_EQ(picture_buffers.size(), 1U);
    return picture_buffers[0];
  }

  scoped_refptr<VideoFrame> CreateVideoFrame(int32_t picture_buffer_id) {
    auto picture =
        Picture(picture_buffer_id,
                /*bitstream_buffer_id=*/0,
                /*visible_rect=*/gfx::Rect(), gfx::ColorSpace::CreateSRGB(),
                /*allow_overlay=*/false);

    picture.set_scoped_shared_image(
        base::MakeRefCounted<Picture::ScopedSharedImage>(
            gpu::Mailbox::GenerateForSharedImage(), GL_TEXTURE_2D,
            base::DoNothing()));

    return pbm_->CreateVideoFrame(picture,
                                  base::TimeDelta(),  // timestamp
                                  gfx::Rect(1, 1),    // visible_rect
                                  gfx::Size(1, 1));   // natural_size
  }

  gpu::SyncToken GenerateSyncToken(scoped_refptr<VideoFrame> video_frame) {
    gpu::SyncToken sync_token(gpu::GPU_IO,
                              gpu::CommandBufferId::FromUnsafeValue(1),
                              next_release_count_++);
    SimpleSyncTokenClient sync_token_client(sync_token);
    video_frame->UpdateReleaseSyncToken(&sync_token_client);
    return sync_token;
  }

  base::test::TaskEnvironment environment_;

  uint64_t next_release_count_ = 1;
  testing::StrictMock<
      base::MockCallback<PictureBufferManager::ReusePictureBufferCB>>
      reuse_cb_;
  scoped_refptr<FakeCommandBufferHelper> cbh_;
  scoped_refptr<PictureBufferManager> pbm_;
};

TEST_F(PictureBufferManagerImplTest, CreateAndDestroy) {}

TEST_F(PictureBufferManagerImplTest, Initialize) {
  Initialize();
}

TEST_F(PictureBufferManagerImplTest, CreatePictureBuffer_SharedImage) {
  Initialize();
  PictureBuffer pb1 = CreateARGBPictureBuffer();
}

TEST_F(PictureBufferManagerImplTest, ReusePictureBuffer) {
  Initialize();
  PictureBuffer pb = CreateARGBPictureBuffer();
  scoped_refptr<VideoFrame> frame = CreateVideoFrame(pb.id());
  gpu::SyncToken sync_token = GenerateSyncToken(frame);

  // Dropping the frame does not immediately trigger reuse.
  frame = nullptr;
  environment_.RunUntilIdle();

  // Completing the SyncToken wait does.
  EXPECT_CALL(reuse_cb_, Run(pb.id()));
  cbh_->ReleaseSyncToken(sync_token);
  environment_.RunUntilIdle();
}

TEST_F(PictureBufferManagerImplTest, ReusePictureBuffer_MultipleOutputs) {
  constexpr size_t kOutputCountPerPictureBuffer = 3;

  Initialize();
  PictureBuffer pb = CreateARGBPictureBuffer();
  std::vector<scoped_refptr<VideoFrame>> frames;
  std::vector<gpu::SyncToken> sync_tokens;
  for (size_t i = 0; i < kOutputCountPerPictureBuffer; i++) {
    scoped_refptr<VideoFrame> frame = CreateVideoFrame(pb.id());
    frames.push_back(frame);
    sync_tokens.push_back(GenerateSyncToken(frame));
  }

  // Dropping the frames does not immediately trigger reuse.
  frames.clear();
  environment_.RunUntilIdle();

  // Completing the SyncToken waits does. (Clients are expected to wait for the
  // output count to reach zero before actually reusing the picture buffer.)
  EXPECT_CALL(reuse_cb_, Run(pb.id())).Times(kOutputCountPerPictureBuffer);
  for (const auto& sync_token : sync_tokens)
    cbh_->ReleaseSyncToken(sync_token);
  environment_.RunUntilIdle();
}

TEST_F(PictureBufferManagerImplTest, CanReadWithoutStalling) {
  // Works before Initialize().
  EXPECT_TRUE(pbm_->CanReadWithoutStalling());

  // True before any picture buffers are allocated.
  Initialize();
  EXPECT_TRUE(pbm_->CanReadWithoutStalling());

  // True when a picture buffer is available.
  PictureBuffer pb = CreateARGBPictureBuffer();
  EXPECT_TRUE(pbm_->CanReadWithoutStalling());

  // False when all picture buffers are used.
  scoped_refptr<VideoFrame> frame = CreateVideoFrame(pb.id());
  EXPECT_FALSE(pbm_->CanReadWithoutStalling());

  // True once a picture buffer is returned.
  frame = nullptr;
  EXPECT_TRUE(pbm_->CanReadWithoutStalling());

  // True after all picture buffers have been dismissed.
  pbm_->DismissPictureBuffer(pb.id());
  EXPECT_TRUE(pbm_->CanReadWithoutStalling());
}

}  // namespace media
