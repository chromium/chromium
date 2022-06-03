// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/app_history/app_history_transition.h"

#include "third_party/blink/renderer/core/app_history/app_history_entry.h"
#include "third_party/blink/renderer/platform/heap/visitor.h"

namespace blink {
AppHistoryTransition::AppHistoryTransition(const String& navigation_type,
                                           AppHistoryEntry* from)
    : navigation_type_(navigation_type), from_(from) {}

void AppHistoryTransition::Trace(Visitor* visitor) const {
  visitor->Trace(from_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
