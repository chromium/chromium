// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_KEYFRAME_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_KEYFRAME_H_

#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "base/optional.h"
#include "third_party/blink/renderer/core/animation/animation_effect.h"
#include "third_party/blink/renderer/core/animation/effect_model.h"
#include "third_party/blink/renderer/core/animation/property_handle.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"

namespace blink {

using PropertyHandleSet = HashSet<PropertyHandle>;

class Element;
class ComputedStyle;
class CompositorKeyframeValue;
class V8ObjectBuilder;

// A base class representing an animation keyframe.
//
// Generically a keyframe is a set of (property, value) pairs. In the
// web-animations spec keyframes have a few additional properties:
//
//   * A possibly-null keyframe offset, which represents the keyframe's position
//     relative to other keyframes in the same effect.
//   * A non-null timing function, which applies to the period of time between
//     this keyframe and the next keyframe in the same effect and influences
//     the interpolation between them.
//   * An keyframe-specific composite operation, which specifies a specific
//     composite operation used to combine values in this keyframe with an
//     underlying value. If this is 'auto', the keyframe effect composite
//     operation is used instead.
//
// For spec details, refer to: https://drafts.csswg.org/web-animations/#keyframe
//
// Implementation-wise the base Keyframe class captures the offset, composite
// operation, and timing function. It is left to subclasses to define and store
// the set of (property, value) pairs.
//
// === PropertySpecificKeyframes ===
//
// When calculating the effect value of a keyframe effect, the web-animations
// spec requires that a set of 'property-specific' keyframes are created.
// Property-specific keyframes resolve any unspecified offsets in the keyframes,
// calculate computed values for the specified properties, convert shorthand
// properties to multiple longhand properties, and resolve any conflicting
// shorthand properties.
//
// In this implementation property-specific keyframes are created only once and
// cached for subsequent calls, rather than re-computing them for every sample
// from the keyframe effect. See KeyframeEffectModelBase::EnsureKeyframeGroups.
//
// FIXME: Make Keyframe immutable
class CORE_EXPORT Keyframe : public GarbageCollected<Keyframe> {
 public:
  virtual ~Keyframe() = default;

  // TODO(smcgruer): The keyframe offset should be immutable.
  void SetOffset(base::Optional<double> offset) { offset_ = offset; }
  base::Optional<double> Offset() const { return offset_; }
  double CheckedOffset() const { return offset_.value(); }

  // TODO(smcgruer): The keyframe composite operation should be immutable.
  void SetComposite(EffectModel::CompositeOperation composite) {
    composite_ = composite;
  }
  bool HasComposite() const { return composite_.has_value(); }
  EffectModel::CompositeOperation Composite() const {
    return composite_.value();
  }

  // TODO(smcgruer): The keyframe timing function should be immutable.
  void SetEasing(scoped_refptr<TimingFunction> easing) {
    if (easing)
      easing_ = std::move(easing);
    else
      easing_ = LinearTimingFunction::Shared();
  }
  TimingFunction& Easing() const { return *easing_; }

  // Returns a set of the properties represented in this keyframe.
  virtual PropertyHandleSet Properties() const = 0;

  // Creates a clone of this keyframe.
  //
  // The clone should have the same (property, value) pairs, offset value,
  // composite operation, and timing function, as well as any other
  // subclass-specific data.
  virtual Keyframe* Clone() const = 0;

  // Helper function to create a clone of this keyframe with a specific offset.
  Keyframe* CloneWithOffset(double offset) const {
    Keyframe* the_clone = Clone();
    the_clone->SetOffset(offset);
    return the_clone;
  }

  // Add the properties represented by this keyframe to the given V8 object.
  //
  // Subclasses should override this to add the (property, value) pairs they
  // store, and call into the base version to add the basic Keyframe properties.
  virtual void AddKeyframePropertiesToV8Object(V8ObjectBuilder&) const;

  virtual bool IsStringKeyframe() const { return false; }
  virtual bool IsTransitionKeyframe() const { return false; }

  virtual void Trace(Visitor*) {}

  // Represents a property-specific keyframe as defined in the spec. Refer to
  // the Keyframe class-level documentation for more details.
  class CORE_EXPORT PropertySpecificKeyframe
      : public GarbageCollected<PropertySpecificKeyframe> {
   public:
    PropertySpecificKeyframe(double offset,
                             scoped_refptr<TimingFunction> easing,
                             EffectModel::CompositeOperation);
    virtual ~PropertySpecificKeyframe() = default;
    double Offset() const { return offset_; }
    TimingFunction& Easing() const { return *easing_; }
    EffectModel::CompositeOperation Composite() const { return composite_; }
    double UnderlyingFraction() const {
      return composite_ == EffectModel::kCompositeReplace ? 0 : 1;
    }
    virtual bool IsNeutral() const = 0;
    virtual PropertySpecificKeyframe* CloneWithOffset(double offset) const = 0;

    // FIXME: Remove this once CompositorAnimations no longer depends on
    // CompositorKeyframeValues
    virtual bool PopulateCompositorKeyframeValue(
        const PropertyHandle&,
        Element&,
        const ComputedStyle& base_style,
        const ComputedStyle* parent_style) const {
      return false;
    }

    virtual const CompositorKeyframeValue* GetCompositorKeyframeValue()
        const = 0;

    virtual bool IsCSSPropertySpecificKeyframe() const { return false; }
    virtual bool IsSVGPropertySpecificKeyframe() const { return false; }
    virtual bool IsTransitionPropertySpecificKeyframe() const { return false; }

    virtual PropertySpecificKeyframe* NeutralKeyframe(
        double offset,
        scoped_refptr<TimingFunction> easing) const = 0;
    virtual Interpolation* CreateInterpolation(
        const PropertyHandle&,
        const Keyframe::PropertySpecificKeyframe& end) const;

    virtual void Trace(Visitor*) {}

   protected:
    double offset_;
    scoped_refptr<TimingFunction> easing_;
    EffectModel::CompositeOperation composite_;

    DISALLOW_COPY_AND_ASSIGN(PropertySpecificKeyframe);
  };

  // Construct and return a property-specific keyframe for this keyframe.
  //
  // The 'effect_composite' parameter is the composite operation of the effect
  // that owns the keyframe. If the keyframe has a keyframe-specific composite
  // operation it should ignore this value when creating the property specific
  // keyframe.
  //
  // The 'offset' parameter is the offset to use in the resultant
  // PropertySpecificKeyframe. For CSS Transitions and CSS Animations, this is
  // the normal offset from the keyframe itself. However in web-animations this
  // will be a computed offset value which may differ from the keyframe offset.
  virtual PropertySpecificKeyframe* CreatePropertySpecificKeyframe(
      const PropertyHandle&,
      EffectModel::CompositeOperation effect_composite,
      double offset) const = 0;

 protected:
  Keyframe()
      : offset_(), composite_(), easing_(LinearTimingFunction::Shared()) {}
  Keyframe(base::Optional<double> offset,
           base::Optional<EffectModel::CompositeOperation> composite,
           scoped_refptr<TimingFunction> easing)
      : offset_(offset), composite_(composite), easing_(std::move(easing)) {
    if (!easing_)
      easing_ = LinearTimingFunction::Shared();
  }

  base::Optional<double> offset_;
  // To avoid having multiple CompositeOperation enums internally (one with
  // 'auto' and one without), we use a base::Optional for composite_. A
  // base::nullopt value represents 'auto'.
  base::Optional<EffectModel::CompositeOperation> composite_;
  scoped_refptr<TimingFunction> easing_;
  DISALLOW_COPY_AND_ASSIGN(Keyframe);
};

using PropertySpecificKeyframe = Keyframe::PropertySpecificKeyframe;

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_KEYFRAME_H_
