// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/accessibility/ax_object_cache_lifecycle.h"

#include "base/notreached.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

#if DCHECK_IS_ON()
bool AXObjectCacheLifecycle::CanAdvanceTo(LifecycleState next_state) const {
  // Can dispose from anywhere, unless already in process or complete.
  if (next_state == kDisposing) {
    return state_ != kDisposing && state_ != kDisposed;
  }

  switch (state_) {
    case kUninitialized:
      return next_state == kDeferTreeUpdates;
    case kDeferTreeUpdates:
      return next_state == kProcessDeferredUpdates;
    case kProcessDeferredUpdates:
      return next_state == kFinalizingTree;
    case kFinalizingTree:
      return next_state == kSerialize;
    case kSerialize:
      return false;
    case kDisposing:
      return next_state == kDisposed;
    case kDisposed:
      return false;
  }
  return false;
}

bool AXObjectCacheLifecycle::CanRewindTo(LifecycleState next_state) const {
  return next_state == kDeferTreeUpdates && state_ != kDisposing &&
         state_ != kDisposed;
}
#endif

#define DEBUG_STRING_CASE(StateName)      \
  case AXObjectCacheLifecycle::StateName: \
    return #StateName

static WTF::String StateAsDebugString(
    const AXObjectCacheLifecycle::LifecycleState& state) {
  switch (state) {
    DEBUG_STRING_CASE(kUninitialized);
    DEBUG_STRING_CASE(kDeferTreeUpdates);
    DEBUG_STRING_CASE(kProcessDeferredUpdates);
    DEBUG_STRING_CASE(kFinalizingTree);
    DEBUG_STRING_CASE(kSerialize);
    DEBUG_STRING_CASE(kDisposing);
    DEBUG_STRING_CASE(kDisposed);
  }

  NOTREACHED();
}

WTF::String AXObjectCacheLifecycle::ToString() const {
  return StateAsDebugString(state_);
}

void AXObjectCacheLifecycle::AdvanceTo(LifecycleState next_state) {
#if DCHECK_IS_ON()
  DCHECK(CanAdvanceTo(next_state))
      << "Cannot advance a11y lifecycle from " << StateAsDebugString(state_)
      << " to " << StateAsDebugString(next_state) << ".";
#endif
  state_ = next_state;
}

void AXObjectCacheLifecycle::EnsureStateAtMost(LifecycleState state) {
#if DCHECK_IS_ON()
  DCHECK(CanRewindTo(state))
      << "Cannot rewind a11y lifecycle from " << StateAsDebugString(state_)
      << " to " << StateAsDebugString(state) << ".";
#endif
  state_ = state;
}

}  // namespace blink
