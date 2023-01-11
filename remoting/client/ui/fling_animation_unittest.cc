// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/client/ui/fling_animation.h"

#include <cmath>

#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/test/simple_test_tick_clock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace remoting {

namespace {

const float kFlingTimeConstant = 325.f;

}  // namespace

class FlingAnimationTest : public testing::Test {
 public:
  void SetUp() override;
  void TearDown() override;

 protected:
  void TickAnimation(base::TimeDelta time_delta);

  // Must be called after the animation ticks.
  void AssertDeltaChanged();

  FlingAnimation fling_animation_{
      kFlingTimeConstant,
      base::BindRepeating(&FlingAnimationTest::OnDeltaChanged,
                          base::Unretained(this))};

  float received_dx_ = 0.f;
  float received_dy_ = 0.f;

 private:
  void OnDeltaChanged(float dx, float dy);

  bool change_received_ = false;

  base::SimpleTestTickClock mock_clock_;
};

void FlingAnimationTest::SetUp() {
  fling_animation_.SetTickClockForTest(&mock_clock_);
}

void FlingAnimationTest::TearDown() {
  ASSERT_FALSE(change_received_);
}

void FlingAnimationTest::TickAnimation(base::TimeDelta time_delta) {
  mock_clock_.Advance(time_delta);
  fling_animation_.Tick();
}

void FlingAnimationTest::AssertDeltaChanged() {
  ASSERT_TRUE(change_received_);
  change_received_ = false;
}

void FlingAnimationTest::OnDeltaChanged(float dx, float dy) {
  received_dx_ = dx;
  received_dy_ = dy;
  change_received_ = true;
}

TEST_F(FlingAnimationTest, TestNoFling) {
  EXPECT_FALSE(fling_animation_.IsAnimationInProgress());

  // This should not change the delta.
  TickAnimation(base::Milliseconds(100));
}

TEST_F(FlingAnimationTest, TestFlingWillEventuallyStop) {
  fling_animation_.SetVelocity(1500.f, 1200.f);

  EXPECT_TRUE(fling_animation_.IsAnimationInProgress());

  TickAnimation(base::Minutes(1));

  EXPECT_FALSE(fling_animation_.IsAnimationInProgress());
}

TEST_F(FlingAnimationTest, TestFlingDeltaIsDecreasing) {
  fling_animation_.SetVelocity(1500.f, 1200.f);

  float previous_dx = std::numeric_limits<float>::infinity();
  float previous_dy = std::numeric_limits<float>::infinity();

  while (true) {
    TickAnimation(base::Milliseconds(16));
    if (!fling_animation_.IsAnimationInProgress()) {
      break;
    }
    AssertDeltaChanged();
    float hyp =
        std::sqrt(received_dx_ * received_dx_ + received_dy_ * received_dy_);
    float prev_hyp =
        std::sqrt(previous_dx * previous_dx + previous_dy * previous_dy);
    EXPECT_LT(hyp, prev_hyp);
    previous_dx = received_dx_;
    previous_dy = received_dy_;
  }
}

TEST_F(FlingAnimationTest, TestIgnoreLowVelocity) {
  fling_animation_.SetVelocity(5.f, 5.f);

  EXPECT_FALSE(fling_animation_.IsAnimationInProgress());

  // This should not change the delta.
  TickAnimation(base::Milliseconds(5));
}

TEST_F(FlingAnimationTest, TestAbortAnimation) {
  fling_animation_.SetVelocity(1500.f, 1200.f);

  EXPECT_TRUE(fling_animation_.IsAnimationInProgress());

  TickAnimation(base::Milliseconds(16));
  AssertDeltaChanged();
  EXPECT_TRUE(fling_animation_.IsAnimationInProgress());

  fling_animation_.Abort();
  EXPECT_FALSE(fling_animation_.IsAnimationInProgress());
}

TEST_F(FlingAnimationTest, TestResetVelocity) {
  fling_animation_.SetVelocity(1000.f, -1000.f);
  EXPECT_TRUE(fling_animation_.IsAnimationInProgress());
  TickAnimation(base::Milliseconds(16));
  EXPECT_TRUE(fling_animation_.IsAnimationInProgress());
  AssertDeltaChanged();
  EXPECT_GT(received_dx_, 0);
  EXPECT_LT(received_dy_, 0);

  fling_animation_.SetVelocity(-1000.f, 1000.f);
  EXPECT_TRUE(fling_animation_.IsAnimationInProgress());
  TickAnimation(base::Milliseconds(16));
  EXPECT_TRUE(fling_animation_.IsAnimationInProgress());
  AssertDeltaChanged();
  EXPECT_LT(received_dx_, 0);
  EXPECT_GT(received_dy_, 0);
}

}  // namespace remoting
