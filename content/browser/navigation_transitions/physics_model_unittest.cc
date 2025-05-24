// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/navigation_transitions/physics_model.h"

#include <memory>

#include "base/numerics/ranges.h"
#include "base/strings/to_string.h"
#include "base/time/time.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

namespace {

static constexpr float kScreenWidthForTesting = 1080.f;

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
        /*screen_width=*/static_cast<int>(kScreenWidthForTesting),
        /*device_scale_factor=*/2.625);
  }

  // Nine gestures: simulate the finger moves from 0px to 900px, before the
  // commit-stop 918px.
  // Every 100px finger move -> every 85px foreground layer move -> every 25px
  // background layer move.
  std::vector<FingerDragCurveConfig> NineGestureProgressed(
      base::TimeDelta increment) {
    return {
        FingerDragCurveConfig{
            .movement_timestamp = {100.f,
                                   NextTimeTickAfter(base::Milliseconds(0))},
            .expected = PhysicsModel::Result{.foreground_offset_physical = 85,
                                             .background_offset_physical = -245,
                                             .done = false}},
        FingerDragCurveConfig{
            .movement_timestamp = {100.f, NextTimeTickAfter(increment)},
            .expected = PhysicsModel::Result{.foreground_offset_physical = 170,
                                             .background_offset_physical = -220,
                                             .done = false}},
        FingerDragCurveConfig{
            .movement_timestamp = {100.f, NextTimeTickAfter(increment)},
            .expected = PhysicsModel::Result{.foreground_offset_physical = 255,
                                             .background_offset_physical = -195,
                                             .done = false}},
        FingerDragCurveConfig{
            .movement_timestamp = {100.f, NextTimeTickAfter(increment)},
            .expected = PhysicsModel::Result{.foreground_offset_physical = 340,
                                             .background_offset_physical = -170,
                                             .done = false}},
        FingerDragCurveConfig{
            .movement_timestamp = {100.f, NextTimeTickAfter(increment)},
            .expected = PhysicsModel::Result{.foreground_offset_physical = 425,
                                             .background_offset_physical = -145,
                                             .done = false}},
        FingerDragCurveConfig{
            .movement_timestamp = {100.f, NextTimeTickAfter(increment)},
            .expected = PhysicsModel::Result{.foreground_offset_physical = 510,
                                             .background_offset_physical = -120,
                                             .done = false}},
        FingerDragCurveConfig{
            .movement_timestamp = {100.f, NextTimeTickAfter(increment)},
            .expected = PhysicsModel::Result{.foreground_offset_physical = 595,
                                             .background_offset_physical = -95,
                                             .done = false}},
        FingerDragCurveConfig{
            .movement_timestamp = {100.f, NextTimeTickAfter(increment)},
            .expected = PhysicsModel::Result{.foreground_offset_physical = 680,
                                             .background_offset_physical = -70,
                                             .done = false}},
        FingerDragCurveConfig{
            .movement_timestamp = {100.f, NextTimeTickAfter(increment)},
            .expected = PhysicsModel::Result{.foreground_offset_physical = 765,
                                             .background_offset_physical = -45,
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
        .expected = PhysicsModel::Result{.foreground_offset_physical = 850.f,
                                         .background_offset_physical = -20.f,
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
     << " done: " << base::ToString(r.done);
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
                               .background_offset_physical = -45,
                               .done = false}},
              // The foreground has reached the commit-stop point. From this
              // point on the background will have offset=0 - it will not
              // bounce.
              SpringConfig{
                  .timestamp = NextTimeTickAfter(base::Milliseconds(100)),
                  .expected = {.foreground_offset_physical = 1034.51,
                               .background_offset_physical = 0,
                               .done = false}},
              SpringConfig{
                  .timestamp = NextTimeTickAfter(base::Milliseconds(100)),
                  .expected = {.foreground_offset_physical = 1078.25,
                               .background_offset_physical = 0,
                               .done = false}},
              SpringConfig{
                  .timestamp = NextTimeTickAfter(base::Milliseconds(100)),
                  .expected = {.foreground_offset_physical =
                                   kScreenWidthForTesting,
                               .background_offset_physical = 0,
                               .done = true}},
              SpringConfig{
                  .timestamp = NextTimeTickAfter(base::Milliseconds(100)),
                  .expected = {.foreground_offset_physical =
                                   kScreenWidthForTesting,
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
  physics_model()->SwitchSpringForReason(
      PhysicsModel::SwitchSpringReason::kGestureInvoked);
  physics_model()->OnNavigationFinished(/*navigation_committed=*/true);

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
                               .background_offset_physical = -20,
                               .done = false}},
              SpringConfig{
                  .timestamp = NextTimeTickAfter(base::Milliseconds(100)),
                  .expected = {.foreground_offset_physical = 1047.77f,
                               .background_offset_physical = 0.f,
                               .done = false}},
              SpringConfig{
                  .timestamp = NextTimeTickAfter(base::Milliseconds(100)),
                  .expected = {.foreground_offset_physical = 1078.77f,
                               .background_offset_physical = 0,
                               .done = false}},
              SpringConfig{
                  .timestamp = NextTimeTickAfter(base::Milliseconds(100)),
                  .expected = {.foreground_offset_physical =
                                   kScreenWidthForTesting,
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

  physics_model()->SwitchSpringForReason(
      PhysicsModel::SwitchSpringReason::kGestureInvoked);
  physics_model()->OnNavigationFinished(/*navigation_committed=*/true);

  for (const auto& invoke : config.invoke) {
    PhysicsModel::Result r = physics_model()->OnAnimate(invoke.timestamp);
    EXPECT_EQ(r, invoke.expected);
  }
}

// Exercise the finger drag curve, the commit-stop and the invoke springs. The
// finger lifts from the screen BEFORE the commit-stop position.
TEST_F(PhysicsModelUnittest, ProgressCommitStopInvoke_LiftBeforeCommitStop) {
  const TestConfig config{
      .gesture_progressed = NineGestureProgressed(base::Milliseconds(50)),
      .commit_stop =
          {
              SpringConfig{
                  .timestamp = NextTimeTickAfter(base::Milliseconds(50)),
                  .expected = {.foreground_offset_physical = 765,
                               .background_offset_physical = -45,
                               .done = false}},
              SpringConfig{
                  .timestamp = NextTimeTickAfter(base::Milliseconds(50)),
                  .expected = {.foreground_offset_physical = 835.502f,
                               .background_offset_physical = -24.264f,
                               .done = false}},
              SpringConfig{
                  .timestamp = NextTimeTickAfter(base::Milliseconds(50)),
                  .expected = {.foreground_offset_physical = 881.257f,
                               .background_offset_physical = -10.807f,
                               .done = false}},
              SpringConfig{
                  .timestamp = NextTimeTickAfter(base::Milliseconds(50)),
                  .expected = {.foreground_offset_physical = 908.63f,
                               .background_offset_physical = -2.756f,
                               .done = false}},
              SpringConfig{
                  .timestamp = NextTimeTickAfter(base::Milliseconds(50)),
                  .expected = {.foreground_offset_physical = 923.264f,
                               .background_offset_physical = 0.f,
                               .done = false}},
              SpringConfig{
                  .timestamp = NextTimeTickAfter(base::Milliseconds(50)),
                  .expected = {.foreground_offset_physical = 929.668f,
                               .background_offset_physical = 0.f,
                               .done = false}},
              SpringConfig{
                  .timestamp = NextTimeTickAfter(base::Milliseconds(50)),
                  .expected = {.foreground_offset_physical = 931.177f,
                               .background_offset_physical = 0.f,
                               .done = false}},
              // The commit-stop spring is bouncing back (towards the left).
              SpringConfig{
                  .timestamp = NextTimeTickAfter(base::Milliseconds(50)),
                  .expected = {.foreground_offset_physical = 930.092f,
                               .background_offset_physical = 0,
                               .done = false}},
              SpringConfig{
                  .timestamp = NextTimeTickAfter(base::Milliseconds(50)),
                  .expected = {.foreground_offset_physical = 927.892f,
                               .background_offset_physical = 0.f,
                               .done = false}},
          },
      .cancel = {},
      .invoke =
          {
              SpringConfig{
                  .timestamp = NextTimeTickAfter(base::Milliseconds(100)),
                  .expected = {.foreground_offset_physical = 1057.78f,
                               .background_offset_physical = 0.f,
                               .done = false}},
              SpringConfig{
                  .timestamp = NextTimeTickAfter(base::Milliseconds(100)),
                  .expected = {.foreground_offset_physical = 1079.14f,
                               .background_offset_physical = 0.f,
                               .done = true}},
              SpringConfig{
                  .timestamp = NextTimeTickAfter(base::Milliseconds(100)),
                  .expected = {.foreground_offset_physical =
                                   kScreenWidthForTesting,
                               .background_offset_physical = 0.f,
                               .done = true}},
              SpringConfig{
                  .timestamp = NextTimeTickAfter(base::Milliseconds(100)),
                  .expected = {.foreground_offset_physical =
                                   kScreenWidthForTesting,
                               .background_offset_physical = 0.f,
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

  physics_model()->SwitchSpringForReason(
      PhysicsModel::SwitchSpringReason::kGestureInvoked);

  for (const auto& commit_stop : config.commit_stop) {
    PhysicsModel::Result r = physics_model()->OnAnimate(commit_stop.timestamp);
    EXPECT_EQ(r, commit_stop.expected);
  }

  physics_model()->OnNavigationFinished(/*navigation_committed=*/true);

  for (const auto& invoke : config.invoke) {
    PhysicsModel::Result r = physics_model()->OnAnimate(invoke.timestamp);
    EXPECT_EQ(r, invoke.expected);
  }
}

TEST_F(PhysicsModelUnittest, ProgressCommitStopInvoke_OneGesture) {
  const TestConfig config{
      .gesture_progressed =
          {
              FingerDragCurveConfig{
                  .movement_timestamp = {700.f, NextTimeTickAfter(
                                                    base::Milliseconds(0))},
                  .expected =
                      PhysicsModel::Result{.foreground_offset_physical = 595,
                                           .background_offset_physical = -95,
                                           .done = false}},
          },
      .commit_stop =
          {
              // The speed is initially assumed as zero, because there aren't
              // enough data points.
              SpringConfig{
                  .timestamp = NextTimeTickAfter(base::Milliseconds(100)),
                  .expected = {.foreground_offset_physical = 595,
                               .background_offset_physical = -95,
                               .done = false}},
              // Then it picks up.
              SpringConfig{
                  .timestamp = NextTimeTickAfter(base::Milliseconds(100)),
                  .expected = {.foreground_offset_physical = 673.327,
                               .background_offset_physical = -71.9627,
                               .done = false}},
              SpringConfig{
                  .timestamp = NextTimeTickAfter(base::Milliseconds(100)),
                  .expected = {.foreground_offset_physical = 787.81,
                               .background_offset_physical = -38.2911,
                               .done = false}},
          },
      .cancel = {},
      .invoke =
          {
              // Swings to the right, then stops.
              SpringConfig{
                  .timestamp = NextTimeTickAfter(base::Milliseconds(100)),
                  .expected = {.foreground_offset_physical = 1038.56,
                               .background_offset_physical = 0,
                               .done = false}},
              SpringConfig{
                  .timestamp = NextTimeTickAfter(base::Milliseconds(100)),
                  .expected = {.foreground_offset_physical = 1078.42,
                               .background_offset_physical = 0,
                               .done = false}},
              SpringConfig{
                  .timestamp = NextTimeTickAfter(base::Milliseconds(100)),
                  .expected = {.foreground_offset_physical =
                                   kScreenWidthForTesting,
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

  physics_model()->SwitchSpringForReason(
      PhysicsModel::SwitchSpringReason::kGestureInvoked);

  for (const auto& commit_stop : config.commit_stop) {
    PhysicsModel::Result r = physics_model()->OnAnimate(commit_stop.timestamp);
    EXPECT_EQ(r, commit_stop.expected);
  }

  physics_model()->OnNavigationFinished(/*navigation_committed=*/true);

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
      .gesture_progressed = TenGestureProgressed(base::Milliseconds(50)),
      .commit_stop =
          {
              SpringConfig{
                  .timestamp = NextTimeTickAfter(base::Milliseconds(50)),
                  .expected = {.foreground_offset_physical = 850,
                               .background_offset_physical = -20,
                               .done = false}},
              SpringConfig{
                  .timestamp = NextTimeTickAfter(base::Milliseconds(50)),
                  .expected = {.foreground_offset_physical = 913.929f,
                               .background_offset_physical = -1.198f,
                               .done = false}},
              SpringConfig{
                  .timestamp = NextTimeTickAfter(base::Milliseconds(50)),
                  .expected = {.foreground_offset_physical = 945.645f,
                               .background_offset_physical = 0.f,
                               .done = false}},
              SpringConfig{
                  .timestamp = NextTimeTickAfter(base::Milliseconds(50)),
                  .expected = {.foreground_offset_physical = 957.281f,
                               .background_offset_physical = 0.f,
                               .done = false}},
              SpringConfig{
                  .timestamp = NextTimeTickAfter(base::Milliseconds(50)),
                  .expected = {.foreground_offset_physical = 957.525f,
                               .background_offset_physical = 0.f,
                               .done = false}},
              // The commit-stop spring is bouncing back (towards the left).
              SpringConfig{
                  .timestamp = NextTimeTickAfter(base::Milliseconds(50)),
                  .expected = {.foreground_offset_physical = 952.183f,
                               .background_offset_physical = 0.f,
                               .done = false}},
              SpringConfig{
                  .timestamp = NextTimeTickAfter(base::Milliseconds(50)),
                  .expected = {.foreground_offset_physical = 944.857f,
                               .background_offset_physical = 0.f,
                               .done = false}},
              SpringConfig{
                  .timestamp = NextTimeTickAfter(base::Milliseconds(50)),
                  .expected = {.foreground_offset_physical = 937.577f,
                               .background_offset_physical = 0.f,
                               .done = false}},
          },
      .cancel = {},
      .invoke =
          {
              SpringConfig{
                  .timestamp = NextTimeTickAfter(base::Milliseconds(100)),
                  .expected = {.foreground_offset_physical = 1058.59f,
                               .background_offset_physical = 0.f,
                               .done = false}},
              SpringConfig{
                  .timestamp = NextTimeTickAfter(base::Milliseconds(100)),
                  .expected = {.foreground_offset_physical = 1079.16f,
                               .background_offset_physical = 0.f,
                               .done = true}},
              SpringConfig{
                  .timestamp = NextTimeTickAfter(base::Milliseconds(100)),
                  .expected = {.foreground_offset_physical =
                                   kScreenWidthForTesting,
                               .background_offset_physical = 0,
                               .done = true}},
              SpringConfig{
                  .timestamp = NextTimeTickAfter(base::Milliseconds(100)),
                  .expected = {.foreground_offset_physical =
                                   kScreenWidthForTesting,
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

  physics_model()->SwitchSpringForReason(
      PhysicsModel::SwitchSpringReason::kGestureInvoked);

  for (const auto& commit_stop : config.commit_stop) {
    PhysicsModel::Result r = physics_model()->OnAnimate(commit_stop.timestamp);
    EXPECT_EQ(r, commit_stop.expected);
  }

  physics_model()->OnNavigationFinished(/*navigation_committed=*/true);

  for (const auto& invoke : config.invoke) {
    PhysicsModel::Result r = physics_model()->OnAnimate(invoke.timestamp);
    EXPECT_EQ(r, invoke.expected);
  }
}

// Exercise the finger drag curve and the cancel springs. The finger lifts from
// the screen BEFORE the commit-stop position.
TEST_F(PhysicsModelUnittest, ProgressCancel_LiftBeforeCommitStop) {
  const TestConfig config{
      .gesture_progressed = NineGestureProgressed(base::Milliseconds(10)),
      .commit_stop = {},
      .cancel =
          {
              SpringConfig{
                  .timestamp = NextTimeTickAfter(base::Milliseconds(10)),
                  .expected = {.foreground_offset_physical = 765,
                               .background_offset_physical = -45,
                               .done = false}},
              // Due to gesture speed, the foreground initially continues to
              // move right.
              SpringConfig{
                  .timestamp = NextTimeTickAfter(base::Milliseconds(10)),
                  .expected = {.foreground_offset_physical = 797.577f,
                               .background_offset_physical = -35.419f,
                               .done = false}},
              // And then bounces to the left.
              SpringConfig{
                  .timestamp = NextTimeTickAfter(base::Milliseconds(10)),
                  .expected = {.foreground_offset_physical = 756.449f,
                               .background_offset_physical = -47.515f,
                               .done = false}},
              SpringConfig{
                  .timestamp = NextTimeTickAfter(base::Milliseconds(100)),
                  .expected = {.foreground_offset_physical = 71.895f,
                               .background_offset_physical = -248.854f,
                               .done = false}},
              SpringConfig{
                  .timestamp = NextTimeTickAfter(base::Milliseconds(100)),
                  .expected = {.foreground_offset_physical = 0.f,
                               .background_offset_physical = -270.f,
                               .done = true}},
              SpringConfig{
                  .timestamp = NextTimeTickAfter(base::Milliseconds(100)),
                  .expected = {.foreground_offset_physical = 0.f,
                               .background_offset_physical = -270.f,
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

  physics_model()->SwitchSpringForReason(
      PhysicsModel::SwitchSpringReason::kGestureCancelled);

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
                  .expected = {.foreground_offset_physical = 850.f,
                               .background_offset_physical = -20.f,
                               .done = false}},
              SpringConfig{
                  .timestamp = NextTimeTickAfter(base::Milliseconds(100)),
                  .expected = {.foreground_offset_physical = 115.395f,
                               .background_offset_physical = -236.06f,
                               .done = false}},
              SpringConfig{
                  .timestamp = NextTimeTickAfter(base::Milliseconds(100)),
                  .expected = {.foreground_offset_physical = 0.f,
                               .background_offset_physical = -270.f,
                               .done = true}},
              SpringConfig{
                  .timestamp = NextTimeTickAfter(base::Milliseconds(100)),
                  .expected = {.foreground_offset_physical = 0.f,
                               .background_offset_physical = -270.f,
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

  physics_model()->SwitchSpringForReason(
      PhysicsModel::SwitchSpringReason::kGestureCancelled);

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
      .gesture_progressed =
          {
              FingerDragCurveConfig{
                  .movement_timestamp = {100.f, NextTimeTickAfter(
                                                    base::Milliseconds(0))},
                  .expected =
                      PhysicsModel::Result{.foreground_offset_physical = 85,
                                           .background_offset_physical = -245,
                                           .done = false}},
              FingerDragCurveConfig{
                  .movement_timestamp = {100.f, NextTimeTickAfter(
                                                    base::Milliseconds(20))},
                  .expected =
                      PhysicsModel::Result{.foreground_offset_physical = 170,
                                           .background_offset_physical = -220,
                                           .done = false}},
              FingerDragCurveConfig{
                  .movement_timestamp = {100.f, NextTimeTickAfter(
                                                    base::Milliseconds(20))},
                  .expected =
                      PhysicsModel::Result{.foreground_offset_physical = 255,
                                           .background_offset_physical = -195,
                                           .done = false}},
              FingerDragCurveConfig{
                  .movement_timestamp = {-100.f, NextTimeTickAfter(
                                                     base::Milliseconds(20))},
                  .expected =
                      PhysicsModel::Result{.foreground_offset_physical = 170,
                                           .background_offset_physical = -220,
                                           .done = false}},
              FingerDragCurveConfig{
                  .movement_timestamp = {-100.f, NextTimeTickAfter(
                                                     base::Milliseconds(20))},
                  .expected =
                      PhysicsModel::Result{.foreground_offset_physical = 85,
                                           .background_offset_physical = -245,
                                           .done = false}},
              FingerDragCurveConfig{
                  .movement_timestamp = {-80.f, NextTimeTickAfter(
                                                    base::Milliseconds(20))},
                  .expected =
                      PhysicsModel::Result{.foreground_offset_physical = 17,
                                           .background_offset_physical = -265,
                                           .done = false}},
          },
      .commit_stop = {},
      .cancel =
          {
              SpringConfig{
                  .timestamp = NextTimeTickAfter(base::Milliseconds(10)),
                  .expected = {.foreground_offset_physical = 17,
                               .background_offset_physical = -265,
                               .done = false}},
              // The spring moves the foreground to the left from the first
              // iteration.
              SpringConfig{
                  .timestamp = NextTimeTickAfter(base::Milliseconds(10)),
                  .expected = {.foreground_offset_physical = 9.006f,
                               .background_offset_physical = -267.351,
                               .done = false}},
              SpringConfig{
                  .timestamp = NextTimeTickAfter(base::Milliseconds(10)),
                  .expected = {.foreground_offset_physical = 3.791f,
                               .background_offset_physical = -268.885f,
                               .done = false}},
              SpringConfig{
                  .timestamp = NextTimeTickAfter(base::Milliseconds(10)),
                  .expected = {.foreground_offset_physical = 0.545f,
                               .background_offset_physical = -269.84f,
                               .done = true}},
              SpringConfig{
                  .timestamp = NextTimeTickAfter(base::Milliseconds(10)),
                  .expected = {.foreground_offset_physical = 0.f,
                               .background_offset_physical = -270,
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

  physics_model()->SwitchSpringForReason(
      PhysicsModel::SwitchSpringReason::kGestureInvoked);
  physics_model()->OnNavigationFinished(/*navigation_committed=*/false);

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
      .gesture_progressed = NineGestureProgressed(base::Milliseconds(25)),
      .commit_stop =
          {
              SpringConfig{
                  .timestamp = NextTimeTickAfter(base::Milliseconds(100)),
                  .expected = {.foreground_offset_physical = 765.f,
                               .background_offset_physical = -45.f,
                               .done = false}},
              SpringConfig{
                  .timestamp = NextTimeTickAfter(base::Milliseconds(100)),
                  .expected = {.foreground_offset_physical = 960.412f,
                               .background_offset_physical = 0.f,
                               .done = false}},
              SpringConfig{
                  .timestamp = NextTimeTickAfter(base::Milliseconds(100)),
                  .expected = {.foreground_offset_physical = 990.197f,
                               .background_offset_physical = 0.f,
                               .done = false}},
              SpringConfig{
                  .timestamp = NextTimeTickAfter(base::Milliseconds(100)),
                  .expected = {.foreground_offset_physical = 968.978f,
                               .background_offset_physical = 0.f,
                               .done = false}},
          },
      .cancel =
          {
              SpringConfig{
                  .timestamp = NextTimeTickAfter(base::Milliseconds(50)),
                  .expected = {.foreground_offset_physical = 519.829f,
                               .background_offset_physical = -117.109f,
                               .done = false}},
              SpringConfig{
                  .timestamp = NextTimeTickAfter(base::Milliseconds(50)),
                  .expected = {.foreground_offset_physical = 138.558f,
                               .background_offset_physical = -229.248f,
                               .done = false}},
              SpringConfig{
                  .timestamp = NextTimeTickAfter(base::Milliseconds(50)),
                  .expected = {.foreground_offset_physical = 21.457f,
                               .background_offset_physical = -263.689f,
                               .done = false}},
              SpringConfig{
                  .timestamp = NextTimeTickAfter(base::Milliseconds(100)),
                  .expected = {.foreground_offset_physical = 0.f,
                               .background_offset_physical = -270.f,
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

  physics_model()->SwitchSpringForReason(
      PhysicsModel::SwitchSpringReason::kGestureInvoked);

  for (const auto& commit_stop : config.commit_stop) {
    PhysicsModel::Result r = physics_model()->OnAnimate(commit_stop.timestamp);
    EXPECT_EQ(r, commit_stop.expected);
  }

  physics_model()->OnNavigationFinished(/*navigation_committed=*/false);

  for (const auto& cancel : config.cancel) {
    PhysicsModel::Result r = physics_model()->OnAnimate(cancel.timestamp);
    EXPECT_EQ(r, cancel.expected);
  }
}

// Regression test for https://crbug.com/326850774: The CommitPending spring
// shouldn't overshoot the left edge neither.
TEST_F(PhysicsModelUnittest, CommitPendingSpringOvershootLeftEdge) {
  // Simulating a fling from 1000px to 0px.
  physics_model()->OnGestureProgressed(
      1000, NextTimeTickAfter(base::Milliseconds(0)));
  // Ten data points so we can evict the first gesture (0px to 1000px). Makes
  // sure that this sequence carries enough speed.
  for (int i = 0; i < 10; ++i) {
    physics_model()->OnGestureProgressed(
        -100, NextTimeTickAfter(base::Milliseconds(1)));
  }

  // Lift the finger. The physics model will switch to the commit-pending
  // spring. The spring will have initial position at the left edge, and with
  // the initial velocity towards the left. Without the clampping, the spring
  // will keep moving to the left, which is incorrect.
  physics_model()->SwitchSpringForReason(
      PhysicsModel::SwitchSpringReason::kGestureInvoked);
  PhysicsModel::Result first_frame =
      physics_model()->OnAnimate(NextTimeTickAfter(base::Milliseconds(100)));
  EXPECT_GE(first_frame.foreground_offset_physical, 0.f);
  EXPECT_LE(first_frame.foreground_offset_physical, kScreenWidthForTesting);
  PhysicsModel::Result second_frame =
      physics_model()->OnAnimate(NextTimeTickAfter(base::Milliseconds(100)));
  EXPECT_GE(second_frame.foreground_offset_physical, 0.f);
  EXPECT_LE(second_frame.foreground_offset_physical, kScreenWidthForTesting);
  PhysicsModel::Result third_frame =
      physics_model()->OnAnimate(NextTimeTickAfter(base::Milliseconds(100)));
  EXPECT_GE(third_frame.foreground_offset_physical, 0.f);
  EXPECT_LE(third_frame.foreground_offset_physical, kScreenWidthForTesting);
}

}  // namespace content
