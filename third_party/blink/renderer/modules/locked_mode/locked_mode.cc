// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/locked_mode/locked_mode.h"

#include "third_party/blink/renderer/core/execution_context/navigator_base.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

// static
LockedMode* LockedMode::lockedMode(NavigatorBase& navigator) {
  LockedMode* locked_mode = navigator.GetLockedMode();
  if (!locked_mode) {
    locked_mode = MakeGarbageCollected<LockedMode>();
    navigator.SetLockedMode(locked_mode);
  }
  return locked_mode;
}

LockedMode::~LockedMode() = default;

void LockedMode::Trace(Visitor* visitor) const {
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
