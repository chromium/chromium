// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/android/codec_wrapper.h"

#include <android/hardware_buffer.h>
#include <stddef.h>
#include <stdint.h>

#include <limits>
#include <memory>
#include <optional>
#include <utility>

#include "base/android/scoped_hardware_buffer_handle.h"
#include "base/android/scoped_java_ref.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/numerics/safe_conversions.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "base/threading/thread.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "media/base/android/media_codec_bridge.h"
#include "media/base/android/media_format_color_space.h"
#include "media/base/android/mock_android_overlay.h"
#include "media/base/android/mock_media_codec_bridge.h"
#include "media/base/encryption_scheme.h"
#include "media/base/subsample_entry.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

using testing::_;
using testing::DoAll;
using testing::Invoke;
using testing::NiceMock;
using testing::Return;
using testing::ReturnRef;
using testing::SetArgPointee;
using testing::SetArgReferee;

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
        output_buffer_release_cb_.Get(), kInitialCodedSize,
        kCodedSizeAlignment);
    ON_CALL(*codec_, DequeueOutputBuffer(_, _, _, _, _, _, _))
        .WillByDefault(Return(OkStatus()));
    ON_CALL(*codec_, DequeueInputBuffer(_, _))
        .WillByDefault(DoAll(SetArgPointee<1>(12), Return(OkStatus())));
    ON_CALL(*codec_, QueueInputBuffer(_, _, _))
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
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

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
  EXPECT_CALL(*codec_, GetOutputSizeAndCropRect(_, _))
      .WillOnce(DoAll(SetArgReferee<0>(gfx::Size(48, 48)),
                      SetArgReferee<1>(gfx::Rect(42, 42)), Return(OkStatus())));
  auto codec_buffer = DequeueCodecOutputBuffer();
  ASSERT_EQ(codec_buffer->visible_size(), gfx::Size(42, 42));
  ASSERT_EQ(codec_buffer->media_format_visible_rect(), gfx::Rect(42, 42));
  ASSERT_EQ(codec_buffer->media_format_output_size(), gfx::Size(48, 48));
}

TEST_F(CodecWrapperTest, CodecOutputBuffersGuessCodedSize) {
  EXPECT_CALL(*codec_, DequeueOutputBuffer(_, _, _, _, _, _, _))
      .WillOnce(Return(MediaCodecResult::Codes::kOutputFormatChanged))
      .WillOnce(Return(OkStatus()));
  EXPECT_CALL(*codec_, GetOutputSizeAndCropRect(_, _))
      .WillOnce(DoAll(SetArgReferee<0>(gfx::Size(48, 48)),
                      SetArgReferee<1>(gfx::Rect(42, 42)), Return(OkStatus())));
  auto codec_buffer = DequeueCodecOutputBuffer();
  ASSERT_EQ(codec_buffer->visible_size(), gfx::Size(42, 42));
  ASSERT_EQ(codec_buffer->media_format_visible_rect(), gfx::Rect(42, 42));
  ASSERT_EQ(codec_buffer->media_format_output_size(), gfx::Size(48, 48));
  EXPECT_TRUE(codec_buffer->CanGuessCodedSize());
  EXPECT_EQ(codec_buffer->GuessCodedSize(), gfx::Size(48, 48));
}

TEST_F(CodecWrapperTest, CodecOutputBuffersGuessCodedSizeNoAlignment) {
  auto surface_pair = wrapper_->TakeCodecSurfacePair();
  wrapper_ = std::make_unique<CodecWrapper>(std::move(surface_pair),
                                            output_buffer_release_cb_.Get(),
                                            kInitialCodedSize, std::nullopt);

  EXPECT_CALL(*codec_, DequeueOutputBuffer(_, _, _, _, _, _, _))
      .WillOnce(Return(MediaCodecResult::Codes::kOutputFormatChanged))
      .WillOnce(Return(OkStatus()));
  EXPECT_CALL(*codec_, GetOutputSizeAndCropRect(_, _))
      .WillOnce(DoAll(SetArgReferee<0>(gfx::Size(48, 48)),
                      SetArgReferee<1>(gfx::Rect(42, 42)), Return(OkStatus())));
  auto codec_buffer = DequeueCodecOutputBuffer();
  ASSERT_EQ(codec_buffer->visible_size(), gfx::Size(42, 42));
  ASSERT_EQ(codec_buffer->media_format_visible_rect(), gfx::Rect(42, 42));
  ASSERT_EQ(codec_buffer->media_format_output_size(), gfx::Size(48, 48));
  EXPECT_FALSE(codec_buffer->CanGuessCodedSize());
}

TEST_F(CodecWrapperTest, CodecOutputBuffersGuessCodedSizeWeirdAlignment) {
  auto surface_pair = wrapper_->TakeCodecSurfacePair();
  wrapper_ = std::make_unique<CodecWrapper>(
      std::move(surface_pair), output_buffer_release_cb_.Get(),
      kInitialCodedSize, gfx::Size(128, 1));

  EXPECT_CALL(*codec_, DequeueOutputBuffer(_, _, _, _, _, _, _))
      .WillOnce(Return(MediaCodecResult::Codes::kOutputFormatChanged))
      .WillOnce(Return(OkStatus()));
  EXPECT_CALL(*codec_, GetOutputSizeAndCropRect(_, _))
      .WillOnce(DoAll(SetArgReferee<0>(gfx::Size(48, 48)),
                      SetArgReferee<1>(gfx::Rect(42, 42)), Return(OkStatus())));
  auto codec_buffer = DequeueCodecOutputBuffer();
  ASSERT_EQ(codec_buffer->visible_size(), gfx::Size(42, 42));
  ASSERT_EQ(codec_buffer->media_format_visible_rect(), gfx::Rect(42, 42));
  ASSERT_EQ(codec_buffer->media_format_output_size(), gfx::Size(48, 48));
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
  EXPECT_CALL(*codec_, QueueInputBuffer(666, _, _))
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
  MediaFormatColorSpace color_space = MediaFormatColorSpace::MakeHdr10();
  EXPECT_CALL(*codec_, GetOutputColorSpace(_))
      .WillOnce(DoAll(SetArgPointee<0>(color_space), Return(OkStatus())));
  auto codec_buffer = DequeueCodecOutputBuffer();
  ASSERT_EQ(codec_buffer->color_space(), color_space);
}

TEST_F(CodecWrapperTest, CodecOutputsIgnoreZeroSize) {
  EXPECT_CALL(*codec_, DequeueOutputBuffer(_, _, _, _, _, _, _))
      .WillOnce(Return(MediaCodecResult::Codes::kOutputFormatChanged))
      .WillOnce(Return(OkStatus()))
      .WillOnce(Return(MediaCodecResult::Codes::kOutputFormatChanged))
      .WillOnce(Return(OkStatus()));

  constexpr gfx::Size kNewSize(1280, 720);
  EXPECT_CALL(*codec_, GetOutputSizeAndCropRect(_, _))
      .WillOnce(DoAll(SetArgReferee<0>(gfx::Size()),
                      SetArgReferee<1>(gfx::Rect()), Return(OkStatus())))
      .WillOnce(DoAll(SetArgReferee<0>(kNewSize),
                      SetArgReferee<1>(gfx::Rect(kNewSize)),
                      Return(OkStatus())));

  auto codec_buffer = DequeueCodecOutputBuffer();
  ASSERT_EQ(codec_buffer->visible_size(), kInitialCodedSize);
  ASSERT_EQ(codec_buffer->media_format_visible_rect(),
            gfx::Rect(kInitialCodedSize));
  ASSERT_EQ(codec_buffer->media_format_output_size(), kInitialCodedSize);

  codec_buffer = DequeueCodecOutputBuffer();
  ASSERT_EQ(codec_buffer->visible_size(), kNewSize);
  ASSERT_EQ(codec_buffer->media_format_visible_rect(), gfx::Rect(kNewSize));
  ASSERT_EQ(codec_buffer->media_format_output_size(), kNewSize);
}

TEST_F(CodecWrapperTest, RecordsSubVsyncReleaseHistogramsForTextureOwner) {
  base::HistogramTester histogram_tester;

  auto codec_buffer1 = DequeueCodecOutputBuffer();
  auto codec_buffer2 = DequeueCodecOutputBuffer();
  auto codec_buffer3 = DequeueCodecOutputBuffer();

  // No surface-mode or release-interval samples are emitted until the first
  // output buffer is actually rendered.
  histogram_tester.ExpectTotalCount("Media.CodecWrapper.IsOverlayBacked", 0);

  ASSERT_TRUE(codec_buffer1->ReleaseToSurface());
  histogram_tester.ExpectUniqueSample("Media.CodecWrapper.IsOverlayBacked",
                                      false, 1);

  task_environment_.FastForwardBy(base::Milliseconds(16));
  ASSERT_TRUE(codec_buffer2->ReleaseToSurface());
  task_environment_.FastForwardBy(base::Milliseconds(3));
  ASSERT_TRUE(codec_buffer3->ReleaseToSurface());

  // Rendering additional buffers on the same surface does not re-emit
  // IsOverlayBacked.
  histogram_tester.ExpectUniqueSample("Media.CodecWrapper.IsOverlayBacked",
                                      false, 1);
  EXPECT_THAT(histogram_tester.GetAllSamples(
                  "Media.CodecOutputBuffer.SubVsyncRelease.TextureOwner"),
              base::BucketsAre(base::Bucket(false, 1), base::Bucket(true, 1)));
  histogram_tester.ExpectTimeBucketCount(
      "Media.CodecOutputBuffer.ReleaseInterval.TextureOwner",
      base::Milliseconds(16), 1);
  histogram_tester.ExpectTimeBucketCount(
      "Media.CodecOutputBuffer.ReleaseInterval.TextureOwner",
      base::Milliseconds(3), 1);
  histogram_tester.ExpectTotalCount(
      "Media.CodecOutputBuffer.ReleaseInterval.TextureOwner", 2);
}

TEST_F(CodecWrapperTest, RecordsSubVsyncReleaseHistogramsForOverlay) {
  base::HistogramTester histogram_tester;

  base::android::ScopedJavaLocalRef<jobject> fake_surface;
  auto overlay = std::make_unique<MockAndroidOverlay>();
  ON_CALL(*overlay, GetJavaSurface()).WillByDefault(ReturnRef(fake_surface));
  auto overlay_bundle =
      base::MakeRefCounted<CodecSurfaceBundle>(std::move(overlay));
  EXPECT_CALL(*codec_, SetSurface(_)).Times(2).WillRepeatedly(Return(true));
  ASSERT_TRUE(wrapper_->SetSurface(overlay_bundle));

  // Transient SetSurface() calls that have not rendered a frame do not emit
  // IsOverlayBacked samples.
  histogram_tester.ExpectTotalCount("Media.CodecWrapper.IsOverlayBacked", 0);

  auto codec_buffer1 = DequeueCodecOutputBuffer();
  auto codec_buffer2 = DequeueCodecOutputBuffer();

  ASSERT_TRUE(codec_buffer1->ReleaseToSurface());
  task_environment_.FastForwardBy(base::Milliseconds(2));
  ASSERT_TRUE(codec_buffer2->ReleaseToSurface());

  // Detaching the overlay at teardown (without rendering to the fallback
  // bundle) must not emit a spurious `false` sample.
  ASSERT_TRUE(wrapper_->SetSurface(base::MakeRefCounted<CodecSurfaceBundle>()));

  histogram_tester.ExpectUniqueSample("Media.CodecWrapper.IsOverlayBacked",
                                      true, 1);
  histogram_tester.ExpectUniqueSample(
      "Media.CodecOutputBuffer.SubVsyncRelease.Overlay", true, 1);
  histogram_tester.ExpectUniqueTimeSample(
      "Media.CodecOutputBuffer.ReleaseInterval.Overlay", base::Milliseconds(2),
      1);
}

namespace {

constexpr char kGetOutputFrameTimeHistogram[] =
    "Media.Android.MediaCodec.OutputFrame.GetOutputFrameTime";

using OutputFrameResult =
    base::expected<base::android::ScopedHardwareBufferHandle,
                   OutputFrameStatus>;

// Minimum usage that passes CodecWrapper validation; leaving out
// COMPOSER_OVERLAY lets allocation succeed on more devices.
constexpr uint64_t kDefaultTestAhbUsage =
    AHARDWAREBUFFER_USAGE_GPU_SAMPLED_IMAGE;

// Allocates an R8G8B8A8_UNORM AHB. Returns an invalid handle if allocation
// fails; the test then calls GTEST_SKIP().
base::android::ScopedHardwareBufferHandle CreateTestAhb(
    const gfx::Size& size,
    uint64_t usage = kDefaultTestAhbUsage) {
  if (size.IsEmpty()) {
    return base::android::ScopedHardwareBufferHandle();
  }
  AHardwareBuffer_Desc desc = {};
  desc.width = base::checked_cast<uint32_t>(size.width());
  desc.height = base::checked_cast<uint32_t>(size.height());
  desc.layers = 1;
  desc.format = AHARDWAREBUFFER_FORMAT_R8G8B8A8_UNORM;
  desc.usage = usage;
  AHardwareBuffer* buffer = nullptr;
  if (AHardwareBuffer_allocate(&desc, &buffer) != 0 || !buffer) {
    return base::android::ScopedHardwareBufferHandle();
  }
  return base::android::ScopedHardwareBufferHandle::Adopt(buffer);
}

AHardwareBuffer_Desc MakeValidAhbDesc(uint32_t width = 640,
                                      uint32_t height = 480) {
  AHardwareBuffer_Desc desc = {};
  desc.width = width;
  desc.height = height;
  desc.layers = 1;
  desc.format = AHARDWAREBUFFER_FORMAT_R8G8B8A8_UNORM;
  desc.usage = kDefaultTestAhbUsage;
  return desc;
}

bool CanAllocateAhb(const gfx::Size& size,
                    uint64_t usage = kDefaultTestAhbUsage) {
  return CreateTestAhb(size, usage).is_valid();
}

// Actions for MediaCodecBridge::GetOutputFrame(). Lambdas rather than
// Return(), because the result is move-only.
auto ReturnStatus(OutputFrameStatus status) {
  return
      [status](int) -> OutputFrameResult { return base::unexpected(status); };
}

auto ReturnNewAhb(const gfx::Size& size,
                  uint64_t usage = kDefaultTestAhbUsage) {
  return [size, usage](int) -> OutputFrameResult {
    return CreateTestAhb(size, usage);
  };
}

}  // namespace

TEST(ValidateOutputFrameDescTest, ChecksAllRejectionReasonsAndPrecedence) {
  constexpr gfx::Rect kVisibleRect(10, 20, 64, 48);  // right=74, bottom=68.

  // Valid descriptor without COMPOSER_OVERLAY still passes.
  EXPECT_EQ(CodecWrapper::ValidateOutputFrameDescForTesting(
                MakeValidAhbDesc(74, 68), kVisibleRect),
            OutputFrameStatus::kOk);

  // Empty buffer or empty visible rect.
  EXPECT_EQ(CodecWrapper::ValidateOutputFrameDescForTesting(
                MakeValidAhbDesc(0, 68), kVisibleRect),
            OutputFrameStatus::kInvalidSize);
  EXPECT_EQ(CodecWrapper::ValidateOutputFrameDescForTesting(
                MakeValidAhbDesc(74, 0), kVisibleRect),
            OutputFrameStatus::kInvalidSize);
  EXPECT_EQ(CodecWrapper::ValidateOutputFrameDescForTesting(
                MakeValidAhbDesc(74, 68), gfx::Rect()),
            OutputFrameStatus::kInvalidSize);

  // Dimensions exceeding INT_MAX.
  const uint32_t kOverIntMax =
      static_cast<uint32_t>(std::numeric_limits<int>::max()) + 1u;
  EXPECT_EQ(CodecWrapper::ValidateOutputFrameDescForTesting(
                MakeValidAhbDesc(kOverIntMax, 68), kVisibleRect),
            OutputFrameStatus::kInvalidSize);
  EXPECT_EQ(CodecWrapper::ValidateOutputFrameDescForTesting(
                MakeValidAhbDesc(74, kOverIntMax), kVisibleRect),
            OutputFrameStatus::kInvalidSize);

  // Negative crop origins and undersized buffer boundaries.
  EXPECT_EQ(CodecWrapper::ValidateOutputFrameDescForTesting(
                MakeValidAhbDesc(74, 68), gfx::Rect(-1, 0, 64, 48)),
            OutputFrameStatus::kInvalidSize);
  EXPECT_EQ(CodecWrapper::ValidateOutputFrameDescForTesting(
                MakeValidAhbDesc(74, 68), gfx::Rect(0, -1, 64, 48)),
            OutputFrameStatus::kInvalidSize);
  EXPECT_EQ(CodecWrapper::ValidateOutputFrameDescForTesting(
                MakeValidAhbDesc(73, 68), kVisibleRect),
            OutputFrameStatus::kInvalidSize);
  EXPECT_EQ(CodecWrapper::ValidateOutputFrameDescForTesting(
                MakeValidAhbDesc(74, 67), kVisibleRect),
            OutputFrameStatus::kInvalidSize);

  // Missing GPU_SAMPLED_IMAGE.
  AHardwareBuffer_Desc cpu_only = MakeValidAhbDesc(74, 68);
  cpu_only.usage = AHARDWAREBUFFER_USAGE_CPU_READ_OFTEN;
  EXPECT_EQ(
      CodecWrapper::ValidateOutputFrameDescForTesting(cpu_only, kVisibleRect),
      OutputFrameStatus::kNotGpuSampleable);

  // BLOB buffers have height 1 and no GPU_SAMPLED_IMAGE; BLOB must win over
  // size and usage checks.
  AHardwareBuffer_Desc blob = MakeValidAhbDesc(4096, 1);
  blob.format = AHARDWAREBUFFER_FORMAT_BLOB;
  blob.usage = AHARDWAREBUFFER_USAGE_CPU_READ_OFTEN;
  EXPECT_EQ(CodecWrapper::ValidateOutputFrameDescForTesting(blob, kVisibleRect),
            OutputFrameStatus::kUnsupportedFormat);

  // Protected content takes precedence over BLOB, size, and missing GPU usage.
  AHardwareBuffer_Desc protected_blob = MakeValidAhbDesc(1, 1);
  protected_blob.format = AHARDWAREBUFFER_FORMAT_BLOB;
  protected_blob.usage = AHARDWAREBUFFER_USAGE_PROTECTED_CONTENT;
  EXPECT_EQ(CodecWrapper::ValidateOutputFrameDescForTesting(protected_blob,
                                                            kVisibleRect),
            OutputFrameStatus::kProtectedBuffer);
}

class CodecWrapperOutputFrameTest : public CodecWrapperTest {
 public:
  CodecWrapperOutputFrameTest() {
    // Each dequeue returns a new buffer with the next index, or a format
    // change if one is pending.
    ON_CALL(*codec_, DequeueOutputBuffer(_, _, _, _, _, _, _))
        .WillByDefault([this](base::TimeDelta, int* index, size_t* offset,
                              size_t* size, base::TimeDelta*, bool*,
                              bool*) -> MediaCodecResult {
          if (format_change_pending_) {
            format_change_pending_ = false;
            return MediaCodecResult::Codes::kOutputFormatChanged;
          }
          *index = next_output_index_++;
          *offset = 0;
          *size = next_output_size_;
          return OkStatus();
        });
    ON_CALL(*codec_, GetOutputSizeAndCropRect(_, _))
        .WillByDefault(DoAll(SetArgReferee<0>(kInitialCodedSize),
                             SetArgReferee<1>(gfx::Rect(kInitialCodedSize)),
                             Return(OkStatus())));
  }

  // Replaces |wrapper_| with one that uses |mode| and the same codec.
  void CreateWrapper(OutputFrameMode mode) {
    CodecSurfacePair pair = wrapper_->TakeCodecSurfacePair();
    wrapper_ = std::make_unique<CodecWrapper>(
        std::move(pair), output_buffer_release_cb_.Get(), kInitialCodedSize,
        kCodedSizeAlignment, mode);
  }

  // Makes every GetOutputFrame() call return a new AHB.
  void ReturnOutputFrames(const gfx::Size& size,
                          uint64_t usage = kDefaultTestAhbUsage) {
    ON_CALL(*codec_, GetOutputFrame(_))
        .WillByDefault(ReturnNewAhb(size, usage));
  }

  std::unique_ptr<CodecOutputBuffer> DequeueBufferOfSize(size_t size) {
    next_output_size_ = size;
    std::unique_ptr<CodecOutputBuffer> buffer = DequeueCodecOutputBuffer();
    next_output_size_ = 1;
    return buffer;
  }

  // Dequeues 3 probed OK buffers matching `expected_size`, then verifies the
  // 4th buffer is unprobed.
  void ExpectProbeWindow(const gfx::Size& expected_size,
                         bool expect_first_of_format) {
    for (int i = 0; i < 3; ++i) {
      SCOPED_TRACE(i);
      std::unique_ptr<CodecOutputBuffer> buffer = DequeueCodecOutputBuffer();
      ASSERT_TRUE(buffer);
      EXPECT_EQ(buffer->output_frame_status(), OutputFrameStatus::kOk);
      ASSERT_TRUE(buffer->output_frame_desc().has_value());
      EXPECT_EQ(buffer->output_frame_desc()->width,
                static_cast<uint32_t>(expected_size.width()));
      EXPECT_EQ(buffer->output_frame_desc()->height,
                static_cast<uint32_t>(expected_size.height()));
      EXPECT_EQ(buffer->is_first_output_frame_of_format(),
                expect_first_of_format && i == 0);
    }

    std::unique_ptr<CodecOutputBuffer> fourth = DequeueCodecOutputBuffer();
    ASSERT_TRUE(fourth);
    EXPECT_FALSE(fourth->output_frame_status().has_value());
    EXPECT_FALSE(fourth->output_frame_desc().has_value());
    EXPECT_FALSE(fourth->is_first_output_frame_of_format());
  }

 protected:
  // Start away from 0 so MediaCodec indices differ from CodecWrapper buffer
  // IDs.
  int next_output_index_ = 10;
  size_t next_output_size_ = 1;
  bool format_change_pending_ = false;
};

TEST_F(CodecWrapperOutputFrameTest, ModeOffNeverCallsGetOutputFrame) {
  base::HistogramTester histograms;
  EXPECT_CALL(*codec_, GetOutputFrame(_)).Times(0);

  std::unique_ptr<CodecOutputBuffer> buffer = DequeueCodecOutputBuffer();
  ASSERT_TRUE(buffer);
  EXPECT_FALSE(buffer->output_frame_status().has_value());
  EXPECT_FALSE(buffer->output_frame_desc().has_value());
  EXPECT_FALSE(buffer->is_first_output_frame_of_format());
  histograms.ExpectTotalCount(kGetOutputFrameTimeHistogram, 0);
}

TEST_F(CodecWrapperOutputFrameTest, ProbesFirstBuffersOfEachFormat) {
  constexpr gfx::Size kNewSize(320, 240);
  if (!CanAllocateAhb(kInitialCodedSize) || !CanAllocateAhb(kNewSize)) {
    GTEST_SKIP() << "AHardwareBuffer allocation is not supported";
  }
  CreateWrapper(OutputFrameMode::kProbe);
  ReturnOutputFrames(kInitialCodedSize);
  EXPECT_CALL(*codec_, GetOutputFrame(_)).Times(6);

  ExpectProbeWindow(kInitialCodedSize, /*expect_first_of_format=*/true);

  // A real format change resets the probe budget and first-of-format tracking.
  format_change_pending_ = true;
  EXPECT_CALL(*codec_, GetOutputSizeAndCropRect(_, _))
      .WillOnce(DoAll(SetArgReferee<0>(kNewSize),
                      SetArgReferee<1>(gfx::Rect(kNewSize)),
                      Return(OkStatus())));
  ReturnOutputFrames(kNewSize);
  ExpectProbeWindow(kNewSize, /*expect_first_of_format=*/true);
}

TEST_F(CodecWrapperOutputFrameTest,
       EmptySizeFormatChangeBlipDoesNotResetProbe) {
  if (!CanAllocateAhb(kInitialCodedSize)) {
    GTEST_SKIP() << "AHardwareBuffer allocation is not supported";
  }
  CreateWrapper(OutputFrameMode::kProbe);
  ReturnOutputFrames(kInitialCodedSize);
  EXPECT_CALL(*codec_, GetOutputFrame(_)).Times(3);

  // First buffer consumes one probe and clears first-of-format tracking.
  std::unique_ptr<CodecOutputBuffer> first = DequeueCodecOutputBuffer();
  ASSERT_TRUE(first);
  EXPECT_EQ(first->output_frame_status(), OutputFrameStatus::kOk);
  EXPECT_TRUE(first->is_first_output_frame_of_format());

  // Transient 0x0 format change blip (crbug.com/1207682) mid-window must
  // neither refill the probe budget nor re-arm first-of-format tracking.
  format_change_pending_ = true;
  EXPECT_CALL(*codec_, GetOutputSizeAndCropRect(_, _))
      .WillOnce(DoAll(SetArgReferee<0>(gfx::Size()),
                      SetArgReferee<1>(gfx::Rect()), Return(OkStatus())));
  for (int i = 0; i < 2; ++i) {
    std::unique_ptr<CodecOutputBuffer> buffer = DequeueCodecOutputBuffer();
    ASSERT_TRUE(buffer);
    EXPECT_EQ(buffer->output_frame_status(), OutputFrameStatus::kOk);
    EXPECT_FALSE(buffer->is_first_output_frame_of_format());
  }

  // Budget is spent after 3 total probes (not refilled by the blip).
  std::unique_ptr<CodecOutputBuffer> fourth = DequeueCodecOutputBuffer();
  ASSERT_TRUE(fourth);
  EXPECT_FALSE(fourth->output_frame_status().has_value());

  // A 0x0 blip once the budget is spent also does not restart probing.
  format_change_pending_ = true;
  EXPECT_CALL(*codec_, GetOutputSizeAndCropRect(_, _))
      .WillOnce(DoAll(SetArgReferee<0>(gfx::Size()),
                      SetArgReferee<1>(gfx::Rect()), Return(OkStatus())));
  std::unique_ptr<CodecOutputBuffer> after_second_blip =
      DequeueCodecOutputBuffer();
  ASSERT_TRUE(after_second_blip);
  EXPECT_FALSE(after_second_blip->output_frame_status().has_value());
}

TEST_F(CodecWrapperOutputFrameTest,
       FlushRefillsProbeBudgetWithoutResettingFormat) {
  if (!CanAllocateAhb(kInitialCodedSize)) {
    GTEST_SKIP() << "AHardwareBuffer allocation is not supported";
  }
  CreateWrapper(OutputFrameMode::kProbe);
  ReturnOutputFrames(kInitialCodedSize);
  EXPECT_CALL(*codec_, GetOutputFrame(_)).Times(6);

  ExpectProbeWindow(kInitialCodedSize, /*expect_first_of_format=*/true);

  // Flush() refills the probe budget without re-marking the format as new.
  ASSERT_TRUE(wrapper_->Flush());
  ExpectProbeWindow(kInitialCodedSize, /*expect_first_of_format=*/false);
}

TEST_F(CodecWrapperOutputFrameTest, EmptyAndEosBuffersAreNotProbed) {
  CreateWrapper(OutputFrameMode::kProbe);
  EXPECT_CALL(*codec_, GetOutputFrame(_)).Times(0);

  std::unique_ptr<CodecOutputBuffer> empty_buffer = DequeueBufferOfSize(0);
  ASSERT_TRUE(empty_buffer);
  EXPECT_FALSE(empty_buffer->output_frame_status().has_value());
  EXPECT_FALSE(empty_buffer->output_frame_desc().has_value());

  codec_->ProduceOneOutput(MockMediaCodecBridge::kEos);
  bool eos = false;
  std::unique_ptr<CodecOutputBuffer> eos_buffer;
  ASSERT_TRUE(
      wrapper_->DequeueOutputBuffer(nullptr, &eos, &eos_buffer).is_ok());
  EXPECT_TRUE(eos);
  EXPECT_FALSE(eos_buffer);

  // Neither the empty buffer nor EOS spent any of the 3 probes; failed calls
  // still consume probe budget without setting a desc or first-of-format.
  testing::Mock::VerifyAndClearExpectations(codec_);
  EXPECT_CALL(*codec_, GetOutputFrame(_))
      .Times(3)
      .WillRepeatedly(ReturnStatus(OutputFrameStatus::kNoHardwareBuffer));
  for (int i = 0; i < 3; ++i) {
    std::unique_ptr<CodecOutputBuffer> buffer = DequeueCodecOutputBuffer();
    ASSERT_TRUE(buffer);
    EXPECT_EQ(buffer->output_frame_status(),
              OutputFrameStatus::kNoHardwareBuffer);
    EXPECT_FALSE(buffer->output_frame_desc().has_value());
    EXPECT_FALSE(buffer->is_first_output_frame_of_format());
  }
  std::unique_ptr<CodecOutputBuffer> fourth = DequeueCodecOutputBuffer();
  ASSERT_TRUE(fourth);
  EXPECT_FALSE(fourth->output_frame_status().has_value());
}

TEST_F(CodecWrapperOutputFrameTest, CapsTotalProbesPerCodecInstance) {
  CreateWrapper(OutputFrameMode::kProbe);
  EXPECT_CALL(*codec_, GetOutputFrame(_))
      .Times(15)
      .WillRepeatedly(ReturnStatus(OutputFrameStatus::kNoHardwareBuffer));

  // Consume all 15 lifetime probes across 5 windows (initial + 4 flushes).
  for (int window = 0; window < 5; ++window) {
    if (window > 0) {
      ASSERT_TRUE(wrapper_->Flush());
    }
    for (int i = 0; i < 3; ++i) {
      std::unique_ptr<CodecOutputBuffer> buffer = DequeueCodecOutputBuffer();
      ASSERT_TRUE(buffer);
      EXPECT_EQ(buffer->output_frame_status(),
                OutputFrameStatus::kNoHardwareBuffer);
    }
  }

  // Once the per-codec cap is reached, neither Flush() nor a non-empty format
  // change probes again.
  ASSERT_TRUE(wrapper_->Flush());
  std::unique_ptr<CodecOutputBuffer> after_flush = DequeueCodecOutputBuffer();
  ASSERT_TRUE(after_flush);
  EXPECT_FALSE(after_flush->output_frame_status().has_value());
  EXPECT_FALSE(after_flush->is_first_output_frame_of_format());

  format_change_pending_ = true;
  std::unique_ptr<CodecOutputBuffer> after_format_change =
      DequeueCodecOutputBuffer();
  ASSERT_TRUE(after_format_change);
  EXPECT_FALSE(after_format_change->output_frame_status().has_value());
  EXPECT_FALSE(after_format_change->is_first_output_frame_of_format());
}

TEST_F(CodecWrapperOutputFrameTest,
       FirstOutputFrameOfFormatWaitsForFirstBufferWithDesc) {
  if (!CanAllocateAhb(kInitialCodedSize)) {
    GTEST_SKIP() << "AHardwareBuffer allocation is not supported";
  }
  CreateWrapper(OutputFrameMode::kProbe);
  EXPECT_CALL(*codec_, GetOutputFrame(_))
      .WillOnce(ReturnStatus(OutputFrameStatus::kNoHardwareBuffer))
      .WillOnce(ReturnNewAhb(kInitialCodedSize))
      .WillOnce(ReturnNewAhb(kInitialCodedSize));

  std::unique_ptr<CodecOutputBuffer> first = DequeueCodecOutputBuffer();
  ASSERT_TRUE(first);
  EXPECT_EQ(first->output_frame_status(), OutputFrameStatus::kNoHardwareBuffer);
  EXPECT_FALSE(first->output_frame_desc().has_value());
  EXPECT_FALSE(first->is_first_output_frame_of_format());

  std::unique_ptr<CodecOutputBuffer> second = DequeueCodecOutputBuffer();
  ASSERT_TRUE(second);
  EXPECT_EQ(second->output_frame_status(), OutputFrameStatus::kOk);
  EXPECT_TRUE(second->output_frame_desc().has_value());
  EXPECT_TRUE(second->is_first_output_frame_of_format());

  std::unique_ptr<CodecOutputBuffer> third = DequeueCodecOutputBuffer();
  ASSERT_TRUE(third);
  EXPECT_EQ(third->output_frame_status(), OutputFrameStatus::kOk);
  EXPECT_TRUE(third->output_frame_desc().has_value());
  EXPECT_FALSE(third->is_first_output_frame_of_format());
}

TEST_F(CodecWrapperOutputFrameTest, NotGpuSampleableBufferIsRejected) {
  constexpr uint64_t kCpuOnlyUsage = AHARDWAREBUFFER_USAGE_CPU_READ_OFTEN;
  if (!CanAllocateAhb(kInitialCodedSize, kCpuOnlyUsage)) {
    GTEST_SKIP() << "CPU-only AHardwareBuffer allocation is not supported";
  }
  CreateWrapper(OutputFrameMode::kProbe);
  ReturnOutputFrames(kInitialCodedSize, kCpuOnlyUsage);

  std::unique_ptr<CodecOutputBuffer> first = DequeueCodecOutputBuffer();
  ASSERT_TRUE(first);
  EXPECT_EQ(first->output_frame_status(), OutputFrameStatus::kNotGpuSampleable);
  ASSERT_TRUE(first->output_frame_desc().has_value());
  EXPECT_EQ(first->output_frame_desc()->width,
            static_cast<uint32_t>(kInitialCodedSize.width()));
  EXPECT_TRUE(first->is_first_output_frame_of_format());

  // A rejected buffer with a desc still counts as the first of its format.
  std::unique_ptr<CodecOutputBuffer> second = DequeueCodecOutputBuffer();
  ASSERT_TRUE(second);
  EXPECT_EQ(second->output_frame_status(),
            OutputFrameStatus::kNotGpuSampleable);
  EXPECT_FALSE(second->is_first_output_frame_of_format());
}

TEST_F(CodecWrapperOutputFrameTest, BufferMustCoverVisibleRectAndOffset) {
  constexpr gfx::Rect kVisibleRect(10, 20, 64, 48);
  constexpr gfx::Size kFormatOutputSize(80, 80);
  // Visible rect right=74, bottom=68.
  constexpr gfx::Size kTooNarrow(kVisibleRect.right() - 1,
                                 kVisibleRect.bottom());
  constexpr gfx::Size kTooShort(kVisibleRect.right(),
                                kVisibleRect.bottom() - 1);
  constexpr gfx::Size kExactFit(kVisibleRect.right(), kVisibleRect.bottom());
  if (!CanAllocateAhb(kTooNarrow) || !CanAllocateAhb(kTooShort) ||
      !CanAllocateAhb(kExactFit)) {
    GTEST_SKIP() << "AHardwareBuffer allocation is not supported";
  }
  CreateWrapper(OutputFrameMode::kProbe);
  format_change_pending_ = true;
  EXPECT_CALL(*codec_, GetOutputSizeAndCropRect(_, _))
      .WillOnce(DoAll(SetArgReferee<0>(kFormatOutputSize),
                      SetArgReferee<1>(kVisibleRect), Return(OkStatus())));
  EXPECT_CALL(*codec_, GetOutputFrame(_))
      .WillOnce(ReturnNewAhb(kTooNarrow))
      .WillOnce(ReturnNewAhb(kTooShort))
      .WillOnce(ReturnNewAhb(kExactFit));

  std::unique_ptr<CodecOutputBuffer> narrow = DequeueCodecOutputBuffer();
  ASSERT_TRUE(narrow);
  EXPECT_EQ(narrow->output_frame_status(), OutputFrameStatus::kInvalidSize);
  EXPECT_TRUE(narrow->output_frame_desc().has_value());

  std::unique_ptr<CodecOutputBuffer> short_buf = DequeueCodecOutputBuffer();
  ASSERT_TRUE(short_buf);
  EXPECT_EQ(short_buf->output_frame_status(), OutputFrameStatus::kInvalidSize);
  EXPECT_TRUE(short_buf->output_frame_desc().has_value());

  std::unique_ptr<CodecOutputBuffer> exact = DequeueCodecOutputBuffer();
  ASSERT_TRUE(exact);
  EXPECT_EQ(exact->output_frame_status(), OutputFrameStatus::kOk);
  EXPECT_TRUE(exact->output_frame_desc().has_value());
}

TEST_F(CodecWrapperOutputFrameTest, OkWithoutBufferCountsAsJniFailure) {
  CreateWrapper(OutputFrameMode::kProbe);
  // Verify MediaCodec's index (10), not CodecWrapper's buffer ID (0), is
  // forwarded to GetOutputFrame().
  EXPECT_CALL(*codec_, GetOutputFrame(10))
      .WillOnce([](int) -> OutputFrameResult {
        return base::ok(base::android::ScopedHardwareBufferHandle());
      });

  std::unique_ptr<CodecOutputBuffer> buffer = DequeueCodecOutputBuffer();
  ASSERT_TRUE(buffer);
  EXPECT_EQ(buffer->output_frame_status(), OutputFrameStatus::kJniFailure);
  EXPECT_FALSE(buffer->output_frame_desc().has_value());
  EXPECT_FALSE(buffer->is_first_output_frame_of_format());
}

TEST_F(CodecWrapperOutputFrameTest, ProbedBufferIsReleasedAsUsual) {
  if (!CanAllocateAhb(kInitialCodedSize)) {
    GTEST_SKIP() << "AHardwareBuffer allocation is not supported";
  }
  CreateWrapper(OutputFrameMode::kProbe);
  ReturnOutputFrames(kInitialCodedSize);

  testing::InSequence seq;
  EXPECT_CALL(*codec_, ReleaseOutputBuffer(10, true));
  EXPECT_CALL(*codec_, ReleaseOutputBuffer(11, false));

  std::unique_ptr<CodecOutputBuffer> rendered = DequeueCodecOutputBuffer();
  ASSERT_TRUE(rendered);
  EXPECT_EQ(rendered->output_frame_status(), OutputFrameStatus::kOk);
  EXPECT_TRUE(rendered->ReleaseToSurface());
  rendered.reset();

  std::unique_ptr<CodecOutputBuffer> dropped = DequeueCodecOutputBuffer();
  ASSERT_TRUE(dropped);
  EXPECT_EQ(dropped->output_frame_status(), OutputFrameStatus::kOk);
  dropped.reset();
}

TEST_F(CodecWrapperOutputFrameTest, RecordsGetOutputFrameTimePerCall) {
  constexpr base::TimeDelta kCallDuration = base::Microseconds(250);
  CreateWrapper(OutputFrameMode::kProbe);
  base::HistogramTester histograms;
  EXPECT_CALL(*codec_, GetOutputFrame(_))
      .Times(3)
      .WillRepeatedly([this, kCallDuration](int) -> OutputFrameResult {
        task_environment_.AdvanceClock(kCallDuration);
        return base::unexpected(OutputFrameStatus::kNoHardwareBuffer);
      });

  for (int i = 0; i < 4; ++i) {
    ASSERT_TRUE(DequeueCodecOutputBuffer());
  }
  histograms.ExpectUniqueSample(kGetOutputFrameTimeHistogram,
                                kCallDuration.InMicroseconds(), 3);
}

}  // namespace media
