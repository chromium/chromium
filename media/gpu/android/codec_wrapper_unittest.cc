// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "base/threading/thread.h"
#include "base/time/time.h"
#include "media/base/android/media_codec_bridge.h"
#include "media/base/android/mock_media_codec_bridge.h"
#include "media/base/encryption_scheme.h"
#include "media/base/subsample_entry.h"
#include "media/gpu/android/codec_wrapper.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::DoAll;
using testing::Invoke;
using testing::NiceMock;
using testing::Return;
using testing::SetArgPointee;

namespace media {

constexpr gfx::Size kInitialCodedSize(640, 480);
constexpr gfx::Size kCodedSizeAlignment(16, 16);

class CodecWrapperTest : public testing::Test {
 public:
  CodecWrapperTest() : other_thread_("Other thread") {
    auto codec = std::make_unique<NiceMock<MockMediaCodecBridge>>();
    codec_ = codec.get();
    surface_bundle_ = base::MakeRefCounted<CodecSurfaceBundle>();
    wrapper_ = std::make_unique<CodecWrapper>(
        CodecSurfacePair(std::move(codec), surface_bundle_),
        output_buffer_release_cb_.Get(),
        // Unrendered output buffers are released on our thread.
        base::SequencedTaskRunner::GetCurrentDefault(), kInitialCodedSize,
        gfx::ColorSpace::CreateREC709(), kCodedSizeAlignment, false);
    ON_CALL(*codec_, DequeueOutputBuffer(_, _, _, _, _, _, _))
        .WillByDefault(Return(OkStatus()));
    ON_CALL(*codec_, DequeueInputBuffer(_, _))
        .WillByDefault(DoAll(SetArgPointee<1>(12), Return(OkStatus())));
    ON_CALL(*codec_, QueueInputBuffer(_, _, _, _))
        .WillByDefault(Return(OkStatus()));

    uint8_t data = 0;
    fake_decoder_buffer_ = DecoderBuffer::CopyFrom(base::span_from_ref(data));

    // May fail.
    other_thread_.Start();
  }

  ~CodecWrapperTest() override {
    // ~CodecWrapper asserts that the codec was taken.
    wrapper_->TakeCodecSurfacePair();
  }

  std::unique_ptr<CodecOutputBuffer> DequeueCodecOutputBuffer() {
    std::unique_ptr<CodecOutputBuffer> codec_buffer;
    wrapper_->DequeueOutputBuffer(nullptr, nullptr, &codec_buffer);
    return codec_buffer;
  }

  // So that we can get the thread's task runner.
  base::test::TaskEnvironment task_environment_;

  raw_ptr<NiceMock<MockMediaCodecBridge>> codec_;
  std::unique_ptr<CodecWrapper> wrapper_;
  scoped_refptr<CodecSurfaceBundle> surface_bundle_;
  NiceMock<base::MockCallback<CodecWrapper::OutputReleasedCB>>
      output_buffer_release_cb_;
  scoped_refptr<DecoderBuffer> fake_decoder_buffer_;

  base::Thread other_thread_;
};

TEST_F(CodecWrapperTest, TakeCodecReturnsTheCodecFirstAndNullLater) {
  ASSERT_EQ(wrapper_->TakeCodecSurfacePair().first.get(), codec_);
  ASSERT_EQ(wrapper_->TakeCodecSurfacePair().first, nullptr);
}

TEST_F(CodecWrapperTest, NoCodecOutputBufferReturnedIfDequeueFails) {
  EXPECT_CALL(*codec_, DequeueOutputBuffer(_, _, _, _, _, _, _))
      .WillOnce(Return(MediaCodecResult::Codes::kError));
  auto codec_buffer = DequeueCodecOutputBuffer();
  ASSERT_EQ(codec_buffer, nullptr);
}

TEST_F(CodecWrapperTest, InitiallyThereAreNoValidCodecOutputBuffers) {
  ASSERT_FALSE(wrapper_->HasUnreleasedOutputBuffers());
}

TEST_F(CodecWrapperTest, FlushInvalidatesCodecOutputBuffers) {
  auto codec_buffer = DequeueCodecOutputBuffer();
  wrapper_->Flush();
  ASSERT_FALSE(codec_buffer->ReleaseToSurface());
}

TEST_F(CodecWrapperTest, TakingTheCodecInvalidatesCodecOutputBuffers) {
  auto codec_buffer = DequeueCodecOutputBuffer();
  wrapper_->TakeCodecSurfacePair();
  ASSERT_FALSE(codec_buffer->ReleaseToSurface());
}

TEST_F(CodecWrapperTest, SetSurfaceInvalidatesCodecOutputBuffers) {
  auto codec_buffer = DequeueCodecOutputBuffer();
  wrapper_->SetSurface(base::MakeRefCounted<CodecSurfaceBundle>());
  ASSERT_FALSE(codec_buffer->ReleaseToSurface());
}

TEST_F(CodecWrapperTest, CodecOutputBuffersAreAllInvalidatedTogether) {
  auto codec_buffer1 = DequeueCodecOutputBuffer();
  auto codec_buffer2 = DequeueCodecOutputBuffer();
  wrapper_->Flush();
  ASSERT_FALSE(codec_buffer1->ReleaseToSurface());
  ASSERT_FALSE(codec_buffer2->ReleaseToSurface());
  ASSERT_FALSE(wrapper_->HasUnreleasedOutputBuffers());
}

TEST_F(CodecWrapperTest, CodecOutputBuffersAfterFlushAreValid) {
  auto codec_buffer = DequeueCodecOutputBuffer();
  wrapper_->Flush();
  codec_buffer = DequeueCodecOutputBuffer();
  ASSERT_TRUE(codec_buffer->ReleaseToSurface());
}

TEST_F(CodecWrapperTest, CodecOutputBufferReleaseUsesCorrectIndex) {
  // The second arg is the buffer index pointer.
  EXPECT_CALL(*codec_, DequeueOutputBuffer(_, _, _, _, _, _, _))
      .WillOnce(DoAll(SetArgPointee<1>(42), Return(OkStatus())));
  auto codec_buffer = DequeueCodecOutputBuffer();
  EXPECT_CALL(*codec_, ReleaseOutputBuffer(42, true));
  codec_buffer->ReleaseToSurface();
}

TEST_F(CodecWrapperTest, CodecOutputBuffersAreInvalidatedByRelease) {
  auto codec_buffer = DequeueCodecOutputBuffer();
  codec_buffer->ReleaseToSurface();
  ASSERT_FALSE(codec_buffer->ReleaseToSurface());
}

TEST_F(CodecWrapperTest, CodecOutputBuffersReleaseOnDestruction) {
  auto codec_buffer = DequeueCodecOutputBuffer();
  EXPECT_CALL(*codec_, ReleaseOutputBuffer(_, false));
  codec_buffer = nullptr;
}

TEST_F(CodecWrapperTest, CodecOutputBuffersDoNotReleaseIfAlreadyReleased) {
  auto codec_buffer = DequeueCodecOutputBuffer();
  codec_buffer->ReleaseToSurface();
  EXPECT_CALL(*codec_, ReleaseOutputBuffer(_, _)).Times(0);
  codec_buffer = nullptr;
}

TEST_F(CodecWrapperTest, ReleasingCodecOutputBuffersAfterTheCodecIsSafe) {
  auto codec_buffer = DequeueCodecOutputBuffer();
  wrapper_->TakeCodecSurfacePair();
  codec_buffer->ReleaseToSurface();
}

TEST_F(CodecWrapperTest, DeletingCodecOutputBuffersAfterTheCodecIsSafe) {
  auto codec_buffer = DequeueCodecOutputBuffer();
  wrapper_->TakeCodecSurfacePair();
  // This test ensures the destructor doesn't crash.
  codec_buffer = nullptr;
}

TEST_F(CodecWrapperTest, CodecOutputBufferReleaseDoesNotInvalidateEarlierOnes) {
  auto codec_buffer1 = DequeueCodecOutputBuffer();
  auto codec_buffer2 = DequeueCodecOutputBuffer();
  codec_buffer2->ReleaseToSurface();
  EXPECT_TRUE(codec_buffer1->ReleaseToSurface());
}

TEST_F(CodecWrapperTest, CodecOutputBufferReleaseDoesNotInvalidateLaterOnes) {
  auto codec_buffer1 = DequeueCodecOutputBuffer();
  auto codec_buffer2 = DequeueCodecOutputBuffer();
  codec_buffer1->ReleaseToSurface();
  ASSERT_TRUE(codec_buffer2->ReleaseToSurface());
}

TEST_F(CodecWrapperTest, FormatChangedStatusIsSwallowed) {
  EXPECT_CALL(*codec_, DequeueOutputBuffer(_, _, _, _, _, _, _))
      .WillOnce(Return(MediaCodecResult::Codes::kOutputFormatChanged))
      .WillOnce(Return(MediaCodecResult::Codes::kTryAgainLater));
  std::unique_ptr<CodecOutputBuffer> codec_buffer;
  auto status = wrapper_->DequeueOutputBuffer(nullptr, nullptr, &codec_buffer);
  ASSERT_EQ(status, CodecWrapper::DequeueStatus::Codes::kTryAgainLater);
}

TEST_F(CodecWrapperTest, BuffersChangedStatusIsSwallowed) {
  EXPECT_CALL(*codec_, DequeueOutputBuffer(_, _, _, _, _, _, _))
      .WillOnce(Return(MediaCodecResult::Codes::kOutputBuffersChanged))
      .WillOnce(Return(MediaCodecResult::Codes::kTryAgainLater));
  std::unique_ptr<CodecOutputBuffer> codec_buffer;
  auto status = wrapper_->DequeueOutputBuffer(nullptr, nullptr, &codec_buffer);
  ASSERT_EQ(status, CodecWrapper::DequeueStatus::Codes::kTryAgainLater);
}

TEST_F(CodecWrapperTest, MultipleFormatChangedStatusesIsAnError) {
  EXPECT_CALL(*codec_, DequeueOutputBuffer(_, _, _, _, _, _, _))
      .WillRepeatedly(Return(MediaCodecResult::Codes::kOutputFormatChanged));
  std::unique_ptr<CodecOutputBuffer> codec_buffer;
  auto status = wrapper_->DequeueOutputBuffer(nullptr, nullptr, &codec_buffer);
  ASSERT_EQ(status, CodecWrapper::DequeueStatus::Codes::kError);
}

TEST_F(CodecWrapperTest, CodecOutputBuffersHaveTheCorrectSize) {
  EXPECT_CALL(*codec_, DequeueOutputBuffer(_, _, _, _, _, _, _))
      .WillOnce(Return(MediaCodecResult::Codes::kOutputFormatChanged))
      .WillOnce(Return(OkStatus()));
  EXPECT_CALL(*codec_, GetOutputSize(_))
      .WillOnce(DoAll(SetArgPointee<0>(gfx::Size(42, 42)), Return(OkStatus())));
  auto codec_buffer = DequeueCodecOutputBuffer();
  ASSERT_EQ(codec_buffer->size(), gfx::Size(42, 42));
}

TEST_F(CodecWrapperTest, CodecOutputBuffersGuessCodedSize) {
  EXPECT_CALL(*codec_, DequeueOutputBuffer(_, _, _, _, _, _, _))
      .WillOnce(Return(MediaCodecResult::Codes::kOutputFormatChanged))
      .WillOnce(Return(OkStatus()));
  EXPECT_CALL(*codec_, GetOutputSize(_))
      .WillOnce(DoAll(SetArgPointee<0>(gfx::Size(42, 42)), Return(OkStatus())));
  auto codec_buffer = DequeueCodecOutputBuffer();
  ASSERT_EQ(codec_buffer->size(), gfx::Size(42, 42));
  EXPECT_TRUE(codec_buffer->CanGuessCodedSize());
  EXPECT_EQ(codec_buffer->GuessCodedSize(), gfx::Size(48, 48));
}

TEST_F(CodecWrapperTest, CodecOutputBuffersGuessCodedSizeNoAlignment) {
  auto surface_pair = wrapper_->TakeCodecSurfacePair();
  wrapper_ = std::make_unique<CodecWrapper>(
      std::move(surface_pair), output_buffer_release_cb_.Get(),
      // Unrendered output buffers are released on our thread.
      base::SequencedTaskRunner::GetCurrentDefault(), kInitialCodedSize,
      gfx::ColorSpace::CreateREC709(), std::nullopt, false);

  EXPECT_CALL(*codec_, DequeueOutputBuffer(_, _, _, _, _, _, _))
      .WillOnce(Return(MediaCodecResult::Codes::kOutputFormatChanged))
      .WillOnce(Return(OkStatus()));
  EXPECT_CALL(*codec_, GetOutputSize(_))
      .WillOnce(DoAll(SetArgPointee<0>(gfx::Size(42, 42)), Return(OkStatus())));
  auto codec_buffer = DequeueCodecOutputBuffer();
  ASSERT_EQ(codec_buffer->size(), gfx::Size(42, 42));
  EXPECT_FALSE(codec_buffer->CanGuessCodedSize());
}

TEST_F(CodecWrapperTest, CodecOutputBuffersGuessCodedSizeWeirdAlignment) {
  auto surface_pair = wrapper_->TakeCodecSurfacePair();
  wrapper_ = std::make_unique<CodecWrapper>(
      std::move(surface_pair), output_buffer_release_cb_.Get(),
      // Unrendered output buffers are released on our thread.
      base::SequencedTaskRunner::GetCurrentDefault(), kInitialCodedSize,
      gfx::ColorSpace::CreateREC709(), gfx::Size(128, 1), false);

  EXPECT_CALL(*codec_, DequeueOutputBuffer(_, _, _, _, _, _, _))
      .WillOnce(Return(MediaCodecResult::Codes::kOutputFormatChanged))
      .WillOnce(Return(OkStatus()));
  EXPECT_CALL(*codec_, GetOutputSize(_))
      .WillOnce(DoAll(SetArgPointee<0>(gfx::Size(42, 42)), Return(OkStatus())));
  auto codec_buffer = DequeueCodecOutputBuffer();
  ASSERT_EQ(codec_buffer->size(), gfx::Size(42, 42));
  EXPECT_TRUE(codec_buffer->CanGuessCodedSize());
  EXPECT_EQ(codec_buffer->GuessCodedSize(), gfx::Size(128, 42));
}

TEST_F(CodecWrapperTest, OutputBufferReleaseCbIsCalledWhenRendering) {
  auto codec_buffer = DequeueCodecOutputBuffer();
  EXPECT_CALL(output_buffer_release_cb_, Run(true)).Times(1);
  codec_buffer->ReleaseToSurface();
}

TEST_F(CodecWrapperTest, OutputBufferReleaseCbIsCalledWhenDestructing) {
  auto codec_buffer = DequeueCodecOutputBuffer();
  EXPECT_CALL(output_buffer_release_cb_, Run(true)).Times(1);
}

TEST_F(CodecWrapperTest, OutputBufferReflectsDrainingOrDrainedStatus) {
  wrapper_->QueueInputBuffer(*fake_decoder_buffer_);
  auto eos = DecoderBuffer::CreateEOSBuffer();
  wrapper_->QueueInputBuffer(*eos);
  ASSERT_TRUE(wrapper_->IsDraining());
  auto codec_buffer = DequeueCodecOutputBuffer();
  EXPECT_CALL(output_buffer_release_cb_, Run(true)).Times(1);
}

TEST_F(CodecWrapperTest, CodecStartsInFlushedState) {
  ASSERT_TRUE(wrapper_->IsFlushed());
  ASSERT_FALSE(wrapper_->IsDraining());
  ASSERT_FALSE(wrapper_->IsDrained());
}

TEST_F(CodecWrapperTest, CodecIsNotInFlushedStateAfterAnInputIsQueued) {
  wrapper_->QueueInputBuffer(*fake_decoder_buffer_);
  ASSERT_FALSE(wrapper_->IsFlushed());
  ASSERT_FALSE(wrapper_->IsDraining());
  ASSERT_FALSE(wrapper_->IsDrained());
}

TEST_F(CodecWrapperTest, FlushTransitionsToFlushedState) {
  wrapper_->QueueInputBuffer(*fake_decoder_buffer_);
  wrapper_->Flush();
  ASSERT_TRUE(wrapper_->IsFlushed());
}

TEST_F(CodecWrapperTest, EosTransitionsToDrainingState) {
  wrapper_->QueueInputBuffer(*fake_decoder_buffer_);
  auto eos = DecoderBuffer::CreateEOSBuffer();
  wrapper_->QueueInputBuffer(*eos);
  ASSERT_TRUE(wrapper_->IsDraining());
}

TEST_F(CodecWrapperTest, DequeuingEosTransitionsToDrainedState) {
  // Set EOS on next dequeue.
  codec_->ProduceOneOutput(MockMediaCodecBridge::kEos);
  DequeueCodecOutputBuffer();
  ASSERT_FALSE(wrapper_->IsFlushed());
  ASSERT_TRUE(wrapper_->IsDrained());
  wrapper_->Flush();
  ASSERT_FALSE(wrapper_->IsDrained());
}

TEST_F(CodecWrapperTest, RejectedInputBuffersAreReused) {
  // If we get a MediaCodecResult::Codes::kNoKey status, the next time we try to
  // queue a buffer the previous input buffer should be reused.
  EXPECT_CALL(*codec_, DequeueInputBuffer(_, _))
      .WillOnce(DoAll(SetArgPointee<1>(666), Return(OkStatus())));
  EXPECT_CALL(*codec_, QueueInputBuffer(666, _, _, _))
      .WillOnce(Return(MediaCodecResult::Codes::kNoKey))
      .WillOnce(Return(OkStatus()));
  auto status = wrapper_->QueueInputBuffer(*fake_decoder_buffer_);
  ASSERT_EQ(status, CodecWrapper::QueueStatus::Codes::kNoKey);
  wrapper_->QueueInputBuffer(*fake_decoder_buffer_);
}

TEST_F(CodecWrapperTest, SurfaceBundleIsInitializedByConstructor) {
  ASSERT_EQ(surface_bundle_.get(), wrapper_->SurfaceBundle());
}

TEST_F(CodecWrapperTest, SurfaceBundleIsUpdatedBySetSurface) {
  auto new_bundle = base::MakeRefCounted<CodecSurfaceBundle>();
  EXPECT_CALL(*codec_, SetSurface(_)).WillOnce(Return(true));
  wrapper_->SetSurface(new_bundle);
  ASSERT_EQ(new_bundle.get(), wrapper_->SurfaceBundle());
}

TEST_F(CodecWrapperTest, SurfaceBundleIsTaken) {
  ASSERT_EQ(wrapper_->TakeCodecSurfacePair().second, surface_bundle_);
  ASSERT_EQ(wrapper_->SurfaceBundle(), nullptr);
}

TEST_F(CodecWrapperTest, EOSWhileFlushedOrDrainedIsElided) {
  // Nothing should call QueueEOS.
  EXPECT_CALL(*codec_, QueueEOS(_)).Times(0);

  // Codec starts in the flushed state.
  auto eos = DecoderBuffer::CreateEOSBuffer();
  wrapper_->QueueInputBuffer(*eos);
  std::unique_ptr<CodecOutputBuffer> codec_buffer;
  bool is_eos = false;
  wrapper_->DequeueOutputBuffer(nullptr, &is_eos, &codec_buffer);
  ASSERT_TRUE(is_eos);

  // Since we also just got the codec into the drained state, make sure that
  // it is elided here too.
  ASSERT_TRUE(wrapper_->IsDrained());
  eos = DecoderBuffer::CreateEOSBuffer();
  wrapper_->QueueInputBuffer(*eos);
  is_eos = false;
  wrapper_->DequeueOutputBuffer(nullptr, &is_eos, &codec_buffer);
  ASSERT_TRUE(is_eos);
}

TEST_F(CodecWrapperTest, CodecWrapperPostsReleaseToProvidedThread) {
  // Releasing an output buffer without rendering on some other thread should
  // post back to the main thread.
  scoped_refptr<base::SequencedTaskRunner> task_runner =
      other_thread_.task_runner();
  // If the thread failed to start, pass.
  if (!task_runner)
    return;

  base::WaitableEvent event(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                            base::WaitableEvent::InitialState::NOT_SIGNALED);
  auto cb = base::BindOnce(
      [](std::unique_ptr<CodecOutputBuffer> codec_buffer,
         base::WaitableEvent* event) {
        codec_buffer.reset();
        event->Signal();
      },
      DequeueCodecOutputBuffer(), base::Unretained(&event));
  task_runner->PostTask(FROM_HERE, std::move(cb));

  // Wait until the CodecOutputBuffer is released.  It should not release the
  // underlying buffer, but should instead post a task to release it.
  event.Wait();

  // The underlying buffer should not be released until we RunUntilIdle.
  EXPECT_CALL(*codec_, ReleaseOutputBuffer(_, false));
  base::RunLoop().RunUntilIdle();
}

TEST_F(CodecWrapperTest, RenderCallbackCalledIfRendered) {
  auto codec_buffer = DequeueCodecOutputBuffer();
  bool flag = false;
  codec_buffer->set_render_cb(base::BindOnce([](bool* flag) { *flag = true; },
                                             base::Unretained(&flag)));
  codec_buffer->ReleaseToSurface();
  EXPECT_TRUE(flag);
}

TEST_F(CodecWrapperTest, RenderCallbackIsNotCalledIfNotRendered) {
  auto codec_buffer = DequeueCodecOutputBuffer();
  bool flag = false;
  codec_buffer->set_render_cb(base::BindOnce([](bool* flag) { *flag = true; },
                                             base::Unretained(&flag)));
  codec_buffer.reset();
  EXPECT_FALSE(flag);
}

TEST_F(CodecWrapperTest, CodecWrapperGetsColorSpaceFromCodec) {
  // CodecWrapper should provide the color space that's reported by the bridge.
  EXPECT_CALL(*codec_, DequeueOutputBuffer(_, _, _, _, _, _, _))
      .WillOnce(Return(MediaCodecResult::Codes::kOutputFormatChanged))
      .WillOnce(Return(OkStatus()));
  gfx::ColorSpace color_space{gfx::ColorSpace::CreateHDR10()};
  EXPECT_CALL(*codec_, GetOutputColorSpace(_))
      .WillOnce(DoAll(SetArgPointee<0>(color_space), Return(OkStatus())));
  auto codec_buffer = DequeueCodecOutputBuffer();
  ASSERT_EQ(codec_buffer->color_space(), color_space);
}

TEST_F(CodecWrapperTest, CodecWrapperDefaultsToSRGB) {
  auto surface_pair = wrapper_->TakeCodecSurfacePair();
  wrapper_ = std::make_unique<CodecWrapper>(
      std::move(surface_pair), output_buffer_release_cb_.Get(),
      // Unrendered output buffers are released on our thread.
      base::SequencedTaskRunner::GetCurrentDefault(), kInitialCodedSize,
      gfx::ColorSpace(), std::nullopt, false);

  // If MediaCodec doesn't provide a color space and we don't have a valid
  // config color space, then CodecWrapper should default to sRGB for sanity.
  // CodecWrapper should provide the color space that's reported by the bridge.
  EXPECT_CALL(*codec_, DequeueOutputBuffer(_, _, _, _, _, _, _))
      .WillOnce(Return(MediaCodecResult::Codes::kOutputFormatChanged))
      .WillOnce(Return(OkStatus()));
  EXPECT_CALL(*codec_, GetOutputColorSpace(_))
      .WillOnce(Return(MediaCodecResult::Codes::kError));
  auto codec_buffer = DequeueCodecOutputBuffer();
  ASSERT_EQ(codec_buffer->color_space(), gfx::ColorSpace::CreateSRGB());
}

TEST_F(CodecWrapperTest, CodecWrapperUseConfigColorSpace) {
  auto surface_pair = wrapper_->TakeCodecSurfacePair();
  wrapper_ = std::make_unique<CodecWrapper>(
      std::move(surface_pair), output_buffer_release_cb_.Get(),
      // Unrendered output buffers are released on our thread.
      base::SequencedTaskRunner::GetCurrentDefault(), kInitialCodedSize,
      gfx::ColorSpace::CreateJpeg(), std::nullopt, false);

  // If MediaCodec doesn't provide a color space and we have a valid config
  // color space, then CodecWrapper should use it.
  EXPECT_CALL(*codec_, DequeueOutputBuffer(_, _, _, _, _, _, _))
      .WillOnce(Return(MediaCodecResult::Codes::kOutputFormatChanged))
      .WillOnce(Return(OkStatus()));
  EXPECT_CALL(*codec_, GetOutputColorSpace(_))
      .WillOnce(Return(MediaCodecResult::Codes::kError));
  auto codec_buffer = DequeueCodecOutputBuffer();
  ASSERT_EQ(codec_buffer->color_space(), gfx::ColorSpace::CreateJpeg());
}

TEST_F(CodecWrapperTest, CodecOutputsIgnoreZeroSize) {
  EXPECT_CALL(*codec_, DequeueOutputBuffer(_, _, _, _, _, _, _))
      .WillOnce(Return(MediaCodecResult::Codes::kOutputFormatChanged))
      .WillOnce(Return(OkStatus()))
      .WillOnce(Return(MediaCodecResult::Codes::kOutputFormatChanged))
      .WillOnce(Return(OkStatus()));

  constexpr gfx::Size kNewSize(1280, 720);
  EXPECT_CALL(*codec_, GetOutputSize(_))
      .WillOnce(DoAll(SetArgPointee<0>(gfx::Size()), Return(OkStatus())))
      .WillOnce(DoAll(SetArgPointee<0>(kNewSize), Return(OkStatus())));

  auto codec_buffer = DequeueCodecOutputBuffer();
  ASSERT_EQ(codec_buffer->size(), kInitialCodedSize);

  codec_buffer = DequeueCodecOutputBuffer();
  ASSERT_EQ(codec_buffer->size(), kNewSize);
}

}  // namespace media
