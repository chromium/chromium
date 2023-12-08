// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/navigation_transitions/physics_model.h"

#include <memory>

#include "base/numerics/ranges.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

namespace {

// Test input and output for finger drag curve.
struct FingerDragCurveConfig {
  std::tuple<float, base::TimeTicks> movement_timestamp;
  PhysicsModel::Result expected;
};

// Test input and output for the spring models.
struct SpringConfig {
  base::TimeTicks timestamp;
  PhysicsModel::Result expected;
};

struct TestConfig {
  std::vector<FingerDragCurveConfig> gesture_progressed;
  std::vector<SpringConfig> commit_stop;
  std::vector<SpringConfig> cancel;
  std::vector<SpringConfig> invoke;
};

class PhysicsModelUnittest : public ::testing::Test {
 public:
  PhysicsModelUnittest() = default;
  ~PhysicsModelUnittest() override = default;

  void SetUp() override {
    // Simulate a Pixel6/7. The commit-stop position is 918px.
    physics_model_ = std::make_unique<PhysicsModel>(
        /*screen_width=*/1080,
        /*device_scale_factor=*/2.625);
  }

  // Nine gestures: simulate the finger moves from 0px to 900px, before the
  // commit-stop 918px.
  std::vector<FingerDragCurveConfig> NineGestureProgressed(
      base::TimeDelta increment) {
    return {
        FingerDragCurveConfig{
            .movement_timestamp = {100.f,
                                   NextTimeTickAfter(base::Milliseconds(0))},
            .expected =
                PhysicsModel::Result{.foreground_offset_physical = 85,
                                     .background_offset_physical = -208.25,
                                     .done = false}},
        FingerDragCurveConfig{
            .movement_timestamp = {100.f, NextTimeTickAfter(increment)},
            .expected = PhysicsModel::Result{.foreground_offset_physical = 170,
                                             .background_offset_physical = -187,
                                             .done = false}},
        FingerDragCurveConfig{
            .movement_timestamp = {100.f, NextTimeTickAfter(increment)},
            .expected =
                PhysicsModel::Result{.foreground_offset_physical = 255,
                                     .background_offset_physical = -165.75,
                                     .done = false}},
        FingerDragCurveConfig{
            .movement_timestamp = {100.f, NextTimeTickAfter(increment)},
            .expected =
                PhysicsModel::Result{.foreground_offset_physical = 340,
                                     .background_offset_physical = -144.5,
                                     .done = false}},
        FingerDragCurveConfig{
            .movement_timestamp = {100.f, NextTimeTickAfter(increment)},
            .expected =
                PhysicsModel::Result{.foreground_offset_physical = 425,
                                     .background_offset_physical = -123.25,
                                     .done = false}},
        FingerDragCurveConfig{
            .movement_timestamp = {100.f, NextTimeTickAfter(increment)},
            .expected = PhysicsModel::Result{.foreground_offset_physical = 510,
                                             .background_offset_physical = -102,
                                             .done = false}},
        FingerDragCurveConfig{
            .movement_timestamp = {100.f, NextTimeTickAfter(increment)},
            .expected =
                PhysicsModel::Result{.foreground_offset_physical = 595,
                                     .background_offset_physical = -80.75,
                                     .done = false}},
        FingerDragCurveConfig{
            .movement_timestamp = {100.f, NextTimeTickAfter(increment)},
            .expected =
                PhysicsModel::Result{.foreground_offset_physical = 680,
                                     .background_offset_physical = -59.5,
                                     .done = false}},
        FingerDragCurveConfig{
            .movement_timestamp = {100.f, NextTimeTickAfter(increment)},
            .expected =
                PhysicsModel::Result{.foreground_offset_physical = 765,
                                     .background_offset_physical = -38.25,
                                     .done = false}},
    };
  }

  // Ten gestures: simulate the finger moves from 0px to 1000px, which is after
  // the commit-stop position.
  std::vector<FingerDragCurveConfig> TenGestureProgressed(
      base::TimeDelta increment) {
    auto nine = NineGestureProgressed(increment);
    nine.push_back(FingerDragCurveConfig{
        .movement_timestamp = {100.f, NextTimeTickAfter(increment)},
        .expected = PhysicsModel::Result{.foreground_offset_physical = 850,
                                         .background_offset_physical = -17,
                                         .done = false}});
    return nine;
  }

  base::TimeTicks NextTimeTickAfter(base::TimeDelta delta) {
    start_ += delta;
    return start_;
  }

  PhysicsModel* physics_model() const { return physics_model_.get(); }

 private:
  std::unique_ptr<PhysicsModel> physics_model_;
  base::TimeTicks start_ = base::TimeTicks::Now();
};

}  // namespace

// Better EXPECT_EQ output.
std::ostream& operator<<(std::ostream& os, const PhysicsModel::Result& r) {
  os << "foreground offset: " << r.foreground_offset_physical
     << " background offset: " << r.background_offset_physical
     << " done: " << (r.done ? "true" : "false");
  return os;
}

bool operator==(const PhysicsModel::Result& lhs,
                const PhysicsModel::Result& rhs) {
  return lhs.done == rhs.done &&
         base::IsApproximatelyEqual(lhs.background_offset_physical,
                                    rhs.background_offset_physical, 0.01f) &&
         base::IsApproximatelyEqual(lhs.foreground_offset_physical,
                                    rhs.foreground_offset_physical, 0.01f);
}

// Exercise the finger drag curve and the invoke spring, and skip the
// commit-stop spring completely. The finger lifts from the screen BEFORE the
// commit-stop position.
TEST_F(PhysicsModelUnittest, ProgressInvoke_LiftBeforeCommitStop) {
  const TestConfig config{
      .gesture_progressed = NineGestureProgressed(base::Milliseconds(100)),
      .commit_stop = {},
      .cancel = {},
      .invoke =
          {
              // Same positional result. With the drag curve we don't store the
              // timestamp in the physics model, so the first requested frame
              // will have a `raf_since_start`=0 calculated from the wallclock,
              // which gives us the same position result as the end of the drag
              // curve. This won't be a problem in real life because we will
              // just be drawing one more frame at the start of the animation.
              SpringConfig{
                  .timestamp = NextTimeTickAfter(base::Milliseconds(100)),
                  .expected = {.foreground_offset_physical = 765,
                               .background_offset_physical = -38.25,
                               .done = false}},
              // The foreground has reached the commit-stop point. From this
              // point on the background will have offset=0 - it will not
              // bounce.
              SpringConfig{
                  .timestamp = NextTimeTickAfter(base::Milliseconds(100)),
                  .expected = {.foreground_offset_physical = 1042.11,
                               .background_offset_physical = 0,
                               .done = false}},
              SpringConfig{
                  .timestamp = NextTimeTickAfter(base::Milliseconds(100)),
                  .expected = {.foreground_offset_physical = 1078.67,
                               .background_offset_physical = 0,
                               .done = false}},
              SpringConfig{
                  .timestamp = NextTimeTickAfter(base::Milliseconds(100)),
                  .expected = {.foreground_offset_physical = 1080,
                               .background_offset_physical = 0,
                               .done = true}},
              SpringConfig{
                  .timestamp = NextTimeTickAfter(base::Milliseconds(100)),
                  .expected = {.foreground_offset_physical = 1080,
                               .background_offset_physical = 0,
                               .done = true}},
          },
  };

  for (const auto& gesture_progress : config.gesture_progressed) {
    float movement = std::get<0>(gesture_progress.movement_timestamp);
    base::TimeTicks timestamp =
        std::get<1>(gesture_progress.movement_timestamp);
    PhysicsModel::Result r =
        physics_model()->OnGestureProgressed(movement, timestamp);
    EXPECT_EQ(r, gesture_progress.expected);
  }

  // This simulates a busy browser UI thread where `PhysicsModel::OnAnimate()`
  // isn't even called once after the user lifts the finger.
  physics_model()->OnGestureDone(/*commit=*/true);
  physics_model()->OnDidFinishNavigation(/*navigation_committed=*/true);

  for (const auto& invoke : config.invoke) {
    PhysicsModel::Result r = physics_model()->OnAnimate(invoke.timestamp);
    EXPECT_EQ(r, invoke.expected);
  }
}

// Exercise the finger drag curve and the invoke spring, and skipping the
// commit-stop spring completely. The finger lifts from the screen AFTER the
// commit-stop position.
TEST_F(PhysicsModelUnittest, ProgressInvoke_LiftAfterCommitStop) {
  const TestConfig config{
      .gesture_progressed = TenGestureProgressed(base::Milliseconds(100)),
      .commit_stop = {},
      .cancel = {},
      .invoke =
          {
              SpringConfig{
                  .timestamp = NextTimeTickAfter(base::Milliseconds(100)),
                  .expected = {.foreground_offset_physical = 850,
                               .background_offset_physical = -17,
                               .done = false}},
              SpringConfig{
                  .timestamp = NextTimeTickAfter(base::Milliseconds(100)),
                  .expected = {.foreground_offset_physical = 1055.37,
                               .background_offset_physical = 0,
                               .done = false}},
              SpringConfig{
                  .timestamp = NextTimeTickAfter(base::Milliseconds(100)),
                  .expected = {.foreground_offset_physical = 1079.2,
                               .background_offset_physical = 0,
                               .done = true}},
              SpringConfig{
                  .timestamp = NextTimeTickAfter(base::Milliseconds(100)),
                  .expected = {.foreground_offset_physical = 1080,
                               .background_offset_physical = 0,
                               .done = true}},
          },
  };

  for (const auto& gesture_progress : config.gesture_progressed) {
    // [float, base::TimeTicks]
    auto [movement, timestamp] = gesture_progress.movement_timestamp;
    PhysicsModel::Result r =
        physics_model()->OnGestureProgressed(movement, timestamp);
    EXPECT_EQ(r, gesture_progress.expected);
  }

  physics_model()->OnGestureDone(/*commit=*/true);
  physics_model()->OnDidFinishNavigation(/*navigation_committed=*/true);

  for (const auto& invoke : config.invoke) {
    PhysicsModel::Result r = physics_model()->OnAnimate(invoke.timestamp);
    EXPECT_EQ(r, invoke.expected);
  }
}

// Exercise the finger drag curve, the commit-stop and the invoke springs. The
// finger lifts from the screen BEFORE the commit-stop position.
TEST_F(PhysicsModelUnittest, ProgressCommitStopInvoke_LiftBeforeCommitStop) {
  const TestConfig config{
      .gesture_progressed = NineGestureProgressed(base::Milliseconds(100)),
      .commit_stop =
          {
              SpringConfig{
                  .timestamp = NextTimeTickAfter(base::Milliseconds(50)),
                  .expected = {.foreground_offset_physical = 765,
                               .background_offset_physical = -38.25,
                               .done = false}},
              SpringConfig{
                  .timestamp = NextTimeTickAfter(base::Milliseconds(50)),
                  .expected = {.foreground_offset_physical = 867.43,
                               .background_offset_physical = -12.64,
                               .done = false}},
              SpringConfig{
                  .timestamp = NextTimeTickAfter(base::Milliseconds(50)),
                  .expected = {.foreground_offset_physical = 924.33,
                               .background_offset_physical = 0,
                               .done = false}},
              SpringConfig{
                  .timestamp = NextTimeTickAfter(base::Milliseconds(50)),
                  .expected = {.foreground_offset_physical = 951.18,
                               .background_offset_physical = 0,
                               .done = false}},
              SpringConfig{
                  .timestamp = NextTimeTickAfter(base::Milliseconds(50)),
                  .expected = {.foreground_offset_physical = 959.68,
                               .background_offset_physical = 0,
                               .done = false}},
              // The commit-stop spring is bouncing back (towards the left).
              SpringConfig{
                  .timestamp = NextTimeTickAfter(base::Milliseconds(50)),
                  .expected = {.foreground_offset_physical = 958.07,
                               .background_offset_physical = 0,
                               .done = false}},
              SpringConfig{
                  .timestamp = NextTimeTickAfter(base::Milliseconds(50)),
                  .expected = {.foreground_offset_physical = 951.75,
                               .background_offset_physical = 0,
                               .done = false}},
              SpringConfig{
                  .timestamp = NextTimeTickAfter(base::Milliseconds(50)),
                  .expected = {.foreground_offset_physical = 944.00,
                               .background_offset_physical = 0,
                               .done = false}},
          },
      .cancel = {},
      .invoke =
          {
              SpringConfig{
                  .timestamp = NextTimeTickAfter(base::Milliseconds(100)),
                  .expected = {.foreground_offset_physical = 1060.61,
                               .background_offset_physical = 0,
                               .done = false}},
              SpringConfig{
                  .timestamp = NextTimeTickAfter(base::Milliseconds(100)),
                  .expected = {.foreground_offset_physical = 1079.26,
                               .background_offset_physical = 0,
                               .done = true}},
              SpringConfig{
                  .timestamp = NextTimeTickAfter(base::Milliseconds(100)),
                  .expected = {.foreground_offset_physical = 1080,
                               .background_offset_physical = 0,
                               .done = true}},
              SpringConfig{
                  .timestamp = NextTimeTickAfter(base::Milliseconds(100)),
                  .expected = {.foreground_offset_physical = 1080,
                               .background_offset_physical = 0,
                               .done = true}},
          },
  };

  for (const auto& gesture_progress : config.gesture_progressed) {
    // [float, base::TimeTicks]
    auto [movement, timestamp] = gesture_progress.movement_timestamp;
    PhysicsModel::Result r =
        physics_model()->OnGestureProgressed(movement, timestamp);
    EXPECT_EQ(r, gesture_progress.expected);
  }

  physics_model()->OnGestureDone(/*commit=*/true);

  for (const auto& commit_stop : config.commit_stop) {
    PhysicsModel::Result r = physics_model()->OnAnimate(commit_stop.timestamp);
    EXPECT_EQ(r, commit_stop.expected);
  }

  physics_model()->OnDidFinishNavigation(/*navigation_committed=*/true);

  for (const auto& invoke : config.invoke) {
    PhysicsModel::Result r = physics_model()->OnAnimate(invoke.timestamp);
    EXPECT_EQ(r, invoke.expected);
  }
}

// Exercise the finger drag curve, the commit-stop and the invoke springs. The
// finger lifts from the screen AFTER the commit-stop position.
TEST_F(PhysicsModelUnittest, ProgressCommitStopInvoke_LiftAfterCommitStop) {
  const TestConfig config{
      // Ten gestures: simulate the finger moves from 0px to 1000px.
      .gesture_progressed = TenGestureProgressed(base::Milliseconds(100)),
      .commit_stop =
          {
              SpringConfig{
                  .timestamp = NextTimeTickAfter(base::Milliseconds(50)),
                  .expected = {.foreground_offset_physical = 850,
                               .background_offset_physical = -17,
                               .done = false}},
              SpringConfig{
                  .timestamp = NextTimeTickAfter(base::Milliseconds(50)),
                  .expected = {.foreground_offset_physical = 945.85,
                               .background_offset_physical = 0,
                               .done = false}},
              SpringConfig{
                  .timestamp = NextTimeTickAfter(base::Milliseconds(50)),
                  .expected = {.foreground_offset_physical = 988.71,
                               .background_offset_physical = 0,
                               .done = false}},
              SpringConfig{
                  .timestamp = NextTimeTickAfter(base::Milliseconds(50)),
                  .expected = {.foreground_offset_physical = 999.83,
                               .background_offset_physical = 0,
                               .done = false}},
              SpringConfig{
                  .timestamp = NextTimeTickAfter(base::Milliseconds(50)),
                  .expected = {.foreground_offset_physical = 993.94,
                               .background_offset_physical = 0,
                               .done = false}},
              // The commit-stop spring is bouncing back (towards the left).
              SpringConfig{
                  .timestamp = NextTimeTickAfter(base::Milliseconds(50)),
                  .expected = {.foreground_offset_physical = 980.58,
                               .background_offset_physical = 0,
                               .done = false}},
              SpringConfig{
                  .timestamp = NextTimeTickAfter(base::Milliseconds(50)),
                  .expected = {.foreground_offset_physical = 965.43,
                               .background_offset_physical = 0,
                               .done = false}},
              SpringConfig{
                  .timestamp = NextTimeTickAfter(base::Milliseconds(50)),
                  .expected = {.foreground_offset_physical = 951.49,
                               .background_offset_physical = 0,
                               .done = false}},
          },
      .cancel = {},
      .invoke =
          {
              SpringConfig{
                  .timestamp = NextTimeTickAfter(base::Milliseconds(100)),
                  .expected = {.foreground_offset_physical = 1060.75,
                               .background_offset_physical = 0,
                               .done = false}},
              SpringConfig{
                  .timestamp = NextTimeTickAfter(base::Milliseconds(100)),
                  .expected = {.foreground_offset_physical = 1079.25,
                               .background_offset_physical = 0,
                               .done = true}},
              SpringConfig{
                  .timestamp = NextTimeTickAfter(base::Milliseconds(100)),
                  .expected = {.foreground_offset_physical = 1080,
                               .background_offset_physical = 0,
                               .done = true}},
              SpringConfig{
                  .timestamp = NextTimeTickAfter(base::Milliseconds(100)),
                  .expected = {.foreground_offset_physical = 1080,
                               .background_offset_physical = 0,
                               .done = true}},
          },
  };

  for (const auto& gesture_progress : config.gesture_progressed) {
    // [float, base::TimeTicks]
    auto [movement, timestamp] = gesture_progress.movement_timestamp;
    PhysicsModel::Result r =
        physics_model()->OnGestureProgressed(movement, timestamp);
    EXPECT_EQ(r, gesture_progress.expected);
  }

  physics_model()->OnGestureDone(/*commit=*/true);

  for (const auto& commit_stop : config.commit_stop) {
    PhysicsModel::Result r = physics_model()->OnAnimate(commit_stop.timestamp);
    EXPECT_EQ(r, commit_stop.expected);
  }

  physics_model()->OnDidFinishNavigation(/*navigation_committed=*/true);

  for (const auto& invoke : config.invoke) {
    PhysicsModel::Result r = physics_model()->OnAnimate(invoke.timestamp);
    EXPECT_EQ(r, invoke.expected);
  }
}

// Exercise the finger drag curve and the cancel springs. The finger lifts from
// the screen BEFORE the commit-stop position.
TEST_F(PhysicsModelUnittest, ProgressCancel_LiftBeforeCommitStop) {
  const TestConfig config{
      .gesture_progressed = NineGestureProgressed(base::Milliseconds(100)),
      .commit_stop = {},
      .cancel =
          {
              SpringConfig{
                  .timestamp = NextTimeTickAfter(base::Milliseconds(100)),
                  .expected = {.foreground_offset_physical = 765,
                               .background_offset_physical = -38.25,
                               .done = false}},
              SpringConfig{
                  .timestamp = NextTimeTickAfter(base::Milliseconds(100)),
                  .expected = {.foreground_offset_physical = 111.73,
                               .background_offset_physical = -201.57,
                               .done = false}},
              SpringConfig{
                  .timestamp = NextTimeTickAfter(base::Milliseconds(100)),
                  .expected = {.foreground_offset_physical = 0,
                               .background_offset_physical = -229.5,
                               .done = true}},
              SpringConfig{
                  .timestamp = NextTimeTickAfter(base::Milliseconds(100)),
                  .expected = {.foreground_offset_physical = 0,
                               .background_offset_physical = -229.5,
                               .done = true}},
          },
      .invoke = {},
  };

  for (const auto& gesture_progress : config.gesture_progressed) {
    // [float, base::TimeTicks]
    auto [movement, timestamp] = gesture_progress.movement_timestamp;
    PhysicsModel::Result r =
        physics_model()->OnGestureProgressed(movement, timestamp);
    EXPECT_EQ(r, gesture_progress.expected);
  }

  physics_model()->OnGestureDone(/*commit=*/false);

  for (const auto& cancel : config.cancel) {
    PhysicsModel::Result r = physics_model()->OnAnimate(cancel.timestamp);
    EXPECT_EQ(r, cancel.expected);
  }
}

// Exercise the finger drag curve and the cancel springs. The finger lifts from
// the screen AFTER the commit-stop.
TEST_F(PhysicsModelUnittest, ProgressCancel_LiftAfterCommitStop) {
  const TestConfig config{
      .gesture_progressed = TenGestureProgressed(base::Milliseconds(100)),
      .commit_stop = {},
      .cancel =
          {
              SpringConfig{
                  .timestamp = NextTimeTickAfter(base::Milliseconds(100)),
                  .expected = {.foreground_offset_physical = 850,
                               .background_offset_physical = -17,
                               .done = false}},
              SpringConfig{
                  .timestamp = NextTimeTickAfter(base::Milliseconds(100)),
                  .expected = {.foreground_offset_physical = 122.91,
                               .background_offset_physical = -198.77,
                               .done = false}},
              SpringConfig{
                  .timestamp = NextTimeTickAfter(base::Milliseconds(100)),
                  .expected = {.foreground_offset_physical = 0,
                               .background_offset_physical = -229.5,
                               .done = true}},
              SpringConfig{
                  .timestamp = NextTimeTickAfter(base::Milliseconds(100)),
                  .expected = {.foreground_offset_physical = 0,
                               .background_offset_physical = -229.5,
                               .done = true}},
          },
      .invoke = {},
  };

  for (const auto& gesture_progress : config.gesture_progressed) {
    // [float, base::TimeTicks]
    auto [movement, timestamp] = gesture_progress.movement_timestamp;
    PhysicsModel::Result r =
        physics_model()->OnGestureProgressed(movement, timestamp);
    EXPECT_EQ(r, gesture_progress.expected);
  }

  physics_model()->OnGestureDone(/*commit=*/false);

  for (const auto& cancel : config.cancel) {
    PhysicsModel::Result r = physics_model()->OnAnimate(cancel.timestamp);
    EXPECT_EQ(r, cancel.expected);
  }
}

// Exercise the finger drag curve and the cancel springs, as if the user has
// signal the start of the navigation and the navigation gets cancelled so fast
// that the commit-pending spring hasn't played a single frame.
TEST_F(PhysicsModelUnittest, ProgressAndCancelNav) {
  const TestConfig config{
      .gesture_progressed = NineGestureProgressed(base::Milliseconds(100)),
      .commit_stop = {},
      .cancel =
          {
              SpringConfig{
                  .timestamp = NextTimeTickAfter(base::Milliseconds(50)),
                  .expected = {.foreground_offset_physical = 765,
                               .background_offset_physical = -38.25,
                               .done = false}},
              SpringConfig{
                  .timestamp = NextTimeTickAfter(base::Milliseconds(50)),
                  .expected = {.foreground_offset_physical = 416.61,
                               .background_offset_physical = -125.35,
                               .done = false}},
              SpringConfig{
                  .timestamp = NextTimeTickAfter(base::Milliseconds(50)),
                  .expected = {.foreground_offset_physical = 111.73,
                               .background_offset_physical = -201.57,
                               .done = false}},
              SpringConfig{
                  .timestamp = NextTimeTickAfter(base::Milliseconds(50)),
                  .expected = {.foreground_offset_physical = 17.45,
                               .background_offset_physical = -225.14,
                               .done = false}},
              SpringConfig{
                  .timestamp = NextTimeTickAfter(base::Milliseconds(50)),
                  .expected = {.foreground_offset_physical = 0,
                               .background_offset_physical = -229.5,
                               .done = true}},
          },
      .invoke = {},
  };

  for (const auto& gesture_progress : config.gesture_progressed) {
    // [float, base::TimeTicks]
    auto [movement, timestamp] = gesture_progress.movement_timestamp;
    PhysicsModel::Result r =
        physics_model()->OnGestureProgressed(movement, timestamp);
    EXPECT_EQ(r, gesture_progress.expected);
  }

  physics_model()->OnGestureDone(/*commit=*/true);
  physics_model()->OnDidFinishNavigation(/*navigation_committed=*/false);

  for (const auto& cancel : config.cancel) {
    PhysicsModel::Result r = physics_model()->OnAnimate(cancel.timestamp);
    EXPECT_EQ(r, cancel.expected);
  }
}

// Exercise the finger drag curve, commit pending springs, and the cancel
// springs. This simulates the user has signal the start of the navigation, but
// the navigation gets cancelled, for which we must bring the outgoing live page
// back.
TEST_F(PhysicsModelUnittest, ProgressCommitPendingAndCancelNav) {
  const TestConfig config{
      .gesture_progressed = NineGestureProgressed(base::Milliseconds(100)),
      .commit_stop =
          {
              SpringConfig{
                  .timestamp = NextTimeTickAfter(base::Milliseconds(100)),
                  .expected = {.foreground_offset_physical = 765,
                               .background_offset_physical = -38.25,
                               .done = false}},
              SpringConfig{
                  .timestamp = NextTimeTickAfter(base::Milliseconds(100)),
                  .expected = {.foreground_offset_physical = 924.33,
                               .background_offset_physical = 0,
                               .done = false}},
              SpringConfig{
                  .timestamp = NextTimeTickAfter(base::Milliseconds(100)),
                  .expected = {.foreground_offset_physical = 959.68,
                               .background_offset_physical = 0,
                               .done = false}},
              SpringConfig{
                  .timestamp = NextTimeTickAfter(base::Milliseconds(100)),
                  .expected = {.foreground_offset_physical = 951.75,
                               .background_offset_physical = 0,
                               .done = false}},
          },
      .cancel =
          {
              SpringConfig{
                  .timestamp = NextTimeTickAfter(base::Milliseconds(50)),
                  .expected = {.foreground_offset_physical = 511.11,
                               .background_offset_physical = -101.72,
                               .done = false}},
              SpringConfig{
                  .timestamp = NextTimeTickAfter(base::Milliseconds(50)),
                  .expected = {.foreground_offset_physical = 136.29,
                               .background_offset_physical = -195.43,
                               .done = false}},
              SpringConfig{
                  .timestamp = NextTimeTickAfter(base::Milliseconds(50)),
                  .expected = {.foreground_offset_physical = 21.12,
                               .background_offset_physical = -224.22,
                               .done = false}},
              SpringConfig{
                  .timestamp = NextTimeTickAfter(base::Milliseconds(50)),
                  .expected = {.foreground_offset_physical = 0,
                               .background_offset_physical = -229.5,
                               .done = true}},
          },
      .invoke = {},
  };

  for (const auto& gesture_progress : config.gesture_progressed) {
    // [float, base::TimeTicks]
    auto [movement, timestamp] = gesture_progress.movement_timestamp;
    PhysicsModel::Result r =
        physics_model()->OnGestureProgressed(movement, timestamp);
    EXPECT_EQ(r, gesture_progress.expected);
  }

  physics_model()->OnGestureDone(/*commit=*/true);

  for (const auto& commit_stop : config.commit_stop) {
    PhysicsModel::Result r = physics_model()->OnAnimate(commit_stop.timestamp);
    EXPECT_EQ(r, commit_stop.expected);
  }

  physics_model()->OnDidFinishNavigation(/*navigation_committed=*/false);

  for (const auto& cancel : config.cancel) {
    PhysicsModel::Result r = physics_model()->OnAnimate(cancel.timestamp);
    EXPECT_EQ(r, cancel.expected);
  }
}

}  // namespace content
