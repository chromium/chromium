// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/scheduler/main_thread/performance_helper.h"

#include <algorithm>

#include "base/time/time.h"

namespace blink::scheduler {

using Params = PerformanceHelper::Params;
using BoostType = PerformanceHelper::BoostType;

void PerformanceHelper::Configure(Params params) {
  params_ = std::move(params);
}

base::TimeDelta PerformanceHelper::GetDuration(const BoostType type) const {
  switch (type) {
    case BoostType::kScroll:
      return params_.scrolling_boost;
    case BoostType::kTapOrTyping:
      return params_.input_boost;
    case BoostType::kPageLoad:
      return params_.loading_boost;
  }
}

void PerformanceHelper::Add(const BoostType type, base::TimeTicks now) {
  const base::TimeTicks candidate = now + GetDuration(type);
  expires_at_ = std::max(candidate, expires_at_);
}

void PerformanceHelper::Check(base::TimeTicks cur) {
  // Check whether we're ahead of the expiry.
  const bool boosted = cur < expires_at_;
  if (boosted != boosted_ && params_.callback) {
    // On change, run the callback.
    params_.callback.Run(!boosted);
  }
  boosted_ = boosted;
}

PerformanceHelper::~PerformanceHelper() {
  // Reset everything back to unlimited performance.
  if (params_.callback) {
    params_.callback.Run(false);
  }
}

}  // namespace blink::scheduler
