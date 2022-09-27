// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/svg/animation/element_smil_animations.h"

#include "third_party/blink/renderer/core/svg/animation/smil_animation_sandwich.h"

namespace blink {

ElementSMILAnimations::ElementSMILAnimations() = default;

void ElementSMILAnimations::AddAnimation(const QualifiedName& attribute,
                                         SVGAnimationElement* animation) {
  auto& sandwich = sandwiches_.insert(attribute, nullptr).stored_value->value;
  if (!sandwich)
    sandwich = MakeGarbageCollected<SMILAnimationSandwich>();

  sandwich->Add(animation);
}

void ElementSMILAnimations::RemoveAnimation(const QualifiedName& attribute,
                                            SVGAnimationElement* animation) {
  auto it = sandwiches_.find(attribute);
  CHECK(it != sandwiches_.end());

  auto& sandwich = *it->value;
  sandwich.Remove(animation);

  if (sandwich.IsEmpty())
    sandwiches_.erase(it);
}

bool ElementSMILAnimations::Apply(SMILTime elapsed) {
  bool did_apply = false;
  for (SMILAnimationSandwich* sandwich : sandwiches_.Values()) {
    sandwich->UpdateActiveAnimationStack(elapsed);
    if (sandwich->ApplyAnimationValues())
      did_apply = true;
  }
  return did_apply;
}

void ElementSMILAnimations::Trace(Visitor* visitor) const {
  visitor->Trace(sandwiches_);
}

}  // namespace blink
