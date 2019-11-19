// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_INVALIDATABLE_INTERPOLATION_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_INVALIDATABLE_INTERPOLATION_H_

#include <memory>
#include "third_party/blink/renderer/core/animation/interpolation.h"
#include "third_party/blink/renderer/core/animation/interpolation_type.h"
#include "third_party/blink/renderer/core/animation/interpolation_types_map.h"
#include "third_party/blink/renderer/core/animation/primitive_interpolation.h"
#include "third_party/blink/renderer/core/animation/typed_interpolation_value.h"

namespace blink {

// See the documentation of Interpolation for general information about this
// class hierarchy.
//
// The InvalidatableInterpolation subclass stores the start and end keyframes as
// PropertySpecificKeyframe objects.
//
// InvalidatableInterpolation uses conversion checkers and the interpolation
// environment to respond to changes to the underlying property value during
// interpolation.
//
// InvalidatableInterpolation is used to implement additive animations. During
// the effect application phase of animation computation, the current animated
// value of the property is applied to the element by calling the static
// ApplyStack function with an ordered list of InvalidatableInterpolation
// objects.
class CORE_EXPORT InvalidatableInterpolation : public Interpolation {
 public:
  InvalidatableInterpolation(const PropertyHandle& property,
                             PropertySpecificKeyframe* start_keyframe,
                             PropertySpecificKeyframe* end_keyframe)
      : Interpolation(),
        property_(property),
        interpolation_types_(nullptr),
        interpolation_types_version_(0),
        start_keyframe_(start_keyframe),
        end_keyframe_(end_keyframe),
        current_fraction_(std::numeric_limits<double>::quiet_NaN()),
        is_conversion_cached_(false) {}

  const PropertyHandle& GetProperty() const final { return property_; }
  void Interpolate(int iteration, double fraction) override;
  bool DependsOnUnderlyingValue() const final;
  static void ApplyStack(const ActiveInterpolations&,
                         InterpolationEnvironment&);

  bool IsInvalidatableInterpolation() const override { return true; }

  const TypedInterpolationValue* GetCachedValueForTesting() const {
    return cached_value_.get();
  }

  void Trace(Visitor* visitor) override {
    visitor->Trace(start_keyframe_);
    visitor->Trace(end_keyframe_);
    Interpolation::Trace(visitor);
  }

 private:
  using ConversionCheckers = InterpolationType::ConversionCheckers;

  std::unique_ptr<TypedInterpolationValue> MaybeConvertUnderlyingValue(
      const InterpolationEnvironment&) const;
  const TypedInterpolationValue* EnsureValidConversion(
      const InterpolationEnvironment&,
      const UnderlyingValueOwner&) const;
  void EnsureValidInterpolationTypes(const InterpolationEnvironment&) const;
  void ClearConversionCache() const;
  bool IsConversionCacheValid(const InterpolationEnvironment&,
                              const UnderlyingValueOwner&) const;
  bool IsNeutralKeyframeActive() const;
  std::unique_ptr<PairwisePrimitiveInterpolation> MaybeConvertPairwise(
      const InterpolationEnvironment&,
      const UnderlyingValueOwner&) const;
  std::unique_ptr<TypedInterpolationValue> ConvertSingleKeyframe(
      const PropertySpecificKeyframe&,
      const InterpolationEnvironment&,
      const UnderlyingValueOwner&) const;
  void AddConversionCheckers(const InterpolationType&,
                             ConversionCheckers&) const;
  void SetFlagIfInheritUsed(InterpolationEnvironment&) const;
  double UnderlyingFraction() const;

  const PropertyHandle property_;
  mutable const InterpolationTypes* interpolation_types_;
  mutable size_t interpolation_types_version_;
  Member<PropertySpecificKeyframe> start_keyframe_;
  Member<PropertySpecificKeyframe> end_keyframe_;
  double current_fraction_;
  mutable bool is_conversion_cached_;
  mutable std::unique_ptr<PrimitiveInterpolation> cached_pair_conversion_;
  mutable ConversionCheckers conversion_checkers_;
  mutable std::unique_ptr<TypedInterpolationValue> cached_value_;
};

DEFINE_TYPE_CASTS(InvalidatableInterpolation,
                  Interpolation,
                  value,
                  value->IsInvalidatableInterpolation(),
                  value.IsInvalidatableInterpolation());

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_INVALIDATABLE_INTERPOLATION_H_
