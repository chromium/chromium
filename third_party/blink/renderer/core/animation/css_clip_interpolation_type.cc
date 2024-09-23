// Copyright 2016 The Chromium Authors
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
  static InheritedClipChecker* Create(const ComputedStyle& parent_style) {
    Vector<Length> inherited_length_list;
    GetClipLengthList(parent_style, inherited_length_list);
    return MakeGarbageCollected<InheritedClipChecker>(
        std::move(inherited_length_list));
  }

  InheritedClipChecker(const Vector<Length>&& inherited_length_list)
      : inherited_length_list_(std::move(inherited_length_list)) {}

 private:
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

class CSSClipNonInterpolableValue final : public NonInterpolableValue {
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
template <>
struct DowncastTraits<CSSClipNonInterpolableValue> {
  static bool AllowFrom(const NonInterpolableValue* value) {
    return value && AllowFrom(*value);
  }
  static bool AllowFrom(const NonInterpolableValue& value) {
    return value.GetType() == CSSClipNonInterpolableValue::static_type_;
  }
};

class UnderlyingAutosChecker final
    : public CSSInterpolationType::CSSConversionChecker {
 public:
  explicit UnderlyingAutosChecker(const ClipAutos& underlying_autos)
      : underlying_autos_(underlying_autos) {}
  ~UnderlyingAutosChecker() final = default;

  static ClipAutos GetUnderlyingAutos(const InterpolationValue& underlying) {
    if (!underlying)
      return ClipAutos();
    return To<CSSClipNonInterpolableValue>(*underlying.non_interpolable_value)
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

static InterpolableValue* ConvertClipComponent(const Length& length,
                                               const CSSProperty& property,
                                               double zoom) {
  if (length.IsAuto()) {
    return MakeGarbageCollected<InterpolableList>(0);
  }
  return InterpolableLength::MaybeConvertLength(
      length, property, zoom,
      /*interpolate_size=*/std::nullopt);
}

static InterpolationValue CreateClipValue(const LengthBox& clip,
                                          const CSSProperty& property,
                                          double zoom) {
  auto* list = MakeGarbageCollected<InterpolableList>(kClipComponentIndexCount);
  list->Set(kClipTop, ConvertClipComponent(clip.Top(), property, zoom));
  list->Set(kClipRight, ConvertClipComponent(clip.Right(), property, zoom));
  list->Set(kClipBottom, ConvertClipComponent(clip.Bottom(), property, zoom));
  list->Set(kClipLeft, ConvertClipComponent(clip.Left(), property, zoom));
  return InterpolationValue(
      list, CSSClipNonInterpolableValue::Create(ClipAutos(clip)));
}

InterpolationValue CSSClipInterpolationType::MaybeConvertNeutral(
    const InterpolationValue& underlying,
    ConversionCheckers& conversion_checkers) const {
  ClipAutos underlying_autos =
      UnderlyingAutosChecker::GetUnderlyingAutos(underlying);
  conversion_checkers.push_back(
      MakeGarbageCollected<UnderlyingAutosChecker>(underlying_autos));
  if (underlying_autos.is_auto)
    return nullptr;
  LengthBox neutral_box(
      underlying_autos.is_top_auto ? Length::Auto() : Length::Fixed(0),
      underlying_autos.is_right_auto ? Length::Auto() : Length::Fixed(0),
      underlying_autos.is_bottom_auto ? Length::Auto() : Length::Fixed(0),
      underlying_autos.is_left_auto ? Length::Auto() : Length::Fixed(0));
  return CreateClipValue(neutral_box, CssProperty(), 1);
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
  return CreateClipValue(state.ParentStyle()->Clip(), CssProperty(),
                         state.ParentStyle()->EffectiveZoom());
}

static bool IsCSSAuto(const CSSValue& value) {
  auto* identifier_value = DynamicTo<CSSIdentifierValue>(value);
  return identifier_value &&
         identifier_value->GetValueID() == CSSValueID::kAuto;
}

static InterpolableValue* ConvertClipComponent(const CSSValue& length) {
  if (IsCSSAuto(length))
    return MakeGarbageCollected<InterpolableList>(0);
  return InterpolableLength::MaybeConvertCSSValue(length);
}

InterpolationValue CSSClipInterpolationType::MaybeConvertValue(
    const CSSValue& value,
    const StyleResolverState*,
    ConversionCheckers&) const {
  const auto* quad = DynamicTo<CSSQuadValue>(value);
  if (!quad)
    return nullptr;
  auto* list = MakeGarbageCollected<InterpolableList>(kClipComponentIndexCount);
  list->Set(kClipTop, ConvertClipComponent(*quad->Top()));
  list->Set(kClipRight, ConvertClipComponent(*quad->Right()));
  list->Set(kClipBottom, ConvertClipComponent(*quad->Bottom()));
  list->Set(kClipLeft, ConvertClipComponent(*quad->Left()));
  ClipAutos autos(IsCSSAuto(*quad->Top()), IsCSSAuto(*quad->Right()),
                  IsCSSAuto(*quad->Bottom()), IsCSSAuto(*quad->Left()));
  return InterpolationValue(list, CSSClipNonInterpolableValue::Create(autos));
}

InterpolationValue
CSSClipInterpolationType::MaybeConvertStandardPropertyUnderlyingValue(
    const ComputedStyle& style) const {
  if (style.HasAutoClip())
    return nullptr;
  return CreateClipValue(style.Clip(), CssProperty(), style.EffectiveZoom());
}

PairwiseInterpolationValue CSSClipInterpolationType::MaybeMergeSingles(
    InterpolationValue&& start,
    InterpolationValue&& end) const {
  const auto& start_autos =
      To<CSSClipNonInterpolableValue>(*start.non_interpolable_value)
          .GetClipAutos();
  const auto& end_autos =
      To<CSSClipNonInterpolableValue>(*end.non_interpolable_value)
          .GetClipAutos();
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
  const auto& underlying_autos =
      To<CSSClipNonInterpolableValue>(
          *underlying_value_owner.Value().non_interpolable_value)
          .GetClipAutos();
  const auto& autos =
      To<CSSClipNonInterpolableValue>(*value.non_interpolable_value)
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
  const auto& autos =
      To<CSSClipNonInterpolableValue>(non_interpolable_value)->GetClipAutos();
  const auto& list = To<InterpolableList>(interpolable_value);
  const auto& convert_index = [&list, &state](bool is_auto, wtf_size_t index) {
    if (is_auto)
      return Length::Auto();
    return To<InterpolableLength>(*list.Get(index))
        .CreateLength(state.CssToLengthConversionData(),
                      Length::ValueRange::kAll);
  };
  state.StyleBuilder().SetClip(
      LengthBox(convert_index(autos.is_top_auto, kClipTop),
                convert_index(autos.is_right_auto, kClipRight),
                convert_index(autos.is_bottom_auto, kClipBottom),
                convert_index(autos.is_left_auto, kClipLeft)));
}

}  // namespace blink
