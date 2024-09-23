// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/android/promotion_hint_aggregator_impl.h"

#include <stdint.h>

#include <memory>

#include "base/functional/bind.h"
#include "base/test/simple_test_tick_clock.h"
#include "testing/gtest/include/gtest/gtest.h"


namespace {
// Default elapsed time between frames.
constexpr base::TimeDelta FrameTime = base::Milliseconds(10);
}  // namespace

namespace media {

// Unit tests for PromotionHintAggregatorImplTest
class PromotionHintAggregatorImplTest : public testing::Test {
 public:
  ~PromotionHintAggregatorImplTest() override {}

  void SetUp() override {
    // Advance the clock so that time 0 isn't recent.
    tick_clock_.Advance(base::Seconds(10000));
    impl_ = std::make_unique<PromotionHintAggregatorImpl>(&tick_clock_);
  }

  void TearDown() override {}

  // Sends a new frame that's |is_promotable| or not, with |elapsed| since the
  // previous frame.  Returns whether the video is promotable.
  bool SendFrame(bool is_promotable, base::TimeDelta elapsed = FrameTime) {
    tick_clock_.Advance(elapsed);
    PromotionHintAggregator::Hint hint(gfx::Rect(), is_promotable);
    impl_->NotifyPromotionHint(hint);
    return impl_->IsSafeToPromote();
  }

  base::SimpleTestTickClock tick_clock_;

  std::unique_ptr<PromotionHintAggregatorImpl> impl_;
};

TEST_F(PromotionHintAggregatorImplTest, InitiallyNotPromotable) {
  // A new aggregator shouldn't promote.
  ASSERT_FALSE(impl_->IsSafeToPromote());
}

TEST_F(PromotionHintAggregatorImplTest, SomePromotableFramesArePromotable) {
  // We should have to send 10 frames before promoting.
  for (int i = 0; i < 9; i++)
    ASSERT_FALSE(SendFrame(true));
  ASSERT_TRUE(SendFrame(true));

  // Waiting a while shouldn't cause un-promotion.
  ASSERT_TRUE(SendFrame(true, base::Milliseconds(10000)));
  ASSERT_TRUE(SendFrame(true, base::Milliseconds(10000)));
}

TEST_F(PromotionHintAggregatorImplTest, UnpromotableFramesDelayPromotion) {
  // Start with an unpromotable frame.
  ASSERT_FALSE(SendFrame(false));
  base::TimeTicks start = tick_clock_.NowTicks();

  // Send more until the minimum time has elapsed.  Note that this will also be
  // at least enough promotable frames in a row.
  while (tick_clock_.NowTicks() - start + FrameTime < base::Seconds(2))
    ASSERT_FALSE(SendFrame(true));

  // The next frame should do it.
  ASSERT_TRUE(SendFrame(true));
}

TEST_F(PromotionHintAggregatorImplTest, PromotableFramesMustBeFastEnough) {
  // Send some promotable frames, but not enough to promote.
  for (int i = 0; i < 8; i++)
    ASSERT_FALSE(SendFrame(true));

  // Time passes.
  tick_clock_.Advance(base::Milliseconds(500));

  // We should now start over.
  for (int i = 0; i < 9; i++)
    ASSERT_FALSE(SendFrame(true));
  ASSERT_TRUE(SendFrame(true));
}

}  // namespace media
