// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/animation/css_font_style_interpolation_type.h"

#include <memory>

#include "base/memory/ptr_util.h"
#include "third_party/blink/renderer/core/animation/length_units_checker.h"
#include "third_party/blink/renderer/core/animation/tree_counting_checker.h"
#include "third_party/blink/renderer/core/animation/underlying_value_owner.h"
#include "third_party/blink/renderer/core/css/css_font_style_range_value.h"
#include "third_party/blink/renderer/core/css/css_identifier_value.h"
#include "third_party/blink/renderer/core/css/resolver/style_builder_converter.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/platform/wtf/math_extras.h"

namespace blink {

class CSSFontStyleNonInterpolableValue final : public NonInterpolableValue {
 public:
  explicit CSSFontStyleNonInterpolableValue(FontDescription::StyleSyntax source)
      : source_(source) {}
  ~CSSFontStyleNonInterpolableValue() final = default;

  FontDescription::StyleSyntax Source() const { return source_; }

  DECLARE_NON_INTERPOLABLE_VALUE_TYPE();

 private:
  const FontDescription::StyleSyntax source_;
};

DEFINE_NON_INTERPOLABLE_VALUE_TYPE(CSSFontStyleNonInterpolableValue);
template <>
struct DowncastTraits<CSSFontStyleNonInterpolableValue> {
  static bool AllowFrom(const NonInterpolableValue* value) {
    return value && AllowFrom(*value);
  }
  static bool AllowFrom(const NonInterpolableValue& value) {
    return value.GetType() == CSSFontStyleNonInterpolableValue::static_type_;
  }
};

namespace {

FontDescription::StyleSyntax Source(
    const NonInterpolableValue* non_interpolable_value) {
  const auto* wrapper =
      DynamicTo<CSSFontStyleNonInterpolableValue>(non_interpolable_value);
  return wrapper ? wrapper->Source()
                 : FontDescription::StyleSyntax::kImplicitAngle;
}

bool IsItalic(FontDescription::StyleSyntax source) {
  return source == FontDescription::StyleSyntax::kItalicKeyword;
}

// Merges two `StyleSyntax` values along the non-italic (oblique/normal) axis:
// the result has an explicit angle whenever either input did.
FontDescription::StyleSyntax MergeNonItalic(FontDescription::StyleSyntax a,
                                            FontDescription::StyleSyntax b) {
  DCHECK(!IsItalic(a));
  DCHECK(!IsItalic(b));
  if (a == FontDescription::StyleSyntax::kExplicitAngle ||
      b == FontDescription::StyleSyntax::kExplicitAngle) {
    return FontDescription::StyleSyntax::kExplicitAngle;
  }
  return FontDescription::StyleSyntax::kImplicitAngle;
}

}  // namespace

class InheritedFontStyleChecker
    : public CSSInterpolationType::CSSConversionChecker {
 public:
  InheritedFontStyleChecker(FontSelectionValue font_style,
                            FontDescription::StyleSyntax source)
      : font_style_(font_style), source_(source) {}

 private:
  bool IsValid(const StyleResolverState& state,
               const InterpolationValue&) const final {
    const FontDescription& parent = state.ParentStyle()->GetFontDescription();
    return font_style_ == parent.Style() && source_ == parent.GetStyleSyntax();
  }

  const double font_style_;
  const FontDescription::StyleSyntax source_;
};

class UnderlyingIsItalicKeywordChecker
    : public CSSInterpolationType::CSSConversionChecker {
 public:
  explicit UnderlyingIsItalicKeywordChecker(bool is_italic_keyword)
      : is_italic_keyword_(is_italic_keyword) {}

 private:
  bool IsValid(const StyleResolverState&,
               const InterpolationValue& underlying) const final {
    return is_italic_keyword_ ==
           IsItalic(Source(underlying.non_interpolable_value));
  }

  const bool is_italic_keyword_;
};

InterpolationValue CSSFontStyleInterpolationType::CreateFontStyleValue(
    FontSelectionValue font_style,
    FontDescription::StyleSyntax source) const {
  return InterpolationValue(
      MakeGarbageCollected<InterpolableNumber>(font_style),
      MakeGarbageCollected<CSSFontStyleNonInterpolableValue>(source));
}

InterpolationValue CSSFontStyleInterpolationType::MaybeConvertNeutral(
    const InterpolationValue& underlying,
    ConversionCheckers& conversion_checkers) const {
  // Additive identity: 0deg for oblique-axis values, italic itself for italic.
  // The neutral itself carries no explicit angle; Composite propagates one
  // from the keyframe via MergeNonItalic if needed.
  const bool underlying_is_italic =
      IsItalic(Source(underlying.non_interpolable_value));
  conversion_checkers.push_back(
      MakeGarbageCollected<UnderlyingIsItalicKeywordChecker>(
          underlying_is_italic));
  if (underlying_is_italic) {
    return CreateFontStyleValue(kItalicSlopeValue,
                                FontDescription::StyleSyntax::kItalicKeyword);
  }
  return CreateFontStyleValue(kNormalSlopeValue,
                              FontDescription::StyleSyntax::kImplicitAngle);
}

InterpolationValue CSSFontStyleInterpolationType::MaybeConvertInitial(
    const StyleResolverState&,
    ConversionCheckers& conversion_checkers) const {
  return CreateFontStyleValue(kNormalSlopeValue,
                              FontDescription::StyleSyntax::kImplicitAngle);
}

InterpolationValue CSSFontStyleInterpolationType::MaybeConvertInherit(
    const StyleResolverState& state,
    ConversionCheckers& conversion_checkers) const {
  DCHECK(state.ParentStyle());
  const FontDescription& parent = state.ParentStyle()->GetFontDescription();
  FontSelectionValue inherited_font_style = parent.Style();
  FontDescription::StyleSyntax inherited_source = parent.GetStyleSyntax();
  conversion_checkers.push_back(MakeGarbageCollected<InheritedFontStyleChecker>(
      inherited_font_style, inherited_source));
  return CreateFontStyleValue(inherited_font_style, inherited_source);
}

InterpolationValue CSSFontStyleInterpolationType::MaybeConvertValue(
    const CSSValue& value,
    const StyleResolverState& state,
    ConversionCheckers& conversion_checkers) const {
  FontDescription::StyleSyntax source =
      FontDescription::StyleSyntax::kImplicitAngle;
  if (const auto* identifier_value = DynamicTo<CSSIdentifierValue>(value)) {
    if (identifier_value->GetValueID() == CSSValueID::kItalic) {
      source = FontDescription::StyleSyntax::kItalicKeyword;
    }
  } else if (const auto* style_range_value =
                 DynamicTo<cssvalue::CSSFontStyleRangeValue>(value)) {
    const CSSValueList* values = style_range_value->GetObliqueValues();
    if (values && values->length()) {
      source = FontDescription::StyleSyntax::kExplicitAngle;
      const auto& primitive_value = To<CSSPrimitiveValue>(values->Item(0));
      if (primitive_value.IsElementDependent()) {
        conversion_checkers.push_back(
            TreeCountingChecker::Create(state.CssToLengthConversionData()));
      }
      CSSPrimitiveValue::LengthTypeFlags types;
      primitive_value.AccumulateLengthUnitTypes(types);
      if (CSSInterpolationType::ConversionChecker* length_units_checker =
              LengthUnitsChecker::MaybeCreate(types, state)) {
        conversion_checkers.push_back(length_units_checker);
      }
    }
  }
  return CreateFontStyleValue(StyleBuilderConverterBase::ConvertFontStyle(
                                  state.CssToLengthConversionData(), value),
                              source);
}

InterpolationValue
CSSFontStyleInterpolationType::MaybeConvertStandardPropertyUnderlyingValue(
    const ComputedStyle& style) const {
  return CreateFontStyleValue(style.GetFontStyle(),
                              style.GetFontDescription().GetStyleSyntax());
}

void CSSFontStyleInterpolationType::Composite(
    UnderlyingValueOwner& underlying_value_owner,
    double underlying_fraction,
    const InterpolationValue& value,
    double interpolation_fraction) const {
  // Angle-based values (`oblique <angle>` and `normal` as 0deg) animate
  // continuously between them, while the `italic` keyword animates discretely.
  const FontDescription::StyleSyntax underlying_source =
      Source(underlying_value_owner.Value().non_interpolable_value);
  const FontDescription::StyleSyntax value_source =
      Source(value.non_interpolable_value);
  if (IsItalic(underlying_source) || IsItalic(value_source)) {
    underlying_value_owner.Set(this, value);
    return;
  }
  underlying_value_owner.MutableValue().interpolable_value->ScaleAndAdd(
      underlying_fraction, *value.interpolable_value);
  underlying_value_owner.MutableValue().non_interpolable_value =
      MakeGarbageCollected<CSSFontStyleNonInterpolableValue>(
          MergeNonItalic(underlying_source, value_source));
}

PairwiseInterpolationValue CSSFontStyleInterpolationType::MaybeMergeSingles(
    InterpolationValue&& start,
    InterpolationValue&& end) const {
  // Interpolating between italic and oblique/normal must be discrete.
  const FontDescription::StyleSyntax start_source =
      Source(start.non_interpolable_value);
  const FontDescription::StyleSyntax end_source =
      Source(end.non_interpolable_value);
  if (IsItalic(start_source) != IsItalic(end_source)) {
    return nullptr;
  }

  // Among non-italic values, `kImplicitAngle` and `kExplicitAngle` serialize
  // differently only at the italic slope (14deg): `oblique` vs `oblique 14deg`.
  // When both endpoints share a slope the angle axis does not move, so a single
  // merged syntax would pin the whole transition to one serialization (e.g.
  // `oblique 14deg` -> `oblique` would stay `oblique 14deg` throughout, never
  // reaching `oblique`). Animate the syntax discretely instead, so each
  // endpoint keeps its own serialization (the framework flips at the 50% mark).
  if (start_source != end_source &&
      start.interpolable_value->Equals(*end.interpolable_value)) {
    return nullptr;
  }

  const FontDescription::StyleSyntax merged_source =
      IsItalic(end_source) ? end_source
                           : MergeNonItalic(start_source, end_source);
  return PairwiseInterpolationValue(
      std::move(start.interpolable_value), std::move(end.interpolable_value),
      MakeGarbageCollected<CSSFontStyleNonInterpolableValue>(merged_source));
}

void CSSFontStyleInterpolationType::ApplyStandardPropertyValue(
    const InterpolableValue& interpolable_value,
    const NonInterpolableValue* non_interpolable_value,
    StyleResolverState& state) const {
  const FontDescription::StyleSyntax source = Source(non_interpolable_value);
  const FontSelectionValue slope =
      IsItalic(source) ? kItalicSlopeValue
                       : FontSelectionValue(ClampTo(
                             To<InterpolableNumber>(interpolable_value)
                                 .Value(state.CssToLengthConversionData()),
                             kMinObliqueValue, kMaxObliqueValue));
  state.GetFontBuilder().SetStyle(slope);
  state.GetFontBuilder().SetStyleSyntax(source);
}

}  // namespace blink
