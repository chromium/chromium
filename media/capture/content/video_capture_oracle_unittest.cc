// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/capture/content/video_capture_oracle.h"

#include <optional>

#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

namespace {

constexpr base::TimeTicks kInitialTestTimeTicks =
    base::TimeTicks() + base::Seconds(1);
constexpr base::TimeDelta k30HzPeriod = base::Seconds(1) / 30;
constexpr gfx::Size k1080pSize = gfx::Size(1920, 1080);
constexpr gfx::Size k720pSize = gfx::Size(1280, 720);
constexpr gfx::Size k360pSize = gfx::Size(640, 360);
constexpr gfx::Size kSmallestNonEmptySize = gfx::Size(2, 2);

}  // namespace

// Tests that VideoCaptureOracle filters out events whose timestamps are
// decreasing.
TEST(VideoCaptureOracleTest, EnforcesEventTimeMonotonicity) {
  const gfx::Rect damage_rect(k720pSize);
  const base::TimeDelta event_increment = k30HzPeriod * 2;

  VideoCaptureOracle oracle(false);
  oracle.SetMinCapturePeriod(k30HzPeriod);
  oracle.SetCaptureSizeConstraints(k720pSize, k720pSize, false);

  base::TimeTicks t = kInitialTestTimeTicks;
  for (int i = 0; i < 10; ++i) {
    t += event_increment;
    ASSERT_TRUE(oracle.ObserveEventAndDecideCapture(
        VideoCaptureOracle::kCompositorUpdate, damage_rect, t));
  }

  base::TimeTicks furthest_event_time = t;
  for (int i = 0; i < 10; ++i) {
    t -= event_increment;
    ASSERT_FALSE(oracle.ObserveEventAndDecideCapture(
        VideoCaptureOracle::kCompositorUpdate, damage_rect, t));
  }

  t = furthest_event_time;
  for (int i = 0; i < 10; ++i) {
    t += event_increment;
    ASSERT_TRUE(oracle.ObserveEventAndDecideCapture(
        VideoCaptureOracle::kCompositorUpdate, damage_rect, t));
  }
}

// Tests that VideoCaptureOracle is enforcing the requirement that
// successfully captured frames are delivered in order.  Otherwise, downstream
// consumers could be tripped-up by out-of-order frames or frame timestamps.
TEST(VideoCaptureOracleTest, EnforcesFramesDeliveredInOrder) {
  const gfx::Rect damage_rect(k720pSize);
  const base::TimeDelta event_increment = k30HzPeriod * 2;

  VideoCaptureOracle oracle(false);
  oracle.SetMinCapturePeriod(k30HzPeriod);
  oracle.SetCaptureSizeConstraints(k720pSize, k720pSize, false);

  // Most basic scenario: Frames delivered one at a time, with no additional
  // captures in-between deliveries.
  base::TimeTicks t = kInitialTestTimeTicks;
  int last_frame_number;
  base::TimeTicks ignored;
  for (int i = 0; i < 10; ++i) {
    t += event_increment;
    ASSERT_TRUE(oracle.ObserveEventAndDecideCapture(
        VideoCaptureOracle::kCompositorUpdate, damage_rect, t));
    last_frame_number = oracle.next_frame_number();
    oracle.RecordCapture(0.0);
    ASSERT_TRUE(oracle.CompleteCapture(last_frame_number, true, &ignored));
  }

  // Basic pipelined scenario: More than one frame in-flight at delivery points.
  for (int i = 0; i < 50; ++i) {
    const int num_in_flight = 1 + i % 3;
    for (int j = 0; j < num_in_flight; ++j) {
      t += event_increment;
      ASSERT_TRUE(oracle.ObserveEventAndDecideCapture(
          VideoCaptureOracle::kCompositorUpdate, damage_rect, t));
      last_frame_number = oracle.next_frame_number();
      oracle.RecordCapture(0.0);
    }
    for (int j = num_in_flight - 1; j >= 0; --j) {
      ASSERT_TRUE(
          oracle.CompleteCapture(last_frame_number - j, true, &ignored));
    }
  }

  // Pipelined scenario with successful out-of-order delivery attempts
  // rejected.
  for (int i = 0; i < 50; ++i) {
    const int num_in_flight = 1 + i % 3;
    for (int j = 0; j < num_in_flight; ++j) {
      t += event_increment;
      ASSERT_TRUE(oracle.ObserveEventAndDecideCapture(
          VideoCaptureOracle::kCompositorUpdate, damage_rect, t));
      last_frame_number = oracle.next_frame_number();
      oracle.RecordCapture(0.0);
    }
    ASSERT_TRUE(oracle.CompleteCapture(last_frame_number, true, &ignored));
    for (int j = 1; j < num_in_flight; ++j) {
      ASSERT_FALSE(
          oracle.CompleteCapture(last_frame_number - j, true, &ignored));
    }
  }

  // Pipelined scenario with successful delivery attempts accepted after an
  // unsuccessful out of order delivery attempt.
  for (int i = 0; i < 50; ++i) {
    const int num_in_flight = 1 + i % 3;
    for (int j = 0; j < num_in_flight; ++j) {
      t += event_increment;
      ASSERT_TRUE(oracle.ObserveEventAndDecideCapture(
          VideoCaptureOracle::kCompositorUpdate, damage_rect, t));
      last_frame_number = oracle.next_frame_number();
      oracle.RecordCapture(0.0);
    }
    // Report the last frame as an out of order failure.
    ASSERT_FALSE(oracle.CompleteCapture(last_frame_number, false, &ignored));
    for (int j = 1; j < num_in_flight - 1; ++j) {
      ASSERT_TRUE(
          oracle.CompleteCapture(last_frame_number - j, true, &ignored));
    }
  }
}

// Tests that VideoCaptureOracle transitions between using its two samplers in a
// way that does not introduce severe jank, pauses, etc.
TEST(VideoCaptureOracleTest, TransitionsSmoothlyBetweenSamplers) {
  const gfx::Rect animation_damage_rect(k720pSize);
  const base::TimeDelta event_increment = k30HzPeriod * 2;

  VideoCaptureOracle oracle(false);
  oracle.SetMinCapturePeriod(k30HzPeriod);
  oracle.SetCaptureSizeConstraints(k720pSize, k720pSize, false);

  // Run sequences of animation events and non-animation events through the
  // oracle.  As the oracle transitions between each sampler, make sure the
  // frame timestamps won't trip-up downstream consumers.
  base::TimeTicks t = kInitialTestTimeTicks;
  base::TimeTicks last_frame_timestamp;
  for (int i = 0; i < 1000; ++i) {
    t += event_increment;

    // For every 100 events, provide 50 that will cause the
    // AnimatedContentSampler to lock-in, followed by 50 that will cause it to
    // lock-out (i.e., the oracle will use the SmoothEventSampler instead).
    const bool provide_animated_content_event =
        (i % 100) >= 25 && (i % 100) < 75;

    // Only the few events that trigger the lock-out transition should be
    // dropped, because the AnimatedContentSampler doesn't yet realize the
    // animation ended.  Otherwise, the oracle should always decide to sample
    // because one of its samplers says to.
    const bool require_oracle_says_sample = (i % 100) < 75 || (i % 100) >= 78;
    const bool oracle_says_sample = oracle.ObserveEventAndDecideCapture(
        VideoCaptureOracle::kCompositorUpdate,
        provide_animated_content_event ? animation_damage_rect : gfx::Rect(),
        t);
    if (require_oracle_says_sample)
      ASSERT_TRUE(oracle_says_sample);
    if (!oracle_says_sample) {
      ASSERT_EQ(base::TimeDelta(), oracle.estimated_frame_duration());
      continue;
    }
    ASSERT_LT(base::TimeDelta(), oracle.estimated_frame_duration());

    const int frame_number = oracle.next_frame_number();
    oracle.RecordCapture(0.0);

    base::TimeTicks frame_timestamp;
    ASSERT_TRUE(oracle.CompleteCapture(frame_number, true, &frame_timestamp));
    ASSERT_FALSE(frame_timestamp.is_null());
    if (!last_frame_timestamp.is_null()) {
      const base::TimeDelta delta = frame_timestamp - last_frame_timestamp;
      EXPECT_LE(event_increment.InMicroseconds(), delta.InMicroseconds());
      // Right after the AnimatedContentSampler lock-out transition, there were
      // a few frames dropped, so allow a gap in the timestamps.  Otherwise, the
      // delta between frame timestamps should never be more than 2X the
      // |event_increment|.
      const base::TimeDelta max_acceptable_delta =
          (i % 100) == 78 ? event_increment * 5 : event_increment * 2;
      EXPECT_GE(max_acceptable_delta.InMicroseconds(), delta.InMicroseconds());
    }
    last_frame_timestamp = frame_timestamp;
  }
}

// Tests that VideoCaptureOracle prevents refresh request events from initiating
// simultaneous captures.
TEST(VideoCaptureOracleTest, SamplesAtCorrectTimesAroundRefreshRequests) {
  const base::TimeDelta vsync_interval = base::Seconds(1) / 60;
  const base::TimeDelta refresh_interval = base::Milliseconds(125);  // 8 FPS

  VideoCaptureOracle oracle(false);
  oracle.SetMinCapturePeriod(k30HzPeriod);
  oracle.SetCaptureSizeConstraints(k720pSize, k720pSize, false);

  // Have the oracle observe some compositor events.  Simulate that each capture
  // completes successfully.
  base::TimeTicks t = kInitialTestTimeTicks;
  base::TimeTicks ignored;
  bool did_complete_a_capture = false;
  for (int i = 0; i < 10; ++i) {
    t += vsync_interval;
    if (oracle.ObserveEventAndDecideCapture(
            VideoCaptureOracle::kCompositorUpdate, gfx::Rect(), t)) {
      const int frame_number = oracle.next_frame_number();
      oracle.RecordCapture(0.0);
      ASSERT_TRUE(oracle.CompleteCapture(frame_number, true, &ignored));
      did_complete_a_capture = true;
    }
  }
  ASSERT_TRUE(did_complete_a_capture);

  // Start one more compositor-based capture, but do not notify of completion
  // yet.
  for (int i = 0; i <= 10; ++i) {
    ASSERT_GT(10, i) << "BUG: Seems like it'll never happen!";
    t += vsync_interval;
    if (oracle.ObserveEventAndDecideCapture(
            VideoCaptureOracle::kCompositorUpdate, gfx::Rect(), t)) {
      break;
    }
  }
  int frame_number = oracle.next_frame_number();
  oracle.RecordCapture(0.0);

  // Stop providing the compositor events and start providing refresh request
  // events.  No overdue samplings should be recommended because of the
  // not-yet-complete compositor-based capture.
  for (int i = 0; i < 10; ++i) {
    t += refresh_interval;
    ASSERT_FALSE(oracle.ObserveEventAndDecideCapture(
        VideoCaptureOracle::kRefreshRequest, gfx::Rect(), t));
  }

  // Now, complete the outstanding compositor-based capture and continue
  // providing refresh request events.  The oracle should start recommending
  // sampling again.
  ASSERT_TRUE(oracle.CompleteCapture(frame_number, true, &ignored));
  did_complete_a_capture = false;
  for (int i = 0; i < 10; ++i) {
    t += refresh_interval;
    if (oracle.ObserveEventAndDecideCapture(VideoCaptureOracle::kRefreshRequest,
                                            gfx::Rect(), t)) {
      frame_number = oracle.next_frame_number();
      oracle.RecordCapture(0.0);
      ASSERT_TRUE(oracle.CompleteCapture(frame_number, true, &ignored));
      did_complete_a_capture = true;
    }
  }
  ASSERT_TRUE(did_complete_a_capture);

  // Start one more "refresh" capture, but do not notify of completion yet.
  for (int i = 0; i <= 10; ++i) {
    ASSERT_GT(10, i) << "BUG: Seems like it'll never happen!";
    t += refresh_interval;
    if (oracle.ObserveEventAndDecideCapture(VideoCaptureOracle::kRefreshRequest,
                                            gfx::Rect(), t)) {
      break;
    }
  }
  frame_number = oracle.next_frame_number();
  oracle.RecordCapture(0.0);

  // Confirm that the oracle does not recommend sampling until the outstanding
  // "refresh" capture completes.
  for (int i = 0; i < 10; ++i) {
    t += refresh_interval;
    ASSERT_FALSE(oracle.ObserveEventAndDecideCapture(
        VideoCaptureOracle::kRefreshRequest, gfx::Rect(), t));
  }
  ASSERT_TRUE(oracle.CompleteCapture(frame_number, true, &ignored));
  for (int i = 0; i <= 10; ++i) {
    ASSERT_GT(10, i) << "BUG: Seems like it'll never happen!";
    t += refresh_interval;
    if (oracle.ObserveEventAndDecideCapture(VideoCaptureOracle::kRefreshRequest,
                                            gfx::Rect(), t)) {
      break;
    }
  }
}

// Tests that VideoCaptureOracle does not rapidly change proposed capture sizes,
// to allow both the source content and the rest of the end-to-end system to
// stabilize (if autothrottling is enabled).
TEST(VideoCaptureOracleTest, DoesNotRapidlyChangeCaptureSize) {
  VideoCaptureOracle oracle(true);
  oracle.SetMinCapturePeriod(k30HzPeriod);
  oracle.SetCaptureSizeConstraints(kSmallestNonEmptySize, k720pSize, false);
  oracle.SetSourceSize(k1080pSize);

  // Run 30 seconds of frame captures without any source size changes.
  base::TimeTicks t = kInitialTestTimeTicks;
  const base::TimeDelta event_increment = k30HzPeriod * 2;
  base::TimeTicks end_t = t + base::Seconds(30);
  for (; t < end_t; t += event_increment) {
    ASSERT_TRUE(oracle.ObserveEventAndDecideCapture(
        VideoCaptureOracle::kCompositorUpdate, gfx::Rect(), t));
    ASSERT_EQ(k720pSize, oracle.capture_size());
    base::TimeTicks ignored;
    const int frame_number = oracle.next_frame_number();
    oracle.RecordCapture(0.0);
    ASSERT_TRUE(oracle.CompleteCapture(frame_number, true, &ignored));
    // Must provide non-zero actionable resource_utilization to enable
    // auto-throttling.
    oracle.RecordConsumerFeedback(frame_number,
                                  media::VideoCaptureFeedback(0.1));
  }

  // Now run 30 seconds of frame captures with lots of random source size
  // changes.  Check that there was no more than one size change per second.
  gfx::Size source_size = oracle.capture_size();
  base::TimeTicks time_of_last_size_change = kInitialTestTimeTicks;
  gfx::Size last_capture_size = oracle.capture_size();
  end_t = t + base::Seconds(30);
  for (; t < end_t; t += event_increment) {
    // Change the source size every frame to a random non-empty size.
    const gfx::Size last_source_size = source_size;
    source_size.SetSize(((last_source_size.width() * 11 + 12345) % 1280) + 1,
                        ((last_source_size.height() * 11 + 12345) % 720) + 1);
    ASSERT_NE(last_source_size, source_size);
    oracle.SetSourceSize(source_size);

    ASSERT_TRUE(oracle.ObserveEventAndDecideCapture(
        VideoCaptureOracle::kCompositorUpdate, gfx::Rect(), t));

    if (oracle.capture_size() != last_capture_size) {
      ASSERT_GE(t - time_of_last_size_change, base::Seconds(1));
      time_of_last_size_change = t;
      last_capture_size = oracle.capture_size();
    }

    base::TimeTicks ignored;
    const int frame_number = oracle.next_frame_number();
    oracle.RecordCapture(0.0);
    ASSERT_TRUE(oracle.CompleteCapture(frame_number, true, &ignored));
    oracle.RecordConsumerFeedback(frame_number,
                                  media::VideoCaptureFeedback(0.0));
  }
}

// Tests that VideoCaptureOracle allows every video frame to have a different
// size if resize throttling is disabled.
TEST(VideoCaptureOracleTest, ResizeThrottlingDisabled) {
  VideoCaptureOracle oracle(true);
  oracle.SetMinCapturePeriod(k30HzPeriod);
  oracle.SetMinSizeChangePeriod(base::TimeDelta());
  oracle.SetCaptureSizeConstraints(kSmallestNonEmptySize, k720pSize, false);
  oracle.SetSourceSize(k1080pSize);

  // Run 30 seconds of frame captures with lots of random source size
  // changes. The capture size should be different every time.
  base::TimeTicks t = kInitialTestTimeTicks;
  const base::TimeDelta event_increment = k30HzPeriod * 2;
  base::TimeTicks end_t = t + base::Seconds(30);
  gfx::Size source_size = oracle.capture_size();
  gfx::Size last_capture_size = oracle.capture_size();
  for (; t < end_t; t += event_increment) {
    // Change the source size every frame to a random non-empty size.
    const gfx::Size last_source_size = source_size;
    source_size.SetSize(((last_source_size.width() * 11 + 12345) % 1280) + 1,
                        ((last_source_size.height() * 11 + 12345) % 720) + 1);
    ASSERT_NE(last_source_size, source_size);
    oracle.SetSourceSize(source_size);

    ASSERT_TRUE(oracle.ObserveEventAndDecideCapture(
        VideoCaptureOracle::kCompositorUpdate, gfx::Rect(), t));

    ASSERT_NE(last_capture_size, oracle.capture_size());
    last_capture_size = oracle.capture_size();

    base::TimeTicks ignored;
    const int frame_number = oracle.next_frame_number();
    oracle.RecordCapture(0.0);
    ASSERT_TRUE(oracle.CompleteCapture(frame_number, true, &ignored));
    oracle.RecordConsumerFeedback(frame_number,
                                  media::VideoCaptureFeedback(0.0));
  }
}

namespace {

// Tests that VideoCaptureOracle can auto-throttle by stepping the capture size
// up or down.  When |is_content_animating| is false, there is more
// aggressiveness expected in the timing of stepping upwards.  If
// |with_consumer_feedback| is false, only buffer pool utilization varies and no
// consumer feedback is provided.  If |with_consumer_feedback| is true, the
// buffer pool utilization is held constant at 25%, and the consumer utilization
// feedback varies.
void RunAutoThrottleTest(bool is_content_animating,
                         bool with_consumer_feedback) {
  SCOPED_TRACE(::testing::Message()
               << "RunAutoThrottleTest("
               << "(is_content_animating=" << is_content_animating
               << ", with_consumer_feedback=" << with_consumer_feedback << ")");

  VideoCaptureOracle oracle(true);
  oracle.SetMinCapturePeriod(k30HzPeriod);
  oracle.SetCaptureSizeConstraints(kSmallestNonEmptySize, k720pSize, false);
  oracle.SetSourceSize(k1080pSize);

  // Run 10 seconds of frame captures with 90% utilization expect no capture
  // size changes.
  base::TimeTicks t = kInitialTestTimeTicks;
  base::TimeTicks time_of_last_size_change = t;
  const base::TimeDelta event_increment = k30HzPeriod * 2;
  base::TimeTicks end_t = t + base::Seconds(10);
  for (; t < end_t; t += event_increment) {
    ASSERT_TRUE(oracle.ObserveEventAndDecideCapture(
        VideoCaptureOracle::kCompositorUpdate,
        is_content_animating ? gfx::Rect(k720pSize) : gfx::Rect(), t));
    ASSERT_EQ(k720pSize, oracle.capture_size());
    const double utilization = 0.9;
    const int frame_number = oracle.next_frame_number();
    oracle.RecordCapture(with_consumer_feedback ? 0.25 : utilization);
    base::TimeTicks ignored;
    ASSERT_TRUE(oracle.CompleteCapture(frame_number, true, &ignored));
    if (with_consumer_feedback) {
      oracle.RecordConsumerFeedback(frame_number,
                                    media::VideoCaptureFeedback(utilization));
    } else if (t == kInitialTestTimeTicks) {
      // Provide feedback with the very first capture to activate the capture
      // size auto-throttling logic. After this, no consumer feedback applies
      // and the buffer utilization will be the only consideration.
      oracle.RecordConsumerFeedback(frame_number,
                                    media::VideoCaptureFeedback(0.0));
    }
  }

  // Cause two downward steppings in resolution.  First, indicate overload
  // until the resolution steps down.  Then, indicate a 90% utilization and
  // expect the resolution to remain constant.  Repeat.
  for (int i = 0; i < 2; ++i) {
    const gfx::Size starting_size = oracle.capture_size();
    SCOPED_TRACE(::testing::Message() << "Stepping down from "
                                      << starting_size.ToString()
                                      << ", i=" << i);

    gfx::Size stepped_down_size;
    end_t = t + base::Seconds(10);
    for (; t < end_t; t += event_increment) {
      ASSERT_TRUE(oracle.ObserveEventAndDecideCapture(
          VideoCaptureOracle::kCompositorUpdate,
          is_content_animating ? gfx::Rect(k720pSize) : gfx::Rect(), t));

      if (stepped_down_size.IsEmpty()) {
        if (oracle.capture_size() != starting_size) {
          time_of_last_size_change = t;
          stepped_down_size = oracle.capture_size();
          ASSERT_GT(starting_size.width(), stepped_down_size.width());
          ASSERT_GT(starting_size.height(), stepped_down_size.height());
        }
      } else {
        ASSERT_EQ(stepped_down_size, oracle.capture_size());
      }

      const double utilization = stepped_down_size.IsEmpty() ? 1.5 : 0.9;
      const int frame_number = oracle.next_frame_number();
      oracle.RecordCapture(with_consumer_feedback ? 0.25 : utilization);
      base::TimeTicks ignored;
      ASSERT_TRUE(oracle.CompleteCapture(frame_number, true, &ignored));
      if (with_consumer_feedback)
        oracle.RecordConsumerFeedback(frame_number,
                                      media::VideoCaptureFeedback(utilization));
    }
  }

  // Now, cause two upward steppings in resolution.  First, indicate
  // under-utilization until the resolution steps up.  Then, indicate a 90%
  // utilization and expect the resolution to remain constant.  Repeat.
  for (int i = 0; i < 2; ++i) {
    const gfx::Size starting_size = oracle.capture_size();
    SCOPED_TRACE(::testing::Message() << "Stepping up from "
                                      << starting_size.ToString()
                                      << ", i=" << i);

    gfx::Size stepped_up_size;
    end_t = t + base::Seconds(is_content_animating ? 90 : 10);
    for (; t < end_t; t += event_increment) {
      ASSERT_TRUE(oracle.ObserveEventAndDecideCapture(
          VideoCaptureOracle::kCompositorUpdate,
          is_content_animating ? gfx::Rect(k720pSize) : gfx::Rect(), t));

      if (stepped_up_size.IsEmpty()) {
        if (oracle.capture_size() != starting_size) {
          // When content is animating, a much longer amount of time must pass
          // before the capture size will step up.
          ASSERT_LT(base::Seconds(is_content_animating ? 15 : 1),
                    t - time_of_last_size_change);
          time_of_last_size_change = t;
          stepped_up_size = oracle.capture_size();
          ASSERT_LT(starting_size.width(), stepped_up_size.width());
          ASSERT_LT(starting_size.height(), stepped_up_size.height());
        }
      } else {
        ASSERT_EQ(stepped_up_size, oracle.capture_size());
      }

      const double utilization = stepped_up_size.IsEmpty() ? 0.0 : 0.9;
      const int frame_number = oracle.next_frame_number();
      oracle.RecordCapture(with_consumer_feedback ? 0.25 : utilization);
      base::TimeTicks ignored;
      ASSERT_TRUE(oracle.CompleteCapture(frame_number, true, &ignored));
      if (with_consumer_feedback)
        oracle.RecordConsumerFeedback(frame_number,
                                      media::VideoCaptureFeedback(utilization));
    }
  }
}

}  // namespace

// Tests that VideoCaptureOracle can auto-throttle by stepping the capture size
// up or down, using utilization feedback signals from either the buffer pool or
// the consumer, and with slightly different behavior depending on whether
// content is animating.
TEST(VideoCaptureOracleTest,
     AutoThrottlesCaptureSizeBasedOnUtilizationFeedback) {
  RunAutoThrottleTest(false, false);
  RunAutoThrottleTest(false, true);
  RunAutoThrottleTest(true, false);
  RunAutoThrottleTest(true, true);
}

// Test that the capture size is not auto-throttled if consumer feedback is
// never provided. This represents VideoCaptureOracle being stuck in the
// kThrottlingEnabled mode, but never having entered the kThrottlingActive mode.
TEST(VideoCaptureOracleTest,
     DoesNotAutoThrottleCaptureSizeWithoutConsumerFeedback) {
  VideoCaptureOracle oracle(true);
  oracle.SetMinCapturePeriod(k30HzPeriod);
  oracle.SetCaptureSizeConstraints(kSmallestNonEmptySize, k720pSize, false);
  oracle.SetSourceSize(k1080pSize);

  // Run 10 seconds of frame captures with 90% utilization expect no capture
  // size changes.
  base::TimeTicks t = kInitialTestTimeTicks;
  const base::TimeDelta event_increment = k30HzPeriod * 2;
  base::TimeTicks end_t = t + base::Seconds(10);
  for (; t < end_t; t += event_increment) {
    ASSERT_TRUE(oracle.ObserveEventAndDecideCapture(
        VideoCaptureOracle::kCompositorUpdate, gfx::Rect(k720pSize), t));
    ASSERT_EQ(k720pSize, oracle.capture_size());
    const int frame_number = oracle.next_frame_number();
    oracle.RecordCapture(0.9);
    base::TimeTicks ignored;
    ASSERT_TRUE(oracle.CompleteCapture(frame_number, true, &ignored));
    // Important: Not calling oracle.RecordConsumerFeedback(frame_number, ...);
  }

  // Increase utilization to 1000%, but expect no capture size change because
  // there has never been any consumer feedback.
  const gfx::Size starting_size = oracle.capture_size();
  end_t = t + base::Seconds(10);
  for (; t < end_t; t += event_increment) {
    ASSERT_TRUE(oracle.ObserveEventAndDecideCapture(
        VideoCaptureOracle::kCompositorUpdate, gfx::Rect(k720pSize), t));
    ASSERT_EQ(starting_size, oracle.capture_size());
    const int frame_number = oracle.next_frame_number();
    oracle.RecordCapture(10.0);
    base::TimeTicks ignored;
    ASSERT_TRUE(oracle.CompleteCapture(frame_number, true, &ignored));
  }
}

// Tests that, while content is animating, VideoCaptureOracle can make frequent
// capture size increases only just after the source size has changed.
// Otherwise, capture size increases should only be made cautiously, after a
// long "proving period of under-utilization" has elapsed.
TEST(VideoCaptureOracleTest, IncreasesFrequentlyOnlyAfterSourceSizeChange) {
  VideoCaptureOracle oracle(true);
  oracle.SetMinCapturePeriod(k30HzPeriod);
  oracle.SetCaptureSizeConstraints(kSmallestNonEmptySize, k720pSize, false);

  // Start out the source size at 360p, so there is room to grow to the 720p
  // maximum.
  oracle.SetSourceSize(k360pSize);

  // Run 10 seconds of frame captures with under-utilization to represent a
  // machine that can do more, but won't because the source size is small.
  base::TimeTicks t = kInitialTestTimeTicks;
  const base::TimeDelta event_increment = k30HzPeriod * 2;
  base::TimeTicks end_t = t + base::Seconds(10);
  for (; t < end_t; t += event_increment) {
    if (!oracle.ObserveEventAndDecideCapture(
            VideoCaptureOracle::kCompositorUpdate, gfx::Rect(k360pSize), t)) {
      continue;
    }
    ASSERT_EQ(k360pSize, oracle.capture_size());
    const int frame_number = oracle.next_frame_number();
    oracle.RecordCapture(0.25);
    base::TimeTicks ignored;
    ASSERT_TRUE(oracle.CompleteCapture(frame_number, true, &ignored));
    oracle.RecordConsumerFeedback(frame_number,
                                  media::VideoCaptureFeedback(0.25));
  }

  // Now, set the source size to 720p, continuing to report under-utilization,
  // and expect the capture size increases to reach a full 720p within 15
  // seconds.
  oracle.SetSourceSize(k720pSize);
  gfx::Size last_capture_size = oracle.capture_size();
  end_t = t + base::Seconds(15);
  for (; t < end_t; t += event_increment) {
    if (!oracle.ObserveEventAndDecideCapture(
            VideoCaptureOracle::kCompositorUpdate, gfx::Rect(k720pSize), t)) {
      continue;
    }
    ASSERT_LE(last_capture_size.width(), oracle.capture_size().width());
    ASSERT_LE(last_capture_size.height(), oracle.capture_size().height());
    last_capture_size = oracle.capture_size();
    const int frame_number = oracle.next_frame_number();
    oracle.RecordCapture(0.25);
    base::TimeTicks ignored;
    ASSERT_TRUE(oracle.CompleteCapture(frame_number, true, &ignored));
    oracle.RecordConsumerFeedback(frame_number,
                                  media::VideoCaptureFeedback(0.25));
  }
  ASSERT_EQ(k720pSize, oracle.capture_size());

  // Now, change the source size again, but report over-utilization so the
  // capture size will decrease.  Once it decreases one step, report 90%
  // utilization to achieve a steady-state.
  oracle.SetSourceSize(k1080pSize);
  gfx::Size stepped_down_size;
  end_t = t + base::Seconds(10);
  for (; t < end_t; t += event_increment) {
    if (!oracle.ObserveEventAndDecideCapture(
            VideoCaptureOracle::kCompositorUpdate, gfx::Rect(k1080pSize), t)) {
      continue;
    }

    if (stepped_down_size.IsEmpty()) {
      if (oracle.capture_size() != k720pSize) {
        stepped_down_size = oracle.capture_size();
        ASSERT_GT(k720pSize.width(), stepped_down_size.width());
        ASSERT_GT(k720pSize.height(), stepped_down_size.height());
      }
    } else {
      ASSERT_EQ(stepped_down_size, oracle.capture_size());
    }

    const double utilization = stepped_down_size.IsEmpty() ? 1.5 : 0.9;
    const int frame_number = oracle.next_frame_number();
    oracle.RecordCapture(utilization);
    base::TimeTicks ignored;
    ASSERT_TRUE(oracle.CompleteCapture(frame_number, true, &ignored));
    oracle.RecordConsumerFeedback(frame_number,
                                  media::VideoCaptureFeedback(utilization));
  }
  ASSERT_FALSE(stepped_down_size.IsEmpty());

  // Now, if we report under-utilization again (without any source size change),
  // there should be a long "proving period" before there is any increase in
  // capture size made by the oracle.
  const base::TimeTicks proving_period_end_time = t + base::Seconds(15);
  gfx::Size stepped_up_size;
  end_t = t + base::Seconds(60);
  for (; t < end_t; t += event_increment) {
    if (!oracle.ObserveEventAndDecideCapture(
            VideoCaptureOracle::kCompositorUpdate, gfx::Rect(k1080pSize), t)) {
      continue;
    }

    if (stepped_up_size.IsEmpty()) {
      if (oracle.capture_size() != stepped_down_size) {
        ASSERT_LT(proving_period_end_time, t);
        stepped_up_size = oracle.capture_size();
        ASSERT_LT(stepped_down_size.width(), stepped_up_size.width());
        ASSERT_LT(stepped_down_size.height(), stepped_up_size.height());
      }
    } else {
      ASSERT_EQ(stepped_up_size, oracle.capture_size());
    }

    const double utilization = stepped_up_size.IsEmpty() ? 0.25 : 0.9;
    const int frame_number = oracle.next_frame_number();
    oracle.RecordCapture(utilization);
    base::TimeTicks ignored;
    ASSERT_TRUE(oracle.CompleteCapture(frame_number, true, &ignored));
    oracle.RecordConsumerFeedback(frame_number,
                                  media::VideoCaptureFeedback(utilization));
  }
  ASSERT_FALSE(stepped_up_size.IsEmpty());
}

// Tests that VideoCaptureOracle does not change the capture size if
// auto-throttling is enabled when using a fixed resolution policy.
TEST(VideoCaptureOracleTest, DoesNotAutoThrottleWhenResolutionIsFixed) {
  VideoCaptureOracle oracle(true);
  oracle.SetMinCapturePeriod(k30HzPeriod);
  oracle.SetCaptureSizeConstraints(k720pSize, k720pSize, false);
  oracle.SetSourceSize(k1080pSize);

  // Run 10 seconds of frame captures with 90% utilization expect no capture
  // size changes.
  base::TimeTicks t = kInitialTestTimeTicks;
  const base::TimeDelta event_increment = k30HzPeriod * 2;
  base::TimeTicks end_t = t + base::Seconds(10);
  for (; t < end_t; t += event_increment) {
    ASSERT_TRUE(oracle.ObserveEventAndDecideCapture(
        VideoCaptureOracle::kCompositorUpdate, gfx::Rect(), t));
    ASSERT_EQ(k720pSize, oracle.capture_size());
    base::TimeTicks ignored;
    const int frame_number = oracle.next_frame_number();
    oracle.RecordCapture(0.9);
    ASSERT_TRUE(oracle.CompleteCapture(frame_number, true, &ignored));
    oracle.RecordConsumerFeedback(frame_number,
                                  media::VideoCaptureFeedback(0.9));
  }

  // Now run 10 seconds with overload indicated.  Still, expect no capture size
  // changes.
  end_t = t + base::Seconds(10);
  for (; t < end_t; t += event_increment) {
    ASSERT_TRUE(oracle.ObserveEventAndDecideCapture(
        VideoCaptureOracle::kCompositorUpdate, gfx::Rect(), t));
    ASSERT_EQ(k720pSize, oracle.capture_size());
    base::TimeTicks ignored;
    const int frame_number = oracle.next_frame_number();
    oracle.RecordCapture(2.0);
    ASSERT_TRUE(oracle.CompleteCapture(frame_number, true, &ignored));
    oracle.RecordConsumerFeedback(frame_number,
                                  media::VideoCaptureFeedback(2.0));
  }
}

// Tests that VideoCaptureOracle reduces resolution on max_pixels feedback.
TEST(VideoCaptureOracleTest, RespectsMaxPixelsFeedback) {
  VideoCaptureOracle oracle(true);
  oracle.SetMinCapturePeriod(k30HzPeriod);
  oracle.SetCaptureSizeConstraints(kSmallestNonEmptySize, k720pSize, false);
  oracle.SetSourceSize(k720pSize);

  // Run 1 second with no feedback and expect no capture size changes.
  base::TimeTicks t = kInitialTestTimeTicks;
  const base::TimeDelta event_increment = k30HzPeriod * 2;
  base::TimeTicks end_t = t + base::Seconds(1);
  for (; t < end_t; t += event_increment) {
    ASSERT_TRUE(oracle.ObserveEventAndDecideCapture(
        VideoCaptureOracle::kCompositorUpdate, gfx::Rect(), t));
    ASSERT_EQ(k720pSize, oracle.capture_size());
    base::TimeTicks ignored;
    const int frame_number = oracle.next_frame_number();
    oracle.RecordCapture(0.25);  // Low buffer utilization.
    ASSERT_TRUE(oracle.CompleteCapture(frame_number, true, &ignored));
    oracle.RecordConsumerFeedback(frame_number, media::VideoCaptureFeedback());
  }

  // Now run for a single frame with 360p pixel limit.
  {
    ASSERT_TRUE(oracle.ObserveEventAndDecideCapture(
        VideoCaptureOracle::kCompositorUpdate, gfx::Rect(), t));
    ASSERT_EQ(k720pSize, oracle.capture_size());
    base::TimeTicks ignored;
    const int frame_number = oracle.next_frame_number();
    oracle.RecordCapture(0.25);  // Low buffer utilization.
    ASSERT_TRUE(oracle.CompleteCapture(frame_number, true, &ignored));
    oracle.RecordConsumerFeedback(
        frame_number,
        media::VideoCaptureFeedback(
            -1.0, std::numeric_limits<float>::infinity(), k360pSize.GetArea()));
    t += event_increment;
  }

  // Capture another frame. Size should be 360p. Make feedback with no limit.
  {
    ASSERT_TRUE(oracle.ObserveEventAndDecideCapture(
        VideoCaptureOracle::kCompositorUpdate, gfx::Rect(), t));
    ASSERT_EQ(k360pSize, oracle.capture_size());
    base::TimeTicks ignored;
    const int frame_number = oracle.next_frame_number();
    oracle.RecordCapture(0.25);  // Low buffer utilization.
    ASSERT_TRUE(oracle.CompleteCapture(frame_number, true, &ignored));
    oracle.RecordConsumerFeedback(frame_number, media::VideoCaptureFeedback());
    t += event_increment;
  }

  // Capture another frame. Size should be reverted back, since the last frame
  // reported no limit.
  {
    ASSERT_TRUE(oracle.ObserveEventAndDecideCapture(
        VideoCaptureOracle::kCompositorUpdate, gfx::Rect(), t));
    ASSERT_EQ(k720pSize, oracle.capture_size());
    base::TimeTicks ignored;
    const int frame_number = oracle.next_frame_number();
    oracle.RecordCapture(0.25);  // Low buffer utilization.
    ASSERT_TRUE(oracle.CompleteCapture(frame_number, true, &ignored));
    oracle.RecordConsumerFeedback(frame_number, media::VideoCaptureFeedback());
    t += event_increment;
  }
}

// Tests that VideoCaptureOracle respects resource_utilization feedback over
// max_pixels feedback.
TEST(VideoCaptureOracleTest, IgnoresMaxPixelsFeedbackIfAutoThrottlingIsOn) {
  VideoCaptureOracle oracle(true);
  oracle.SetMinCapturePeriod(k30HzPeriod);
  oracle.SetCaptureSizeConstraints(kSmallestNonEmptySize, k720pSize, false);
  oracle.SetSourceSize(k720pSize);

  // Run 1 second with no feedback and expect no capture size changes.
  base::TimeTicks t = kInitialTestTimeTicks;
  const base::TimeDelta event_increment = k30HzPeriod * 2;
  base::TimeTicks end_t = t + base::Seconds(1);
  for (; t < end_t; t += event_increment) {
    ASSERT_TRUE(oracle.ObserveEventAndDecideCapture(
        VideoCaptureOracle::kCompositorUpdate, gfx::Rect(), t));
    ASSERT_EQ(k720pSize, oracle.capture_size());
    base::TimeTicks ignored;
    const int frame_number = oracle.next_frame_number();
    oracle.RecordCapture(0.25);  // Low buffer utilization.
    ASSERT_TRUE(oracle.CompleteCapture(frame_number, true, &ignored));
    oracle.RecordConsumerFeedback(frame_number,
                                  media::VideoCaptureFeedback(0.25));
  }

  // Now run with a new 360p pixel limit returned in the feedback.
  {
    ASSERT_TRUE(oracle.ObserveEventAndDecideCapture(
        VideoCaptureOracle::kCompositorUpdate, gfx::Rect(), t));
    ASSERT_EQ(k720pSize, oracle.capture_size());
    base::TimeTicks ignored;
    const int frame_number = oracle.next_frame_number();
    oracle.RecordCapture(0.25);  // Low buffer utilization.
    ASSERT_TRUE(oracle.CompleteCapture(frame_number, true, &ignored));
    oracle.RecordConsumerFeedback(
        frame_number,
        media::VideoCaptureFeedback(
            0.25, std::numeric_limits<float>::infinity(), k360pSize.GetArea()));
    t += event_increment;
  }

  // Capture another frame. Size should still be 720p, since the autothrottling
  // was enabled by the previous feedback.
  {
    ASSERT_TRUE(oracle.ObserveEventAndDecideCapture(
        VideoCaptureOracle::kCompositorUpdate, gfx::Rect(), t));
    ASSERT_EQ(k720pSize, oracle.capture_size());
    base::TimeTicks ignored;
    const int frame_number = oracle.next_frame_number();
    oracle.RecordCapture(0.25);  // Low buffer utilization.
    ASSERT_TRUE(oracle.CompleteCapture(frame_number, true, &ignored));
    oracle.RecordConsumerFeedback(frame_number, media::VideoCaptureFeedback());
    t += event_increment;
  }
}

// Tests that VideoCaptureOracle respects the max framerate requested by the
// consumer.
TEST(VideoCaptureOracleTest, RespectsMaxFrameRateFeedback) {
  constexpr base::TimeDelta vsync_interval = base::Hertz(60);
  constexpr base::TimeDelta k5HzPeriod = base::Hertz(5);
  constexpr base::TimeDelta kAllowedError = base::Milliseconds(1);
  constexpr float k5Fps = 5.0;
  constexpr float kNoResourceUtilization = -1.0;
  constexpr float kNoFpsLimit = std::numeric_limits<float>::infinity();

  VideoCaptureOracle oracle(false);
  oracle.SetMinCapturePeriod(k30HzPeriod);
  oracle.SetCaptureSizeConstraints(k720pSize, k720pSize, false);

  // Have the oracle observe some compositor events.  Simulate that each capture
  // completes successfully.
  base::TimeTicks t = kInitialTestTimeTicks;
  base::TimeTicks ignored;
  int frame_number;

  // As if previous frame was captured at 30 fps.
  std::optional<base::TimeTicks> last_capture_time;
  for (int i = 0; i < 100; ++i) {
    t += vsync_interval;
    if (oracle.ObserveEventAndDecideCapture(
            VideoCaptureOracle::kCompositorUpdate, gfx::Rect(), t)) {
      frame_number = oracle.next_frame_number();
      oracle.RecordCapture(0.0);
      ASSERT_TRUE(oracle.CompleteCapture(frame_number, true, &ignored));
      if (last_capture_time) {
        EXPECT_GE(t - *last_capture_time, k30HzPeriod - kAllowedError);
        EXPECT_LE(t - *last_capture_time, k30HzPeriod + kAllowedError);
      }
      last_capture_time = t;
    }
  }

  // 100 vsync periods is more than enough to capture something.
  ASSERT_TRUE(last_capture_time.has_value());

  // Receive feedback on the very last frame.
  oracle.RecordConsumerFeedback(
      frame_number, media::VideoCaptureFeedback(kNoResourceUtilization, k5Fps));

  // Don't measure frame-rate across different target frame-rates.
  last_capture_time = std::nullopt;
  // Continue capturing frames, observe that frame-rate limit is respected.
  for (int i = 0; i < 100; ++i) {
    t += vsync_interval;
    if (oracle.ObserveEventAndDecideCapture(
            VideoCaptureOracle::kCompositorUpdate, gfx::Rect(), t)) {
      frame_number = oracle.next_frame_number();
      oracle.RecordCapture(0.0);
      ASSERT_TRUE(oracle.CompleteCapture(frame_number, true, &ignored));
      if (last_capture_time) {
        EXPECT_GE(t - *last_capture_time, k5HzPeriod - kAllowedError);
        EXPECT_LE(t - *last_capture_time, k5HzPeriod + kAllowedError);
      }
      last_capture_time = t;
    }
  }

  // 100 vsync periods is more than enough to capture something.
  ASSERT_TRUE(last_capture_time.has_value());

  // Receive feedback with no limit.
  oracle.RecordConsumerFeedback(
      frame_number,
      media::VideoCaptureFeedback(kNoResourceUtilization, kNoFpsLimit));

  // Don't measure frame-rate across different target frame-rates.
  last_capture_time = std::nullopt;
  // Continue capturing frames, observe that original min capture period is
  // respected.
  for (int i = 0; i < 100; ++i) {
    t += vsync_interval;
    if (oracle.ObserveEventAndDecideCapture(
            VideoCaptureOracle::kCompositorUpdate, gfx::Rect(), t)) {
      frame_number = oracle.next_frame_number();
      oracle.RecordCapture(0.0);
      ASSERT_TRUE(oracle.CompleteCapture(frame_number, true, &ignored));
      if (last_capture_time) {
        EXPECT_GE(t - *last_capture_time, k30HzPeriod - kAllowedError);
        EXPECT_LE(t - *last_capture_time, k30HzPeriod + kAllowedError);
      }
      last_capture_time = t;
    }
  }

  // 100 vsync periods is more than enough to capture something.
  ASSERT_TRUE(last_capture_time.has_value());
}

}  // namespace media
