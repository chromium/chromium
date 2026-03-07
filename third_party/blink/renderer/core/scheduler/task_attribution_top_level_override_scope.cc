// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/scheduler/task_attribution_top_level_override_scope.h"

#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/scheduler/task_attribution_tracker_impl.h"
#include "third_party/blink/renderer/platform/scheduler/public/task_attribution_tracker.h"

namespace blink {

TaskAttributionTopLevelOverrideScope::TaskAttributionTopLevelOverrideScope(
    ExecutionContext* context,
    Type type,
    PassKeyType) {
  if (type == Type::kDoNotOverride) {
    return;
  }
  // If the window is detached, script won't execute, so do nothing.
  if (!context) {
    return;
  }
  tracker_ = static_cast<scheduler::TaskAttributionTrackerImpl*>(
      scheduler::TaskAttributionTracker::From(context->GetIsolate()));
  // `tracker_` will be null if the feature is disabled.
  if (!tracker_) {
    return;
  }
  tracker_->SetShouldOverrideTopLevelCheck(true);
}

TaskAttributionTopLevelOverrideScope::~TaskAttributionTopLevelOverrideScope() {
  // This will have already been cleared if task state was set, but ensure it's
  // cleared in case exiting the scope before actually setting task state.
  if (tracker_) {
    tracker_->SetShouldOverrideTopLevelCheck(false);
  }
}

}  // namespace blink
