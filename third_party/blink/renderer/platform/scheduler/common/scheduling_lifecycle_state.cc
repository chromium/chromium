// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/scheduler/public/scheduling_lifecycle_state.h"
#include "base/notreached.h"

namespace blink {
namespace scheduler {

// static
const char* SchedulingLifecycleStateToString(SchedulingLifecycleState state) {
  switch (state) {
    case SchedulingLifecycleState::kNotThrottled:
      return "not throttled";
    case SchedulingLifecycleState::kHidden:
      return "hidden";
    case SchedulingLifecycleState::kThrottled:
      return "throttled";
    case SchedulingLifecycleState::kStopped:
      return "frozen";
    default:
      NOTREACHED();
      return nullptr;
  }
}

}  // namespace scheduler
}  // namespace blink
