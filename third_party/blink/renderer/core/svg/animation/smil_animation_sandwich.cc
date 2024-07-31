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

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/core/svg/animation/smil_animation_sandwich.h"

#include "base/not_fatal_until.h"
#include "base/ranges/algorithm.h"
#include "third_party/blink/renderer/core/svg/animation/smil_animation_value.h"
#include "third_party/blink/renderer/core/svg/svg_animation_element.h"

namespace blink {

namespace {

struct PriorityCompare {
  PriorityCompare(SMILTime elapsed) : elapsed_(elapsed) {}
  bool operator()(const Member<SVGSMILElement>& a,
                  const Member<SVGSMILElement>& b) {
    return b->IsHigherPriorityThan(a.Get(), elapsed_);
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
  auto position = base::ranges::find(sandwich_, animation);
  CHECK(sandwich_.end() != position, base::NotFatalUntil::M130);
  sandwich_.erase(position);
  // Clear the animated value when there are active animation elements but the
  // sandwich is empty.
  if (!active_.empty() && sandwich_.empty()) {
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

  const bool was_active = !active_.empty();
  active_.Shrink(0);
  active_.reserve(sandwich_.size());
  // Build the contributing/active sandwich.
  for (auto& animation : sandwich_) {
    if (!animation->IsContributing(presentation_time))
      continue;
    animation->UpdateProgressState(presentation_time);
    active_.push_back(animation);
  }
  // If the sandwich was previously active but no longer is, clear any animated
  // value.
  if (was_active && active_.empty())
    sandwich_.front()->ClearAnimationValue();
}

bool SMILAnimationSandwich::ApplyAnimationValues() {
  if (active_.empty())
    return false;

  // Animations have to be applied lowest to highest prio.
  //
  // Only calculate the relevant animations. If we actually set the
  // animation value, we don't need to calculate what is beneath it
  // in the sandwich.
  auto sandwich_start = active_.end();
  while (sandwich_start != active_.begin()) {
    --sandwich_start;
    if ((*sandwich_start)->OverwritesUnderlyingAnimationValue())
      break;
  }

  // For now we need an element to setup and apply an animation. Any animation
  // element in the sandwich will do.
  SVGAnimationElement* animation = sandwich_.front();

  // Only reset the animated type to the base value once for
  // the lowest priority animation that animates and
  // contributes to a particular element/attribute pair.
  SMILAnimationValue animation_value = animation->CreateAnimationValue();

  for (auto sandwich_it = sandwich_start; sandwich_it != active_.end();
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
