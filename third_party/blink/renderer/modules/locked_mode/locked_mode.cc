// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/locked_mode/locked_mode.h"

#include "third_party/blink/renderer/core/execution_context/navigator_base.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

const char LockedMode::kSupplementName[] = "LockedMode";

// static
LockedMode* LockedMode::lockedMode(NavigatorBase& navigator) {
  LockedMode* locked_mode =
      Supplement<NavigatorBase>::From<LockedMode>(navigator);
  if (!locked_mode) {
    locked_mode = MakeGarbageCollected<LockedMode>(navigator);
    ProvideTo(navigator, locked_mode);
  }
  return locked_mode;
}

LockedMode::LockedMode(NavigatorBase& navigator)
    : Supplement<NavigatorBase>(navigator) {}

LockedMode::~LockedMode() = default;

void LockedMode::Trace(Visitor* visitor) const {
  ScriptWrappable::Trace(visitor);
  Supplement<NavigatorBase>::Trace(visitor);
}

}  // namespace blink
