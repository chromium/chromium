// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/animation/css_ray_interpolation_type.h"

#include <memory>
#include <utility>

#include "base/memory/ptr_util.h"
#include "third_party/blink/renderer/core/css/basic_shape_functions.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver_state.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/core/style/style_ray.h"

namespace blink {

namespace {

class RayMode {
 public:
  RayMode(StyleRay::RaySize size, bool contain)
      : size_(size), contain_(contain) {}

  explicit RayMode(const StyleRay& style_ray)
      : size_(style_ray.Size()), contain_(style_ray.Contain()) {}

  StyleRay::RaySize Size() const { return size_; }
  bool Contain() const { return contain_; }

  bool operator==(const RayMode& other) const {
    return size_ == other.size_ && contain_ == other.contain_;
  }
  bool operator!=(const RayMode& other) const { return !(*this == other); }

 private:
  StyleRay::RaySize size_;
  bool contain_;
};

}  // namespace

class CSSRayNonInterpolableValue : public NonInterpolableValue {
 public:
  static scoped_refptr<CSSRayNonInterpolableValue> Create(const RayMode& mode) {
    return base::AdoptRef(new CSSRayNonInterpolableValue(mode));
  }

  const RayMode& Mode() const { return mode_; }

  DECLARE_NON_INTERPOLABLE_VALUE_TYPE();

 private:
  CSSRayNonInterpolableValue(const RayMode& mode) : mode_(mode) {}

  const RayMode mode_;
};

DEFINE_NON_INTERPOLABLE_VALUE_TYPE(CSSRayNonInterpolableValue);
DEFINE_NON_INTERPOLABLE_VALUE_TYPE_CASTS(CSSRayNonInterpolableValue);

namespace {

// Returns the offset-path ray() value.
// If the offset-path is not a ray(), returns nullptr.
StyleRay* GetRay(const ComputedStyle& style) {
  BasicShape* offset_path = style.OffsetPath();
  if (!offset_path || offset_path->GetType() != BasicShape::kStyleRayType)
    return nullptr;
  return To<StyleRay>(style.OffsetPath());
}

class UnderlyingRayModeChecker
    : public CSSInterpolationType::CSSConversionChecker {
 public:
  explicit UnderlyingRayModeChecker(const RayMode& mode) : mode_(mode) {}

  bool IsValid(const StyleResolverState&,
               const InterpolationValue& underlying) const final {
    return mode_ ==
           ToCSSRayNonInterpolableValue(*underlying.non_interpolable_value)
               .Mode();
  }

 private:
  const RayMode mode_;
};

class InheritedRayChecker : public CSSInterpolationType::CSSConversionChecker {
 public:
  InheritedRayChecker(scoped_refptr<StyleRay> style_ray)
      : style_ray_(std::move(style_ray)) {
    DCHECK(style_ray_);
  }

 private:
  bool IsValid(const StyleResolverState& state,
               const InterpolationValue&) const final {
    return GetRay(*state.ParentStyle()) == style_ray_.get();
  }

  const scoped_refptr<StyleRay> style_ray_;
};

InterpolationValue CreateValue(float angle, const RayMode& mode) {
  return InterpolationValue(std::make_unique<InterpolableNumber>(angle),
                            CSSRayNonInterpolableValue::Create(mode));
}

}  // namespace

void CSSRayInterpolationType::ApplyStandardPropertyValue(
    const InterpolableValue& interpolable_value,
    const NonInterpolableValue* non_interpolable_value,
    StyleResolverState& state) const {
  const CSSRayNonInterpolableValue& ray_non_interpolable_value =
      ToCSSRayNonInterpolableValue(*non_interpolable_value);
  state.Style()->SetOffsetPath(
      StyleRay::Create(ToInterpolableNumber(interpolable_value).Value(),
                       ray_non_interpolable_value.Mode().Size(),
                       ray_non_interpolable_value.Mode().Contain()));
}

void CSSRayInterpolationType::Composite(
    UnderlyingValueOwner& underlying_value_owner,
    double underlying_fraction,
    const InterpolationValue& value,
    double interpolation_fraction) const {
  const RayMode& underlying_mode =
      ToCSSRayNonInterpolableValue(
          *underlying_value_owner.Value().non_interpolable_value)
          .Mode();
  const RayMode& ray_mode =
      ToCSSRayNonInterpolableValue(*value.non_interpolable_value).Mode();
  if (underlying_mode == ray_mode) {
    underlying_value_owner.MutableValue().interpolable_value->ScaleAndAdd(
        underlying_fraction, *value.interpolable_value);
  } else {
    underlying_value_owner.Set(*this, value);
  }
}

InterpolationValue CSSRayInterpolationType::MaybeConvertNeutral(
    const InterpolationValue& underlying,
    ConversionCheckers& conversion_checkers) const {
  const RayMode& underlying_mode =
      ToCSSRayNonInterpolableValue(*underlying.non_interpolable_value).Mode();
  conversion_checkers.push_back(
      std::make_unique<UnderlyingRayModeChecker>(underlying_mode));
  return CreateValue(0, underlying_mode);
}

InterpolationValue CSSRayInterpolationType::MaybeConvertInitial(
    const StyleResolverState&,
    ConversionCheckers&) const {
  // 'none' is not a ray().
  return nullptr;
}

InterpolationValue CSSRayInterpolationType::MaybeConvertInherit(
    const StyleResolverState& state,
    ConversionCheckers& conversion_checkers) const {
  if (!state.ParentStyle())
    return nullptr;

  StyleRay* inherited_ray = GetRay(*state.ParentStyle());
  if (!inherited_ray)
    return nullptr;

  conversion_checkers.push_back(
      std::make_unique<InheritedRayChecker>(inherited_ray));
  return CreateValue(inherited_ray->Angle(), RayMode(*inherited_ray));
}

PairwiseInterpolationValue CSSRayInterpolationType::MaybeMergeSingles(
    InterpolationValue&& start,
    InterpolationValue&& end) const {
  const RayMode& start_mode =
      ToCSSRayNonInterpolableValue(*start.non_interpolable_value).Mode();
  const RayMode& end_mode =
      ToCSSRayNonInterpolableValue(*end.non_interpolable_value).Mode();
  if (start_mode != end_mode)
    return nullptr;
  return PairwiseInterpolationValue(std::move(start.interpolable_value),
                                    std::move(end.interpolable_value),
                                    std::move(start.non_interpolable_value));
}

InterpolationValue
CSSRayInterpolationType::MaybeConvertStandardPropertyUnderlyingValue(
    const ComputedStyle& style) const {
  StyleRay* underlying_ray = GetRay(style);
  if (!underlying_ray)
    return nullptr;

  return CreateValue(underlying_ray->Angle(), RayMode(*underlying_ray));
}

InterpolationValue CSSRayInterpolationType::MaybeConvertValue(
    const CSSValue& value,
    const StyleResolverState* state,
    ConversionCheckers&) const {
  DCHECK(state);
  if (!value.IsRayValue())
    return nullptr;

  scoped_refptr<BasicShape> shape = BasicShapeForValue(*state, value);
  return CreateValue(To<StyleRay>(*shape).Angle(),
                     RayMode(To<StyleRay>(*shape)));
}

}  // namespace blink
