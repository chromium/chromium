// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/forms/element_behavior.h"

#include "third_party/blink/renderer/core/html/custom/element_internals.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

namespace blink {

ElementBehavior::ElementBehavior() {
  DCHECK(RuntimeEnabledFeatures::ElementInternalsBehaviorsEnabled());
}

ElementBehavior::~ElementBehavior() = default;

bool ElementBehavior::HandleActivation(Event&) {
  return false;
}

void ElementBehavior::Trace(Visitor* visitor) const {
  visitor->Trace(internals_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
