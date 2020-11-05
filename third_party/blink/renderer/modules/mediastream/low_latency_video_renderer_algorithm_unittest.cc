// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <queue>

#include "media/base/video_frame_pool.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/modules/mediastream/low_latency_video_renderer_algorithm.h"

namespace blink {

class LowLatencyVideoRendererAlgorithmTest : public testing::Test {
 public:
  LowLatencyVideoRendererAlgorithmTest()
      : algorithm_(nullptr),
        current_render_time_(base::TimeTicks() + base::TimeDelta::FromDays(1)) {
  }

  ~LowLatencyVideoRendererAlgorithmTest() override = default;

  scoped_refptr<media::VideoFrame> CreateFrame(
      size_t maximum_composition_delay_in_frames) {
    const gfx::Size natural_size(8, 8);
    scoped_refptr<media::VideoFrame> frame = frame_pool_.CreateFrame(
        media::PIXEL_FORMAT_I420, natural_size, gfx::Rect(natural_size),
        natural_size, base::TimeDelta());
    frame->metadata()->maximum_composition_delay_in_frames =
        maximum_composition_delay_in_frames;
    return frame;
  }

  size_t frames_queued() const { return algorithm_.frames_queued(); }

  scoped_refptr<media::VideoFrame> RenderAndStep(size_t* frames_dropped) {
    constexpr base::TimeDelta kRenderInterval =
        base::TimeDelta::FromMillisecondsD(1000.0 / 60.0);  // 60fps.
    const base::TimeTicks start = current_render_time_;
    current_render_time_ += kRenderInterval;
    const base::TimeTicks end = current_render_time_;
    return algorithm_.Render(start, end, frames_dropped);
  }

 protected:
  media::VideoFramePool frame_pool_;
  LowLatencyVideoRendererAlgorithm algorithm_;
  base::TimeTicks current_render_time_;

 private:
  DISALLOW_COPY_AND_ASSIGN(LowLatencyVideoRendererAlgorithmTest);
};

TEST_F(LowLatencyVideoRendererAlgorithmTest, Empty) {
  size_t frames_dropped = 0;
  EXPECT_EQ(0u, frames_queued());
  EXPECT_FALSE(RenderAndStep(&frames_dropped));
  EXPECT_EQ(0u, frames_dropped);
  EXPECT_EQ(0u, frames_queued());
}

TEST_F(LowLatencyVideoRendererAlgorithmTest, NormalMode) {
  // Every frame rendered.
  constexpr int kNumberOfFrames = 100;
  constexpr int kMaxCompositionDelayInFrames = 6;
  for (int i = 0; i < kNumberOfFrames; ++i) {
    scoped_refptr<media::VideoFrame> frame =
        CreateFrame(kMaxCompositionDelayInFrames);
    int frame_id = frame->unique_id();
    algorithm_.EnqueueFrame(std::move(frame));
    size_t frames_dropped = 0u;
    scoped_refptr<media::VideoFrame> rendered_frame =
        RenderAndStep(&frames_dropped);
    ASSERT_TRUE(rendered_frame);
    EXPECT_EQ(rendered_frame->unique_id(), frame_id);
    EXPECT_EQ(frames_dropped, 0u);
  }
}

TEST_F(LowLatencyVideoRendererAlgorithmTest, EnterDrainMode) {
  // Enter drain mode when more than 6 frames are in the queue.
  constexpr int kMaxCompositionDelayInFrames = 6;
  constexpr int kNumberOfFramesSubmitted = kMaxCompositionDelayInFrames + 1;
  std::queue<int> enqueued_frame_ids;
  for (int i = 0; i < kNumberOfFramesSubmitted; ++i) {
    scoped_refptr<media::VideoFrame> frame =
        CreateFrame(kMaxCompositionDelayInFrames);
    enqueued_frame_ids.push(frame->unique_id());
    algorithm_.EnqueueFrame(std::move(frame));
  }
  // Every other frame will be rendered until there's one frame in the queue.
  int processed_frames_count = 0;
  while (processed_frames_count < kNumberOfFramesSubmitted - 1) {
    size_t frames_dropped = 0;
    scoped_refptr<media::VideoFrame> rendered_frame =
        RenderAndStep(&frames_dropped);
    ASSERT_TRUE(rendered_frame);
    EXPECT_EQ(frames_dropped, 1u);
    enqueued_frame_ids.pop();
    EXPECT_EQ(rendered_frame->unique_id(), enqueued_frame_ids.front());
    enqueued_frame_ids.pop();
    processed_frames_count += 1 + frames_dropped;
  }

  // One more frame to render.
  size_t frames_dropped = 0;
  scoped_refptr<media::VideoFrame> rendered_frame =
      RenderAndStep(&frames_dropped);
  ASSERT_TRUE(rendered_frame);
  EXPECT_EQ(frames_dropped, 0u);
  EXPECT_EQ(rendered_frame->unique_id(), enqueued_frame_ids.front());
  enqueued_frame_ids.pop();
  EXPECT_EQ(enqueued_frame_ids.size(), 0u);
}

TEST_F(LowLatencyVideoRendererAlgorithmTest, ExitDrainMode) {
  // Enter drain mode when more than 6 frames are in the queue.
  constexpr int kMaxCompositionDelayInFrames = 6;
  int number_of_frames_submitted = kMaxCompositionDelayInFrames + 1;
  std::queue<int> enqueued_frame_ids;
  for (int i = 0; i < number_of_frames_submitted; ++i) {
    scoped_refptr<media::VideoFrame> frame =
        CreateFrame(kMaxCompositionDelayInFrames);
    enqueued_frame_ids.push(frame->unique_id());
    algorithm_.EnqueueFrame(std::move(frame));
  }

  // Every other frame will be rendered until there's one frame in the queue.
  int processed_frames_count = 0;
  while (processed_frames_count < number_of_frames_submitted - 1) {
    size_t frames_dropped = 0;
    scoped_refptr<media::VideoFrame> rendered_frame =
        RenderAndStep(&frames_dropped);
    ASSERT_TRUE(rendered_frame);
    EXPECT_EQ(frames_dropped, 1u);
    enqueued_frame_ids.pop();
    EXPECT_EQ(rendered_frame->unique_id(), enqueued_frame_ids.front());
    enqueued_frame_ids.pop();
    // Enqueue a new frame.
    scoped_refptr<media::VideoFrame> frame =
        CreateFrame(kMaxCompositionDelayInFrames);
    enqueued_frame_ids.push(frame->unique_id());
    algorithm_.EnqueueFrame(std::move(frame));
    ++number_of_frames_submitted;
    processed_frames_count += 1 + frames_dropped;
  }

  // Continue in normal mode without dropping frames.
  constexpr int kNumberOfFramesInNormalMode = 30;
  for (int i = 0; i < kNumberOfFramesInNormalMode; ++i) {
    size_t frames_dropped = 0;
    scoped_refptr<media::VideoFrame> rendered_frame =
        RenderAndStep(&frames_dropped);
    ASSERT_TRUE(rendered_frame);
    EXPECT_EQ(frames_dropped, 0u);
    EXPECT_EQ(rendered_frame->unique_id(), enqueued_frame_ids.front());
    enqueued_frame_ids.pop();
    scoped_refptr<media::VideoFrame> frame =
        CreateFrame(kMaxCompositionDelayInFrames);
    enqueued_frame_ids.push(frame->unique_id());
    algorithm_.EnqueueFrame(std::move(frame));
  }
}

TEST_F(LowLatencyVideoRendererAlgorithmTest, SteadyStateQueueReduction) {
  // Create an initial queue of 8 frames.
  constexpr int kMaxCompositionDelayInFrames = 10;
  constexpr size_t kInitialQueueSize = 8;
  std::queue<int> enqueued_frame_ids;
  for (size_t i = 0; i < kInitialQueueSize; ++i) {
    scoped_refptr<media::VideoFrame> frame =
        CreateFrame(kMaxCompositionDelayInFrames);
    enqueued_frame_ids.push(frame->unique_id());
    algorithm_.EnqueueFrame(std::move(frame));
  }
  EXPECT_EQ(frames_queued(), kInitialQueueSize);

  constexpr size_t kNumberOfFramesSubmitted = 100;
  constexpr int kMinimumNumberOfFramesBetweenDrops = 8;
  int processed_frames_since_last_frame_drop = 0;
  for (size_t i = kInitialQueueSize; i < kNumberOfFramesSubmitted; ++i) {
    // Every frame will be rendered with occasional frame drops to reduce the
    // steady state queue.
    size_t frames_dropped = 0;
    scoped_refptr<media::VideoFrame> rendered_frame =
        RenderAndStep(&frames_dropped);

    ASSERT_TRUE(rendered_frame);
    if (frames_dropped > 0) {
      ASSERT_EQ(frames_dropped, 1u);
      EXPECT_GE(processed_frames_since_last_frame_drop,
                kMinimumNumberOfFramesBetweenDrops);
      enqueued_frame_ids.pop();
      processed_frames_since_last_frame_drop = 0;
    } else {
      ++processed_frames_since_last_frame_drop;
    }

    EXPECT_EQ(rendered_frame->unique_id(), enqueued_frame_ids.front());
    enqueued_frame_ids.pop();
    // Enqueue a new frame.
    scoped_refptr<media::VideoFrame> frame =
        CreateFrame(kMaxCompositionDelayInFrames);
    enqueued_frame_ids.push(frame->unique_id());
    algorithm_.EnqueueFrame(std::move(frame));
  }

  // Steady state queue should now have been reduced to one frame + the current
  // frame that is also counted.
  EXPECT_EQ(frames_queued(), 2u);
}

TEST_F(LowLatencyVideoRendererAlgorithmTest,
       DropAllFramesIfQueueExceedsMaxSize) {
  // Create an initial queue of 60 frames.
  constexpr int kMaxCompositionDelayInFrames = 10;
  constexpr size_t kInitialQueueSize = 60;
  int last_id = 0;
  for (size_t i = 0; i < kInitialQueueSize; ++i) {
    scoped_refptr<media::VideoFrame> frame =
        CreateFrame(kMaxCompositionDelayInFrames);
    last_id = frame->unique_id();
    algorithm_.EnqueueFrame(std::move(frame));
  }
  EXPECT_EQ(frames_queued(), kInitialQueueSize);

  // Last submitted frame should be rendered.
  size_t frames_dropped = 0;
  scoped_refptr<media::VideoFrame> rendered_frame =
      RenderAndStep(&frames_dropped);
  ASSERT_TRUE(rendered_frame);
  EXPECT_EQ(frames_dropped, kInitialQueueSize - 1);
  EXPECT_EQ(rendered_frame->unique_id(), last_id);
}

}  // namespace blink
