// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/animation/length_property_functions.h"

#include "third_party/blink/renderer/core/animation/interpolable_length.h"
#include "third_party/blink/renderer/core/style/computed_style.h"

namespace blink {

Length::ValueRange LengthPropertyFunctions::GetValueRange(
    const CSSProperty& property) {
  switch (property.PropertyID()) {
    case CSSPropertyID::kBorderBottomWidth:
    case CSSPropertyID::kBorderLeftWidth:
    case CSSPropertyID::kBorderRightWidth:
    case CSSPropertyID::kBorderTopWidth:
    case CSSPropertyID::kFlexBasis:
    case CSSPropertyID::kHeight:
    case CSSPropertyID::kLineHeight:
    case CSSPropertyID::kMaxHeight:
    case CSSPropertyID::kMaxWidth:
    case CSSPropertyID::kMinHeight:
    case CSSPropertyID::kMinWidth:
    case CSSPropertyID::kOutlineWidth:
    case CSSPropertyID::kPaddingBottom:
    case CSSPropertyID::kPaddingLeft:
    case CSSPropertyID::kPaddingRight:
    case CSSPropertyID::kPaddingTop:
    case CSSPropertyID::kPerspective:
    case CSSPropertyID::kR:
    case CSSPropertyID::kRx:
    case CSSPropertyID::kRy:
    case CSSPropertyID::kShapeMargin:
    case CSSPropertyID::kStrokeWidth:
    case CSSPropertyID::kWebkitBorderHorizontalSpacing:
    case CSSPropertyID::kWebkitBorderVerticalSpacing:
    case CSSPropertyID::kColumnGap:
    case CSSPropertyID::kRowGap:
    case CSSPropertyID::kColumnWidth:
    case CSSPropertyID::kWidth:
    case CSSPropertyID::kTabSize:
      return Length::ValueRange::kNonNegative;
    default:
      return Length::ValueRange::kAll;
  }
}

bool LengthPropertyFunctions::IsZoomedLength(const CSSProperty& property) {
  return property.PropertyID() != CSSPropertyID::kStrokeWidth;
}

bool LengthPropertyFunctions::CanAnimateKeyword(const CSSProperty& property,
                                                CSSValueID value_id) {
  bool is_max_size = false;
  switch (CSSPropertyID property_id = property.PropertyID()) {
    case CSSPropertyID::kMaxWidth:
    case CSSPropertyID::kMaxHeight:
      is_max_size = true;
      [[fallthrough]];
    case CSSPropertyID::kFlexBasis:
    case CSSPropertyID::kWidth:
    case CSSPropertyID::kHeight:
    case CSSPropertyID::kMinWidth:
    case CSSPropertyID::kMinHeight:
      if (RuntimeEnabledFeatures::CSSCalcSizeFunctionEnabled()) {
        switch (value_id) {
          case CSSValueID::kContent:
            return property_id == CSSPropertyID::kFlexBasis;
          case CSSValueID::kAuto:
            return !is_max_size;
          case CSSValueID::kMinContent:
          case CSSValueID::kMaxContent:
          case CSSValueID::kFitContent:
            return true;
          case CSSValueID::kWebkitMinContent:
          case CSSValueID::kWebkitMaxContent:
          case CSSValueID::kWebkitFitContent:
          case CSSValueID::kWebkitFillAvailable:
            return property_id != CSSPropertyID::kFlexBasis;
          default:
            return false;
        }
      }
      return false;
    default:
      return false;
  }
}

bool LengthPropertyFunctions::GetPixelsForKeyword(const CSSProperty& property,
                                                  CSSValueID value_id,
                                                  double& result) {
  switch (property.PropertyID()) {
    case CSSPropertyID::kBaselineShift:
      if (value_id == CSSValueID::kBaseline) {
        result = 0;
        return true;
      }
      return false;
    case CSSPropertyID::kBorderBottomWidth:
    case CSSPropertyID::kBorderLeftWidth:
    case CSSPropertyID::kBorderRightWidth:
    case CSSPropertyID::kBorderTopWidth:
    case CSSPropertyID::kColumnRuleWidth:
    case CSSPropertyID::kOutlineWidth:
      if (value_id == CSSValueID::kThin) {
        result = 1;
        return true;
      }
      if (value_id == CSSValueID::kMedium) {
        result = 3;
        return true;
      }
      if (value_id == CSSValueID::kThick) {
        result = 5;
        return true;
      }
      return false;
    case CSSPropertyID::kLetterSpacing:
    case CSSPropertyID::kWordSpacing:
      if (value_id == CSSValueID::kNormal) {
        result = 0;
        return true;
      }
      return false;
    default:
      return false;
  }
}

bool LengthPropertyFunctions::GetInitialLength(
    const CSSProperty& property,
    const ComputedStyle& initial_style,
    Length& result) {
  switch (property.PropertyID()) {
    // The computed value of "initial" for the following properties is 0px if
    // the associated *-style property resolves to "none" or "hidden".
    // - border-width:
    //   https://drafts.csswg.org/css-backgrounds-3/#the-border-width
    // - outline-width: https://drafts.csswg.org/css-ui-3/#outline-width
    // - column-rule-width: https://drafts.csswg.org/css-multicol-1/#crw
    // We ignore this value adjustment for animations and use the wrong value
    // for hidden widths to avoid having to restart our animations based on the
    // computed *-style values. This is acceptable since animations running on
    // hidden widths are unobservable to the user, even via getComputedStyle().
    case CSSPropertyID::kBorderBottomWidth:
    case CSSPropertyID::kBorderLeftWidth:
    case CSSPropertyID::kBorderRightWidth:
    case CSSPropertyID::kBorderTopWidth:
      result = Length::Fixed(ComputedStyleInitialValues::InitialBorderWidth());
      return true;
    case CSSPropertyID::kOutlineWidth:
      result = Length::Fixed(ComputedStyleInitialValues::InitialOutlineWidth());
      return true;
    case CSSPropertyID::kColumnRuleWidth:
      result =
          Length::Fixed(ComputedStyleInitialValues::InitialColumnRuleWidth());
      return true;

    default:
      return GetLength(property, initial_style, result);
  }
}

bool LengthPropertyFunctions::GetLength(const CSSProperty& property,
                                        const ComputedStyle& style,
                                        Length& result_param) {
  bool success = false;
  Length result;
  switch (property.PropertyID()) {
    case CSSPropertyID::kBottom:
      result = style.Bottom();
      success = true;
      break;
    case CSSPropertyID::kCx:
      result = style.Cx();
      success = true;
      break;
    case CSSPropertyID::kCy:
      result = style.Cy();
      success = true;
      break;
    case CSSPropertyID::kFlexBasis:
      result = style.FlexBasis();
      success = true;
      break;
    case CSSPropertyID::kHeight:
      result = style.Height();
      success = true;
      break;
    case CSSPropertyID::kLeft:
      result = style.Left();
      success = true;
      break;
    case CSSPropertyID::kMarginBottom:
      result = style.MarginBottom();
      success = true;
      break;
    case CSSPropertyID::kMarginLeft:
      result = style.MarginLeft();
      success = true;
      break;
    case CSSPropertyID::kMarginRight:
      result = style.MarginRight();
      success = true;
      break;
    case CSSPropertyID::kMarginTop:
      result = style.MarginTop();
      success = true;
      break;
    case CSSPropertyID::kMaxHeight:
      result = style.MaxHeight();
      success = true;
      break;
    case CSSPropertyID::kMaxWidth:
      result = style.MaxWidth();
      success = true;
      break;
    case CSSPropertyID::kMinHeight:
      result = style.MinHeight();
      success = true;
      break;
    case CSSPropertyID::kMinWidth:
      result = style.MinWidth();
      success = true;
      break;
    case CSSPropertyID::kOffsetDistance:
      result = style.OffsetDistance();
      success = true;
      break;
    case CSSPropertyID::kPaddingBottom:
      result = style.PaddingBottom();
      success = true;
      break;
    case CSSPropertyID::kPaddingLeft:
      result = style.PaddingLeft();
      success = true;
      break;
    case CSSPropertyID::kPaddingRight:
      result = style.PaddingRight();
      success = true;
      break;
    case CSSPropertyID::kPaddingTop:
      result = style.PaddingTop();
      success = true;
      break;
    case CSSPropertyID::kR:
      result = style.R();
      success = true;
      break;
    case CSSPropertyID::kRight:
      result = style.Right();
      success = true;
      break;
    case CSSPropertyID::kRx:
      result = style.Rx();
      success = true;
      break;
    case CSSPropertyID::kRy:
      result = style.Ry();
      success = true;
      break;
    case CSSPropertyID::kShapeMargin:
      result = style.ShapeMargin();
      success = true;
      break;
    case CSSPropertyID::kStrokeDashoffset:
      result = style.StrokeDashOffset();
      success = true;
      break;
    case CSSPropertyID::kTextIndent:
      result = style.TextIndent();
      success = true;
      break;
    case CSSPropertyID::kTextUnderlineOffset:
      result = style.TextUnderlineOffset();
      success = true;
      break;
    case CSSPropertyID::kTop:
      result = style.Top();
      success = true;
      break;
    case CSSPropertyID::kWebkitPerspectiveOriginX:
      result = style.PerspectiveOrigin().X();
      success = true;
      break;
    case CSSPropertyID::kWebkitPerspectiveOriginY:
      result = style.PerspectiveOrigin().Y();
      success = true;
      break;
    case CSSPropertyID::kWebkitTransformOriginX:
      result = style.GetTransformOrigin().X();
      success = true;
      break;
    case CSSPropertyID::kWebkitTransformOriginY:
      result = style.GetTransformOrigin().Y();
      success = true;
      break;
    case CSSPropertyID::kWidth:
      result = style.Width();
      success = true;
      break;
    case CSSPropertyID::kX:
      result = style.X();
      success = true;
      break;
    case CSSPropertyID::kY:
      result = style.Y();
      success = true;
      break;

    case CSSPropertyID::kBorderBottomWidth:
      result = Length::Fixed(style.BorderBottomWidth());
      success = true;
      break;
    case CSSPropertyID::kBorderLeftWidth:
      result = Length::Fixed(style.BorderLeftWidth());
      success = true;
      break;
    case CSSPropertyID::kBorderRightWidth:
      result = Length::Fixed(style.BorderRightWidth());
      success = true;
      break;
    case CSSPropertyID::kBorderTopWidth:
      result = Length::Fixed(style.BorderTopWidth());
      success = true;
      break;
    case CSSPropertyID::kLetterSpacing:
      result = Length::Fixed(style.LetterSpacing());
      success = true;
      break;
    case CSSPropertyID::kOutlineOffset:
      result = Length::Fixed(style.OutlineOffset());
      success = true;
      break;
    case CSSPropertyID::kOutlineWidth:
      result = Length::Fixed(style.OutlineWidth());
      success = true;
      break;
    case CSSPropertyID::kWebkitBorderHorizontalSpacing:
      result = Length::Fixed(style.HorizontalBorderSpacing());
      success = true;
      break;
    case CSSPropertyID::kWebkitBorderVerticalSpacing:
      result = Length::Fixed(style.VerticalBorderSpacing());
      success = true;
      break;
    case CSSPropertyID::kRowGap:
      if (style.RowGap()) {
        result = *style.RowGap();
        success = true;
      }
      break;
    case CSSPropertyID::kColumnGap:
      if (style.ColumnGap()) {
        result = *style.ColumnGap();
        success = true;
      }
      break;
    case CSSPropertyID::kColumnRuleWidth:
      result = Length::Fixed(style.ColumnRuleWidth());
      success = true;
      break;
    case CSSPropertyID::kWebkitTransformOriginZ:
      result = Length::Fixed(style.GetTransformOrigin().Z());
      success = true;
      break;
    case CSSPropertyID::kWordSpacing:
      result = Length::Fixed(style.WordSpacing());
      success = true;
      break;

    case CSSPropertyID::kBaselineShift:
      if (style.BaselineShiftType() == EBaselineShiftType::kLength) {
        result = style.BaselineShift();
        success = true;
      }
      break;
    case CSSPropertyID::kLineHeight: {
      const Length& line_height = style.SpecifiedLineHeight();
      // Percent Lengths are used to represent numbers on line-height.
      if (!line_height.HasPercent()) {
        result = line_height;
        success = true;
      }
      break;
    }
    case CSSPropertyID::kTabSize:
      if (!style.GetTabSize().IsSpaces()) {
        result = Length::Fixed(style.GetTabSize().float_value_);
        success = true;
      }
      break;
    case CSSPropertyID::kPerspective:
      if (style.HasPerspective()) {
        result = Length::Fixed(style.Perspective());
        success = true;
      }
      break;
    case CSSPropertyID::kStrokeWidth:
      DCHECK(!IsZoomedLength(CSSProperty::Get(CSSPropertyID::kStrokeWidth)));
      result = style.StrokeWidth().length();
      success = true;
      break;
    case CSSPropertyID::kVerticalAlign:
      if (style.VerticalAlign() == EVerticalAlign::kLength) {
        result = style.GetVerticalAlignLength();
        success = true;
      }
      break;
    case CSSPropertyID::kColumnWidth:
      if (!style.HasAutoColumnWidth()) {
        result = Length::Fixed(style.ColumnWidth());
        success = true;
      }
      break;
    default:
      break;
  }

  // Don't report a length that will convert to a keyword if the property
  // doesn't support animation of that keyword.
  if (success) {
    CSSValueID id =
        InterpolableLength::LengthTypeToCSSValueID(result.GetType());
    if (id != CSSValueID::kInvalid && !CanAnimateKeyword(property, id)) {
      success = false;
    }
  }

  if (success) {
    result_param = std::move(result);
  }

  return success;
}

bool LengthPropertyFunctions::SetLength(const CSSProperty& property,
                                        ComputedStyleBuilder& builder,
                                        const Length& value) {
  switch (property.PropertyID()) {
    // Setters that take a Length value.
    case CSSPropertyID::kBaselineShift:
      builder.SetBaselineShiftType(EBaselineShiftType::kLength);
      builder.SetBaselineShift(value);
      return true;
    case CSSPropertyID::kBottom:
      builder.SetBottom(value);
      return true;
    case CSSPropertyID::kCx:
      builder.SetCx(value);
      return true;
    case CSSPropertyID::kCy:
      builder.SetCy(value);
      return true;
    case CSSPropertyID::kFlexBasis:
      builder.SetFlexBasis(value);
      return true;
    case CSSPropertyID::kHeight:
      builder.SetHeight(value);
      return true;
    case CSSPropertyID::kLeft:
      builder.SetLeft(value);
      return true;
    case CSSPropertyID::kMarginBottom:
      builder.SetMarginBottom(value);
      return true;
    case CSSPropertyID::kMarginLeft:
      builder.SetMarginLeft(value);
      return true;
    case CSSPropertyID::kMarginRight:
      builder.SetMarginRight(value);
      return true;
    case CSSPropertyID::kMarginTop:
      builder.SetMarginTop(value);
      return true;
    case CSSPropertyID::kMaxHeight:
      builder.SetMaxHeight(value);
      return true;
    case CSSPropertyID::kMaxWidth:
      builder.SetMaxWidth(value);
      return true;
    case CSSPropertyID::kMinHeight:
      builder.SetMinHeight(value);
      return true;
    case CSSPropertyID::kMinWidth:
      builder.SetMinWidth(value);
      return true;
    case CSSPropertyID::kOffsetDistance:
      builder.SetOffsetDistance(value);
      return true;
    case CSSPropertyID::kPaddingBottom:
      builder.SetPaddingBottom(value);
      return true;
    case CSSPropertyID::kPaddingLeft:
      builder.SetPaddingLeft(value);
      return true;
    case CSSPropertyID::kPaddingRight:
      builder.SetPaddingRight(value);
      return true;
    case CSSPropertyID::kPaddingTop:
      builder.SetPaddingTop(value);
      return true;
    case CSSPropertyID::kR:
      builder.SetR(value);
      return true;
    case CSSPropertyID::kRx:
      builder.SetRx(value);
      return true;
    case CSSPropertyID::kRy:
      builder.SetRy(value);
      return true;
    case CSSPropertyID::kRight:
      builder.SetRight(value);
      return true;
    case CSSPropertyID::kShapeMargin:
      builder.SetShapeMargin(value);
      return true;
    case CSSPropertyID::kStrokeDashoffset:
      builder.SetStrokeDashOffset(value);
      return true;
    case CSSPropertyID::kTop:
      builder.SetTop(value);
      return true;
    case CSSPropertyID::kWidth:
      builder.SetWidth(value);
      return true;
    case CSSPropertyID::kWebkitPerspectiveOriginX:
      builder.SetPerspectiveOriginX(value);
      return true;
    case CSSPropertyID::kWebkitPerspectiveOriginY:
      builder.SetPerspectiveOriginY(value);
      return true;
    case CSSPropertyID::kWebkitTransformOriginX:
      builder.SetTransformOriginX(value);
      return true;
    case CSSPropertyID::kWebkitTransformOriginY:
      builder.SetTransformOriginY(value);
      return true;
    case CSSPropertyID::kX:
      builder.SetX(value);
      return true;
    case CSSPropertyID::kY:
      builder.SetY(value);
      return true;

    case CSSPropertyID::kLineHeight:
      // Percent Lengths are used to represent numbers on line-height.
      if (value.HasPercent()) {
        return false;
      }
      builder.SetLineHeight(value);
      return true;

    // TODO(alancutter): Support setters that take a numeric value (need to
    // resolve percentages).
    case CSSPropertyID::kBorderBottomWidth:
    case CSSPropertyID::kBorderLeftWidth:
    case CSSPropertyID::kBorderRightWidth:
    case CSSPropertyID::kBorderTopWidth:
    case CSSPropertyID::kLetterSpacing:
    case CSSPropertyID::kOutlineOffset:
    case CSSPropertyID::kOutlineWidth:
    case CSSPropertyID::kPerspective:
    case CSSPropertyID::kStrokeWidth:
    case CSSPropertyID::kVerticalAlign:
    case CSSPropertyID::kWebkitBorderHorizontalSpacing:
    case CSSPropertyID::kWebkitBorderVerticalSpacing:
    case CSSPropertyID::kColumnGap:
    case CSSPropertyID::kRowGap:
    case CSSPropertyID::kColumnRuleWidth:
    case CSSPropertyID::kColumnWidth:
    case CSSPropertyID::kWebkitTransformOriginZ:
    case CSSPropertyID::kWordSpacing:
    case CSSPropertyID::kTabSize:
      return false;

    default:
      return false;
  }
}

}  // namespace blink
