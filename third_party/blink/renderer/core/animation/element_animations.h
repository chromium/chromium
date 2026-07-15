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

#include "base/gtest_prod_util.h"
#include "third_party/blink/renderer/core/animation/css/css_animations.h"
#include "third_party/blink/renderer/core/animation/css/css_image_animations.h"
#include "third_party/blink/renderer/core/animation/effect_stack.h"
#include "third_party/blink/renderer/core/animation/native_paint_worklet_data.h"
#include "third_party/blink/renderer/core/animation/worklet_animation_base.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/css/properties/css_bitset.h"
#include "third_party/blink/renderer/core/dom/node_rare_data_field.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_counted_set.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_set.h"
#include "third_party/blink/renderer/platform/wtf/hash_counted_set.h"

namespace blink {

class CSSAnimations;

using AnimationCountedSet = HeapHashCountedSet<WeakMember<Animation>>;
using WorkletAnimationSet = HeapHashSet<WeakMember<WorkletAnimationBase>>;

class CORE_EXPORT ElementAnimations final
    : public GarbageCollected<ElementAnimations>,
      public NodeRareDataField {
 public:
  ElementAnimations();
  ElementAnimations(const ElementAnimations&) = delete;
  ElementAnimations& operator=(const ElementAnimations&) = delete;
  ~ElementAnimations();

  using CompositedPaintStatus = NativePaintWorkletData::CompositedPaintStatus;

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
           animations_.empty() && worklet_animations_.empty();
  }

  CSSImageAnimations& CssImageAnimations() { return css_image_animations_; }
  const CSSImageAnimations& CssImageAnimations() const {
    return css_image_animations_;
  }

  void RestartAnimationOnCompositor();

  void SetAnimationStyleChange(bool animation_style_change) {
    animation_style_change_ = animation_style_change;
  }
  bool IsAnimationStyleChange() const { return animation_style_change_; }

  bool UpdateBoxSizeAndCheckTransformAxisAlignment(const gfx::SizeF& box_size);
  bool IsIdentityOrTranslation() const;

  bool HasCompositedPaintWorkletAnimation();

  void RecalcCompositedStatusForKeyframeChange(
      Element& element,
      Animation::NativePaintWorkletReasons properties);
  void RecalcCompositedStatus(Element* element);

  // TODO(crbug.com/1301961): Consider converting to an array or flat map of
  // fields for paint properties that can be composited.

  NativePaintWorkletData* EnsureBackgroundColorNpwData(Element* element);

  CompositedPaintStatus CompositedBackgroundColorStatus() {
    return background_color_npw_data_
               ? background_color_npw_data_->GetCompositedPaintStatus()
               : CompositedPaintStatus::kNoAnimation;
  }

  bool SetCompositedBackgroundColorStatus(CompositedPaintStatus status);

  Animation* PaintWorkletClipPathAnimation() {
    return clip_path_npw_data_ ? clip_path_npw_data_->GetAnimation() : nullptr;
  }

  CompositedPaintStatus CompositedClipPathStatus() {
    return clip_path_npw_data_ ? clip_path_npw_data_->GetCompositedPaintStatus()
                               : CompositedPaintStatus::kNoAnimation;
  }

  bool SetCompositedClipPathStatus(CompositedPaintStatus status);

  NativePaintWorkletData* EnsureClipPathNpwData(Element* element);

  // Animations affecting properties marked as important cannot be composited.
  // An animation running on the compositor must be cancelled once the affected
  // property is added to the important set. Note that a animation affecting
  // an important property still continues to run on the main thread, but the
  // property value will not applied by the style cascade.
  void CancelCompositedAnimationsAffectingProperties(
      const CSSBitset& property_bitset);

  void Trace(Visitor*) const override;

 protected:
  bool SetCompositedPaintStatus(Member<NativePaintWorkletData>& data,
                                CompositedPaintStatus status);

 private:
  EffectStack effect_stack_;
  CSSAnimations css_animations_;
  CSSImageAnimations css_image_animations_;
  AnimationCountedSet animations_;
  WorkletAnimationSet worklet_animations_;
  Member<NativePaintWorkletData> background_color_npw_data_;
  Member<NativePaintWorkletData> clip_path_npw_data_;

  // When an Element is being animated, its entire style will be dirtied every
  // frame by the running animation - even if the animation is only changing a
  // few properties. To avoid the expensive cost of recomputing the entire
  // style, we store a cached value of the 'base' computed style (e.g. with no
  // change from the running animations) and use that during style recalc,
  // applying only the animation changes on top of it.
  //
  // See also StyleBaseData.
  bool animation_style_change_ : 1;

  FRIEND_TEST_ALL_PREFIXES(StyleEngineTest, PseudoElementBaseComputedStyle);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_ELEMENT_ANIMATIONS_H_
