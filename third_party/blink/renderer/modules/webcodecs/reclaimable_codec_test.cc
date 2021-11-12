// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webcodecs/reclaimable_codec.h"

#include "base/test/simple_test_tick_clock.h"
#include "base/time/default_tick_clock.h"
#include "media/base/test_helpers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

namespace {

class FakeReclaimableCodec final
    : public GarbageCollected<FakeReclaimableCodec>,
      public ReclaimableCodec {
 public:
  void SimulateActivity() {
    MarkCodecActive();
    reclaimed_ = false;
  }

  void SimulateReset() { PauseCodecReclamation(); }

  void OnCodecReclaimed(DOMException* ex) final { reclaimed_ = true; }

  // GarbageCollected override.
  void Trace(Visitor* visitor) const override {
    ReclaimableCodec::Trace(visitor);
  }

  bool reclaimed() const { return reclaimed_; }

 private:
  bool reclaimed_ = false;
};

}  // namespace

class ReclaimableCodecTest : public testing::Test {
 public:
  ReclaimableCodecTest() = default;
  ~ReclaimableCodecTest() override = default;
};

TEST_F(ReclaimableCodecTest, InactivityTimerStartStops) {
  auto* codec = MakeGarbageCollected<FakeReclaimableCodec>();

  // Codecs should not be reclaimable for inactivity until first activity.
  EXPECT_FALSE(codec->IsReclamationTimerActiveForTesting());

  codec->SimulateReset();
  EXPECT_FALSE(codec->IsReclamationTimerActiveForTesting());

  // Activity should start the timer.
  codec->SimulateActivity();
  EXPECT_TRUE(codec->IsReclamationTimerActiveForTesting());

  // More activity should not stop the timer.
  codec->SimulateActivity();
  EXPECT_TRUE(codec->IsReclamationTimerActiveForTesting());

  // The timer should be stopped when asked.
  codec->SimulateReset();
  EXPECT_FALSE(codec->IsReclamationTimerActiveForTesting());

  // It should be possible to restart the timer after stopping it.
  codec->SimulateActivity();
  EXPECT_TRUE(codec->IsReclamationTimerActiveForTesting());
}

TEST_F(ReclaimableCodecTest, InactivityTimerWorks) {
  auto* codec = MakeGarbageCollected<FakeReclaimableCodec>();

  // Codecs should not be reclaimable for inactivity until first activity.
  EXPECT_FALSE(codec->IsReclamationTimerActiveForTesting());

  base::SimpleTestTickClock tick_clock;
  codec->set_tick_clock_for_testing(&tick_clock);

  // Activity should start the timer.
  codec->SimulateActivity();
  EXPECT_TRUE(codec->IsReclamationTimerActiveForTesting());
  EXPECT_FALSE(codec->reclaimed());

  // Fire when codec is fresh to ensure first tick isn't treated as idle.
  codec->SimulateActivityTimerFiredForTesting();
  EXPECT_FALSE(codec->reclaimed());

  // One timer period should not be enough to reclaim the codec.
  tick_clock.Advance(ReclaimableCodec::kTimerPeriod);
  codec->SimulateActivityTimerFiredForTesting();
  EXPECT_FALSE(codec->reclaimed());

  // Advancing an additional timer period should be enough to trigger
  // reclamation.
  tick_clock.Advance(ReclaimableCodec::kTimerPeriod);
  codec->SimulateActivityTimerFiredForTesting();
  EXPECT_TRUE(codec->reclaimed());

  // Restore default tick clock since |codec| is a garbage collected object that
  // may outlive the scope of this function.
  codec->set_tick_clock_for_testing(base::DefaultTickClock::GetInstance());
}

}  // namespace blink
