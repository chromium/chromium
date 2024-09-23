// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/modules/mediastream/low_latency_video_renderer_algorithm.h"

#include <queue>

#include "base/time/time.h"
#include "media/base/video_frame_pool.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"

namespace blink {

class LowLatencyVideoRendererAlgorithmTest : public testing::Test {
 public:
  LowLatencyVideoRendererAlgorithmTest()
      : algorithm_(nullptr),
        current_render_time_(base::TimeTicks() + base::Days(1)) {}

  LowLatencyVideoRendererAlgorithmTest(
      const LowLatencyVideoRendererAlgorithmTest&) = delete;
  LowLatencyVideoRendererAlgorithmTest& operator=(
      const LowLatencyVideoRendererAlgorithmTest&) = delete;

  ~LowLatencyVideoRendererAlgorithmTest() override = default;

  scoped_refptr<media::VideoFrame> CreateFrame(
      int maximum_composition_delay_in_frames) {
    const gfx::Size natural_size(8, 8);
    scoped_refptr<media::VideoFrame> frame = frame_pool_.CreateFrame(
        media::PIXEL_FORMAT_I420, natural_size, gfx::Rect(natural_size),
        natural_size, base::TimeDelta());
    frame->metadata().maximum_composition_delay_in_frames =
        maximum_composition_delay_in_frames;
    return frame;
  }

  media::VideoFrame::ID CreateAndEnqueueFrame(
      int max_composition_delay_in_frames) {
    scoped_refptr<media::VideoFrame> frame =
        CreateFrame(max_composition_delay_in_frames);
    media::VideoFrame::ID unique_id = frame->unique_id();
    algorithm_.EnqueueFrame(std::move(frame));
    return unique_id;
  }

  size_t frames_queued() const { return algorithm_.frames_queued(); }

  scoped_refptr<media::VideoFrame> RenderAndStep(size_t* frames_dropped) {
    constexpr base::TimeDelta kRenderInterval =
        base::Milliseconds(1000.0 / 60.0);  // 60fps.
    return RenderAndStep(frames_dropped, kRenderInterval);
  }

  scoped_refptr<media::VideoFrame> RenderAndStep(
      size_t* frames_dropped,
      base::TimeDelta render_interval) {
    const base::TimeTicks start = current_render_time_;
    current_render_time_ += render_interval;
    const base::TimeTicks end = current_render_time_;
    return algorithm_.Render(start, end, frames_dropped);
  }

  scoped_refptr<media::VideoFrame> RenderWithGlitchAndStep(
      size_t* frames_dropped,
      double deadline_begin_error,
      double deadline_end_error) {
    constexpr base::TimeDelta kRenderInterval =
        base::Milliseconds(1000.0 / 60.0);  // 60fps.
    return RenderAndStep(frames_dropped, kRenderInterval);
  }

  scoped_refptr<media::VideoFrame> RenderWithGlitchAndStep(
      size_t* frames_dropped,
      base::TimeDelta render_interval,
      double deadline_begin_error,
      double deadline_end_error) {
    const base::TimeTicks start =
        current_render_time_ + deadline_begin_error * render_interval;
    current_render_time_ += render_interval;
    const base::TimeTicks end =
        current_render_time_ + deadline_end_error * render_interval;
    return algorithm_.Render(start, end, frames_dropped);
  }

  void StepUntilJustBeforeNextFrameIsRendered(
      base::TimeDelta render_interval,
      std::optional<media::VideoFrame::ID> expected_id = std::nullopt) {
    // No frame will be rendered until the total render time that has passed is
    // greater than the frame duration of a frame.
    base::TimeTicks start_time = current_render_time_;
    while (current_render_time_ - start_time + render_interval <
           FrameDuration()) {
      scoped_refptr<media::VideoFrame> rendered_frame =
          RenderAndStep(nullptr, render_interval);
      if (expected_id) {
        ASSERT_TRUE(rendered_frame);
        EXPECT_EQ(rendered_frame->unique_id(), *expected_id);
      } else {
        EXPECT_FALSE(rendered_frame);
      }
    }
  }

  base::TimeDelta FrameDuration() const {
    // Assume 60 Hz video content.
    return base::Milliseconds(1000.0 / 60.0);
  }

 protected:
  test::TaskEnvironment task_environment_;
  media::VideoFramePool frame_pool_;
  LowLatencyVideoRendererAlgorithm algorithm_;
  base::TimeTicks current_render_time_;
};

TEST_F(LowLatencyVideoRendererAlgorithmTest, Empty) {
  size_t frames_dropped = 0;
  EXPECT_EQ(0u, frames_queued());
  EXPECT_FALSE(RenderAndStep(&frames_dropped));
  EXPECT_EQ(0u, frames_dropped);
  EXPECT_EQ(0u, frames_queued());
}

TEST_F(LowLatencyVideoRendererAlgorithmTest, NormalMode60Hz) {
  // Every frame rendered.
  constexpr int kNumberOfFrames = 100;
  constexpr int kMaxCompositionDelayInFrames = 6;
  for (int i = 0; i < kNumberOfFrames; ++i) {
    media::VideoFrame::ID frame_id =
        CreateAndEnqueueFrame(kMaxCompositionDelayInFrames);
    size_t frames_dropped = 0u;
    scoped_refptr<media::VideoFrame> rendered_frame =
        RenderAndStep(&frames_dropped);
    ASSERT_TRUE(rendered_frame);
    EXPECT_EQ(rendered_frame->unique_id(), frame_id);
    EXPECT_EQ(frames_dropped, 0u);
  }
}

// Half frame rate (30Hz playing back 60Hz video)
TEST_F(LowLatencyVideoRendererAlgorithmTest, NormalMode30Hz) {
  constexpr base::TimeDelta kRenderInterval =
      base::Milliseconds(1000.0 / 30.0);  // 30Hz.
  constexpr int kMaxCompositionDelayInFrames = 6;

  constexpr size_t kNumberOfFrames = 120;
  for (size_t i = 0; i < kNumberOfFrames; ++i) {
    scoped_refptr<media::VideoFrame> frame;
    size_t expected_frames_dropped = 0;
    if (i > 0) {
      // This frame will be dropped.
      CreateAndEnqueueFrame(kMaxCompositionDelayInFrames);
      ++expected_frames_dropped;
    }

    media::VideoFrame::ID last_id =
        CreateAndEnqueueFrame(kMaxCompositionDelayInFrames);

    size_t frames_dropped = 0;
    scoped_refptr<media::VideoFrame> rendered_frame =
        RenderAndStep(&frames_dropped, kRenderInterval);
    ASSERT_TRUE(rendered_frame);
    EXPECT_EQ(rendered_frame->unique_id(), last_id);
    EXPECT_EQ(frames_dropped, expected_frames_dropped);
  }
  // Only the currently rendered frame is in the queue.
  EXPECT_EQ(frames_queued(), 1u);
}

// Fractional frame rate (90Hz playing back 60Hz video)
TEST_F(LowLatencyVideoRendererAlgorithmTest, NormalMode90Hz) {
  constexpr base::TimeDelta kRenderInterval =
      base::Milliseconds(1000.0 / 90.0);  // 90Hz.
  constexpr int kMaxCompositionDelayInFrames = 6;

  CreateAndEnqueueFrame(kMaxCompositionDelayInFrames);

  constexpr size_t kNumberOfFramesToSubmit = 100;
  size_t submitted_frames = 0;
  while (submitted_frames < kNumberOfFramesToSubmit) {
    // In each while iteration: Enqueue two new frames (60Hz) and render three
    // times (90Hz).
    for (int i = 0; i < 2; ++i) {
      size_t frames_dropped = 0;
      scoped_refptr<media::VideoFrame> rendered_frame =
          RenderAndStep(&frames_dropped, kRenderInterval);
      ASSERT_TRUE(rendered_frame);
      EXPECT_EQ(frames_dropped, 0u);
      // Enqueue a new frame.
      CreateAndEnqueueFrame(kMaxCompositionDelayInFrames);
      ++submitted_frames;
    }
    size_t frames_dropped = 0;
    scoped_refptr<media::VideoFrame> rendered_frame =
        RenderAndStep(&frames_dropped, kRenderInterval);
    ASSERT_TRUE(rendered_frame);
    EXPECT_EQ(frames_dropped, 0u);
  }
}

// Double frame rate (120Hz playing back 60Hz video)
TEST_F(LowLatencyVideoRendererAlgorithmTest, NormalMode120Hz) {
  constexpr base::TimeDelta kRenderInterval =
      base::Milliseconds(1000.0 / 120.0);  // 120Hz.
  constexpr int kMaxCompositionDelayInFrames = 6;

  // Add one initial frame.
  media::VideoFrame::ID last_id =
      CreateAndEnqueueFrame(kMaxCompositionDelayInFrames);

  constexpr size_t kNumberOfFrames = 120;
  for (size_t i = 0; i < kNumberOfFrames; ++i) {
    size_t frames_dropped = 0;
    scoped_refptr<media::VideoFrame> rendered_frame =
        RenderAndStep(&frames_dropped, kRenderInterval);
    ASSERT_TRUE(rendered_frame);
    media::VideoFrame::ID rendered_frame_id = last_id;
    EXPECT_EQ(rendered_frame->unique_id(), rendered_frame_id);

    last_id = CreateAndEnqueueFrame(kMaxCompositionDelayInFrames);

    // The same frame should be rendered.
    rendered_frame = RenderAndStep(&frames_dropped, kRenderInterval);
    ASSERT_TRUE(rendered_frame);
    EXPECT_EQ(rendered_frame->unique_id(), rendered_frame_id);
  }
  // Two frames in the queue including the last rendered frame.
  EXPECT_EQ(frames_queued(), 2u);
}

// Super high display rate (600Hz playing back 60Hz video)
TEST_F(LowLatencyVideoRendererAlgorithmTest, NormalMode600Hz) {
  constexpr base::TimeDelta kRenderInterval =
      base::Milliseconds(1000.0 / 600.0 + 1.0e-3);  // 600Hz.
  constexpr int kMaxCompositionDelayInFrames = 6;

  // Add one initial frame.
  media::VideoFrame::ID last_id =
      CreateAndEnqueueFrame(kMaxCompositionDelayInFrames);

  constexpr size_t kNumberOfFrames = 120;
  for (size_t i = 0; i < kNumberOfFrames; ++i) {
    size_t frames_dropped = 0;
    scoped_refptr<media::VideoFrame> rendered_frame =
        RenderAndStep(&frames_dropped, kRenderInterval);
    ASSERT_TRUE(rendered_frame);
    media::VideoFrame::ID rendered_frame_id = last_id;
    EXPECT_EQ(rendered_frame->unique_id(), rendered_frame_id);

    last_id = CreateAndEnqueueFrame(kMaxCompositionDelayInFrames);

    // The same frame should be rendered 9 times.
    StepUntilJustBeforeNextFrameIsRendered(kRenderInterval, rendered_frame_id);
  }
  // Two frames in the queue including the last rendered frame.
  EXPECT_EQ(frames_queued(), 2u);
}

TEST_F(LowLatencyVideoRendererAlgorithmTest,
       DropAllFramesIfQueueExceedsMaxSize) {
  // Create an initial queue of 60 frames.
  constexpr int kMaxCompositionDelayInFrames = 6;
  constexpr size_t kInitialQueueSize = 60;
  media::VideoFrame::ID last_id;
  for (size_t i = 0; i < kInitialQueueSize; ++i) {
    last_id = CreateAndEnqueueFrame(kMaxCompositionDelayInFrames);
  }
  EXPECT_EQ(frames_queued(), kInitialQueueSize);

  // Last submitted frame should be rendered.
  size_t frames_dropped = 0;
  scoped_refptr<media::VideoFrame> rendered_frame =
      RenderAndStep(&frames_dropped);
  ASSERT_TRUE(rendered_frame);
  EXPECT_EQ(frames_dropped, kInitialQueueSize - 1);
  EXPECT_EQ(rendered_frame->unique_id(), last_id);

  // The following frame should be rendered as normal.
  last_id = CreateAndEnqueueFrame(kMaxCompositionDelayInFrames);
  rendered_frame = RenderAndStep(&frames_dropped);
  ASSERT_TRUE(rendered_frame);
  EXPECT_EQ(frames_dropped, 0u);
  EXPECT_EQ(rendered_frame->unique_id(), last_id);
}

TEST_F(LowLatencyVideoRendererAlgorithmTest, EnterDrainMode60Hz) {
  // Enter drain mode when more than 6 frames are in the queue.
  constexpr int kMaxCompositionDelayInFrames = 6;
  constexpr int kNumberOfFramesSubmitted = kMaxCompositionDelayInFrames + 1;
  std::queue<media::VideoFrame::ID> enqueued_frame_ids;
  for (int i = 0; i < kNumberOfFramesSubmitted; ++i) {
    enqueued_frame_ids.push(
        CreateAndEnqueueFrame(kMaxCompositionDelayInFrames));
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

TEST_F(LowLatencyVideoRendererAlgorithmTest, ExitDrainMode60Hz) {
  // Enter drain mode when more than 6 frames are in the queue.
  constexpr int kMaxCompositionDelayInFrames = 6;
  int number_of_frames_submitted = kMaxCompositionDelayInFrames + 1;
  std::queue<media::VideoFrame::ID> enqueued_frame_ids;
  for (int i = 0; i < number_of_frames_submitted; ++i) {
    enqueued_frame_ids.push(
        CreateAndEnqueueFrame(kMaxCompositionDelayInFrames));
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
    enqueued_frame_ids.push(
        CreateAndEnqueueFrame(kMaxCompositionDelayInFrames));
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
    enqueued_frame_ids.push(
        CreateAndEnqueueFrame(kMaxCompositionDelayInFrames));
  }
}

// Double Rate Drain (120Hz playing back 60Hz video in DRAIN mode)
TEST_F(LowLatencyVideoRendererAlgorithmTest, EnterDrainMode120Hz) {
  constexpr base::TimeDelta kRenderInterval =
      base::Milliseconds(1000.0 / 120.0);  // 120Hz.
  // Enter drain mode when more than 6 frames are in the queue.
  constexpr int kMaxCompositionDelayInFrames = 6;

  // Process one frame to initialize the algorithm.
  CreateAndEnqueueFrame(kMaxCompositionDelayInFrames);
  EXPECT_TRUE(RenderAndStep(nullptr, kRenderInterval));

  constexpr int kNumberOfFramesSubmitted = kMaxCompositionDelayInFrames + 1;
  std::queue<media::VideoFrame::ID> enqueued_frame_ids;
  for (int i = 0; i < kNumberOfFramesSubmitted; ++i) {
    enqueued_frame_ids.push(
        CreateAndEnqueueFrame(kMaxCompositionDelayInFrames));
  }
  // Every frame will be rendered at double rate until there's one frame in the
  // queue.
  int processed_frames_count = 0;
  while (processed_frames_count < kNumberOfFramesSubmitted - 1) {
    size_t frames_dropped = 0;
    scoped_refptr<media::VideoFrame> rendered_frame =
        RenderAndStep(&frames_dropped, kRenderInterval);
    ASSERT_TRUE(rendered_frame);
    EXPECT_EQ(frames_dropped, 0u);
    EXPECT_EQ(rendered_frame->unique_id(), enqueued_frame_ids.front());
    enqueued_frame_ids.pop();
    processed_frames_count += 1 + frames_dropped;
  }

  // One more frame to render.
  size_t frames_dropped = 0;
  scoped_refptr<media::VideoFrame> rendered_frame =
      RenderAndStep(&frames_dropped, kRenderInterval);
  ASSERT_TRUE(rendered_frame);
  EXPECT_EQ(frames_dropped, 0u);
  EXPECT_EQ(rendered_frame->unique_id(), enqueued_frame_ids.front());
  enqueued_frame_ids.pop();
  EXPECT_EQ(enqueued_frame_ids.size(), 0u);
}

TEST_F(LowLatencyVideoRendererAlgorithmTest, SteadyStateQueueReduction60Hz) {
  // Create an initial queue of 5 frames.
  constexpr int kMaxCompositionDelayInFrames = 6;
  constexpr size_t kInitialQueueSize = 5;
  std::queue<media::VideoFrame::ID> enqueued_frame_ids;
  for (size_t i = 0; i < kInitialQueueSize; ++i) {
    enqueued_frame_ids.push(
        CreateAndEnqueueFrame(kMaxCompositionDelayInFrames));
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
    enqueued_frame_ids.push(
        CreateAndEnqueueFrame(kMaxCompositionDelayInFrames));
  }

  // Steady state queue should now have been reduced to one frame + the current
  // frame that is also counted.
  EXPECT_EQ(frames_queued(), 2u);
}

// Fractional rate, steady state queue reduction.
TEST_F(LowLatencyVideoRendererAlgorithmTest, SteadyStateReduction90Hz) {
  constexpr base::TimeDelta kRenderInterval =
      base::Milliseconds(1000.0 / 90.0);  // 90Hz.

  // Create an initial queue of 5 frames.
  constexpr int kMaxCompositionDelayInFrames = 6;
  constexpr size_t kInitialQueueSize = 5;
  for (size_t i = 0; i < kInitialQueueSize; ++i) {
    CreateAndEnqueueFrame(kMaxCompositionDelayInFrames);
  }
  EXPECT_EQ(frames_queued(), kInitialQueueSize);

  constexpr size_t kNumberOfFramesToSubmit = 100;
  constexpr int kMinimumNumberOfFramesBetweenDrops = 8;
  int processed_frames_since_last_frame_drop = 0;
  size_t submitted_frames = kInitialQueueSize;
  while (submitted_frames < kNumberOfFramesToSubmit) {
    // Every frame will be rendered with occasional frame drops to reduce the
    // steady state queue.

    // In each while iteration: Enqueue two new frames (60Hz) and render three
    // times (90Hz).
    for (int i = 0; i < 2; ++i) {
      size_t frames_dropped = 0;
      scoped_refptr<media::VideoFrame> rendered_frame =
          RenderAndStep(&frames_dropped, kRenderInterval);
      ASSERT_TRUE(rendered_frame);
      if (frames_dropped > 0) {
        ASSERT_EQ(frames_dropped, 1u);
        EXPECT_GE(processed_frames_since_last_frame_drop,
                  kMinimumNumberOfFramesBetweenDrops);
        processed_frames_since_last_frame_drop = 0;
      } else {
        ++processed_frames_since_last_frame_drop;
      }
      CreateAndEnqueueFrame(kMaxCompositionDelayInFrames);
      ++submitted_frames;
    }
    size_t frames_dropped = 0;
    scoped_refptr<media::VideoFrame> rendered_frame =
        RenderAndStep(&frames_dropped, kRenderInterval);
    ASSERT_TRUE(rendered_frame);
    if (frames_dropped > 0) {
      ASSERT_EQ(frames_dropped, 1u);
      EXPECT_GE(processed_frames_since_last_frame_drop,
                kMinimumNumberOfFramesBetweenDrops);
      processed_frames_since_last_frame_drop = 0;
    } else {
      ++processed_frames_since_last_frame_drop;
    }
  }

  // Steady state queue should now have been reduced to one frame + the current
  // frame that is also counted.
  EXPECT_EQ(frames_queued(), 2u);
}

TEST_F(LowLatencyVideoRendererAlgorithmTest,
       RenderFrameImmediatelyAfterOutage) {
  constexpr base::TimeDelta kRenderInterval =
      base::Milliseconds(1000.0 / 600.0 + 1.0e-3);  // 600Hz.
  constexpr int kMaxCompositionDelayInFrames = 6;

  for (int outage_length = 0; outage_length < 100; ++outage_length) {
    algorithm_.Reset();

    // Process one frame to get the algorithm initialized.
    CreateAndEnqueueFrame(kMaxCompositionDelayInFrames);
    scoped_refptr<media::VideoFrame> rendered_frame =
        RenderAndStep(nullptr, kRenderInterval);
    ASSERT_TRUE(rendered_frame);
    media::VideoFrame::ID frame_id_0 = rendered_frame->unique_id();
    StepUntilJustBeforeNextFrameIsRendered(kRenderInterval,
                                           rendered_frame->unique_id());

    for (int i = 0; i < outage_length; ++i) {
      // Try to render, but no new frame has been enqueued so the last frame
      // will be rendered again.
      rendered_frame = RenderAndStep(nullptr, kRenderInterval);
      ASSERT_TRUE(rendered_frame);
      EXPECT_EQ(rendered_frame->unique_id(), frame_id_0);
    }

    // Enqueue two frames.
    media::VideoFrame::ID frame_id_1 =
        CreateAndEnqueueFrame(kMaxCompositionDelayInFrames);
    media::VideoFrame::ID frame_id_2 =
        CreateAndEnqueueFrame(kMaxCompositionDelayInFrames);

    // The first submitted frame should be rendered.
    rendered_frame = RenderAndStep(nullptr, kRenderInterval);
    ASSERT_TRUE(rendered_frame);
    EXPECT_EQ(rendered_frame->unique_id(), frame_id_1);
    // The same frame is rendered for 9 more render intervals.
    StepUntilJustBeforeNextFrameIsRendered(kRenderInterval, frame_id_1);

    // The next frame is rendered.
    rendered_frame = RenderAndStep(nullptr, kRenderInterval);
    ASSERT_TRUE(rendered_frame);
    EXPECT_EQ(rendered_frame->unique_id(), frame_id_2);
  }
}

// Render at 60Hz with irregular vsync boundaries.
TEST_F(LowLatencyVideoRendererAlgorithmTest, NormalModeWithGlitch60Hz) {
  constexpr int kNumberOfFrames = 5;
  constexpr int kMaxCompositionDelayInFrames = 6;
  constexpr double kDeadlineBeginErrorRate[] = {0.01, 0.03, -0.01, -0.02, 0.02};
  constexpr double kDeadlineEndErrorRate[] = {0.02, -0.03, -0.02, 0.03, 0.01};
  for (int i = 0; i < kNumberOfFrames; ++i) {
    media::VideoFrame::ID frame_id =
        CreateAndEnqueueFrame(kMaxCompositionDelayInFrames);
    size_t frames_dropped = 0u;
    scoped_refptr<media::VideoFrame> rendered_frame = RenderWithGlitchAndStep(
        &frames_dropped, kDeadlineBeginErrorRate[i], kDeadlineEndErrorRate[i]);
    ASSERT_TRUE(rendered_frame);
    EXPECT_EQ(rendered_frame->unique_id(), frame_id);
    EXPECT_EQ(frames_dropped, 0u);
  }
}

// Double frame rate (120Hz playing back 60Hz video) and render with irregular
// vsync boundaries.
TEST_F(LowLatencyVideoRendererAlgorithmTest, NormalModeWithGlitch120Hz) {
  constexpr size_t kNumberOfFrames = 5;
  constexpr base::TimeDelta kRenderInterval =
      base::Milliseconds(1000.0 / 120.0);  // 120Hz.
  constexpr int kMaxCompositionDelayInFrames = 6;
  constexpr double kDeadlineBeginErrorRate[] = {0.01, 0.03, -0.01, -0.02, 0.02};
  constexpr double kDeadlineEndErrorRate[] = {0.02, -0.03, -0.02, 0.03, 0.01};

  // Add one initial frame.
  media::VideoFrame::ID last_id =
      CreateAndEnqueueFrame(kMaxCompositionDelayInFrames);

  for (size_t i = 0; i < kNumberOfFrames; ++i) {
    size_t frames_dropped = 0;
    scoped_refptr<media::VideoFrame> rendered_frame = RenderWithGlitchAndStep(
        &frames_dropped, kRenderInterval, kDeadlineBeginErrorRate[i],
        kDeadlineEndErrorRate[i]);
    ASSERT_TRUE(rendered_frame);
    media::VideoFrame::ID rendered_frame_id = last_id;
    EXPECT_EQ(rendered_frame->unique_id(), rendered_frame_id);

    last_id = CreateAndEnqueueFrame(kMaxCompositionDelayInFrames);

    // The same frame should be rendered.
    rendered_frame = RenderWithGlitchAndStep(&frames_dropped, kRenderInterval,
                                             kDeadlineBeginErrorRate[i],
                                             kDeadlineEndErrorRate[i]);
    ASSERT_TRUE(rendered_frame);
    EXPECT_EQ(rendered_frame->unique_id(), rendered_frame_id);
  }
}

}  // namespace blink
