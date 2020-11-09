/*
 * Copyright (C) 2008 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/svg/animation/smil_animation_sandwich.h"

#include <algorithm>

#include "third_party/blink/renderer/core/svg/animation/smil_animation_value.h"
#include "third_party/blink/renderer/core/svg/svg_animation_element.h"

namespace blink {

namespace {

struct PriorityCompare {
  PriorityCompare(SMILTime elapsed) : elapsed_(elapsed) {}
  bool operator()(const Member<SVGSMILElement>& a,
                  const Member<SVGSMILElement>& b) {
    return b->IsHigherPriorityThan(a, elapsed_);
  }
  SMILTime elapsed_;
};

}  // namespace

SMILAnimationSandwich::SMILAnimationSandwich() = default;

void SMILAnimationSandwich::Add(SVGAnimationElement* animation) {
  DCHECK(!sandwich_.Contains(animation));
  sandwich_.push_back(animation);
}

void SMILAnimationSandwich::Remove(SVGAnimationElement* animation) {
  auto* position = std::find(sandwich_.begin(), sandwich_.end(), animation);
  DCHECK(sandwich_.end() != position);
  sandwich_.erase(position);
  // If the sandwich is now empty, clear any animated value if there are active
  // animation elements.
  if (sandwich_.IsEmpty() && !active_.IsEmpty()) {
    animation->ClearAnimationValue();
    active_.Shrink(0);
  }
}

void SMILAnimationSandwich::UpdateActiveAnimationStack(
    SMILTime presentation_time) {
  if (!std::is_sorted(sandwich_.begin(), sandwich_.end(),
                      PriorityCompare(presentation_time))) {
    std::sort(sandwich_.begin(), sandwich_.end(),
              PriorityCompare(presentation_time));
  }

  active_.Shrink(0);
  active_.ReserveCapacity(sandwich_.size());
  // Build the contributing/active sandwich.
  for (auto& animation : sandwich_) {
    if (!animation->IsContributing(presentation_time))
      continue;
    animation->UpdateProgressState(presentation_time);
    active_.push_back(animation);
  }
}

bool SMILAnimationSandwich::ApplyAnimationValues() {
  // For now we need an element to setup and apply an animation. Any animation
  // element in the sandwich will do.
  SVGAnimationElement* animation = sandwich_.front();

  // If the sandwich does not have any active elements, clear any animated
  // value.
  if (active_.IsEmpty()) {
    animation->ClearAnimationValue();
    return false;
  }

  // Animations have to be applied lowest to highest prio.
  //
  // Only calculate the relevant animations. If we actually set the
  // animation value, we don't need to calculate what is beneath it
  // in the sandwich.
  bool needs_underlying_value = true;
  auto* sandwich_start = active_.end();
  while (sandwich_start != active_.begin()) {
    --sandwich_start;
    if ((*sandwich_start)->OverwritesUnderlyingAnimationValue()) {
      needs_underlying_value = false;
      break;
    }
  }

  // Only reset the animated type to the base value once for
  // the lowest priority animation that animates and
  // contributes to a particular element/attribute pair.
  SMILAnimationValue animation_value =
      animation->CreateAnimationValue(needs_underlying_value);

  for (auto* sandwich_it = sandwich_start; sandwich_it != active_.end();
       sandwich_it++) {
    (*sandwich_it)->ApplyAnimation(animation_value);
  }

  animation->ApplyResultsToTarget(animation_value);
  return true;
}

void SMILAnimationSandwich::Trace(Visitor* visitor) const {
  visitor->Trace(sandwich_);
  visitor->Trace(active_);
}

}  // namespace blink
