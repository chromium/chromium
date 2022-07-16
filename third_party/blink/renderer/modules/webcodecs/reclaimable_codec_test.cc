// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webcodecs/reclaimable_codec.h"

#include "media/base/test_helpers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

namespace {

class FakeReclaimableCodec final
    : public GarbageCollected<FakeReclaimableCodec>,
      public ReclaimableCodec {
 public:
  void SimulateActivity() { MarkCodecActive(); }

  void SimulateReset() { PauseCodecReclamation(); }

  void OnCodecReclaimed(DOMException* ex) final {}

  // GarbageCollected override.
  void Trace(Visitor* visitor) const override {
    ReclaimableCodec::Trace(visitor);
  }
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

}  // namespace blink
