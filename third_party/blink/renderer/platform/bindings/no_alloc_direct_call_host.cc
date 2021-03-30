// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/bindings/no_alloc_direct_call_host.h"

namespace blink {

void NoAllocDirectCallHost::PostDeferrableAction(DeferrableAction&& action) {
  // This should never be called after NoAllocFallbackForAllocation
#if DCHECK_IS_ON()
  DCHECK(can_post_deferrable_actions_);
#endif

  if (IsInFastMode()) {
    deferred_actions_.push_back(std::move(action));
    callback_options_->fallback = true;
  } else {
    // In slow mode, action is executed immediately.
    std::move(action).Run();
  }
}

bool NoAllocDirectCallHost::NoAllocFallbackForAllocation() {
  if (IsInFastMode()) {
    // Discarding deferred actions means that FlushDeferredActions will return
    // false, which will result in the fallback bindings code path not taking
    // the early exit and re-calling the API implementation with allocations
    // allowed.
    deferred_actions_.clear();
    callback_options_->fallback = true;
#if DCHECK_IS_ON()
    can_post_deferrable_actions_ = false;
#endif
    return true;
  }
  // Already in the fallback code path.
  return false;
}

void NoAllocDirectCallHost::FlushDeferredActions() {
  for (auto& action : deferred_actions_) {
    std::move(action).Run();
  }
  deferred_actions_.clear();
}

}  // namespace blink
