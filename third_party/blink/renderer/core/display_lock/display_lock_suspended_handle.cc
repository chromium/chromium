// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/display_lock/display_lock_suspended_handle.h"
#include "third_party/blink/renderer/core/display_lock/display_lock_context.h"

namespace blink {

DisplayLockSuspendedHandle::DisplayLockSuspendedHandle(
    DisplayLockContext* context)
    : context_(context) {}

DisplayLockSuspendedHandle::~DisplayLockSuspendedHandle() {}

void DisplayLockSuspendedHandle::Trace(blink::Visitor* visitor) {
  ScriptWrappable::Trace(visitor);
  visitor->Trace(context_);
}

void DisplayLockSuspendedHandle::Dispose() {
  // If we're disposing the handle and we still have a valid reference to the
  // context, it means that this handle was never resumed. In turn, this means
  // that we will never resume the context. We should inform the context of
  // this.
  // TODO(vmpstr): It is possible that we want to resume the context on dispose,
  // making gc observable from script. If that's the case, this should be
  // changed to instead resume the context.
  if (context_)
    context_->NotifyWillNotResume();
  context_ = nullptr;
}

void DisplayLockSuspendedHandle::resume() {
  if (context_)
    context_->Resume();
  context_ = nullptr;
}

}  // namespace blink
