// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/animation/css_clip_interpolation_type.h"

#include <memory>
#include <utility>

#include "base/memory/ptr_util.h"
#include "third_party/blink/renderer/core/animation/interpolable_length.h"
#include "third_party/blink/renderer/core/css/css_identifier_value.h"
#include "third_party/blink/renderer/core/css/css_quad_value.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver_state.h"
#include "third_party/blink/renderer/core/style/computed_style.h"

namespace blink {

struct ClipAutos {
  ClipAutos()
      : is_auto(true),
        is_top_auto(false),
        is_right_auto(false),
        is_bottom_auto(false),
        is_left_auto(false) {}
  ClipAutos(bool is_top_auto,
            bool is_right_auto,
            bool is_bottom_auto,
            bool is_left_auto)
      : is_auto(false),
        is_top_auto(is_top_auto),
        is_right_auto(is_right_auto),
        is_bottom_auto(is_bottom_auto),
        is_left_auto(is_left_auto) {}
  explicit ClipAutos(const LengthBox& clip)
      : is_auto(false),
        is_top_auto(clip.Top().IsAuto()),
        is_right_auto(clip.Right().IsAuto()),
        is_bottom_auto(clip.Bottom().IsAuto()),
        is_left_auto(clip.Left().IsAuto()) {}

  bool operator==(const ClipAutos& other) const {
    return is_auto == other.is_auto && is_top_auto == other.is_top_auto &&
           is_right_auto == other.is_right_auto &&
           is_bottom_auto == other.is_bottom_auto &&
           is_left_auto == other.is_left_auto;
  }
  bool operator!=(const ClipAutos& other) const { return !(*this == other); }

  bool is_auto;
  bool is_top_auto;
  bool is_right_auto;
  bool is_bottom_auto;
  bool is_left_auto;
};

class InheritedClipChecker : public CSSInterpolationType::CSSConversionChecker {
 public:
  static std::unique_ptr<InheritedClipChecker> Create(
      const ComputedStyle& parent_style) {
    Vector<Length> inherited_length_list;
    GetClipLengthList(parent_style, inherited_length_list);
    return base::WrapUnique(
        new InheritedClipChecker(std::move(inherited_length_list)));
  }

 private:
  InheritedClipChecker(const Vector<Length>&& inherited_length_list)
      : inherited_length_list_(std::move(inherited_length_list)) {}

  bool IsValid(const StyleResolverState& state,
               const InterpolationValue& underlying) const final {
    Vector<Length> inherited_length_list;
    GetClipLengthList(*state.ParentStyle(), inherited_length_list);
    return inherited_length_list_ == inherited_length_list;
  }

  static void GetClipLengthList(const ComputedStyle& style,
                                Vector<Length>& length_list) {
    if (style.HasAutoClip())
      return;
    length_list.push_back(style.ClipTop());
    length_list.push_back(style.ClipRight());
    length_list.push_back(style.ClipBottom());
    length_list.push_back(style.ClipLeft());
  }

  const Vector<Length> inherited_length_list_;
};

class CSSClipNonInterpolableValue : public NonInterpolableValue {
 public:
  ~CSSClipNonInterpolableValue() final = default;

  static scoped_refptr<CSSClipNonInterpolableValue> Create(
      const ClipAutos& clip_autos) {
    return base::AdoptRef(new CSSClipNonInterpolableValue(clip_autos));
  }

  const ClipAutos& GetClipAutos() const { return clip_autos_; }

  DECLARE_NON_INTERPOLABLE_VALUE_TYPE();

 private:
  CSSClipNonInterpolableValue(const ClipAutos& clip_autos)
      : clip_autos_(clip_autos) {
    DCHECK(!clip_autos_.is_auto);
  }

  const ClipAutos clip_autos_;
};

DEFINE_NON_INTERPOLABLE_VALUE_TYPE(CSSClipNonInterpolableValue);
DEFINE_NON_INTERPOLABLE_VALUE_TYPE_CASTS(CSSClipNonInterpolableValue);

class UnderlyingAutosChecker
    : public CSSInterpolationType::CSSConversionChecker {
 public:
  explicit UnderlyingAutosChecker(const ClipAutos& underlying_autos)
      : underlying_autos_(underlying_autos) {}
  ~UnderlyingAutosChecker() final = default;

  static ClipAutos GetUnderlyingAutos(const InterpolationValue& underlying) {
    if (!underlying)
      return ClipAutos();
    return ToCSSClipNonInterpolableValue(*underlying.non_interpolable_value)
        .GetClipAutos();
  }

 private:
  bool IsValid(const StyleResolverState&,
               const InterpolationValue& underlying) const final {
    return underlying_autos_ == GetUnderlyingAutos(underlying);
  }

  const ClipAutos underlying_autos_;
};

enum ClipComponentIndex : unsigned {
  kClipTop,
  kClipRight,
  kClipBottom,
  kClipLeft,
  kClipComponentIndexCount,
};

static std::unique_ptr<InterpolableValue> ConvertClipComponent(
    const Length& length,
    double zoom) {
  if (length.IsAuto())
    return std::make_unique<InterpolableList>(0);
  return InterpolableLength::MaybeConvertLength(length, zoom);
}

static InterpolationValue CreateClipValue(const LengthBox& clip, double zoom) {
  auto list = std::make_unique<InterpolableList>(kClipComponentIndexCount);
  list->Set(kClipTop, ConvertClipComponent(clip.Top(), zoom));
  list->Set(kClipRight, ConvertClipComponent(clip.Right(), zoom));
  list->Set(kClipBottom, ConvertClipComponent(clip.Bottom(), zoom));
  list->Set(kClipLeft, ConvertClipComponent(clip.Left(), zoom));
  return InterpolationValue(
      std::move(list), CSSClipNonInterpolableValue::Create(ClipAutos(clip)));
}

InterpolationValue CSSClipInterpolationType::MaybeConvertNeutral(
    const InterpolationValue& underlying,
    ConversionCheckers& conversion_checkers) const {
  ClipAutos underlying_autos =
      UnderlyingAutosChecker::GetUnderlyingAutos(underlying);
  conversion_checkers.push_back(
      std::make_unique<UnderlyingAutosChecker>(underlying_autos));
  if (underlying_autos.is_auto)
    return nullptr;
  LengthBox neutral_box(
      underlying_autos.is_top_auto ? Length::Auto() : Length::Fixed(0),
      underlying_autos.is_right_auto ? Length::Auto() : Length::Fixed(0),
      underlying_autos.is_bottom_auto ? Length::Auto() : Length::Fixed(0),
      underlying_autos.is_left_auto ? Length::Auto() : Length::Fixed(0));
  return CreateClipValue(neutral_box, 1);
}

InterpolationValue CSSClipInterpolationType::MaybeConvertInitial(
    const StyleResolverState&,
    ConversionCheckers&) const {
  return nullptr;
}

InterpolationValue CSSClipInterpolationType::MaybeConvertInherit(
    const StyleResolverState& state,
    ConversionCheckers& conversion_checkers) const {
  conversion_checkers.push_back(
      InheritedClipChecker::Create(*state.ParentStyle()));
  if (state.ParentStyle()->HasAutoClip())
    return nullptr;
  return CreateClipValue(state.ParentStyle()->Clip(),
                         state.ParentStyle()->EffectiveZoom());
}

static bool IsCSSAuto(const CSSValue& value) {
  auto* identifier_value = DynamicTo<CSSIdentifierValue>(value);
  return identifier_value &&
         identifier_value->GetValueID() == CSSValueID::kAuto;
}

static std::unique_ptr<InterpolableValue> ConvertClipComponent(
    const CSSValue& length) {
  if (IsCSSAuto(length))
    return std::make_unique<InterpolableList>(0);
  return InterpolableLength::MaybeConvertCSSValue(length);
}

InterpolationValue CSSClipInterpolationType::MaybeConvertValue(
    const CSSValue& value,
    const StyleResolverState*,
    ConversionCheckers&) const {
  const auto* quad = DynamicTo<CSSQuadValue>(value);
  if (!quad)
    return nullptr;
  auto list = std::make_unique<InterpolableList>(kClipComponentIndexCount);
  list->Set(kClipTop, ConvertClipComponent(*quad->Top()));
  list->Set(kClipRight, ConvertClipComponent(*quad->Right()));
  list->Set(kClipBottom, ConvertClipComponent(*quad->Bottom()));
  list->Set(kClipLeft, ConvertClipComponent(*quad->Left()));
  ClipAutos autos(IsCSSAuto(*quad->Top()), IsCSSAuto(*quad->Right()),
                  IsCSSAuto(*quad->Bottom()), IsCSSAuto(*quad->Left()));
  return InterpolationValue(std::move(list),
                            CSSClipNonInterpolableValue::Create(autos));
}

InterpolationValue
CSSClipInterpolationType::MaybeConvertStandardPropertyUnderlyingValue(
    const ComputedStyle& style) const {
  if (style.HasAutoClip())
    return nullptr;
  return CreateClipValue(style.Clip(), style.EffectiveZoom());
}

PairwiseInterpolationValue CSSClipInterpolationType::MaybeMergeSingles(
    InterpolationValue&& start,
    InterpolationValue&& end) const {
  const ClipAutos& start_autos =
      ToCSSClipNonInterpolableValue(*start.non_interpolable_value)
          .GetClipAutos();
  const ClipAutos& end_autos =
      ToCSSClipNonInterpolableValue(*end.non_interpolable_value).GetClipAutos();
  if (start_autos != end_autos)
    return nullptr;
  return PairwiseInterpolationValue(std::move(start.interpolable_value),
                                    std::move(end.interpolable_value),
                                    std::move(start.non_interpolable_value));
}

void CSSClipInterpolationType::Composite(
    UnderlyingValueOwner& underlying_value_owner,
    double underlying_fraction,
    const InterpolationValue& value,
    double interpolation_fraction) const {
  const ClipAutos& underlying_autos =
      ToCSSClipNonInterpolableValue(
          *underlying_value_owner.Value().non_interpolable_value)
          .GetClipAutos();
  const ClipAutos& autos =
      ToCSSClipNonInterpolableValue(*value.non_interpolable_value)
          .GetClipAutos();
  if (underlying_autos == autos)
    underlying_value_owner.MutableValue().interpolable_value->ScaleAndAdd(
        underlying_fraction, *value.interpolable_value);
  else
    underlying_value_owner.Set(*this, value);
}

void CSSClipInterpolationType::ApplyStandardPropertyValue(
    const InterpolableValue& interpolable_value,
    const NonInterpolableValue* non_interpolable_value,
    StyleResolverState& state) const {
  const ClipAutos& autos =
      ToCSSClipNonInterpolableValue(non_interpolable_value)->GetClipAutos();
  const InterpolableList& list = ToInterpolableList(interpolable_value);
  const auto& convert_index = [&list, &state](bool is_auto, wtf_size_t index) {
    if (is_auto)
      return Length::Auto();
    return To<InterpolableLength>(*list.Get(index))
        .CreateLength(state.CssToLengthConversionData(), kValueRangeAll);
  };
  state.Style()->SetClip(
      LengthBox(convert_index(autos.is_top_auto, kClipTop),
                convert_index(autos.is_right_auto, kClipRight),
                convert_index(autos.is_bottom_auto, kClipBottom),
                convert_index(autos.is_left_auto, kClipLeft)));
}

}  // namespace blink
