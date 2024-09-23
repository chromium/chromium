// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "media/filters/video_renderer_algorithm.h"

#include <stddef.h>
#include <stdint.h>

#include <cmath>
#include <tuple>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ref_counted.h"
#include "base/strings/stringprintf.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "media/base/media_util.h"
#include "media/base/timestamp_constants.h"
#include "media/base/video_frame_pool.h"
#include "media/base/wall_clock_time_source.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

// Slows down the given |fps| according to NTSC field reduction standards; see
// http://en.wikipedia.org/wiki/Frame_rate#Digital_video_and_television
static double NTSC(double fps) {
  return fps / 1.001;
}

// Helper class for generating TimeTicks in a sequence according to a frequency.
class TickGenerator {
 public:
  TickGenerator(base::TimeTicks base_timestamp, double hertz)
      : tick_count_(0),
        hertz_(hertz),
        microseconds_per_tick_(base::Time::kMicrosecondsPerSecond / hertz),
        base_time_(base_timestamp) {}

  TickGenerator(const TickGenerator&) = delete;
  TickGenerator& operator=(const TickGenerator&) = delete;

  base::TimeDelta interval(int tick_count) const {
    return base::Microseconds(tick_count * microseconds_per_tick_);
  }

  base::TimeTicks current() const { return base_time_ + interval(tick_count_); }
  base::TimeTicks step() { return step(1); }
  base::TimeTicks step(int n) {
    tick_count_ += n;
    return current();
  }

  double hertz() const { return hertz_; }

  void Reset(base::TimeTicks base_timestamp) {
    base_time_ = base_timestamp;
    tick_count_ = 0;
  }

 private:
  // Track a tick count and seconds per tick value to ensure we don't drift too
  // far due to accumulated errors during testing.
  int64_t tick_count_;
  const double hertz_;
  const double microseconds_per_tick_;
  base::TimeTicks base_time_;
};

class VideoRendererAlgorithmTest : public testing::Test {
 public:
  VideoRendererAlgorithmTest()
      : tick_clock_(std::make_unique<base::SimpleTestTickClock>()),
        time_source_(tick_clock_.get()),
        algorithm_(base::BindRepeating(&WallClockTimeSource::GetWallClockTimes,
                                       base::Unretained(&time_source_)),
                   &media_log_) {
    // Always start the TickClock at a non-zero value since null values have
    // special connotations.
    tick_clock_->Advance(base::Microseconds(10000));
  }

  VideoRendererAlgorithmTest(const VideoRendererAlgorithmTest&) = delete;
  VideoRendererAlgorithmTest& operator=(const VideoRendererAlgorithmTest&) =
      delete;

  ~VideoRendererAlgorithmTest() override = default;

  scoped_refptr<VideoFrame> CreateFrame(base::TimeDelta timestamp) {
    const gfx::Size natural_size(8, 8);
    return frame_pool_.CreateFrame(PIXEL_FORMAT_I420, natural_size,
                                   gfx::Rect(natural_size), natural_size,
                                   timestamp);
  }

  base::TimeDelta minimum_glitch_time() const {
    return base::Seconds(
        VideoRendererAlgorithm::kMinimumAcceptableTimeBetweenGlitchesSecs);
  }

  base::TimeDelta max_acceptable_drift() const {
    return algorithm_.max_acceptable_drift_;
  }

  void disable_cadence_hysteresis() {
    algorithm_.cadence_estimator_.set_cadence_hysteresis_threshold_for_testing(
        base::TimeDelta());
  }

  bool last_render_had_glitch() const {
    return algorithm_.last_render_had_glitch_;
  }

  bool is_using_cadence() const {
    return algorithm_.cadence_estimator_.has_cadence();
  }

  bool IsCadenceBelowOne() const {
    if (!is_using_cadence())
      return false;

    return algorithm_.cadence_estimator_.avg_cadence_for_testing() < 1.0;
  }

  double CadenceValue() const {
    return algorithm_.cadence_estimator_.avg_cadence_for_testing();
  }

  size_t frames_queued() const { return algorithm_.frame_queue_.size(); }

  std::string GetCadence(double frame_rate, double display_rate) {
    TickGenerator display_tg(tick_clock_->NowTicks(), display_rate);
    TickGenerator frame_tg(base::TimeTicks(), frame_rate);
    time_source_.StartTicking();

    // Enqueue enough frames for cadence detection.
    size_t frames_dropped = 0;
    disable_cadence_hysteresis();
    algorithm_.EnqueueFrame(CreateFrame(frame_tg.interval(0)));
    algorithm_.EnqueueFrame(CreateFrame(frame_tg.interval(1)));
    EXPECT_TRUE(RenderAndStep(&display_tg, &frames_dropped));

    // Store cadence before resetting the algorithm.
    const std::string cadence =
        algorithm_.cadence_estimator_.GetCadenceForTesting();
    time_source_.StopTicking();
    algorithm_.Reset();
    return cadence;
  }

  base::TimeDelta CalculateAbsoluteDriftForFrame(base::TimeTicks deadline_min,
                                                 int frame_index) {
    return algorithm_.CalculateAbsoluteDriftForFrame(deadline_min, frame_index);
  }

  bool DriftOfLastRenderWasWithinTolerance(base::TimeTicks deadline_min) {
    return CalculateAbsoluteDriftForFrame(deadline_min, 0) <=
           algorithm_.max_acceptable_drift_;
  }

  scoped_refptr<VideoFrame> RenderAndStep(TickGenerator* tg,
                                          size_t* frames_dropped) {
    const base::TimeTicks start = tg->current();
    const base::TimeTicks end = tg->step();
    return algorithm_.Render(start, end, frames_dropped);
  }

  // Allows tests to run a Render() loop with sufficient frames for the various
  // rendering modes. Upon each Render() |render_test_func| will be called with
  // the rendered frame and the number of frames dropped.
  template <typename OnRenderCallback>
  void RunFramePumpTest(bool reset,
                        TickGenerator* frame_tg,
                        TickGenerator* display_tg,
                        OnRenderCallback render_test_func) {
    SCOPED_TRACE(base::StringPrintf("Rendering %.03f fps into %0.03f",
                                    frame_tg->hertz(), display_tg->hertz()));
    tick_clock_->Advance(display_tg->current() - tick_clock_->NowTicks());
    time_source_.StartTicking();

    const bool fresh_algorithm = !algorithm_.have_rendered_frames_;

    base::TimeDelta last_start_timestamp = kNoTimestamp;
    bool should_use_cadence = false;
    int glitch_count = 0;
    const base::TimeTicks start_time = tick_clock_->NowTicks();
    while (tick_clock_->NowTicks() - start_time < minimum_glitch_time()) {
      while (EffectiveFramesQueued() < 3 ||
             frame_tg->current() - time_source_.CurrentMediaTime() <
                 base::TimeTicks()) {
        algorithm_.EnqueueFrame(
            CreateFrame(frame_tg->current() - base::TimeTicks()));
        frame_tg->step();
      }

      size_t frames_dropped = 0;
      const base::TimeTicks deadline_min = display_tg->current();
      const base::TimeTicks deadline_max = display_tg->step();
      scoped_refptr<VideoFrame> frame =
          algorithm_.Render(deadline_min, deadline_max, &frames_dropped);
      EXPECT_EQ(deadline_max - deadline_min, algorithm_.render_interval());

      render_test_func(frame, frames_dropped);
      tick_clock_->Advance(display_tg->current() - tick_clock_->NowTicks());

      if (HasFatalFailure())
        return;

      // Render() should always return a frame within drift tolerances.
      ASSERT_TRUE(DriftOfLastRenderWasWithinTolerance(deadline_min));

      // If we have a frame, the timestamps should always be monotonically
      // increasing.
      if (frame) {
        if (last_start_timestamp != kNoTimestamp)
          ASSERT_LE(last_start_timestamp, frame->timestamp());
        else
          last_start_timestamp = frame->timestamp();
      }

      // Only verify certain properties for fresh instances.
      if (fresh_algorithm) {
        ASSERT_NEAR(frame_tg->interval(1).InMicroseconds(),
                    algorithm_.average_frame_duration().InMicroseconds(), 1);

        if (is_using_cadence() && last_render_had_glitch())
          ++glitch_count;

        // Once cadence starts, it should never stop for the current set of
        // tests.
        if (is_using_cadence())
          should_use_cadence = true;
        ASSERT_EQ(is_using_cadence(), should_use_cadence);
      }

      // When there are no frames, we're not using cadence based selection, or a
      // frame is under cadence the two queue size reports should be equal to
      // the number of usable frames; i.e. those frames whose end time was not
      // within the last render interval.
      if (!is_using_cadence() || !frames_queued() ||
          GetCurrentFrameDisplayCount() < GetCurrentFrameIdealDisplayCount()) {
        ASSERT_NEAR(GetUsableFrameCount(deadline_max), EffectiveFramesQueued(),
                    fresh_algorithm ? 0 : 1);
      } else if (is_using_cadence() && !IsCadenceBelowOne()) {
        // If there was no glitch in the last render, the two queue sizes should
        // be off by exactly one frame; i.e., the current frame doesn't count.
        if (!last_render_had_glitch() && fresh_algorithm)
          ASSERT_EQ(frames_queued() - 1, EffectiveFramesQueued());
      } else if (IsCadenceBelowOne()) {
        // The frame estimate should be off by at most one frame.
        const size_t estimated_frames_queued =
            std::floor(frames_queued() * CadenceValue());
        ASSERT_NEAR(EffectiveFramesQueued(), estimated_frames_queued, 1);
      }
    }

    // When using cadence, the glitch count should be at most one for when
    // rendering for the less than minimum_glitch_time().
    if (fresh_algorithm && is_using_cadence())
      ASSERT_LE(glitch_count, 1);

    time_source_.StopTicking();
    if (reset) {
      algorithm_.Reset();
      time_source_.SetMediaTime(base::TimeDelta());
    }
  }

  int FindBestFrameByCoverage(base::TimeTicks deadline_min,
                              base::TimeTicks deadline_max,
                              int* second_best) {
    return algorithm_.FindBestFrameByCoverage(deadline_min, deadline_max,
                                              second_best);
  }

  int FindBestFrameByDrift(base::TimeTicks deadline_min,
                           base::TimeDelta* selected_frame_drift) {
    return algorithm_.FindBestFrameByDrift(deadline_min, selected_frame_drift);
  }

  int GetCurrentFrameDropCount() const {
    DCHECK_GT(frames_queued(), 0u);
    return algorithm_.frame_queue_.front().drop_count;
  }

  int GetCurrentFrameDisplayCount() const {
    DCHECK_GT(frames_queued(), 0u);
    return algorithm_.frame_queue_.front().render_count;
  }

  int GetCurrentFrameIdealDisplayCount() const {
    DCHECK_GT(frames_queued(), 0u);
    return algorithm_.frame_queue_.front().ideal_render_count;
  }

  int AccountForMissedIntervalsAndStep(TickGenerator* tg) {
    const base::TimeTicks start = tg->current();
    const base::TimeTicks end = tg->step();
    return AccountForMissedIntervals(start, end);
  }

  int AccountForMissedIntervals(base::TimeTicks deadline_min,
                                base::TimeTicks deadline_max) {
    algorithm_.AccountForMissedIntervals(deadline_min, deadline_max);
    return frames_queued() ? GetCurrentFrameDisplayCount() : -1;
  }

  size_t GetUsableFrameCount(base::TimeTicks deadline_max) {
    if (is_using_cadence())
      return frames_queued();

    for (size_t i = 0; i < frames_queued(); ++i)
      if (algorithm_.frame_queue_[i].end_time > deadline_max)
        return frames_queued() - i;
    return 0;
  }

  size_t EffectiveFramesQueued() {
    const size_t expected_frames_queued = algorithm_.effective_frames_queued();
    // These values should always be in sync.
    algorithm_.UpdateEffectiveFramesQueued();
    EXPECT_EQ(expected_frames_queued, algorithm_.effective_frames_queued());
    return expected_frames_queued;
  }

 protected:
  NullMediaLog media_log_;
  VideoFramePool frame_pool_;
  std::unique_ptr<base::SimpleTestTickClock> tick_clock_;
  WallClockTimeSource time_source_;
  VideoRendererAlgorithm algorithm_;
};

TEST_F(VideoRendererAlgorithmTest, Empty) {
  TickGenerator tg(tick_clock_->NowTicks(), 50);
  size_t frames_dropped = 0;
  EXPECT_EQ(0u, frames_queued());
  EXPECT_FALSE(RenderAndStep(&tg, &frames_dropped));
  EXPECT_EQ(0u, frames_dropped);
  EXPECT_EQ(0u, frames_queued());
  EXPECT_NE(base::TimeDelta(), max_acceptable_drift());
}

TEST_F(VideoRendererAlgorithmTest, Reset) {
  TickGenerator tg(tick_clock_->NowTicks(), 50);
  algorithm_.EnqueueFrame(CreateFrame(tg.interval(0)));
  EXPECT_EQ(1u, frames_queued());
  EXPECT_NE(base::TimeDelta(), max_acceptable_drift());
  algorithm_.Reset();
  EXPECT_EQ(0u, frames_queued());
  EXPECT_NE(base::TimeDelta(), max_acceptable_drift());

  // Enqueue a frame and render enough such that the next frame should be
  // considered ineffective.
  time_source_.StartTicking();
  size_t frames_dropped = 0;
  algorithm_.EnqueueFrame(CreateFrame(tg.interval(0)));
  algorithm_.EnqueueFrame(CreateFrame(tg.interval(1)));
  scoped_refptr<VideoFrame> frame = RenderAndStep(&tg, &frames_dropped);
  ASSERT_TRUE(frame);
  EXPECT_EQ(tg.interval(0), frame->timestamp());
  EXPECT_EQ(0u, frames_dropped);
  EXPECT_EQ(1u, EffectiveFramesQueued());

  for (int i = 0; i < 2; ++i) {
    frame = RenderAndStep(&tg, &frames_dropped);
    ASSERT_TRUE(frame);
    EXPECT_EQ(tg.interval(1), frame->timestamp());
    EXPECT_EQ(0u, frames_dropped);
    EXPECT_EQ(0u, EffectiveFramesQueued());
  }
  time_source_.StopTicking();

  // After reset the new frame should still be counted as ineffective.
  algorithm_.Reset(
      VideoRendererAlgorithm::ResetFlag::kPreserveNextFrameEstimates);
  algorithm_.EnqueueFrame(CreateFrame(tg.interval(2)));
  EXPECT_EQ(0u, EffectiveFramesQueued());
  algorithm_.EnqueueFrame(CreateFrame(tg.interval(3)));
  ASSERT_EQ(1u, algorithm_.RemoveExpiredFrames(
                    tg.current() + algorithm_.average_frame_duration()));
}

TEST_F(VideoRendererAlgorithmTest, AccountForMissingIntervals) {
  TickGenerator tg(tick_clock_->NowTicks(), 50);
  time_source_.StartTicking();

  // Disable hysteresis since AccountForMissingIntervals() only affects cadence
  // based rendering.
  disable_cadence_hysteresis();

  // Simulate Render() called before any frames are present.
  EXPECT_EQ(-1, AccountForMissedIntervalsAndStep(&tg));

  algorithm_.EnqueueFrame(CreateFrame(tg.interval(0)));
  algorithm_.EnqueueFrame(CreateFrame(tg.interval(1)));
  algorithm_.EnqueueFrame(CreateFrame(tg.interval(2)));
  algorithm_.EnqueueFrame(CreateFrame(tg.interval(3)));

  // Simulate Render() called before any frames have been rendered.
  EXPECT_EQ(0, AccountForMissedIntervalsAndStep(&tg));

  // Render one frame (several are in the past and will be dropped).
  base::TimeTicks deadline_min = tg.current();
  base::TimeTicks deadline_max = tg.step();
  size_t frames_dropped = 0;
  scoped_refptr<VideoFrame> frame =
      algorithm_.Render(deadline_min, deadline_max, &frames_dropped);
  ASSERT_TRUE(frame);
  EXPECT_EQ(tg.interval(2), frame->timestamp());
  EXPECT_EQ(2u, frames_dropped);

  ASSERT_EQ(1, GetCurrentFrameDisplayCount());

  // Now calling AccountForMissingIntervals with an interval which overlaps the
  // previous should do nothing.
  deadline_min += tg.interval(1) / 2;
  deadline_max += tg.interval(1) / 2;
  EXPECT_EQ(1, AccountForMissedIntervals(deadline_min, deadline_max));

  // Steping by 1.5 intervals, is not enough to increase the count.
  deadline_min += tg.interval(1);
  deadline_max += tg.interval(1);
  EXPECT_EQ(1, AccountForMissedIntervals(deadline_min, deadline_max));

  // Calling it after a full skipped interval should increase the count by 1 for
  // each skipped interval.
  tg.step();
  EXPECT_EQ(2, AccountForMissedIntervalsAndStep(&tg));

  // 4 because [tg.current(), tg.step()] now represents 2 additional intervals.
  EXPECT_EQ(4, AccountForMissedIntervalsAndStep(&tg));

  // Frame should be way over cadence and no good frames remain, so last frame
  // should be returned.
  frame = RenderAndStep(&tg, &frames_dropped);
  ASSERT_TRUE(frame);
  EXPECT_EQ(tg.interval(3), frame->timestamp());
  EXPECT_EQ(0u, frames_dropped);
  EXPECT_EQ(1, GetCurrentFrameDisplayCount());

  // Stop the time source and verify AccountForMissedIntervals() doesn't try to
  // account for intervals from pause behavior.
  time_source_.StopTicking();
  frame = RenderAndStep(&tg, &frames_dropped);
  ASSERT_TRUE(frame);
  EXPECT_EQ(tg.interval(3), frame->timestamp());
  EXPECT_EQ(0u, frames_dropped);
  EXPECT_EQ(2, GetCurrentFrameDisplayCount());

  tg.step(100);
  frame = RenderAndStep(&tg, &frames_dropped);
  ASSERT_TRUE(frame);
  EXPECT_EQ(tg.interval(3), frame->timestamp());
  EXPECT_EQ(0u, frames_dropped);
  EXPECT_EQ(3, GetCurrentFrameDisplayCount());

  time_source_.StartTicking();

  // Now run the same test using set_time_stopped();
  frame = RenderAndStep(&tg, &frames_dropped);
  ASSERT_TRUE(frame);
  EXPECT_EQ(tg.interval(3), frame->timestamp());
  EXPECT_EQ(0u, frames_dropped);
  EXPECT_EQ(4, GetCurrentFrameDisplayCount());

  algorithm_.set_time_stopped();
  tg.step(100);
  frame = RenderAndStep(&tg, &frames_dropped);
  ASSERT_TRUE(frame);
  EXPECT_EQ(tg.interval(3), frame->timestamp());
  EXPECT_EQ(0u, frames_dropped);
  EXPECT_EQ(5, GetCurrentFrameDisplayCount());
}

TEST_F(VideoRendererAlgorithmTest, OnLastFrameDropped) {
  TickGenerator frame_tg(base::TimeTicks(), 25);
  TickGenerator display_tg(tick_clock_->NowTicks(), 50);
  time_source_.StartTicking();

  // Disable hysteresis since OnLastFrameDropped() only affects cadence based
  // rendering.
  disable_cadence_hysteresis();

  algorithm_.EnqueueFrame(CreateFrame(frame_tg.interval(0)));
  algorithm_.EnqueueFrame(CreateFrame(frame_tg.interval(1)));
  algorithm_.EnqueueFrame(CreateFrame(frame_tg.interval(2)));

  // Render one frame (several are in the past and will be dropped).
  size_t frames_dropped = 0;
  scoped_refptr<VideoFrame> frame = RenderAndStep(&display_tg, &frames_dropped);
  ASSERT_TRUE(frame);
  EXPECT_EQ(frame_tg.interval(0), frame->timestamp());
  EXPECT_EQ(0u, frames_dropped);

  // The frame should have its display count decremented once it's reported as
  // dropped.
  ASSERT_EQ(1, GetCurrentFrameDisplayCount());
  ASSERT_EQ(0, GetCurrentFrameDropCount());
  algorithm_.OnLastFrameDropped();
  ASSERT_EQ(1, GetCurrentFrameDisplayCount());
  ASSERT_EQ(1, GetCurrentFrameDropCount());

  // Render the frame again and then force another drop.
  frame = RenderAndStep(&display_tg, &frames_dropped);
  ASSERT_TRUE(frame);
  EXPECT_EQ(frame_tg.interval(0), frame->timestamp());
  EXPECT_EQ(0u, frames_dropped);

  ASSERT_EQ(2, GetCurrentFrameDisplayCount());
  ASSERT_EQ(1, GetCurrentFrameDropCount());
  algorithm_.OnLastFrameDropped();
  ASSERT_EQ(2, GetCurrentFrameDisplayCount());
  ASSERT_EQ(2, GetCurrentFrameDropCount());

  // The next Render() call should now count this frame as dropped.
  frame = RenderAndStep(&display_tg, &frames_dropped);
  ASSERT_TRUE(frame);
  EXPECT_EQ(frame_tg.interval(1), frame->timestamp());
  EXPECT_EQ(1u, frames_dropped);
  ASSERT_EQ(1, GetCurrentFrameDisplayCount());
  ASSERT_EQ(0, GetCurrentFrameDropCount());

  // Rendering again should result in the same frame being displayed.
  frame = RenderAndStep(&display_tg, &frames_dropped);
  ASSERT_TRUE(frame);
  EXPECT_EQ(frame_tg.interval(1), frame->timestamp());
  EXPECT_EQ(0u, frames_dropped);

  // In this case, the drop count is less than the display count, so the frame
  // should not be counted as dropped.
  ASSERT_EQ(2, GetCurrentFrameDisplayCount());
  ASSERT_EQ(0, GetCurrentFrameDropCount());
  algorithm_.OnLastFrameDropped();
  ASSERT_EQ(2, GetCurrentFrameDisplayCount());
  ASSERT_EQ(1, GetCurrentFrameDropCount());

  // The third frame should be rendered correctly now and the previous frame not
  // counted as having been dropped.
  frame = RenderAndStep(&display_tg, &frames_dropped);
  ASSERT_TRUE(frame);
  EXPECT_EQ(frame_tg.interval(2), frame->timestamp());
  EXPECT_EQ(0u, frames_dropped);
}

TEST_F(VideoRendererAlgorithmTest, OnLastFrameDroppedFirstFrame) {
  TickGenerator frame_tg(base::TimeTicks(), 25);
  TickGenerator display_tg(tick_clock_->NowTicks(), 50);
  time_source_.StartTicking();

  // Disable hysteresis since OnLastFrameDropped() only affects cadence based
  // rendering.
  disable_cadence_hysteresis();

  // Use frames in the future to simulate cases where the first frame may be
  // renderered many times.
  algorithm_.EnqueueFrame(CreateFrame(frame_tg.interval(5)));
  algorithm_.EnqueueFrame(CreateFrame(frame_tg.interval(6)));
  algorithm_.EnqueueFrame(CreateFrame(frame_tg.interval(7)));

  size_t frames_dropped = 0;
  scoped_refptr<VideoFrame> frame =
      algorithm_.Render(base::TimeTicks(), base::TimeTicks(), &frames_dropped);
  ASSERT_TRUE(frame);
  EXPECT_EQ(frame_tg.interval(5), frame->timestamp());
  EXPECT_EQ(0u, frames_dropped);

  // The frame should have its drop count updated once it's reported as dropped.
  ASSERT_EQ(1, GetCurrentFrameDisplayCount());
  ASSERT_EQ(0, GetCurrentFrameDropCount());
  algorithm_.OnLastFrameDropped();
  ASSERT_EQ(1, GetCurrentFrameDisplayCount());
  ASSERT_EQ(1, GetCurrentFrameDropCount());

  // Render the frame and check counts at each step.
  const int kLastValue = 2 * 5 + 2 - 1;  // Cadence is 2, -1 for Render() above.
  for (int i = 0; i < kLastValue; ++i) {
    frame = RenderAndStep(&display_tg, &frames_dropped);
    ASSERT_TRUE(frame);
    EXPECT_EQ(frame_tg.interval(5), frame->timestamp());
    EXPECT_EQ(0u, frames_dropped);

    ASSERT_EQ(i + 2, GetCurrentFrameDisplayCount());
    if (i == 0) {
      ASSERT_EQ(i + 1, GetCurrentFrameDropCount());
      algorithm_.OnLastFrameDropped();
      ASSERT_EQ(i + 2, GetCurrentFrameDisplayCount());
      ASSERT_EQ(i + 2, GetCurrentFrameDropCount());
    }
  }

  // Ensure the next frame does not pick up the overage.
  frame = RenderAndStep(&display_tg, &frames_dropped);
  ASSERT_TRUE(frame);
  EXPECT_EQ(frame_tg.interval(6), frame->timestamp());
  EXPECT_EQ(0u, frames_dropped);

  ASSERT_EQ(1, GetCurrentFrameDisplayCount());
  ASSERT_EQ(0, GetCurrentFrameDropCount());
  algorithm_.OnLastFrameDropped();
  ASSERT_EQ(1, GetCurrentFrameDisplayCount());
  ASSERT_EQ(1, GetCurrentFrameDropCount());

  // Stop time and verify cadence overage isn't accumulated for next frame.
  time_source_.StopTicking();
  for (int i = 0; i < 5; ++i) {
    frame = RenderAndStep(&display_tg, &frames_dropped);
    ASSERT_TRUE(frame);
    EXPECT_EQ(frame_tg.interval(6), frame->timestamp());
    EXPECT_EQ(0u, frames_dropped);

    ASSERT_EQ(i + 2, GetCurrentFrameDisplayCount());
    ASSERT_EQ(1, GetCurrentFrameDropCount());
  }

  time_source_.StartTicking();
  frame = RenderAndStep(&display_tg, &frames_dropped);
  ASSERT_TRUE(frame);
  EXPECT_EQ(frame_tg.interval(7), frame->timestamp());
  EXPECT_EQ(0u, frames_dropped);

  ASSERT_EQ(1, GetCurrentFrameDisplayCount());
  ASSERT_EQ(0, GetCurrentFrameDropCount());
  algorithm_.OnLastFrameDropped();
  ASSERT_EQ(1, GetCurrentFrameDisplayCount());
  ASSERT_EQ(1, GetCurrentFrameDropCount());
}

TEST_F(VideoRendererAlgorithmTest, EffectiveFramesQueued) {
  TickGenerator frame_tg(base::TimeTicks(), 50);
  TickGenerator display_tg(tick_clock_->NowTicks(), 25);

  // Disable hysteresis since EffectiveFramesQueued() is tested as part of the
  // normal frame pump tests when cadence is not present.
  disable_cadence_hysteresis();

  EXPECT_EQ(0u, EffectiveFramesQueued());
  time_source_.StartTicking();

  algorithm_.EnqueueFrame(CreateFrame(frame_tg.interval(0)));
  EXPECT_EQ(1u, EffectiveFramesQueued());

  algorithm_.EnqueueFrame(CreateFrame(frame_tg.interval(1)));
  EXPECT_EQ(2u, EffectiveFramesQueued());

  algorithm_.EnqueueFrame(CreateFrame(frame_tg.interval(2)));
  EXPECT_EQ(3u, EffectiveFramesQueued());

  algorithm_.EnqueueFrame(CreateFrame(frame_tg.interval(3)));
  EXPECT_EQ(4u, EffectiveFramesQueued());
  EXPECT_EQ(4u, frames_queued());

  // Render one frame which will detect cadence...
  size_t frames_dropped = 0;
  scoped_refptr<VideoFrame> frame = RenderAndStep(&display_tg, &frames_dropped);
  ASSERT_TRUE(frame);
  EXPECT_EQ(frame_tg.interval(0), frame->timestamp());
  EXPECT_EQ(0u, frames_dropped);

  // Fractional cadence should be detected and the count will decrease.
  ASSERT_TRUE(is_using_cadence());
  EXPECT_EQ(1u, EffectiveFramesQueued());
  EXPECT_EQ(4u, frames_queued());

  // Dropping the last rendered frame should do nothing, since the last frame
  // is already excluded from the count if it has a display count of 1.
  algorithm_.OnLastFrameDropped();
  EXPECT_EQ(1u, EffectiveFramesQueued());
}

TEST_F(VideoRendererAlgorithmTest, EffectiveFramesQueuedWithoutCadence) {
  TickGenerator tg(tick_clock_->NowTicks(), 60);

  EXPECT_EQ(0u, EffectiveFramesQueued());
  time_source_.StartTicking();

  algorithm_.EnqueueFrame(CreateFrame(tg.interval(0)));
  EXPECT_EQ(1u, EffectiveFramesQueued());

  algorithm_.EnqueueFrame(CreateFrame(tg.interval(1)));
  EXPECT_EQ(2u, EffectiveFramesQueued());

  algorithm_.EnqueueFrame(CreateFrame(tg.interval(2)));
  EXPECT_EQ(3u, EffectiveFramesQueued());

  algorithm_.EnqueueFrame(CreateFrame(tg.interval(3)));
  EXPECT_EQ(4u, EffectiveFramesQueued());
  EXPECT_EQ(4u, frames_queued());
  EXPECT_EQ(384, algorithm_.GetMemoryUsage());

  // Issue a render call that should drop the first two frames and mark the 3rd
  // as consumed.
  tg.step(2);
  size_t frames_dropped = 0;
  scoped_refptr<VideoFrame> frame = RenderAndStep(&tg, &frames_dropped);
  ASSERT_FALSE(is_using_cadence());
  ASSERT_TRUE(frame);
  EXPECT_EQ(2u, frames_dropped);
  EXPECT_EQ(tg.interval(2), frame->timestamp());
  EXPECT_EQ(1u, EffectiveFramesQueued());
  EXPECT_EQ(2u, frames_queued());
  EXPECT_EQ(192, algorithm_.GetMemoryUsage());

  // Rendering one more frame should return 0 effective frames queued.
  frame = RenderAndStep(&tg, &frames_dropped);
  ASSERT_FALSE(is_using_cadence());
  ASSERT_TRUE(frame);
  EXPECT_EQ(0u, frames_dropped);
  EXPECT_EQ(tg.interval(3), frame->timestamp());
  EXPECT_EQ(0u, EffectiveFramesQueued());
  EXPECT_EQ(1u, frames_queued());
  EXPECT_EQ(96, algorithm_.GetMemoryUsage());
}

TEST_F(VideoRendererAlgorithmTest, EffectiveFramesQueuedWithoutFrameDropping) {
  TickGenerator tg(tick_clock_->NowTicks(), 50);

  algorithm_.disable_frame_dropping();

  ASSERT_EQ(0u, EffectiveFramesQueued());
  time_source_.StartTicking();

  for (size_t i = 0; i < 3; ++i) {
    algorithm_.EnqueueFrame(CreateFrame(tg.interval(i)));
    EXPECT_EQ(i + 1, EffectiveFramesQueued());
    EXPECT_EQ(i + 1, frames_queued());
  }

  // Issue a render call and verify that undropped frames remain effective.
  tg.step(2);
  size_t frames_dropped = 0;
  scoped_refptr<VideoFrame> frame = RenderAndStep(&tg, &frames_dropped);
  ASSERT_NE(nullptr, frame);
  EXPECT_EQ(tg.interval(0), frame->timestamp());
  EXPECT_EQ(0u, frames_dropped);
  EXPECT_EQ(2u, EffectiveFramesQueued());

  // As the next frame is consumed, the count of effective frames is
  // decremented.
  frame = RenderAndStep(&tg, &frames_dropped);
  ASSERT_NE(nullptr, frame);
  EXPECT_EQ(tg.interval(1), frame->timestamp());
  EXPECT_EQ(0u, frames_dropped);
  EXPECT_EQ(1u, EffectiveFramesQueued());
}

// The maximum acceptable drift should be updated once we have two frames.
TEST_F(VideoRendererAlgorithmTest, AcceptableDriftUpdated) {
  TickGenerator tg(tick_clock_->NowTicks(), 50);

  size_t frames_dropped = 0;
  const base::TimeDelta original_drift = max_acceptable_drift();
  algorithm_.EnqueueFrame(CreateFrame(tg.interval(0)));
  EXPECT_EQ(1u, frames_queued());
  EXPECT_TRUE(RenderAndStep(&tg, &frames_dropped));
  EXPECT_EQ(original_drift, max_acceptable_drift());

  // Time must be ticking to get wall clock times for frames.
  time_source_.StartTicking();

  algorithm_.EnqueueFrame(CreateFrame(tg.interval(1)));
  EXPECT_EQ(2u, frames_queued());
  EXPECT_TRUE(RenderAndStep(&tg, &frames_dropped));
  EXPECT_NE(original_drift, max_acceptable_drift());
}

// Verifies behavior when time stops.
TEST_F(VideoRendererAlgorithmTest, TimeIsStopped) {
  TickGenerator tg(tick_clock_->NowTicks(), 50);

  // Prior to rendering the first frame, the algorithm should always return the
  // first available frame.
  size_t frames_dropped = 0;
  algorithm_.EnqueueFrame(CreateFrame(tg.interval(0)));
  EXPECT_EQ(1u, frames_queued());
  scoped_refptr<VideoFrame> frame = RenderAndStep(&tg, &frames_dropped);
  ASSERT_TRUE(frame);
  EXPECT_EQ(tg.interval(0), frame->timestamp());
  EXPECT_EQ(0u, frames_dropped);
  EXPECT_EQ(1u, frames_queued());
  EXPECT_EQ(1u, EffectiveFramesQueued());

  // The same timestamp should be returned after time starts.
  tick_clock_->Advance(tg.interval(1));
  time_source_.StartTicking();
  frame = RenderAndStep(&tg, &frames_dropped);
  ASSERT_TRUE(frame);
  EXPECT_EQ(tg.interval(0), frame->timestamp());
  EXPECT_EQ(0u, frames_dropped);
  EXPECT_EQ(1u, frames_queued());
  EXPECT_EQ(1u, EffectiveFramesQueued());

  // Ensure the next suitable frame is vended as time advances.
  algorithm_.EnqueueFrame(CreateFrame(tg.interval(1)));
  EXPECT_EQ(2u, frames_queued());
  EXPECT_EQ(2u, EffectiveFramesQueued());
  frame = RenderAndStep(&tg, &frames_dropped);
  ASSERT_TRUE(frame);
  EXPECT_EQ(tg.interval(1), frame->timestamp());
  EXPECT_EQ(0u, frames_dropped);
  EXPECT_EQ(1u, frames_queued());
  EXPECT_EQ(0u, EffectiveFramesQueued());

  // Once time stops ticking, any further frames shouldn't be returned, even if
  // the interval requested more closely matches.
  algorithm_.EnqueueFrame(CreateFrame(tg.interval(2)));
  time_source_.StopTicking();
  frame = RenderAndStep(&tg, &frames_dropped);
  ASSERT_TRUE(frame);
  EXPECT_EQ(tg.interval(1), frame->timestamp());
  EXPECT_EQ(0u, frames_dropped);
  EXPECT_EQ(2u, frames_queued());
  EXPECT_EQ(1u, EffectiveFramesQueued());
}

// Verify frames inserted out of order end up in the right spot and are rendered
// according to the API contract.
TEST_F(VideoRendererAlgorithmTest, SortedFrameQueue) {
  TickGenerator tg(tick_clock_->NowTicks(), 50);

  // Ensure frames handed in out of order before time starts ticking are sorted
  // and returned in the correct order upon Render().
  algorithm_.EnqueueFrame(CreateFrame(tg.interval(3)));
  algorithm_.EnqueueFrame(CreateFrame(tg.interval(2)));
  EXPECT_EQ(2u, frames_queued());
  EXPECT_EQ(2u, EffectiveFramesQueued());

  time_source_.StartTicking();

  // The first call should return the earliest frame appended.
  size_t frames_dropped = 0;
  scoped_refptr<VideoFrame> frame = RenderAndStep(&tg, &frames_dropped);
  EXPECT_EQ(0u, frames_dropped);
  EXPECT_EQ(tg.interval(2), frame->timestamp());
  EXPECT_EQ(2u, frames_queued());
  EXPECT_EQ(2u, EffectiveFramesQueued());

  // Since a frame has already been rendered, queuing this frame and calling
  // Render() should result in it being dropped; even though it's a better
  // candidate for the desired interval.  The frame is dropped during enqueue so
  // it won't show up in frames_queued().
  algorithm_.EnqueueFrame(CreateFrame(tg.interval(1)));
  EXPECT_EQ(2u, frames_queued());
  EXPECT_EQ(2u, EffectiveFramesQueued());
  frame = RenderAndStep(&tg, &frames_dropped);
  EXPECT_EQ(1u, frames_dropped);
  EXPECT_EQ(tg.interval(2), frame->timestamp());
  EXPECT_EQ(2u, frames_queued());
  EXPECT_EQ(2u, EffectiveFramesQueued());
}

// Run through integer cadence selection for 1, 2, 3, and 4.
TEST_F(VideoRendererAlgorithmTest, BestFrameByCadence) {
  const double kTestRates[][2] = {{60, 60}, {30, 60}, {25, 75}, {25, 100}};

  for (const auto& test_rate : kTestRates) {
    disable_cadence_hysteresis();

    TickGenerator frame_tg(base::TimeTicks(), test_rate[0]);
    TickGenerator display_tg(tick_clock_->NowTicks(), test_rate[1]);

    int actual_frame_pattern = 0;
    const int desired_frame_pattern = test_rate[1] / test_rate[0];
    scoped_refptr<VideoFrame> current_frame;
    RunFramePumpTest(
        true, &frame_tg, &display_tg,
        [&current_frame, &actual_frame_pattern, desired_frame_pattern, this](
            scoped_refptr<VideoFrame> frame, size_t frames_dropped) {
          ASSERT_TRUE(frame);
          ASSERT_EQ(0u, frames_dropped);

          // Each frame should display for exactly it's desired cadence pattern.
          if (!current_frame || current_frame == frame) {
            actual_frame_pattern++;
          } else {
            ASSERT_EQ(actual_frame_pattern, desired_frame_pattern);
            actual_frame_pattern = 1;
          }

          current_frame = frame;
          ASSERT_TRUE(is_using_cadence());
        });

    if (HasFatalFailure())
      return;
  }
}

TEST_F(VideoRendererAlgorithmTest, BestFrameByCadenceOverdisplayed) {
  TickGenerator frame_tg(base::TimeTicks(), 25);
  TickGenerator display_tg(tick_clock_->NowTicks(), 50);
  time_source_.StartTicking();
  disable_cadence_hysteresis();

  algorithm_.EnqueueFrame(CreateFrame(frame_tg.interval(0)));
  algorithm_.EnqueueFrame(CreateFrame(frame_tg.interval(1)));

  // Render frames until we've exhausted available frames and the last frame is
  // forced to be over displayed.
  for (int i = 0; i < 5; ++i) {
    size_t frames_dropped = 0;
    scoped_refptr<VideoFrame> frame =
        RenderAndStep(&display_tg, &frames_dropped);
    ASSERT_TRUE(frame);
    EXPECT_EQ(frame_tg.interval(i < 4 ? i / 2 : 1), frame->timestamp());
    EXPECT_EQ(0u, frames_dropped);
    ASSERT_EQ(2, GetCurrentFrameIdealDisplayCount());
  }

  // Verify last frame is above cadence (2 in this case)
  ASSERT_EQ(GetCurrentFrameIdealDisplayCount() + 1,
            GetCurrentFrameDisplayCount());
  algorithm_.EnqueueFrame(CreateFrame(frame_tg.interval(2)));
  algorithm_.EnqueueFrame(CreateFrame(frame_tg.interval(3)));

  // The next frame should still be displayed once, even though the previous
  // one was displayed twice; the eventual drift reset will correct this (tested
  // by BestFrameByCadenceOverdisplayedForDrift below).
  size_t frames_dropped = 0;
  scoped_refptr<VideoFrame> frame = RenderAndStep(&display_tg, &frames_dropped);
  ASSERT_TRUE(frame);
  EXPECT_EQ(frame_tg.interval(2), frame->timestamp());
  EXPECT_EQ(0u, frames_dropped);

  // Enqueuing a new frame should keep the correct cadence values.
  algorithm_.EnqueueFrame(CreateFrame(frame_tg.interval(4)));

  ASSERT_EQ(1, GetCurrentFrameDisplayCount());
  ASSERT_EQ(0, GetCurrentFrameDropCount());
  ASSERT_EQ(2, GetCurrentFrameIdealDisplayCount());
}

TEST_F(VideoRendererAlgorithmTest, BestFrameByCadenceOverdisplayedForDrift) {
  // Use 24.94 to ensure drift expires pretty rapidly (8.36s in this case).
  TickGenerator frame_tg(base::TimeTicks(), 24.94);
  TickGenerator display_tg(tick_clock_->NowTicks(), 50);
  time_source_.StartTicking();
  disable_cadence_hysteresis();

  scoped_refptr<VideoFrame> last_frame;
  bool have_overdisplayed_frame = false;
  while (!have_overdisplayed_frame) {
    while (EffectiveFramesQueued() < 2) {
      algorithm_.EnqueueFrame(
          CreateFrame(frame_tg.current() - base::TimeTicks()));
      frame_tg.step();
    }

    size_t frames_dropped = 0;
    last_frame = RenderAndStep(&display_tg, &frames_dropped);
    ASSERT_TRUE(last_frame);
    ASSERT_TRUE(is_using_cadence());
    ASSERT_EQ(0u, frames_dropped);
    ASSERT_EQ(2, GetCurrentFrameIdealDisplayCount());
    have_overdisplayed_frame = GetCurrentFrameDisplayCount() > 2;
  }

  ASSERT_TRUE(last_render_had_glitch());

  // We've reached the point where the current frame is over displayed due to
  // drift, the next frame should resume cadence without accounting for the
  // overdisplayed frame.

  size_t frames_dropped = 0;
  scoped_refptr<VideoFrame> next_frame =
      RenderAndStep(&display_tg, &frames_dropped);
  ASSERT_EQ(0u, frames_dropped);
  ASSERT_NE(last_frame, next_frame);
  ASSERT_TRUE(is_using_cadence());
  ASSERT_EQ(2, GetCurrentFrameIdealDisplayCount());
  ASSERT_EQ(1, GetCurrentFrameDisplayCount());
  last_frame = next_frame;

  next_frame = RenderAndStep(&display_tg, &frames_dropped);
  ASSERT_EQ(0u, frames_dropped);
  ASSERT_EQ(last_frame, next_frame);
  ASSERT_TRUE(is_using_cadence());
  ASSERT_EQ(2, GetCurrentFrameIdealDisplayCount());
  ASSERT_EQ(2, GetCurrentFrameDisplayCount());
}

TEST_F(VideoRendererAlgorithmTest, BestFrameByCoverage) {
  TickGenerator tg(tick_clock_->NowTicks(), 50);
  time_source_.StartTicking();

  algorithm_.EnqueueFrame(CreateFrame(tg.interval(0)));
  algorithm_.EnqueueFrame(CreateFrame(tg.interval(1)));
  algorithm_.EnqueueFrame(CreateFrame(tg.interval(2)));

  base::TimeTicks deadline_min = tg.current();
  base::TimeTicks deadline_max = deadline_min + tg.interval(1);

  size_t frames_dropped = 0;
  scoped_refptr<VideoFrame> frame =
      algorithm_.Render(deadline_min, deadline_max, &frames_dropped);
  ASSERT_TRUE(frame);
  EXPECT_EQ(tg.interval(0), frame->timestamp());
  EXPECT_EQ(0u, frames_dropped);

  int second_best = 0;

  // Coverage should be 1 for if the frame overlaps the interval entirely, no
  // second best should be found.
  EXPECT_EQ(0,
            FindBestFrameByCoverage(deadline_min, deadline_max, &second_best));
  EXPECT_EQ(-1, second_best);

  // 49/51 coverage for frame 0 and frame 1 should be within tolerance such that
  // the earlier frame should still be chosen.
  deadline_min = tg.current() + tg.interval(1) / 2 + base::Microseconds(250);
  deadline_max = deadline_min + tg.interval(1);
  EXPECT_EQ(0,
            FindBestFrameByCoverage(deadline_min, deadline_max, &second_best));
  EXPECT_EQ(1, second_best);

  // 48/52 coverage should result in the second frame being chosen.
  deadline_min = tg.current() + tg.interval(1) / 2 + base::Microseconds(500);
  deadline_max = deadline_min + tg.interval(1);
  EXPECT_EQ(1,
            FindBestFrameByCoverage(deadline_min, deadline_max, &second_best));
  EXPECT_EQ(0, second_best);

  // Overlapping three frames should choose the one with the most coverage and
  // the second best should be the earliest frame.
  deadline_min = tg.current() + tg.interval(1) / 2;
  deadline_max = deadline_min + tg.interval(2);
  EXPECT_EQ(1,
            FindBestFrameByCoverage(deadline_min, deadline_max, &second_best));
  EXPECT_EQ(0, second_best);

  // Requesting coverage outside of all known frames should return -1 for both
  // best indices.
  deadline_min = tg.current() + tg.interval(frames_queued());
  deadline_max = deadline_min + tg.interval(1);
  EXPECT_EQ(-1,
            FindBestFrameByCoverage(deadline_min, deadline_max, &second_best));
  EXPECT_EQ(-1, second_best);
}

TEST_F(VideoRendererAlgorithmTest, BestFrameByDriftAndDriftCalculations) {
  TickGenerator tg(tick_clock_->NowTicks(), 50);
  time_source_.StartTicking();

  algorithm_.EnqueueFrame(CreateFrame(tg.interval(0)));
  algorithm_.EnqueueFrame(CreateFrame(tg.interval(1)));

  size_t frames_dropped = 0;
  scoped_refptr<VideoFrame> frame = algorithm_.Render(
      tg.current(), tg.current() + tg.interval(1), &frames_dropped);
  ASSERT_TRUE(frame);
  EXPECT_EQ(tg.interval(0), frame->timestamp());
  EXPECT_EQ(0u, frames_dropped);

  base::TimeDelta zero_drift, half_drift = tg.interval(1) / 2;
  base::TimeDelta detected_drift;

  // Frame_0 overlaps the deadline, Frame_1 is a full interval away.
  base::TimeTicks deadline = tg.current();
  EXPECT_EQ(zero_drift, CalculateAbsoluteDriftForFrame(deadline, 0));
  EXPECT_EQ(tg.interval(1), CalculateAbsoluteDriftForFrame(deadline, 1));
  EXPECT_EQ(0, FindBestFrameByDrift(deadline, &detected_drift));
  EXPECT_EQ(zero_drift, detected_drift);

  // Frame_0 overlaps the deadline, Frame_1 is a half interval away.
  deadline += half_drift;
  EXPECT_EQ(zero_drift, CalculateAbsoluteDriftForFrame(deadline, 0));
  EXPECT_EQ(half_drift, CalculateAbsoluteDriftForFrame(deadline, 1));
  EXPECT_EQ(0, FindBestFrameByDrift(deadline, &detected_drift));
  EXPECT_EQ(zero_drift, detected_drift);

  // Both frames overlap the deadline.
  deadline += half_drift;
  EXPECT_EQ(zero_drift, CalculateAbsoluteDriftForFrame(deadline, 0));
  EXPECT_EQ(zero_drift, CalculateAbsoluteDriftForFrame(deadline, 1));
  EXPECT_EQ(1, FindBestFrameByDrift(deadline, &detected_drift));
  EXPECT_EQ(zero_drift, detected_drift);

  // Frame_0 is half an interval away, Frame_1 overlaps the deadline.
  deadline += half_drift;
  EXPECT_EQ(half_drift, CalculateAbsoluteDriftForFrame(deadline, 0));
  EXPECT_EQ(zero_drift, CalculateAbsoluteDriftForFrame(deadline, 1));
  EXPECT_EQ(1, FindBestFrameByDrift(deadline, &detected_drift));
  EXPECT_EQ(zero_drift, detected_drift);

  // Frame_0 is a full interval away, Frame_1 overlaps the deadline.
  deadline += half_drift;
  EXPECT_EQ(tg.interval(1), CalculateAbsoluteDriftForFrame(deadline, 0));
  EXPECT_EQ(zero_drift, CalculateAbsoluteDriftForFrame(deadline, 1));
  EXPECT_EQ(1, FindBestFrameByDrift(deadline, &detected_drift));
  EXPECT_EQ(zero_drift, detected_drift);

  // Both frames are entirely before the deadline.
  deadline += half_drift;
  EXPECT_EQ(tg.interval(1) + half_drift,
            CalculateAbsoluteDriftForFrame(deadline, 0));
  EXPECT_EQ(half_drift, CalculateAbsoluteDriftForFrame(deadline, 1));
  EXPECT_EQ(1, FindBestFrameByDrift(deadline, &detected_drift));
  EXPECT_EQ(half_drift, detected_drift);
}

// Run through fractional cadence selection for 1/2, 1/3, and 1/4.
TEST_F(VideoRendererAlgorithmTest, BestFrameByFractionalCadence) {
  const double kTestRates[][2] = {{120, 60}, {72, 24}, {100, 25}};

  for (const auto& test_rate : kTestRates) {
    disable_cadence_hysteresis();

    TickGenerator frame_tg(base::TimeTicks(), test_rate[0]);
    TickGenerator display_tg(tick_clock_->NowTicks(), test_rate[1]);

    scoped_refptr<VideoFrame> current_frame;
    RunFramePumpTest(true, &frame_tg, &display_tg,
                     [&current_frame, this](scoped_refptr<VideoFrame> frame,
                                            size_t frames_dropped) {
                       ASSERT_TRUE(frame);

                       // We don't count frames dropped that cadence says we
                       // should skip.
                       ASSERT_EQ(0u, frames_dropped);
                       ASSERT_NE(current_frame, frame);
                       ASSERT_TRUE(is_using_cadence());
                       current_frame = frame;
                     });

    if (HasFatalFailure())
      return;
  }
}

// Verify a 3:2 frame pattern for 23.974fps and 24fps in 60Hz.
TEST_F(VideoRendererAlgorithmTest, FilmCadence) {
  const double kTestRates[] = {NTSC(24), 24};
  disable_cadence_hysteresis();

  for (double frame_rate : kTestRates) {
    scoped_refptr<VideoFrame> current_frame;
    int actual_frame_pattern = 0, desired_frame_pattern = 3;

    TickGenerator frame_tg(base::TimeTicks(), frame_rate);
    TickGenerator display_tg(tick_clock_->NowTicks(), 60);

    RunFramePumpTest(
        true, &frame_tg, &display_tg,
        [&current_frame, &actual_frame_pattern, &desired_frame_pattern, this](
            scoped_refptr<VideoFrame> frame, size_t frames_dropped) {
          ASSERT_TRUE(frame);
          ASSERT_EQ(0u, frames_dropped);

          if (!current_frame || current_frame == frame) {
            actual_frame_pattern++;
          } else {
            ASSERT_EQ(actual_frame_pattern, desired_frame_pattern);
            actual_frame_pattern = 1;
            desired_frame_pattern = (desired_frame_pattern == 3 ? 2 : 3);
          }

          current_frame = frame;
          ASSERT_TRUE(is_using_cadence());
        });

    if (HasFatalFailure())
      return;
  }
}

// Spot check common display and frame rate pairs for correctness.
TEST_F(VideoRendererAlgorithmTest, CadenceCalculations) {
  ASSERT_EQ("[3:2]", GetCadence(24, 60));
  ASSERT_EQ("[3:2]", GetCadence(NTSC(24), 60));
  ASSERT_EQ("[2:3:2:3:2]", GetCadence(25, 60));
  ASSERT_EQ("[2]", GetCadence(NTSC(30), 60));
  ASSERT_EQ("[2]", GetCadence(30, 60));
  ASSERT_EQ("[1:1:2:1:1]", GetCadence(50, 60));
  ASSERT_EQ("[1]", GetCadence(NTSC(60), 60));
  ASSERT_EQ("[1:0]", GetCadence(120, 60));

  // 50Hz is common in the EU.
  ASSERT_EQ("[]", GetCadence(NTSC(24), 50));
  ASSERT_EQ("[]", GetCadence(24, 50));
  ASSERT_EQ("[2]", GetCadence(NTSC(25), 50));
  ASSERT_EQ("[2]", GetCadence(25, 50));
  ASSERT_EQ("[2:1:2]", GetCadence(NTSC(30), 50));
  ASSERT_EQ("[2:1:2]", GetCadence(30, 50));
  ASSERT_EQ("[]", GetCadence(NTSC(60), 50));
  ASSERT_EQ("[]", GetCadence(60, 50));

  ASSERT_EQ("[2:3:2:3:2]", GetCadence(25, NTSC(60)));
  ASSERT_EQ("[1:0]", GetCadence(120, NTSC(60)));
  ASSERT_EQ("[60]", GetCadence(1, NTSC(60)));
}

TEST_F(VideoRendererAlgorithmTest, RemoveExpiredFramesWithoutRendering) {
  TickGenerator tg(tick_clock_->NowTicks(), 50);

  // Removing expired frames before anything is enqueued should do nothing.
  ASSERT_EQ(0u, algorithm_.RemoveExpiredFrames(tg.current()));

  // First verify that frames without a duration are always effective when only
  // one frame is present in the queue.
  algorithm_.EnqueueFrame(CreateFrame(tg.interval(0)));
  ASSERT_EQ(0u, algorithm_.RemoveExpiredFrames(tg.current()));
  EXPECT_EQ(1u, EffectiveFramesQueued());

  ASSERT_EQ(0u, algorithm_.RemoveExpiredFrames(tg.current() + tg.interval(3)));
  EXPECT_EQ(1u, EffectiveFramesQueued());

  algorithm_.Reset();

  // Now try a frame with duration information, this frame should not be counted
  // as effective since we know the duration of it. It is not removed since we
  // only have one frame in the queue though.
  auto frame = CreateFrame(tg.interval(0));
  frame->metadata().frame_duration = tg.interval(1);
  algorithm_.EnqueueFrame(frame);
  ASSERT_EQ(0u, algorithm_.RemoveExpiredFrames(tg.current() + tg.interval(3)));
  EXPECT_EQ(0u, EffectiveFramesQueued());
}

TEST_F(VideoRendererAlgorithmTest, RemoveExpiredFrames) {
  TickGenerator tg(tick_clock_->NowTicks(), 50);

  // Removing expired frames before anything is enqueued should do nothing.
  ASSERT_EQ(0u, algorithm_.RemoveExpiredFrames(tg.current()));

  algorithm_.EnqueueFrame(CreateFrame(tg.interval(0)));
  ASSERT_EQ(0u, algorithm_.RemoveExpiredFrames(tg.current()));
  EXPECT_EQ(1u, EffectiveFramesQueued());

  time_source_.StartTicking();

  size_t frames_dropped = 0;
  scoped_refptr<VideoFrame> frame = RenderAndStep(&tg, &frames_dropped);
  ASSERT_TRUE(frame);
  EXPECT_EQ(tg.interval(0), frame->timestamp());
  EXPECT_EQ(0u, frames_dropped);

  algorithm_.EnqueueFrame(CreateFrame(tg.interval(1)));
  algorithm_.EnqueueFrame(CreateFrame(tg.interval(2)));
  algorithm_.EnqueueFrame(CreateFrame(tg.interval(3)));
  algorithm_.EnqueueFrame(CreateFrame(tg.interval(4)));
  EXPECT_EQ(5u, EffectiveFramesQueued());

  tg.step(2);
  // Two frames are removed, one displayed frame (which should not be counted as
  // dropped) and one undisplayed one.
  ASSERT_EQ(2u, algorithm_.RemoveExpiredFrames(tg.current()));
  // Since we just removed the last rendered frame, OnLastFrameDropped() should
  // be ignored.
  algorithm_.OnLastFrameDropped();
  frame = RenderAndStep(&tg, &frames_dropped);
  EXPECT_EQ(0u, frames_dropped);
  EXPECT_EQ(2u, frames_queued());
  EXPECT_EQ(1u, EffectiveFramesQueued());
  ASSERT_TRUE(frame);
  EXPECT_EQ(tg.interval(3), frame->timestamp());

  // Advance expiry enough that one frame is removed, but one remains and is
  // still counted as effective; the expired frame was displayed so it is not
  // counted as dropped.
  ASSERT_EQ(
      0u, algorithm_.RemoveExpiredFrames(tg.current() + tg.interval(1) * 0.9));
  EXPECT_EQ(1u, frames_queued());
  EXPECT_EQ(1u, EffectiveFramesQueued());

  // Advancing expiry once more should mark the frame as ineffective.
  tg.step();
  ASSERT_EQ(0u, algorithm_.RemoveExpiredFrames(tg.current()));
  EXPECT_EQ(1u, frames_queued());
  EXPECT_EQ(0u, EffectiveFramesQueued());
}

TEST_F(VideoRendererAlgorithmTest, RemoveExpiredFramesPartialReset) {
  TickGenerator tg(tick_clock_->NowTicks(), 50);

  algorithm_.EnqueueFrame(CreateFrame(tg.interval(0)));
  algorithm_.EnqueueFrame(CreateFrame(tg.interval(1)));
  ASSERT_EQ(0u, algorithm_.RemoveExpiredFrames(tg.current()));
  EXPECT_EQ(2u, EffectiveFramesQueued());

  time_source_.StartTicking();

  // Render such that the next enqueued frame should be counting as expired.
  for (int i = 0; i < 3; ++i) {
    size_t frames_dropped = 0;
    scoped_refptr<VideoFrame> frame = RenderAndStep(&tg, &frames_dropped);
    ASSERT_TRUE(frame);
    EXPECT_EQ(tg.interval(std::min(i, 1)), frame->timestamp());
    EXPECT_EQ(0u, frames_dropped);
  }

  time_source_.StopTicking();
  algorithm_.Reset(
      VideoRendererAlgorithm::ResetFlag::kPreserveNextFrameEstimates);
  // Skip ahead several frames to ensure EnqueueFrame() estimates correctly.
  algorithm_.EnqueueFrame(CreateFrame(tg.interval(5)));
  EXPECT_EQ(1u, EffectiveFramesQueued());
  ASSERT_EQ(0u, algorithm_.RemoveExpiredFrames(tg.current()));
  EXPECT_EQ(1u, EffectiveFramesQueued());
}

TEST_F(VideoRendererAlgorithmTest, RemoveExpiredFramesCadence) {
  TickGenerator tg(tick_clock_->NowTicks(), 50);
  disable_cadence_hysteresis();

  algorithm_.EnqueueFrame(CreateFrame(tg.interval(0)));
  algorithm_.EnqueueFrame(CreateFrame(tg.interval(1)));
  algorithm_.EnqueueFrame(CreateFrame(tg.interval(2)));

  ASSERT_EQ(0u, algorithm_.RemoveExpiredFrames(tg.current()));
  EXPECT_EQ(3u, EffectiveFramesQueued());

  time_source_.StartTicking();

  size_t frames_dropped = 0;
  scoped_refptr<VideoFrame> frame = RenderAndStep(&tg, &frames_dropped);
  ASSERT_TRUE(frame);
  EXPECT_EQ(tg.interval(0), frame->timestamp());
  EXPECT_EQ(0u, frames_dropped);
  ASSERT_TRUE(is_using_cadence());
  EXPECT_EQ(2u, EffectiveFramesQueued());

  // Advance expiry enough that some frames are removed, but one remains and is
  // still counted as effective.  1 undisplayed and 1 displayed frame will be
  // expired.
  ASSERT_EQ(1u, algorithm_.RemoveExpiredFrames(tg.current() + tg.interval(1) +
                                               max_acceptable_drift() * 1.25));
  EXPECT_EQ(1u, frames_queued());
  EXPECT_EQ(1u, EffectiveFramesQueued());

  // Advancing expiry once more should mark the frame as ineffective.
  tg.step(3);
  ASSERT_EQ(0u, algorithm_.RemoveExpiredFrames(tg.current()));
  EXPECT_EQ(1u, frames_queued());
  EXPECT_EQ(0u, EffectiveFramesQueued());
}

TEST_F(VideoRendererAlgorithmTest, RemoveExpiredFramesFractionalCadence) {
  TickGenerator frame_tg(base::TimeTicks(), 60);
  TickGenerator display_tg(tick_clock_->NowTicks(), 30);
  disable_cadence_hysteresis();

  constexpr size_t kFrameCount = 5;
  for (size_t i = 0; i < kFrameCount; ++i)
    algorithm_.EnqueueFrame(CreateFrame(frame_tg.interval(i)));

  ASSERT_EQ(0u, algorithm_.RemoveExpiredFrames(display_tg.current()));
  EXPECT_EQ(kFrameCount, EffectiveFramesQueued());

  time_source_.StartTicking();

  size_t frames_dropped = 0;
  scoped_refptr<VideoFrame> frame = RenderAndStep(&display_tg, &frames_dropped);
  ASSERT_TRUE(frame);
  EXPECT_EQ(frame_tg.interval(0), frame->timestamp());
  EXPECT_EQ(0u, frames_dropped);
  ASSERT_TRUE(is_using_cadence());
  EXPECT_EQ((kFrameCount - 1) / 2, EffectiveFramesQueued());
  EXPECT_EQ(kFrameCount, frames_queued());

  // Advance expiry enough that some frames are removed, but one remains and is
  // still counted as effective.  1 undisplayed and 1 displayed frame will be
  // expired.
  ASSERT_EQ(1u, algorithm_.RemoveExpiredFrames(display_tg.current() +
                                               display_tg.interval(1) +
                                               max_acceptable_drift() * 1.25));
  EXPECT_EQ(1u, frames_queued());
  EXPECT_EQ(1u, EffectiveFramesQueued());

  // Advancing expiry once more should mark the frame as ineffective.
  display_tg.step(3);
  ASSERT_EQ(0u, algorithm_.RemoveExpiredFrames(display_tg.current()));
  EXPECT_EQ(1u, frames_queued());
  EXPECT_EQ(0u, EffectiveFramesQueued());
}

class VideoRendererAlgorithmCadenceTest
    : public VideoRendererAlgorithmTest,
      public ::testing::WithParamInterface<::testing::tuple<double, double>> {};

TEST_P(VideoRendererAlgorithmCadenceTest, CadenceTest) {
  double display_rate = std::get<0>(GetParam());
  double frame_rate = std::get<1>(GetParam());

  TickGenerator frame_tg(base::TimeTicks(), frame_rate);
  TickGenerator display_tg(tick_clock_->NowTicks(), display_rate);
  RunFramePumpTest(
      true, &frame_tg, &display_tg,
      [](scoped_refptr<VideoFrame> frame, size_t frames_dropped) {});
}

// Common display rates.
const double kDisplayRates[] = {
    NTSC(24), 24, NTSC(25), 25, NTSC(30), 30,  48,
    NTSC(50), 50, NTSC(60), 60, 75,       120, 144,
};

// List of common frame rate values. Values pulled from local test media,
// videostack test matrix, and Wikipedia.
const double kTestRates[] = {
    1,        10, 12.5,  15,  NTSC(24), 24,  NTSC(25), 25,
    NTSC(30), 30, 30.12, 48,  NTSC(50), 50,  58.74,    NTSC(60),
    60,       72, 90,    100, 120,      144, 240,      300,
};

INSTANTIATE_TEST_SUITE_P(All,
                         VideoRendererAlgorithmCadenceTest,
                         ::testing::Combine(::testing::ValuesIn(kDisplayRates),
                                            ::testing::ValuesIn(kTestRates)));

// Rotate through various playback rates and ensure algorithm adapts correctly.
TEST_F(VideoRendererAlgorithmTest, VariablePlaybackRateCadence) {
  TickGenerator frame_tg(base::TimeTicks(), NTSC(30));
  TickGenerator display_tg(tick_clock_->NowTicks(), 60);

  const double kPlaybackRates[] = {1.0, 2, 0.215, 0.5, 1.0, 3.15};
  const bool kTestRateHasCadence[std::size(kPlaybackRates)] = {
      true, true, true, true, true, false};

  for (size_t i = 0; i < std::size(kPlaybackRates); ++i) {
    const double playback_rate = kPlaybackRates[i];
    SCOPED_TRACE(base::StringPrintf("Playback Rate: %.03f", playback_rate));
    time_source_.SetPlaybackRate(playback_rate);
    RunFramePumpTest(
        false, &frame_tg, &display_tg,
        [](scoped_refptr<VideoFrame> frame, size_t frames_dropped) {});
    if (HasFatalFailure())
      return;

    ASSERT_EQ(kTestRateHasCadence[i], is_using_cadence());
  }

  // TODO(dalecurtis): Is there more we can test here?
}

// Ensures media which only expresses timestamps in milliseconds, gets the right
// cadence detection.
TEST_F(VideoRendererAlgorithmTest, UglyTimestampsHaveCadence) {
  TickGenerator display_tg(tick_clock_->NowTicks(), 60);
  time_source_.StartTicking();

  // 59.94fps, timestamp deltas from https://youtu.be/byoLvAo9qjs
  const int kBadTimestampsMs[] = {
      17, 16, 17, 17, 16, 17, 17, 16, 17, 17, 17, 16, 17, 17, 16, 17, 17, 16,
      17, 17, 16, 17, 17, 16, 17, 17, 16, 17, 17, 17, 16, 17, 17, 16, 17, 17,
      16, 17, 17, 16, 17, 17, 16, 17, 17, 16, 17, 17, 16, 17, 17, 17};

  // Run through ~1.6 seconds worth of frames.
  bool cadence_detected = false;
  base::TimeDelta timestamp;
  for (size_t i = 0; i < std::size(kBadTimestampsMs) * 2; ++i) {
    while (EffectiveFramesQueued() < 3) {
      algorithm_.EnqueueFrame(CreateFrame(timestamp));
      timestamp +=
          base::Milliseconds(kBadTimestampsMs[i % std::size(kBadTimestampsMs)]);
    }

    size_t frames_dropped = 0;
    RenderAndStep(&display_tg, &frames_dropped);
    ASSERT_EQ(0u, frames_dropped);

    // Cadence won't be detected immediately on this clip, but it will after
    // enough frames are encountered; after which it should not drop out of
    // cadence.
    if (is_using_cadence())
      cadence_detected = true;

    if (cadence_detected)
      ASSERT_TRUE(is_using_cadence());
  }
}

// Ensures media with variable frame rate should not be applied with Cadence.
TEST_F(VideoRendererAlgorithmTest, VariableFrameRateNoCadence) {
  TickGenerator display_tg(tick_clock_->NowTicks(), 60);
  time_source_.StartTicking();

  const int kBadTimestampsMs[] = {200,  200,  200,  200,  200,  1000,
                                  1000, 1000, 1000, 200,  200,  200,
                                  200,  200,  1000, 1000, 1000, 1000};

  // Run throught ~10 seconds worth of frames.
  bool cadence_detected = false;
  bool cadence_turned_off = false;
  base::TimeDelta timestamp;
  for (size_t i = 0; i < std::size(kBadTimestampsMs);) {
    while (EffectiveFramesQueued() < 3) {
      algorithm_.EnqueueFrame(CreateFrame(timestamp));
      timestamp +=
          base::Milliseconds(kBadTimestampsMs[i % std::size(kBadTimestampsMs)]);
      ++i;
    }

    size_t frames_dropped = 0;
    RenderAndStep(&display_tg, &frames_dropped);
    ASSERT_EQ(0u, frames_dropped);

    // Cadence would be detected during the first second, and then
    // it should be off due to variable FPS detection, and then for this
    // sample, it should never be on.
    if (is_using_cadence())
      cadence_detected = true;

    if (cadence_detected) {
      if (!is_using_cadence())
        cadence_turned_off = true;
    }

    if (cadence_turned_off) {
      ASSERT_FALSE(is_using_cadence());
    }
  }

  // Make sure Cadence is turned off somewhen, not always on.
  ASSERT_TRUE(cadence_turned_off);
}

TEST_F(VideoRendererAlgorithmTest, EnqueueFrames) {
  TickGenerator tg(base::TimeTicks(), 50);
  time_source_.StartTicking();

  EXPECT_EQ(0u, frames_queued());
  scoped_refptr<VideoFrame> frame_1 = CreateFrame(tg.interval(0));
  algorithm_.EnqueueFrame(frame_1);
  EXPECT_EQ(1u, frames_queued());

  // Enqueuing a frame with the same timestamp should always be dropped.
  scoped_refptr<VideoFrame> frame_2 = CreateFrame(tg.interval(0));
  algorithm_.EnqueueFrame(frame_2);
  EXPECT_EQ(1u, frames_queued());

  size_t frames_dropped = 0;
  scoped_refptr<VideoFrame> rendered_frame =
      RenderAndStep(&tg, &frames_dropped);
  EXPECT_EQ(1u, frames_queued());
  EXPECT_EQ(frame_1, rendered_frame);
  EXPECT_EQ(1, GetCurrentFrameDisplayCount());

  // The replaced frame should count as dropped.
  EXPECT_EQ(1u, frames_dropped);

  // Trying to replace frame_1 with frame_2 should do nothing.
  algorithm_.EnqueueFrame(frame_2);
  EXPECT_EQ(1u, frames_queued());

  rendered_frame = RenderAndStep(&tg, &frames_dropped);
  EXPECT_EQ(1u, frames_queued());
  EXPECT_EQ(frame_1, rendered_frame);
  EXPECT_EQ(1u, frames_dropped);
  EXPECT_EQ(2, GetCurrentFrameDisplayCount());

  // Trying to add a frame < 1 ms after the last frame should drop the frame.
  algorithm_.EnqueueFrame(CreateFrame(base::Microseconds(999)));
  rendered_frame = RenderAndStep(&tg, &frames_dropped);
  EXPECT_EQ(1u, frames_queued());
  EXPECT_EQ(frame_1, rendered_frame);
  EXPECT_EQ(1u, frames_dropped);
  EXPECT_EQ(3, GetCurrentFrameDisplayCount());

  scoped_refptr<VideoFrame> frame_3 = CreateFrame(tg.interval(1));
  algorithm_.EnqueueFrame(frame_3);
  EXPECT_EQ(2u, frames_queued());

  // Trying to add a frame < 1 ms before the last frame should drop the frame.
  algorithm_.EnqueueFrame(
      CreateFrame(tg.interval(1) - base::Microseconds(999)));
  rendered_frame = RenderAndStep(&tg, &frames_dropped);
  EXPECT_EQ(1u, frames_queued());
  EXPECT_EQ(frame_3, rendered_frame);
  EXPECT_EQ(1u, frames_dropped);
  EXPECT_EQ(1, GetCurrentFrameDisplayCount());
}

TEST_F(VideoRendererAlgorithmTest, CadenceForFutureFrames) {
  TickGenerator tg(base::TimeTicks(), 50);
  time_source_.StartTicking();

  disable_cadence_hysteresis();

  algorithm_.EnqueueFrame(CreateFrame(tg.interval(10)));
  algorithm_.EnqueueFrame(CreateFrame(tg.interval(11)));
  algorithm_.EnqueueFrame(CreateFrame(tg.interval(12)));
  EXPECT_EQ(3u, frames_queued());

  // Call Render() a few times to increment the render count.
  for (int i = 0; i < 10; ++i) {
    size_t frames_dropped = 0;
    scoped_refptr<VideoFrame> rendered_frame =
        RenderAndStep(&tg, &frames_dropped);
    EXPECT_EQ(3u, frames_queued());
    EXPECT_EQ(tg.interval(10), rendered_frame->timestamp());
    ASSERT_TRUE(is_using_cadence());
  }

  // Add some noise to the tick generator so it our first frame
  // doesn't line up evenly on a deadline.
  tg.Reset(tg.current() + base::Milliseconds(5));

  // We're now at the first frame, cadence should be one, so
  // it should only be displayed once.
  size_t frames_dropped = 0;
  scoped_refptr<VideoFrame> rendered_frame =
      RenderAndStep(&tg, &frames_dropped);
  EXPECT_EQ(3u, frames_queued());
  EXPECT_EQ(tg.interval(10), rendered_frame->timestamp());
  ASSERT_TRUE(is_using_cadence());

  // Then the next frame should be displayed.
  rendered_frame = RenderAndStep(&tg, &frames_dropped);
  EXPECT_EQ(2u, frames_queued());
  EXPECT_EQ(tg.interval(11), rendered_frame->timestamp());
  ASSERT_TRUE(is_using_cadence());

  // Finally the last frame.
  rendered_frame = RenderAndStep(&tg, &frames_dropped);
  EXPECT_EQ(1u, frames_queued());
  EXPECT_EQ(tg.interval(12), rendered_frame->timestamp());
  ASSERT_TRUE(is_using_cadence());
}

TEST_F(VideoRendererAlgorithmTest, InfiniteDurationMetadata) {
  TickGenerator tg(tick_clock_->NowTicks(), 50);

  auto frame = CreateFrame(kInfiniteDuration);
  frame->metadata().frame_duration = tg.interval(1);
  algorithm_.EnqueueFrame(frame);

  // This should not crash or fail.
  size_t frames_dropped = 0;
  frame = RenderAndStep(&tg, &frames_dropped);
  EXPECT_TRUE(algorithm_.average_frame_duration().is_zero());
}

TEST_F(VideoRendererAlgorithmTest, UsesFrameDuration) {
  TickGenerator tg(tick_clock_->NowTicks(), 50);

  auto frame = CreateFrame(tg.interval(0));
  frame->metadata().frame_duration = tg.interval(1);
  algorithm_.EnqueueFrame(frame);

  // This should not crash or fail.
  size_t frames_dropped = 0;
  frame = RenderAndStep(&tg, &frames_dropped);
  EXPECT_EQ(tg.interval(1), algorithm_.average_frame_duration());

  // Add a bunch of normal frames and then one with a 3s duration.
  constexpr base::TimeDelta kLongDuration = base::Seconds(3);
  for (int i = 1; i < 4; ++i) {
    frame = CreateFrame(tg.interval(i));
    frame->metadata().frame_duration = i == 3 ? kLongDuration : tg.interval(1);
    algorithm_.EnqueueFrame(frame);
  }

  frame = RenderAndStep(&tg, &frames_dropped);
  EXPECT_EQ(tg.interval(1), algorithm_.average_frame_duration());
  EXPECT_EQ(algorithm_.last_frame_end_time(),
            base::TimeTicks() + kLongDuration + tg.interval(1) * 3);
}

// Check that VideoRendererAlgorithm correctly sets WALLCLOCK_FRAME_DURATION
// for each frame.
TEST_F(VideoRendererAlgorithmTest, WallClockDurationMetadataSet) {
  int playback_rate = 4;
  int frame_count = 10;
  TickGenerator tg(tick_clock_->NowTicks(), 25);

  time_source_.SetPlaybackRate(playback_rate);
  auto intended_duration = tg.interval(1) / playback_rate;

  for (int i = 0; i < frame_count; i++) {
    auto frame = CreateFrame(tg.interval(i));
    frame->metadata().frame_duration = tg.interval(1);
    algorithm_.EnqueueFrame(frame);
  }

  for (int i = 0; i < frame_count; i++) {
    size_t frames_dropped = 0;
    auto frame = RenderAndStep(&tg, &frames_dropped);

    SCOPED_TRACE(base::StringPrintf("Frame #%d", i));

    EXPECT_EQ(*frame->metadata().wallclock_frame_duration, intended_duration);
    EXPECT_EQ(algorithm_.average_frame_duration(), intended_duration);
  }
}

}  // namespace media
