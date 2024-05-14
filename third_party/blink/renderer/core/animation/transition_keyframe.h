// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_TRANSITION_KEYFRAME_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_TRANSITION_KEYFRAME_H_

#include "base/notreached.h"
#include "third_party/blink/renderer/core/animation/css/compositor_keyframe_value.h"
#include "third_party/blink/renderer/core/animation/keyframe.h"
#include "third_party/blink/renderer/core/animation/typed_interpolation_value.h"
#include "third_party/blink/renderer/core/core_export.h"

namespace blink {

// An implementation of Keyframe specifically for CSS Transitions.
//
// TransitionKeyframes are a simple form of keyframe, which only have one
// (property, value) pair. CSS Transitions do not support SVG attributes, so the
// property will always be a CSSPropertyID (for CSS properties and presentation
// attributes) or an AtomicString (for custom CSS properties).
class CORE_EXPORT TransitionKeyframe : public Keyframe {
 public:
  TransitionKeyframe(const PropertyHandle& property) : property_(property) {
    DCHECK(!property.IsSVGAttribute());
  }

  TransitionKeyframe(const TransitionKeyframe& copy_from)
      : Keyframe(copy_from.offset_,
                 copy_from.timeline_offset_,
                 copy_from.composite_,
                 copy_from.easing_),
        property_(copy_from.property_),
        value_(copy_from.value_->Clone()),
        compositor_value_(copy_from.compositor_value_) {}

  void SetValue(TypedInterpolationValue* value) {
    // Speculative CHECK to help investigate crbug.com/826627. The theory is
    // that |SetValue| is being called with a |value| that has no underlying
    // InterpolableValue. This then would later cause a crash in the
    // TransitionInterpolation constructor.
    // TODO(crbug.com/826627): Revert once bug is fixed.
    CHECK(!!value->Value());
    value_ = value;
  }
  void SetCompositorValue(CompositorKeyframeValue*);
  PropertyHandleSet Properties() const final;

  void AddKeyframePropertiesToV8Object(V8ObjectBuilder&,
                                       Element*) const override;

  void Trace(Visitor*) const override;

  class PropertySpecificKeyframe : public Keyframe::PropertySpecificKeyframe {
   public:
    PropertySpecificKeyframe(double offset,
                             scoped_refptr<TimingFunction> easing,
                             EffectModel::CompositeOperation composite,
                             TypedInterpolationValue* value,
                             CompositorKeyframeValue* compositor_value)
        : Keyframe::PropertySpecificKeyframe(offset,
                                             std::move(easing),
                                             composite),
          value_(value),
          compositor_value_(compositor_value) {}

    const CompositorKeyframeValue* GetCompositorKeyframeValue() const final {
      return compositor_value_.Get();
    }

    bool IsNeutral() const final { return false; }
    bool IsRevert() const final { return false; }
    bool IsRevertLayer() const final { return false; }
    Keyframe::PropertySpecificKeyframe* NeutralKeyframe(
        double offset,
        scoped_refptr<TimingFunction> easing) const final {
      NOTREACHED_IN_MIGRATION();
      return nullptr;
    }
    Interpolation* CreateInterpolation(
        const PropertyHandle&,
        const Keyframe::PropertySpecificKeyframe& other) const final;

    bool IsTransitionPropertySpecificKeyframe() const final { return true; }

    const TypedInterpolationValue* GetValue() const { return value_.Get(); }

    void Trace(Visitor*) const override;

   private:
    Keyframe::PropertySpecificKeyframe* CloneWithOffset(
        double offset) const final {
      return MakeGarbageCollected<PropertySpecificKeyframe>(
          offset, easing_, composite_, value_->Clone(), compositor_value_);
    }

    Member<TypedInterpolationValue> value_;
    Member<CompositorKeyframeValue> compositor_value_;
  };

 private:
  bool IsTransitionKeyframe() const final { return true; }

  Keyframe* Clone() const final {
    return MakeGarbageCollected<TransitionKeyframe>(*this);
  }

  Keyframe::PropertySpecificKeyframe* CreatePropertySpecificKeyframe(
      const PropertyHandle&,
      EffectModel::CompositeOperation effect_composite,
      double offset) const final;

  PropertyHandle property_;
  Member<TypedInterpolationValue> value_;
  Member<CompositorKeyframeValue> compositor_value_;
};

using TransitionPropertySpecificKeyframe =
    TransitionKeyframe::PropertySpecificKeyframe;

template <>
struct DowncastTraits<TransitionKeyframe> {
  static bool AllowFrom(const Keyframe& value) {
    return value.IsTransitionKeyframe();
  }
};
template <>
struct DowncastTraits<TransitionPropertySpecificKeyframe> {
  static bool AllowFrom(const Keyframe::PropertySpecificKeyframe& value) {
    return value.IsTransitionPropertySpecificKeyframe();
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_TRANSITION_KEYFRAME_H_
