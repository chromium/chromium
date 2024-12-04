// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/capture/content/animated_content_sampler.h"

#include <stddef.h>
#include <stdint.h>

#include <cmath>
#include <memory>
#include <utility>
#include <vector>

#include "base/logging.h"
#include "base/numerics/safe_conversions.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/rect.h"

namespace media {

namespace {

base::TimeTicks InitialTestTimeTicks() {
  return base::TimeTicks() + base::Seconds(1);
}

base::TimeDelta FpsAsPeriod(int frame_rate) {
  return base::Seconds(1) / frame_rate;
}

}  // namespace

class AnimatedContentSamplerTest : public ::testing::Test {
 public:
  AnimatedContentSamplerTest() = default;
  ~AnimatedContentSamplerTest() override = default;

  void SetUp() override {
    rand_seed_ = static_cast<uint32_t>(
        (InitialTestTimeTicks() - base::TimeTicks()).InMicroseconds());
    sampler_ = std::make_unique<AnimatedContentSampler>(GetMinCapturePeriod());
  }

 protected:
  // Overridden by subclass for parameterized tests.
  virtual base::TimeDelta GetMinCapturePeriod() const {
    return base::Seconds(1) / 30;
  }

  AnimatedContentSampler* sampler() const { return sampler_.get(); }

  int GetRandomInRange(int begin, int end) {
    const int len = end - begin;
    const int rand_offset = (len == 0) ? 0 : (NextRandomInt() % (end - begin));
    return begin + rand_offset;
  }

  gfx::Rect GetRandomDamageRect() {
    return gfx::Rect(0, 0, GetRandomInRange(1, 100), GetRandomInRange(1, 100));
  }

  gfx::Rect GetContentDamageRect() {
    // This must be distinct from anything GetRandomDamageRect() could return.
    return gfx::Rect(0, 0, 1280, 720);
  }

  // Directly inject an observation.  Only used to test
  // ElectMajorityDamageRect().
  void ObserveDamageRect(const gfx::Rect& damage_rect) {
    sampler_->observations_.push_back(
        AnimatedContentSampler::Observation(damage_rect, base::TimeTicks()));
  }

  gfx::Rect ElectMajorityDamageRect() const {
    return sampler_->ElectMajorityDamageRect();
  }

  static base::TimeDelta ComputeSamplingPeriod(
      base::TimeDelta detected_period,
      base::TimeDelta target_sampling_period,
      base::TimeDelta min_capture_period) {
    return AnimatedContentSampler::ComputeSamplingPeriod(
        detected_period, target_sampling_period, min_capture_period);
  }

 private:
  // Note: Not using base::RandInt() because it is horribly slow on debug
  // builds.  The following is a very simple, deterministic LCG:
  int NextRandomInt() {
    rand_seed_ = (1103515245 * rand_seed_ + 12345) % (1 << 31);
    return static_cast<int>(rand_seed_);
  }

  uint32_t rand_seed_;
  std::unique_ptr<AnimatedContentSampler> sampler_;
};

TEST_F(AnimatedContentSamplerTest, ElectsNoneFromZeroDamageRects) {
  EXPECT_EQ(gfx::Rect(), ElectMajorityDamageRect());
}

TEST_F(AnimatedContentSamplerTest, ElectsMajorityFromOneDamageRect) {
  const gfx::Rect the_one_rect(0, 0, 1, 1);
  ObserveDamageRect(the_one_rect);
  EXPECT_EQ(the_one_rect, ElectMajorityDamageRect());
}

TEST_F(AnimatedContentSamplerTest, ElectsNoneFromTwoDamageRectsOfSameArea) {
  const gfx::Rect one_rect(0, 0, 1, 1);
  const gfx::Rect another_rect(1, 1, 1, 1);
  ObserveDamageRect(one_rect);
  ObserveDamageRect(another_rect);
  EXPECT_EQ(gfx::Rect(), ElectMajorityDamageRect());
}

TEST_F(AnimatedContentSamplerTest, ElectsLargerOfTwoDamageRects_1) {
  const gfx::Rect one_rect(0, 0, 1, 1);
  const gfx::Rect another_rect(0, 0, 2, 2);
  ObserveDamageRect(one_rect);
  ObserveDamageRect(another_rect);
  EXPECT_EQ(another_rect, ElectMajorityDamageRect());
}

TEST_F(AnimatedContentSamplerTest, ElectsLargerOfTwoDamageRects_2) {
  const gfx::Rect one_rect(0, 0, 2, 2);
  const gfx::Rect another_rect(0, 0, 1, 1);
  ObserveDamageRect(one_rect);
  ObserveDamageRect(another_rect);
  EXPECT_EQ(one_rect, ElectMajorityDamageRect());
}

TEST_F(AnimatedContentSamplerTest, ElectsSameAsMooreDemonstration) {
  // A more complex sequence (from Moore's web site): Three different Rects with
  // the same area, but occurring a different number of times.  C should win the
  // vote.
  const gfx::Rect rect_a(0, 0, 1, 4);
  const gfx::Rect rect_b(1, 1, 4, 1);
  const gfx::Rect rect_c(2, 2, 2, 2);
  for (int i = 0; i < 3; ++i)
    ObserveDamageRect(rect_a);
  for (int i = 0; i < 2; ++i)
    ObserveDamageRect(rect_c);
  for (int i = 0; i < 2; ++i)
    ObserveDamageRect(rect_b);
  for (int i = 0; i < 3; ++i)
    ObserveDamageRect(rect_c);
  ObserveDamageRect(rect_b);
  for (int i = 0; i < 2; ++i)
    ObserveDamageRect(rect_c);
  EXPECT_EQ(rect_c, ElectMajorityDamageRect());
}

TEST_F(AnimatedContentSamplerTest, Elects24FpsVideoInsteadOf48FpsSpinner) {
  // Scenario: 24 FPS 720x480 Video versus 48 FPS 96x96 "Busy Spinner"
  const gfx::Rect video_rect(100, 100, 720, 480);
  const gfx::Rect spinner_rect(360, 0, 96, 96);
  for (int i = 0; i < 100; ++i) {
    // |video_rect| occurs once for every two |spinner_rect|.  Vary the order
    // of events between the two:
    ObserveDamageRect(video_rect);
    ObserveDamageRect(spinner_rect);
    ObserveDamageRect(spinner_rect);
    ObserveDamageRect(video_rect);
    ObserveDamageRect(spinner_rect);
    ObserveDamageRect(spinner_rect);
    ObserveDamageRect(spinner_rect);
    ObserveDamageRect(video_rect);
    ObserveDamageRect(spinner_rect);
    ObserveDamageRect(spinner_rect);
    ObserveDamageRect(video_rect);
    ObserveDamageRect(spinner_rect);
  }
  EXPECT_EQ(video_rect, ElectMajorityDamageRect());
}

TEST_F(AnimatedContentSamplerTest, TargetsSamplingPeriod) {
  struct Helper {
    static void RunTargetSamplingPeriodTest(int target_fps) {
      const base::TimeDelta min_capture_period = FpsAsPeriod(60);
      const base::TimeDelta target_sampling_period = FpsAsPeriod(target_fps);

      for (int content_fps = 1; content_fps <= 60; ++content_fps) {
        const base::TimeDelta content_period = FpsAsPeriod(content_fps);
        const base::TimeDelta sampling_period = ComputeSamplingPeriod(
            content_period, target_sampling_period, min_capture_period);
        if (content_period >= target_sampling_period) {
          ASSERT_EQ(content_period, sampling_period);
        } else {
          ASSERT_LE(min_capture_period, sampling_period);

          // Check that the sampling rate is as close (or closer) to the target
          // sampling rate than any integer-subsampling of the content frame
          // rate.
          const double absolute_diff =
              std::abs(1.0 / sampling_period.InSecondsF() - target_fps);
          const double fudge_for_acceptable_rounding_error = 0.005;
          for (double divisor = 1; divisor < 4; ++divisor) {
            SCOPED_TRACE(::testing::Message() << "target_fps=" << target_fps
                                              << ", content_fps=" << content_fps
                                              << ", divisor=" << divisor);
            ASSERT_GE(std::abs(content_fps / divisor - target_fps),
                      absolute_diff - fudge_for_acceptable_rounding_error);
          }
        }
      }
    }
  };

  for (int target_fps = 1; target_fps <= 60; ++target_fps)
    Helper::RunTargetSamplingPeriodTest(target_fps);
}

namespace {

// A test scenario for AnimatedContentSamplerParameterizedTest.
struct Scenario {
  base::TimeDelta vsync_interval;      // Reflects compositor's update rate.
  base::TimeDelta min_capture_period;  // Reflects maximum capture rate.
  base::TimeDelta content_period;      // Reflects content animation rate.
  base::TimeDelta target_sampling_period;

  Scenario(int compositor_frequency, int max_frame_rate, int content_frame_rate)
      : vsync_interval(FpsAsPeriod(compositor_frequency)),
        min_capture_period(FpsAsPeriod(max_frame_rate)),
        content_period(FpsAsPeriod(content_frame_rate)) {
    CHECK(content_period >= vsync_interval)
        << "Bad test params: Impossible to animate faster than the compositor.";
  }

  Scenario(int compositor_frequency,
           int max_frame_rate,
           int content_frame_rate,
           int target_sampling_rate)
      : vsync_interval(FpsAsPeriod(compositor_frequency)),
        min_capture_period(FpsAsPeriod(max_frame_rate)),
        content_period(FpsAsPeriod(content_frame_rate)),
        target_sampling_period(FpsAsPeriod(target_sampling_rate)) {
    CHECK(content_period >= vsync_interval)
        << "Bad test params: Impossible to animate faster than the compositor.";
  }
};

// Value printer for Scenario.
::std::ostream& operator<<(::std::ostream& os, const Scenario& s) {
  return os << "{ vsync_interval=" << s.vsync_interval.InMicroseconds()
            << ", min_capture_period=" << s.min_capture_period.InMicroseconds()
            << ", content_period=" << s.content_period.InMicroseconds() << " }";
}

}  // namespace

class AnimatedContentSamplerParameterizedTest
    : public AnimatedContentSamplerTest,
      public ::testing::WithParamInterface<Scenario> {
 public:
  AnimatedContentSamplerParameterizedTest()
      : count_dropped_frames_(0), count_sampled_frames_(0) {}
  virtual ~AnimatedContentSamplerParameterizedTest() = default;

  void SetUp() override {
    AnimatedContentSamplerTest::SetUp();
    sampler()->SetTargetSamplingPeriod(GetParam().target_sampling_period);
  }

 protected:
  typedef std::pair<gfx::Rect, base::TimeTicks> Event;

  base::TimeDelta GetMinCapturePeriod() const override {
    return GetParam().min_capture_period;
  }

  base::TimeDelta ComputeExpectedSamplingPeriod() const {
    return AnimatedContentSamplerTest::ComputeSamplingPeriod(
        GetParam().content_period, GetParam().target_sampling_period,
        GetParam().min_capture_period);
  }

  // Generate a sequence of events from the compositor pipeline.  The event
  // times will all be at compositor vsync boundaries.
  std::vector<Event> GenerateEventSequence(base::TimeTicks begin,
                                           base::TimeTicks end,
                                           bool include_content_frame_events,
                                           bool include_random_events,
                                           base::TimeTicks* next_begin_time) {
    DCHECK(GetParam().content_period >= GetParam().vsync_interval);
    base::TimeTicks next_content_time = begin;
    std::vector<Event> events;
    base::TimeTicks compositor_time;
    for (compositor_time = begin; compositor_time < end;
         compositor_time += GetParam().vsync_interval) {
      if (next_content_time <= compositor_time) {
        next_content_time += GetParam().content_period;
        if (include_content_frame_events) {
          events.push_back(Event(GetContentDamageRect(), compositor_time));
          continue;
        }
      }
      if (include_random_events && GetRandomInRange(0, 1) == 0) {
        events.push_back(Event(GetRandomDamageRect(), compositor_time));
      }
    }

    if (next_begin_time) {
      while (compositor_time < next_content_time)
        compositor_time += GetParam().vsync_interval;
      *next_begin_time = compositor_time;
    }

    DCHECK(!events.empty());
    return events;
  }

  // Feed |events| through the sampler, and detect whether the expected
  // lock-in/out transition occurs.  Also, track and measure the frame drop
  // ratio and check it against the expected drop rate.
  void RunEventSequence(const std::vector<Event> events,
                        bool was_detecting_before,
                        bool is_detecting_after,
                        bool simulate_pipeline_back_pressure,
                        const char* description) {
    SCOPED_TRACE(::testing::Message() << "Description: " << description);

    gfx::Rect first_detected_region;

    EXPECT_EQ(was_detecting_before, sampler()->HasProposal());
    bool has_detection_switched = false;
    bool has_detection_flip_flopped_once = false;
    ResetFrameCounters();
    for (auto i = events.begin(); i != events.end(); ++i) {
      sampler()->ConsiderPresentationEvent(i->first, i->second);

      // Detect when the sampler locks in/out, and that it stays that way for
      // all further iterations of this loop.  It is permissible for the lock-in
      // to flip-flop once, but no more than that.
      if (!has_detection_switched &&
          was_detecting_before != sampler()->HasProposal()) {
        has_detection_switched = true;
      } else if (has_detection_switched &&
                 is_detecting_after != sampler()->HasProposal()) {
        ASSERT_FALSE(has_detection_flip_flopped_once);
        has_detection_flip_flopped_once = true;
        has_detection_switched = false;
      }
      ASSERT_EQ(
          has_detection_switched ? is_detecting_after : was_detecting_before,
          sampler()->HasProposal());

      if (sampler()->HasProposal()) {
        // Make sure the sampler doesn't flip-flop and keep proposing sampling
        // based on locking into different regions.
        if (first_detected_region.IsEmpty()) {
          first_detected_region = sampler()->detected_region();
          ASSERT_FALSE(first_detected_region.IsEmpty());
        } else {
          EXPECT_EQ(first_detected_region, sampler()->detected_region());
        }

        if (simulate_pipeline_back_pressure && GetRandomInRange(0, 2) == 0)
          ClientCannotSampleFrame(*i);
        else
          ClientDoesWhatSamplerProposes(*i);
      } else {
        EXPECT_FALSE(sampler()->ShouldSample());
        if (!simulate_pipeline_back_pressure || GetRandomInRange(0, 2) == 1)
          sampler()->RecordSample(i->second);
      }
    }
    EXPECT_EQ(is_detecting_after, sampler()->HasProposal());
    ExpectFrameDropRatioIsCorrect();
  }

  void ResetFrameCounters() {
    count_dropped_frames_ = 0;
    count_sampled_frames_ = 0;
  }

  // Keep track what the sampler is proposing, and call RecordSample() if it
  // proposes sampling |event|.
  void ClientDoesWhatSamplerProposes(const Event& event) {
    if (sampler()->ShouldSample()) {
      EXPECT_EQ(GetContentDamageRect(), event.first);
      sampler()->RecordSample(sampler()->frame_timestamp());
      ++count_sampled_frames_;
    } else if (event.first == GetContentDamageRect()) {
      ++count_dropped_frames_;
    }
  }

  // RecordSample() is not called, but for testing, keep track of what the
  // sampler is proposing for |event|.
  void ClientCannotSampleFrame(const Event& event) {
    if (sampler()->ShouldSample()) {
      EXPECT_EQ(GetContentDamageRect(), event.first);
      ++count_sampled_frames_;
    } else if (event.first == GetContentDamageRect()) {
      ++count_dropped_frames_;
    }
  }

  // Confirm the AnimatedContentSampler is not dropping more frames than
  // expected, given current test parameters.
  void ExpectFrameDropRatioIsCorrect() {
    if (count_sampled_frames_ == 0) {
      EXPECT_EQ(0, count_dropped_frames_);
      return;
    }
    const double expected_sampling_ratio =
        GetParam().content_period / ComputeExpectedSamplingPeriod();
    const int total_frames = count_dropped_frames_ + count_sampled_frames_;
    EXPECT_NEAR(total_frames * expected_sampling_ratio, count_sampled_frames_,
                1.5);
    EXPECT_NEAR(total_frames * (1.0 - expected_sampling_ratio),
                count_dropped_frames_, 1.5);
  }

 private:
  // These counters only include the frames with the desired content.
  int count_dropped_frames_;
  int count_sampled_frames_;
};

// Tests that the implementation locks in/out of frames containing stable
// animated content, whether or not random events are also simultaneously
// present.
TEST_P(AnimatedContentSamplerParameterizedTest, DetectsAnimatedContent) {
  // |begin| refers to the start of an event sequence in terms of the
  // Compositor's clock.
  base::TimeTicks begin = InitialTestTimeTicks();

  // Provide random events and expect no lock-in.
  RunEventSequence(GenerateEventSequence(begin, begin + base::Seconds(5), false,
                                         true, &begin),
                   false, false, false,
                   "Provide random events and expect no lock-in.");
  if (HasFailure())
    return;

  // Provide content frame events with some random events mixed-in, and expect
  // the sampler to lock-in.
  RunEventSequence(
      GenerateEventSequence(begin, begin + base::Seconds(5), true, true,
                            &begin),
      false, true, false,
      "Provide content frame events with some random events mixed-in, and "
      "expect the sampler to lock-in.");
  if (HasFailure())
    return;

  // Continue providing content frame events without the random events mixed-in
  // and expect the lock-in to hold.
  RunEventSequence(
      GenerateEventSequence(begin, begin + base::Seconds(5), true, false,
                            &begin),
      true, true, false,
      "Continue providing content frame events without the random events "
      "mixed-in and expect the lock-in to hold.");
  if (HasFailure())
    return;

  // Continue providing just content frame events and expect the lock-in to
  // hold.  Also simulate the capture pipeline experiencing back pressure.
  RunEventSequence(
      GenerateEventSequence(begin, begin + base::Seconds(20), true, false,
                            &begin),
      true, true, true,
      "Continue providing just content frame events and expect the lock-in to "
      "hold.  Also simulate the capture pipeline experiencing back pressure.");
  if (HasFailure())
    return;

  // Provide a half-second of random events only, and expect the lock-in to be
  // broken.
  RunEventSequence(
      GenerateEventSequence(begin, begin + base::Milliseconds(500), false, true,
                            &begin),
      true, false, false,
      "Provide a half-second of random events only, and expect the lock-in to "
      "be broken.");
  if (HasFailure())
    return;

  // Now, go back to providing content frame events, and expect the sampler to
  // lock-in once again.
  RunEventSequence(
      GenerateEventSequence(begin, begin + base::Seconds(5), true, false,
                            &begin),
      false, true, false,
      "Now, go back to providing content frame events, and expect the sampler "
      "to lock-in once again.");
}

// Tests that AnimatedContentSampler won't lock in to, nor flip-flop between,
// two animations of the same pixel change rate.  VideoCaptureOracle should
// revert to using the SmoothEventSampler for these kinds of situations, as
// there is no "right answer" as to which animation to lock into.
TEST_P(AnimatedContentSamplerParameterizedTest,
       DoesNotLockInToTwoCompetingAnimations) {
  // Don't test when the event stream cannot indicate two separate content
  // animations under the current test parameters.
  if (GetParam().content_period < 2 * GetParam().vsync_interval)
    return;

  // Start the first animation and run for a bit, and expect the sampler to
  // lock-in.
  base::TimeTicks begin = InitialTestTimeTicks();
  RunEventSequence(
      GenerateEventSequence(begin, begin + base::Seconds(5), true, false,
                            &begin),
      false, true, false,
      "Start the first animation and run for a bit, and expect the sampler to "
      "lock-in.");
  if (HasFailure())
    return;

  // Now, keep the first animation and blend in a second animation of the same
  // size and frame rate, but at a different position.  This will should cause
  // the sampler to enter an "undetected" state since it's unclear which
  // animation should be locked into.
  std::vector<Event> first_animation_events = GenerateEventSequence(
      begin, begin + base::Seconds(20), true, false, &begin);
  gfx::Rect second_animation_rect(
      gfx::Point(0, GetContentDamageRect().height()),
      GetContentDamageRect().size());
  std::vector<Event> both_animations_events;
  base::TimeDelta second_animation_offset = GetParam().vsync_interval;
  for (std::vector<Event>::const_iterator i = first_animation_events.begin();
       i != first_animation_events.end(); ++i) {
    both_animations_events.push_back(*i);
    both_animations_events.push_back(
        Event(second_animation_rect, i->second + second_animation_offset));
  }
  RunEventSequence(
      both_animations_events, true, false, false,
      "Now, blend-in a second animation of the same size and frame rate, but "
      "at a different position.");
  if (HasFailure())
    return;

  // Now, run just the first animation, and expect the sampler to lock-in once
  // again.
  RunEventSequence(
      GenerateEventSequence(begin, begin + base::Seconds(5), true, false,
                            &begin),
      false, true, false,
      "Now, run just the first animation, and expect the sampler to lock-in "
      "once again.");
  if (HasFailure())
    return;

  // Now, blend in the second animation again, but it has half the frame rate of
  // the first animation and damage Rects with twice the area.  This will should
  // cause the sampler to enter an "undetected" state again.  This tests that
  // pixel-weighting is being accounted for in the sampler's logic.
  first_animation_events = GenerateEventSequence(
      begin, begin + base::Seconds(20), true, false, &begin);
  second_animation_rect.set_width(second_animation_rect.width() * 2);
  both_animations_events.clear();
  bool include_second_animation_frame = true;
  for (std::vector<Event>::const_iterator i = first_animation_events.begin();
       i != first_animation_events.end(); ++i) {
    both_animations_events.push_back(*i);
    if (include_second_animation_frame) {
      both_animations_events.push_back(
          Event(second_animation_rect, i->second + second_animation_offset));
    }
    include_second_animation_frame = !include_second_animation_frame;
  }
  RunEventSequence(
      both_animations_events, true, false, false,
      "Now, blend in the second animation again, but it has half the frame "
      "rate of the first animation and damage Rects with twice the area.");
}

// Tests that the frame timestamps are smooth; meaning, that when run through a
// simulated compositor, each frame is held displayed for the right number of
// v-sync intervals.
TEST_P(AnimatedContentSamplerParameterizedTest, FrameTimestampsAreSmooth) {
  // Generate 30 seconds of animated content events, run the events through
  // AnimatedContentSampler, and record all frame timestamps being proposed
  // once lock-in is continuous.
  const base::TimeTicks begin = InitialTestTimeTicks();
  std::vector<Event> events = GenerateEventSequence(
      begin, begin + base::Seconds(20), true, false, nullptr);
  typedef std::vector<base::TimeTicks> Timestamps;
  Timestamps frame_timestamps;
  for (std::vector<Event>::const_iterator i = events.begin(); i != events.end();
       ++i) {
    sampler()->ConsiderPresentationEvent(i->first, i->second);
    if (sampler()->HasProposal()) {
      if (sampler()->ShouldSample()) {
        frame_timestamps.push_back(sampler()->frame_timestamp());
        sampler()->RecordSample(sampler()->frame_timestamp());
      }
    } else {
      frame_timestamps.clear();  // Reset until continuous lock-in.
    }
  }
  ASSERT_LE(2u, frame_timestamps.size());

  // Iterate through the |frame_timestamps|, building a histogram counting the
  // number of times each frame was displayed k times.  For example, 10 frames
  // of 30 Hz content on a 60 Hz v-sync interval should result in
  // display_counts[2] == 10.  Quit early if any one frame was obviously
  // repeated too many times.
  const int64_t max_expected_repeats_per_frame =
      1 + ComputeExpectedSamplingPeriod().IntDiv(GetParam().vsync_interval);
  std::vector<size_t> display_counts(max_expected_repeats_per_frame + 1, 0);
  base::TimeTicks last_present_time = frame_timestamps.front();
  for (Timestamps::const_iterator i = frame_timestamps.begin() + 1;
       i != frame_timestamps.end(); ++i) {
    const size_t num_vsync_intervals = base::ClampFloor<size_t>(
        (*i - last_present_time) / GetParam().vsync_interval);
    ASSERT_LT(0u, num_vsync_intervals);
    ASSERT_GT(display_counts.size(), num_vsync_intervals);  // Quit early.
    ++display_counts[num_vsync_intervals];
    last_present_time += num_vsync_intervals * GetParam().vsync_interval;
  }

  // Analyze the histogram for an expected result pattern.  If the frame
  // timestamps are smooth, there should only be one or two buckets with
  // non-zero counts and they should be next to each other.  Because the clock
  // precision for the event_times provided to the sampler is very granular
  // (i.e., the vsync_interval), it's okay if other buckets have a tiny "stray"
  // count in this test.
  size_t highest_count = 0;
  size_t second_highest_count = 0;
  for (size_t repeats = 1; repeats < display_counts.size(); ++repeats) {
    DVLOG(1) << "display_counts[" << repeats << "] is "
             << display_counts[repeats];
    if (display_counts[repeats] >= highest_count) {
      second_highest_count = highest_count;
      highest_count = display_counts[repeats];
    } else if (display_counts[repeats] > second_highest_count) {
      second_highest_count = display_counts[repeats];
    }
  }
  size_t stray_count_remaining =
      (frame_timestamps.size() - 1) - (highest_count + second_highest_count);
  // Expect no more than 0.75% of frames fall outside the two main buckets.
  EXPECT_GT(frame_timestamps.size() * 75 / 10000, stray_count_remaining);
  for (size_t repeats = 1; repeats < display_counts.size() - 1; ++repeats) {
    if (display_counts[repeats] == highest_count) {
      EXPECT_EQ(second_highest_count, display_counts[repeats + 1]);
      ++repeats;
    } else if (second_highest_count > 0 &&
               display_counts[repeats] == second_highest_count) {
      EXPECT_EQ(highest_count, display_counts[repeats + 1]);
      ++repeats;
    } else {
      EXPECT_GE(stray_count_remaining, display_counts[repeats]);
      stray_count_remaining -= display_counts[repeats];
    }
  }
}

// Tests that frame timestamps are "lightly pushed" back towards the original
// presentation event times, which tells us the AnimatedContentSampler can
// account for sources of timestamp drift and correct the drift.
TEST_P(AnimatedContentSamplerParameterizedTest,
       FrameTimestampsConvergeTowardsEventTimes) {
  const int max_drift_increment_millis = 3;

  // Generate a full minute of events.
  const base::TimeTicks begin = InitialTestTimeTicks();
  std::vector<Event> events = GenerateEventSequence(
      begin, begin + base::Minutes(1), true, false, nullptr);

  // Modify the event sequence so that 1-3 ms of additional drift is suddenly
  // present every 100 events.  This is meant to simulate that, external to
  // AnimatedContentSampler, the video hardware vsync timebase is being
  // refreshed and is showing severe drift from the system clock.
  base::TimeDelta accumulated_drift;
  for (size_t i = 1; i < events.size(); ++i) {
    if (i % 100 == 0) {
      accumulated_drift += base::Milliseconds(
          GetRandomInRange(1, max_drift_increment_millis + 1));
    }
    events[i].second += accumulated_drift;
  }

  // Run all the events through the sampler and track the last rewritten frame
  // timestamp.
  base::TimeTicks last_frame_timestamp;
  for (std::vector<Event>::const_iterator i = events.begin(); i != events.end();
       ++i) {
    sampler()->ConsiderPresentationEvent(i->first, i->second);
    if (sampler()->ShouldSample())
      last_frame_timestamp = sampler()->frame_timestamp();
  }

  // If drift was accounted for, the |last_frame_timestamp| should be close to
  // the last event's timestamp.
  const base::TimeDelta total_error =
      events.back().second - last_frame_timestamp;
  const base::TimeDelta max_acceptable_error =
      GetParam().min_capture_period +
      base::Milliseconds(max_drift_increment_millis);
  EXPECT_NEAR(0.0, total_error.InMicroseconds(),
              max_acceptable_error.InMicroseconds());
}

INSTANTIATE_TEST_SUITE_P(
    All,
    AnimatedContentSamplerParameterizedTest,
    ::testing::Values(
        // Typical frame rate content: Compositor runs at 60 Hz, capture at 30
        // Hz, and content video animates at 30, 25, or 24 Hz.
        Scenario(60, 30, 30),
        Scenario(60, 30, 25),
        Scenario(60, 30, 24),

        // High frame rate content that leverages the Compositor's
        // capabilities, but capture is still at 30 Hz.
        Scenario(60, 30, 60),
        Scenario(60, 30, 50),
        Scenario(60, 30, 48),

        // High frame rate content that leverages the Compositor's
        // capabilities, and capture is also a buttery 60 Hz.
        Scenario(60, 60, 60),
        Scenario(60, 60, 50),
        Scenario(60, 60, 48),

        // High frame rate content that leverages the Compositor's
        // capabilities, but the client has disabled HFR sampling.
        Scenario(60, 60, 60, 30),
        Scenario(60, 60, 50, 30),
        Scenario(60, 60, 48, 30),

        // On some platforms, the Compositor runs at 50 Hz.
        Scenario(50, 30, 30),
        Scenario(50, 30, 25),
        Scenario(50, 30, 24),
        Scenario(50, 30, 50),
        Scenario(50, 30, 48),

        // Stable, but non-standard content frame rates.
        Scenario(60, 30, 16),
        Scenario(60, 30, 20),
        Scenario(60, 30, 23),
        Scenario(60, 30, 26),
        Scenario(60, 30, 27),
        Scenario(60, 30, 28),
        Scenario(60, 30, 29),
        Scenario(60, 30, 31),
        Scenario(60, 30, 32),
        Scenario(60, 30, 33)));

}  // namespace media
