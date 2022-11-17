// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/css_property_equality.h"

#include "base/memory/values_equivalent.h"
#include "third_party/blink/renderer/core/animation/property_handle.h"
#include "third_party/blink/renderer/core/css/css_value.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/core/style/shadow_list.h"

// TODO(ikilpatrick): generate this file.

namespace blink {

namespace {

template <CSSPropertyID property>
bool FillLayersEqual(const FillLayer& a_layers, const FillLayer& b_layers) {
  const FillLayer* a_layer = &a_layers;
  const FillLayer* b_layer = &b_layers;
  while (a_layer && b_layer) {
    switch (property) {
      case CSSPropertyID::kBackgroundPositionX:
      case CSSPropertyID::kWebkitMaskPositionX:
        if (a_layer->PositionX() != b_layer->PositionX())
          return false;
        if (a_layer->BackgroundXOrigin() != b_layer->BackgroundXOrigin())
          return false;
        break;
      case CSSPropertyID::kBackgroundPositionY:
      case CSSPropertyID::kWebkitMaskPositionY:
        if (a_layer->PositionY() != b_layer->PositionY())
          return false;
        if (a_layer->BackgroundYOrigin() != b_layer->BackgroundYOrigin())
          return false;
        break;
      case CSSPropertyID::kBackgroundSize:
      case CSSPropertyID::kWebkitMaskSize:
        if (!(a_layer->SizeLength() == b_layer->SizeLength()))
          return false;
        break;
      case CSSPropertyID::kBackgroundImage:
        if (!base::ValuesEquivalent(a_layer->GetImage(), b_layer->GetImage()))
          return false;
        break;
      default:
        NOTREACHED();
        return true;
    }

    a_layer = a_layer->Next();
    b_layer = b_layer->Next();
  }

  // FIXME: Shouldn't this be return !aLayer && !bLayer; ?
  return true;
}

}  // namespace

bool CSSPropertyEquality::PropertiesEqual(const PropertyHandle& property,
                                          const ComputedStyle& a,
                                          const ComputedStyle& b) {
  if (property.IsCSSCustomProperty()) {
    const AtomicString& name = property.CustomPropertyName();
    return base::ValuesEquivalent(a.GetVariableValue(name),
                                  b.GetVariableValue(name));
  }
  switch (property.GetCSSProperty().PropertyID()) {
    case CSSPropertyID::kBackgroundColor:
      return a.BackgroundColor() == b.BackgroundColor() &&
             a.InternalVisitedBackgroundColor() ==
                 b.InternalVisitedBackgroundColor();
    case CSSPropertyID::kBackgroundImage:
      return FillLayersEqual<CSSPropertyID::kBackgroundImage>(
          a.BackgroundLayers(), b.BackgroundLayers());
    case CSSPropertyID::kBackgroundPositionX:
      return FillLayersEqual<CSSPropertyID::kBackgroundPositionX>(
          a.BackgroundLayers(), b.BackgroundLayers());
    case CSSPropertyID::kBackgroundPositionY:
      return FillLayersEqual<CSSPropertyID::kBackgroundPositionY>(
          a.BackgroundLayers(), b.BackgroundLayers());
    case CSSPropertyID::kBackgroundSize:
      return FillLayersEqual<CSSPropertyID::kBackgroundSize>(
          a.BackgroundLayers(), b.BackgroundLayers());
    case CSSPropertyID::kBaselineShift:
      return a.BaselineShift() == b.BaselineShift();
    case CSSPropertyID::kBorderBottomColor:
      return a.BorderBottomColor() == b.BorderBottomColor() &&
             a.InternalVisitedBorderBottomColor() ==
                 b.InternalVisitedBorderBottomColor();
    case CSSPropertyID::kBorderBottomLeftRadius:
      return a.BorderBottomLeftRadius() == b.BorderBottomLeftRadius();
    case CSSPropertyID::kBorderBottomRightRadius:
      return a.BorderBottomRightRadius() == b.BorderBottomRightRadius();
    case CSSPropertyID::kBorderBottomWidth:
      return a.BorderBottomWidth() == b.BorderBottomWidth();
    case CSSPropertyID::kBorderImageOutset:
      return a.BorderImageOutset() == b.BorderImageOutset();
    case CSSPropertyID::kBorderImageSlice:
      return a.BorderImageSlices() == b.BorderImageSlices();
    case CSSPropertyID::kBorderImageSource:
      return base::ValuesEquivalent(a.BorderImageSource(),
                                    b.BorderImageSource());
    case CSSPropertyID::kBorderImageWidth:
      return a.BorderImageWidth() == b.BorderImageWidth();
    case CSSPropertyID::kBorderLeftColor:
      return a.BorderLeftColor() == b.BorderLeftColor() &&
             a.InternalVisitedBorderLeftColor() ==
                 b.InternalVisitedBorderLeftColor();
    case CSSPropertyID::kBorderLeftWidth:
      return a.BorderLeftWidth() == b.BorderLeftWidth();
    case CSSPropertyID::kBorderRightColor:
      return a.BorderRightColor() == b.BorderRightColor() &&
             a.InternalVisitedBorderRightColor() ==
                 b.InternalVisitedBorderRightColor();
    case CSSPropertyID::kBorderRightWidth:
      return a.BorderRightWidth() == b.BorderRightWidth();
    case CSSPropertyID::kBorderTopColor:
      return a.BorderTopColor() == b.BorderTopColor() &&
             a.InternalVisitedBorderTopColor() ==
                 b.InternalVisitedBorderTopColor();
    case CSSPropertyID::kBorderTopLeftRadius:
      return a.BorderTopLeftRadius() == b.BorderTopLeftRadius();
    case CSSPropertyID::kBorderTopRightRadius:
      return a.BorderTopRightRadius() == b.BorderTopRightRadius();
    case CSSPropertyID::kBorderTopWidth:
      return a.BorderTopWidth() == b.BorderTopWidth();
    case CSSPropertyID::kBottom:
      return a.Bottom() == b.Bottom();
    case CSSPropertyID::kBoxShadow:
      return base::ValuesEquivalent(a.BoxShadow(), b.BoxShadow());
    case CSSPropertyID::kCaretColor:
      return a.CaretColor() == b.CaretColor() &&
             a.InternalVisitedCaretColor() == b.InternalVisitedCaretColor();
    case CSSPropertyID::kClip:
      return a.Clip() == b.Clip();
    case CSSPropertyID::kColor:
      return a.Color() == b.Color() &&
             a.InternalVisitedColor() == b.InternalVisitedColor();
    case CSSPropertyID::kFill:
      return a.FillPaint().EqualTypeOrColor(b.FillPaint()) &&
             a.InternalVisitedFillPaint().EqualTypeOrColor(
                 b.InternalVisitedFillPaint());
    case CSSPropertyID::kFillOpacity:
      return a.FillOpacity() == b.FillOpacity();
    case CSSPropertyID::kFlexBasis:
      return a.FlexBasis() == b.FlexBasis();
    case CSSPropertyID::kFlexGrow:
      return a.FlexGrow() == b.FlexGrow();
    case CSSPropertyID::kFlexShrink:
      return a.FlexShrink() == b.FlexShrink();
    case CSSPropertyID::kFloodColor:
      return a.FloodColor() == b.FloodColor();
    case CSSPropertyID::kFloodOpacity:
      return a.FloodOpacity() == b.FloodOpacity();
    case CSSPropertyID::kFontSize:
      // CSSPropertyID::kFontSize: Must pass a specified size to setFontSize if
      // Text Autosizing is enabled, but a computed size if text zoom is enabled
      // (if neither is enabled it's irrelevant as they're probably the same).
      // FIXME: Should we introduce an option to pass the computed font size
      // here, allowing consumers to enable text zoom rather than Text
      // Autosizing? See http://crbug.com/227545.
      return a.SpecifiedFontSize() == b.SpecifiedFontSize();
    case CSSPropertyID::kFontSizeAdjust:
      return a.FontSizeAdjust() == b.FontSizeAdjust();
    case CSSPropertyID::kFontStretch:
      return a.GetFontStretch() == b.GetFontStretch();
    case CSSPropertyID::kFontVariationSettings:
      return base::ValuesEquivalent(a.GetFontDescription().VariationSettings(),
                                    b.GetFontDescription().VariationSettings());
    case CSSPropertyID::kFontWeight:
      return a.GetFontWeight() == b.GetFontWeight();
    case CSSPropertyID::kGridTemplateColumns:
      return a.GridTemplateColumns() == b.GridTemplateColumns();
    case CSSPropertyID::kGridTemplateRows:
      return a.GridTemplateRows() == b.GridTemplateRows();
    case CSSPropertyID::kHeight:
      return a.Height() == b.Height();
    case CSSPropertyID::kLeft:
      return a.Left() == b.Left();
    case CSSPropertyID::kLetterSpacing:
      return a.LetterSpacing() == b.LetterSpacing();
    case CSSPropertyID::kLightingColor:
      return a.LightingColor() == b.LightingColor();
    case CSSPropertyID::kLineHeight:
      return a.SpecifiedLineHeight() == b.SpecifiedLineHeight();
    case CSSPropertyID::kTabSize:
      return a.GetTabSize() == b.GetTabSize();
    case CSSPropertyID::kListStyleImage:
      return base::ValuesEquivalent(a.ListStyleImage(), b.ListStyleImage());
    case CSSPropertyID::kMarginBottom:
      return a.MarginBottom() == b.MarginBottom();
    case CSSPropertyID::kMarginLeft:
      return a.MarginLeft() == b.MarginLeft();
    case CSSPropertyID::kMarginRight:
      return a.MarginRight() == b.MarginRight();
    case CSSPropertyID::kMarginTop:
      return a.MarginTop() == b.MarginTop();
    case CSSPropertyID::kMaxHeight:
      return a.MaxHeight() == b.MaxHeight();
    case CSSPropertyID::kMaxWidth:
      return a.MaxWidth() == b.MaxWidth();
    case CSSPropertyID::kMinHeight:
      return a.MinHeight() == b.MinHeight();
    case CSSPropertyID::kMinWidth:
      return a.MinWidth() == b.MinWidth();
    case CSSPropertyID::kObjectPosition:
      return a.ObjectPosition() == b.ObjectPosition();
    case CSSPropertyID::kObjectViewBox:
      return base::ValuesEquivalent(a.ObjectViewBox(), b.ObjectViewBox());
    case CSSPropertyID::kOffsetAnchor:
      return a.OffsetAnchor() == b.OffsetAnchor();
    case CSSPropertyID::kOffsetDistance:
      return a.OffsetDistance() == b.OffsetDistance();
    case CSSPropertyID::kOffsetPath:
      return base::ValuesEquivalent(a.OffsetPath(), b.OffsetPath());
    case CSSPropertyID::kOffsetPosition:
      return a.OffsetPosition() == b.OffsetPosition();
    case CSSPropertyID::kOffsetRotate:
      return a.OffsetRotate() == b.OffsetRotate();
    case CSSPropertyID::kOpacity:
      return a.Opacity() == b.Opacity();
    case CSSPropertyID::kOrder:
      return a.Order() == b.Order();
    case CSSPropertyID::kOrphans:
      return a.Orphans() == b.Orphans();
    case CSSPropertyID::kOutlineColor:
      return a.OutlineColor() == b.OutlineColor() &&
             a.InternalVisitedOutlineColor() == b.InternalVisitedOutlineColor();
    case CSSPropertyID::kOutlineOffset:
      return a.OutlineOffset() == b.OutlineOffset();
    case CSSPropertyID::kOutlineWidth:
      return a.OutlineWidth() == b.OutlineWidth();
    case CSSPropertyID::kPaddingBottom:
      return a.PaddingBottom() == b.PaddingBottom();
    case CSSPropertyID::kPaddingLeft:
      return a.PaddingLeft() == b.PaddingLeft();
    case CSSPropertyID::kPaddingRight:
      return a.PaddingRight() == b.PaddingRight();
    case CSSPropertyID::kPaddingTop:
      return a.PaddingTop() == b.PaddingTop();
    case CSSPropertyID::kRight:
      return a.Right() == b.Right();
    case CSSPropertyID::kShapeImageThreshold:
      return a.ShapeImageThreshold() == b.ShapeImageThreshold();
    case CSSPropertyID::kShapeMargin:
      return a.ShapeMargin() == b.ShapeMargin();
    case CSSPropertyID::kShapeOutside:
      return base::ValuesEquivalent(a.ShapeOutside(), b.ShapeOutside());
    case CSSPropertyID::kStopColor:
      return a.StopColor() == b.StopColor();
    case CSSPropertyID::kStopOpacity:
      return a.StopOpacity() == b.StopOpacity();
    case CSSPropertyID::kStroke:
      return a.StrokePaint().EqualTypeOrColor(b.StrokePaint()) &&
             a.InternalVisitedStrokePaint().EqualTypeOrColor(
                 b.InternalVisitedStrokePaint());
    case CSSPropertyID::kStrokeDasharray:
      return a.StrokeDashArray() == b.StrokeDashArray();
    case CSSPropertyID::kStrokeDashoffset:
      return a.StrokeDashOffset() == b.StrokeDashOffset();
    case CSSPropertyID::kStrokeMiterlimit:
      return a.StrokeMiterLimit() == b.StrokeMiterLimit();
    case CSSPropertyID::kStrokeOpacity:
      return a.StrokeOpacity() == b.StrokeOpacity();
    case CSSPropertyID::kStrokeWidth:
      return a.StrokeWidth() == b.StrokeWidth();
    case CSSPropertyID::kTextDecorationColor:
      return a.TextDecorationColor() == b.TextDecorationColor() &&
             a.InternalVisitedTextDecorationColor() ==
                 b.InternalVisitedTextDecorationColor();
    case CSSPropertyID::kTextDecorationSkipInk:
      return a.TextDecorationSkipInk() == b.TextDecorationSkipInk();
    case CSSPropertyID::kTextIndent:
      return a.TextIndent() == b.TextIndent();
    case CSSPropertyID::kTextShadow:
      return base::ValuesEquivalent(a.TextShadow(), b.TextShadow());
    case CSSPropertyID::kTextSizeAdjust:
      return a.GetTextSizeAdjust() == b.GetTextSizeAdjust();
    case CSSPropertyID::kTextUnderlineOffset:
      return a.TextUnderlineOffset() == b.TextUnderlineOffset();
    case CSSPropertyID::kTop:
      return a.Top() == b.Top();
    case CSSPropertyID::kVerticalAlign:
      return a.VerticalAlign() == b.VerticalAlign() &&
             (a.VerticalAlign() != EVerticalAlign::kLength ||
              a.GetVerticalAlignLength() == b.GetVerticalAlignLength());
    case CSSPropertyID::kVisibility:
      return a.Visibility() == b.Visibility();
    case CSSPropertyID::kWebkitBorderHorizontalSpacing:
      return a.HorizontalBorderSpacing() == b.HorizontalBorderSpacing();
    case CSSPropertyID::kWebkitBorderVerticalSpacing:
      return a.VerticalBorderSpacing() == b.VerticalBorderSpacing();
    case CSSPropertyID::kClipPath:
      return base::ValuesEquivalent(a.ClipPath(), b.ClipPath());
    case CSSPropertyID::kColumnCount:
      return a.ColumnCount() == b.ColumnCount();
    case CSSPropertyID::kColumnGap:
      return a.ColumnGap() == b.ColumnGap();
    case CSSPropertyID::kRowGap:
      return a.RowGap() == b.RowGap();
    case CSSPropertyID::kColumnRuleColor:
      return a.ColumnRuleColor() == b.ColumnRuleColor() &&
             a.InternalVisitedColumnRuleColor() ==
                 b.InternalVisitedColumnRuleColor();
    case CSSPropertyID::kColumnRuleWidth:
      return a.ColumnRuleWidth() == b.ColumnRuleWidth();
    case CSSPropertyID::kColumnWidth:
      return a.ColumnWidth() == b.ColumnWidth();
    case CSSPropertyID::kFilter:
      return a.Filter() == b.Filter();
    case CSSPropertyID::kBackdropFilter:
      return a.BackdropFilter() == b.BackdropFilter();
    case CSSPropertyID::kWebkitMaskBoxImageOutset:
      return a.MaskBoxImageOutset() == b.MaskBoxImageOutset();
    case CSSPropertyID::kWebkitMaskBoxImageSlice:
      return a.MaskBoxImageSlices() == b.MaskBoxImageSlices();
    case CSSPropertyID::kWebkitMaskBoxImageSource:
      return base::ValuesEquivalent(a.MaskBoxImageSource(),
                                    b.MaskBoxImageSource());
    case CSSPropertyID::kWebkitMaskBoxImageWidth:
      return a.MaskBoxImageWidth() == b.MaskBoxImageWidth();
    case CSSPropertyID::kWebkitMaskImage:
      return base::ValuesEquivalent(a.MaskImage(), b.MaskImage());
    case CSSPropertyID::kWebkitMaskPositionX:
      return FillLayersEqual<CSSPropertyID::kWebkitMaskPositionX>(
          a.MaskLayers(), b.MaskLayers());
    case CSSPropertyID::kWebkitMaskPositionY:
      return FillLayersEqual<CSSPropertyID::kWebkitMaskPositionY>(
          a.MaskLayers(), b.MaskLayers());
    case CSSPropertyID::kWebkitMaskSize:
      return FillLayersEqual<CSSPropertyID::kWebkitMaskSize>(a.MaskLayers(),
                                                             b.MaskLayers());
    case CSSPropertyID::kPerspective:
      return a.Perspective() == b.Perspective();
    case CSSPropertyID::kPerspectiveOrigin:
      return a.PerspectiveOrigin() == b.PerspectiveOrigin();
    case CSSPropertyID::kWebkitTextStrokeColor:
      return a.TextStrokeColor() == b.TextStrokeColor() &&
             a.InternalVisitedTextStrokeColor() ==
                 b.InternalVisitedTextStrokeColor();
    case CSSPropertyID::kTransform:
      return a.Transform() == b.Transform();
    case CSSPropertyID::kTranslate:
      return base::ValuesEquivalent<TransformOperation>(a.Translate(),
                                                        b.Translate());
    case CSSPropertyID::kRotate:
      return base::ValuesEquivalent<TransformOperation>(a.Rotate(), b.Rotate());
    case CSSPropertyID::kScale:
      return base::ValuesEquivalent<TransformOperation>(a.Scale(), b.Scale());
    case CSSPropertyID::kTransformOrigin:
      return a.GetTransformOrigin() == b.GetTransformOrigin();
    case CSSPropertyID::kWebkitPerspectiveOriginX:
      return a.PerspectiveOrigin().X() == b.PerspectiveOrigin().X();
    case CSSPropertyID::kWebkitPerspectiveOriginY:
      return a.PerspectiveOrigin().Y() == b.PerspectiveOrigin().Y();
    case CSSPropertyID::kWebkitTransformOriginX:
      return a.GetTransformOrigin().X() == b.GetTransformOrigin().X();
    case CSSPropertyID::kWebkitTransformOriginY:
      return a.GetTransformOrigin().Y() == b.GetTransformOrigin().Y();
    case CSSPropertyID::kWebkitTransformOriginZ:
      return a.GetTransformOrigin().Z() == b.GetTransformOrigin().Z();
    case CSSPropertyID::kWidows:
      return a.Widows() == b.Widows();
    case CSSPropertyID::kWidth:
      return a.Width() == b.Width();
    case CSSPropertyID::kWordSpacing:
      return a.WordSpacing() == b.WordSpacing();
    case CSSPropertyID::kD:
      return base::ValuesEquivalent(a.D(), b.D());
    case CSSPropertyID::kCx:
      return a.Cx() == b.Cx();
    case CSSPropertyID::kCy:
      return a.Cy() == b.Cy();
    case CSSPropertyID::kX:
      return a.X() == b.X();
    case CSSPropertyID::kY:
      return a.Y() == b.Y();
    case CSSPropertyID::kR:
      return a.R() == b.R();
    case CSSPropertyID::kRx:
      return a.Rx() == b.Rx();
    case CSSPropertyID::kRy:
      return a.Ry() == b.Ry();
    case CSSPropertyID::kZIndex:
      return a.HasAutoZIndex() == b.HasAutoZIndex() &&
             (a.HasAutoZIndex() || a.ZIndex() == b.ZIndex());
    case CSSPropertyID::kContainIntrinsicWidth:
      return a.ContainIntrinsicWidth() == b.ContainIntrinsicWidth();
    case CSSPropertyID::kContainIntrinsicHeight:
      return a.ContainIntrinsicHeight() == b.ContainIntrinsicHeight();
    case CSSPropertyID::kAspectRatio:
      return a.AspectRatio() == b.AspectRatio();
    case CSSPropertyID::kMathDepth:
      return a.MathDepth() == b.MathDepth();
    case CSSPropertyID::kAccentColor:
      return a.AccentColor() == b.AccentColor();
    case CSSPropertyID::kTextEmphasisColor:
      return a.TextEmphasisColor() == b.TextEmphasisColor();
    default:
      NOTREACHED();
      return true;
  }
}

}  // namespace blink
