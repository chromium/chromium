// Copyright 2025 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_DOM_ELEMENT_ANIMATION_TRIGGER_DATA_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_DOM_ELEMENT_ANIMATION_TRIGGER_DATA_H_

#include "third_party/blink/renderer/core/animation/animation_trigger.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/style/scoped_css_name.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_map.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/member.h"

namespace blink {

class ElementAnimationTriggerData
    : public GarbageCollected<ElementAnimationTriggerData>,
      public ElementRareDataField {
 public:
  void SetNamedTriggers(NamedAnimationTriggerMap& named_triggers);
  NamedAnimationTriggerMap& NamedTriggers();

  void Trace(Visitor*) const override;

 private:
  // A map of all AnimationTriggers declared by CSS on the associated element or
  // within its subtree.
  NamedAnimationTriggerMap named_triggers_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_DOM_ELEMENT_ANIMATION_TRIGGER_DATA_H_
