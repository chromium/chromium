// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/animation/css_text_indent_interpolation_type.h"

#include <memory>
#include <utility>

#include "base/memory/ptr_util.h"
#include "third_party/blink/renderer/core/animation/interpolable_length.h"
#include "third_party/blink/renderer/core/css/css_identifier_value.h"
#include "third_party/blink/renderer/core/css/css_primitive_value.h"
#include "third_party/blink/renderer/core/css/css_value_list.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver_state.h"
#include "third_party/blink/renderer/core/style/computed_style.h"

namespace blink {

namespace {

struct IndentMode {
  IndentMode(const TextIndentLine line, const TextIndentType type)
      : line(line), type(type) {}
  explicit IndentMode(const ComputedStyle& style)
      : line(style.GetTextIndentLine()), type(style.GetTextIndentType()) {}

  bool operator==(const IndentMode& other) const {
    return line == other.line && type == other.type;
  }
  bool operator!=(const IndentMode& other) const { return !(*this == other); }

  const TextIndentLine line;
  const TextIndentType type;
};

}  // namespace

class CSSTextIndentNonInterpolableValue : public NonInterpolableValue {
 public:
  static scoped_refptr<CSSTextIndentNonInterpolableValue> Create(
      scoped_refptr<const NonInterpolableValue> length_non_interpolable_value,
      const IndentMode& mode) {
    return base::AdoptRef(new CSSTextIndentNonInterpolableValue(
        std::move(length_non_interpolable_value), mode));
  }

  const NonInterpolableValue* LengthNonInterpolableValue() const {
    return length_non_interpolable_value_.get();
  }
  const IndentMode& Mode() const { return mode_; }

  DECLARE_NON_INTERPOLABLE_VALUE_TYPE();

 private:
  CSSTextIndentNonInterpolableValue(
      scoped_refptr<const NonInterpolableValue> length_non_interpolable_value,
      const IndentMode& mode)
      : length_non_interpolable_value_(
            std::move(length_non_interpolable_value)),
        mode_(mode) {}

  scoped_refptr<const NonInterpolableValue> length_non_interpolable_value_;
  const IndentMode mode_;
};

DEFINE_NON_INTERPOLABLE_VALUE_TYPE(CSSTextIndentNonInterpolableValue);
DEFINE_NON_INTERPOLABLE_VALUE_TYPE_CASTS(CSSTextIndentNonInterpolableValue);

namespace {

class UnderlyingIndentModeChecker
    : public CSSInterpolationType::CSSConversionChecker {
 public:
  explicit UnderlyingIndentModeChecker(const IndentMode& mode) : mode_(mode) {}

  bool IsValid(const StyleResolverState&,
               const InterpolationValue& underlying) const final {
    return mode_ == ToCSSTextIndentNonInterpolableValue(
                        *underlying.non_interpolable_value)
                        .Mode();
  }

 private:
  const IndentMode mode_;
};

class InheritedIndentChecker
    : public CSSInterpolationType::CSSConversionChecker {
 public:
  InheritedIndentChecker(const Length& length, const IndentMode& mode)
      : length_(length), mode_(mode) {}

  bool IsValid(const StyleResolverState& state,
               const InterpolationValue&) const final {
    return length_ == state.ParentStyle()->TextIndent() &&
           mode_ == IndentMode(*state.ParentStyle());
  }

 private:
  const Length length_;
  const IndentMode mode_;
};

InterpolationValue CreateValue(const Length& length,
                               const IndentMode& mode,
                               double zoom) {
  InterpolationValue converted_length(
      InterpolableLength::MaybeConvertLength(length, zoom));
  DCHECK(converted_length);
  return InterpolationValue(
      std::move(converted_length.interpolable_value),
      CSSTextIndentNonInterpolableValue::Create(
          std::move(converted_length.non_interpolable_value), mode));
}

}  // namespace

InterpolationValue CSSTextIndentInterpolationType::MaybeConvertNeutral(
    const InterpolationValue& underlying,
    ConversionCheckers& conversion_checkers) const {
  IndentMode mode =
      ToCSSTextIndentNonInterpolableValue(*underlying.non_interpolable_value)
          .Mode();
  conversion_checkers.push_back(
      std::make_unique<UnderlyingIndentModeChecker>(mode));
  return CreateValue(Length::Fixed(0), mode, 1);
}

InterpolationValue CSSTextIndentInterpolationType::MaybeConvertInitial(
    const StyleResolverState&,
    ConversionCheckers&) const {
  IndentMode mode(ComputedStyleInitialValues::InitialTextIndentLine(),
                  ComputedStyleInitialValues::InitialTextIndentType());
  return CreateValue(ComputedStyleInitialValues::InitialTextIndent(), mode, 1);
}

InterpolationValue CSSTextIndentInterpolationType::MaybeConvertInherit(
    const StyleResolverState& state,
    ConversionCheckers& conversion_checkers) const {
  const ComputedStyle& parent_style = *state.ParentStyle();
  IndentMode mode(parent_style);
  conversion_checkers.push_back(std::make_unique<InheritedIndentChecker>(
      parent_style.TextIndent(), mode));
  return CreateValue(parent_style.TextIndent(), mode,
                     parent_style.EffectiveZoom());
}

InterpolationValue CSSTextIndentInterpolationType::MaybeConvertValue(
    const CSSValue& value,
    const StyleResolverState*,
    ConversionCheckers&) const {
  InterpolationValue length = nullptr;
  TextIndentLine line = ComputedStyleInitialValues::InitialTextIndentLine();
  TextIndentType type = ComputedStyleInitialValues::InitialTextIndentType();

  for (const auto& item : To<CSSValueList>(value)) {
    auto* identifier_value = DynamicTo<CSSIdentifierValue>(item.Get());
    if (identifier_value &&
        identifier_value->GetValueID() == CSSValueID::kEachLine) {
      line = TextIndentLine::kEachLine;
    } else if (identifier_value &&
               identifier_value->GetValueID() == CSSValueID::kHanging) {
      type = TextIndentType::kHanging;
    } else {
      length =
          InterpolationValue(InterpolableLength::MaybeConvertCSSValue(*item));
    }
  }
  DCHECK(length);

  return InterpolationValue(
      std::move(length.interpolable_value),
      CSSTextIndentNonInterpolableValue::Create(
          std::move(length.non_interpolable_value), IndentMode(line, type)));
}

InterpolationValue
CSSTextIndentInterpolationType::MaybeConvertStandardPropertyUnderlyingValue(
    const ComputedStyle& style) const {
  return CreateValue(style.TextIndent(), IndentMode(style),
                     style.EffectiveZoom());
}

PairwiseInterpolationValue CSSTextIndentInterpolationType::MaybeMergeSingles(
    InterpolationValue&& start,
    InterpolationValue&& end) const {
  const CSSTextIndentNonInterpolableValue& start_non_interpolable_value =
      ToCSSTextIndentNonInterpolableValue(*start.non_interpolable_value);
  const CSSTextIndentNonInterpolableValue& end_non_interpolable_value =
      ToCSSTextIndentNonInterpolableValue(*end.non_interpolable_value);

  if (start_non_interpolable_value.Mode() != end_non_interpolable_value.Mode())
    return nullptr;

  PairwiseInterpolationValue result = InterpolableLength::MergeSingles(
      std::move(start.interpolable_value), std::move(end.interpolable_value));
  result.non_interpolable_value = CSSTextIndentNonInterpolableValue::Create(
      std::move(result.non_interpolable_value),
      start_non_interpolable_value.Mode());
  return result;
}

void CSSTextIndentInterpolationType::Composite(
    UnderlyingValueOwner& underlying_value_owner,
    double underlying_fraction,
    const InterpolationValue& value,
    double interpolation_fraction) const {
  const IndentMode& underlying_mode =
      ToCSSTextIndentNonInterpolableValue(
          *underlying_value_owner.Value().non_interpolable_value)
          .Mode();
  const CSSTextIndentNonInterpolableValue& non_interpolable_value =
      ToCSSTextIndentNonInterpolableValue(*value.non_interpolable_value);
  const IndentMode& mode = non_interpolable_value.Mode();

  if (underlying_mode != mode) {
    underlying_value_owner.Set(*this, value);
    return;
  }

  underlying_value_owner.MutableInterpolableValue().ScaleAndAdd(
      underlying_fraction, *value.interpolable_value);
}

void CSSTextIndentInterpolationType::ApplyStandardPropertyValue(
    const InterpolableValue& interpolable_value,
    const NonInterpolableValue* non_interpolable_value,
    StyleResolverState& state) const {
  const CSSTextIndentNonInterpolableValue&
      css_text_indent_non_interpolable_value =
          ToCSSTextIndentNonInterpolableValue(*non_interpolable_value);
  ComputedStyle& style = *state.Style();
  style.SetTextIndent(
      To<InterpolableLength>(interpolable_value)
          .CreateLength(state.CssToLengthConversionData(), kValueRangeAll));

  const IndentMode& mode = css_text_indent_non_interpolable_value.Mode();
  style.SetTextIndentLine(mode.line);
  style.SetTextIndentType(mode.type);
}

}  // namespace blink
