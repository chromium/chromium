/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/css/resolver/style_builder_converter.h"

#include <algorithm>

#include "build/build_config.h"
#include "third_party/blink/renderer/core/css/basic_shape_functions.h"
#include "third_party/blink/renderer/core/css/css_color_value.h"
#include "third_party/blink/renderer/core/css/css_content_distribution_value.h"
#include "third_party/blink/renderer/core/css/css_custom_ident_value.h"
#include "third_party/blink/renderer/core/css/css_font_family_value.h"
#include "third_party/blink/renderer/core/css/css_font_feature_value.h"
#include "third_party/blink/renderer/core/css/css_font_style_range_value.h"
#include "third_party/blink/renderer/core/css/css_font_variation_value.h"
#include "third_party/blink/renderer/core/css/css_grid_auto_repeat_value.h"
#include "third_party/blink/renderer/core/css/css_path_value.h"
#include "third_party/blink/renderer/core/css/css_primitive_value_mappings.h"
#include "third_party/blink/renderer/core/css/css_quad_value.h"
#include "third_party/blink/renderer/core/css/css_reflect_value.h"
#include "third_party/blink/renderer/core/css/css_shadow_value.h"
#include "third_party/blink/renderer/core/css/css_uri_value.h"
#include "third_party/blink/renderer/core/css/resolver/filter_operation_resolver.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver_state.h"
#include "third_party/blink/renderer/core/css/resolver/transform_builder.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/use_counter.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/core/style/reference_clip_path_operation.h"
#include "third_party/blink/renderer/core/style/shape_clip_path_operation.h"
#include "third_party/blink/renderer/core/style/style_svg_resource.h"

namespace blink {

using namespace cssvalue;

namespace {

static GridLength ConvertGridTrackBreadth(const StyleResolverState& state,
                                          const CSSValue& value) {
  // Fractional unit.
  if (value.IsPrimitiveValue() && ToCSSPrimitiveValue(value).IsFlex())
    return GridLength(ToCSSPrimitiveValue(value).GetDoubleValue());

  if (value.IsIdentifierValue() &&
      ToCSSIdentifierValue(value).GetValueID() == CSSValueMinContent)
    return Length(kMinContent);

  if (value.IsIdentifierValue() &&
      ToCSSIdentifierValue(value).GetValueID() == CSSValueMaxContent)
    return Length(kMaxContent);

  return StyleBuilderConverter::ConvertLengthOrAuto(state, value);
}

}  // namespace

scoped_refptr<StyleReflection> StyleBuilderConverter::ConvertBoxReflect(
    StyleResolverState& state,
    const CSSValue& value) {
  if (value.IsIdentifierValue()) {
    DCHECK_EQ(ToCSSIdentifierValue(value).GetValueID(), CSSValueNone);
    return ComputedStyleInitialValues::InitialBoxReflect();
  }

  const CSSReflectValue& reflect_value = ToCSSReflectValue(value);
  scoped_refptr<StyleReflection> reflection = StyleReflection::Create();
  reflection->SetDirection(
      reflect_value.Direction()->ConvertTo<CSSReflectionDirection>());
  if (reflect_value.Offset())
    reflection->SetOffset(reflect_value.Offset()->ConvertToLength(
        state.CssToLengthConversionData()));
  if (reflect_value.Mask()) {
    NinePieceImage mask = NinePieceImage::MaskDefaults();
    CSSToStyleMap::MapNinePieceImage(state, CSSPropertyWebkitBoxReflect,
                                     *reflect_value.Mask(), mask);
    reflection->SetMask(mask);
  }

  return reflection;
}

Color StyleBuilderConverter::ConvertColor(StyleResolverState& state,
                                          const CSSValue& value,
                                          bool for_visited_link) {
  return state.GetDocument().GetTextLinkColors().ColorFromCSSValue(
      value, state.Style()->GetColor(), for_visited_link);
}

scoped_refptr<StyleSVGResource> StyleBuilderConverter::ConvertElementReference(
    StyleResolverState& state,
    const CSSValue& value) {
  if (!value.IsURIValue())
    return nullptr;
  const CSSURIValue& url_value = ToCSSURIValue(value);
  SVGResource* resource =
      state.GetElementStyleResources().GetSVGResourceFromValue(
          state.GetTreeScope(), url_value);
  return StyleSVGResource::Create(resource, url_value.ValueForSerialization());
}

LengthBox StyleBuilderConverter::ConvertClip(StyleResolverState& state,
                                             const CSSValue& value) {
  const CSSQuadValue& rect = ToCSSQuadValue(value);

  return LengthBox(ConvertLengthOrAuto(state, *rect.Top()),
                   ConvertLengthOrAuto(state, *rect.Right()),
                   ConvertLengthOrAuto(state, *rect.Bottom()),
                   ConvertLengthOrAuto(state, *rect.Left()));
}

scoped_refptr<ClipPathOperation> StyleBuilderConverter::ConvertClipPath(
    StyleResolverState& state,
    const CSSValue& value) {
  if (value.IsBasicShapeValue())
    return ShapeClipPathOperation::Create(BasicShapeForValue(state, value));
  if (value.IsURIValue()) {
    const CSSURIValue& url_value = ToCSSURIValue(value);
    SVGResource* resource =
        state.GetElementStyleResources().GetSVGResourceFromValue(
            state.GetTreeScope(), url_value);
    // TODO(fs): Doesn't work with external SVG references (crbug.com/109212.)
    return ReferenceClipPathOperation::Create(url_value.ValueForSerialization(),
                                              resource);
  }
  DCHECK(value.IsIdentifierValue() &&
         ToCSSIdentifierValue(value).GetValueID() == CSSValueNone);
  return nullptr;
}

FilterOperations StyleBuilderConverter::ConvertFilterOperations(
    StyleResolverState& state,
    const CSSValue& value) {
  return FilterOperationResolver::CreateFilterOperations(state, value);
}

FilterOperations StyleBuilderConverter::ConvertOffscreenFilterOperations(
    const CSSValue& value,
    const Font& font) {
  return FilterOperationResolver::CreateOffscreenFilterOperations(value, font);
}

static FontDescription::GenericFamilyType ConvertGenericFamily(
    CSSValueID value_id) {
  switch (value_id) {
    case CSSValueWebkitBody:
      return FontDescription::kStandardFamily;
    case CSSValueSerif:
      return FontDescription::kSerifFamily;
    case CSSValueSansSerif:
      return FontDescription::kSansSerifFamily;
    case CSSValueCursive:
      return FontDescription::kCursiveFamily;
    case CSSValueFantasy:
      return FontDescription::kFantasyFamily;
    case CSSValueMonospace:
      return FontDescription::kMonospaceFamily;
    case CSSValueWebkitPictograph:
      return FontDescription::kPictographFamily;
    default:
      return FontDescription::kNoFamily;
  }
}

static bool ConvertFontFamilyName(
    const CSSValue& value,
    FontDescription::GenericFamilyType& generic_family,
    AtomicString& family_name,
    FontBuilder* font_builder,
    const Document* document_for_count) {
  if (value.IsFontFamilyValue()) {
    generic_family = FontDescription::kNoFamily;
    family_name = AtomicString(ToCSSFontFamilyValue(value).Value());
#if defined(OS_MACOSX)
    if (family_name == FontCache::LegacySystemFontFamily()) {
      UseCounter::Count(*document_for_count, WebFeature::kBlinkMacSystemFont);
      family_name = FontFamilyNames::system_ui;
    }
#endif
  } else if (font_builder) {
    generic_family =
        ConvertGenericFamily(ToCSSIdentifierValue(value).GetValueID());
    family_name = font_builder->GenericFontFamilyName(generic_family);
  }

  return !family_name.IsEmpty();
}

FontDescription::FamilyDescription StyleBuilderConverterBase::ConvertFontFamily(
    const CSSValue& value,
    FontBuilder* font_builder,
    const Document* document_for_count) {
  DCHECK(value.IsValueList());

  FontDescription::FamilyDescription desc(FontDescription::kNoFamily);
  FontFamily* curr_family = nullptr;

  for (auto& family : ToCSSValueList(value)) {
    FontDescription::GenericFamilyType generic_family =
        FontDescription::kNoFamily;
    AtomicString family_name;

    if (!ConvertFontFamilyName(*family, generic_family, family_name,
                               font_builder, document_for_count))
      continue;

    if (!curr_family) {
      curr_family = &desc.family;
    } else {
      scoped_refptr<SharedFontFamily> new_family = SharedFontFamily::Create();
      curr_family->AppendFamily(new_family);
      curr_family = new_family.get();
    }

    curr_family->SetFamily(family_name);

    if (generic_family != FontDescription::kNoFamily)
      desc.generic_family = generic_family;
  }

  return desc;
}

FontDescription::FamilyDescription StyleBuilderConverter::ConvertFontFamily(
    StyleResolverState& state,
    const CSSValue& value) {
  return StyleBuilderConverterBase::ConvertFontFamily(
      value,
      state.GetDocument().GetSettings() ? &state.GetFontBuilder() : nullptr,
      &state.GetDocument());
}

scoped_refptr<FontFeatureSettings>
StyleBuilderConverter::ConvertFontFeatureSettings(StyleResolverState& state,
                                                  const CSSValue& value) {
  if (value.IsIdentifierValue() &&
      ToCSSIdentifierValue(value).GetValueID() == CSSValueNormal)
    return FontBuilder::InitialFeatureSettings();

  const CSSValueList& list = ToCSSValueList(value);
  scoped_refptr<FontFeatureSettings> settings = FontFeatureSettings::Create();
  int len = list.length();
  for (int i = 0; i < len; ++i) {
    const cssvalue::CSSFontFeatureValue& feature =
        ToCSSFontFeatureValue(list.Item(i));
    settings->Append(FontFeature(feature.Tag(), feature.Value()));
  }
  return settings;
}

scoped_refptr<FontVariationSettings>
StyleBuilderConverter::ConvertFontVariationSettings(StyleResolverState& state,
                                                    const CSSValue& value) {
  if (value.IsIdentifierValue() &&
      ToCSSIdentifierValue(value).GetValueID() == CSSValueNormal)
    return FontBuilder::InitialVariationSettings();

  const CSSValueList& list = ToCSSValueList(value);
  scoped_refptr<FontVariationSettings> settings =
      FontVariationSettings::Create();
  int len = list.length();
  for (int i = 0; i < len; ++i) {
    const CSSFontVariationValue& feature =
        ToCSSFontVariationValue(list.Item(i));
    settings->Append(FontVariationAxis(feature.Tag(), feature.Value()));
  }
  return settings;
}

static float ComputeFontSize(const CSSToLengthConversionData& conversion_data,
                             const CSSPrimitiveValue& primitive_value,
                             const FontDescription::Size& parent_size) {
  if (primitive_value.IsLength())
    return primitive_value.ComputeLength<float>(conversion_data);
  if (primitive_value.IsCalculatedPercentageWithLength())
    return primitive_value.CssCalcValue()
        ->ToCalcValue(conversion_data)
        ->Evaluate(parent_size.value);

  NOTREACHED();
  return 0;
}

FontDescription::Size StyleBuilderConverterBase::ConvertFontSize(
    const CSSValue& value,
    const CSSToLengthConversionData& conversion_data,
    FontDescription::Size parent_size) {
  if (value.IsIdentifierValue()) {
    CSSValueID value_id = ToCSSIdentifierValue(value).GetValueID();
    if (FontSizeFunctions::IsValidValueID(value_id)) {
      return FontDescription::Size(FontSizeFunctions::KeywordSize(value_id),
                                   0.0f, false);
    }
    if (value_id == CSSValueSmaller)
      return FontDescription::SmallerSize(parent_size);
    if (value_id == CSSValueLarger)
      return FontDescription::LargerSize(parent_size);
    NOTREACHED();
    return FontBuilder::InitialSize();
  }

  const CSSPrimitiveValue& primitive_value = ToCSSPrimitiveValue(value);
  if (primitive_value.IsPercentage()) {
    return FontDescription::Size(
        0, (primitive_value.GetFloatValue() * parent_size.value / 100.0f),
        parent_size.is_absolute);
  }

  return FontDescription::Size(
      0, ComputeFontSize(conversion_data, primitive_value, parent_size),
      parent_size.is_absolute || !primitive_value.IsFontRelativeLength());
}

FontDescription::Size StyleBuilderConverter::ConvertFontSize(
    StyleResolverState& state,
    const CSSValue& value) {
  return StyleBuilderConverterBase::ConvertFontSize(
      value, state.FontSizeConversionData(),
      // FIXME: Find out when parentStyle could be 0?
      state.ParentStyle() ? state.ParentFontDescription().GetSize()
                          : FontDescription::Size(0, 0.0f, false));
}

float StyleBuilderConverter::ConvertFontSizeAdjust(StyleResolverState& state,
                                                   const CSSValue& value) {
  if (value.IsIdentifierValue() &&
      ToCSSIdentifierValue(value).GetValueID() == CSSValueNone)
    return FontBuilder::InitialSizeAdjust();

  const CSSPrimitiveValue& primitive_value = ToCSSPrimitiveValue(value);
  DCHECK(primitive_value.IsNumber());
  return primitive_value.GetFloatValue();
}

double StyleBuilderConverter::ConvertValueToNumber(
    const CSSFunctionValue* filter,
    const CSSPrimitiveValue* value) {
  switch (filter->FunctionType()) {
    case CSSValueGrayscale:
    case CSSValueSepia:
    case CSSValueSaturate:
    case CSSValueInvert:
    case CSSValueBrightness:
    case CSSValueContrast:
    case CSSValueOpacity: {
      double amount = (filter->FunctionType() == CSSValueBrightness ||
                       filter->FunctionType() == CSSValueInvert)
                          ? 0
                          : 1;
      if (filter->length() == 1) {
        amount = value->GetDoubleValue();
        if (value->IsPercentage())
          amount /= 100;
      }
      return amount;
    }
    case CSSValueHueRotate: {
      double angle = 0;
      if (filter->length() == 1)
        angle = value->ComputeDegrees();
      return angle;
    }
    default:
      return 0;
  }
}

FontSelectionValue StyleBuilderConverterBase::ConvertFontStretch(
    const blink::CSSValue& value) {
  if (value.IsPrimitiveValue()) {
    const CSSPrimitiveValue& primitive_value = ToCSSPrimitiveValue(value);
    if (primitive_value.IsPercentage())
      return clampTo<FontSelectionValue>(primitive_value.GetFloatValue());
  }

  // TODO(drott) crbug.com/750014: Consider not parsing them as IdentifierValue
  // any more?
  if (value.IsIdentifierValue()) {
    const CSSIdentifierValue& identifier_value = ToCSSIdentifierValue(value);
    switch (identifier_value.GetValueID()) {
      case CSSValueUltraCondensed:
        return UltraCondensedWidthValue();
      case CSSValueExtraCondensed:
        return ExtraCondensedWidthValue();
      case CSSValueCondensed:
        return CondensedWidthValue();
      case CSSValueSemiCondensed:
        return SemiCondensedWidthValue();
      case CSSValueNormal:
        return NormalWidthValue();
      case CSSValueSemiExpanded:
        return SemiExpandedWidthValue();
      case CSSValueExpanded:
        return ExpandedWidthValue();
      case CSSValueExtraExpanded:
        return ExtraExpandedWidthValue();
      case CSSValueUltraExpanded:
        return UltraExpandedWidthValue();
      default:
        break;
    }
  }
  NOTREACHED();
  return NormalWidthValue();
}

FontSelectionValue StyleBuilderConverter::ConvertFontStretch(
    blink::StyleResolverState& state,
    const blink::CSSValue& value) {
  return StyleBuilderConverterBase::ConvertFontStretch(value);
}

FontSelectionValue StyleBuilderConverterBase::ConvertFontStyle(
    const CSSValue& value) {
  DCHECK(!value.IsPrimitiveValue());

  if (value.IsIdentifierValue()) {
    const CSSIdentifierValue& identifier_value = ToCSSIdentifierValue(value);
    switch (identifier_value.GetValueID()) {
      case CSSValueItalic:
      case CSSValueOblique:
        return ItalicSlopeValue();
      case CSSValueNormal:
        return NormalSlopeValue();
      default:
        NOTREACHED();
        return NormalSlopeValue();
    }
  } else if (value.IsFontStyleRangeValue()) {
    const CSSFontStyleRangeValue& style_range_value =
        ToCSSFontStyleRangeValue(value);
    const CSSValueList* values = style_range_value.GetObliqueValues();
    CHECK_LT(values->length(), 2u);
    if (values->length()) {
      return FontSelectionValue(
          ToCSSPrimitiveValue(values->Item(0)).GetFloatValue());
    } else {
      const CSSIdentifierValue* identifier_value =
          style_range_value.GetFontStyleValue();
      if (identifier_value->GetValueID() == CSSValueNormal)
        return NormalSlopeValue();
      if (identifier_value->GetValueID() == CSSValueItalic ||
          identifier_value->GetValueID() == CSSValueOblique)
        return ItalicSlopeValue();
    }
  }

  NOTREACHED();
  return NormalSlopeValue();
}

FontSelectionValue StyleBuilderConverter::ConvertFontStyle(
    StyleResolverState& state,
    const CSSValue& value) {
  return StyleBuilderConverterBase::ConvertFontStyle(value);
}

FontSelectionValue StyleBuilderConverterBase::ConvertFontWeight(
    const CSSValue& value,
    FontSelectionValue parent_weight) {
  if (value.IsPrimitiveValue()) {
    const CSSPrimitiveValue& primitive_value = ToCSSPrimitiveValue(value);
    if (primitive_value.IsNumber())
      return clampTo<FontSelectionValue>(primitive_value.GetFloatValue());
  }

  if (value.IsIdentifierValue()) {
    const CSSIdentifierValue& identifier_value = ToCSSIdentifierValue(value);
    switch (identifier_value.GetValueID()) {
      case CSSValueNormal:
        return NormalWeightValue();
      case CSSValueBold:
        return BoldWeightValue();
      case CSSValueBolder:
        return FontDescription::BolderWeight(parent_weight);
      case CSSValueLighter:
        return FontDescription::LighterWeight(parent_weight);
      default:
        NOTREACHED();
        return NormalWeightValue();
    }
  }
  NOTREACHED();
  return NormalWeightValue();
}

FontSelectionValue StyleBuilderConverter::ConvertFontWeight(
    StyleResolverState& state,
    const CSSValue& value) {
  return StyleBuilderConverterBase::ConvertFontWeight(
      value, state.ParentStyle()->GetFontDescription().Weight());
}

FontDescription::FontVariantCaps
StyleBuilderConverterBase::ConvertFontVariantCaps(const CSSValue& value) {
  SECURITY_DCHECK(value.IsIdentifierValue());
  CSSValueID value_id = ToCSSIdentifierValue(value).GetValueID();
  switch (value_id) {
    case CSSValueNormal:
      return FontDescription::kCapsNormal;
    case CSSValueSmallCaps:
      return FontDescription::kSmallCaps;
    case CSSValueAllSmallCaps:
      return FontDescription::kAllSmallCaps;
    case CSSValuePetiteCaps:
      return FontDescription::kPetiteCaps;
    case CSSValueAllPetiteCaps:
      return FontDescription::kAllPetiteCaps;
    case CSSValueUnicase:
      return FontDescription::kUnicase;
    case CSSValueTitlingCaps:
      return FontDescription::kTitlingCaps;
    default:
      return FontDescription::kCapsNormal;
  }
}

FontDescription::FontVariantCaps StyleBuilderConverter::ConvertFontVariantCaps(
    StyleResolverState&,
    const CSSValue& value) {
  return StyleBuilderConverterBase::ConvertFontVariantCaps(value);
}

FontDescription::VariantLigatures
StyleBuilderConverter::ConvertFontVariantLigatures(StyleResolverState&,
                                                   const CSSValue& value) {
  if (value.IsValueList()) {
    FontDescription::VariantLigatures ligatures;
    const CSSValueList& value_list = ToCSSValueList(value);
    for (wtf_size_t i = 0; i < value_list.length(); ++i) {
      const CSSValue& item = value_list.Item(i);
      switch (ToCSSIdentifierValue(item).GetValueID()) {
        case CSSValueNoCommonLigatures:
          ligatures.common = FontDescription::kDisabledLigaturesState;
          break;
        case CSSValueCommonLigatures:
          ligatures.common = FontDescription::kEnabledLigaturesState;
          break;
        case CSSValueNoDiscretionaryLigatures:
          ligatures.discretionary = FontDescription::kDisabledLigaturesState;
          break;
        case CSSValueDiscretionaryLigatures:
          ligatures.discretionary = FontDescription::kEnabledLigaturesState;
          break;
        case CSSValueNoHistoricalLigatures:
          ligatures.historical = FontDescription::kDisabledLigaturesState;
          break;
        case CSSValueHistoricalLigatures:
          ligatures.historical = FontDescription::kEnabledLigaturesState;
          break;
        case CSSValueNoContextual:
          ligatures.contextual = FontDescription::kDisabledLigaturesState;
          break;
        case CSSValueContextual:
          ligatures.contextual = FontDescription::kEnabledLigaturesState;
          break;
        default:
          NOTREACHED();
          break;
      }
    }
    return ligatures;
  }

  SECURITY_DCHECK(value.IsIdentifierValue());
  if (ToCSSIdentifierValue(value).GetValueID() == CSSValueNone) {
    return FontDescription::VariantLigatures(
        FontDescription::kDisabledLigaturesState);
  }

  DCHECK_EQ(ToCSSIdentifierValue(value).GetValueID(), CSSValueNormal);
  return FontDescription::VariantLigatures();
}

FontVariantNumeric StyleBuilderConverter::ConvertFontVariantNumeric(
    StyleResolverState&,
    const CSSValue& value) {
  if (value.IsIdentifierValue()) {
    DCHECK_EQ(ToCSSIdentifierValue(value).GetValueID(), CSSValueNormal);
    return FontVariantNumeric();
  }

  FontVariantNumeric variant_numeric;
  for (const CSSValue* feature : ToCSSValueList(value)) {
    switch (ToCSSIdentifierValue(feature)->GetValueID()) {
      case CSSValueLiningNums:
        variant_numeric.SetNumericFigure(FontVariantNumeric::kLiningNums);
        break;
      case CSSValueOldstyleNums:
        variant_numeric.SetNumericFigure(FontVariantNumeric::kOldstyleNums);
        break;
      case CSSValueProportionalNums:
        variant_numeric.SetNumericSpacing(
            FontVariantNumeric::kProportionalNums);
        break;
      case CSSValueTabularNums:
        variant_numeric.SetNumericSpacing(FontVariantNumeric::kTabularNums);
        break;
      case CSSValueDiagonalFractions:
        variant_numeric.SetNumericFraction(
            FontVariantNumeric::kDiagonalFractions);
        break;
      case CSSValueStackedFractions:
        variant_numeric.SetNumericFraction(
            FontVariantNumeric::kStackedFractions);
        break;
      case CSSValueOrdinal:
        variant_numeric.SetOrdinal(FontVariantNumeric::kOrdinalOn);
        break;
      case CSSValueSlashedZero:
        variant_numeric.SetSlashedZero(FontVariantNumeric::kSlashedZeroOn);
        break;
      default:
        NOTREACHED();
        break;
    }
  }
  return variant_numeric;
}

FontVariantEastAsian StyleBuilderConverter::ConvertFontVariantEastAsian(
    StyleResolverState&,
    const CSSValue& value) {
  if (value.IsIdentifierValue()) {
    DCHECK_EQ(ToCSSIdentifierValue(value).GetValueID(), CSSValueNormal);
    return FontVariantEastAsian();
  }

  FontVariantEastAsian variant_east_asian;
  for (const CSSValue* feature : ToCSSValueList(value)) {
    switch (ToCSSIdentifierValue(feature)->GetValueID()) {
      case CSSValueJis78:
        variant_east_asian.SetForm(FontVariantEastAsian::kJis78);
        break;
      case CSSValueJis83:
        variant_east_asian.SetForm(FontVariantEastAsian::kJis83);
        break;
      case CSSValueJis90:
        variant_east_asian.SetForm(FontVariantEastAsian::kJis90);
        break;
      case CSSValueJis04:
        variant_east_asian.SetForm(FontVariantEastAsian::kJis04);
        break;
      case CSSValueSimplified:
        variant_east_asian.SetForm(FontVariantEastAsian::kSimplified);
        break;
      case CSSValueTraditional:
        variant_east_asian.SetForm(FontVariantEastAsian::kTraditional);
        break;
      case CSSValueFullWidth:
        variant_east_asian.SetWidth(FontVariantEastAsian::kFullWidth);
        break;
      case CSSValueProportionalWidth:
        variant_east_asian.SetWidth(FontVariantEastAsian::kProportionalWidth);
        break;
      case CSSValueRuby:
        variant_east_asian.SetRuby(true);
        break;
      default:
        NOTREACHED();
        break;
    }
  }
  return variant_east_asian;
}

StyleSelfAlignmentData StyleBuilderConverter::ConvertSelfOrDefaultAlignmentData(
    StyleResolverState&,
    const CSSValue& value) {
  StyleSelfAlignmentData alignment_data =
      ComputedStyleInitialValues::InitialAlignSelf();
  if (value.IsValuePair()) {
    const CSSValuePair& pair = ToCSSValuePair(value);
    if (ToCSSIdentifierValue(pair.First()).GetValueID() == CSSValueLegacy) {
      alignment_data.SetPositionType(ItemPositionType::kLegacy);
      alignment_data.SetPosition(
          ToCSSIdentifierValue(pair.Second()).ConvertTo<ItemPosition>());
    } else if (ToCSSIdentifierValue(pair.First()).GetValueID() ==
               CSSValueFirst) {
      alignment_data.SetPosition(ItemPosition::kBaseline);
    } else if (ToCSSIdentifierValue(pair.First()).GetValueID() ==
               CSSValueLast) {
      alignment_data.SetPosition(ItemPosition::kLastBaseline);
    } else {
      alignment_data.SetOverflow(
          ToCSSIdentifierValue(pair.First()).ConvertTo<OverflowAlignment>());
      alignment_data.SetPosition(
          ToCSSIdentifierValue(pair.Second()).ConvertTo<ItemPosition>());
    }
  } else {
    alignment_data.SetPosition(
        ToCSSIdentifierValue(value).ConvertTo<ItemPosition>());
  }
  return alignment_data;
}

StyleContentAlignmentData StyleBuilderConverter::ConvertContentAlignmentData(
    StyleResolverState&,
    const CSSValue& value) {
  StyleContentAlignmentData alignment_data =
      ComputedStyleInitialValues::InitialContentAlignment();
  const CSSContentDistributionValue& content_value =
      ToCSSContentDistributionValue(value);
  if (content_value.Distribution() != CSSValueInvalid) {
    alignment_data.SetDistribution(
        CSSIdentifierValue::Create(content_value.Distribution())
            ->ConvertTo<ContentDistributionType>());
  }
  if (content_value.Position() != CSSValueInvalid) {
    alignment_data.SetPosition(
        CSSIdentifierValue::Create(content_value.Position())
            ->ConvertTo<ContentPosition>());
  }
  if (content_value.Overflow() != CSSValueInvalid) {
    alignment_data.SetOverflow(
        CSSIdentifierValue::Create(content_value.Overflow())
            ->ConvertTo<OverflowAlignment>());
  }

  return alignment_data;
}

GridAutoFlow StyleBuilderConverter::ConvertGridAutoFlow(StyleResolverState&,
                                                        const CSSValue& value) {
  const CSSValueList& list = ToCSSValueList(value);

  DCHECK_GE(list.length(), 1u);
  const CSSIdentifierValue& first = ToCSSIdentifierValue(list.Item(0));
  const CSSIdentifierValue* second =
      list.length() == 2 ? &ToCSSIdentifierValue(list.Item(1)) : nullptr;

  switch (first.GetValueID()) {
    case CSSValueRow:
      if (second && second->GetValueID() == CSSValueDense)
        return kAutoFlowRowDense;
      return kAutoFlowRow;
    case CSSValueColumn:
      if (second && second->GetValueID() == CSSValueDense)
        return kAutoFlowColumnDense;
      return kAutoFlowColumn;
    case CSSValueDense:
      if (second && second->GetValueID() == CSSValueColumn)
        return kAutoFlowColumnDense;
      return kAutoFlowRowDense;
    default:
      NOTREACHED();
      return ComputedStyleInitialValues::InitialGridAutoFlow();
  }
}

GridPosition StyleBuilderConverter::ConvertGridPosition(StyleResolverState&,
                                                        const CSSValue& value) {
  // We accept the specification's grammar:
  // 'auto' | [ <integer> || <custom-ident> ] |
  // [ span && [ <integer> || <custom-ident> ] ] | <custom-ident>

  GridPosition position;

  if (value.IsCustomIdentValue()) {
    position.SetNamedGridArea(ToCSSCustomIdentValue(value).Value());
    return position;
  }

  if (value.IsIdentifierValue()) {
    DCHECK_EQ(ToCSSIdentifierValue(value).GetValueID(), CSSValueAuto);
    return position;
  }

  const CSSValueList& values = ToCSSValueList(value);
  DCHECK(values.length());

  bool is_span_position = false;
  // The specification makes the <integer> optional, in which case it default to
  // '1'.
  int grid_line_number = 1;
  AtomicString grid_line_name;

  auto* it = values.begin();
  const CSSValue* current_value = it->Get();
  if (current_value->IsIdentifierValue() &&
      ToCSSIdentifierValue(current_value)->GetValueID() == CSSValueSpan) {
    is_span_position = true;
    ++it;
    current_value = it != values.end() ? it->Get() : nullptr;
  }

  if (current_value && current_value->IsPrimitiveValue() &&
      ToCSSPrimitiveValue(current_value)->IsNumber()) {
    grid_line_number = ToCSSPrimitiveValue(current_value)->GetIntValue();
    ++it;
    current_value = it != values.end() ? it->Get() : nullptr;
  }

  if (current_value && current_value->IsCustomIdentValue()) {
    grid_line_name = ToCSSCustomIdentValue(current_value)->Value();
    ++it;
  }

  DCHECK_EQ(it, values.end());
  if (is_span_position)
    position.SetSpanPosition(grid_line_number, grid_line_name);
  else
    position.SetExplicitPosition(grid_line_number, grid_line_name);

  return position;
}

GridTrackSize StyleBuilderConverter::ConvertGridTrackSize(
    StyleResolverState& state,
    const CSSValue& value) {
  if (value.IsPrimitiveValue() || value.IsIdentifierValue())
    return GridTrackSize(ConvertGridTrackBreadth(state, value));

  auto& function = ToCSSFunctionValue(value);
  if (function.FunctionType() == CSSValueFitContent) {
    SECURITY_DCHECK(function.length() == 1);
    return GridTrackSize(ConvertGridTrackBreadth(state, function.Item(0)),
                         kFitContentTrackSizing);
  }

  SECURITY_DCHECK(function.length() == 2);
  GridLength min_track_breadth(
      ConvertGridTrackBreadth(state, function.Item(0)));
  GridLength max_track_breadth(
      ConvertGridTrackBreadth(state, function.Item(1)));
  return GridTrackSize(min_track_breadth, max_track_breadth);
}

static void ConvertGridLineNamesList(
    const CSSValue& value,
    size_t current_named_grid_line,
    NamedGridLinesMap& named_grid_lines,
    OrderedNamedGridLines& ordered_named_grid_lines) {
  DCHECK(value.IsGridLineNamesValue());

  for (auto& named_grid_line_value : ToCSSValueList(value)) {
    String named_grid_line =
        ToCSSCustomIdentValue(*named_grid_line_value).Value();
    NamedGridLinesMap::AddResult result =
        named_grid_lines.insert(named_grid_line, Vector<size_t>());
    result.stored_value->value.push_back(current_named_grid_line);
    OrderedNamedGridLines::AddResult ordered_insertion_result =
        ordered_named_grid_lines.insert(current_named_grid_line,
                                        Vector<String>());
    ordered_insertion_result.stored_value->value.push_back(named_grid_line);
  }
}

Vector<GridTrackSize> StyleBuilderConverter::ConvertGridTrackSizeList(
    StyleResolverState& state,
    const CSSValue& value) {
  DCHECK(value.IsValueList());
  Vector<GridTrackSize> track_sizes;
  for (auto& curr_value : ToCSSValueList(value)) {
    DCHECK(!curr_value->IsGridLineNamesValue());
    DCHECK(!curr_value->IsGridAutoRepeatValue());
    track_sizes.push_back(ConvertGridTrackSize(state, *curr_value));
  }
  return track_sizes;
}

void StyleBuilderConverter::ConvertGridTrackList(
    const CSSValue& value,
    Vector<GridTrackSize>& track_sizes,
    NamedGridLinesMap& named_grid_lines,
    OrderedNamedGridLines& ordered_named_grid_lines,
    Vector<GridTrackSize>& auto_repeat_track_sizes,
    NamedGridLinesMap& auto_repeat_named_grid_lines,
    OrderedNamedGridLines& auto_repeat_ordered_named_grid_lines,
    size_t& auto_repeat_insertion_point,
    AutoRepeatType& auto_repeat_type,
    StyleResolverState& state) {
  if (value.IsIdentifierValue()) {
    DCHECK_EQ(ToCSSIdentifierValue(value).GetValueID(), CSSValueNone);
    return;
  }

  size_t current_named_grid_line = 0;
  for (auto curr_value : ToCSSValueList(value)) {
    if (curr_value->IsGridLineNamesValue()) {
      ConvertGridLineNamesList(*curr_value, current_named_grid_line,
                               named_grid_lines, ordered_named_grid_lines);
      continue;
    }

    if (curr_value->IsGridAutoRepeatValue()) {
      DCHECK(auto_repeat_track_sizes.IsEmpty());
      size_t auto_repeat_index = 0;
      CSSValueID auto_repeat_id =
          ToCSSGridAutoRepeatValue(curr_value.Get())->AutoRepeatID();
      DCHECK(auto_repeat_id == CSSValueAutoFill ||
             auto_repeat_id == CSSValueAutoFit);
      auto_repeat_type = auto_repeat_id == CSSValueAutoFill
                             ? AutoRepeatType::kAutoFill
                             : AutoRepeatType::kAutoFit;
      for (auto auto_repeat_value : ToCSSValueList(*curr_value)) {
        if (auto_repeat_value->IsGridLineNamesValue()) {
          ConvertGridLineNamesList(*auto_repeat_value, auto_repeat_index,
                                   auto_repeat_named_grid_lines,
                                   auto_repeat_ordered_named_grid_lines);
          continue;
        }
        ++auto_repeat_index;
        auto_repeat_track_sizes.push_back(
            ConvertGridTrackSize(state, *auto_repeat_value));
      }
      auto_repeat_insertion_point = current_named_grid_line++;
      continue;
    }

    ++current_named_grid_line;
    track_sizes.push_back(ConvertGridTrackSize(state, *curr_value));
  }

  // The parser should have rejected any <track-list> without any <track-size>
  // as this is not conformant to the syntax.
  DCHECK(!track_sizes.IsEmpty() || !auto_repeat_track_sizes.IsEmpty());
}

void StyleBuilderConverter::ConvertOrderedNamedGridLinesMapToNamedGridLinesMap(
    const OrderedNamedGridLines& ordered_named_grid_lines,
    NamedGridLinesMap& named_grid_lines) {
  DCHECK_EQ(named_grid_lines.size(), 0u);

  if (ordered_named_grid_lines.size() == 0)
    return;

  for (auto& ordered_named_grid_line : ordered_named_grid_lines) {
    for (auto& line_name : ordered_named_grid_line.value) {
      NamedGridLinesMap::AddResult start_result =
          named_grid_lines.insert(line_name, Vector<size_t>());
      start_result.stored_value->value.push_back(ordered_named_grid_line.key);
    }
  }

  for (auto& named_grid_line : named_grid_lines) {
    Vector<size_t>& grid_line_indexes = named_grid_line.value;
    std::sort(grid_line_indexes.begin(), grid_line_indexes.end());
  }
}

void StyleBuilderConverter::CreateImplicitNamedGridLinesFromGridArea(
    const NamedGridAreaMap& named_grid_areas,
    NamedGridLinesMap& named_grid_lines,
    GridTrackSizingDirection direction) {
  for (const auto& named_grid_area_entry : named_grid_areas) {
    GridSpan area_span = direction == kForRows
                             ? named_grid_area_entry.value.rows
                             : named_grid_area_entry.value.columns;
    {
      NamedGridLinesMap::AddResult start_result = named_grid_lines.insert(
          named_grid_area_entry.key + "-start", Vector<size_t>());
      start_result.stored_value->value.push_back(area_span.StartLine());
      std::sort(start_result.stored_value->value.begin(),
                start_result.stored_value->value.end());
    }
    {
      NamedGridLinesMap::AddResult end_result = named_grid_lines.insert(
          named_grid_area_entry.key + "-end", Vector<size_t>());
      end_result.stored_value->value.push_back(area_span.EndLine());
      std::sort(end_result.stored_value->value.begin(),
                end_result.stored_value->value.end());
    }
  }
}

float StyleBuilderConverter::ConvertBorderWidth(StyleResolverState& state,
                                                const CSSValue& value) {
  if (value.IsIdentifierValue()) {
    CSSValueID value_id = ToCSSIdentifierValue(value).GetValueID();
    if (value_id == CSSValueThin)
      return 1;
    if (value_id == CSSValueMedium)
      return 3;
    if (value_id == CSSValueThick)
      return 5;
    NOTREACHED();
    return 0;
  }
  const CSSPrimitiveValue& primitive_value = ToCSSPrimitiveValue(value);
  double result =
      primitive_value.ComputeLength<double>(state.CssToLengthConversionData());
  return clampTo<float>(RoundForImpreciseConversion<float>(result),
                        defaultMinimumForClamp<float>(),
                        defaultMaximumForClamp<float>());
}

GapLength StyleBuilderConverter::ConvertGapLength(StyleResolverState& state,
                                                  const CSSValue& value) {
  if (value.IsIdentifierValue() &&
      ToCSSIdentifierValue(value).GetValueID() == CSSValueNormal)
    return GapLength();

  return GapLength(ConvertLength(state, value));
}

Length StyleBuilderConverter::ConvertLength(const StyleResolverState& state,
                                            const CSSValue& value) {
  return ToCSSPrimitiveValue(value).ConvertToLength(
      state.CssToLengthConversionData());
}

UnzoomedLength StyleBuilderConverter::ConvertUnzoomedLength(
    const StyleResolverState& state,
    const CSSValue& value) {
  return UnzoomedLength(ToCSSPrimitiveValue(value).ConvertToLength(
      state.CssToLengthConversionData().CopyWithAdjustedZoom(1.0f)));
}

Length StyleBuilderConverter::ConvertLengthOrAuto(
    const StyleResolverState& state,
    const CSSValue& value) {
  if (value.IsIdentifierValue() &&
      ToCSSIdentifierValue(value).GetValueID() == CSSValueAuto)
    return Length(kAuto);
  return ToCSSPrimitiveValue(value).ConvertToLength(
      state.CssToLengthConversionData());
}

Length StyleBuilderConverter::ConvertLengthSizing(StyleResolverState& state,
                                                  const CSSValue& value) {
  if (!value.IsIdentifierValue())
    return ConvertLength(state, value);

  const CSSIdentifierValue& identifier_value = ToCSSIdentifierValue(value);
  switch (identifier_value.GetValueID()) {
    case CSSValueMinContent:
    case CSSValueWebkitMinContent:
      return Length(kMinContent);
    case CSSValueMaxContent:
    case CSSValueWebkitMaxContent:
      return Length(kMaxContent);
    case CSSValueWebkitFillAvailable:
      return Length(kFillAvailable);
    case CSSValueWebkitFitContent:
    case CSSValueFitContent:
      return Length(kFitContent);
    case CSSValueAuto:
      return Length(kAuto);
    default:
      NOTREACHED();
      return Length();
  }
}

Length StyleBuilderConverter::ConvertLengthMaxSizing(StyleResolverState& state,
                                                     const CSSValue& value) {
  if (value.IsIdentifierValue() &&
      ToCSSIdentifierValue(value).GetValueID() == CSSValueNone)
    return Length(kMaxSizeNone);
  return ConvertLengthSizing(state, value);
}

TabSize StyleBuilderConverter::ConvertLengthOrTabSpaces(
    StyleResolverState& state,
    const CSSValue& value) {
  const CSSPrimitiveValue& primitive_value = ToCSSPrimitiveValue(value);
  if (primitive_value.IsNumber())
    return TabSize(primitive_value.GetIntValue());
  return TabSize(
      primitive_value.ComputeLength<float>(state.CssToLengthConversionData()));
}

static CSSToLengthConversionData LineHeightToLengthConversionData(
    StyleResolverState& state) {
  float multiplier = state.Style()->EffectiveZoom();
  if (LocalFrame* frame = state.GetDocument().GetFrame())
    multiplier *= frame->TextZoomFactor();
  return state.CssToLengthConversionData().CopyWithAdjustedZoom(multiplier);
}

Length StyleBuilderConverter::ConvertLineHeight(StyleResolverState& state,
                                                const CSSValue& value) {
  if (value.IsPrimitiveValue()) {
    const CSSPrimitiveValue& primitive_value = ToCSSPrimitiveValue(value);
    if (primitive_value.IsLength()) {
      return primitive_value.ComputeLength<Length>(
          LineHeightToLengthConversionData(state));
    }
    if (primitive_value.IsPercentage()) {
      return Length(
          (state.Style()->ComputedFontSize() * primitive_value.GetIntValue()) /
              100.0,
          kFixed);
    }
    if (primitive_value.IsNumber()) {
      return Length(clampTo<float>(primitive_value.GetDoubleValue() * 100.0),
                    kPercent);
    }
    if (primitive_value.IsCalculated()) {
      Length zoomed_length = Length(primitive_value.CssCalcValue()->ToCalcValue(
          LineHeightToLengthConversionData(state)));
      return Length(
          ValueForLength(zoomed_length,
                         LayoutUnit(state.Style()->ComputedFontSize())),
          kFixed);
    }
  }

  DCHECK_EQ(ToCSSIdentifierValue(value).GetValueID(), CSSValueNormal);
  return ComputedStyleInitialValues::InitialLineHeight();
}

float StyleBuilderConverter::ConvertNumberOrPercentage(
    StyleResolverState& state,
    const CSSValue& value) {
  const CSSPrimitiveValue& primitive_value = ToCSSPrimitiveValue(value);
  DCHECK(primitive_value.IsNumber() || primitive_value.IsPercentage());
  if (primitive_value.IsNumber())
    return primitive_value.GetFloatValue();
  return primitive_value.GetFloatValue() / 100.0f;
}

StyleOffsetRotation StyleBuilderConverter::ConvertOffsetRotate(
    StyleResolverState&,
    const CSSValue& value) {
  return ConvertOffsetRotate(value);
}

StyleOffsetRotation StyleBuilderConverter::ConvertOffsetRotate(
    const CSSValue& value) {
  StyleOffsetRotation result(0, OffsetRotationType::kFixed);

  const CSSValueList& list = ToCSSValueList(value);
  DCHECK(list.length() == 1 || list.length() == 2);
  for (const auto& item : list) {
    if (item->IsIdentifierValue() &&
        ToCSSIdentifierValue(*item).GetValueID() == CSSValueAuto) {
      result.type = OffsetRotationType::kAuto;
    } else if (item->IsIdentifierValue() &&
               ToCSSIdentifierValue(*item).GetValueID() == CSSValueReverse) {
      result.type = OffsetRotationType::kAuto;
      result.angle = clampTo<float>(result.angle + 180);
    } else {
      const CSSPrimitiveValue& primitive_value = ToCSSPrimitiveValue(*item);
      result.angle =
          clampTo<float>(result.angle + primitive_value.ComputeDegrees());
    }
  }

  return result;
}

LengthPoint StyleBuilderConverter::ConvertPosition(StyleResolverState& state,
                                                   const CSSValue& value) {
  const CSSValuePair& pair = ToCSSValuePair(value);
  return LengthPoint(
      ConvertPositionLength<CSSValueLeft, CSSValueRight>(state, pair.First()),
      ConvertPositionLength<CSSValueTop, CSSValueBottom>(state, pair.Second()));
}

LengthPoint StyleBuilderConverter::ConvertPositionOrAuto(
    StyleResolverState& state,
    const CSSValue& value) {
  if (value.IsValuePair())
    return ConvertPosition(state, value);
  DCHECK(ToCSSIdentifierValue(value).GetValueID() == CSSValueAuto);
  return LengthPoint(Length(kAuto), Length(kAuto));
}

static float ConvertPerspectiveLength(
    StyleResolverState& state,
    const CSSPrimitiveValue& primitive_value) {
  return std::max(
      primitive_value.ComputeLength<float>(state.CssToLengthConversionData()),
      0.0f);
}

float StyleBuilderConverter::ConvertPerspective(StyleResolverState& state,
                                                const CSSValue& value) {
  if (value.IsIdentifierValue() &&
      ToCSSIdentifierValue(value).GetValueID() == CSSValueNone)
    return ComputedStyleInitialValues::InitialPerspective();
  return ConvertPerspectiveLength(state, ToCSSPrimitiveValue(value));
}

EPaintOrder StyleBuilderConverter::ConvertPaintOrder(
    StyleResolverState&,
    const CSSValue& css_paint_order) {
  if (css_paint_order.IsValueList()) {
    const CSSValueList& order_type_list = ToCSSValueList(css_paint_order);
    switch (ToCSSIdentifierValue(order_type_list.Item(0)).GetValueID()) {
      case CSSValueFill:
        return order_type_list.length() > 1 ? kPaintOrderFillMarkersStroke
                                            : kPaintOrderFillStrokeMarkers;
      case CSSValueStroke:
        return order_type_list.length() > 1 ? kPaintOrderStrokeMarkersFill
                                            : kPaintOrderStrokeFillMarkers;
      case CSSValueMarkers:
        return order_type_list.length() > 1 ? kPaintOrderMarkersStrokeFill
                                            : kPaintOrderMarkersFillStroke;
      default:
        NOTREACHED();
        return kPaintOrderNormal;
    }
  }

  return kPaintOrderNormal;
}

Length StyleBuilderConverter::ConvertQuirkyLength(StyleResolverState& state,
                                                  const CSSValue& value) {
  Length length = ConvertLengthOrAuto(state, value);
  // This is only for margins which use __qem
  length.SetQuirk(value.IsPrimitiveValue() &&
                  ToCSSPrimitiveValue(value).IsQuirkyEms());
  return length;
}

scoped_refptr<QuotesData> StyleBuilderConverter::ConvertQuotes(
    StyleResolverState&,
    const CSSValue& value) {
  if (value.IsValueList()) {
    const CSSValueList& list = ToCSSValueList(value);
    scoped_refptr<QuotesData> quotes = QuotesData::Create();
    for (wtf_size_t i = 0; i < list.length(); i += 2) {
      String start_quote = ToCSSStringValue(list.Item(i)).Value();
      String end_quote = ToCSSStringValue(list.Item(i + 1)).Value();
      quotes->AddPair(std::make_pair(start_quote, end_quote));
    }
    return quotes;
  }
  DCHECK_EQ(ToCSSIdentifierValue(value).GetValueID(), CSSValueNone);
  return QuotesData::Create();
}

LengthSize StyleBuilderConverter::ConvertRadius(StyleResolverState& state,
                                                const CSSValue& value) {
  const CSSValuePair& pair = ToCSSValuePair(value);
  Length radius_width = ToCSSPrimitiveValue(pair.First())
                            .ConvertToLength(state.CssToLengthConversionData());
  Length radius_height =
      ToCSSPrimitiveValue(pair.Second())
          .ConvertToLength(state.CssToLengthConversionData());
  return LengthSize(radius_width, radius_height);
}

ShadowData StyleBuilderConverter::ConvertShadow(
    const CSSToLengthConversionData& conversion_data,
    StyleResolverState* state,
    const CSSValue& value) {
  const CSSShadowValue& shadow = ToCSSShadowValue(value);
  float x = shadow.x->ComputeLength<float>(conversion_data);
  float y = shadow.y->ComputeLength<float>(conversion_data);
  float blur =
      shadow.blur ? shadow.blur->ComputeLength<float>(conversion_data) : 0;
  float spread =
      shadow.spread ? shadow.spread->ComputeLength<float>(conversion_data) : 0;
  ShadowStyle shadow_style =
      shadow.style && shadow.style->GetValueID() == CSSValueInset ? kInset
                                                                  : kNormal;
  StyleColor color = StyleColor::CurrentColor();
  if (shadow.color) {
    if (state) {
      color = ConvertStyleColor(*state, *shadow.color);
    } else {
      // For OffScreen canvas, we default to black and only parse non
      // Document dependent CSS colors.
      color = StyleColor(Color::kBlack);
      if (shadow.color->IsColorValue()) {
        color = ToCSSColorValue(*shadow.color).Value();
      } else {
        CSSValueID value_id = ToCSSIdentifierValue(*shadow.color).GetValueID();
        switch (value_id) {
          case CSSValueInvalid:
            NOTREACHED();
            FALLTHROUGH;
          case CSSValueInternalQuirkInherit:
          case CSSValueWebkitLink:
          case CSSValueWebkitActivelink:
          case CSSValueWebkitFocusRingColor:
          case CSSValueCurrentcolor:
            break;
          default:
            color = StyleColor::ColorFromKeyword(value_id);
        }
      }
    }
  }

  return ShadowData(FloatPoint(x, y), blur, spread, shadow_style, color);
}

scoped_refptr<ShadowList> StyleBuilderConverter::ConvertShadowList(
    StyleResolverState& state,
    const CSSValue& value) {
  if (value.IsIdentifierValue()) {
    DCHECK_EQ(ToCSSIdentifierValue(value).GetValueID(), CSSValueNone);
    return scoped_refptr<ShadowList>();
  }

  ShadowDataVector shadows;
  for (const auto& item : ToCSSValueList(value)) {
    shadows.push_back(
        ConvertShadow(state.CssToLengthConversionData(), &state, *item));
  }

  return ShadowList::Adopt(shadows);
}

ShapeValue* StyleBuilderConverter::ConvertShapeValue(StyleResolverState& state,
                                                     const CSSValue& value) {
  if (value.IsIdentifierValue()) {
    DCHECK_EQ(ToCSSIdentifierValue(value).GetValueID(), CSSValueNone);
    return nullptr;
  }

  if (value.IsImageValue() || value.IsImageGeneratorValue() ||
      value.IsImageSetValue())
    return ShapeValue::CreateImageValue(
        state.GetStyleImage(CSSPropertyShapeOutside, value));

  scoped_refptr<BasicShape> shape;
  CSSBoxType css_box = CSSBoxType::kMissing;
  const CSSValueList& value_list = ToCSSValueList(value);
  for (unsigned i = 0; i < value_list.length(); ++i) {
    const CSSValue& value = value_list.Item(i);
    if (value.IsBasicShapeValue()) {
      shape = BasicShapeForValue(state, value);
    } else {
      css_box = ToCSSIdentifierValue(value).ConvertTo<CSSBoxType>();
    }
  }

  if (shape)
    return ShapeValue::CreateShapeValue(std::move(shape), css_box);

  DCHECK_NE(css_box, CSSBoxType::kMissing);
  return ShapeValue::CreateBoxShapeValue(css_box);
}

float StyleBuilderConverter::ConvertSpacing(StyleResolverState& state,
                                            const CSSValue& value) {
  if (value.IsIdentifierValue() &&
      ToCSSIdentifierValue(value).GetValueID() == CSSValueNormal)
    return 0;
  return ToCSSPrimitiveValue(value).ComputeLength<float>(
      state.CssToLengthConversionData());
}

scoped_refptr<SVGDashArray> StyleBuilderConverter::ConvertStrokeDasharray(
    StyleResolverState& state,
    const CSSValue& value) {
  if (!value.IsValueList())
    return SVGComputedStyle::InitialStrokeDashArray();

  const CSSValueList& dashes = ToCSSValueList(value);

  scoped_refptr<SVGDashArray> array = SVGDashArray::Create();
  wtf_size_t length = dashes.length();
  for (wtf_size_t i = 0; i < length; ++i) {
    array->push_back(ConvertLength(state, ToCSSPrimitiveValue(dashes.Item(i))));
  }

  return array;
}

StyleColor StyleBuilderConverter::ConvertStyleColor(StyleResolverState& state,
                                                    const CSSValue& value,
                                                    bool for_visited_link) {
  if (value.IsIdentifierValue() &&
      ToCSSIdentifierValue(value).GetValueID() == CSSValueCurrentcolor)
    return StyleColor::CurrentColor();
  return state.GetDocument().GetTextLinkColors().ColorFromCSSValue(
      value, Color(), for_visited_link);
}

StyleAutoColor StyleBuilderConverter::ConvertStyleAutoColor(
    StyleResolverState& state,
    const CSSValue& value,
    bool for_visited_link) {
  if (value.IsIdentifierValue()) {
    if (ToCSSIdentifierValue(value).GetValueID() == CSSValueCurrentcolor)
      return StyleAutoColor::CurrentColor();
    if (ToCSSIdentifierValue(value).GetValueID() == CSSValueAuto)
      return StyleAutoColor::AutoColor();
  }
  return state.GetDocument().GetTextLinkColors().ColorFromCSSValue(
      value, Color(), for_visited_link);
}

SVGPaint StyleBuilderConverter::ConvertSVGPaint(StyleResolverState& state,
                                                const CSSValue& value) {
  const CSSValue* local_value = &value;
  SVGPaint paint;
  if (value.IsValueList()) {
    const CSSValueList& list = ToCSSValueList(value);
    DCHECK_EQ(list.length(), 2u);
    paint.resource = ConvertElementReference(state, list.Item(0));
    local_value = &list.Item(1);
  }

  if (local_value->IsURIValue()) {
    paint.type = SVG_PAINTTYPE_URI;
    paint.resource = ConvertElementReference(state, *local_value);
  } else if (local_value->IsIdentifierValue() &&
             ToCSSIdentifierValue(local_value)->GetValueID() == CSSValueNone) {
    paint.type = !paint.resource ? SVG_PAINTTYPE_NONE : SVG_PAINTTYPE_URI_NONE;
  } else if (local_value->IsIdentifierValue() &&
             ToCSSIdentifierValue(local_value)->GetValueID() ==
                 CSSValueCurrentcolor) {
    paint.color = state.Style()->GetColor();
    paint.type = !paint.resource ? SVG_PAINTTYPE_CURRENTCOLOR
                                 : SVG_PAINTTYPE_URI_CURRENTCOLOR;
  } else {
    paint.color = ConvertColor(state, *local_value);
    paint.type =
        !paint.resource ? SVG_PAINTTYPE_RGBCOLOR : SVG_PAINTTYPE_URI_RGBCOLOR;
  }
  return paint;
}

TextEmphasisPosition StyleBuilderConverter::ConvertTextTextEmphasisPosition(
    StyleResolverState& state,
    const CSSValue& value) {
  const CSSValueList& list = ToCSSValueList(value);
  DCHECK(list.Item(0).IsIdentifierValue());
  DCHECK(list.Item(1).IsIdentifierValue());
  CSSValueID first = ToCSSIdentifierValue(list.Item(0)).GetValueID();
  CSSValueID second = ToCSSIdentifierValue(list.Item(1)).GetValueID();
  if (first == CSSValueOver && second == CSSValueRight)
    return TextEmphasisPosition::kOverRight;
  if (first == CSSValueOver && second == CSSValueLeft)
    return TextEmphasisPosition::kOverLeft;
  if (first == CSSValueUnder && second == CSSValueRight)
    return TextEmphasisPosition::kUnderRight;
  if (first == CSSValueUnder && second == CSSValueLeft)
    return TextEmphasisPosition::kUnderLeft;
  return TextEmphasisPosition::kOverRight;
}

float StyleBuilderConverter::ConvertTextStrokeWidth(StyleResolverState& state,
                                                    const CSSValue& value) {
  if (value.IsIdentifierValue() && ToCSSIdentifierValue(value).GetValueID()) {
    float multiplier = ConvertLineWidth<float>(state, value);
    return CSSPrimitiveValue::Create(multiplier / 48,
                                     CSSPrimitiveValue::UnitType::kEms)
        ->ComputeLength<float>(state.CssToLengthConversionData());
  }
  return ToCSSPrimitiveValue(value).ComputeLength<float>(
      state.CssToLengthConversionData());
}

TextSizeAdjust StyleBuilderConverter::ConvertTextSizeAdjust(
    StyleResolverState& state,
    const CSSValue& value) {
  if (value.IsIdentifierValue() &&
      ToCSSIdentifierValue(value).GetValueID() == CSSValueNone)
    return TextSizeAdjust::AdjustNone();
  if (value.IsIdentifierValue() &&
      ToCSSIdentifierValue(value).GetValueID() == CSSValueAuto)
    return TextSizeAdjust::AdjustAuto();
  const CSSPrimitiveValue& primitive_value = ToCSSPrimitiveValue(value);
  DCHECK(primitive_value.IsPercentage());
  return TextSizeAdjust(primitive_value.GetFloatValue() / 100.0f);
}

TextUnderlinePosition StyleBuilderConverter::ConvertTextUnderlinePosition(
    StyleResolverState& state,
    const CSSValue& value) {
  TextUnderlinePosition flags = kTextUnderlinePositionAuto;

  auto process = [&flags](const CSSValue& identifier) {
    flags |=
        ToCSSIdentifierValue(identifier).ConvertTo<TextUnderlinePosition>();
  };

  if (value.IsValueList()) {
    for (auto& entry : ToCSSValueList(value)) {
      process(*entry);
    }
  } else {
    process(value);
  }
  return flags;
}

TransformOperations StyleBuilderConverter::ConvertTransformOperations(
    StyleResolverState& state,
    const CSSValue& value) {
  return TransformBuilder::CreateTransformOperations(
      value, state.CssToLengthConversionData());
}

TransformOrigin StyleBuilderConverter::ConvertTransformOrigin(
    StyleResolverState& state,
    const CSSValue& value) {
  const CSSValueList& list = ToCSSValueList(value);
  DCHECK_EQ(list.length(), 3U);
  DCHECK(list.Item(0).IsPrimitiveValue() || list.Item(0).IsIdentifierValue());
  DCHECK(list.Item(1).IsPrimitiveValue() || list.Item(1).IsIdentifierValue());
  DCHECK(list.Item(2).IsPrimitiveValue());

  return TransformOrigin(
      ConvertPositionLength<CSSValueLeft, CSSValueRight>(state, list.Item(0)),
      ConvertPositionLength<CSSValueTop, CSSValueBottom>(state, list.Item(1)),
      StyleBuilderConverter::ConvertComputedLength<float>(state, list.Item(2)));
}

ScrollSnapType StyleBuilderConverter::ConvertSnapType(StyleResolverState&,
                                                      const CSSValue& value) {
  ScrollSnapType snapType = ComputedStyleInitialValues::InitialScrollSnapType();
  if (value.IsValuePair()) {
    const CSSValuePair& pair = ToCSSValuePair(value);
    snapType.is_none = false;
    snapType.axis = ToCSSIdentifierValue(pair.First()).ConvertTo<SnapAxis>();
    snapType.strictness =
        ToCSSIdentifierValue(pair.Second()).ConvertTo<SnapStrictness>();
    return snapType;
  }

  if (ToCSSIdentifierValue(value).GetValueID() == CSSValueNone) {
    snapType.is_none = true;
    return snapType;
  }

  snapType.is_none = false;
  snapType.axis = ToCSSIdentifierValue(value).ConvertTo<SnapAxis>();
  return snapType;
}

ScrollSnapAlign StyleBuilderConverter::ConvertSnapAlign(StyleResolverState&,
                                                        const CSSValue& value) {
  ScrollSnapAlign snapAlign =
      ComputedStyleInitialValues::InitialScrollSnapAlign();
  if (value.IsValuePair()) {
    const CSSValuePair& pair = ToCSSValuePair(value);
    snapAlign.alignment_block =
        ToCSSIdentifierValue(pair.First()).ConvertTo<SnapAlignment>();
    snapAlign.alignment_inline =
        ToCSSIdentifierValue(pair.Second()).ConvertTo<SnapAlignment>();
  } else {
    snapAlign.alignment_block =
        ToCSSIdentifierValue(value).ConvertTo<SnapAlignment>();
    snapAlign.alignment_inline = snapAlign.alignment_block;
  }
  return snapAlign;
}

scoped_refptr<TranslateTransformOperation>
StyleBuilderConverter::ConvertTranslate(StyleResolverState& state,
                                        const CSSValue& value) {
  if (value.IsIdentifierValue()) {
    DCHECK_EQ(ToCSSIdentifierValue(value).GetValueID(), CSSValueNone);
    return nullptr;
  }
  const CSSValueList& list = ToCSSValueList(value);
  DCHECK_LE(list.length(), 3u);
  Length tx = ConvertLength(state, list.Item(0));
  Length ty(0, kFixed);
  double tz = 0;
  if (list.length() >= 2)
    ty = ConvertLength(state, list.Item(1));
  if (list.length() == 3)
    tz = ToCSSPrimitiveValue(list.Item(2))
             .ComputeLength<double>(state.CssToLengthConversionData());

  return TranslateTransformOperation::Create(tx, ty, tz,
                                             TransformOperation::kTranslate3D);
}

Rotation StyleBuilderConverter::ConvertRotation(const CSSValue& value) {
  if (value.IsIdentifierValue()) {
    DCHECK_EQ(ToCSSIdentifierValue(value).GetValueID(), CSSValueNone);
    return Rotation(FloatPoint3D(0, 0, 1), 0);
  }

  const CSSValueList& list = ToCSSValueList(value);
  DCHECK(list.length() == 1 || list.length() == 4);
  double x = 0;
  double y = 0;
  double z = 1;
  if (list.length() == 4) {
    x = ToCSSPrimitiveValue(list.Item(0)).GetDoubleValue();
    y = ToCSSPrimitiveValue(list.Item(1)).GetDoubleValue();
    z = ToCSSPrimitiveValue(list.Item(2)).GetDoubleValue();
  }
  double angle =
      ToCSSPrimitiveValue(list.Item(list.length() - 1)).ComputeDegrees();
  return Rotation(FloatPoint3D(x, y, z), angle);
}

scoped_refptr<RotateTransformOperation> StyleBuilderConverter::ConvertRotate(
    StyleResolverState& state,
    const CSSValue& value) {
  if (value.IsIdentifierValue()) {
    DCHECK_EQ(ToCSSIdentifierValue(value).GetValueID(), CSSValueNone);
    return nullptr;
  }

  return RotateTransformOperation::Create(ConvertRotation(value),
                                          TransformOperation::kRotate3D);
}

scoped_refptr<ScaleTransformOperation> StyleBuilderConverter::ConvertScale(
    StyleResolverState& state,
    const CSSValue& value) {
  if (value.IsIdentifierValue()) {
    DCHECK_EQ(ToCSSIdentifierValue(value).GetValueID(), CSSValueNone);
    return nullptr;
  }

  const CSSValueList& list = ToCSSValueList(value);
  DCHECK_LE(list.length(), 3u);
  double sx = ToCSSPrimitiveValue(list.Item(0)).GetDoubleValue();
  double sy = sx;
  double sz = 1;
  if (list.length() >= 2)
    sy = ToCSSPrimitiveValue(list.Item(1)).GetDoubleValue();
  if (list.length() == 3)
    sz = ToCSSPrimitiveValue(list.Item(2)).GetDoubleValue();

  return ScaleTransformOperation::Create(sx, sy, sz,
                                         TransformOperation::kScale3D);
}

RespectImageOrientationEnum StyleBuilderConverter::ConvertImageOrientation(
    StyleResolverState& state,
    const CSSValue& value) {
  return value.IsIdentifierValue() &&
                 ToCSSIdentifierValue(value).GetValueID() == CSSValueFromImage
             ? kRespectImageOrientation
             : kDoNotRespectImageOrientation;
}

scoped_refptr<StylePath> StyleBuilderConverter::ConvertPathOrNone(
    StyleResolverState& state,
    const CSSValue& value) {
  if (value.IsPathValue())
    return ToCSSPathValue(value).GetStylePath();
  DCHECK_EQ(ToCSSIdentifierValue(value).GetValueID(), CSSValueNone);
  return nullptr;
}

scoped_refptr<BasicShape> StyleBuilderConverter::ConvertOffsetPath(
    StyleResolverState& state,
    const CSSValue& value) {
  if (value.IsRayValue())
    return BasicShapeForValue(state, value);
  return ConvertPathOrNone(state, value);
}

static const CSSValue& ComputeRegisteredPropertyValue(
    const CSSToLengthConversionData& css_to_length_conversion_data,
    const CSSValue& value) {
  // TODO(timloh): Images values can also contain lengths.
  if (value.IsFunctionValue()) {
    const CSSFunctionValue& function_value = ToCSSFunctionValue(value);
    CSSFunctionValue* new_function =
        CSSFunctionValue::Create(function_value.FunctionType());
    for (const CSSValue* inner_value : ToCSSValueList(value)) {
      new_function->Append(ComputeRegisteredPropertyValue(
          css_to_length_conversion_data, *inner_value));
    }
    return *new_function;
  }

  if (value.IsValueList()) {
    const CSSValueList& old_list = ToCSSValueList(value);
    CSSValueList* new_list = CSSValueList::CreateWithSeparatorFrom(old_list);
    for (const CSSValue* inner_value : old_list) {
      new_list->Append(ComputeRegisteredPropertyValue(
          css_to_length_conversion_data, *inner_value));
    }
    return *new_list;
  }

  if (value.IsPrimitiveValue()) {
    const CSSPrimitiveValue& primitive_value = ToCSSPrimitiveValue(value);
    if ((primitive_value.IsCalculated() &&
         (primitive_value.IsCalculatedPercentageWithLength() ||
          primitive_value.IsLength() || primitive_value.IsPercentage())) ||
        CSSPrimitiveValue::IsRelativeUnit(
            primitive_value.TypeWithCalcResolved())) {
      // Instead of the actual zoom, use 1 to avoid potential rounding errors
      Length length = primitive_value.ConvertToLength(
          css_to_length_conversion_data.CopyWithAdjustedZoom(1));
      return *CSSPrimitiveValue::Create(length, 1);
    }
    // If we encounter a calculated number that was not resolved during
    // parsing, it means that a calc()-expression was allowed in place of
    // an integer. Such calc()-for-integers must be rounded at computed value
    // time.
    // https://drafts.csswg.org/css-values-4/#calc-type-checking
    if (primitive_value.IsCalculated() &&
        (primitive_value.TypeWithCalcResolved() ==
         CSSPrimitiveValue::UnitType::kNumber)) {
      double double_value = primitive_value.CssCalcValue()->DoubleValue();
      auto unit_type = CSSPrimitiveValue::UnitType::kInteger;
      return *CSSPrimitiveValue::Create(std::round(double_value), unit_type);
    }
  }
  return value;
}

const CSSValue& StyleBuilderConverter::ConvertRegisteredPropertyInitialValue(
    const CSSValue& value) {
  return ComputeRegisteredPropertyValue(CSSToLengthConversionData(), value);
}

const CSSValue& StyleBuilderConverter::ConvertRegisteredPropertyValue(
    const StyleResolverState& state,
    const CSSValue& value) {
  return ComputeRegisteredPropertyValue(state.CssToLengthConversionData(),
                                        value);
}

const CSSToLengthConversionData&
StyleBuilderConverter::CssToLengthConversionData(StyleResolverState& state) {
  return state.CssToLengthConversionData();
}

}  // namespace blink
