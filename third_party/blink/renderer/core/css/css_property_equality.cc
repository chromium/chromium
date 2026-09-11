// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/css_property_equality.h"

#include <type_traits>

#include "base/compiler_specific.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/values_equivalent.h"
#include "third_party/blink/renderer/core/animation/css/css_animations.h"
#include "third_party/blink/renderer/core/animation/property_handle.h"
#include "third_party/blink/renderer/core/css/css_property_names.h"
#include "third_party/blink/renderer/core/css/css_value.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/core/style/shadow_list.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/wtf/type_traits.h"

// TODO(ikilpatrick): generate this file.

namespace blink {

namespace {

template <typename T>
struct IsScopedRefptr : std::false_type {};
template <typename T>
struct IsScopedRefptr<scoped_refptr<T>> : std::true_type {};

// Compares two computed values with ==. Values held by pointer must go
// through base::ValuesEquivalent() instead, because the style builder makes
// a new object each time a declaration is applied and == would compare
// addresses. ALWAYS_INLINE so this compiles to the same code as a bare ==.
template <typename T>
  requires(std::is_arithmetic_v<T> || std::is_enum_v<T>)
ALWAYS_INLINE bool ValueEquals(T a, T b) {
  return a == b;
}
template <typename T>
  requires(!std::is_arithmetic_v<T> && !std::is_enum_v<T>)
ALWAYS_INLINE bool ValueEquals(const T& a, const T& b) {
  static_assert(
      !std::is_pointer_v<T> && !IsAnyMemberType<T>::value &&
          !IsScopedRefptr<T>::value,
      "Pointer-valued property: compare with base::ValuesEquivalent()");
  return a == b;
}

template <CSSPropertyID property>
bool CounterRulesEqual(const CounterDirectiveMap* a_map,
                       const CounterDirectiveMap* b_map) {
  if (a_map == b_map) {
    return true;
  }
  if (!a_map || !b_map) {
    return false;
  }

  if (RuntimeEnabledFeatures::CSSCounterResetReversedEnabled()) {
    if (a_map->size() != b_map->size()) {
      return false;
    }

    for (const auto& a_entry : *a_map) {
      const auto b_iterator = b_map->find(a_entry.key);
      if (b_iterator == b_map->end()) {
        return false;
      }

      const CounterDirectives& a_value = a_entry.value;
      const CounterDirectives& b_value = b_iterator->value;
      switch (property) {
        case CSSPropertyID::kCounterIncrement:
          if (a_value.HasIncrement() != b_value.HasIncrement()) {
            return false;
          }
          if (a_value.HasIncrement() &&
              a_value.IncrementValue() != b_value.IncrementValue()) {
            return false;
          }
          break;
        case CSSPropertyID::kCounterReset:
          if (a_value.IsReset() != b_value.IsReset()) {
            return false;
          }
          if (a_value.IsReset() &&
              (a_value.ResetValue() != b_value.ResetValue() ||
               a_value.IsResetReversed() != b_value.IsResetReversed())) {
            return false;
          }
          break;
        case CSSPropertyID::kCounterSet:
          if (a_value.HasSet() != b_value.HasSet()) {
            return false;
          }
          if (a_value.HasSet() && a_value.SetValue() != b_value.SetValue()) {
            return false;
          }
          break;
        default:
          NOTREACHED();
      }
    }
    return true;
  }

  return std::ranges::equal(*a_map, *b_map, [](const auto& a, const auto& b) {
    switch (property) {
      case CSSPropertyID::kCounterIncrement:
        if (a.value.HasIncrement() != b.value.HasIncrement()) {
          return false;
        }
        if (a.value.HasIncrement() &&
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
        if (a.value.HasSet() != b.value.HasSet()) {
          return false;
        }
        if (a.value.HasSet() && a.value.SetValue() != b.value.SetValue()) {
          return false;
        }
        break;
      default:
        NOTREACHED();
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
        NOTREACHED();
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
      return ValueEquals(a.AlignContent(), b.AlignContent());
    case CSSPropertyID::kAlignItems:
      return ValueEquals(a.AlignItems(), b.AlignItems());
    case CSSPropertyID::kAlignSelf:
      return ValueEquals(a.AlignSelf(), b.AlignSelf());
    case CSSPropertyID::kAlignmentBaseline:
      return ValueEquals(a.AlignmentBaseline(), b.AlignmentBaseline());
    case CSSPropertyID::kPositionAnchor:
      return ValueEquals(a.PositionAnchor(), b.PositionAnchor());
    case CSSPropertyID::kAnchorName:
      return base::ValuesEquivalent(a.AnchorName(), b.AnchorName());
    case CSSPropertyID::kAnchorScope:
      return ValueEquals(a.AnchorScope(), b.AnchorScope());
    case CSSPropertyID::kAppearance:
      return ValueEquals(a.Appearance(), b.Appearance());
    case CSSPropertyID::kAppRegion:
    case CSSPropertyID::kWindowDrag:
      return ValueEquals(a.DraggableRegionMode(), b.DraggableRegionMode());
    case CSSPropertyID::kBackfaceVisibility:
      return ValueEquals(a.BackfaceVisibility(), b.BackfaceVisibility());
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
      return ValueEquals(a.BackgroundColor(), b.BackgroundColor()) &&
             ValueEquals(a.InternalVisitedBackgroundColor(),
                         b.InternalVisitedBackgroundColor());
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
      return ValueEquals(a.BaselineShift(), b.BaselineShift());
    case CSSPropertyID::kBaselineSource:
      return ValueEquals(a.BaselineSource(), b.BaselineSource());
    case CSSPropertyID::kBlockEllipsis:
      return ValueEquals(a.BlockEllipsis(), b.BlockEllipsis());
    case CSSPropertyID::kBorderBottomColor:
      return ValueEquals(a.BorderBottomColor(), b.BorderBottomColor()) &&
             ValueEquals(a.InternalVisitedBorderBottomColor(),
                         b.InternalVisitedBorderBottomColor());
    case CSSPropertyID::kBorderBottomLeftRadius:
      return ValueEquals(a.BorderBottomLeftRadius(),
                         b.BorderBottomLeftRadius());
    case CSSPropertyID::kBorderBottomRightRadius:
      return ValueEquals(a.BorderBottomRightRadius(),
                         b.BorderBottomRightRadius());
    case CSSPropertyID::kBorderBottomStyle:
      return ValueEquals(a.BorderBottomStyle(), b.BorderBottomStyle());
    case CSSPropertyID::kBorderBottomWidth:
      return ValueEquals(a.SpecifiedBorderBottomWidth(),
                         b.SpecifiedBorderBottomWidth());
    case CSSPropertyID::kBorderCollapse:
      return ValueEquals(a.BorderCollapse(), b.BorderCollapse());
    case CSSPropertyID::kBorderImageOutset:
      return ValueEquals(a.BorderImageOutset(), b.BorderImageOutset());
    case CSSPropertyID::kBorderImageRepeat:
      return ValueEquals(a.BorderImage().HorizontalRule(),
                         b.BorderImage().HorizontalRule()) &&
             ValueEquals(a.BorderImage().VerticalRule(),
                         b.BorderImage().VerticalRule());
    case CSSPropertyID::kBorderImageSlice:
      return ValueEquals(a.BorderImageSlices(), b.BorderImageSlices());
    case CSSPropertyID::kBorderImageSource:
      return base::ValuesEquivalent(a.BorderImageSource(),
                                    b.BorderImageSource());
    case CSSPropertyID::kBorderImageWidth:
      return ValueEquals(a.BorderImageWidth(), b.BorderImageWidth());
    case CSSPropertyID::kBorderLeftColor:
      return ValueEquals(a.BorderLeftColor(), b.BorderLeftColor()) &&
             ValueEquals(a.InternalVisitedBorderLeftColor(),
                         b.InternalVisitedBorderLeftColor());
    case CSSPropertyID::kBorderLeftStyle:
      return ValueEquals(a.BorderLeftStyle(), b.BorderLeftStyle());
    case CSSPropertyID::kBorderLeftWidth:
      return ValueEquals(a.SpecifiedBorderLeftWidth(),
                         b.SpecifiedBorderLeftWidth());
    case CSSPropertyID::kBorderRightColor:
      return ValueEquals(a.BorderRightColor(), b.BorderRightColor()) &&
             ValueEquals(a.InternalVisitedBorderRightColor(),
                         b.InternalVisitedBorderRightColor());
    case CSSPropertyID::kBorderRightStyle:
      return ValueEquals(a.BorderRightStyle(), b.BorderRightStyle());
    case CSSPropertyID::kBorderRightWidth:
      return ValueEquals(a.SpecifiedBorderRightWidth(),
                         b.SpecifiedBorderRightWidth());
    case CSSPropertyID::kBorderTopColor:
      return ValueEquals(a.BorderTopColor(), b.BorderTopColor()) &&
             ValueEquals(a.InternalVisitedBorderTopColor(),
                         b.InternalVisitedBorderTopColor());
    case CSSPropertyID::kBorderTopLeftRadius:
      return ValueEquals(a.BorderTopLeftRadius(), b.BorderTopLeftRadius());
    case CSSPropertyID::kBorderTopRightRadius:
      return ValueEquals(a.BorderTopRightRadius(), b.BorderTopRightRadius());
    case CSSPropertyID::kBorderTopStyle:
      return ValueEquals(a.BorderTopStyle(), b.BorderTopStyle());
    case CSSPropertyID::kBorderTopWidth:
      return ValueEquals(a.SpecifiedBorderTopWidth(),
                         b.SpecifiedBorderTopWidth());
    case CSSPropertyID::kBorderShape:
      return base::ValuesEquivalent(a.BorderShape(), b.BorderShape());
    case CSSPropertyID::kBottom:
      return ValueEquals(a.Bottom(), b.Bottom());
    case CSSPropertyID::kBoxDecorationBreak:
      return ValueEquals(a.BoxDecorationBreak(), b.BoxDecorationBreak());
    case CSSPropertyID::kBoxShadow:
      return base::ValuesEquivalent(a.BoxShadow(), b.BoxShadow());
    case CSSPropertyID::kBoxSizing:
      return ValueEquals(a.BoxSizing(), b.BoxSizing());
    case CSSPropertyID::kBreakAfter:
      return ValueEquals(a.BreakAfter(), b.BreakAfter());
    case CSSPropertyID::kBreakBefore:
      return ValueEquals(a.BreakBefore(), b.BreakBefore());
    case CSSPropertyID::kBreakInside:
      return ValueEquals(a.BreakInside(), b.BreakInside());
    case CSSPropertyID::kBufferedRendering:
      return ValueEquals(a.BufferedRendering(), b.BufferedRendering());
    case CSSPropertyID::kCaptionSide:
      return ValueEquals(a.CaptionSide(), b.CaptionSide());
    case CSSPropertyID::kCaretAnimation:
      return ValueEquals(a.CaretAnimation(), b.CaretAnimation());
    case CSSPropertyID::kCaretColor:
      return ValueEquals(a.CaretColor(), b.CaretColor()) &&
             ValueEquals(a.InternalVisitedCaretColor(),
                         b.InternalVisitedCaretColor());
    case CSSPropertyID::kCaretShape:
      return ValueEquals(a.CaretShape(), b.CaretShape());
    case CSSPropertyID::kClear:
      return ValueEquals(a.Clear(), b.Clear());
    case CSSPropertyID::kClip:
      return ValueEquals(a.Clip(), b.Clip());
    case CSSPropertyID::kClipRule:
      return ValueEquals(a.ClipRule(), b.ClipRule());
    case CSSPropertyID::kColor:
      return ValueEquals(a.Color(), b.Color()) &&
             ValueEquals(a.InternalVisitedColor(), b.InternalVisitedColor());
    case CSSPropertyID::kColorInterpolation:
      return ValueEquals(a.ColorInterpolation(), b.ColorInterpolation());
    case CSSPropertyID::kColorInterpolationFilters:
      return ValueEquals(a.ColorInterpolationFilters(),
                         b.ColorInterpolationFilters());
    case CSSPropertyID::kColorRendering:
      return ValueEquals(a.ColorRendering(), b.ColorRendering());
    case CSSPropertyID::kColorScheme:
      return ValueEquals(a.ColorScheme(), b.ColorScheme());
    case CSSPropertyID::kColumnFill:
      return ValueEquals(a.GetColumnFill(), b.GetColumnFill());
    case CSSPropertyID::kColumnRuleStyle:
      return ValueEquals(a.ColumnRuleStyle(), b.ColumnRuleStyle());
    case CSSPropertyID::kContinue:
      return ValueEquals(a.Continue(), b.Continue());
    case CSSPropertyID::kRowRuleStyle:
      return ValueEquals(a.RowRuleStyle(), b.RowRuleStyle());
    case CSSPropertyID::kColumnSpan:
      return ValueEquals(a.GetColumnSpan(), b.GetColumnSpan());
    case CSSPropertyID::kContent:
      return base::ValuesEquivalent(a.GetContentData(), b.GetContentData());
    case CSSPropertyID::kCornerBottomLeftShape:
      return ValueEquals(a.CornerBottomLeftShape(), b.CornerBottomLeftShape());
    case CSSPropertyID::kCornerBottomRightShape:
      return ValueEquals(a.CornerBottomRightShape(),
                         b.CornerBottomRightShape());
    case CSSPropertyID::kCornerTopLeftShape:
      return ValueEquals(a.CornerTopLeftShape(), b.CornerTopLeftShape());
    case CSSPropertyID::kCornerTopRightShape:
      return ValueEquals(a.CornerTopRightShape(), b.CornerTopRightShape());
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
      return ValueEquals(a.Cursor(), b.Cursor());
    case CSSPropertyID::kDisplay:
      return ValueEquals(a.Display(), b.Display());
    case CSSPropertyID::kContentVisibility:
      return ValueEquals(a.ContentVisibility(), b.ContentVisibility());
    case CSSPropertyID::kDominantBaseline:
      return ValueEquals(a.DominantBaseline(), b.DominantBaseline());
    case CSSPropertyID::kDynamicRangeLimit:
      return ValueEquals(a.GetDynamicRangeLimit(), b.GetDynamicRangeLimit());
    case CSSPropertyID::kEmptyCells:
      return ValueEquals(a.EmptyCells(), b.EmptyCells());
    case CSSPropertyID::kFill:
      return a.FillPaint().EqualTypeOrColor(b.FillPaint()) &&
             a.InternalVisitedFillPaint().EqualTypeOrColor(
                 b.InternalVisitedFillPaint());
    case CSSPropertyID::kFillRule:
      return ValueEquals(a.FillRule(), b.FillRule());
    case CSSPropertyID::kFlexDirection:
      return ValueEquals(a.FlexDirection(), b.FlexDirection());
    case CSSPropertyID::kFillOpacity:
      return ValueEquals(a.FillOpacity(), b.FillOpacity());
    case CSSPropertyID::kFlexBasis:
      return ValueEquals(a.FlexBasis(), b.FlexBasis());
    case CSSPropertyID::kFlexGrow:
      return ValueEquals(a.FlexGrow(), b.FlexGrow());
    case CSSPropertyID::kFlexShrink:
      return ValueEquals(a.FlexShrink(), b.FlexShrink());
    case CSSPropertyID::kFlexWrap:
      return ValueEquals(a.FlexWrap(), b.FlexWrap());
    case CSSPropertyID::kFlexLineCount:
      return ValueEquals(a.FlexLineCount(), b.FlexLineCount());
    case CSSPropertyID::kFloat:
      return ValueEquals(a.Floating(), b.Floating());
    case CSSPropertyID::kFloodColor:
      return ValueEquals(a.FloodColor(), b.FloodColor());
    case CSSPropertyID::kFloodOpacity:
      return ValueEquals(a.FloodOpacity(), b.FloodOpacity());
    case CSSPropertyID::kFontFamily:
      return ValueEquals(a.GetFontDescription().Family(),
                         b.GetFontDescription().Family());
    case CSSPropertyID::kFontKerning:
      return ValueEquals(a.GetFontDescription().GetKerning(),
                         b.GetFontDescription().GetKerning());
    case CSSPropertyID::kFontLanguageOverride:
      return ValueEquals(a.GetFontDescription().FontLanguageOverride(),
                         b.GetFontDescription().FontLanguageOverride());
    case CSSPropertyID::kFontOpticalSizing:
      return ValueEquals(a.GetFontDescription().FontOpticalSizing(),
                         b.GetFontDescription().FontOpticalSizing());
    case CSSPropertyID::kFontPalette:
      return base::ValuesEquivalent(a.GetFontPalette(), b.GetFontPalette());
    case CSSPropertyID::kFontFeatureSettings:
      return base::ValuesEquivalent(a.GetFontDescription().FeatureSettings(),
                                    b.GetFontDescription().FeatureSettings());
    case CSSPropertyID::kFontSize:
      // CSSPropertyID::kFontSize: Must pass a specified size to setFontSize if
      // Text Autosizing is enabled, but a computed size if text zoom is enabled
      // (if neither is enabled it's irrelevant as they're probably the same).
      // FIXME: Should we introduce an option to pass the computed font size
      // here, allowing consumers to enable text zoom rather than Text
      // Autosizing? See http://crbug.com/227545.
      return ValueEquals(a.SpecifiedFontSize(), b.SpecifiedFontSize());
    case CSSPropertyID::kFontSizeAdjust:
      return ValueEquals(a.FontSizeAdjust(), b.FontSizeAdjust());
    case CSSPropertyID::kFontStretch:
      return ValueEquals(a.GetFontStretch(), b.GetFontStretch());
    case CSSPropertyID::kFontStyle: {
      // Mirror the buckets in ComputedStyleUtils::ValueForFontStyle so a
      // transition fires iff the serialized computed value changes. `italic`
      // and any non-italic value never match; a slope of 0 always serializes
      // as `normal` regardless of source; the source only distinguishes the
      // serialization at slope == kItalicSlopeValue (bare `oblique` vs
      // `oblique 14deg`).
      using StyleSyntax = FontDescription::StyleSyntax;
      const FontDescription& fa = a.GetFontDescription();
      const FontDescription& fb = b.GetFontDescription();
      const bool a_italic = fa.GetStyleSyntax() == StyleSyntax::kItalicKeyword;
      const bool b_italic = fb.GetStyleSyntax() == StyleSyntax::kItalicKeyword;
      if (a_italic != b_italic) {
        return false;
      }
      if (a_italic) {
        return true;
      }
      if (a.GetFontStyle() != b.GetFontStyle()) {
        return false;
      }
      if (a.GetFontStyle() == kItalicSlopeValue) {
        return (fa.GetStyleSyntax() == StyleSyntax::kExplicitAngle) ==
               (fb.GetStyleSyntax() == StyleSyntax::kExplicitAngle);
      }
      return true;
    }
    case CSSPropertyID::kFontSynthesisSmallCaps:
      return ValueEquals(a.GetFontDescription().GetFontSynthesisSmallCaps(),
                         b.GetFontDescription().GetFontSynthesisSmallCaps());
    case CSSPropertyID::kFontSynthesisStyle:
      return ValueEquals(a.GetFontDescription().GetFontSynthesisStyle(),
                         b.GetFontDescription().GetFontSynthesisStyle());
    case CSSPropertyID::kFontSynthesisWeight:
      return ValueEquals(a.GetFontDescription().GetFontSynthesisWeight(),
                         b.GetFontDescription().GetFontSynthesisWeight());
    case CSSPropertyID::kFontVariantAlternates:
      return base::ValuesEquivalent(
          a.GetFontDescription().GetFontVariantAlternates(),
          b.GetFontDescription().GetFontVariantAlternates());
    case CSSPropertyID::kFontVariantCaps:
      return ValueEquals(a.GetFontDescription().VariantCaps(),
                         b.GetFontDescription().VariantCaps());
    case CSSPropertyID::kFontVariantEastAsian:
      return ValueEquals(a.GetFontDescription().VariantEastAsian(),
                         b.GetFontDescription().VariantEastAsian());
    case CSSPropertyID::kFontVariantEmoji:
      return ValueEquals(a.GetFontDescription().VariantEmoji(),
                         b.GetFontDescription().VariantEmoji());
    case CSSPropertyID::kFontVariantLigatures:
      return ValueEquals(a.GetFontDescription().GetVariantLigatures(),
                         b.GetFontDescription().GetVariantLigatures());
    case CSSPropertyID::kFontVariantNumeric:
      return ValueEquals(a.GetFontDescription().VariantNumeric(),
                         b.GetFontDescription().VariantNumeric());
    case CSSPropertyID::kFontVariantPosition:
      return ValueEquals(a.GetFontDescription().VariantPosition(),
                         b.GetFontDescription().VariantPosition());
    case CSSPropertyID::kFontVariationSettings:
      return base::ValuesEquivalent(a.GetFontDescription().VariationSettings(),
                                    b.GetFontDescription().VariationSettings());
    case CSSPropertyID::kFontWeight:
      return ValueEquals(a.GetFontWeight(), b.GetFontWeight());
    case CSSPropertyID::kForcedColorAdjust:
      return ValueEquals(a.ForcedColorAdjust(), b.ForcedColorAdjust());
    case CSSPropertyID::kFieldSizing:
      return ValueEquals(a.FieldSizing(), b.FieldSizing());
    case CSSPropertyID::kFlowTolerance:
      return ValueEquals(a.GetFlowTolerance(), b.GetFlowTolerance());
    case CSSPropertyID::kFrameSizing:
      return ValueEquals(a.FrameSizing(), b.FrameSizing());
    case CSSPropertyID::kGridAutoColumns:
      return ValueEquals(a.GridAutoColumns(), b.GridAutoColumns());
    case CSSPropertyID::kGridAutoFlow:
      return ValueEquals(a.GetGridAutoFlow(), b.GetGridAutoFlow());
    case CSSPropertyID::kGridAutoRows:
      return ValueEquals(a.GridAutoRows(), b.GridAutoRows());
    case CSSPropertyID::kGridColumnEnd:
      return ValueEquals(a.GridColumnEnd(), b.GridColumnEnd());
    case CSSPropertyID::kGridColumnStart:
      return ValueEquals(a.GridColumnStart(), b.GridColumnStart());
    case CSSPropertyID::kGridLanesDirection:
      return ValueEquals(a.GetGridLanesDirection(), b.GetGridLanesDirection());
    case CSSPropertyID::kGridLanesPack:
      return ValueEquals(a.GridLanesPack(), b.GridLanesPack());
    case CSSPropertyID::kGridRowEnd:
      return ValueEquals(a.GridRowEnd(), b.GridRowEnd());
    case CSSPropertyID::kGridRowStart:
      return ValueEquals(a.GridRowStart(), b.GridRowStart());
    case CSSPropertyID::kGridTemplateAreas:
      return base::ValuesEquivalent(a.GridTemplateAreas(),
                                    b.GridTemplateAreas());
    case CSSPropertyID::kGridTemplateColumns:
      return ValueEquals(a.GridTemplateColumns(), b.GridTemplateColumns());
    case CSSPropertyID::kGridTemplateRows:
      return ValueEquals(a.GridTemplateRows(), b.GridTemplateRows());
    case CSSPropertyID::kHangingPunctuation:
      return ValueEquals(a.GetHangingPunctuation(), b.GetHangingPunctuation());
    case CSSPropertyID::kHeight:
      return ValueEquals(a.Height(), b.Height());
    case CSSPropertyID::kInterestDelayStart:
      return ValueEquals(a.InterestDelayStart(), b.InterestDelayStart());
    case CSSPropertyID::kInterestDelayEnd:
      return ValueEquals(a.InterestDelayEnd(), b.InterestDelayEnd());
    case CSSPropertyID::kHyphenateCharacter:
      return ValueEquals(a.HyphenationString(), b.HyphenationString());
    case CSSPropertyID::kHyphenateLimitChars:
      return ValueEquals(a.HyphenateLimitChars(), b.HyphenateLimitChars());
    case CSSPropertyID::kHyphens:
      return ValueEquals(a.GetHyphens(), b.GetHyphens());
    case CSSPropertyID::kImageAnimation:
      return ValueEquals(a.ImageAnimation(), b.ImageAnimation());
    case CSSPropertyID::kImageOrientation:
      return ValueEquals(a.ImageOrientation(), b.ImageOrientation());
    case CSSPropertyID::kImageRendering:
      return ValueEquals(a.ImageRendering(), b.ImageRendering());
    case CSSPropertyID::kInitialLetter:
      return ValueEquals(a.InitialLetter(), b.InitialLetter());
    case CSSPropertyID::kPositionArea:
      return ValueEquals(a.GetPositionArea(), b.GetPositionArea());
    case CSSPropertyID::kInteractivity:
      return ValueEquals(a.Interactivity(), b.Interactivity());
    case CSSPropertyID::kInterpolateSize:
      return ValueEquals(a.InterpolateSize(), b.InterpolateSize());
    case CSSPropertyID::kIsolation:
      return ValueEquals(a.Isolation(), b.Isolation());
    case CSSPropertyID::kJustifyContent:
      return ValueEquals(a.JustifyContent(), b.JustifyContent());
    case CSSPropertyID::kJustifyItems:
      return ValueEquals(a.JustifyItems(), b.JustifyItems());
    case CSSPropertyID::kJustifySelf:
      return ValueEquals(a.JustifySelf(), b.JustifySelf());
    case CSSPropertyID::kLeft:
      return ValueEquals(a.Left(), b.Left());
    case CSSPropertyID::kLetterSpacing:
      return ValueEquals(a.ComputedLetterSpacing(), b.ComputedLetterSpacing());
    case CSSPropertyID::kLightingColor:
      return ValueEquals(a.LightingColor(), b.LightingColor());
    case CSSPropertyID::kLineBreak:
      return ValueEquals(a.GetLineBreak(), b.GetLineBreak());
    case CSSPropertyID::kLineClamp:
    case CSSPropertyID::kAlternativeWebkitLineClampLonghand:
      return ValueEquals(a.Continue(), b.Continue()) &&
             ValueEquals(a.MaxLines(), b.MaxLines()) &&
             ValueEquals(a.LineClampInternalBlockEllipsis(),
                         b.LineClampInternalBlockEllipsis());
    case CSSPropertyID::kLineHeight:
      return ValueEquals(a.LineHeight(), b.LineHeight());
    case CSSPropertyID::kTabSize:
      return ValueEquals(a.GetTabSize(), b.GetTabSize());
    case CSSPropertyID::kListStyleImage:
      return base::ValuesEquivalent(a.ListStyleImage(), b.ListStyleImage());
    case CSSPropertyID::kListStylePosition:
      return ValueEquals(a.ListStylePosition(), b.ListStylePosition());
    case CSSPropertyID::kListStyleType:
      return base::ValuesEquivalent(a.ListStyleType(), b.ListStyleType());
    case CSSPropertyID::kMarginBottom:
      return ValueEquals(a.MarginBottom(), b.MarginBottom());
    case CSSPropertyID::kMarginLeft:
      return ValueEquals(a.MarginLeft(), b.MarginLeft());
    case CSSPropertyID::kMarginRight:
      return ValueEquals(a.MarginRight(), b.MarginRight());
    case CSSPropertyID::kMarginTop:
      return ValueEquals(a.MarginTop(), b.MarginTop());
    case CSSPropertyID::kMarginTrim:
      return ValueEquals(a.MarginTrim(), b.MarginTrim());
    case CSSPropertyID::kMarkerEnd:
      return base::ValuesEquivalent(a.MarkerEndResource(),
                                    b.MarkerEndResource());
    case CSSPropertyID::kMarkerMid:
      return base::ValuesEquivalent(a.MarkerMidResource(),
                                    b.MarkerMidResource());
    case CSSPropertyID::kMarkerStart:
      return base::ValuesEquivalent(a.MarkerStartResource(),
                                    b.MarkerStartResource());
    case CSSPropertyID::kMaskType:
      return ValueEquals(a.MaskType(), b.MaskType());
    case CSSPropertyID::kMaxLines:
      return ValueEquals(a.MaxLines(), b.MaxLines());
    case CSSPropertyID::kMathShift:
      return ValueEquals(a.MathShift(), b.MathShift());
    case CSSPropertyID::kMathStyle:
      return ValueEquals(a.MathStyle(), b.MathStyle());
    case CSSPropertyID::kMaxContentSizing:
      return ValueEquals(a.MaxContentSizing(), b.MaxContentSizing());
    case CSSPropertyID::kMaxHeight:
      return ValueEquals(a.MaxHeight(), b.MaxHeight());
    case CSSPropertyID::kMaxWidth:
      return ValueEquals(a.MaxWidth(), b.MaxWidth());
    case CSSPropertyID::kMinHeight:
      return ValueEquals(a.MinHeight(), b.MinHeight());
    case CSSPropertyID::kMinWidth:
      return ValueEquals(a.MinWidth(), b.MinWidth());
    case CSSPropertyID::kMixBlendMode:
      return ValueEquals(a.GetBlendMode(), b.GetBlendMode());
    case CSSPropertyID::kObjectFit:
      return ValueEquals(a.GetObjectFit(), b.GetObjectFit());
    case CSSPropertyID::kObjectPosition:
      return ValueEquals(a.ObjectPosition(), b.ObjectPosition());
    case CSSPropertyID::kObjectViewBox:
      return base::ValuesEquivalent(a.ObjectViewBox(), b.ObjectViewBox());
    case CSSPropertyID::kOffsetAnchor:
      return ValueEquals(a.OffsetAnchor(), b.OffsetAnchor());
    case CSSPropertyID::kOffsetDistance:
      return ValueEquals(a.OffsetDistance(), b.OffsetDistance());
    case CSSPropertyID::kOffsetPath:
      return base::ValuesEquivalent(a.OffsetPath(), b.OffsetPath());
    case CSSPropertyID::kOffsetPosition:
      return ValueEquals(a.OffsetPosition(), b.OffsetPosition());
    case CSSPropertyID::kOffsetRotate:
      return ValueEquals(a.OffsetRotate(), b.OffsetRotate());
    case CSSPropertyID::kOpacity:
      return ValueEquals(a.Opacity(), b.Opacity());
    case CSSPropertyID::kOrder:
      return ValueEquals(a.Order(), b.Order());
    case CSSPropertyID::kOriginTrialTestProperty:
      return ValueEquals(a.OriginTrialTestProperty(),
                         b.OriginTrialTestProperty());
    case CSSPropertyID::kOrphans:
      return ValueEquals(a.Orphans(), b.Orphans());
    case CSSPropertyID::kOutlineColor:
      return ValueEquals(a.OutlineColor(), b.OutlineColor()) &&
             ValueEquals(a.InternalVisitedOutlineColor(),
                         b.InternalVisitedOutlineColor());
    case CSSPropertyID::kOutlineOffset:
      return ValueEquals(a.OutlineOffset(), b.OutlineOffset());
    case CSSPropertyID::kOutlineStyle:
      return ValueEquals(a.OutlineStyle(), b.OutlineStyle());
    case CSSPropertyID::kOutlineWidth:
      return ValueEquals(a.OutlineWidth(), b.OutlineWidth());
    case CSSPropertyID::kOverflowAnchor:
      return ValueEquals(a.OverflowAnchor(), b.OverflowAnchor());
    case CSSPropertyID::kOverflowClipMargin:
      return ValueEquals(a.OverflowClipMargin(), b.OverflowClipMargin());
    case CSSPropertyID::kOverflowWrap:
      return ValueEquals(a.OverflowWrap(), b.OverflowWrap());
    case CSSPropertyID::kOverflowX:
      return ValueEquals(a.OverflowX(), b.OverflowX());
    case CSSPropertyID::kOverflowY:
      return ValueEquals(a.OverflowY(), b.OverflowY());
    case CSSPropertyID::kOverscrollBehaviorX:
      return ValueEquals(a.OverscrollBehaviorX(), b.OverscrollBehaviorX());
    case CSSPropertyID::kOverscrollBehaviorY:
      return ValueEquals(a.OverscrollBehaviorY(), b.OverscrollBehaviorY());
    case CSSPropertyID::kOverscrollContainerType:
      return ValueEquals(a.OverscrollContainerType(),
                         b.OverscrollContainerType());
    case CSSPropertyID::kPaddingBottom:
      return ValueEquals(a.PaddingBottom(), b.PaddingBottom());
    case CSSPropertyID::kPaddingLeft:
      return ValueEquals(a.PaddingLeft(), b.PaddingLeft());
    case CSSPropertyID::kPaddingRight:
      return ValueEquals(a.PaddingRight(), b.PaddingRight());
    case CSSPropertyID::kPaddingTop:
      return ValueEquals(a.PaddingTop(), b.PaddingTop());
    case CSSPropertyID::kPage:
      return ValueEquals(a.Page(), b.Page());
    case CSSPropertyID::kPageMarginSafety:
      return ValueEquals(a.GetPageMarginSafety(), b.GetPageMarginSafety());
    case CSSPropertyID::kPageOrientation:
      return ValueEquals(a.GetPageOrientation(), b.GetPageOrientation());
    case CSSPropertyID::kPaintOrder:
      return ValueEquals(a.PaintOrder(), b.PaintOrder());
    case CSSPropertyID::kPointerEvents:
      return ValueEquals(a.PointerEvents(), b.PointerEvents());
    case CSSPropertyID::kPosition:
      return ValueEquals(a.GetPosition(), b.GetPosition());
    case CSSPropertyID::kQuotes:
      return base::ValuesEquivalent(a.Quotes(), b.Quotes());
    case CSSPropertyID::kReadingFlow:
      return ValueEquals(a.ReadingFlow(), b.ReadingFlow());
    case CSSPropertyID::kReadingOrder:
      return ValueEquals(a.ReadingOrder(), b.ReadingOrder());
    case CSSPropertyID::kResize:
      return ValueEquals(a.Resize(), b.Resize());
    case CSSPropertyID::kRight:
      return ValueEquals(a.Right(), b.Right());
    case CSSPropertyID::kRubyAlign:
      return ValueEquals(a.RubyAlign(), b.RubyAlign());
    case CSSPropertyID::kRubyOverhang:
      return ValueEquals(a.RubyOverhang(), b.RubyOverhang());
    case CSSPropertyID::kRubyPosition:
      return ValueEquals(a.GetRubyPosition(), b.GetRubyPosition());
    case CSSPropertyID::kScrollTargetGroup:
      return ValueEquals(a.ScrollTargetGroup(), b.ScrollTargetGroup());
    case CSSPropertyID::kScrollMarkerGroup:
      return base::ValuesEquivalent(a.GetScrollMarkerGroup(),
                                    b.GetScrollMarkerGroup());
    case CSSPropertyID::kScrollbarColor:
      return base::ValuesEquivalent(a.ScrollbarColor(), b.ScrollbarColor());
    case CSSPropertyID::kScrollbarGutter:
      return ValueEquals(a.ScrollbarGutter(), b.ScrollbarGutter());
    case CSSPropertyID::kScrollbarWidth:
      return ValueEquals(a.ScrollbarWidth(), b.ScrollbarWidth());
    case CSSPropertyID::kScrollAxisLock:
      return ValueEquals(a.ScrollAxisLock(), b.ScrollAxisLock());
    case CSSPropertyID::kScrollBehavior:
      return ValueEquals(a.GetScrollBehavior(), b.GetScrollBehavior());
    case CSSPropertyID::kScrollInitialTarget:
      return ValueEquals(a.ScrollInitialTarget(), b.ScrollInitialTarget());
    case CSSPropertyID::kScrollMarginBottom:
      return ValueEquals(a.ScrollMarginBottom(), b.ScrollMarginBottom());
    case CSSPropertyID::kScrollMarginLeft:
      return ValueEquals(a.ScrollMarginLeft(), b.ScrollMarginLeft());
    case CSSPropertyID::kScrollMarginRight:
      return ValueEquals(a.ScrollMarginRight(), b.ScrollMarginRight());
    case CSSPropertyID::kScrollMarginTop:
      return ValueEquals(a.ScrollMarginTop(), b.ScrollMarginTop());
    case CSSPropertyID::kScrollPaddingBottom:
      return ValueEquals(a.ScrollPaddingBottom(), b.ScrollPaddingBottom());
    case CSSPropertyID::kScrollPaddingLeft:
      return ValueEquals(a.ScrollPaddingLeft(), b.ScrollPaddingLeft());
    case CSSPropertyID::kScrollPaddingRight:
      return ValueEquals(a.ScrollPaddingRight(), b.ScrollPaddingRight());
    case CSSPropertyID::kScrollPaddingTop:
      return ValueEquals(a.ScrollPaddingTop(), b.ScrollPaddingTop());
    case CSSPropertyID::kScrollSnapAlign:
      return ValueEquals(a.GetScrollSnapAlign(), b.GetScrollSnapAlign());
    case CSSPropertyID::kScrollSnapStop:
      return ValueEquals(a.ScrollSnapStop(), b.ScrollSnapStop());
    case CSSPropertyID::kScrollSnapType:
      return ValueEquals(a.GetScrollSnapType(), b.GetScrollSnapType());
    case CSSPropertyID::kShapeImageThreshold:
      return ValueEquals(a.ShapeImageThreshold(), b.ShapeImageThreshold());
    case CSSPropertyID::kShapeMargin:
      return ValueEquals(a.ShapeMargin(), b.ShapeMargin());
    case CSSPropertyID::kShapeOutside:
      return base::ValuesEquivalent(a.ShapeOutside(), b.ShapeOutside());
    case CSSPropertyID::kShapeRendering:
      return ValueEquals(a.ShapeRendering(), b.ShapeRendering());
    case CSSPropertyID::kSizeAdjust:
      return ValueEquals(a.GetFontDescription().SizeAdjust(),
                         b.GetFontDescription().SizeAdjust());
    case CSSPropertyID::kSpeak:
      return ValueEquals(a.Speak(), b.Speak());
    case CSSPropertyID::kStopColor:
      return ValueEquals(a.StopColor(), b.StopColor());
    case CSSPropertyID::kStopOpacity:
      return ValueEquals(a.StopOpacity(), b.StopOpacity());
    case CSSPropertyID::kStroke:
      return a.StrokePaint().EqualTypeOrColor(b.StrokePaint()) &&
             a.InternalVisitedStrokePaint().EqualTypeOrColor(
                 b.InternalVisitedStrokePaint());
    case CSSPropertyID::kStrokeDasharray:
      return base::ValuesEquivalent(a.StrokeDashArray(), b.StrokeDashArray());
    case CSSPropertyID::kStrokeDashoffset:
      return ValueEquals(a.StrokeDashOffset(), b.StrokeDashOffset());
    case CSSPropertyID::kStrokeLinecap:
      return ValueEquals(a.CapStyle(), b.CapStyle());
    case CSSPropertyID::kStrokeLinejoin:
      return ValueEquals(a.JoinStyle(), b.JoinStyle());
    case CSSPropertyID::kStrokeMiterlimit:
      return ValueEquals(a.StrokeMiterLimit(), b.StrokeMiterLimit());
    case CSSPropertyID::kStrokeOpacity:
      return ValueEquals(a.StrokeOpacity(), b.StrokeOpacity());
    case CSSPropertyID::kStrokeWidth:
      return ValueEquals(a.StrokeWidth(), b.StrokeWidth());
    case CSSPropertyID::kTableLayout:
      return ValueEquals(a.TableLayout(), b.TableLayout());
    case CSSPropertyID::kTextAlign:
      return ValueEquals(a.GetTextAlign(), b.GetTextAlign());
    case CSSPropertyID::kTextAlignLast:
      return ValueEquals(a.TextAlignLast(), b.TextAlignLast());
    case CSSPropertyID::kTextAnchor:
      return ValueEquals(a.TextAnchor(), b.TextAnchor());
    case CSSPropertyID::kTextAutospace:
      return ValueEquals(a.TextAutospace(), b.TextAutospace());
    case blink::CSSPropertyID::kTextBoxEdge:
      return ValueEquals(a.GetTextBoxEdge(), b.GetTextBoxEdge());
    case blink::CSSPropertyID::kTextBoxTrim:
      return ValueEquals(a.TextBoxTrim(), b.TextBoxTrim());
    case CSSPropertyID::kTextDecorationColor:
      return ValueEquals(a.TextDecorationColor(), b.TextDecorationColor()) &&
             ValueEquals(a.InternalVisitedTextDecorationColor(),
                         b.InternalVisitedTextDecorationColor());
    case CSSPropertyID::kTextDecorationInset:
      return ValueEquals(a.GetTextDecorationInset(),
                         b.GetTextDecorationInset());
    case CSSPropertyID::kTextDecorationLine:
      return ValueEquals(a.GetTextDecorationLine(), b.GetTextDecorationLine());
    case CSSPropertyID::kTextDecorationSkipInk:
      return ValueEquals(a.TextDecorationSkipInk(), b.TextDecorationSkipInk());
    case CSSPropertyID::kTextDecorationSkipSpaces:
      return ValueEquals(a.GetTextDecorationSkipSpaces(),
                         b.GetTextDecorationSkipSpaces());
    case CSSPropertyID::kTextDecorationStyle:
      return ValueEquals(a.TextDecorationStyle(), b.TextDecorationStyle());
    case CSSPropertyID::kTextDecorationThickness:
      return ValueEquals(a.GetTextDecorationThickness(),
                         b.GetTextDecorationThickness());
    case CSSPropertyID::kTextEmphasisPosition:
      return ValueEquals(a.GetTextEmphasisPosition(),
                         b.GetTextEmphasisPosition());
    case CSSPropertyID::kTextEmphasisStyle:
      return ValueEquals(a.GetTextEmphasisFill(), b.GetTextEmphasisFill()) &&
             ValueEquals(a.GetTextEmphasisMark(), b.GetTextEmphasisMark()) &&
             ValueEquals(a.TextEmphasisCustomMark(),
                         b.TextEmphasisCustomMark());
    case CSSPropertyID::kTextFit:
      return ValueEquals(a.GetTextFit(), b.GetTextFit());
    case CSSPropertyID::kTextIndent:
      return ValueEquals(a.TextIndent(), b.TextIndent()) &&
             ValueEquals(a.GetTextIndentFlags(), b.GetTextIndentFlags());
    case CSSPropertyID::kTextJustify:
      return ValueEquals(a.GetTextJustify(), b.GetTextJustify());
    case CSSPropertyID::kTextOverflow:
      return ValueEquals(a.TextOverflow(), b.TextOverflow());
    case CSSPropertyID::kTextRendering:
      return ValueEquals(a.GetFontDescription().TextRendering(),
                         b.GetFontDescription().TextRendering());
    case CSSPropertyID::kTextShadow:
      return base::ValuesEquivalent(a.TextShadow(), b.TextShadow());
    case CSSPropertyID::kTextSizeAdjust:
      return ValueEquals(a.GetTextSizeAdjust(), b.GetTextSizeAdjust());
    case CSSPropertyID::kTextSpacingTrim:
      return ValueEquals(a.GetFontDescription().GetTextSpacingTrim(),
                         b.GetFontDescription().GetTextSpacingTrim());
    case CSSPropertyID::kTextTransform:
      return ValueEquals(a.TextTransform(), b.TextTransform());
    case CSSPropertyID::kTextUnderlineOffset:
      return ValueEquals(a.TextUnderlineOffset(), b.TextUnderlineOffset());
    case CSSPropertyID::kTextUnderlinePosition:
      return ValueEquals(a.GetTextUnderlinePosition(),
                         b.GetTextUnderlinePosition());
    case CSSPropertyID::kTextWrapMode:
      return ValueEquals(a.GetTextWrapMode(), b.GetTextWrapMode());
    case CSSPropertyID::kTextWrapStyle:
      return ValueEquals(a.GetTextWrapStyle(), b.GetTextWrapStyle());
    case CSSPropertyID::kTop:
      return ValueEquals(a.Top(), b.Top());
    case CSSPropertyID::kOverlay:
      return ValueEquals(a.Overlay(), b.Overlay());
    case CSSPropertyID::kTouchAction:
      return ValueEquals(a.GetTouchAction(), b.GetTouchAction());
    case CSSPropertyID::kTransformBox:
      return ValueEquals(a.TransformBox(), b.TransformBox());
    case CSSPropertyID::kTransformStyle:
      return ValueEquals(a.TransformStyle3D(), b.TransformStyle3D());
    case CSSPropertyID::kTriggerScope:
      return ValueEquals(a.TriggerScope(), b.TriggerScope());
    case CSSPropertyID::kUserSelect:
      return ValueEquals(a.UserSelect(), b.UserSelect());
    case CSSPropertyID::kVectorEffect:
      return ValueEquals(a.VectorEffect(), b.VectorEffect());
    case CSSPropertyID::kVerticalAlign:
      return ValueEquals(a.VerticalAlign(), b.VerticalAlign()) &&
             (a.VerticalAlign() != EVerticalAlign::kLength ||
              ValueEquals(a.GetVerticalAlignLength(),
                          b.GetVerticalAlignLength()));
    case CSSPropertyID::kViewTransitionClass:
      return base::ValuesEquivalent(a.ViewTransitionClass(),
                                    b.ViewTransitionClass());
    case CSSPropertyID::kViewTransitionGroup:
      return ValueEquals(a.ViewTransitionGroup(), b.ViewTransitionGroup());
    case CSSPropertyID::kViewTransitionName:
      return base::ValuesEquivalent(a.ViewTransitionName(),
                                    b.ViewTransitionName());
    case CSSPropertyID::kViewTransitionScope:
      return ValueEquals(a.ViewTransitionScope(), b.ViewTransitionScope());
    case CSSPropertyID::kVisibility:
      return ValueEquals(a.Visibility(), b.Visibility());
    case CSSPropertyID::kWebkitBorderHorizontalSpacing:
      return ValueEquals(a.HorizontalBorderSpacing(),
                         b.HorizontalBorderSpacing());
    case CSSPropertyID::kWebkitBorderVerticalSpacing:
      return ValueEquals(a.VerticalBorderSpacing(), b.VerticalBorderSpacing());
    case CSSPropertyID::kClipPath:
      return base::ValuesEquivalent(a.ClipPath(), b.ClipPath());
    case CSSPropertyID::kColumnCount:
      return ValueEquals(a.ColumnCount(), b.ColumnCount());
    case CSSPropertyID::kColumnGap:
      return ValueEquals(a.ColumnGap(), b.ColumnGap());
    case CSSPropertyID::kRowGap:
      return ValueEquals(a.RowGap(), b.RowGap());
    case CSSPropertyID::kRuleOverlap:
      return ValueEquals(a.RuleOverlap(), b.RuleOverlap());
    case CSSPropertyID::kColumnRuleBreak:
      return ValueEquals(a.ColumnRuleBreak(), b.ColumnRuleBreak());
    case CSSPropertyID::kRowRuleBreak:
      return ValueEquals(a.RowRuleBreak(), b.RowRuleBreak());
    case CSSPropertyID::kColumnRuleInsetCapEnd:
      return ValueEquals(a.ColumnRuleInsetCapEnd(), b.ColumnRuleInsetCapEnd());
    case CSSPropertyID::kRowRuleInsetCapEnd:
      return ValueEquals(a.RowRuleInsetCapEnd(), b.RowRuleInsetCapEnd());
    case CSSPropertyID::kColumnRuleInsetCapStart:
      return ValueEquals(a.ColumnRuleInsetCapStart(),
                         b.ColumnRuleInsetCapStart());
    case CSSPropertyID::kRowRuleInsetCapStart:
      return ValueEquals(a.RowRuleInsetCapStart(), b.RowRuleInsetCapStart());
    case CSSPropertyID::kColumnRuleInsetJunctionEnd:
      return ValueEquals(a.ColumnRuleInsetJunctionEnd(),
                         b.ColumnRuleInsetJunctionEnd());
    case CSSPropertyID::kRowRuleInsetJunctionEnd:
      return ValueEquals(a.RowRuleInsetJunctionEnd(),
                         b.RowRuleInsetJunctionEnd());
    case CSSPropertyID::kColumnRuleInsetJunctionStart:
      return ValueEquals(a.ColumnRuleInsetJunctionStart(),
                         b.ColumnRuleInsetJunctionStart());
    case CSSPropertyID::kRowRuleInsetJunctionStart:
      return ValueEquals(a.RowRuleInsetJunctionStart(),
                         b.RowRuleInsetJunctionStart());
    case CSSPropertyID::kColumnRuleColor:
      return ValueEquals(a.ColumnRuleColor(), b.ColumnRuleColor()) &&
             ValueEquals(a.InternalVisitedColumnRuleColor(),
                         b.InternalVisitedColumnRuleColor());
    case CSSPropertyID::kRowRuleColor:
      return ValueEquals(a.RowRuleColor(), b.RowRuleColor());
    case CSSPropertyID::kColumnRuleVisibilityItems:
      return ValueEquals(a.ColumnRuleVisibilityItems(),
                         b.ColumnRuleVisibilityItems());
    case CSSPropertyID::kRowRuleVisibilityItems:
      return ValueEquals(a.RowRuleVisibilityItems(),
                         b.RowRuleVisibilityItems());
    case CSSPropertyID::kColumnRuleWidth:
      return ValueEquals(a.ColumnRuleWidth(), b.ColumnRuleWidth());
    case CSSPropertyID::kRowRuleWidth:
      return ValueEquals(a.RowRuleWidth(), b.RowRuleWidth());
    case CSSPropertyID::kColumnWidth:
      return ValueEquals(a.ColumnWidth(), b.ColumnWidth());
    case CSSPropertyID::kColumnHeight:
      return ValueEquals(a.ColumnHeight(), b.ColumnHeight());
    case CSSPropertyID::kColumnWrap:
      return ValueEquals(a.ColumnWrap(), b.ColumnWrap());
    case CSSPropertyID::kFilter:
      return ValueEquals(a.Filter(), b.Filter());
    case CSSPropertyID::kBackdropFilter:
      return ValueEquals(a.BackdropFilter(), b.BackdropFilter());
    case CSSPropertyID::kWebkitFontSmoothing:
      return ValueEquals(a.GetFontDescription().FontSmoothing(),
                         b.GetFontDescription().FontSmoothing());
    case CSSPropertyID::kWebkitLineClamp:
      return ValueEquals(a.WebkitLineClamp(), b.WebkitLineClamp());
    case CSSPropertyID::kWebkitLocale:
      return ValueEquals(a.Locale(), b.Locale());
    case CSSPropertyID::kWebkitMaskBoxImageOutset:
      return ValueEquals(a.MaskBoxImageOutset(), b.MaskBoxImageOutset());
    case CSSPropertyID::kWebkitMaskBoxImageSlice:
      return ValueEquals(a.MaskBoxImageSlices(), b.MaskBoxImageSlices());
    case CSSPropertyID::kWebkitMaskBoxImageSource:
      return base::ValuesEquivalent(a.MaskBoxImageSource(),
                                    b.MaskBoxImageSource());
    case CSSPropertyID::kWebkitMaskBoxImageWidth:
      return ValueEquals(a.MaskBoxImageWidth(), b.MaskBoxImageWidth());
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
      return ValueEquals(a.TextFillColor(), b.TextFillColor());
    case CSSPropertyID::kWebkitTextOrientation:
      return ValueEquals(a.GetTextOrientation(), b.GetTextOrientation());
    case CSSPropertyID::kPerspective:
      return ValueEquals(a.Perspective(), b.Perspective());
    case CSSPropertyID::kPerspectiveOrigin:
      return ValueEquals(a.PerspectiveOrigin(), b.PerspectiveOrigin());
    case CSSPropertyID::kWebkitTextStrokeColor:
      return ValueEquals(a.TextStrokeColor(), b.TextStrokeColor()) &&
             ValueEquals(a.InternalVisitedTextStrokeColor(),
                         b.InternalVisitedTextStrokeColor());
    case CSSPropertyID::kWebkitTextStrokeWidth:
      return ValueEquals(a.TextStrokeWidth(), b.TextStrokeWidth());
    case CSSPropertyID::kTransform:
      return ValueEquals(a.Transform(), b.Transform());
    case CSSPropertyID::kTranslate:
      return base::ValuesEquivalent<TransformOperation>(a.Translate(),
                                                        b.Translate());
    case CSSPropertyID::kRotate:
      return base::ValuesEquivalent<TransformOperation>(a.Rotate(), b.Rotate());
    case CSSPropertyID::kScale:
      return base::ValuesEquivalent<TransformOperation>(a.Scale(), b.Scale());
    case CSSPropertyID::kSize:
      return ValueEquals(a.GetPageSizeType(), b.GetPageSizeType()) &&
             ValueEquals(a.PageSize(), b.PageSize());
    case CSSPropertyID::kTransformOrigin:
      return ValueEquals(a.GetTransformOrigin(), b.GetTransformOrigin());
    case CSSPropertyID::kWebkitPerspectiveOriginX:
      return ValueEquals(a.PerspectiveOrigin().X(), b.PerspectiveOrigin().X());
    case CSSPropertyID::kWebkitPerspectiveOriginY:
      return ValueEquals(a.PerspectiveOrigin().Y(), b.PerspectiveOrigin().Y());
    case CSSPropertyID::kWebkitTransformOriginX:
      return ValueEquals(a.GetTransformOrigin().X(),
                         b.GetTransformOrigin().X());
    case CSSPropertyID::kWebkitTransformOriginY:
      return ValueEquals(a.GetTransformOrigin().Y(),
                         b.GetTransformOrigin().Y());
    case CSSPropertyID::kWebkitTransformOriginZ:
      return ValueEquals(a.GetTransformOrigin().Z(),
                         b.GetTransformOrigin().Z());
    case CSSPropertyID::kWhiteSpaceCollapse:
      return ValueEquals(a.GetWhiteSpaceCollapse(), b.GetWhiteSpaceCollapse());
    case CSSPropertyID::kWidows:
      return ValueEquals(a.Widows(), b.Widows());
    case CSSPropertyID::kWidth:
      return ValueEquals(a.Width(), b.Width());
    case CSSPropertyID::kWordBreak:
      return ValueEquals(a.WordBreak(), b.WordBreak());
    case CSSPropertyID::kWordSpacing:
      return ValueEquals(a.ComputedWordSpacing(), b.ComputedWordSpacing());
    case CSSPropertyID::kD:
      return base::ValuesEquivalent(a.D(), b.D());
    case CSSPropertyID::kPathLength:
      return ValueEquals(a.PathLength(), b.PathLength());
    case CSSPropertyID::kCx:
      return ValueEquals(a.Cx(), b.Cx());
    case CSSPropertyID::kCy:
      return ValueEquals(a.Cy(), b.Cy());
    case CSSPropertyID::kX:
      return ValueEquals(a.X(), b.X());
    case CSSPropertyID::kY:
      return ValueEquals(a.Y(), b.Y());
    case CSSPropertyID::kR:
      return ValueEquals(a.R(), b.R());
    case CSSPropertyID::kRx:
      return ValueEquals(a.Rx(), b.Rx());
    case CSSPropertyID::kRy:
      return ValueEquals(a.Ry(), b.Ry());
    case CSSPropertyID::kZIndex:
      return ValueEquals(a.HasAutoZIndex(), b.HasAutoZIndex()) &&
             (a.HasAutoZIndex() || ValueEquals(a.ZIndex(), b.ZIndex()));
    case CSSPropertyID::kContainIntrinsicWidth:
      return ValueEquals(a.ContainIntrinsicWidth(), b.ContainIntrinsicWidth());
    case CSSPropertyID::kContainIntrinsicHeight:
      return ValueEquals(a.ContainIntrinsicHeight(),
                         b.ContainIntrinsicHeight());
    case CSSPropertyID::kAspectRatio:
      return ValueEquals(a.AspectRatio(), b.AspectRatio());
    case CSSPropertyID::kMathDepth:
      return ValueEquals(a.MathDepth(), b.MathDepth());
    case CSSPropertyID::kAccentColor:
      return ValueEquals(a.AccentColor(), b.AccentColor());
    case CSSPropertyID::kTextEmphasisColor:
      return ValueEquals(a.TextEmphasisColor(), b.TextEmphasisColor());
    case CSSPropertyID::kZoom:
      return ValueEquals(a.Zoom(), b.Zoom());
    case CSSPropertyID::kPositionTryOrder:
      return ValueEquals(a.PositionTryOrder(), b.PositionTryOrder());
    case CSSPropertyID::kPositionTryFallbacks:
      return base::ValuesEquivalent(a.GetPositionTryFallbacks(),
                                    b.GetPositionTryFallbacks());
    case CSSPropertyID::kPositionVisibility:
      return ValueEquals(a.GetPositionVisibility(), b.GetPositionVisibility());
    case CSSPropertyID::kPrintColorAdjust:
      return ValueEquals(a.PrintColorAdjust(), b.PrintColorAdjust());

    // These properties are not animateable, but perhaps equality should still
    // be defined for them.
    case CSSPropertyID::kAnimationTrigger:
    case CSSPropertyID::kScrollTimelineAxis:
    case CSSPropertyID::kScrollTimelineName:
    case CSSPropertyID::kTimelineTriggerName:
    case CSSPropertyID::kTimelineTriggerActivationRangeStart:
    case CSSPropertyID::kTimelineTriggerActivationRangeEnd:
    case CSSPropertyID::kTimelineTriggerActiveRangeStart:
    case CSSPropertyID::kTimelineTriggerActiveRangeEnd:
    case CSSPropertyID::kTimelineTriggerSource:
    case CSSPropertyID::kViewTimelineAxis:
    case CSSPropertyID::kViewTimelineInset:
    case CSSPropertyID::kViewTimelineName:
      NOTREACHED() << property.GetCSSPropertyName().ToAtomicString().Ascii();

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
    case CSSPropertyID::kAliasWebkitPrintColorAdjust:
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
      NOTREACHED()
          << "Aliases CSS properties should be converted to their non-aliased "
             "counterpart before calling this function. CSS property name: "
          << property.GetCSSPropertyName().ToAtomicString().Ascii();

    // Webkit prefixed properties which don't have non-aliased counterparts.
    // TODO(crbug.com/40919412): Implement comparison for these properties. They
    // are reachable via transitions now.
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
    case CSSPropertyID::kCornerStartStartShape:
    case CSSPropertyID::kCornerStartEndShape:
    case CSSPropertyID::kCornerEndStartShape:
    case CSSPropertyID::kCornerEndEndShape:
    case CSSPropertyID::kInsetInlineStart:
    case CSSPropertyID::kInsetInlineEnd:
    case CSSPropertyID::kInsetBlockStart:
    case CSSPropertyID::kInsetBlockEnd:
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
      NOTREACHED() << property.GetCSSPropertyName().ToAtomicString().Ascii();

    // No transitions on internal properties:
    case CSSPropertyID::kInternalAlignContentBlock:
    case CSSPropertyID::kInternalEmptyLineHeight:
    case CSSPropertyID::kInternalFontSizeDelta:
    case CSSPropertyID::kInternalForcedBackgroundColor:
    case CSSPropertyID::kInternalForcedBorderColor:
    case CSSPropertyID::kInternalForcedColor:
    case CSSPropertyID::kInternalForcedOutlineColor:
    case CSSPropertyID::kInternalForcedVisitedColor:
    case CSSPropertyID::kInternalOverscrollContainer:
    case CSSPropertyID::kInternalOverscrollPosition:
    case CSSPropertyID::kInternalUnbounded:
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
      NOTREACHED() << property.GetCSSPropertyName().ToAtomicString().Ascii();

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
    case CSSPropertyID::kColumnRuleInset:
    case CSSPropertyID::kColumnRuleInsetCap:
    case CSSPropertyID::kColumnRuleInsetEnd:
    case CSSPropertyID::kColumnRuleInsetJunction:
    case CSSPropertyID::kColumnRuleInsetStart:
    case CSSPropertyID::kColumns:
    case CSSPropertyID::kContainIntrinsicSize:
    case CSSPropertyID::kContainer:
    case CSSPropertyID::kCorner:
    case CSSPropertyID::kCornerTopLeft:
    case CSSPropertyID::kCornerTopRight:
    case CSSPropertyID::kCornerBottomLeft:
    case CSSPropertyID::kCornerBottomRight:
    case CSSPropertyID::kCornerStartStart:
    case CSSPropertyID::kCornerStartEnd:
    case CSSPropertyID::kCornerEndStart:
    case CSSPropertyID::kCornerEndEnd:
    case CSSPropertyID::kCornerTop:
    case CSSPropertyID::kCornerRight:
    case CSSPropertyID::kCornerBottom:
    case CSSPropertyID::kCornerLeft:
    case CSSPropertyID::kCornerInlineStart:
    case CSSPropertyID::kCornerInlineEnd:
    case CSSPropertyID::kCornerBlockStart:
    case CSSPropertyID::kCornerBlockEnd:
    case CSSPropertyID::kCornerShape:
    case CSSPropertyID::kCornerTopShape:
    case CSSPropertyID::kCornerRightShape:
    case CSSPropertyID::kCornerBottomShape:
    case CSSPropertyID::kCornerLeftShape:
    case CSSPropertyID::kCornerInlineStartShape:
    case CSSPropertyID::kCornerInlineEndShape:
    case CSSPropertyID::kCornerBlockStartShape:
    case CSSPropertyID::kCornerBlockEndShape:
    case CSSPropertyID::kInset:
    case CSSPropertyID::kInterestDelay:
    case CSSPropertyID::kFlex:
    case CSSPropertyID::kFlexFlow:
    case CSSPropertyID::kFont:
    case CSSPropertyID::kFontSynthesis:
    case CSSPropertyID::kFontVariant:
    case CSSPropertyID::kGap:
    case CSSPropertyID::kGrid:
    case CSSPropertyID::kGridArea:
    case CSSPropertyID::kGridColumn:
    case CSSPropertyID::kGridLanes:
    case CSSPropertyID::kGridRow:
    case CSSPropertyID::kGridTemplate:
    case CSSPropertyID::kAlternativeLineClampShorthand:
    case CSSPropertyID::kListStyle:
    case CSSPropertyID::kMargin:
    case CSSPropertyID::kMarker:
    case CSSPropertyID::kMask:
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
    case CSSPropertyID::kRowRule:
    case CSSPropertyID::kRowRuleInset:
    case CSSPropertyID::kRowRuleInsetCap:
    case CSSPropertyID::kRowRuleInsetEnd:
    case CSSPropertyID::kRowRuleInsetJunction:
    case CSSPropertyID::kRowRuleInsetStart:
    case CSSPropertyID::kRuleInsetCap:
    case CSSPropertyID::kRuleInsetEnd:
    case CSSPropertyID::kRuleInsetJunction:
    case CSSPropertyID::kRuleInsetStart:
    case CSSPropertyID::kRule:
    case CSSPropertyID::kRuleBreak:
    case CSSPropertyID::kRuleColor:
    case CSSPropertyID::kRuleInset:
    case CSSPropertyID::kRuleVisibilityItems:
    case CSSPropertyID::kRuleWidth:
    case CSSPropertyID::kRuleStyle:
    case CSSPropertyID::kScrollMargin:
    case CSSPropertyID::kScrollPadding:
    case CSSPropertyID::kScrollTimeline:
    case CSSPropertyID::kTextBox:
    case CSSPropertyID::kTextDecoration:
    case CSSPropertyID::kTextEmphasis:
    case CSSPropertyID::kTextSpacing:
    case CSSPropertyID::kTextWrap:
    case CSSPropertyID::kTimelineTrigger:
    case CSSPropertyID::kTimelineTriggerActivationRange:
    case CSSPropertyID::kTimelineTriggerActiveRange:
    case CSSPropertyID::kTransition:
    case CSSPropertyID::kViewTimeline:
    case CSSPropertyID::kWebkitColumnBreakAfter:
    case CSSPropertyID::kWebkitColumnBreakBefore:
    case CSSPropertyID::kWebkitColumnBreakInside:
    case CSSPropertyID::kAlternativeWebkitLineClampShorthand:
    case CSSPropertyID::kWebkitMaskBoxImage:
    case CSSPropertyID::kMaskPosition:
    case CSSPropertyID::kWebkitTextStroke:
    case CSSPropertyID::kWhiteSpace:
      NOTREACHED() << property.GetCSSPropertyName().ToAtomicString().Ascii();

    // Non-animateable properties
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
      NOTREACHED() << property.GetCSSPropertyName().ToAtomicString().Ascii();

    // CSSPropertyIDs which are descriptors only
    case CSSPropertyID::kAdditiveSymbols:
    case CSSPropertyID::kAscentOverride:
    case CSSPropertyID::kBasePalette:
    case CSSPropertyID::kBaseUrl:
    case CSSPropertyID::kDescentOverride:
    case CSSPropertyID::kFallback:
    case CSSPropertyID::kFontDisplay:
    case CSSPropertyID::kHash:
    case CSSPropertyID::kHostname:
    case CSSPropertyID::kInherits:
    case CSSPropertyID::kInitialValue:
    case CSSPropertyID::kLineGapOverride:
    case CSSPropertyID::kNavigation:
    case CSSPropertyID::kNegative:
    case CSSPropertyID::kOverrideColors:
    case CSSPropertyID::kPad:
    case CSSPropertyID::kPathname:
    case CSSPropertyID::kPattern:
    case CSSPropertyID::kPort:
    case CSSPropertyID::kPrefix:
    case CSSPropertyID::kProtocol:
    case CSSPropertyID::kRange:
    case CSSPropertyID::kResult:
    case CSSPropertyID::kSearch:
    case CSSPropertyID::kSpeakAs:
    case CSSPropertyID::kSrc:
    case CSSPropertyID::kSuffix:
    case CSSPropertyID::kSymbols:
    case CSSPropertyID::kSyntax:
    case CSSPropertyID::kSystem:
    case CSSPropertyID::kTypes:
    case CSSPropertyID::kUnicodeRange:
    // Invalid properties
    case CSSPropertyID::kAll:
    case CSSPropertyID::kInvalid:
    case CSSPropertyID::kVariable:
      NOTREACHED() << property.GetCSSPropertyName().ToAtomicString().Ascii();
  }
}

}  // namespace blink
