// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/css_property_equality.h"

#include "base/memory/values_equivalent.h"
#include "third_party/blink/renderer/core/animation/css/css_animations.h"
#include "third_party/blink/renderer/core/animation/property_handle.h"
#include "third_party/blink/renderer/core/css/css_property_names.h"
#include "third_party/blink/renderer/core/css/css_value.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/core/style/shadow_list.h"

// TODO(ikilpatrick): generate this file.

namespace blink {

namespace {

template <CSSPropertyID property>
bool CounterRulesEqual(const CounterDirectiveMap* a_map,
                       const CounterDirectiveMap* b_map) {
  if (a_map == b_map) {
    return true;
  }
  if (!a_map || !b_map) {
    return false;
  }

  return base::ranges::equal(*a_map, *b_map, [](const auto& a, const auto& b) {
    switch (property) {
      case CSSPropertyID::kCounterIncrement:
        if (a.value.IsIncrement() != b.value.IsIncrement()) {
          return false;
        }
        if (a.value.IsIncrement() &&
            a.value.IncrementValue() != b.value.IncrementValue()) {
          return false;
        }
        break;
      case CSSPropertyID::kCounterReset:
        if (a.value.IsReset() != b.value.IsReset()) {
          return false;
        }
        if (a.value.IsReset() && a.value.ResetValue() != b.value.ResetValue()) {
          return false;
        }
        break;
      case CSSPropertyID::kCounterSet:
        if (a.value.IsSet() != b.value.IsSet()) {
          return false;
        }
        if (a.value.IsSet() && a.value.SetValue() != b.value.SetValue()) {
          return false;
        }
        break;
      default:
        NOTREACHED_IN_MIGRATION();
        return true;
    }
    return true;
  });
}

template <CSSPropertyID property>
bool FillLayersEqual(const FillLayer& a_layers, const FillLayer& b_layers) {
  const FillLayer* a_layer = &a_layers;
  const FillLayer* b_layer = &b_layers;
  while (a_layer && b_layer) {
    switch (property) {
      case CSSPropertyID::kBackgroundAttachment:
        if (a_layer->Attachment() != b_layer->Attachment()) {
          return false;
        }
        break;
      case CSSPropertyID::kBackgroundBlendMode:
        if (a_layer->GetBlendMode() != b_layer->GetBlendMode()) {
          return false;
        }
        break;
      case CSSPropertyID::kBackgroundClip:
      case CSSPropertyID::kMaskClip:
        if (a_layer->Clip() != b_layer->Clip()) {
          return false;
        }
        break;
      case CSSPropertyID::kMaskComposite:
        if (a_layer->CompositingOperator() != b_layer->CompositingOperator()) {
          return false;
        }
        break;
      case CSSPropertyID::kBackgroundOrigin:
      case CSSPropertyID::kMaskOrigin:
        if (a_layer->Origin() != b_layer->Origin()) {
          return false;
        }
        break;
      case CSSPropertyID::kMaskMode:
        if (a_layer->MaskMode() != b_layer->MaskMode()) {
          return false;
        }
        break;
      case CSSPropertyID::kBackgroundPositionX:
      case CSSPropertyID::kWebkitMaskPositionX:
        if (a_layer->PositionX() != b_layer->PositionX()) {
          return false;
        }
        if (a_layer->BackgroundXOrigin() != b_layer->BackgroundXOrigin()) {
          return false;
        }
        break;
      case CSSPropertyID::kBackgroundPositionY:
      case CSSPropertyID::kWebkitMaskPositionY:
        if (a_layer->PositionY() != b_layer->PositionY()) {
          return false;
        }
        if (a_layer->BackgroundYOrigin() != b_layer->BackgroundYOrigin()) {
          return false;
        }
        break;
      case CSSPropertyID::kBackgroundRepeat:
      case CSSPropertyID::kMaskRepeat:
        if (a_layer->Repeat() != b_layer->Repeat()) {
          return false;
        }
        break;
      case CSSPropertyID::kBackgroundSize:
      case CSSPropertyID::kMaskSize:
        if (!(a_layer->SizeLength() == b_layer->SizeLength())) {
          return false;
        }
        break;
      case CSSPropertyID::kBackgroundImage:
      case CSSPropertyID::kMaskImage:
        if (!base::ValuesEquivalent(a_layer->GetImage(), b_layer->GetImage())) {
          return false;
        }
        break;
      default:
        NOTREACHED_IN_MIGRATION();
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
    case CSSPropertyID::kAlignContent:
      return a.AlignContent() == b.AlignContent();
    case CSSPropertyID::kAlignItems:
      return a.AlignItems() == b.AlignItems();
    case CSSPropertyID::kAlignSelf:
      return a.AlignSelf() == b.AlignSelf();
    case CSSPropertyID::kAlignmentBaseline:
      return a.AlignmentBaseline() == b.AlignmentBaseline();
    case CSSPropertyID::kPositionAnchor:
      return base::ValuesEquivalent(a.PositionAnchor(), b.PositionAnchor());
    case CSSPropertyID::kAnchorName:
      return base::ValuesEquivalent(a.AnchorName(), b.AnchorName());
    case CSSPropertyID::kAnchorScope:
      return a.AnchorScope() == b.AnchorScope();
    case CSSPropertyID::kAppearance:
      return a.Appearance() == b.Appearance();
    case CSSPropertyID::kAppRegion:
      return a.DraggableRegionMode() == b.DraggableRegionMode();
    case CSSPropertyID::kBackfaceVisibility:
      return a.BackfaceVisibility() == b.BackfaceVisibility();
    case CSSPropertyID::kBackgroundAttachment:
      return FillLayersEqual<CSSPropertyID::kBackgroundAttachment>(
          a.BackgroundLayers(), b.BackgroundLayers());
    case CSSPropertyID::kBackgroundBlendMode:
      return FillLayersEqual<CSSPropertyID::kBackgroundBlendMode>(
          a.BackgroundLayers(), b.BackgroundLayers());
    case CSSPropertyID::kBackgroundClip:
      return FillLayersEqual<CSSPropertyID::kBackgroundClip>(
          a.BackgroundLayers(), b.BackgroundLayers());
    case CSSPropertyID::kBackgroundColor:
      return a.BackgroundColor() == b.BackgroundColor() &&
             a.InternalVisitedBackgroundColor() ==
                 b.InternalVisitedBackgroundColor();
    case CSSPropertyID::kBackgroundImage:
      return FillLayersEqual<CSSPropertyID::kBackgroundImage>(
          a.BackgroundLayers(), b.BackgroundLayers());
    case CSSPropertyID::kBackgroundOrigin:
      return FillLayersEqual<CSSPropertyID::kBackgroundOrigin>(
          a.BackgroundLayers(), b.BackgroundLayers());
    case CSSPropertyID::kBackgroundPositionX:
      return FillLayersEqual<CSSPropertyID::kBackgroundPositionX>(
          a.BackgroundLayers(), b.BackgroundLayers());
    case CSSPropertyID::kBackgroundPositionY:
      return FillLayersEqual<CSSPropertyID::kBackgroundPositionY>(
          a.BackgroundLayers(), b.BackgroundLayers());
    case CSSPropertyID::kBackgroundRepeat:
      return FillLayersEqual<CSSPropertyID::kBackgroundRepeat>(
          a.BackgroundLayers(), b.BackgroundLayers());
    case CSSPropertyID::kBackgroundSize:
      return FillLayersEqual<CSSPropertyID::kBackgroundSize>(
          a.BackgroundLayers(), b.BackgroundLayers());
    case CSSPropertyID::kBaselineShift:
      return a.BaselineShift() == b.BaselineShift();
    case CSSPropertyID::kBaselineSource:
      return a.BaselineSource() == b.BaselineSource();
    case CSSPropertyID::kBorderBottomColor:
      return a.BorderBottomColor() == b.BorderBottomColor() &&
             a.InternalVisitedBorderBottomColor() ==
                 b.InternalVisitedBorderBottomColor();
    case CSSPropertyID::kBorderBottomLeftRadius:
      return a.BorderBottomLeftRadius() == b.BorderBottomLeftRadius();
    case CSSPropertyID::kBorderBottomRightRadius:
      return a.BorderBottomRightRadius() == b.BorderBottomRightRadius();
    case CSSPropertyID::kBorderBottomStyle:
      return a.BorderBottomStyle() == b.BorderBottomStyle();
    case CSSPropertyID::kBorderBottomWidth:
      return a.BorderBottomWidth() == b.BorderBottomWidth();
    case CSSPropertyID::kBorderCollapse:
      return a.BorderCollapse() == b.BorderCollapse();
    case CSSPropertyID::kBorderImageOutset:
      return a.BorderImageOutset() == b.BorderImageOutset();
    case CSSPropertyID::kBorderImageRepeat:
      return (a.BorderImage().HorizontalRule() ==
              b.BorderImage().HorizontalRule()) &&
             (a.BorderImage().VerticalRule() == b.BorderImage().VerticalRule());
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
    case CSSPropertyID::kBorderLeftStyle:
      return a.BorderLeftStyle() == b.BorderLeftStyle();
    case CSSPropertyID::kBorderLeftWidth:
      return a.BorderLeftWidth() == b.BorderLeftWidth();
    case CSSPropertyID::kBorderRightColor:
      return a.BorderRightColor() == b.BorderRightColor() &&
             a.InternalVisitedBorderRightColor() ==
                 b.InternalVisitedBorderRightColor();
    case CSSPropertyID::kBorderRightStyle:
      return a.BorderRightStyle() == b.BorderRightStyle();
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
    case CSSPropertyID::kBorderTopStyle:
      return a.BorderTopStyle() == b.BorderTopStyle();
    case CSSPropertyID::kBorderTopWidth:
      return a.BorderTopWidth() == b.BorderTopWidth();
    case CSSPropertyID::kBottom:
      return a.Bottom() == b.Bottom();
    case CSSPropertyID::kBoxDecorationBreak:
      return a.BoxDecorationBreak() == b.BoxDecorationBreak();
    case CSSPropertyID::kBoxShadow:
      return base::ValuesEquivalent(a.BoxShadow(), b.BoxShadow());
    case CSSPropertyID::kBoxSizing:
      return a.BoxSizing() == b.BoxSizing();
    case CSSPropertyID::kBreakAfter:
      return a.BreakAfter() == b.BreakAfter();
    case CSSPropertyID::kBreakBefore:
      return a.BreakBefore() == b.BreakBefore();
    case CSSPropertyID::kBreakInside:
      return a.BreakInside() == b.BreakInside();
    case CSSPropertyID::kBufferedRendering:
      return a.BufferedRendering() == b.BufferedRendering();
    case CSSPropertyID::kCaptionSide:
      return a.CaptionSide() == b.CaptionSide();
    case CSSPropertyID::kCaretAnimation:
      return a.CaretAnimation() == b.CaretAnimation();
    case CSSPropertyID::kCaretColor:
      return a.CaretColor() == b.CaretColor() &&
             a.InternalVisitedCaretColor() == b.InternalVisitedCaretColor();
    case CSSPropertyID::kClear:
      return a.Clear() == b.Clear();
    case CSSPropertyID::kClip:
      return a.Clip() == b.Clip();
    case CSSPropertyID::kClipRule:
      return a.ClipRule() == b.ClipRule();
    case CSSPropertyID::kColor:
      return a.Color() == b.Color() &&
             a.InternalVisitedColor() == b.InternalVisitedColor();
    case CSSPropertyID::kColorInterpolation:
      return a.ColorInterpolation() == b.ColorInterpolation();
    case CSSPropertyID::kColorInterpolationFilters:
      return a.ColorInterpolationFilters() == b.ColorInterpolationFilters();
    case CSSPropertyID::kColorRendering:
      return a.ColorRendering() == b.ColorRendering();
    case CSSPropertyID::kColorScheme:
      return a.ColorScheme() == b.ColorScheme();
    case CSSPropertyID::kColumnFill:
      return a.GetColumnFill() == b.GetColumnFill();
    case CSSPropertyID::kColumnRuleStyle:
      return a.ColumnRuleStyle() == b.ColumnRuleStyle();
    case CSSPropertyID::kColumnSpan:
      return a.GetColumnSpan() == b.GetColumnSpan();
    case CSSPropertyID::kContent:
      return base::ValuesEquivalent(a.GetContentData(), b.GetContentData());
    case CSSPropertyID::kCounterIncrement:
      return CounterRulesEqual<CSSPropertyID::kCounterIncrement>(
          a.GetCounterDirectives(), b.GetCounterDirectives());
    case CSSPropertyID::kCounterReset:
      return CounterRulesEqual<CSSPropertyID::kCounterReset>(
          a.GetCounterDirectives(), b.GetCounterDirectives());
    case CSSPropertyID::kCounterSet:
      return CounterRulesEqual<CSSPropertyID::kCounterSet>(
          a.GetCounterDirectives(), b.GetCounterDirectives());
    case CSSPropertyID::kCursor:
      return a.Cursor() == b.Cursor();
    case CSSPropertyID::kDisplay:
      return a.Display() == b.Display();
    case CSSPropertyID::kContentVisibility:
      return a.ContentVisibility() == b.ContentVisibility();
    case CSSPropertyID::kDominantBaseline:
      return a.DominantBaseline() == b.DominantBaseline();
    case CSSPropertyID::kDynamicRangeLimit:
      return a.GetDynamicRangeLimit() == b.GetDynamicRangeLimit();
    case CSSPropertyID::kEmptyCells:
      return a.EmptyCells() == b.EmptyCells();
    case CSSPropertyID::kFill:
      return a.FillPaint().EqualTypeOrColor(b.FillPaint()) &&
             a.InternalVisitedFillPaint().EqualTypeOrColor(
                 b.InternalVisitedFillPaint());
    case CSSPropertyID::kFillRule:
      return a.FillRule() == b.FillRule();
    case CSSPropertyID::kFlexDirection:
      return a.FlexDirection() == b.FlexDirection();
    case CSSPropertyID::kFillOpacity:
      return a.FillOpacity() == b.FillOpacity();
    case CSSPropertyID::kFlexBasis:
      return a.FlexBasis() == b.FlexBasis();
    case CSSPropertyID::kFlexGrow:
      return a.FlexGrow() == b.FlexGrow();
    case CSSPropertyID::kFlexShrink:
      return a.FlexShrink() == b.FlexShrink();
    case CSSPropertyID::kFlexWrap:
      return a.FlexWrap() == b.FlexWrap();
    case CSSPropertyID::kFloat:
      return a.Floating() == b.Floating();
    case CSSPropertyID::kFloodColor:
      return a.FloodColor() == b.FloodColor();
    case CSSPropertyID::kFloodOpacity:
      return a.FloodOpacity() == b.FloodOpacity();
    case CSSPropertyID::kFontFamily:
      return a.GetFontDescription().Family() == b.GetFontDescription().Family();
    case CSSPropertyID::kFontKerning:
      return a.GetFontDescription().GetKerning() ==
             b.GetFontDescription().GetKerning();
    case CSSPropertyID::kFontOpticalSizing:
      return a.GetFontDescription().FontOpticalSizing() ==
             b.GetFontDescription().FontOpticalSizing();
    case CSSPropertyID::kFontPalette:
      return base::ValuesEquivalent(a.GetFontPalette(), b.GetFontPalette());
    case CSSPropertyID::kFontFeatureSettings:
      return a.GetFontDescription().FeatureSettings() ==
             b.GetFontDescription().FeatureSettings();
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
    case CSSPropertyID::kFontStyle:
      return a.GetFontStyle() == b.GetFontStyle();
    case CSSPropertyID::kFontSynthesisSmallCaps:
      return a.GetFontDescription().GetFontSynthesisSmallCaps() ==
             b.GetFontDescription().GetFontSynthesisSmallCaps();
    case CSSPropertyID::kFontSynthesisStyle:
      return a.GetFontDescription().GetFontSynthesisStyle() ==
             b.GetFontDescription().GetFontSynthesisStyle();
    case CSSPropertyID::kFontSynthesisWeight:
      return a.GetFontDescription().GetFontSynthesisWeight() ==
             b.GetFontDescription().GetFontSynthesisWeight();
    case CSSPropertyID::kFontVariantAlternates:
      return a.GetFontDescription().GetFontVariantAlternates() ==
             b.GetFontDescription().GetFontVariantAlternates();
    case CSSPropertyID::kFontVariantCaps:
      return a.GetFontDescription().VariantCaps() ==
             b.GetFontDescription().VariantCaps();
    case CSSPropertyID::kFontVariantEastAsian:
      return a.GetFontDescription().VariantEastAsian() ==
             b.GetFontDescription().VariantEastAsian();
    case CSSPropertyID::kFontVariantEmoji:
      return a.GetFontDescription().VariantEmoji() ==
             b.GetFontDescription().VariantEmoji();
    case CSSPropertyID::kFontVariantLigatures:
      return a.GetFontDescription().GetVariantLigatures() ==
             b.GetFontDescription().GetVariantLigatures();
    case CSSPropertyID::kFontVariantNumeric:
      return a.GetFontDescription().VariantNumeric() ==
             b.GetFontDescription().VariantNumeric();
    case CSSPropertyID::kFontVariantPosition:
      return a.GetFontDescription().VariantPosition() ==
             b.GetFontDescription().VariantPosition();
    case CSSPropertyID::kFontVariationSettings:
      return base::ValuesEquivalent(a.GetFontDescription().VariationSettings(),
                                    b.GetFontDescription().VariationSettings());
    case CSSPropertyID::kFontWeight:
      return a.GetFontWeight() == b.GetFontWeight();
    case CSSPropertyID::kForcedColorAdjust:
      return a.ForcedColorAdjust() == b.ForcedColorAdjust();
    case CSSPropertyID::kFieldSizing:
      return a.FieldSizing() == b.FieldSizing();
    case CSSPropertyID::kGridAutoColumns:
      return a.GridAutoColumns() == b.GridAutoColumns();
    case CSSPropertyID::kGridAutoFlow:
      return a.GetGridAutoFlow() == b.GetGridAutoFlow();
    case CSSPropertyID::kGridAutoRows:
      return a.GridAutoRows() == b.GridAutoRows();
    case CSSPropertyID::kGridColumnEnd:
      return a.GridColumnEnd() == b.GridColumnEnd();
    case CSSPropertyID::kGridColumnStart:
      return a.GridColumnStart() == b.GridColumnStart();
    case CSSPropertyID::kGridRowEnd:
      return a.GridRowEnd() == b.GridRowEnd();
    case CSSPropertyID::kGridRowStart:
      return a.GridRowStart() == b.GridRowStart();
    case CSSPropertyID::kGridTemplateAreas:
      return base::ValuesEquivalent(a.GridTemplateAreas(),
                                    b.GridTemplateAreas());
    case CSSPropertyID::kGridTemplateColumns:
      return a.GridTemplateColumns() == b.GridTemplateColumns();
    case CSSPropertyID::kGridTemplateRows:
      return a.GridTemplateRows() == b.GridTemplateRows();
    case CSSPropertyID::kHeight:
      return a.Height() == b.Height();
    case CSSPropertyID::kPopoverShowDelay:
      return a.PopoverShowDelay() == b.PopoverShowDelay();
    case CSSPropertyID::kPopoverHideDelay:
      return a.PopoverHideDelay() == b.PopoverHideDelay();
    case CSSPropertyID::kHyphenateCharacter:
      return a.HyphenationString() == b.HyphenationString();
    case CSSPropertyID::kHyphenateLimitChars:
      return a.HyphenateLimitChars() == b.HyphenateLimitChars();
    case CSSPropertyID::kHyphens:
      return a.GetHyphens() == b.GetHyphens();
    case CSSPropertyID::kImageOrientation:
      return a.ImageOrientation() == b.ImageOrientation();
    case CSSPropertyID::kImageRendering:
      return a.ImageRendering() == b.ImageRendering();
    case CSSPropertyID::kInitialLetter:
      return a.InitialLetter() == b.InitialLetter();
    case CSSPropertyID::kPositionArea:
    case CSSPropertyID::kInsetArea:
      return a.GetPositionArea() == b.GetPositionArea();
    case CSSPropertyID::kInterpolateSize:
      return a.InterpolateSize() == b.InterpolateSize();
    case CSSPropertyID::kIsolation:
      return a.Isolation() == b.Isolation();
    case CSSPropertyID::kJustifyContent:
      return a.JustifyContent() == b.JustifyContent();
    case CSSPropertyID::kJustifyItems:
      return a.JustifyItems() == b.JustifyItems();
    case CSSPropertyID::kJustifySelf:
      return a.JustifySelf() == b.JustifySelf();
    case CSSPropertyID::kLeft:
      return a.Left() == b.Left();
    case CSSPropertyID::kLetterSpacing:
      return a.LetterSpacing() == b.LetterSpacing();
    case CSSPropertyID::kLightingColor:
      return a.LightingColor() == b.LightingColor();
    case CSSPropertyID::kLineBreak:
      return a.GetLineBreak() == b.GetLineBreak();
    case CSSPropertyID::kLineClamp:
      return a.StandardLineClamp() == b.StandardLineClamp();
    case CSSPropertyID::kLineHeight:
      return a.SpecifiedLineHeight() == b.SpecifiedLineHeight();
    case CSSPropertyID::kTabSize:
      return a.GetTabSize() == b.GetTabSize();
    case CSSPropertyID::kListStyleImage:
      return base::ValuesEquivalent(a.ListStyleImage(), b.ListStyleImage());
    case CSSPropertyID::kListStylePosition:
      return a.ListStylePosition() == b.ListStylePosition();
    case CSSPropertyID::kListStyleType:
      return a.ListStyleType() == b.ListStyleType();
    case CSSPropertyID::kMarginBottom:
      return a.MarginBottom() == b.MarginBottom();
    case CSSPropertyID::kMarginLeft:
      return a.MarginLeft() == b.MarginLeft();
    case CSSPropertyID::kMarginRight:
      return a.MarginRight() == b.MarginRight();
    case CSSPropertyID::kMarginTop:
      return a.MarginTop() == b.MarginTop();
    case CSSPropertyID::kMarkerEnd:
      return a.MarkerEndResource() == b.MarkerEndResource();
    case CSSPropertyID::kMarkerMid:
      return a.MarkerMidResource() == b.MarkerMidResource();
    case CSSPropertyID::kMarkerStart:
      return a.MarkerStartResource() == b.MarkerStartResource();
    case CSSPropertyID::kMaskType:
      return a.MaskType() == b.MaskType();
    case CSSPropertyID::kMasonrySlack:
      return a.MasonrySlack() == b.MasonrySlack();
    case CSSPropertyID::kMasonryTemplateTracks:
      return a.MasonryTemplateTracks() == b.MasonryTemplateTracks();
    case CSSPropertyID::kMasonryTrackEnd:
      return a.MasonryTrackEnd() == b.MasonryTrackEnd();
    case CSSPropertyID::kMasonryTrackStart:
      return a.MasonryTrackStart() == b.MasonryTrackStart();
    case CSSPropertyID::kMathShift:
      return a.MathShift() == b.MathShift();
    case CSSPropertyID::kMathStyle:
      return a.MathStyle() == b.MathStyle();
    case CSSPropertyID::kMaxHeight:
      return a.MaxHeight() == b.MaxHeight();
    case CSSPropertyID::kMaxWidth:
      return a.MaxWidth() == b.MaxWidth();
    case CSSPropertyID::kMinHeight:
      return a.MinHeight() == b.MinHeight();
    case CSSPropertyID::kMinWidth:
      return a.MinWidth() == b.MinWidth();
    case CSSPropertyID::kMixBlendMode:
      return a.GetBlendMode() == b.GetBlendMode();
    case CSSPropertyID::kObjectFit:
      return a.GetObjectFit() == b.GetObjectFit();
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
    case CSSPropertyID::kOriginTrialTestProperty:
      return a.OriginTrialTestProperty() == b.OriginTrialTestProperty();
    case CSSPropertyID::kOrphans:
      return a.Orphans() == b.Orphans();
    case CSSPropertyID::kOutlineColor:
      return a.OutlineColor() == b.OutlineColor() &&
             a.InternalVisitedOutlineColor() == b.InternalVisitedOutlineColor();
    case CSSPropertyID::kOutlineOffset:
      return a.OutlineOffset() == b.OutlineOffset();
    case CSSPropertyID::kOutlineStyle:
      return a.OutlineStyle() == b.OutlineStyle();
    case CSSPropertyID::kOutlineWidth:
      return a.OutlineWidth() == b.OutlineWidth();
    case CSSPropertyID::kOverflowAnchor:
      return a.OverflowAnchor() == b.OverflowAnchor();
    case CSSPropertyID::kOverflowClipMargin:
      return a.OverflowClipMargin() == b.OverflowClipMargin();
    case CSSPropertyID::kOverflowWrap:
      return a.OverflowWrap() == b.OverflowWrap();
    case CSSPropertyID::kOverflowX:
      return a.OverflowX() == b.OverflowX();
    case CSSPropertyID::kOverflowY:
      return a.OverflowY() == b.OverflowY();
    case CSSPropertyID::kOverscrollBehaviorX:
      return a.OverscrollBehaviorX() == b.OverscrollBehaviorX();
    case CSSPropertyID::kOverscrollBehaviorY:
      return a.OverscrollBehaviorY() == b.OverscrollBehaviorY();
    case CSSPropertyID::kPaddingBottom:
      return a.PaddingBottom() == b.PaddingBottom();
    case CSSPropertyID::kPaddingLeft:
      return a.PaddingLeft() == b.PaddingLeft();
    case CSSPropertyID::kPaddingRight:
      return a.PaddingRight() == b.PaddingRight();
    case CSSPropertyID::kPaddingTop:
      return a.PaddingTop() == b.PaddingTop();
    case CSSPropertyID::kPage:
      return a.Page() == b.Page();
    case CSSPropertyID::kPageOrientation:
      return a.GetPageOrientation() == b.GetPageOrientation();
    case CSSPropertyID::kPaintOrder:
      return a.PaintOrder() == b.PaintOrder();
    case CSSPropertyID::kPointerEvents:
      return a.PointerEvents() == b.PointerEvents();
    case CSSPropertyID::kPosition:
      return a.GetPosition() == b.GetPosition();
    case CSSPropertyID::kQuotes:
      return a.Quotes() == b.Quotes();
    case CSSPropertyID::kReadingFlow:
      return a.ReadingFlow() == b.ReadingFlow();
    case CSSPropertyID::kResize:
      return a.Resize() == b.Resize();
    case CSSPropertyID::kRight:
      return a.Right() == b.Right();
    case CSSPropertyID::kRubyAlign:
      return a.RubyAlign() == b.RubyAlign();
    case CSSPropertyID::kRubyPosition:
      return a.GetRubyPosition() == b.GetRubyPosition();
    case CSSPropertyID::kScrollMarkerGroup:
      return a.ScrollMarkerGroup() == b.ScrollMarkerGroup();
    case CSSPropertyID::kScrollbarColor:
      return a.ScrollbarColor() == b.ScrollbarColor();
    case CSSPropertyID::kScrollbarGutter:
      return a.ScrollbarGutter() == b.ScrollbarGutter();
    case CSSPropertyID::kScrollbarWidth:
      return a.ScrollbarWidth() == b.ScrollbarWidth();
    case CSSPropertyID::kScrollBehavior:
      return a.GetScrollBehavior() == b.GetScrollBehavior();
    case CSSPropertyID::kScrollMarginBottom:
      return a.ScrollMarginBottom() == b.ScrollMarginBottom();
    case CSSPropertyID::kScrollMarginLeft:
      return a.ScrollMarginLeft() == b.ScrollMarginLeft();
    case CSSPropertyID::kScrollMarginRight:
      return a.ScrollMarginRight() == b.ScrollMarginRight();
    case CSSPropertyID::kScrollMarginTop:
      return a.ScrollMarginTop() == b.ScrollMarginTop();
    case CSSPropertyID::kScrollPaddingBottom:
      return a.ScrollPaddingBottom() == b.ScrollPaddingBottom();
    case CSSPropertyID::kScrollPaddingLeft:
      return a.ScrollPaddingLeft() == b.ScrollPaddingLeft();
    case CSSPropertyID::kScrollPaddingRight:
      return a.ScrollPaddingRight() == b.ScrollPaddingRight();
    case CSSPropertyID::kScrollPaddingTop:
      return a.ScrollPaddingTop() == b.ScrollPaddingTop();
    case CSSPropertyID::kScrollSnapAlign:
      return a.GetScrollSnapAlign() == b.GetScrollSnapAlign();
    case CSSPropertyID::kScrollSnapStop:
      return a.ScrollSnapStop() == b.ScrollSnapStop();
    case CSSPropertyID::kScrollSnapType:
      return a.GetScrollSnapType() == b.GetScrollSnapType();
    case CSSPropertyID::kScrollStartTarget:
      return a.ScrollStartTarget() == b.ScrollStartTarget();
    case CSSPropertyID::kScrollStartX:
      return a.ScrollStartX() == b.ScrollStartX();
    case CSSPropertyID::kScrollStartY:
      return a.ScrollStartY() == b.ScrollStartY();
    case CSSPropertyID::kShapeImageThreshold:
      return a.ShapeImageThreshold() == b.ShapeImageThreshold();
    case CSSPropertyID::kShapeMargin:
      return a.ShapeMargin() == b.ShapeMargin();
    case CSSPropertyID::kShapeOutside:
      return base::ValuesEquivalent(a.ShapeOutside(), b.ShapeOutside());
    case CSSPropertyID::kShapeRendering:
      return a.ShapeRendering() == b.ShapeRendering();
    case CSSPropertyID::kSizeAdjust:
      return a.GetFontDescription().SizeAdjust() ==
             b.GetFontDescription().SizeAdjust();
    case CSSPropertyID::kSpeak:
      return a.Speak() == b.Speak();
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
    case CSSPropertyID::kStrokeLinecap:
      return a.CapStyle() == b.CapStyle();
    case CSSPropertyID::kStrokeLinejoin:
      return a.JoinStyle() == b.JoinStyle();
    case CSSPropertyID::kStrokeMiterlimit:
      return a.StrokeMiterLimit() == b.StrokeMiterLimit();
    case CSSPropertyID::kStrokeOpacity:
      return a.StrokeOpacity() == b.StrokeOpacity();
    case CSSPropertyID::kStrokeWidth:
      return a.StrokeWidth() == b.StrokeWidth();
    case CSSPropertyID::kTableLayout:
      return a.TableLayout() == b.TableLayout();
    case CSSPropertyID::kTextAlign:
      return a.GetTextAlign() == b.GetTextAlign();
    case CSSPropertyID::kTextAlignLast:
      return a.TextAlignLast() == b.TextAlignLast();
    case CSSPropertyID::kTextAnchor:
      return a.TextAnchor() == b.TextAnchor();
    case CSSPropertyID::kTextAutospace:
      return a.TextAutospace() == b.TextAutospace();
    case blink::CSSPropertyID::kTextBoxEdge:
      return a.GetTextBoxEdge() == b.GetTextBoxEdge();
    case blink::CSSPropertyID::kTextBoxTrim:
      return a.TextBoxTrim() == b.TextBoxTrim();
    case CSSPropertyID::kTextDecorationColor:
      return a.TextDecorationColor() == b.TextDecorationColor() &&
             a.InternalVisitedTextDecorationColor() ==
                 b.InternalVisitedTextDecorationColor();
    case CSSPropertyID::kTextDecorationLine:
      return a.GetTextDecorationLine() == b.GetTextDecorationLine();
    case CSSPropertyID::kTextDecorationSkipInk:
      return a.TextDecorationSkipInk() == b.TextDecorationSkipInk();
    case CSSPropertyID::kTextDecorationStyle:
      return a.TextDecorationStyle() == b.TextDecorationStyle();
    case CSSPropertyID::kTextDecorationThickness:
      return a.GetTextDecorationThickness() == b.GetTextDecorationThickness();
    case CSSPropertyID::kTextEmphasisPosition:
      return a.GetTextEmphasisPosition() == b.GetTextEmphasisPosition();
    case CSSPropertyID::kTextEmphasisStyle:
      return (a.GetTextEmphasisFill() == b.GetTextEmphasisFill()) &&
             (a.GetTextEmphasisMark() == b.GetTextEmphasisMark()) &&
             (a.TextEmphasisCustomMark() == b.TextEmphasisCustomMark());
    case CSSPropertyID::kTextIndent:
      return a.TextIndent() == b.TextIndent();
    case CSSPropertyID::kTextOverflow:
      return a.TextOverflow() == b.TextOverflow();
    case CSSPropertyID::kTextRendering:
      return a.GetFontDescription().TextRendering() ==
             b.GetFontDescription().TextRendering();
    case CSSPropertyID::kTextShadow:
      return base::ValuesEquivalent(a.TextShadow(), b.TextShadow());
    case CSSPropertyID::kTextSizeAdjust:
      return a.GetTextSizeAdjust() == b.GetTextSizeAdjust();
    case CSSPropertyID::kTextSpacingTrim:
      return a.GetFontDescription().GetTextSpacingTrim() ==
             b.GetFontDescription().GetTextSpacingTrim();
    case CSSPropertyID::kTextTransform:
      return a.TextTransform() == b.TextTransform();
    case CSSPropertyID::kTextUnderlineOffset:
      return a.TextUnderlineOffset() == b.TextUnderlineOffset();
    case CSSPropertyID::kTextUnderlinePosition:
      return a.GetTextUnderlinePosition() == b.GetTextUnderlinePosition();
    case CSSPropertyID::kTextWrapMode:
      return a.GetTextWrapMode() == b.GetTextWrapMode();
    case CSSPropertyID::kTextWrapStyle:
      return a.GetTextWrapStyle() == b.GetTextWrapStyle();
    case CSSPropertyID::kTop:
      return a.Top() == b.Top();
    case CSSPropertyID::kOverlay:
      return a.Overlay() == b.Overlay();
    case CSSPropertyID::kTouchAction:
      return a.GetTouchAction() == b.GetTouchAction();
    case CSSPropertyID::kTransformBox:
      return a.TransformBox() == b.TransformBox();
    case CSSPropertyID::kTransformStyle:
      return a.TransformStyle3D() == b.TransformStyle3D();
    case CSSPropertyID::kUserSelect:
      return a.UserSelect() == b.UserSelect();
    case CSSPropertyID::kVectorEffect:
      return a.VectorEffect() == b.VectorEffect();
    case CSSPropertyID::kVerticalAlign:
      return a.VerticalAlign() == b.VerticalAlign() &&
             (a.VerticalAlign() != EVerticalAlign::kLength ||
              a.GetVerticalAlignLength() == b.GetVerticalAlignLength());
    case CSSPropertyID::kViewTransitionClass:
      return a.ViewTransitionClass() == b.ViewTransitionClass();
    case CSSPropertyID::kViewTransitionGroup:
      return a.ViewTransitionGroup() == b.ViewTransitionGroup();
    case CSSPropertyID::kViewTransitionName:
      return a.ViewTransitionName() == b.ViewTransitionName();
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
    case CSSPropertyID::kWebkitFontSmoothing:
      return a.GetFontDescription().FontSmoothing() ==
             b.GetFontDescription().FontSmoothing();
    case CSSPropertyID::kWebkitLineClamp:
      return a.WebkitLineClamp() == b.WebkitLineClamp();
    case CSSPropertyID::kWebkitLocale:
      return a.Locale() == b.Locale();
    case CSSPropertyID::kWebkitMaskBoxImageOutset:
      return a.MaskBoxImageOutset() == b.MaskBoxImageOutset();
    case CSSPropertyID::kWebkitMaskBoxImageSlice:
      return a.MaskBoxImageSlices() == b.MaskBoxImageSlices();
    case CSSPropertyID::kWebkitMaskBoxImageSource:
      return base::ValuesEquivalent(a.MaskBoxImageSource(),
                                    b.MaskBoxImageSource());
    case CSSPropertyID::kWebkitMaskBoxImageWidth:
      return a.MaskBoxImageWidth() == b.MaskBoxImageWidth();
    case CSSPropertyID::kMaskClip:
      return FillLayersEqual<CSSPropertyID::kMaskClip>(a.MaskLayers(),
                                                       b.MaskLayers());
    case CSSPropertyID::kMaskComposite:
      return FillLayersEqual<CSSPropertyID::kMaskComposite>(a.MaskLayers(),
                                                            b.MaskLayers());
    case CSSPropertyID::kMaskImage:
      return FillLayersEqual<CSSPropertyID::kMaskImage>(a.MaskLayers(),
                                                        b.MaskLayers());
    case CSSPropertyID::kMaskOrigin:
      return FillLayersEqual<CSSPropertyID::kMaskOrigin>(a.MaskLayers(),
                                                         b.MaskLayers());
    case CSSPropertyID::kMaskMode:
      return FillLayersEqual<CSSPropertyID::kMaskMode>(a.MaskLayers(),
                                                       b.MaskLayers());
    case CSSPropertyID::kWebkitMaskPositionX:
      return FillLayersEqual<CSSPropertyID::kWebkitMaskPositionX>(
          a.MaskLayers(), b.MaskLayers());
    case CSSPropertyID::kWebkitMaskPositionY:
      return FillLayersEqual<CSSPropertyID::kWebkitMaskPositionY>(
          a.MaskLayers(), b.MaskLayers());
    case CSSPropertyID::kMaskRepeat:
      return FillLayersEqual<CSSPropertyID::kMaskRepeat>(a.MaskLayers(),
                                                         b.MaskLayers());
    case CSSPropertyID::kMaskSize:
      return FillLayersEqual<CSSPropertyID::kMaskSize>(a.MaskLayers(),
                                                       b.MaskLayers());
    case CSSPropertyID::kWebkitTextFillColor:
      return a.TextFillColor() == b.TextFillColor();
    case CSSPropertyID::kWebkitTextOrientation:
      return a.GetTextOrientation() == b.GetTextOrientation();
    case CSSPropertyID::kPerspective:
      return a.Perspective() == b.Perspective();
    case CSSPropertyID::kPerspectiveOrigin:
      return a.PerspectiveOrigin() == b.PerspectiveOrigin();
    case CSSPropertyID::kWebkitTextStrokeColor:
      return a.TextStrokeColor() == b.TextStrokeColor() &&
             a.InternalVisitedTextStrokeColor() ==
                 b.InternalVisitedTextStrokeColor();
    case CSSPropertyID::kWebkitTextStrokeWidth:
      return a.TextStrokeWidth() == b.TextStrokeWidth();
    case CSSPropertyID::kTransform:
      return a.Transform() == b.Transform();
    case CSSPropertyID::kTranslate:
      return base::ValuesEquivalent<TransformOperation>(a.Translate(),
                                                        b.Translate());
    case CSSPropertyID::kRotate:
      return base::ValuesEquivalent<TransformOperation>(a.Rotate(), b.Rotate());
    case CSSPropertyID::kScale:
      return base::ValuesEquivalent<TransformOperation>(a.Scale(), b.Scale());
    case CSSPropertyID::kSize:
      return a.GetPageSizeType() == b.GetPageSizeType() &&
             a.PageSize() == b.PageSize();
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
    case CSSPropertyID::kWhiteSpaceCollapse:
      return a.GetWhiteSpaceCollapse() == b.GetWhiteSpaceCollapse();
    case CSSPropertyID::kWidows:
      return a.Widows() == b.Widows();
    case CSSPropertyID::kWidth:
      return a.Width() == b.Width();
    case CSSPropertyID::kWordBreak:
      return a.WordBreak() == b.WordBreak();
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
    case CSSPropertyID::kZoom:
      return a.Zoom() == b.Zoom();
    case CSSPropertyID::kPositionTryOrder:
      return a.PositionTryOrder() == b.PositionTryOrder();
    case CSSPropertyID::kPositionTryFallbacks:
      return base::ValuesEquivalent(a.GetPositionTryFallbacks(),
                                    b.GetPositionTryFallbacks());
    case CSSPropertyID::kPositionVisibility:
      return a.GetPositionVisibility() == b.GetPositionVisibility();

    // These properties are not animateable, but perhaps equality should still
    // be defined for them.
    case CSSPropertyID::kScrollTimelineAxis:
    case CSSPropertyID::kScrollTimelineName:
    case CSSPropertyID::kViewTimelineAxis:
    case CSSPropertyID::kViewTimelineInset:
    case CSSPropertyID::kViewTimelineName:
      NOTREACHED_IN_MIGRATION()
          << property.GetCSSPropertyName().ToAtomicString().Ascii();
      return true;

    // Webkit Aliases. These should not be reachable since they are converted to
    // their non-aliased counterpart before calling this function.
    case CSSPropertyID::kAliasEpubCaptionSide:
    case CSSPropertyID::kAliasEpubTextCombine:
    case CSSPropertyID::kAliasEpubTextEmphasis:
    case CSSPropertyID::kAliasEpubTextEmphasisColor:
    case CSSPropertyID::kAliasEpubTextEmphasisStyle:
    case CSSPropertyID::kAliasEpubTextOrientation:
    case CSSPropertyID::kAliasEpubTextTransform:
    case CSSPropertyID::kAliasEpubWordBreak:
    case CSSPropertyID::kAliasEpubWritingMode:
    case CSSPropertyID::kAliasWebkitAlignContent:
    case CSSPropertyID::kAliasWebkitAlignItems:
    case CSSPropertyID::kAliasWebkitAlignSelf:
    case CSSPropertyID::kAliasWebkitAlternativeAnimationWithTimeline:
    case CSSPropertyID::kAliasWebkitAnimation:
    case CSSPropertyID::kAliasWebkitAnimationDelay:
    case CSSPropertyID::kAliasWebkitAnimationDirection:
    case CSSPropertyID::kAliasWebkitAnimationDuration:
    case CSSPropertyID::kAliasWebkitAnimationFillMode:
    case CSSPropertyID::kAliasWebkitAnimationIterationCount:
    case CSSPropertyID::kAliasWebkitAnimationName:
    case CSSPropertyID::kAliasWebkitAnimationPlayState:
    case CSSPropertyID::kAliasWebkitAnimationTimingFunction:
    case CSSPropertyID::kAliasWebkitAppRegion:
    case CSSPropertyID::kAliasWebkitAppearance:
    case CSSPropertyID::kAliasWebkitBackfaceVisibility:
    case CSSPropertyID::kAliasWebkitBackgroundClip:
    case CSSPropertyID::kAliasWebkitBackgroundOrigin:
    case CSSPropertyID::kAliasWebkitBackgroundSize:
    case CSSPropertyID::kAliasWebkitBorderAfter:
    case CSSPropertyID::kAliasWebkitBorderAfterColor:
    case CSSPropertyID::kAliasWebkitBorderAfterStyle:
    case CSSPropertyID::kAliasWebkitBorderAfterWidth:
    case CSSPropertyID::kAliasWebkitBorderBefore:
    case CSSPropertyID::kAliasWebkitBorderBeforeColor:
    case CSSPropertyID::kAliasWebkitBorderBeforeStyle:
    case CSSPropertyID::kAliasWebkitBorderBeforeWidth:
    case CSSPropertyID::kAliasWebkitBorderBottomLeftRadius:
    case CSSPropertyID::kAliasWebkitBorderBottomRightRadius:
    case CSSPropertyID::kAliasWebkitBorderEnd:
    case CSSPropertyID::kAliasWebkitBorderEndColor:
    case CSSPropertyID::kAliasWebkitBorderEndStyle:
    case CSSPropertyID::kAliasWebkitBorderEndWidth:
    case CSSPropertyID::kAliasWebkitBorderRadius:
    case CSSPropertyID::kAliasWebkitBorderStart:
    case CSSPropertyID::kAliasWebkitBorderStartColor:
    case CSSPropertyID::kAliasWebkitBorderStartStyle:
    case CSSPropertyID::kAliasWebkitBorderStartWidth:
    case CSSPropertyID::kAliasWebkitBorderTopLeftRadius:
    case CSSPropertyID::kAliasWebkitBorderTopRightRadius:
    case CSSPropertyID::kAliasWebkitBoxShadow:
    case CSSPropertyID::kAliasWebkitBoxSizing:
    case CSSPropertyID::kAliasWebkitClipPath:
    case CSSPropertyID::kAliasWebkitColumnCount:
    case CSSPropertyID::kAliasWebkitColumnGap:
    case CSSPropertyID::kAliasWebkitColumnRule:
    case CSSPropertyID::kAliasWebkitColumnRuleColor:
    case CSSPropertyID::kAliasWebkitColumnRuleStyle:
    case CSSPropertyID::kAliasWebkitColumnRuleWidth:
    case CSSPropertyID::kAliasWebkitColumnSpan:
    case CSSPropertyID::kAliasWebkitColumnWidth:
    case CSSPropertyID::kAliasWebkitColumns:
    case CSSPropertyID::kAliasWebkitFilter:
    case CSSPropertyID::kAliasWebkitFlex:
    case CSSPropertyID::kAliasWebkitFlexBasis:
    case CSSPropertyID::kAliasWebkitFlexDirection:
    case CSSPropertyID::kAliasWebkitFlexFlow:
    case CSSPropertyID::kAliasWebkitFlexGrow:
    case CSSPropertyID::kAliasWebkitFlexShrink:
    case CSSPropertyID::kAliasWebkitFlexWrap:
    case CSSPropertyID::kAliasWebkitFontFeatureSettings:
    case CSSPropertyID::kAliasWebkitHyphenateCharacter:
    case CSSPropertyID::kAliasWebkitJustifyContent:
    case CSSPropertyID::kAliasWebkitLogicalHeight:
    case CSSPropertyID::kAliasWebkitLogicalWidth:
    case CSSPropertyID::kAliasWebkitMarginAfter:
    case CSSPropertyID::kAliasWebkitMarginBefore:
    case CSSPropertyID::kAliasWebkitMarginEnd:
    case CSSPropertyID::kAliasWebkitMarginStart:
    case CSSPropertyID::kAliasWebkitMask:
    case CSSPropertyID::kAliasWebkitMaskClip:
    case CSSPropertyID::kAliasWebkitMaskComposite:
    case CSSPropertyID::kAliasWebkitMaskImage:
    case CSSPropertyID::kAliasWebkitMaskOrigin:
    case CSSPropertyID::kAliasWebkitMaskPosition:
    case CSSPropertyID::kAliasWebkitMaskRepeat:
    case CSSPropertyID::kAliasWebkitMaskSize:
    case CSSPropertyID::kAliasWebkitMaxLogicalHeight:
    case CSSPropertyID::kAliasWebkitMaxLogicalWidth:
    case CSSPropertyID::kAliasWebkitMinLogicalHeight:
    case CSSPropertyID::kAliasWebkitMinLogicalWidth:
    case CSSPropertyID::kAliasWebkitOpacity:
    case CSSPropertyID::kAliasWebkitOrder:
    case CSSPropertyID::kAliasWebkitPaddingAfter:
    case CSSPropertyID::kAliasWebkitPaddingBefore:
    case CSSPropertyID::kAliasWebkitPaddingEnd:
    case CSSPropertyID::kAliasWebkitPaddingStart:
    case CSSPropertyID::kAliasWebkitPerspective:
    case CSSPropertyID::kAliasWebkitPerspectiveOrigin:
    case CSSPropertyID::kAliasWebkitShapeImageThreshold:
    case CSSPropertyID::kAliasWebkitShapeMargin:
    case CSSPropertyID::kAliasWebkitShapeOutside:
    case CSSPropertyID::kAliasWebkitTextEmphasis:
    case CSSPropertyID::kAliasWebkitTextEmphasisColor:
    case CSSPropertyID::kAliasWebkitTextEmphasisPosition:
    case CSSPropertyID::kAliasWebkitTextEmphasisStyle:
    case CSSPropertyID::kAliasWebkitTextSizeAdjust:
    case CSSPropertyID::kAliasWebkitTransform:
    case CSSPropertyID::kAliasWebkitTransformOrigin:
    case CSSPropertyID::kAliasWebkitTransformStyle:
    case CSSPropertyID::kAliasWebkitTransition:
    case CSSPropertyID::kAliasWebkitTransitionDelay:
    case CSSPropertyID::kAliasWebkitTransitionDuration:
    case CSSPropertyID::kAliasWebkitTransitionProperty:
    case CSSPropertyID::kAliasWebkitTransitionTimingFunction:
    case CSSPropertyID::kAliasWebkitUserSelect:
    case CSSPropertyID::kAliasWordWrap:
    case CSSPropertyID::kAliasGridColumnGap:
    case CSSPropertyID::kAliasGridRowGap:
    case CSSPropertyID::kAliasGridGap:
      NOTREACHED_IN_MIGRATION()
          << "Aliases CSS properties should be converted to their non-aliased "
             "counterpart before calling this function. CSS property name: "
          << property.GetCSSPropertyName().ToAtomicString().Ascii();
      return true;

    // Webkit prefixed properties which don't have non-aliased counterparts.
    // TODO ensure that each of these are reachable since they supposedly aren't
    // just aliases.
    case CSSPropertyID::kWebkitBorderImage:
    case CSSPropertyID::kWebkitBoxAlign:
    case CSSPropertyID::kWebkitBoxDecorationBreak:
    case CSSPropertyID::kWebkitBoxDirection:
    case CSSPropertyID::kWebkitBoxFlex:
    case CSSPropertyID::kWebkitBoxOrdinalGroup:
    case CSSPropertyID::kWebkitBoxOrient:
    case CSSPropertyID::kWebkitBoxPack:
    case CSSPropertyID::kWebkitBoxReflect:
    case CSSPropertyID::kWebkitLineBreak:
    case CSSPropertyID::kWebkitMaskBoxImageRepeat:
    case CSSPropertyID::kWebkitPrintColorAdjust:
    case CSSPropertyID::kWebkitRtlOrdering:
    case CSSPropertyID::kWebkitRubyPosition:
    case CSSPropertyID::kWebkitTapHighlightColor:
    case CSSPropertyID::kWebkitTextCombine:
    case CSSPropertyID::kWebkitTextDecorationsInEffect:
    case CSSPropertyID::kWebkitTextSecurity:
    case CSSPropertyID::kWebkitUserDrag:
    case CSSPropertyID::kWebkitUserModify:
      return true;

    // These logical properties compute to physical properties. Transitions
    // should check for equality on physical properties and run there.
    case CSSPropertyID::kBlockSize:
    case CSSPropertyID::kBorderBlockEndColor:
    case CSSPropertyID::kBorderBlockEndStyle:
    case CSSPropertyID::kBorderBlockEndWidth:
    case CSSPropertyID::kBorderBlockStartColor:
    case CSSPropertyID::kBorderBlockStartStyle:
    case CSSPropertyID::kBorderBlockStartWidth:
    case CSSPropertyID::kBorderEndEndRadius:
    case CSSPropertyID::kBorderEndStartRadius:
    case CSSPropertyID::kBorderInlineEndColor:
    case CSSPropertyID::kBorderInlineEndStyle:
    case CSSPropertyID::kBorderInlineEndWidth:
    case CSSPropertyID::kBorderInlineStartColor:
    case CSSPropertyID::kBorderInlineStartStyle:
    case CSSPropertyID::kBorderInlineStartWidth:
    case CSSPropertyID::kBorderStartEndRadius:
    case CSSPropertyID::kBorderStartStartRadius:
    case CSSPropertyID::kBorderBlock:
    case CSSPropertyID::kBorderBlockColor:
    case CSSPropertyID::kBorderBlockEnd:
    case CSSPropertyID::kBorderBlockStart:
    case CSSPropertyID::kBorderBlockStyle:
    case CSSPropertyID::kBorderBlockWidth:
    case CSSPropertyID::kBorderInline:
    case CSSPropertyID::kBorderInlineColor:
    case CSSPropertyID::kBorderInlineEnd:
    case CSSPropertyID::kBorderInlineStart:
    case CSSPropertyID::kBorderInlineStyle:
    case CSSPropertyID::kBorderInlineWidth:
    case CSSPropertyID::kContainIntrinsicBlockSize:
    case CSSPropertyID::kContainIntrinsicInlineSize:
    case CSSPropertyID::kInsetInlineStart:
    case CSSPropertyID::kInsetInlineEnd:
    case CSSPropertyID::kInsetBlockStart:
    case CSSPropertyID::kInsetBlockEnd:
    case CSSPropertyID::kInternalOverflowBlock:
    case CSSPropertyID::kInternalOverflowInline:
    case CSSPropertyID::kOverflowBlock:
    case CSSPropertyID::kOverflowInline:
    case CSSPropertyID::kOverscrollBehaviorBlock:
    case CSSPropertyID::kOverscrollBehaviorInline:
    case CSSPropertyID::kMinInlineSize:
    case CSSPropertyID::kMinBlockSize:
    case CSSPropertyID::kMaxInlineSize:
    case CSSPropertyID::kMaxBlockSize:
    case CSSPropertyID::kMarginInlineStart:
    case CSSPropertyID::kMarginInlineEnd:
    case CSSPropertyID::kMarginBlockStart:
    case CSSPropertyID::kMarginBlockEnd:
    case CSSPropertyID::kPaddingInlineStart:
    case CSSPropertyID::kPaddingInlineEnd:
    case CSSPropertyID::kPaddingBlockStart:
    case CSSPropertyID::kPaddingBlockEnd:
    case CSSPropertyID::kScrollMarginBlockEnd:
    case CSSPropertyID::kScrollMarginBlockStart:
    case CSSPropertyID::kScrollMarginInlineEnd:
    case CSSPropertyID::kScrollMarginInlineStart:
    case CSSPropertyID::kScrollPaddingBlockEnd:
    case CSSPropertyID::kScrollPaddingBlockStart:
    case CSSPropertyID::kScrollPaddingInlineEnd:
    case CSSPropertyID::kScrollPaddingInlineStart:
    case CSSPropertyID::kScrollStartBlock:
    case CSSPropertyID::kScrollStartInline:
    case CSSPropertyID::kInlineSize:
    case CSSPropertyID::kInsetBlock:
    case CSSPropertyID::kInsetInline:
    case CSSPropertyID::kMarginBlock:
    case CSSPropertyID::kMarginInline:
    case CSSPropertyID::kPaddingBlock:
    case CSSPropertyID::kPaddingInline:
    case CSSPropertyID::kScrollMarginBlock:
    case CSSPropertyID::kScrollMarginInline:
    case CSSPropertyID::kScrollPaddingBlock:
    case CSSPropertyID::kScrollPaddingInline:
      NOTREACHED_IN_MIGRATION()
          << property.GetCSSPropertyName().ToAtomicString().Ascii();
      return true;

    // No transitions on internal properties:
    case CSSPropertyID::kInternalAlignContentBlock:
    case CSSPropertyID::kInternalEmptyLineHeight:
    case CSSPropertyID::kInternalFontSizeDelta:
    case CSSPropertyID::kInternalForcedBackgroundColor:
    case CSSPropertyID::kInternalForcedBorderColor:
    case CSSPropertyID::kInternalForcedColor:
    case CSSPropertyID::kInternalForcedOutlineColor:
    case CSSPropertyID::kInternalForcedVisitedColor:
    case CSSPropertyID::kInternalVisitedBackgroundColor:
    case CSSPropertyID::kInternalVisitedBorderBlockEndColor:
    case CSSPropertyID::kInternalVisitedBorderBlockStartColor:
    case CSSPropertyID::kInternalVisitedBorderBottomColor:
    case CSSPropertyID::kInternalVisitedBorderInlineEndColor:
    case CSSPropertyID::kInternalVisitedBorderInlineStartColor:
    case CSSPropertyID::kInternalVisitedBorderLeftColor:
    case CSSPropertyID::kInternalVisitedBorderRightColor:
    case CSSPropertyID::kInternalVisitedBorderTopColor:
    case CSSPropertyID::kInternalVisitedCaretColor:
    case CSSPropertyID::kInternalVisitedColor:
    case CSSPropertyID::kInternalVisitedColumnRuleColor:
    case CSSPropertyID::kInternalVisitedFill:
    case CSSPropertyID::kInternalVisitedOutlineColor:
    case CSSPropertyID::kInternalVisitedStroke:
    case CSSPropertyID::kInternalVisitedTextDecorationColor:
    case CSSPropertyID::kInternalVisitedTextEmphasisColor:
    case CSSPropertyID::kInternalVisitedTextFillColor:
    case CSSPropertyID::kInternalVisitedTextStrokeColor:
      NOTREACHED_IN_MIGRATION()
          << property.GetCSSPropertyName().ToAtomicString().Ascii();
      return true;

    // Shorthand properties shouldn't be compared, use their longhands.
    case CSSPropertyID::kBackground:
    case CSSPropertyID::kBackgroundPosition:
    case CSSPropertyID::kBorder:
    case CSSPropertyID::kBorderBottom:
    case CSSPropertyID::kBorderColor:
    case CSSPropertyID::kBorderImage:
    case CSSPropertyID::kBorderLeft:
    case CSSPropertyID::kBorderRadius:
    case CSSPropertyID::kBorderRight:
    case CSSPropertyID::kBorderSpacing:
    case CSSPropertyID::kBorderStyle:
    case CSSPropertyID::kBorderTop:
    case CSSPropertyID::kBorderWidth:
    case CSSPropertyID::kColumnRule:
    case CSSPropertyID::kColumns:
    case CSSPropertyID::kContainIntrinsicSize:
    case CSSPropertyID::kContainer:
    case CSSPropertyID::kInset:
    case CSSPropertyID::kFlex:
    case CSSPropertyID::kFlexFlow:
    case CSSPropertyID::kFont:
    case CSSPropertyID::kFontSynthesis:
    case CSSPropertyID::kFontVariant:
    case CSSPropertyID::kGap:
    case CSSPropertyID::kGrid:
    case CSSPropertyID::kGridArea:
    case CSSPropertyID::kGridColumn:
    case CSSPropertyID::kGridRow:
    case CSSPropertyID::kGridTemplate:
    case CSSPropertyID::kListStyle:
    case CSSPropertyID::kMargin:
    case CSSPropertyID::kMarker:
    case CSSPropertyID::kMask:
    case CSSPropertyID::kMasonryTrack:
    case CSSPropertyID::kOffset:
    case CSSPropertyID::kOutline:
    case CSSPropertyID::kOverflow:
    case CSSPropertyID::kOverscrollBehavior:
    case CSSPropertyID::kPadding:
    case CSSPropertyID::kPageBreakAfter:
    case CSSPropertyID::kPageBreakBefore:
    case CSSPropertyID::kPageBreakInside:
    case CSSPropertyID::kPlaceContent:
    case CSSPropertyID::kPlaceItems:
    case CSSPropertyID::kPlaceSelf:
    case CSSPropertyID::kPositionTry:
    case CSSPropertyID::kScrollMargin:
    case CSSPropertyID::kScrollPadding:
    case CSSPropertyID::kScrollStart:
    case CSSPropertyID::kScrollTimeline:
    case CSSPropertyID::kTextBox:
    case CSSPropertyID::kTextDecoration:
    case CSSPropertyID::kTextEmphasis:
    case CSSPropertyID::kTextSpacing:
    case CSSPropertyID::kTextWrap:
    case CSSPropertyID::kTransition:
    case CSSPropertyID::kViewTimeline:
    case CSSPropertyID::kWebkitColumnBreakAfter:
    case CSSPropertyID::kWebkitColumnBreakBefore:
    case CSSPropertyID::kWebkitColumnBreakInside:
    case CSSPropertyID::kWebkitMaskBoxImage:
    case CSSPropertyID::kMaskPosition:
    case CSSPropertyID::kWebkitTextStroke:
    case CSSPropertyID::kWhiteSpace:
      NOTREACHED_IN_MIGRATION()
          << property.GetCSSPropertyName().ToAtomicString().Ascii();
      return true;

    // Non-animateable properties
    case CSSPropertyID::kAlternativeAnimationWithTimeline:
    case CSSPropertyID::kAnimation:
    case CSSPropertyID::kAnimationComposition:
    case CSSPropertyID::kAnimationDelay:
    case CSSPropertyID::kAnimationDirection:
    case CSSPropertyID::kAnimationDuration:
    case CSSPropertyID::kAnimationFillMode:
    case CSSPropertyID::kAnimationIterationCount:
    case CSSPropertyID::kAnimationName:
    case CSSPropertyID::kAnimationPlayState:
    case CSSPropertyID::kAnimationRange:
    case CSSPropertyID::kAnimationRangeEnd:
    case CSSPropertyID::kAnimationRangeStart:
    case CSSPropertyID::kAnimationTimeline:
    case CSSPropertyID::kAnimationTimingFunction:
    case CSSPropertyID::kContain:
    case CSSPropertyID::kContainerName:
    case CSSPropertyID::kContainerType:
    case CSSPropertyID::kDirection:
    case CSSPropertyID::kTextCombineUpright:
    case CSSPropertyID::kTextOrientation:
    case CSSPropertyID::kTimelineScope:
    case CSSPropertyID::kTransitionBehavior:
    case CSSPropertyID::kTransitionDelay:
    case CSSPropertyID::kTransitionDuration:
    case CSSPropertyID::kTransitionProperty:
    case CSSPropertyID::kTransitionTimingFunction:
    case CSSPropertyID::kUnicodeBidi:
    case CSSPropertyID::kWebkitWritingMode:
    case CSSPropertyID::kWillChange:
    case CSSPropertyID::kWritingMode:
      NOTREACHED_IN_MIGRATION()
          << property.GetCSSPropertyName().ToAtomicString().Ascii();
      return true;

    // TODO(crbug.com/1459374): Implement comparison for these properties. They
    // are reachable via transitions now.
    case CSSPropertyID::kAdditiveSymbols:
    case CSSPropertyID::kAscentOverride:
    case CSSPropertyID::kBasePalette:
    case CSSPropertyID::kDescentOverride:
    case CSSPropertyID::kFallback:
    case CSSPropertyID::kFontDisplay:
    case CSSPropertyID::kInherits:
    case CSSPropertyID::kInitialValue:
    case CSSPropertyID::kLineGapOverride:
    case CSSPropertyID::kNavigation:
    case CSSPropertyID::kNegative:
    case CSSPropertyID::kOverrideColors:
    case CSSPropertyID::kPad:
    case CSSPropertyID::kPrefix:
    case CSSPropertyID::kRange:
    case CSSPropertyID::kSpeakAs:
    case CSSPropertyID::kSrc:
    case CSSPropertyID::kSuffix:
    case CSSPropertyID::kSymbols:
    case CSSPropertyID::kSyntax:
    case CSSPropertyID::kSystem:
    case CSSPropertyID::kTypes:
    case CSSPropertyID::kUnicodeRange:
      return true;

    // Invalid properties.
    case CSSPropertyID::kAll:
    case CSSPropertyID::kInvalid:
    case CSSPropertyID::kVariable:
      NOTREACHED_IN_MIGRATION()
          << property.GetCSSPropertyName().ToAtomicString().Ascii();
      return true;
  }
}

}  // namespace blink
