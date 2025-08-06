// Copyright 2025 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/dom/element_animation_trigger_data.h"

namespace blink {

void ElementAnimationTriggerData::SetNamedTriggers(
    NamedAnimationTriggerMap& named_triggers) {
  named_triggers_ = std::move(named_triggers);
}

NamedAnimationTriggerMap& ElementAnimationTriggerData::NamedTriggers() {
  return named_triggers_;
}

void ElementAnimationTriggerData::Trace(Visitor* visitor) const {
  visitor->Trace(named_triggers_);
  ElementRareDataField::Trace(visitor);
}

}  // namespace blink
