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

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_CSS_CSS_ANIMATIONS_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_CSS_CSS_ANIMATIONS_H_

#include "base/macros.h"
#include "third_party/blink/renderer/core/animation/css/css_animation_data.h"
#include "third_party/blink/renderer/core/animation/css/css_animation_update.h"
#include "third_party/blink/renderer/core/animation/css/css_transition_data.h"
#include "third_party/blink/renderer/core/animation/inert_effect.h"
#include "third_party/blink/renderer/core/animation/interpolation.h"
#include "third_party/blink/renderer/core/css/css_keyframes_rule.h"
#include "third_party/blink/renderer/core/css/css_property_value_set.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/platform/wtf/hash_map.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"

namespace blink {

class CSSTransitionData;
class Element;
class StylePropertyShorthand;
class StyleResolver;

class CORE_EXPORT CSSAnimations final {
  DISALLOW_NEW();

 public:
  CSSAnimations();

  static const StylePropertyShorthand& PropertiesForTransitionAll();
  static bool IsAnimationAffectingProperty(const CSSProperty&);
  static bool IsAffectedByKeyframesFromScope(const Element&, const TreeScope&);
  static bool IsAnimatingCustomProperties(const ElementAnimations*);
  static void CalculateAnimationUpdate(CSSAnimationUpdate&,
                                       const Element* animating_element,
                                       Element&,
                                       const ComputedStyle&,
                                       const ComputedStyle* parent_style,
                                       StyleResolver*);
  static void CalculateCompositorAnimationUpdate(
      CSSAnimationUpdate&,
      const Element* animating_element,
      Element&,
      const ComputedStyle&,
      const ComputedStyle* parent_style,
      bool was_viewport_changed);

  // Specifies whether to process custom or standard CSS properties.
  enum class PropertyPass { kCustom, kStandard };
  static void CalculateTransitionUpdate(CSSAnimationUpdate&,
                                        PropertyPass,
                                        const Element* animating_element,
                                        const ComputedStyle&);

  static void SnapshotCompositorKeyframes(Element&,
                                          CSSAnimationUpdate&,
                                          const ComputedStyle&,
                                          const ComputedStyle* parent_style);

  void SetPendingUpdate(const CSSAnimationUpdate& update) {
    ClearPendingUpdate();
    pending_update_.Copy(update);
  }
  void ClearPendingUpdate() { pending_update_.Clear(); }
  void MaybeApplyPendingUpdate(Element*);
  bool IsEmpty() const {
    return running_animations_.IsEmpty() && transitions_.IsEmpty() &&
           pending_update_.IsEmpty();
  }
  void Cancel();

  void Trace(blink::Visitor*);

 private:
  class RunningAnimation final : public GarbageCollected<RunningAnimation> {
   public:
    RunningAnimation(Animation* animation, NewCSSAnimation new_animation)
        : animation(animation),
          name(new_animation.name),
          name_index(new_animation.name_index),
          specified_timing(new_animation.timing),
          style_rule(new_animation.style_rule),
          style_rule_version(new_animation.style_rule_version),
          play_state_list(new_animation.play_state_list) {}

    void Update(UpdatedCSSAnimation update) {
      DCHECK_EQ(update.animation, animation);
      style_rule = update.style_rule;
      style_rule_version = update.style_rule_version;
      play_state_list = update.play_state_list;
      specified_timing = update.specified_timing;
    }

    void Trace(blink::Visitor* visitor) {
      visitor->Trace(animation);
      visitor->Trace(style_rule);
    }

    Member<Animation> animation;
    AtomicString name;
    size_t name_index;
    Timing specified_timing;
    Member<StyleRuleKeyframes> style_rule;
    unsigned style_rule_version;
    Vector<EAnimPlayState> play_state_list;
  };

  struct RunningTransition {
    DISALLOW_NEW();

   public:
    void Trace(blink::Visitor* visitor) { visitor->Trace(animation); }

    Member<Animation> animation;
    scoped_refptr<const ComputedStyle> from;
    scoped_refptr<const ComputedStyle> to;
    scoped_refptr<const ComputedStyle> reversing_adjusted_start_value;
    double reversing_shortening_factor;
  };

  HeapVector<Member<RunningAnimation>> running_animations_;

  using TransitionMap = HeapHashMap<PropertyHandle, RunningTransition>;
  TransitionMap transitions_;

  CSSAnimationUpdate pending_update_;

  ActiveInterpolationsMap previous_active_interpolations_for_custom_animations_;
  ActiveInterpolationsMap
      previous_active_interpolations_for_standard_animations_;

  struct TransitionUpdateState {
    STACK_ALLOCATED();

   public:
    CSSAnimationUpdate& update;
    Member<const Element> animating_element;
    const ComputedStyle& old_style;
    const ComputedStyle& style;
    scoped_refptr<const ComputedStyle> cloned_style;
    const TransitionMap* active_transitions;
    HashSet<PropertyHandle>& listed_properties;
    const CSSTransitionData& transition_data;
  };

  static void CalculateTransitionUpdateForCustomProperty(
      TransitionUpdateState&,
      const CSSTransitionData::TransitionProperty&,
      size_t transition_index);

  static void CalculateTransitionUpdateForStandardProperty(
      TransitionUpdateState&,
      const CSSTransitionData::TransitionProperty&,
      size_t transition_index,
      const ComputedStyle&);

  static void CalculateTransitionUpdateForProperty(TransitionUpdateState&,
                                                   const PropertyHandle&,
                                                   size_t transition_index);

  static void CalculateAnimationActiveInterpolations(
      CSSAnimationUpdate&,
      const Element* animating_element);
  static void CalculateTransitionActiveInterpolations(
      CSSAnimationUpdate&,
      PropertyPass,
      const Element* animating_element);

  class AnimationEventDelegate final : public AnimationEffect::EventDelegate {
   public:
    AnimationEventDelegate(Element* animation_target, const AtomicString& name)
        : animation_target_(animation_target),
          name_(name),
          previous_phase_(Timing::kPhaseNone) {}
    bool RequiresIterationEvents(const AnimationEffect&) override;
    void OnEventCondition(const AnimationEffect&, Timing::Phase) override;
    void Trace(blink::Visitor*) override;

   private:
    const Element& AnimationTarget() const { return *animation_target_; }
    EventTarget* GetEventTarget() const;
    Document& GetDocument() const { return animation_target_->GetDocument(); }

    void MaybeDispatch(Document::ListenerType,
                       const AtomicString& event_name,
                       const AnimationTimeDelta& elapsed_time);
    Member<Element> animation_target_;
    const AtomicString name_;
    Timing::Phase previous_phase_;
    base::Optional<double> previous_iteration_;
  };

  class TransitionEventDelegate final : public AnimationEffect::EventDelegate {
   public:
    TransitionEventDelegate(Element* transition_target,
                            const PropertyHandle& property)
        : transition_target_(transition_target),
          property_(property),
          previous_phase_(Timing::kPhaseNone) {}
    bool RequiresIterationEvents(const AnimationEffect&) override {
      return false;
    }
    void OnEventCondition(const AnimationEffect&, Timing::Phase) override;
    void Trace(blink::Visitor*) override;

   private:
    void EnqueueEvent(const WTF::AtomicString& type,
                      const AnimationTimeDelta& elapsed_time);

    const Element& TransitionTarget() const { return *transition_target_; }
    EventTarget* GetEventTarget() const;
    PseudoId GetPseudoId() const { return transition_target_->GetPseudoId(); }
    Document& GetDocument() const { return transition_target_->GetDocument(); }

    Member<Element> transition_target_;
    PropertyHandle property_;
    Timing::Phase previous_phase_;
  };

  DISALLOW_COPY_AND_ASSIGN(CSSAnimations);
};

}  // namespace blink

#endif
