// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/animation/length_property_functions.h"

#include "third_party/blink/renderer/core/style/computed_style.h"

namespace blink {

ValueRange LengthPropertyFunctions::GetValueRange(const CSSProperty& property) {
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
      return kValueRangeNonNegative;
    default:
      return kValueRangeAll;
  }
}

bool LengthPropertyFunctions::IsZoomedLength(const CSSProperty& property) {
  return property.PropertyID() != CSSPropertyID::kStrokeWidth;
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

bool LengthPropertyFunctions::GetInitialLength(const CSSProperty& property,
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
      return GetLength(property, ComputedStyle::InitialStyle(), result);
  }
}

bool LengthPropertyFunctions::GetLength(const CSSProperty& property,
                                        const ComputedStyle& style,
                                        Length& result) {
  switch (property.PropertyID()) {
    case CSSPropertyID::kBottom:
      result = style.Bottom();
      return true;
    case CSSPropertyID::kCx:
      result = style.SvgStyle().Cx();
      return true;
    case CSSPropertyID::kCy:
      result = style.SvgStyle().Cy();
      return true;
    case CSSPropertyID::kFlexBasis:
      result = style.FlexBasis();
      return true;
    case CSSPropertyID::kHeight:
      result = style.Height();
      return true;
    case CSSPropertyID::kLeft:
      result = style.Left();
      return true;
    case CSSPropertyID::kMarginBottom:
      result = style.MarginBottom();
      return true;
    case CSSPropertyID::kMarginLeft:
      result = style.MarginLeft();
      return true;
    case CSSPropertyID::kMarginRight:
      result = style.MarginRight();
      return true;
    case CSSPropertyID::kMarginTop:
      result = style.MarginTop();
      return true;
    case CSSPropertyID::kMaxHeight:
      result = style.MaxHeight();
      return true;
    case CSSPropertyID::kMaxWidth:
      result = style.MaxWidth();
      return true;
    case CSSPropertyID::kMinHeight:
      result = style.MinHeight();
      return true;
    case CSSPropertyID::kMinWidth:
      result = style.MinWidth();
      return true;
    case CSSPropertyID::kOffsetDistance:
      result = style.OffsetDistance();
      return true;
    case CSSPropertyID::kPaddingBottom:
      result = style.PaddingBottom();
      return true;
    case CSSPropertyID::kPaddingLeft:
      result = style.PaddingLeft();
      return true;
    case CSSPropertyID::kPaddingRight:
      result = style.PaddingRight();
      return true;
    case CSSPropertyID::kPaddingTop:
      result = style.PaddingTop();
      return true;
    case CSSPropertyID::kR:
      result = style.SvgStyle().R();
      return true;
    case CSSPropertyID::kRight:
      result = style.Right();
      return true;
    case CSSPropertyID::kRx:
      result = style.SvgStyle().Rx();
      return true;
    case CSSPropertyID::kRy:
      result = style.SvgStyle().Ry();
      return true;
    case CSSPropertyID::kShapeMargin:
      result = style.ShapeMargin();
      return true;
    case CSSPropertyID::kStrokeDashoffset:
      result = style.StrokeDashOffset();
      return true;
    case CSSPropertyID::kTextIndent:
      result = style.TextIndent();
      return true;
    case CSSPropertyID::kTop:
      result = style.Top();
      return true;
    case CSSPropertyID::kWebkitPerspectiveOriginX:
      result = style.PerspectiveOriginX();
      return true;
    case CSSPropertyID::kWebkitPerspectiveOriginY:
      result = style.PerspectiveOriginY();
      return true;
    case CSSPropertyID::kWebkitTransformOriginX:
      result = style.TransformOriginX();
      return true;
    case CSSPropertyID::kWebkitTransformOriginY:
      result = style.TransformOriginY();
      return true;
    case CSSPropertyID::kWidth:
      result = style.Width();
      return true;
    case CSSPropertyID::kX:
      result = style.SvgStyle().X();
      return true;
    case CSSPropertyID::kY:
      result = style.SvgStyle().Y();
      return true;

    case CSSPropertyID::kBorderBottomWidth:
      result = Length::Fixed(style.BorderBottomWidth());
      return true;
    case CSSPropertyID::kBorderLeftWidth:
      result = Length::Fixed(style.BorderLeftWidth());
      return true;
    case CSSPropertyID::kBorderRightWidth:
      result = Length::Fixed(style.BorderRightWidth());
      return true;
    case CSSPropertyID::kBorderTopWidth:
      result = Length::Fixed(style.BorderTopWidth());
      return true;
    case CSSPropertyID::kLetterSpacing:
      result = Length::Fixed(style.LetterSpacing());
      return true;
    case CSSPropertyID::kOutlineOffset:
      result = Length::Fixed(style.OutlineOffset());
      return true;
    case CSSPropertyID::kOutlineWidth:
      result = Length::Fixed(style.OutlineWidth());
      return true;
    case CSSPropertyID::kWebkitBorderHorizontalSpacing:
      result = Length::Fixed(style.HorizontalBorderSpacing());
      return true;
    case CSSPropertyID::kWebkitBorderVerticalSpacing:
      result = Length::Fixed(style.VerticalBorderSpacing());
      return true;
    case CSSPropertyID::kRowGap:
      if (style.RowGap().IsNormal())
        return false;
      result = style.RowGap().GetLength();
      return true;
    case CSSPropertyID::kColumnGap:
      if (style.ColumnGap().IsNormal())
        return false;
      result = style.ColumnGap().GetLength();
      return true;
    case CSSPropertyID::kColumnRuleWidth:
      result = Length::Fixed(style.ColumnRuleWidth());
      return true;
    case CSSPropertyID::kWebkitTransformOriginZ:
      result = Length::Fixed(style.TransformOriginZ());
      return true;
    case CSSPropertyID::kWordSpacing:
      result = Length::Fixed(style.WordSpacing());
      return true;

    case CSSPropertyID::kBaselineShift:
      if (style.BaselineShift() != BS_LENGTH)
        return false;
      result = style.BaselineShiftValue();
      return true;
    case CSSPropertyID::kLineHeight:
      // Percent Lengths are used to represent numbers on line-height.
      if (style.SpecifiedLineHeight().IsPercentOrCalc())
        return false;
      result = style.SpecifiedLineHeight();
      return true;
    case CSSPropertyID::kPerspective:
      if (!style.HasPerspective())
        return false;
      result = Length::Fixed(style.Perspective());
      return true;
    case CSSPropertyID::kStrokeWidth:
      DCHECK(!IsZoomedLength(CSSProperty::Get(CSSPropertyID::kStrokeWidth)));
      result = style.StrokeWidth().length();
      return true;
    case CSSPropertyID::kVerticalAlign:
      if (style.VerticalAlign() != EVerticalAlign::kLength)
        return false;
      result = style.GetVerticalAlignLength();
      return true;
    case CSSPropertyID::kColumnWidth:
      if (style.HasAutoColumnWidth())
        return false;
      result = Length::Fixed(style.ColumnWidth());
      return true;
    default:
      return false;
  }
}

bool LengthPropertyFunctions::SetLength(const CSSProperty& property,
                                        ComputedStyle& style,
                                        const Length& value) {
  switch (property.PropertyID()) {
    // Setters that take a Length value.
    case CSSPropertyID::kBaselineShift:
      style.SetBaselineShiftValue(value);
      return true;
    case CSSPropertyID::kBottom:
      style.SetBottom(value);
      return true;
    case CSSPropertyID::kCx:
      style.SetCx(value);
      return true;
    case CSSPropertyID::kCy:
      style.SetCy(value);
      return true;
    case CSSPropertyID::kFlexBasis:
      style.SetFlexBasis(value);
      return true;
    case CSSPropertyID::kHeight:
      style.SetHeight(value);
      return true;
    case CSSPropertyID::kLeft:
      style.SetLeft(value);
      return true;
    case CSSPropertyID::kMarginBottom:
      style.SetMarginBottom(value);
      return true;
    case CSSPropertyID::kMarginLeft:
      style.SetMarginLeft(value);
      return true;
    case CSSPropertyID::kMarginRight:
      style.SetMarginRight(value);
      return true;
    case CSSPropertyID::kMarginTop:
      style.SetMarginTop(value);
      return true;
    case CSSPropertyID::kMaxHeight:
      style.SetMaxHeight(value);
      return true;
    case CSSPropertyID::kMaxWidth:
      style.SetMaxWidth(value);
      return true;
    case CSSPropertyID::kMinHeight:
      style.SetMinHeight(value);
      return true;
    case CSSPropertyID::kMinWidth:
      style.SetMinWidth(value);
      return true;
    case CSSPropertyID::kOffsetDistance:
      style.SetOffsetDistance(value);
      return true;
    case CSSPropertyID::kPaddingBottom:
      style.SetPaddingBottom(value);
      return true;
    case CSSPropertyID::kPaddingLeft:
      style.SetPaddingLeft(value);
      return true;
    case CSSPropertyID::kPaddingRight:
      style.SetPaddingRight(value);
      return true;
    case CSSPropertyID::kPaddingTop:
      style.SetPaddingTop(value);
      return true;
    case CSSPropertyID::kR:
      style.SetR(value);
      return true;
    case CSSPropertyID::kRx:
      style.SetRx(value);
      return true;
    case CSSPropertyID::kRy:
      style.SetRy(value);
      return true;
    case CSSPropertyID::kRight:
      style.SetRight(value);
      return true;
    case CSSPropertyID::kShapeMargin:
      style.SetShapeMargin(value);
      return true;
    case CSSPropertyID::kStrokeDashoffset:
      style.SetStrokeDashOffset(value);
      return true;
    case CSSPropertyID::kTop:
      style.SetTop(value);
      return true;
    case CSSPropertyID::kWidth:
      style.SetWidth(value);
      return true;
    case CSSPropertyID::kWebkitPerspectiveOriginX:
      style.SetPerspectiveOriginX(value);
      return true;
    case CSSPropertyID::kWebkitPerspectiveOriginY:
      style.SetPerspectiveOriginY(value);
      return true;
    case CSSPropertyID::kWebkitTransformOriginX:
      style.SetTransformOriginX(value);
      return true;
    case CSSPropertyID::kWebkitTransformOriginY:
      style.SetTransformOriginY(value);
      return true;
    case CSSPropertyID::kX:
      style.SetX(value);
      return true;
    case CSSPropertyID::kY:
      style.SetY(value);
      return true;

    case CSSPropertyID::kLineHeight:
      // Percent Lengths are used to represent numbers on line-height.
      if (value.IsPercentOrCalc())
        return false;
      style.SetLineHeight(value);
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
      return false;

    default:
      return false;
  }
}

}  // namespace blink
