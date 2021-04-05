/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_ELEMENT_ANIMATIONS_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_ELEMENT_ANIMATIONS_H_

#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "third_party/blink/renderer/core/animation/css/css_animations.h"
#include "third_party/blink/renderer/core/animation/effect_stack.h"
#include "third_party/blink/renderer/core/animation/worklet_animation_base.h"
#include "third_party/blink/renderer/core/css/properties/css_bitset.h"
#include "third_party/blink/renderer/platform/wtf/hash_counted_set.h"
#include "third_party/blink/renderer/platform/wtf/hash_map.h"

namespace blink {

class CSSAnimations;

using AnimationCountedSet = HeapHashCountedSet<WeakMember<Animation>>;
using WorkletAnimationSet = HeapHashSet<WeakMember<WorkletAnimationBase>>;

class CORE_EXPORT ElementAnimations final
    : public GarbageCollected<ElementAnimations> {
 public:
  ElementAnimations();
  ~ElementAnimations();

  // Animations that are currently active for this element, their effects will
  // be applied during a style recalc. CSS Transitions are included in this
  // stack.
  EffectStack& GetEffectStack() { return effect_stack_; }
  const EffectStack& GetEffectStack() const { return effect_stack_; }
  // Tracks the state of active CSS Animations and Transitions. The individual
  // animations will also be part of the animation stack, but the mapping
  // between animation name and animation is kept here.
  CSSAnimations& CssAnimations() { return css_animations_; }
  const CSSAnimations& CssAnimations() const { return css_animations_; }

  // Animations which have effects targeting this element.
  AnimationCountedSet& Animations() { return animations_; }
  // Worklet Animations which have effects targeting this element.
  WorkletAnimationSet& GetWorkletAnimations() { return worklet_animations_; }

  bool IsEmpty() const {
    return effect_stack_.IsEmpty() && css_animations_.IsEmpty() &&
           animations_.IsEmpty();
  }

  void RestartAnimationOnCompositor();

  void UpdateAnimationFlags(ComputedStyle&);
  void SetAnimationStyleChange(bool animation_style_change) {
    animation_style_change_ = animation_style_change;
  }
  bool IsAnimationStyleChange() const { return animation_style_change_; }

  const ComputedStyle* BaseComputedStyle() const;
  const CSSBitset* BaseImportantSet() const;
  void UpdateBaseComputedStyle(const ComputedStyle*,
                               std::unique_ptr<CSSBitset> base_important_set);
  void ClearBaseComputedStyle();

  bool UpdateBoxSizeAndCheckTransformAxisAlignment(const FloatSize& box_size);
  bool IsIdentityOrTranslation() const;

  void Trace(Visitor*) const;

 private:
  EffectStack effect_stack_;
  CSSAnimations css_animations_;
  AnimationCountedSet animations_;
  WorkletAnimationSet worklet_animations_;

  // When an Element is being animated, its entire style will be dirtied every
  // frame by the running animation - even if the animation is only changing a
  // few properties. To avoid the expensive cost of recomputing the entire
  // style, we store a cached value of the 'base' computed style (e.g. with no
  // change from the running animations) and use that during style recalc,
  // applying only the animation changes on top of it.
  bool animation_style_change_;
  scoped_refptr<ComputedStyle> base_computed_style_;
  // Keeps track of the !important declarations used to build the base
  // computed style. These declarations must not be overwritten by animation
  // effects, hence we have to disable the base computed style optimization when
  // !important declarations conflict with active animations.
  //
  // If there were no !important declarations in the base style, this field
  // will be nullptr.
  //
  // TODO(andruud): We should be able to simply skip applying the animation
  // for properties in this set instead of disabling the optimization.
  // However, we currently need the cascade to handle the case where
  // an !important declaration appears in a :visited selector.
  // See https://crbug.com/1062217.
  std::unique_ptr<CSSBitset> base_important_set_;

  DISALLOW_COPY_AND_ASSIGN(ElementAnimations);

  FRIEND_TEST_ALL_PREFIXES(StyleEngineTest, PseudoElementBaseComputedStyle);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_ELEMENT_ANIMATIONS_H_
