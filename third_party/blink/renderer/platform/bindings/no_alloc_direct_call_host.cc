// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/bindings/no_alloc_direct_call_host.h"
#include "third_party/blink/renderer/platform/heap/thread_state.h"

namespace blink {

NoAllocDirectCallHost::NoAllocDirectCallHost()
    : heap_handle_(ThreadState::Current()->cpp_heap().GetHeapHandle()) {}

void NoAllocDirectCallHost::PostDeferrableAction(DeferrableAction&& action) {
  if (IsInFastMode()) {
    deferred_actions_.push_back(std::move(action));
    callback_options_->fallback = true;
  } else {
    // In slow mode, action is executed immediately.
    std::move(action).Run();
  }
}

void NoAllocDirectCallHost::FlushDeferredActions() {
  for (auto& action : deferred_actions_) {
    std::move(action).Run();
  }
  deferred_actions_.clear();
}

}  // namespace blink
