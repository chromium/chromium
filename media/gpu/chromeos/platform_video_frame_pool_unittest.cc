// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/chromeos/platform_video_frame_pool.h"

#include <stddef.h>
#include <stdint.h>
#include <memory>
#include <vector>

#include "base/functional/callback_helpers.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/task_environment.h"
#include "gpu/command_buffer/common/mailbox_holder.h"
#include "media/base/format_utils.h"
#include "media/base/video_types.h"
#include "media/base/video_util.h"
#include "media/gpu/chromeos/fourcc.h"
#include "media/gpu/chromeos/video_frame_resource.h"
#include "media/video/fake_gpu_memory_buffer.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

namespace {

template <uint64_t modifier>
CroStatus::Or<scoped_refptr<FrameResource>> CreateGpuMemoryBufferFrameResource(
    VideoPixelFormat format,
    const gfx::Size& coded_size,
    const gfx::Rect& visible_rect,
    const gfx::Size& natural_size,
    bool use_protected,
    bool use_linear_buffers,
    bool needs_detiling,
    base::TimeDelta timestamp) {
  std::optional<gfx::BufferFormat> gfx_format =
      VideoPixelFormatToGfxBufferFormat(format);
  DCHECK(gfx_format);
  return static_cast<scoped_refptr<FrameResource>>(
      VideoFrameResource::Create(VideoFrame::WrapExternalGpuMemoryBuffer(
          visible_rect, natural_size,
          std::make_unique<FakeGpuMemoryBuffer>(coded_size, *gfx_format,
                                                modifier),
          timestamp)));
}

}  // namespace

class PlatformVideoFramePoolTestBase : public ::testing::Test {
 public:
  PlatformVideoFramePoolTestBase()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME),
        pool_(new PlatformVideoFramePool()) {
    pool_->SetCustomFrameAllocator(
        base::BindRepeating(&CreateGpuMemoryBufferFrameResource<
                            gfx::NativePixmapHandle::kNoModifier>),
        VideoFrame::STORAGE_GPU_MEMORY_BUFFER);
    pool_->set_parent_task_runner(
        base::SingleThreadTaskRunner::GetCurrentDefault());
  }

  bool Initialize(const Fourcc& fourcc) {
    constexpr gfx::Size kCodedSize(320, 240);
    return Initialize(kCodedSize, /*visible_rect=*/gfx::Rect(kCodedSize),
                      fourcc);
  }

  bool Initialize(const gfx::Size& coded_size,
                  const gfx::Rect& visible_rect,
                  const Fourcc& fourcc) {
    constexpr size_t kNumFrames = 10;
    visible_rect_ = visible_rect;
    natural_size_ = visible_rect.size();
    auto status_or_layout = pool_->Initialize(fourcc, coded_size, visible_rect_,
                                              natural_size_, kNumFrames,
                                              /*use_protected=*/false,
                                              /*use_linear_buffers=*/false);
    if (!status_or_layout.has_value()) {
      return false;
    }
    layout_ = std::move(status_or_layout).value();
    return true;
  }

  scoped_refptr<FrameResource> GetFrame(int timestamp_ms) {
    scoped_refptr<FrameResource> frame = pool_->GetFrame();
    frame->set_timestamp(base::Milliseconds(timestamp_ms));

    EXPECT_EQ(layout_->modifier(), frame->layout().modifier());
    EXPECT_EQ(layout_->fourcc(),
              *Fourcc::FromVideoPixelFormat(frame->format()));
    EXPECT_EQ(layout_->size(), frame->coded_size());
    EXPECT_EQ(visible_rect_, frame->visible_rect());
    EXPECT_EQ(natural_size_, frame->natural_size());
    // We can't assert any of the |frame| metadata because the frame creation
    // callback is a fake.

    return frame;
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<PlatformVideoFramePool> pool_;

  std::optional<GpuBufferLayout> layout_;
  gfx::Rect visible_rect_;
  gfx::Size natural_size_;
};

class PlatformVideoFramePoolTest
    : public PlatformVideoFramePoolTestBase,
      public testing::WithParamInterface<VideoPixelFormat> {};

constexpr VideoPixelFormat kPixelFormats[] = {
    PIXEL_FORMAT_YV12, PIXEL_FORMAT_NV12, PIXEL_FORMAT_P010LE};
INSTANTIATE_TEST_SUITE_P(
    All,
    PlatformVideoFramePoolTest,
    testing::ValuesIn(kPixelFormats),
    [](const ::testing::TestParamInfo<VideoPixelFormat>& info) {
      return VideoPixelFormatToString(info.param);
    });

TEST_P(PlatformVideoFramePoolTest, SingleFrameReuse) {
  const auto fourcc = Fourcc::FromVideoPixelFormat(GetParam());
  ASSERT_TRUE(fourcc.has_value());
  ASSERT_TRUE(Initialize(fourcc.value()));
  scoped_refptr<FrameResource> frame = GetFrame(10);
  gfx::GenericSharedMemoryId id = frame->GetSharedMemoryId();

  // Clear frame reference to return the frame to the pool.
  frame = nullptr;
  task_environment_.RunUntilIdle();

  // Verify that the next frame from the pool uses the same memory.
  scoped_refptr<FrameResource> new_frame = GetFrame(20);
  EXPECT_EQ(id, new_frame->GetSharedMemoryId());
}

TEST_P(PlatformVideoFramePoolTest, MultipleFrameReuse) {
  const auto fourcc = Fourcc::FromVideoPixelFormat(GetParam());
  ASSERT_TRUE(fourcc.has_value());
  ASSERT_TRUE(Initialize(fourcc.value()));
  scoped_refptr<FrameResource> frame1 = GetFrame(10);
  scoped_refptr<FrameResource> frame2 = GetFrame(20);
  gfx::GenericSharedMemoryId id1 = frame1->GetSharedMemoryId();
  gfx::GenericSharedMemoryId id2 = frame2->GetSharedMemoryId();

  frame1 = nullptr;
  task_environment_.RunUntilIdle();
  frame1 = GetFrame(30);
  EXPECT_EQ(id1, frame1->GetSharedMemoryId());

  frame2 = nullptr;
  task_environment_.RunUntilIdle();
  frame2 = GetFrame(40);
  EXPECT_EQ(id2, frame2->GetSharedMemoryId());

  frame1 = nullptr;
  frame2 = nullptr;
  task_environment_.RunUntilIdle();
  EXPECT_EQ(2u, pool_->GetPoolSizeForTesting());
}

TEST_P(PlatformVideoFramePoolTest, InitializeWithDifferentFourcc) {
  const auto fourcc = Fourcc::FromVideoPixelFormat(GetParam());
  ASSERT_TRUE(fourcc.has_value());
  ASSERT_TRUE(Initialize(fourcc.value()));
  scoped_refptr<FrameResource> frame_a = GetFrame(10);
  scoped_refptr<FrameResource> frame_b = GetFrame(10);

  // Clear frame references to return the frames to the pool.
  frame_a = nullptr;
  frame_b = nullptr;
  task_environment_.RunUntilIdle();

  // Verify that both frames are in the pool.
  EXPECT_EQ(2u, pool_->GetPoolSizeForTesting());

  // Verify that requesting a frame with a different format causes the pool
  // to get drained.
  const Fourcc different_fourcc(*fourcc != Fourcc(Fourcc::NV12) ? Fourcc::NV12
                                                                : Fourcc::P010);
  ASSERT_TRUE(Initialize(different_fourcc));
  scoped_refptr<FrameResource> new_frame = GetFrame(10);
  EXPECT_EQ(0u, pool_->GetPoolSizeForTesting());
}

TEST_P(PlatformVideoFramePoolTest, InitializeWithDifferentUsableArea) {
  const auto fourcc = Fourcc::FromVideoPixelFormat(GetParam());
  ASSERT_TRUE(fourcc.has_value());

  constexpr gfx::Size kCodedSize(640, 368);
  constexpr gfx::Rect kInitialVisibleRect(10, 20, 300, 200);
  ASSERT_TRUE(Initialize(kCodedSize, kInitialVisibleRect, fourcc.value()));
  scoped_refptr<FrameResource> frame_a = GetFrame(10);
  scoped_refptr<FrameResource> frame_b = GetFrame(10);

  // Clear frame references to return the frames to the pool.
  frame_a = nullptr;
  frame_b = nullptr;
  task_environment_.RunUntilIdle();

  // Verify that both frames are in the pool.
  EXPECT_EQ(2u, pool_->GetPoolSizeForTesting());

  // Verify that requesting a frame with a different "usable area" causes the
  // pool to get drained. The usable area is the area of the buffer starting at
  // (0, 0) and extending to the bottom-right corner of the visible rectangle.
  // It corresponds to the area used to import the video frame into a graphics
  // API or to create the DRM framebuffer for hardware overlay purposes.
  constexpr gfx::Rect kDifferentVisibleRect(5, 15, 300, 200);
  ASSERT_EQ(kDifferentVisibleRect.size(), natural_size_);
  ASSERT_NE(GetRectSizeFromOrigin(kInitialVisibleRect),
            GetRectSizeFromOrigin(kDifferentVisibleRect));
  ASSERT_TRUE(Initialize(kCodedSize, kDifferentVisibleRect, fourcc.value()));
  scoped_refptr<FrameResource> new_frame = GetFrame(10);
  EXPECT_EQ(0u, pool_->GetPoolSizeForTesting());
}

TEST_P(PlatformVideoFramePoolTest, InitializeWithDifferentCodedSize) {
  const auto fourcc = Fourcc::FromVideoPixelFormat(GetParam());
  ASSERT_TRUE(fourcc.has_value());

  constexpr gfx::Size kInitialCodedSize(640, 368);
  constexpr gfx::Rect kVisibleRect(10, 20, 300, 200);
  ASSERT_TRUE(Initialize(kInitialCodedSize, kVisibleRect, fourcc.value()));
  scoped_refptr<FrameResource> frame_a = GetFrame(10);
  scoped_refptr<FrameResource> frame_b = GetFrame(10);

  // Clear frame references to return the frames to the pool.
  frame_a = nullptr;
  frame_b = nullptr;
  task_environment_.RunUntilIdle();

  // Verify that both frames are in the pool.
  EXPECT_EQ(2u, pool_->GetPoolSizeForTesting());

  // Verify that requesting a frame with a different coded size causes the pool
  // to get drained.
  constexpr gfx::Size kDifferentCodedSize(624, 368);
  ASSERT_TRUE(Initialize(kDifferentCodedSize, kVisibleRect, fourcc.value()));
  scoped_refptr<FrameResource> new_frame = GetFrame(10);
  EXPECT_EQ(0u, pool_->GetPoolSizeForTesting());
}

TEST_P(PlatformVideoFramePoolTest, GetOriginalFrame) {
  const auto fourcc = Fourcc::FromVideoPixelFormat(GetParam());
  ASSERT_TRUE(fourcc.has_value());
  ASSERT_TRUE(Initialize(fourcc.value()));
  scoped_refptr<FrameResource> frame_1 = GetFrame(10);
  scoped_refptr<FrameResource> frame_2 = frame_1->CreateWrappingFrame();
  EXPECT_EQ(pool_->GetOriginalFrame(frame_1->GetSharedMemoryId()),
            pool_->GetOriginalFrame(frame_2->GetSharedMemoryId()));
  EXPECT_EQ(frame_1->GetSharedMemoryId().id, frame_2->GetSharedMemoryId().id);

  scoped_refptr<FrameResource> frame_3 = GetFrame(20);
  EXPECT_NE(pool_->GetOriginalFrame(frame_1->GetSharedMemoryId()),
            pool_->GetOriginalFrame(frame_3->GetSharedMemoryId()));
  EXPECT_NE(frame_1->GetSharedMemoryId().id, frame_3->GetSharedMemoryId().id);
}

TEST_P(PlatformVideoFramePoolTest,
       InitializeWithSameFourccUsableAreaAndCodedSize) {
  const auto fourcc = Fourcc::FromVideoPixelFormat(GetParam());
  ASSERT_TRUE(fourcc.has_value());

  constexpr gfx::Size kCodedSize(640, 368);
  constexpr gfx::Rect kInitialVisibleRect(kCodedSize);
  ASSERT_TRUE(Initialize(kCodedSize, kInitialVisibleRect, fourcc.value()));
  scoped_refptr<FrameResource> frame1 = GetFrame(10);
  gfx::GenericSharedMemoryId id1 = frame1->GetSharedMemoryId();

  // Clear frame references to return the frames to the pool.
  frame1 = nullptr;
  task_environment_.RunUntilIdle();

  // Request frame with the same format, coded size, and "usable area." To make
  // things interesting, we change the visible rect while keeping the
  // "usable area" constant. The usable area is the area of the buffer starting
  // at (0, 0) and extending to the bottom-right corner of the visible
  // rectangle. It corresponds to the area used to import the video frame into a
  // graphics API or to create the DRM framebuffer for hardware overlay
  // purposes. The pool should not request new frames.
  constexpr gfx::Rect kDifferentVisibleRect(10, 20, 630, 348);
  ASSERT_EQ(GetRectSizeFromOrigin(kInitialVisibleRect),
            GetRectSizeFromOrigin(kDifferentVisibleRect));
  ASSERT_TRUE(Initialize(kCodedSize, kDifferentVisibleRect, fourcc.value()));

  scoped_refptr<FrameResource> frame2 = GetFrame(20);
  gfx::GenericSharedMemoryId id2 = frame2->GetSharedMemoryId();
  EXPECT_EQ(id1, id2);
}

TEST_P(PlatformVideoFramePoolTest, InitializeFail) {
  const auto fourcc = Fourcc::FromVideoPixelFormat(GetParam());
  ASSERT_TRUE(fourcc.has_value());
  pool_->SetCustomFrameAllocator(
      base::BindRepeating([](VideoPixelFormat format,
                             const gfx::Size& coded_size,
                             const gfx::Rect& visible_rect,
                             const gfx::Size& natural_size, bool use_protected,
                             bool use_linear_buffers, bool needs_detiling,
                             base::TimeDelta timestamp) {
        return CroStatus::Or<scoped_refptr<FrameResource>>(
            CroStatus::Codes::kFailedToCreateVideoFrame);
      }),
      VideoFrame::STORAGE_GPU_MEMORY_BUFFER);

  EXPECT_FALSE(Initialize(fourcc.value()));
}

TEST_P(PlatformVideoFramePoolTest, ModifierIsPassed) {
  const uint64_t kSampleModifier = 0x001234567890abcdULL;
  const auto fourcc = Fourcc::FromVideoPixelFormat(GetParam());
  ASSERT_TRUE(fourcc.has_value());
  pool_->SetCustomFrameAllocator(
      base::BindRepeating(&CreateGpuMemoryBufferFrameResource<kSampleModifier>),
      VideoFrame::STORAGE_GPU_MEMORY_BUFFER);
  ASSERT_TRUE(Initialize(fourcc.value()));

  EXPECT_EQ(layout_->modifier(), kSampleModifier);
  EXPECT_TRUE(GetFrame(10));
}

// TODO(akahuang): Add a testcase to verify calling Initialize() only with
// different |max_num_frames|.

}  // namespace media
