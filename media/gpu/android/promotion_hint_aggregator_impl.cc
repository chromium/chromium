// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/android/promotion_hint_aggregator_impl.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/time/default_tick_clock.h"

namespace media {

// Minimum amount of time between promotable frames before we start over.  The
// idea is to prevent promoting on paused / background rendering.  Note that
// this time is only enforced when transitioning from unpromotable to promotable
// frames.  We don't unpromote later because of this.
constexpr base::TimeDelta MaximumInterFrameTime = base::Milliseconds(100);

// Minimum number of consecutive promotable frames before we actually start
// promoting frames.
constexpr int MinimumPromotableFrames = 10;

// Minimum time since the last unpromotable frame that we require before we will
// promote new ones.
constexpr base::TimeDelta MinimumUnpromotableFrameTime =
    base::Milliseconds(2000);

PromotionHintAggregatorImpl::PromotionHintAggregatorImpl(
    const base::TickClock* tick_clock) {
  if (!tick_clock)
    tick_clock = base::DefaultTickClock::GetInstance();
  tick_clock_ = tick_clock;
}

PromotionHintAggregatorImpl::~PromotionHintAggregatorImpl() {}

void PromotionHintAggregatorImpl::NotifyPromotionHint(const Hint& hint) {
  base::TimeTicks now = tick_clock_->NowTicks();

  if (!hint.is_promotable) {
    most_recent_unpromotable_ = now;
    consecutive_promotable_frames_ = 0;
  } else if (!IsSafeToPromote() &&
             now - most_recent_update_ > MaximumInterFrameTime) {
    // Promotable, but we aren't getting frames fast enough to count.  We
    // don't want to transition to promotable unless frames are actually
    // playing.  We check IsSafeToPromote() so that we don't transition back
    // to unpromotable just because it's paused; that would cause the frame
    // to become unrenderable.  We just want to delay the transition into
    // promotable until it works.
    consecutive_promotable_frames_ = 1;
  } else {
    // Promotable frame, and we're getting frames fast enough.
    consecutive_promotable_frames_++;
  }

  most_recent_update_ = now;
}

bool PromotionHintAggregatorImpl::IsSafeToPromote() {
  base::TimeTicks now = tick_clock_->NowTicks();
  base::TimeDelta since_last_unpromotable = now - most_recent_unpromotable_;

  return consecutive_promotable_frames_ >= MinimumPromotableFrames &&
         since_last_unpromotable >= MinimumUnpromotableFrameTime;
}

}  // namespace media
