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
#include <memory>
#include <optional>
#include <utility>

#include "base/containers/adapters.h"
#include "base/notreached.h"
#include "build/build_config.h"
#include "third_party/blink/renderer/core/css/basic_shape_functions.h"
#include "third_party/blink/renderer/core/css/css_alternate_value.h"
#include "third_party/blink/renderer/core/css/css_axis_value.h"
#include "third_party/blink/renderer/core/css/css_color.h"
#include "third_party/blink/renderer/core/css/css_color_mix_value.h"
#include "third_party/blink/renderer/core/css/css_content_distribution_value.h"
#include "third_party/blink/renderer/core/css/css_custom_ident_value.h"
#include "third_party/blink/renderer/core/css/css_dynamic_range_limit_mix_value.h"
#include "third_party/blink/renderer/core/css/css_font_family_value.h"
#include "third_party/blink/renderer/core/css/css_font_feature_value.h"
#include "third_party/blink/renderer/core/css/css_font_style_range_value.h"
#include "third_party/blink/renderer/core/css/css_font_variation_value.h"
#include "third_party/blink/renderer/core/css/css_grid_auto_repeat_value.h"
#include "third_party/blink/renderer/core/css/css_grid_integer_repeat_value.h"
#include "third_party/blink/renderer/core/css/css_grid_template_areas_value.h"
#include "third_party/blink/renderer/core/css/css_identifier_value.h"
#include "third_party/blink/renderer/core/css/css_identifier_value_mappings.h"
#include "third_party/blink/renderer/core/css/css_light_dark_value_pair.h"
#include "third_party/blink/renderer/core/css/css_math_expression_node.h"
#include "third_party/blink/renderer/core/css/css_math_function_value.h"
#include "third_party/blink/renderer/core/css/css_numeric_literal_value.h"
#include "third_party/blink/renderer/core/css/css_palette_mix_value.h"
#include "third_party/blink/renderer/core/css/css_path_value.h"
#include "third_party/blink/renderer/core/css/css_pending_system_font_value.h"
#include "third_party/blink/renderer/core/css/css_primitive_value.h"
#include "third_party/blink/renderer/core/css/css_quad_value.h"
#include "third_party/blink/renderer/core/css/css_ratio_value.h"
#include "third_party/blink/renderer/core/css/css_reflect_value.h"
#include "third_party/blink/renderer/core/css/css_relative_color_value.h"
#include "third_party/blink/renderer/core/css/css_repeat_value.h"
#include "third_party/blink/renderer/core/css/css_scoped_keyword_value.h"
#include "third_party/blink/renderer/core/css/css_shadow_value.h"
#include "third_party/blink/renderer/core/css/css_superellipse_value.h"
#include "third_party/blink/renderer/core/css/css_unresolved_color_value.h"
#include "third_party/blink/renderer/core/css/css_uri_value.h"
#include "third_party/blink/renderer/core/css/css_value_list.h"
#include "third_party/blink/renderer/core/css/css_value_pair.h"
#include "third_party/blink/renderer/core/css/parser/css_tokenizer.h"
#include "third_party/blink/renderer/core/css/properties/computed_style_utils.h"
#include "third_party/blink/renderer/core/css/properties/css_parsing_utils.h"
#include "third_party/blink/renderer/core/css/resolver/filter_operation_resolver.h"
#include "third_party/blink/renderer/core/css/resolver/transform_builder.h"
#include "third_party/blink/renderer/core/css/style_color.h"
#include "third_party/blink/renderer/core/css/style_engine.h"
#include "third_party/blink/renderer/core/css/style_sheet_contents.h"
#include "third_party/blink/renderer/core/css_value_keywords.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/style/computed_style_constants.h"
#include "third_party/blink/renderer/core/style/coord_box_offset_path_operation.h"
#include "third_party/blink/renderer/core/style/geometry_box_clip_path_operation.h"
#include "third_party/blink/renderer/core/style/offset_path_operation.h"
#include "third_party/blink/renderer/core/style/reference_clip_path_operation.h"
#include "third_party/blink/renderer/core/style/reference_offset_path_operation.h"
#include "third_party/blink/renderer/core/style/scoped_css_name.h"
#include "third_party/blink/renderer/core/style/scroll_marker_group.h"
#include "third_party/blink/renderer/core/style/shape_clip_path_operation.h"
#include "third_party/blink/renderer/core/style/shape_offset_path_operation.h"
#include "third_party/blink/renderer/core/style/style_border_shape.h"
#include "third_party/blink/renderer/core/style/style_overflow_clip_margin.h"
#include "third_party/blink/renderer/core/style/style_svg_resource.h"
#include "third_party/blink/renderer/core/style/style_view_transition_group.h"
#include "third_party/blink/renderer/core/style/superellipse.h"
#include "third_party/blink/renderer/core/style/text_overflow_data.h"
#include "third_party/blink/renderer/platform/fonts/font_palette.h"
#include "third_party/blink/renderer/platform/fonts/opentype/open_type_math_support.h"
#include "third_party/blink/renderer/platform/geometry/layout_unit.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/wtf/hash_map.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

const double kFinalStatePercentage = 100.0;
const double kMiddleStatePercentage = 50.0;

namespace {

Length ConvertGridTrackBreadth(const StyleResolverState& state,
                               const CSSValue& value) {
  // Fractional unit.
  auto* primitive_value = DynamicTo<CSSPrimitiveValue>(value);
  if (primitive_value && primitive_value->IsFlex()) {
    return Length::Flex(primitive_value->ComputeValueInCanonicalUnit(
        state.CssToLengthConversionData()));
  }

  auto* identifier_value = DynamicTo<CSSIdentifierValue>(value);
  if (identifier_value) {
    if (identifier_value->GetValueID() == CSSValueID::kMinContent) {
      return Length::MinContent();
    }
    if (identifier_value->GetValueID() == CSSValueID::kMaxContent) {
      return Length::MaxContent();
    }
  }

  return StyleBuilderConverter::ConvertLengthOrAuto(state, value);
}

Vector<AtomicString> ValueListToAtomicStringVector(
    const CSSValueList& value_list) {
  Vector<AtomicString> ret;
  for (const auto& list_entry : value_list) {
    const CSSCustomIdentValue& ident = To<CSSCustomIdentValue>(*list_entry);
    ret.push_back(ident.Value());
  }
  return ret;
}

AtomicString FirstEntryAsAtomicString(const CSSValueList& value_list) {
  DCHECK_EQ(value_list.length(), 1u);
  return To<CSSCustomIdentValue>(value_list.Item(0)).Value();
}

bool IsQuirkOrLinkOrFocusRingColor(CSSValueID value_id) {
  return value_id == CSSValueID::kInternalQuirkInherit ||
         value_id == CSSValueID::kWebkitLink ||
         value_id == CSSValueID::kWebkitActivelink ||
         value_id == CSSValueID::kWebkitFocusRingColor;
}

Color ResolveQuirkOrLinkOrFocusRingColor(
    CSSValueID value_id,
    const TextLinkColors& text_link_colors,
    mojom::blink::ColorScheme used_color_scheme,
    bool for_visited_link) {
  switch (value_id) {
    case CSSValueID::kInternalQuirkInherit:
      return text_link_colors.TextColor(used_color_scheme);
    case CSSValueID::kWebkitLink:
      return for_visited_link
                 ? text_link_colors.VisitedLinkColor(used_color_scheme)
                 : text_link_colors.LinkColor(used_color_scheme);
    case CSSValueID::kWebkitActivelink:
      return text_link_colors.ActiveLinkColor(used_color_scheme);
    case CSSValueID::kWebkitFocusRingColor:
      return LayoutTheme::GetTheme().FocusRingColor(used_color_scheme);
    default:
      NOTREACHED();
  }
}

ScopedCSSNameList* ConvertNoneOrCustomIdentList(StyleResolverState& state,
                                                const CSSValue& value) {
  DCHECK(value.IsScopedValue());
  DCHECK(value.IsBaseValueList());
  HeapVector<Member<const ScopedCSSName>> names;
  for (const Member<const CSSValue>& item : To<CSSValueList>(value)) {
    names.push_back(
        StyleBuilderConverter::ConvertNoneOrCustomIdent(state, *item));
  }
  return MakeGarbageCollected<ScopedCSSNameList>(std::move(names));
}

}  // namespace

StyleReflection* StyleBuilderConverter::ConvertBoxReflect(
    StyleResolverState& state,
    const CSSValue& value) {
  if (auto* identifier_value = DynamicTo<CSSIdentifierValue>(value)) {
    DCHECK_EQ(identifier_value->GetValueID(), CSSValueID::kNone);
    return ComputedStyleInitialValues::InitialBoxReflect();
  }

  const auto& reflect_value = To<cssvalue::CSSReflectValue>(value);
  StyleReflection* reflection = MakeGarbageCollected<StyleReflection>();
  reflection->SetDirection(
      reflect_value.Direction()->ConvertTo<CSSReflectionDirection>());
  if (reflect_value.Offset()) {
    reflection->SetOffset(reflect_value.Offset()->ConvertToLength(
        state.CssToLengthConversionData()));
  }
  if (reflect_value.Mask()) {
    NinePieceImage mask = NinePieceImage::MaskDefaults();
    CSSToStyleMap::MapNinePieceImage(state, CSSPropertyID::kWebkitBoxReflect,
                                     *reflect_value.Mask(), mask);
    reflection->SetMask(mask);
  }

  return reflection;
}

DynamicRangeLimit StyleBuilderConverter::ConvertDynamicRangeLimit(
    const StyleResolverState& state,
    const CSSValue& value) {
  if (auto* mix_value =
          DynamicTo<cssvalue::CSSDynamicRangeLimitMixValue>(value)) {
    float standard_mix_sum = 0.f;
    float constrained_high_mix_sum = 0.f;
    float fraction_sum = 0.f;
    for (size_t i = 0; i < mix_value->Limits().size(); ++i) {
      const DynamicRangeLimit limit =
          ConvertDynamicRangeLimit(state, *mix_value->Limits()[i]);
      const float fraction =
          0.01f *
          std::clamp(mix_value->Percentages()[i]->ComputePercentage<float>(
                         state.CssToLengthConversionData()),
                     0.f, 100.f);
      fraction_sum += fraction;
      standard_mix_sum += fraction * limit.standard_mix;
      constrained_high_mix_sum += fraction * limit.constrained_high_mix;
    }
    if (fraction_sum == 0.f) {
      return DynamicRangeLimit(cc::PaintFlags::DynamicRangeLimit::kHigh);
    }
    return DynamicRangeLimit(
        /*standard_mix=*/standard_mix_sum / fraction_sum,
        /*constrained_high_mix=*/constrained_high_mix_sum / fraction_sum);
  } else if (auto* identifier_value = DynamicTo<CSSIdentifierValue>(value)) {
    switch (identifier_value->GetValueID()) {
      case CSSValueID::kNoLimit:
        return DynamicRangeLimit(cc::PaintFlags::DynamicRangeLimit::kHigh);
      case CSSValueID::kConstrained:
        return DynamicRangeLimit(
            cc::PaintFlags::DynamicRangeLimit::kConstrainedHigh);
      case CSSValueID::kStandard:
        return DynamicRangeLimit(cc::PaintFlags::DynamicRangeLimit::kStandard);
      default:
        break;
    }
  }
  return DynamicRangeLimit(cc::PaintFlags::DynamicRangeLimit::kHigh);
}

StyleSVGResource* StyleBuilderConverter::ConvertElementReference(
    StyleResolverState& state,
    const CSSValue& value,
    CSSPropertyID property_id) {
  if (auto* identifier_value = DynamicTo<CSSIdentifierValue>(value)) {
    DCHECK_EQ(identifier_value->GetValueID(), CSSValueID::kNone);
    return nullptr;
  }

  const auto& url_value = To<cssvalue::CSSURIValue>(value);
  return MakeGarbageCollected<StyleSVGResource>(
      state.GetSVGResource(property_id, url_value),
      url_value.ValueForSerialization());
}

namespace {

struct BasicShapeAndCoordBox {
  STACK_ALLOCATED();

 public:
  BasicShape* shape;
  GeometryBox box;
};

template <GeometryBox default_box>
BasicShapeAndCoordBox BasicShapeAndCoordBoxForValue(StyleResolverState& state,
                                                    const CSSValue& value) {
  BasicShape* shape = nullptr;
  GeometryBox box = default_box;
  if (const auto* pair = DynamicTo<CSSValuePair>(value)) {
    shape = BasicShapeForValue(state, pair->First());
    box = To<CSSIdentifierValue>(pair->Second()).ConvertTo<GeometryBox>();
  } else {
    shape = BasicShapeForValue(state, value);
  }
  return {shape, box};
}

}  // namespace

StyleBorderShape* StyleBuilderConverter::ConvertBorderShape(
    StyleResolverState& state,
    const CSSValue& value) {
  // Either:
  // - none;
  // - a single shape (meaning default box is half-border-box for stroking);
  // - a pair of shape + box;
  // - list of either: two pairs of shape + box or two shapes.
  if (value.IsIdentifierValue()) {
    CHECK_EQ(To<CSSIdentifierValue>(value).GetValueID(), CSSValueID::kNone);
    return nullptr;
  }

  if (const auto* list = DynamicTo<CSSValueList>(value)) {
    DCHECK_EQ(list->length(), 2u);
    auto [outer_shape, outer_box] =
        BasicShapeAndCoordBoxForValue<GeometryBox::kBorderBox>(state,
                                                               list->First());
    auto [inner_shape, inner_box] =
        BasicShapeAndCoordBoxForValue<GeometryBox::kPaddingBox>(state,
                                                                list->Last());
    return MakeGarbageCollected<StyleBorderShape>(*outer_shape, inner_shape,
                                                  outer_box, inner_box);
  }

  auto [outer_shape, outer_coord_box] =
      BasicShapeAndCoordBoxForValue<GeometryBox::kHalfBorderBox>(state, value);
  return MakeGarbageCollected<StyleBorderShape>(
      *outer_shape, outer_shape, outer_coord_box, outer_coord_box);
}

LengthBox StyleBuilderConverter::ConvertClip(StyleResolverState& state,
                                             const CSSValue& value) {
  const CSSQuadValue& rect = To<CSSQuadValue>(value);

  return LengthBox(ConvertLengthOrAuto(state, *rect.Top()),
                   ConvertLengthOrAuto(state, *rect.Right()),
                   ConvertLengthOrAuto(state, *rect.Bottom()),
                   ConvertLengthOrAuto(state, *rect.Left()));
}

ClipPathOperation* StyleBuilderConverter::ConvertClipPath(
    StyleResolverState& state,
    const CSSValue& value) {
  if (const auto* list = DynamicTo<CSSValueList>(value)) {
    if (list->First().IsBasicShapeValue() || list->First().IsPathValue() ||
        list->First().IsShapeValue()) {
      const CSSValue& shape_value = list->First();
      const CSSIdentifierValue* geometry_box_value = nullptr;
      if (list->length() == 2) {
        geometry_box_value = DynamicTo<CSSIdentifierValue>(list->Item(1));
      }
      if (geometry_box_value) {
        UseCounter::Count(state.GetDocument(),
                          WebFeature::kClipPathGeometryBox);
      }
      // If <geometry-box> is omitted, default to border-box.
      GeometryBox geometry_box =
          geometry_box_value ? geometry_box_value->ConvertTo<GeometryBox>()
                             : GeometryBox::kBorderBox;
      return MakeGarbageCollected<ShapeClipPathOperation>(
          BasicShapeForValue(state, shape_value), geometry_box);
    }
    UseCounter::Count(state.GetDocument(), WebFeature::kClipPathGeometryBox);
    auto& geometry_box_value = To<CSSIdentifierValue>(list->First());
    GeometryBox geometry_box = geometry_box_value.ConvertTo<GeometryBox>();
    return MakeGarbageCollected<GeometryBoxClipPathOperation>(geometry_box);
  }

  if (const auto* url_value = DynamicTo<cssvalue::CSSURIValue>(value)) {
    return MakeGarbageCollected<ReferenceClipPathOperation>(
        url_value->ValueForSerialization(),
        state.GetSVGResource(CSSPropertyID::kClipPath, *url_value));
  }
  auto* identifier_value = DynamicTo<CSSIdentifierValue>(value);
  DCHECK(identifier_value &&
         identifier_value->GetValueID() == CSSValueID::kNone);
  return nullptr;
}

FilterOperations StyleBuilderConverter::ConvertFilterOperations(
    StyleResolverState& state,
    const CSSValue& value,
    CSSPropertyID property_id) {
  return FilterOperationResolver::CreateFilterOperations(state, value,
                                                         property_id);
}

FilterOperations StyleBuilderConverter::ConvertOffscreenFilterOperations(
    const CSSValue& value,
    const Font* font) {
  return FilterOperationResolver::CreateOffscreenFilterOperations(value, font);
}

StyleFlexWrapData StyleBuilderConverter::ConvertFlexWrapData(
    StyleResolverState& state,
    const CSSValue& value) {
  FlexWrapMode wrap_mode = FlexWrapMode::kNowrap;
  bool is_balanced = false;
  uint16_t min_line_count = 1u;
  auto process = [&](const CSSValue& value) {
    if (const CSSPrimitiveValue* primitive =
            DynamicTo<CSSPrimitiveValue>(value)) {
      DCHECK(primitive->IsNumber());
      min_line_count = ClampTo<uint16_t>(ConvertInteger(state, *primitive));
      return;
    }
    const CSSIdentifierValue& identifier = To<CSSIdentifierValue>(value);
    if (identifier.GetValueID() == CSSValueID::kBalance) {
      is_balanced = true;
    } else {
      wrap_mode = identifier.ConvertTo<FlexWrapMode>();
    }
  };

  if (auto* list = DynamicTo<CSSValueList>(value)) {
    for (const auto& entry : *list) {
      process(*entry);
    }
  } else {
    process(value);
  }

  // Coerce the wrapping value if balanced.
  if (is_balanced && wrap_mode == FlexWrapMode::kNowrap) {
    wrap_mode = FlexWrapMode::kWrap;
  }

  return StyleFlexWrapData(wrap_mode, is_balanced, min_line_count);
}

static FontDescription::GenericFamilyType ConvertGenericFamily(
    CSSValueID value_id) {
  switch (value_id) {
    case CSSValueID::kWebkitBody:
      return FontDescription::kWebkitBodyFamily;
    case CSSValueID::kSerif:
      return FontDescription::kSerifFamily;
    case CSSValueID::kSansSerif:
      return FontDescription::kSansSerifFamily;
    case CSSValueID::kCursive:
      return FontDescription::kCursiveFamily;
    case CSSValueID::kFantasy:
      return FontDescription::kFantasyFamily;
    case CSSValueID::kMonospace:
      return FontDescription::kMonospaceFamily;
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
  if (auto* font_family_value = DynamicTo<CSSFontFamilyValue>(value)) {
    generic_family = FontDescription::kNoFamily;
    family_name = font_family_value->Value();
  } else if (font_builder) {
    // TODO(crbug.com/1065468): Get rid of GenericFamilyType.
    auto cssValueID = To<CSSIdentifierValue>(value).GetValueID();
    generic_family = ConvertGenericFamily(cssValueID);
    if (generic_family != FontDescription::kNoFamily) {
      family_name = font_builder->GenericFontFamilyName(generic_family);
      if (document_for_count && cssValueID == CSSValueID::kWebkitBody &&
          !family_name.empty()) {
        // TODO(crbug.com/1065468): Remove this counter when it's no longer
        // necessary.
        document_for_count->CountUse(
            WebFeature::kFontBuilderCSSFontFamilyWebKitPrefixBody);
      }
    } else if (cssValueID == CSSValueID::kSystemUi) {
      family_name = font_family_names::kSystemUi;
    } else if (cssValueID == CSSValueID::kMath) {
      family_name = font_family_names::kMath;
    }
    // Something went wrong with the conversion or retrieving the name from
    // preferences for the specific generic family.
    if (family_name.empty()) {
      return false;
    }
  }

  // Empty font family names (converted from CSSFontFamilyValue above) are
  // acceptable for defining and matching against
  // @font-faces, compare https://github.com/w3c/csswg-drafts/issues/4510.
  return !family_name.IsNull();
}

FontDescription::FamilyDescription StyleBuilderConverterBase::ConvertFontFamily(
    const CSSValue& value,
    FontBuilder* font_builder,
    const Document* document_for_count) {
  FontDescription::FamilyDescription desc(FontDescription::kNoFamily);

  if (const auto* system_font =
          DynamicTo<cssvalue::CSSPendingSystemFontValue>(value)) {
    desc.family = FontFamily(system_font->ResolveFontFamily(),
                             FontFamily::Type::kFamilyName);
    return desc;
  }

#if BUILDFLAG(IS_MAC)
  bool count_blink_mac_system_font = false;
#endif

  AtomicString family_name;
  FontFamily::Type family_type = FontFamily::Type::kFamilyName;
  scoped_refptr<SharedFontFamily> next;
  bool has_value = false;

  for (auto& family : base::Reversed(To<CSSValueList>(value))) {
    AtomicString next_family_name;
    FontDescription::GenericFamilyType generic_family =
        FontDescription::kNoFamily;

    if (!ConvertFontFamilyName(*family, generic_family, next_family_name,
                               font_builder, document_for_count)) {
      continue;
    }

    // TODO(crbug.com/1065468): Get rid of GenericFamilyType.
    const bool is_generic = generic_family != FontDescription::kNoFamily ||
                            IsA<CSSIdentifierValue>(*family);

    // Take the previous value and wrap it in a `SharedFontFamily` adding to
    // the linked list.
    if (has_value) {
      next =
          SharedFontFamily::Create(family_name, family_type, std::move(next));
    }
    family_name = next_family_name;
    family_type = is_generic ? FontFamily::Type::kGenericFamily
                             : FontFamily::Type::kFamilyName;
    has_value = true;

#if BUILDFLAG(IS_MAC)
    // TODO(https://crbug.com/554590): Remove this counter when it's no longer
    // necessary.
    if (IsA<CSSFontFamilyValue>(*family) &&
        family_name == FontCache::LegacySystemFontFamily()) {
      count_blink_mac_system_font = true;
      family_name = font_family_names::kSystemUi;
    } else if (is_generic && family_name == font_family_names::kSystemUi) {
      // If system-ui comes before BlinkMacSystemFont don't use-count.
      count_blink_mac_system_font = false;
    }
#endif

    if (desc.generic_family == FontDescription::GenericFamilyType::kNoFamily) {
      desc.generic_family = generic_family;
    }
  }

#if BUILDFLAG(IS_MAC)
  if (document_for_count && count_blink_mac_system_font) {
    document_for_count->CountUse(WebFeature::kBlinkMacSystemFont);
  }
#endif

  desc.family = FontFamily(family_name, family_type, std::move(next));
  return desc;
}

FontDescription::FamilyDescription StyleBuilderConverter::ConvertFontFamily(
    StyleResolverState& state,
    const CSSValue& value) {
  // TODO(crbug.com/336876): Use the correct tree scope.
  state.GetFontBuilder().SetFamilyTreeScope(&state.GetDocument());
  return StyleBuilderConverterBase::ConvertFontFamily(
      value,
      state.GetDocument().GetSettings() ? &state.GetFontBuilder() : nullptr,
      &state.GetDocument());
}

FontDescription::Kerning StyleBuilderConverter::ConvertFontKerning(
    StyleResolverState&,
    const CSSValue& value) {
  // When the font shorthand is specified, font-kerning property should
  // be reset to it's initial value.In this case, the CSS parser uses a special
  // value CSSPendingSystemFontValue to defer resolution of system font
  // properties. The auto generated converter does not handle this incoming
  // value.
  if (value.IsPendingSystemFontValue()) {
    return FontDescription::kAutoKerning;
  }

  CSSValueID value_id = To<CSSIdentifierValue>(value).GetValueID();
  switch (value_id) {
    case CSSValueID::kAuto:
      return FontDescription::kAutoKerning;
    case CSSValueID::kNormal:
      return FontDescription::kNormalKerning;
    case CSSValueID::kNone:
      return FontDescription::kNoneKerning;
    default:
      NOTREACHED();
  }
}

FontDescription::FontVariantPosition
StyleBuilderConverter::ConvertFontVariantPosition(StyleResolverState&,
                                                  const CSSValue& value) {
  // When the font shorthand is specified, font-variant-position property should
  // be reset to it's initial value. In this case, the CSS parser uses a special
  // value CSSPendingSystemFontValue to defer resolution of system font
  // properties. The auto generated converter does not handle this incoming
  // value.
  if (value.IsPendingSystemFontValue()) {
    return FontDescription::kNormalVariantPosition;
  }

  CSSValueID value_id = To<CSSIdentifierValue>(value).GetValueID();
  switch (value_id) {
    case CSSValueID::kNormal:
      return FontDescription::kNormalVariantPosition;
    case CSSValueID::kSub:
      return FontDescription::kSubVariantPosition;
    case CSSValueID::kSuper:
      return FontDescription::kSuperVariantPosition;
    default:
      NOTREACHED();
  }
}

FontVariantEmoji StyleBuilderConverter::ConvertFontVariantEmoji(
    StyleResolverState&,
    const CSSValue& value) {
  // When the font shorthand is specified, font-variant-emoji property should
  // be reset to it's initial value. In this case, the CSS parser uses a special
  // value CSSPendingSystemFontValue to defer resolution of system font
  // properties. The auto generated converter does not handle this incoming
  // value.
  if (value.IsPendingSystemFontValue()) {
    return kNormalVariantEmoji;
  }

  return To<CSSIdentifierValue>(value).ConvertTo<FontVariantEmoji>();
}

OpticalSizing StyleBuilderConverter::ConvertFontOpticalSizing(
    StyleResolverState&,
    const CSSValue& value) {
  // When the font shorthand is specified, font-optical-sizing property should
  // be reset to it's initial value. In this case, the CSS parser uses a special
  // value CSSPendingSystemFontValue to defer resolution of system font
  // properties. The auto generated converter does not handle this incoming
  // value.
  if (value.IsPendingSystemFontValue()) {
    return kAutoOpticalSizing;
  }

  CSSValueID value_id = To<CSSIdentifierValue>(value).GetValueID();
  switch (value_id) {
    case CSSValueID::kAuto:
      return kAutoOpticalSizing;
    case CSSValueID::kNone:
      return kNoneOpticalSizing;
    default:
      NOTREACHED();
  }
}

scoped_refptr<FontFeatureSettings>
StyleBuilderConverter::ConvertFontFeatureSettings(StyleResolverState& state,
                                                  const CSSValue& value) {
  return StyleBuilderConverterBase::ConvertFontFeatureSettings(
      state.CssToLengthConversionData(), value);
}

scoped_refptr<FontFeatureSettings>
StyleBuilderConverterBase::ConvertFontFeatureSettings(
    const CSSLengthResolver& length_resolver,
    const CSSValue& value) {
  auto* identifier_value = DynamicTo<CSSIdentifierValue>(value);
  if (identifier_value &&
      identifier_value->GetValueID() == CSSValueID::kNormal) {
    return FontFeatureSettings::Create();
  }

  if (value.IsPendingSystemFontValue()) {
    return FontFeatureSettings::Create();
  }

  const auto& list = To<CSSValueList>(value);
  std::map<uint32_t, int> features;

  for (const auto& css_value : list) {
    const auto& feature = To<cssvalue::CSSFontFeatureValue>(*css_value);
    features[AtomicStringToFourByteTag(feature.Tag())] =
        feature.Value(length_resolver);
  }

  scoped_refptr<FontFeatureSettings> settings = FontFeatureSettings::Create();
  for (const auto& [tag, feature_value] : features) {
    settings->Append(FontFeature(tag, feature_value));
  }

  DCHECK(std::is_sorted(settings->begin(), settings->end()));
  return settings;
}

static bool CompareTags(FontVariationAxis a, FontVariationAxis b) {
  return a.Tag() < b.Tag();
}

scoped_refptr<FontVariationSettings>
StyleBuilderConverter::ConvertFontVariationSettings(
    const StyleResolverState& state,
    const CSSValue& value) {
  auto* identifier_value = DynamicTo<CSSIdentifierValue>(value);
  if (identifier_value &&
      identifier_value->GetValueID() == CSSValueID::kNormal) {
    return FontBuilder::InitialVariationSettings();
  }

  if (value.IsPendingSystemFontValue()) {
    return FontBuilder::InitialVariationSettings();
  }

  const auto& list = To<CSSValueList>(value);
  int len = list.length();
  HashMap<uint32_t, float> axes;
  // Use a temporary HashMap to remove duplicate tags, keeping the last
  // occurrence of each.
  for (int i = 0; i < len; ++i) {
    const auto& feature = To<cssvalue::CSSFontVariationValue>(list.Item(i));
    axes.Set(
        AtomicStringToFourByteTag(feature.Tag()),
        feature.Value()->ConvertTo<float>(state.CssToLengthConversionData()));
  }
  scoped_refptr<FontVariationSettings> settings =
      FontVariationSettings::Create();
  for (auto& axis : axes) {
    settings->Append(FontVariationAxis(axis.key, axis.value));
  }
  std::sort(settings->begin(), settings->end(), CompareTags);
  return settings;
}

AtomicString StyleBuilderConverter::ConvertFontLanguageOverride(
    StyleResolverState& state,
    const CSSValue& value) {
  const auto* identifier_value = DynamicTo<CSSIdentifierValue>(value);
  if (identifier_value &&
      identifier_value->GetValueID() == CSSValueID::kNormal) {
    return AtomicString();
  }
  if (auto* string_value = DynamicTo<CSSStringValue>(value)) {
    return AtomicString(string_value->Value());
  }
  return AtomicString();
}

scoped_refptr<FontPalette> StyleBuilderConverter::ConvertFontPalette(
    StyleResolverState& state,
    const CSSValue& value) {
  return StyleBuilderConverterBase::ConvertFontPalette(
      state.CssToLengthConversionData(), value);
}

scoped_refptr<FontPalette> StyleBuilderConverterBase::ConvertPaletteMix(
    const CSSLengthResolver& length_resolver,
    const CSSValue& value) {
  auto* palette_mix_value = DynamicTo<cssvalue::CSSPaletteMixValue>(value);
  if (palette_mix_value) {
    scoped_refptr<FontPalette> palette1 =
        ConvertFontPalette(length_resolver, palette_mix_value->Palette1());
    if (palette1 == nullptr) {
      // Use normal palette.
      palette1 = FontPalette::Create();
    }
    scoped_refptr<FontPalette> palette2 =
        ConvertFontPalette(length_resolver, palette_mix_value->Palette2());
    if (palette2 == nullptr) {
      palette2 = FontPalette::Create();
    }

    Color::ColorSpace color_space =
        palette_mix_value->ColorInterpolationSpace();
    Color::HueInterpolationMethod hue_interpolation_method =
        palette_mix_value->HueInterpolationMethod();

    double alpha_multiplier;
    double normalized_percentage;
    if (cssvalue::CSSColorMixValue::NormalizePercentages(
            palette_mix_value->Percentage1(), palette_mix_value->Percentage2(),
            normalized_percentage, alpha_multiplier, length_resolver)) {
      double percentage1 = kMiddleStatePercentage;
      double percentage2 = kMiddleStatePercentage;
      if (palette_mix_value->Percentage1() &&
          palette_mix_value->Percentage2()) {
        percentage1 = palette_mix_value->Percentage1()->ComputePercentage(
            length_resolver);
        percentage2 = palette_mix_value->Percentage2()->ComputePercentage(
            length_resolver);
      } else if (palette_mix_value->Percentage1()) {
        percentage1 = palette_mix_value->Percentage1()->ComputePercentage(
            length_resolver);
        percentage2 = kFinalStatePercentage - percentage1;
      } else if (palette_mix_value->Percentage2()) {
        percentage2 = palette_mix_value->Percentage2()->ComputePercentage(
            length_resolver);
        percentage1 = kFinalStatePercentage - percentage2;
      }
      return FontPalette::Mix(palette1, palette2, percentage1, percentage2,
                              normalized_percentage, alpha_multiplier,
                              color_space, hue_interpolation_method);
    }
  }
  return nullptr;
}

scoped_refptr<FontPalette> StyleBuilderConverterBase::ConvertFontPalette(
    const CSSLengthResolver& length_resolver,
    const CSSValue& value) {
  auto* identifier_value = DynamicTo<CSSIdentifierValue>(value);
  if (identifier_value &&
      identifier_value->GetValueID() == CSSValueID::kNormal) {
    return nullptr;
  }

  if (identifier_value && identifier_value->GetValueID() == CSSValueID::kDark) {
    return FontPalette::Create(FontPalette::kDarkPalette);
  }

  if (identifier_value &&
      identifier_value->GetValueID() == CSSValueID::kLight) {
    return FontPalette::Create(FontPalette::kLightPalette);
  }

  auto* custom_identifier = DynamicTo<CSSCustomIdentValue>(value);
  if (custom_identifier) {
    return FontPalette::Create(custom_identifier->Value());
  }

  return ConvertPaletteMix(length_resolver, value);
}

float MathScriptScaleFactor(StyleResolverState& state) {
  int a = state.ParentStyle()->MathDepth();
  int b = state.StyleBuilder().MathDepth();
  if (b == a) {
    return 1.0;
  }
  bool invertScaleFactor = false;
  if (b < a) {
    std::swap(a, b);
    invertScaleFactor = true;
  }

  // Determine the scale factors from the inherited font.
  float defaultScaleDown = 0.71;
  int exponent = b - a;
  float scaleFactor = 1.0;
  if (const SimpleFontData* font_data =
          state.ParentStyle()->GetFont()->PrimaryFont()) {
    HarfBuzzFace* parent_harfbuzz_face =
        font_data->PlatformData().GetHarfBuzzFace();
    if (OpenTypeMathSupport::HasMathData(parent_harfbuzz_face)) {
      float scriptPercentScaleDown =
          OpenTypeMathSupport::MathConstant(
              parent_harfbuzz_face,
              OpenTypeMathSupport::MathConstants::kScriptPercentScaleDown)
              .value_or(0);
      // Note: zero can mean both zero for the math constant and the fallback.
      if (!scriptPercentScaleDown) {
        scriptPercentScaleDown = defaultScaleDown;
      }
      float scriptScriptPercentScaleDown =
          OpenTypeMathSupport::MathConstant(
              parent_harfbuzz_face,
              OpenTypeMathSupport::MathConstants::kScriptScriptPercentScaleDown)
              .value_or(0);
      // Note: zero can mean both zero for the math constant and the fallback.
      if (!scriptScriptPercentScaleDown) {
        scriptScriptPercentScaleDown = defaultScaleDown * defaultScaleDown;
      }
      if (a <= 0 && b >= 2) {
        scaleFactor *= scriptScriptPercentScaleDown;
        exponent -= 2;
      } else if (a == 1) {
        scaleFactor *= scriptScriptPercentScaleDown / scriptPercentScaleDown;
        exponent--;
      } else if (b == 1) {
        scaleFactor *= scriptPercentScaleDown;
        exponent--;
      }
    }
  }
  scaleFactor *= pow(defaultScaleDown, exponent);
  return invertScaleFactor ? 1 / scaleFactor : scaleFactor;
}

static float ComputeFontSize(const CSSToLengthConversionData& conversion_data,
                             const CSSPrimitiveValue& primitive_value,
                             const FontDescription::Size& parent_size) {
  if (primitive_value.IsLength()) {
    return primitive_value.ComputeLength<float>(conversion_data);
  }
  if (primitive_value.IsCalculated()) {
    return To<CSSMathFunctionValue>(primitive_value)
        .ToCalcValue(conversion_data)
        ->Evaluate(parent_size.value);
  }
  NOTREACHED();
}

FontDescription::Size StyleBuilderConverterBase::ConvertFontSize(
    const CSSValue& value,
    const CSSToLengthConversionData& conversion_data,
    FontDescription::Size parent_size,
    const Document* document) {
  if (auto* identifier_value = DynamicTo<CSSIdentifierValue>(value)) {
    CSSValueID value_id = identifier_value->GetValueID();
    if (FontSizeFunctions::IsValidValueID(value_id)) {
      return FontDescription::Size(FontSizeFunctions::KeywordSize(value_id),
                                   0.0f, false);
    }
    if (value_id == CSSValueID::kSmaller) {
      return FontDescription::SmallerSize(parent_size);
    }
    if (value_id == CSSValueID::kLarger) {
      return FontDescription::LargerSize(parent_size);
    }
    NOTREACHED();
  }

  if (const auto* system_font =
          DynamicTo<cssvalue::CSSPendingSystemFontValue>(value)) {
    return FontDescription::Size(0, system_font->ResolveFontSize(document),
                                 true);
  }

  const auto& primitive_value = To<CSSPrimitiveValue>(value);
  if (primitive_value.IsPercentage()) {
    return FontDescription::Size(
        /*keyword=*/0,
        (primitive_value.ComputePercentage(conversion_data) *
         parent_size.value / 100.0f),
        parent_size.is_absolute);
  }

  // TODO(crbug.com/979895): This is the result of a refactoring, which might
  // have revealed an existing bug with calculated lengths. Investigate.
  const bool is_absolute =
      parent_size.is_absolute || primitive_value.IsMathFunctionValue() ||
      !To<CSSNumericLiteralValue>(primitive_value).IsFontRelativeLength() ||
      To<CSSNumericLiteralValue>(primitive_value).GetType() ==
          CSSPrimitiveValue::UnitType::kRems;
  return FontDescription::Size(
      /*keyword=*/0,
      ComputeFontSize(conversion_data, primitive_value, parent_size),
      is_absolute);
}

FontDescription::Size StyleBuilderConverter::ConvertFontSize(
    StyleResolverState& state,
    const CSSValue& value) {
  // FIXME: Find out when parentStyle could be 0?
  auto parent_size = state.ParentStyle()
                         ? state.ParentFontDescription().GetSize()
                         : FontDescription::Size(0, 0.0f, false);

  auto* identifier_value = DynamicTo<CSSIdentifierValue>(value);
  if (identifier_value && identifier_value->GetValueID() == CSSValueID::kMath) {
    auto scale_factor = MathScriptScaleFactor(state);
    state.StyleBuilder().SetHasGlyphRelativeUnits();
    return FontDescription::Size(0, (scale_factor * parent_size.value),
                                 parent_size.is_absolute);
  }

  return StyleBuilderConverterBase::ConvertFontSize(
      value, state.FontSizeConversionData(), parent_size, &state.GetDocument());
}

FontSizeAdjust StyleBuilderConverterBase::ConvertFontSizeAdjust(
    const StyleResolverState& state,
    const CSSValue& value) {
  auto* identifier_value = DynamicTo<CSSIdentifierValue>(value);
  if (identifier_value && identifier_value->GetValueID() == CSSValueID::kNone) {
    return FontBuilder::InitialSizeAdjust();
  }

  if (value.IsPendingSystemFontValue()) {
    return FontBuilder::InitialSizeAdjust();
  }

  if (identifier_value &&
      identifier_value->GetValueID() == CSSValueID::kFromFont) {
    return FontSizeAdjust(FontSizeAdjust::kFontSizeAdjustNone,
                          FontSizeAdjust::ValueType::kFromFont);
  }

  if (value.IsPrimitiveValue()) {
    const auto& primitive_value = To<CSSPrimitiveValue>(value);
    DCHECK(primitive_value.IsNumber());
    return FontSizeAdjust(
        primitive_value.ComputeNumber(state.CssToLengthConversionData()));
  }

  DCHECK(value.IsValuePair());
  const auto& pair = To<CSSValuePair>(value);
  auto metric =
      To<CSSIdentifierValue>(pair.First()).ConvertTo<FontSizeAdjust::Metric>();

  if (pair.Second().IsPrimitiveValue()) {
    const auto& primitive_value = To<CSSPrimitiveValue>(pair.Second());
    DCHECK(primitive_value.IsNumber());
    return FontSizeAdjust(
        primitive_value.ComputeNumber(state.CssToLengthConversionData()),
        metric);
  }

  DCHECK(To<CSSIdentifierValue>(pair.Second()).GetValueID() ==
         CSSValueID::kFromFont);
  return FontSizeAdjust(FontSizeAdjust::kFontSizeAdjustNone, metric,
                        FontSizeAdjust::ValueType::kFromFont);
}

FontSizeAdjust StyleBuilderConverter::ConvertFontSizeAdjust(
    StyleResolverState& state,
    const CSSValue& value) {
  return StyleBuilderConverterBase::ConvertFontSizeAdjust(state, value);
}

std::optional<FontSelectionValue>
StyleBuilderConverter::ConvertFontStretchKeyword(const CSSValue& value) {
  // TODO(drott) crbug.com/750014: Consider not parsing them as IdentifierValue
  // any more?
  if (const auto* identifier_value = DynamicTo<CSSIdentifierValue>(value)) {
    switch (identifier_value->GetValueID()) {
      case CSSValueID::kUltraCondensed:
        return kUltraCondensedWidthValue;
      case CSSValueID::kExtraCondensed:
        return kExtraCondensedWidthValue;
      case CSSValueID::kCondensed:
        return kCondensedWidthValue;
      case CSSValueID::kSemiCondensed:
        return kSemiCondensedWidthValue;
      case CSSValueID::kNormal:
        return kNormalWidthValue;
      case CSSValueID::kSemiExpanded:
        return kSemiExpandedWidthValue;
      case CSSValueID::kExpanded:
        return kExpandedWidthValue;
      case CSSValueID::kExtraExpanded:
        return kExtraExpandedWidthValue;
      case CSSValueID::kUltraExpanded:
        return kUltraExpandedWidthValue;
      default:
        break;
    }
  }
  return {};
}

FontSelectionValue StyleBuilderConverterBase::ConvertFontStretch(
    const CSSLengthResolver& length_resolver,
    const blink::CSSValue& value) {
  if (const auto* primitive_value = DynamicTo<CSSPrimitiveValue>(value)) {
    if (primitive_value->IsPercentage()) {
      return ClampTo<FontSelectionValue>(
          primitive_value->ComputePercentage(length_resolver));
    }
  }

  if (std::optional<FontSelectionValue> keyword =
          StyleBuilderConverter::ConvertFontStretchKeyword(value);
      keyword.has_value()) {
    return keyword.value();
  }

  if (value.IsPendingSystemFontValue()) {
    return kNormalWidthValue;
  }

  NOTREACHED();
}

FontSelectionValue StyleBuilderConverter::ConvertFontStretch(
    blink::StyleResolverState& state,
    const blink::CSSValue& value) {
  return StyleBuilderConverterBase::ConvertFontStretch(
      state.CssToLengthConversionData(), value);
}

FontSelectionValue StyleBuilderConverterBase::ConvertFontStyle(
    const CSSLengthResolver& length_resolver,
    const CSSValue& value) {
  DCHECK(!value.IsPrimitiveValue());

  if (const auto* identifier_value = DynamicTo<CSSIdentifierValue>(value)) {
    switch (identifier_value->GetValueID()) {
      case CSSValueID::kItalic:
      case CSSValueID::kOblique:
        return kItalicSlopeValue;
      case CSSValueID::kNormal:
        return kNormalSlopeValue;
      default:
        NOTREACHED();
    }
  } else if (IsA<cssvalue::CSSPendingSystemFontValue>(value)) {
    return kNormalSlopeValue;
  } else if (const auto* style_range_value =
                 DynamicTo<cssvalue::CSSFontStyleRangeValue>(value)) {
    const CSSValueList* values = style_range_value->GetObliqueValues();
    CHECK_LT(values->length(), 2u);
    if (values->length()) {
      const double angle_degrees = To<CSSPrimitiveValue>(values->Item(0))
                                       .ComputeDegrees(length_resolver);
      if (RuntimeEnabledFeatures::FontStyleObliqueZeroDegreeAsNormalEnabled() &&
          angle_degrees == 0.0) {
        return kNormalSlopeValue;
      }
      return FontSelectionValue(angle_degrees);
    } else {
      identifier_value = style_range_value->GetFontStyleValue();
      if (identifier_value->GetValueID() == CSSValueID::kNormal) {
        return kNormalSlopeValue;
      }
      if (identifier_value->GetValueID() == CSSValueID::kItalic ||
          identifier_value->GetValueID() == CSSValueID::kOblique) {
        return kItalicSlopeValue;
      }
    }
  }

  NOTREACHED();
}

FontSelectionValue StyleBuilderConverter::ConvertFontStyle(
    StyleResolverState& state,
    const CSSValue& value) {
  return StyleBuilderConverterBase::ConvertFontStyle(
      state.CssToLengthConversionData(), value);
}

FontSelectionValue StyleBuilderConverterBase::ConvertFontWeight(
    const CSSLengthResolver& length_resolver,
    const CSSValue& value,
    FontSelectionValue parent_weight) {
  if (const auto* primitive_value = DynamicTo<CSSPrimitiveValue>(value)) {
    if (primitive_value->IsNumber()) {
      // clamp to [1, 1000] range as per
      // https://drafts.csswg.org/css-fonts/#font-weight-prop.
      return ClampTo<FontSelectionValue>(std::clamp(
          primitive_value->ComputeNumber(length_resolver), 1., 1000.));
    }
  }

  if (IsA<cssvalue::CSSPendingSystemFontValue>(value)) {
    return kNormalWeightValue;
  }

  if (const auto* identifier_value = DynamicTo<CSSIdentifierValue>(value)) {
    switch (identifier_value->GetValueID()) {
      case CSSValueID::kNormal:
        return kNormalWeightValue;
      case CSSValueID::kBold:
        return kBoldWeightValue;
      case CSSValueID::kBolder:
        return FontDescription::BolderWeight(parent_weight);
      case CSSValueID::kLighter:
        return FontDescription::LighterWeight(parent_weight);
      default:
        NOTREACHED();
    }
  }
  NOTREACHED();
}

FontSelectionValue StyleBuilderConverter::ConvertFontWeight(
    StyleResolverState& state,
    const CSSValue& value) {
  return StyleBuilderConverterBase::ConvertFontWeight(
      state.CssToLengthConversionData(), value,
      state.ParentStyle()->GetFontDescription().Weight());
}

FontDescription::FontVariantCaps
StyleBuilderConverterBase::ConvertFontVariantCaps(const CSSValue& value) {
  if (value.IsPendingSystemFontValue()) {
    return FontDescription::kCapsNormal;
  }

  CSSValueID value_id = To<CSSIdentifierValue>(value).GetValueID();
  switch (value_id) {
    case CSSValueID::kNormal:
      return FontDescription::kCapsNormal;
    case CSSValueID::kSmallCaps:
      return FontDescription::kSmallCaps;
    case CSSValueID::kAllSmallCaps:
      return FontDescription::kAllSmallCaps;
    case CSSValueID::kPetiteCaps:
      return FontDescription::kPetiteCaps;
    case CSSValueID::kAllPetiteCaps:
      return FontDescription::kAllPetiteCaps;
    case CSSValueID::kUnicase:
      return FontDescription::kUnicase;
    case CSSValueID::kTitlingCaps:
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
  if (const auto* value_list = DynamicTo<CSSValueList>(value)) {
    FontDescription::VariantLigatures ligatures;
    for (wtf_size_t i = 0; i < value_list->length(); ++i) {
      const CSSValue& item = value_list->Item(i);
      switch (To<CSSIdentifierValue>(item).GetValueID()) {
        case CSSValueID::kNoCommonLigatures:
          ligatures.common = FontDescription::kDisabledLigaturesState;
          break;
        case CSSValueID::kCommonLigatures:
          ligatures.common = FontDescription::kEnabledLigaturesState;
          break;
        case CSSValueID::kNoDiscretionaryLigatures:
          ligatures.discretionary = FontDescription::kDisabledLigaturesState;
          break;
        case CSSValueID::kDiscretionaryLigatures:
          ligatures.discretionary = FontDescription::kEnabledLigaturesState;
          break;
        case CSSValueID::kNoHistoricalLigatures:
          ligatures.historical = FontDescription::kDisabledLigaturesState;
          break;
        case CSSValueID::kHistoricalLigatures:
          ligatures.historical = FontDescription::kEnabledLigaturesState;
          break;
        case CSSValueID::kNoContextual:
          ligatures.contextual = FontDescription::kDisabledLigaturesState;
          break;
        case CSSValueID::kContextual:
          ligatures.contextual = FontDescription::kEnabledLigaturesState;
          break;
        default:
          NOTREACHED();
      }
    }
    return ligatures;
  }

  if (value.IsPendingSystemFontValue()) {
    return FontDescription::VariantLigatures();
  }

  if (To<CSSIdentifierValue>(value).GetValueID() == CSSValueID::kNone) {
    return FontDescription::VariantLigatures(
        FontDescription::kDisabledLigaturesState);
  }

  DCHECK_EQ(To<CSSIdentifierValue>(value).GetValueID(), CSSValueID::kNormal);
  return FontDescription::VariantLigatures();
}

FontVariantNumeric StyleBuilderConverter::ConvertFontVariantNumeric(
    StyleResolverState&,
    const CSSValue& value) {
  if (auto* identifier_value = DynamicTo<CSSIdentifierValue>(value)) {
    DCHECK_EQ(identifier_value->GetValueID(), CSSValueID::kNormal);
    return FontVariantNumeric();
  }

  if (value.IsPendingSystemFontValue()) {
    return FontVariantNumeric();
  }

  FontVariantNumeric variant_numeric;
  for (const CSSValue* feature : To<CSSValueList>(value)) {
    switch (To<CSSIdentifierValue>(feature)->GetValueID()) {
      case CSSValueID::kLiningNums:
        variant_numeric.SetNumericFigure(FontVariantNumeric::kLiningNums);
        break;
      case CSSValueID::kOldstyleNums:
        variant_numeric.SetNumericFigure(FontVariantNumeric::kOldstyleNums);
        break;
      case CSSValueID::kProportionalNums:
        variant_numeric.SetNumericSpacing(
            FontVariantNumeric::kProportionalNums);
        break;
      case CSSValueID::kTabularNums:
        variant_numeric.SetNumericSpacing(FontVariantNumeric::kTabularNums);
        break;
      case CSSValueID::kDiagonalFractions:
        variant_numeric.SetNumericFraction(
            FontVariantNumeric::kDiagonalFractions);
        break;
      case CSSValueID::kStackedFractions:
        variant_numeric.SetNumericFraction(
            FontVariantNumeric::kStackedFractions);
        break;
      case CSSValueID::kOrdinal:
        variant_numeric.SetOrdinal(FontVariantNumeric::kOrdinalOn);
        break;
      case CSSValueID::kSlashedZero:
        variant_numeric.SetSlashedZero(FontVariantNumeric::kSlashedZeroOn);
        break;
      default:
        NOTREACHED();
    }
  }
  return variant_numeric;
}

scoped_refptr<FontVariantAlternates>
StyleBuilderConverter::ConvertFontVariantAlternates(StyleResolverState&,
                                                    const CSSValue& value) {
  scoped_refptr<FontVariantAlternates> alternates =
      FontVariantAlternates::Create();
  // See FontVariantAlternates::ParseSingleValue - we either receive the normal
  // identifier or a list of 1 or more elements if it's non normal.
  if (auto* identifier_value = DynamicTo<CSSIdentifierValue>(value)) {
    DCHECK_EQ(identifier_value->GetValueID(), CSSValueID::kNormal);
    return nullptr;
  }

  if (value.IsPendingSystemFontValue()) {
    return nullptr;
  }

  // If it's not the single normal identifier, it has to be a list.
  for (const CSSValue* alternate : To<CSSValueList>(value)) {
    const cssvalue::CSSAlternateValue* alternate_value =
        DynamicTo<cssvalue::CSSAlternateValue>(alternate);
    if (alternate_value) {
      switch (alternate_value->Function().FunctionType()) {
        case CSSValueID::kStylistic:
          alternates->SetStylistic(
              FirstEntryAsAtomicString(alternate_value->Aliases()));
          break;
        case CSSValueID::kSwash:
          alternates->SetSwash(
              FirstEntryAsAtomicString(alternate_value->Aliases()));
          break;
        case CSSValueID::kOrnaments:
          alternates->SetOrnaments(
              FirstEntryAsAtomicString(alternate_value->Aliases()));
          break;
        case CSSValueID::kAnnotation:
          alternates->SetAnnotation(
              FirstEntryAsAtomicString(alternate_value->Aliases()));
          break;
        case CSSValueID::kStyleset:
          alternates->SetStyleset(
              ValueListToAtomicStringVector(alternate_value->Aliases()));
          break;
        case CSSValueID::kCharacterVariant:
          alternates->SetCharacterVariant(
              ValueListToAtomicStringVector(alternate_value->Aliases()));
          break;
        default:
          NOTREACHED();
      }
    }
    const CSSIdentifierValue* alternate_value_ident =
        DynamicTo<CSSIdentifierValue>(alternate);
    if (alternate_value_ident) {
      DCHECK_EQ(alternate_value_ident->GetValueID(),
                CSSValueID::kHistoricalForms);
      alternates->SetHistoricalForms();
    }
  }

  if (alternates->IsNormal()) {
    return nullptr;
  }

  return alternates;
}

FontVariantEastAsian StyleBuilderConverter::ConvertFontVariantEastAsian(
    StyleResolverState&,
    const CSSValue& value) {
  if (auto* identifier_value = DynamicTo<CSSIdentifierValue>(value)) {
    DCHECK_EQ(identifier_value->GetValueID(), CSSValueID::kNormal);
    return FontVariantEastAsian();
  }

  if (value.IsPendingSystemFontValue()) {
    return FontVariantEastAsian();
  }

  FontVariantEastAsian variant_east_asian;
  for (const CSSValue* feature : To<CSSValueList>(value)) {
    switch (To<CSSIdentifierValue>(feature)->GetValueID()) {
      case CSSValueID::kJis78:
        variant_east_asian.SetForm(FontVariantEastAsian::kJis78);
        break;
      case CSSValueID::kJis83:
        variant_east_asian.SetForm(FontVariantEastAsian::kJis83);
        break;
      case CSSValueID::kJis90:
        variant_east_asian.SetForm(FontVariantEastAsian::kJis90);
        break;
      case CSSValueID::kJis04:
        variant_east_asian.SetForm(FontVariantEastAsian::kJis04);
        break;
      case CSSValueID::kSimplified:
        variant_east_asian.SetForm(FontVariantEastAsian::kSimplified);
        break;
      case CSSValueID::kTraditional:
        variant_east_asian.SetForm(FontVariantEastAsian::kTraditional);
        break;
      case CSSValueID::kFullWidth:
        variant_east_asian.SetWidth(FontVariantEastAsian::kFullWidth);
        break;
      case CSSValueID::kProportionalWidth:
        variant_east_asian.SetWidth(FontVariantEastAsian::kProportionalWidth);
        break;
      case CSSValueID::kRuby:
        variant_east_asian.SetRuby(true);
        break;
      default:
        NOTREACHED();
    }
  }
  return variant_east_asian;
}

StyleSelfAlignmentData StyleBuilderConverter::ConvertSelfOrDefaultAlignmentData(
    StyleResolverState&,
    const CSSValue& value) {
  StyleSelfAlignmentData alignment_data =
      ComputedStyleInitialValues::InitialAlignSelf();
  if (const auto* pair = DynamicTo<CSSValuePair>(value)) {
    if (To<CSSIdentifierValue>(pair->First()).GetValueID() ==
        CSSValueID::kLegacy) {
      alignment_data.SetPositionType(ItemPositionType::kLegacy);
      alignment_data.SetPosition(
          To<CSSIdentifierValue>(pair->Second()).ConvertTo<ItemPosition>());
    } else if (To<CSSIdentifierValue>(pair->First()).GetValueID() ==
               CSSValueID::kFirst) {
      alignment_data.SetPosition(ItemPosition::kBaseline);
    } else if (To<CSSIdentifierValue>(pair->First()).GetValueID() ==
               CSSValueID::kLast) {
      alignment_data.SetPosition(ItemPosition::kLastBaseline);
    } else {
      alignment_data.SetOverflow(
          To<CSSIdentifierValue>(pair->First()).ConvertTo<OverflowAlignment>());
      alignment_data.SetPosition(
          To<CSSIdentifierValue>(pair->Second()).ConvertTo<ItemPosition>());
    }
  } else {
    alignment_data.SetPosition(
        To<CSSIdentifierValue>(value).ConvertTo<ItemPosition>());
  }
  return alignment_data;
}

StyleContentAlignmentData StyleBuilderConverter::ConvertContentAlignmentData(
    StyleResolverState&,
    const CSSValue& value) {
  StyleContentAlignmentData alignment_data =
      ComputedStyleInitialValues::InitialContentAlignment();
  const cssvalue::CSSContentDistributionValue& content_value =
      To<cssvalue::CSSContentDistributionValue>(value);
  if (IsValidCSSValueID(content_value.Distribution())) {
    alignment_data.SetDistribution(
        CSSIdentifierValue::Create(content_value.Distribution())
            ->ConvertTo<ContentDistributionType>());
  }
  if (IsValidCSSValueID(content_value.Position())) {
    alignment_data.SetPosition(
        CSSIdentifierValue::Create(content_value.Position())
            ->ConvertTo<ContentPosition>());
  }
  if (IsValidCSSValueID(content_value.Overflow())) {
    alignment_data.SetOverflow(
        CSSIdentifierValue::Create(content_value.Overflow())
            ->ConvertTo<OverflowAlignment>());
  }

  return alignment_data;
}

GridAutoFlow StyleBuilderConverter::ConvertGridAutoFlow(
    StyleResolverState& state,
    const CSSValue& value) {
  const auto* list = DynamicTo<CSSValueList>(&value);
  if (list) {
    DCHECK_GE(list->length(), 1u);
  } else {
    DCHECK(value.IsIdentifierValue());
  }

  const CSSIdentifierValue& first =
      To<CSSIdentifierValue>(list ? list->Item(0) : value);
  const CSSIdentifierValue* second =
      list && list->length() == 2 ? &To<CSSIdentifierValue>(list->Item(1))
                                  : nullptr;

  switch (first.GetValueID()) {
    case CSSValueID::kRow:
      if (second && second->GetValueID() == CSSValueID::kDense) {
        UseCounter::Count(state.GetDocument(),
                          WebFeature::kGridAutoFlowRowDense);
        return kAutoFlowRowDense;
      }
      return kAutoFlowRow;
    case CSSValueID::kColumn:
      if (second && second->GetValueID() == CSSValueID::kDense) {
        UseCounter::Count(state.GetDocument(),
                          WebFeature::kGridAutoFlowColumnDense);
        return kAutoFlowColumnDense;
      }
      return kAutoFlowColumn;
    case CSSValueID::kDense:
      if (second && second->GetValueID() == CSSValueID::kColumn) {
        UseCounter::Count(state.GetDocument(),
                          WebFeature::kGridAutoFlowColumnDense);
        return kAutoFlowColumnDense;
      }
      UseCounter::Count(state.GetDocument(), WebFeature::kGridAutoFlowRowDense);
      return kAutoFlowRowDense;
    default:
      NOTREACHED();
  }
}

GridPosition StyleBuilderConverter::ConvertGridPosition(
    StyleResolverState& state,
    const CSSValue& value) {
  // We accept the specification's grammar:
  // 'auto' | [ <integer> || <custom-ident> ] |
  // [ span && [ <integer> || <custom-ident> ] ] | <custom-ident>

  GridPosition position;

  if (auto* ident_value = DynamicTo<CSSCustomIdentValue>(value)) {
    position.SetNamedGridArea(ident_value->Value());
    return position;
  }

  if (auto* identifier_value = DynamicTo<CSSIdentifierValue>(value)) {
    DCHECK_EQ(identifier_value->GetValueID(), CSSValueID::kAuto);
    return position;
  }

  const auto& values = To<CSSValueList>(value);
  DCHECK(values.length());

  bool is_span_position = false;
  // The specification makes the <integer> optional, in which case it default to
  // '1'.
  int grid_line_number = 1;
  AtomicString grid_line_name;

  auto it = values.begin();
  const CSSValue* current_value = it->Get();
  auto* current_identifier_value = DynamicTo<CSSIdentifierValue>(current_value);
  if (current_identifier_value &&
      current_identifier_value->GetValueID() == CSSValueID::kSpan) {
    is_span_position = true;
    UNSAFE_TODO(++it);
    current_value = it != values.end() ? it->Get() : nullptr;
  }

  auto* current_primitive_value = DynamicTo<CSSPrimitiveValue>(current_value);
  if (current_primitive_value && current_primitive_value->IsNumber()) {
    grid_line_number = current_primitive_value->ComputeInteger(
        state.CssToLengthConversionData());
    UNSAFE_TODO(++it);
    current_value = it != values.end() ? it->Get() : nullptr;
  }

  auto* current_ident_value = DynamicTo<CSSCustomIdentValue>(current_value);
  if (current_ident_value) {
    grid_line_name = current_ident_value->Value();
    UNSAFE_TODO(++it);
  }

  DCHECK_EQ(it, values.end());
  if (is_span_position) {
    position.SetSpanPosition(grid_line_number, grid_line_name);
  } else {
    position.SetExplicitPosition(grid_line_number, grid_line_name);
  }

  return position;
}

// static
ComputedGridTemplateAreas* StyleBuilderConverter::ConvertGridTemplateAreas(
    StyleResolverState&,
    const CSSValue& value) {
  if (auto* identifier_value = DynamicTo<CSSIdentifierValue>(value)) {
    DCHECK_EQ(identifier_value->GetValueID(), CSSValueID::kNone);
    return nullptr;
  }

  const auto& grid_template_areas_value =
      To<cssvalue::CSSGridTemplateAreasValue>(value);
  return MakeGarbageCollected<ComputedGridTemplateAreas>(
      grid_template_areas_value.GridAreaMap(),
      grid_template_areas_value.RowCount(),
      grid_template_areas_value.ColumnCount());
}

GridTrackSize StyleBuilderConverter::ConvertGridTrackSize(
    StyleResolverState& state,
    const CSSValue& value) {
  if (value.IsPrimitiveValue() || value.IsIdentifierValue()) {
    return GridTrackSize(ConvertGridTrackBreadth(state, value));
  }

  auto& function = To<CSSFunctionValue>(value);
  if (function.FunctionType() == CSSValueID::kFitContent) {
    SECURITY_DCHECK(function.length() == 1);
    return GridTrackSize(ConvertGridTrackBreadth(state, function.Item(0)),
                         kFitContentTrackSizing);
  }

  SECURITY_DCHECK(function.length() == 2);
  return GridTrackSize(ConvertGridTrackBreadth(state, function.Item(0)),
                       ConvertGridTrackBreadth(state, function.Item(1)));
}

static void ConvertGridLineNamesList(
    const CSSValue& value,
    wtf_size_t current_named_grid_line,
    NamedGridLinesMap& named_grid_lines,
    OrderedNamedGridLines& ordered_named_grid_lines,
    bool is_in_repeat = false,
    bool is_first_repeat = false) {
  DCHECK(value.IsGridLineNamesValue());

  for (auto& named_grid_line_value : To<CSSValueList>(value)) {
    AtomicString named_grid_line =
        To<CSSCustomIdentValue>(*named_grid_line_value).Value();
    NamedGridLinesMap::AddResult result =
        named_grid_lines.insert(named_grid_line, Vector<wtf_size_t>());
    result.stored_value->value.push_back(current_named_grid_line);
    OrderedNamedGridLines::AddResult ordered_insertion_result =
        ordered_named_grid_lines.insert(current_named_grid_line,
                                        Vector<NamedGridLine>());
    ordered_insertion_result.stored_value->value.push_back(
        NamedGridLine(named_grid_line, is_in_repeat, is_first_repeat));
  }
}

GridTrackList StyleBuilderConverter::ConvertGridTrackSizeList(
    StyleResolverState& state,
    const CSSValue& value) {
  const CSSValueList* list = DynamicTo<CSSValueList>(value);
  if (!list) {
    const auto& ident = To<CSSIdentifierValue>(value);
    DCHECK_EQ(ident.GetValueID(), CSSValueID::kAuto);
    return GridTrackList(GridTrackSize(Length::Auto()));
  }

  Vector<GridTrackSize, 1> track_sizes;
  for (auto& curr_value : To<CSSValueList>(value)) {
    DCHECK(!curr_value->IsGridLineNamesValue());
    DCHECK(!curr_value->IsGridRepeatValue());
    track_sizes.push_back(ConvertGridTrackSize(state, *curr_value));
  }

  GridTrackList track_list;
  track_list.AddRepeater(track_sizes);
  return track_list;
}

ComputedGridTrackList* StyleBuilderConverter::ConvertGridTrackList(
    StyleResolverState& state,
    const CSSValue& value) {
  if (auto* identifier_value = DynamicTo<CSSIdentifierValue>(value)) {
    DCHECK_EQ(identifier_value->GetValueID(), CSSValueID::kNone);
    return nullptr;
  }

  ComputedGridTrackList* computed_grid_track_list =
      MakeGarbageCollected<ComputedGridTrackList>();
  GridTrackList& track_list = computed_grid_track_list->GetMutableTrackList();

  wtf_size_t current_named_grid_line = 0;
  auto ConvertLineNameOrTrackSize =
      [&](const CSSValue& curr_value, bool is_in_repeat = false,
          bool is_first_repeat = false) -> wtf_size_t {
    wtf_size_t line_name_indices_count = 0;
    if (curr_value.IsGridLineNamesValue()) {
      ++line_name_indices_count;
      ConvertGridLineNamesList(
          curr_value, current_named_grid_line,
          computed_grid_track_list->GetMutableNamedGridLines(),
          computed_grid_track_list->GetMutableOrderedNamedGridLines(),
          is_in_repeat, is_first_repeat);
      if (computed_grid_track_list->IsSubgriddedAxis()) {
        ++current_named_grid_line;
        track_list.IncrementNonAutoRepeatLineCount();
      }
    } else {
      DCHECK_EQ(computed_grid_track_list->GetGridAxisType(),
                GridAxisType::kStandaloneAxis);
      ++current_named_grid_line;
    }
    return line_name_indices_count;
  };

  const auto& values = To<CSSValueList>(value);
  auto curr_value = values.begin();
  bool is_subgrid = false;

  auto* identifier_value = DynamicTo<CSSIdentifierValue>(curr_value->Get());
  if (identifier_value &&
      identifier_value->GetValueID() == CSSValueID::kSubgrid) {
    state.GetDocument().CountUse(WebFeature::kCSSSubgridLayout);
    computed_grid_track_list->SetGridAxisType(GridAxisType::kSubgriddedAxis);
    track_list.SetAxisType(GridAxisType::kSubgriddedAxis);
    is_subgrid = true;
    UNSAFE_TODO(++curr_value);
  }

  for (; curr_value != values.end(); UNSAFE_TODO(++curr_value)) {
    if (auto* grid_auto_repeat_value =
            DynamicTo<cssvalue::CSSGridAutoRepeatValue>(curr_value->Get())) {
      Vector<GridTrackSize, 1> repeated_track_sizes;
      wtf_size_t auto_repeat_index = 0;
      CSSValueID auto_repeat_id = grid_auto_repeat_value->AutoRepeatID();
      DCHECK(auto_repeat_id == CSSValueID::kAutoFill ||
             auto_repeat_id == CSSValueID::kAutoFit);
      computed_grid_track_list->SetAutoRepeatType(
          (auto_repeat_id == CSSValueID::kAutoFill) ? AutoRepeatType::kAutoFill
                                                    : AutoRepeatType::kAutoFit);
      for (const CSSValue* auto_repeat_value : To<CSSValueList>(**curr_value)) {
        if (auto_repeat_value->IsGridLineNamesValue()) {
          ConvertGridLineNamesList(
              *auto_repeat_value, auto_repeat_index,
              computed_grid_track_list->GetMutableAutoRepeatNamedGridLines(),
              computed_grid_track_list
                  ->GetMutableOrderedAutoRepeatNamedGridLines());
          if (computed_grid_track_list->IsSubgriddedAxis()) {
            ++auto_repeat_index;
          }
          continue;
        }
        ++auto_repeat_index;
        repeated_track_sizes.push_back(
            ConvertGridTrackSize(state, *auto_repeat_value));
      }
      // `repeat_count` is always 1 for auto-repeaters.
      track_list.AddRepeater(repeated_track_sizes,
                             static_cast<GridTrackRepeater::RepeatType>(
                                 computed_grid_track_list->GetAutoRepeatType()),
                             /*repeat_count=*/1u, auto_repeat_index);
      computed_grid_track_list->SetAutoRepeatInsertionPoint(
          current_named_grid_line++);
      continue;
    }

    if (auto* grid_integer_repeat_value =
            DynamicTo<cssvalue::CSSGridIntegerRepeatValue>(curr_value->Get())) {
      const wtf_size_t repetitions =
          grid_integer_repeat_value->ComputeRepetitions(
              state.CssToLengthConversionData());
      wtf_size_t line_name_indices_count = 0;

      for (wtf_size_t i = 0; i < repetitions; ++i) {
        const bool is_first_repeat = i == 0;
        for (auto integer_repeat_value : *grid_integer_repeat_value) {
          const wtf_size_t current_line_name_indices_count =
              ConvertLineNameOrTrackSize(*integer_repeat_value,
                                         /* is_inside_repeat */ true,
                                         is_first_repeat);
          // Only add to `line_name_indices_count` on the first iteration so it
          // doesn't need to be divided by `repetitions`.
          if (is_first_repeat) {
            line_name_indices_count += current_line_name_indices_count;
          }
        }
      }

      // For standalone grids, consume track sizes that will be repeated.
      // Subgrids will instead use the count of line name indices to determine
      // the number of tracks to repeat.
      Vector<GridTrackSize, 1> repeater_track_sizes;
      if (!is_subgrid) {
        for (auto integer_repeat_value : *grid_integer_repeat_value) {
          if (!integer_repeat_value->IsGridLineNamesValue()) {
            repeater_track_sizes.push_back(
                ConvertGridTrackSize(state, *integer_repeat_value));
          }
        }
      }
      // For subgrids, we can use the count of line name indices to represent
      // the number of lines being repeated. For non-subgrids, this value will
      // always be 1, since the track sizes are used to determine the number of
      // lines being repeated.
      const wtf_size_t repeat_number_of_lines =
          is_subgrid ? line_name_indices_count : 1u;
      track_list.AddRepeater(repeater_track_sizes,
                             GridTrackRepeater::RepeatType::kInteger,
                             repetitions, repeat_number_of_lines);
      continue;
    }

    // Handle non-repeaters.
    ConvertLineNameOrTrackSize(**curr_value);
    if (!curr_value->Get()->IsGridLineNamesValue()) {
      track_list.AddRepeater({ConvertGridTrackSize(state, **curr_value)});
    } else if (is_subgrid) {
      track_list.AddRepeater(/*repeater_track_sizes=*/{});
    }
  }

  // Unless the axis is subgridded, the parser should have rejected any
  // <track-list> without any <track-size> as this is not conformant to
  // the syntax.
  DCHECK(track_list.RepeaterCount() ||
         computed_grid_track_list->IsSubgriddedAxis());

  return computed_grid_track_list;
}

ScrollMarkerGroup* StyleBuilderConverter::ConvertScrollMarkerGroup(
    StyleResolverState&,
    const CSSValue& value) {
  if (auto* identifier_value = DynamicTo<CSSIdentifierValue>(value)) {
    if (identifier_value->GetValueID() == CSSValueID::kNone) {
      return nullptr;
    }
    auto position =
        identifier_value->ConvertTo<ScrollMarkerGroup::ScrollMarkerPosition>();
    return MakeGarbageCollected<ScrollMarkerGroup>(position);
  }
  const auto& pair = To<CSSValuePair>(value);
  auto position = To<CSSIdentifierValue>(pair.First())
                      .ConvertTo<ScrollMarkerGroup::ScrollMarkerPosition>();
  auto mode = To<CSSIdentifierValue>(pair.Second())
                  .ConvertTo<ScrollMarkerGroup::ScrollMarkerMode>();
  return MakeGarbageCollected<ScrollMarkerGroup>(position, mode);
}

ItemTolerance StyleBuilderConverter::ConvertItemTolerance(
    const StyleResolverState& state,
    const CSSValue& value) {
  auto* identifier_value = DynamicTo<CSSIdentifierValue>(value);
  if (identifier_value) {
    return ItemTolerance(identifier_value->GetValueID());
  }

  return ItemTolerance(ConvertLength(state, value));
}

StyleHyphenateLimitChars StyleBuilderConverter::ConvertHyphenateLimitChars(
    StyleResolverState& state,
    const CSSValue& value) {
  if (const auto* ident = DynamicTo<CSSIdentifierValue>(value)) {
    DCHECK_EQ(ident->GetValueID(), CSSValueID::kAuto);
    return StyleHyphenateLimitChars();
  }
  const auto& list = To<CSSValueList>(value);
  DCHECK_GE(list.length(), 1u);
  DCHECK_LE(list.length(), 3u);
  Vector<unsigned, 3> values;
  for (const Member<const CSSValue>& item : list) {
    if (const auto* primitive = DynamicTo<CSSPrimitiveValue>(item.Get())) {
      DCHECK(primitive->IsInteger());
      DCHECK_GE(primitive->ComputeInteger(state.CssToLengthConversionData()),
                1);
      values.push_back(
          primitive->ComputeInteger(state.CssToLengthConversionData()));
      continue;
    }
    if (const auto* ident = DynamicTo<CSSIdentifierValue>(item.Get())) {
      DCHECK_EQ(ident->GetValueID(), CSSValueID::kAuto);
      values.push_back(0);
      continue;
    }
    NOTREACHED();
  }
  values.Grow(3);
  return StyleHyphenateLimitChars(values[0], values[1], values[2]);
}

StyleInterestDelay StyleBuilderConverter::ConvertInterestDelayValue(
    const StyleResolverState& state,
    const CSSValue& value) {
  if (const auto* ident = DynamicTo<CSSIdentifierValue>(value)) {
    DCHECK_EQ(ident->GetValueID(), CSSValueID::kNormal);
    return StyleInterestDelay();
  }
  return StyleInterestDelay(
      StyleBuilderConverter::ConvertTimeValue(state, value));
}

int StyleBuilderConverter::ConvertBorderWidth(const StyleResolverState& state,
                                              const CSSValue& value) {
  double result = 0;

  if (auto* identifier_value = DynamicTo<CSSIdentifierValue>(value)) {
    switch (identifier_value->GetValueID()) {
      case CSSValueID::kThin:
        result = 1;
        break;
      case CSSValueID::kMedium:
        result = 3;
        break;
      case CSSValueID::kThick:
        result = 5;
        break;
      default:
        NOTREACHED();
    }

    result = state.CssToLengthConversionData().ZoomedComputedPixels(
        result, CSSPrimitiveValue::UnitType::kPixels);
  } else {
    const auto& primitive_value = To<CSSPrimitiveValue>(value);
    result =
        primitive_value.ComputeLength<float>(state.CssToLengthConversionData());
  }

  if (result > 0.0 && result < 1.0) {
    return 1;
  }

  // Clamp the result to a reasonable range for layout.
  return ClampTo<int>(floor(result), 0, LayoutUnit::Max().ToInt());
}

Superellipse StyleBuilderConverter::ConvertCornerShape(
    const StyleResolverState& state,
    const CSSValue& value) {
  if (const auto* keyword = DynamicTo<CSSIdentifierValue>(value)) {
    switch (keyword->GetValueID()) {
      case CSSValueID::kBevel:
        return Superellipse::Bevel();
      case CSSValueID::kNotch:
        return Superellipse::Notch();
      case CSSValueID::kRound:
        return Superellipse::Round();
      case CSSValueID::kSquircle:
        return Superellipse::Squircle();
      case CSSValueID::kScoop:
        return Superellipse::Scoop();
      case CSSValueID::kSquare:
        return Superellipse::Square();
      default:
        NOTREACHED();
    }
  }

  const auto& superellipse = To<cssvalue::CSSSuperellipseValue>(value);
  return Superellipse(
      superellipse.Param().ComputeNumber(state.CssToLengthConversionData()));
}

LayoutUnit StyleBuilderConverter::ConvertLayoutUnit(
    const StyleResolverState& state,
    const CSSValue& value) {
  return LayoutUnit::Clamp(ConvertComputedLength<float>(state, value));
}

std::optional<Length> StyleBuilderConverter::ConvertGapLength(
    const StyleResolverState& state,
    const CSSValue& value) {
  auto* identifier_value = DynamicTo<CSSIdentifierValue>(value);
  if (identifier_value &&
      identifier_value->GetValueID() == CSSValueID::kNormal) {
    return std::nullopt;
  }

  return ConvertLength(state, value);
}

Length StyleBuilderConverter::ConvertLength(const StyleResolverState& state,
                                            const CSSValue& value) {
  return To<CSSPrimitiveValue>(value).ConvertToLength(
      state.CssToLengthConversionData());
}

UnzoomedLength StyleBuilderConverter::ConvertUnzoomedLength(
    StyleResolverState& state,
    const CSSValue& value) {
  return UnzoomedLength(To<CSSPrimitiveValue>(value).ConvertToLength(
      state.UnzoomedLengthConversionData()));
}

float StyleBuilderConverter::ConvertZoom(const StyleResolverState& state,
                                         const CSSValue& value) {
  SECURITY_DCHECK(value.IsPrimitiveValue() || value.IsIdentifierValue());

  if (const auto* identifier_value = DynamicTo<CSSIdentifierValue>(value)) {
    if (identifier_value->GetValueID() == CSSValueID::kNormal) {
      return ComputedStyleInitialValues::InitialZoom();
    }
  } else if (const auto* primitive_value =
                 DynamicTo<CSSPrimitiveValue>(value)) {
    if (primitive_value->IsPercentage()) {
      float percent =
          primitive_value->ComputePercentage(state.CssToLengthConversionData());
      return percent ? (percent / 100.0f) : 1.0f;
    } else if (primitive_value->IsNumber()) {
      float number =
          primitive_value->ComputeNumber(state.CssToLengthConversionData());
      return number ? number : 1.0f;
    }
  }

  NOTREACHED();
}

Length StyleBuilderConverter::ConvertLengthOrAuto(
    const StyleResolverState& state,
    const CSSValue& value) {
  auto* identifier_value = DynamicTo<CSSIdentifierValue>(value);
  if (identifier_value && identifier_value->GetValueID() == CSSValueID::kAuto) {
    return Length::Auto();
  }
  return To<CSSPrimitiveValue>(value).ConvertToLength(
      state.CssToLengthConversionData());
}

std::optional<Length> StyleBuilderConverter::ConvertLengthOrNone(
    const StyleResolverState& state,
    const CSSValue& value) {
  if (const auto* identifier_value = DynamicTo<CSSIdentifierValue>(value)) {
    DCHECK_EQ(identifier_value->GetValueID(), CSSValueID::kNone);
    return std::nullopt;
  }
  return ConvertLength(state, To<CSSPrimitiveValue>(value));
}

Length StyleBuilderConverter::ConvertLengthSizing(StyleResolverState& state,
                                                  const CSSValue& value) {
  const auto* identifier_value = DynamicTo<CSSIdentifierValue>(value);
  if (!identifier_value) {
    return ConvertLength(state, value);
  }

  switch (identifier_value->GetValueID()) {
    case CSSValueID::kMinContent:
    case CSSValueID::kWebkitMinContent:
      return Length::MinContent();
    case CSSValueID::kMaxContent:
    case CSSValueID::kWebkitMaxContent:
      return Length::MaxContent();
    case CSSValueID::kStretch:
    case CSSValueID::kWebkitFillAvailable:
      return Length::Stretch();
    case CSSValueID::kWebkitFitContent:
    case CSSValueID::kFitContent:
      return Length::FitContent();
    case CSSValueID::kContent:
      return Length::Content();
    case CSSValueID::kAuto:
      return Length::Auto();
    default:
      NOTREACHED();
  }
}

Length StyleBuilderConverter::ConvertLengthMaxSizing(StyleResolverState& state,
                                                     const CSSValue& value) {
  auto* identifier_value = DynamicTo<CSSIdentifierValue>(value);
  if (identifier_value && identifier_value->GetValueID() == CSSValueID::kNone) {
    return Length::None();
  }
  return ConvertLengthSizing(state, value);
}

TabSize StyleBuilderConverter::ConvertLengthOrTabSpaces(
    StyleResolverState& state,
    const CSSValue& value) {
  const auto& primitive_value = To<CSSPrimitiveValue>(value);
  if (primitive_value.IsNumber()) {
    return TabSize(
        primitive_value.ComputeNumber(state.CssToLengthConversionData()),
        TabSizeValueType::kSpace);
  }
  return TabSize(
      primitive_value.ComputeLength<float>(state.CssToLengthConversionData()),
      TabSizeValueType::kLength);
}

static CSSToLengthConversionData AdjustedZoomConversionData(
    StyleResolverState& state) {
  float multiplier = state.StyleBuilder().EffectiveZoom();
  if (LocalFrame* frame = state.GetDocument().GetFrame()) {
    multiplier *= frame->TextZoomFactor();
  }

  if (!state.StyleBuilder().GetTextSizeAdjust().IsAuto()) {
    Settings* settings = state.GetDocument().GetSettings();
    if (settings && settings->GetTextAutosizingEnabled()) {
      multiplier *= state.StyleBuilder().GetTextSizeAdjust().Multiplier();
    }
  }
  return state.CssToLengthConversionData().CopyWithAdjustedZoom(multiplier);
}

Length StyleBuilderConverter::ConvertLineHeight(StyleResolverState& state,
                                                const CSSValue& value) {
  if (const auto* primitive_value = DynamicTo<CSSPrimitiveValue>(value)) {
    if (primitive_value->IsLength()) {
      return primitive_value->ComputeLength<Length>(
          AdjustedZoomConversionData(state));
    }
    if (primitive_value->IsNumber()) {
      return Length::Percent(ClampTo<float>(
          primitive_value->ComputeNumber(AdjustedZoomConversionData(state)) *
          100.0));
    }
    float computed_font_size =
        state.StyleBuilder().GetFontDescription().ComputedSize();
    if (primitive_value->IsPercentage()) {
      return Length::Fixed(
          (computed_font_size * ClampTo<int>(primitive_value->ComputePercentage(
                                    AdjustedZoomConversionData(state)))) /
          100.0);
    }
    if (primitive_value->IsCalculated()) {
      Length zoomed_length =
          Length(To<CSSMathFunctionValue>(primitive_value)
                     ->ToCalcValue(AdjustedZoomConversionData(state)));
      return Length::Fixed(
          ValueForLength(zoomed_length, LayoutUnit(computed_font_size)));
    }
  }

  if (value.IsPendingSystemFontValue()) {
    return ComputedStyleInitialValues::InitialLineHeight();
  }

  DCHECK_EQ(To<CSSIdentifierValue>(value).GetValueID(), CSSValueID::kNormal);
  return ComputedStyleInitialValues::InitialLineHeight();
}

float StyleBuilderConverter::ConvertNumberOrPercentage(
    StyleResolverState& state,
    const CSSValue& value) {
  const auto& primitive_value = To<CSSPrimitiveValue>(value);
  DCHECK(primitive_value.IsNumber() || primitive_value.IsPercentage());
  return primitive_value.ConvertTo<float>(state.CssToLengthConversionData());
}

int StyleBuilderConverter::ConvertInteger(StyleResolverState& state,
                                          const CSSValue& value) {
  return To<CSSPrimitiveValue>(value).ComputeInteger(
      state.CssToLengthConversionData());
}

float StyleBuilderConverter::ConvertAlpha(StyleResolverState& state,
                                          const CSSValue& value) {
  return ClampTo<float>(ConvertNumberOrPercentage(state, value), 0, 1);
}

ScopedCSSName* StyleBuilderConverter::ConvertNoneOrCustomIdent(
    StyleResolverState& state,
    const CSSValue& value) {
  DCHECK(value.IsScopedValue());
  if (const auto* identifier_value = DynamicTo<CSSIdentifierValue>(value)) {
    DCHECK_EQ(identifier_value->GetValueID(), CSSValueID::kNone);
    return nullptr;
  }
  return ConvertCustomIdent(state, value);
}

ScopedCSSName* StyleBuilderConverter::ConvertNormalOrCustomIdent(
    StyleResolverState& state,
    const CSSValue& value) {
  DCHECK(value.IsScopedValue());
  if (const auto* identifier_value = DynamicTo<CSSIdentifierValue>(value)) {
    DCHECK_EQ(identifier_value->GetValueID(), CSSValueID::kNormal);
    return nullptr;
  }
  return ConvertCustomIdent(state, value);
}

ScopedCSSName* StyleBuilderConverter::ConvertCustomIdent(
    StyleResolverState& state,
    const CSSValue& value) {
  state.SetHasTreeScopedReference();
  const CSSCustomIdentValue& custom_ident = To<CSSCustomIdentValue>(value);
  return MakeGarbageCollected<ScopedCSSName>(
      custom_ident.ComputeIdent(state.CssToLengthConversionData()),
      custom_ident.GetTreeScope());
}

StylePositionAnchor StyleBuilderConverter::ConvertPositionAnchor(
    StyleResolverState& state,
    const CSSValue& value) {
  DCHECK(value.IsScopedValue());
  if (const auto* identifier_value = DynamicTo<CSSIdentifierValue>(value)) {
    if (identifier_value->GetValueID() == CSSValueID::kNone) {
      return StylePositionAnchor(StylePositionAnchor::Type::kNone);
    }
    DCHECK_EQ(identifier_value->GetValueID(), CSSValueID::kAuto);
    return StylePositionAnchor(StylePositionAnchor::Type::kAuto);
  }
  return StylePositionAnchor(ConvertCustomIdent(state, value));
}

PositionVisibility StyleBuilderConverter::ConvertPositionVisibility(
    StyleResolverState& state,
    const CSSValue& value) {
  PositionVisibility flags = PositionVisibility::kAlways;

  auto process = [&flags](const CSSValue& identifier) {
    flags |= To<CSSIdentifierValue>(identifier).ConvertTo<PositionVisibility>();
  };
  if (auto* value_list = DynamicTo<CSSValueList>(value)) {
    for (auto& entry : *value_list) {
      process(*entry);
    }
  } else {
    process(value);
  }
  return flags;
}

ScopedCSSNameList* StyleBuilderConverter::ConvertAnchorName(
    StyleResolverState& state,
    const CSSValue& value) {
  DCHECK(value.IsScopedValue());
  if (const auto* identifier_value = DynamicTo<CSSIdentifierValue>(value)) {
    DCHECK_EQ(identifier_value->GetValueID(), CSSValueID::kNone);
    return nullptr;
  }
  DCHECK(value.IsBaseValueList());
  HeapVector<Member<const ScopedCSSName>> names;
  for (const Member<const CSSValue>& item : To<CSSValueList>(value)) {
    names.push_back(ConvertCustomIdent(state, *item));
  }
  return MakeGarbageCollected<ScopedCSSNameList>(std::move(names));
}

StyleNameScope StyleBuilderConverter::ConvertNameScope(
    StyleResolverState& state,
    const CSSValue& value) {
  CHECK(value.IsScopedValue());
  if (const auto* scoped_keyword_value =
          DynamicTo<cssvalue::CSSScopedKeywordValue>(value)) {
    CHECK_EQ(scoped_keyword_value->GetValueID(), CSSValueID::kAll);
    state.SetHasTreeScopedReference();
    return StyleNameScope(StyleNameScope::Type::kAll,
                          scoped_keyword_value->GetTreeScope(),
                          /* names */ nullptr);
  }
  if (const auto* identifier_value = DynamicTo<CSSIdentifierValue>(value)) {
    CHECK_EQ(identifier_value->GetValueID(), CSSValueID::kNone);
    return StyleNameScope();
  }
  CHECK(value.IsBaseValueList());
  HeapVector<Member<const ScopedCSSName>> names;
  for (const Member<const CSSValue>& item : To<CSSValueList>(value)) {
    names.push_back(ConvertCustomIdent(state, *item));
  }
  return StyleNameScope(
      StyleNameScope::Type::kNames, /* all_tree_scope */ nullptr,
      /* names */ MakeGarbageCollected<ScopedCSSNameList>(std::move(names)));
}

StyleAnchorScope StyleBuilderConverter::ConvertAnchorScope(
    StyleResolverState& state,
    const CSSValue& value) {
  return ConvertNameScope(state, value);
}

StyleInitialLetter StyleBuilderConverter::ConvertInitialLetter(
    StyleResolverState& state,
    const CSSValue& value) {
  if (auto* normal_value = DynamicTo<CSSIdentifierValue>(value)) {
    DCHECK_EQ(normal_value->GetValueID(), CSSValueID::kNormal);
    return StyleInitialLetter::Normal();
  }

  const auto& list = To<CSSValueList>(value);
  DCHECK(list.length() == 1 || list.length() == 2);
  const float size = To<CSSPrimitiveValue>(list.Item(0))
                         .ComputeNumber(state.CssToLengthConversionData());
  DCHECK_GE(size, 1);
  if (list.length() == 1) {
    return StyleInitialLetter(size);
  }

  const CSSValue& second = list.Item(1);
  if (auto* sink_type = DynamicTo<CSSIdentifierValue>(second)) {
    if (sink_type->GetValueID() == CSSValueID::kDrop) {
      return StyleInitialLetter::Drop(size);
    }
    if (sink_type->GetValueID() == CSSValueID::kRaise) {
      return StyleInitialLetter::Raise(size);
    }
    NOTREACHED() << "Unexpected sink type " << sink_type;
  }

  if (auto* sink = DynamicTo<CSSPrimitiveValue>(second)) {
    DCHECK_GE(sink->ComputeNumber(state.CssToLengthConversionData()), 1);
    return StyleInitialLetter(
        size, sink->ComputeNumber(state.CssToLengthConversionData()));
  }

  return StyleInitialLetter::Normal();
}

StyleOffsetRotation StyleBuilderConverter::ConvertOffsetRotate(
    StyleResolverState& state,
    const CSSValue& value) {
  return ConvertOffsetRotate(state.CssToLengthConversionData(), value);
}

StyleOffsetRotation StyleBuilderConverter::ConvertOffsetRotate(
    const CSSLengthResolver& length_resolver,
    const CSSValue& value) {
  StyleOffsetRotation result(0, OffsetRotationType::kFixed);

  if (auto* identifier = DynamicTo<CSSIdentifierValue>(value)) {
    DCHECK_EQ(identifier->GetValueID(), CSSValueID::kAuto);
    result.type = OffsetRotationType::kAuto;
    return result;
  }

  const auto& list = To<CSSValueList>(value);
  DCHECK(list.length() == 1 || list.length() == 2);
  for (const auto& item : list) {
    auto* identifier_value = DynamicTo<CSSIdentifierValue>(item.Get());
    if (identifier_value &&
        identifier_value->GetValueID() == CSSValueID::kAuto) {
      result.type = OffsetRotationType::kAuto;
    } else if (identifier_value &&
               identifier_value->GetValueID() == CSSValueID::kReverse) {
      result.type = OffsetRotationType::kAuto;
      result.angle = ClampTo<float>(result.angle + 180);
    } else {
      const auto& primitive_value = To<CSSPrimitiveValue>(*item);
      result.angle = ClampTo<float>(
          result.angle + primitive_value.ComputeDegrees(length_resolver));
    }
  }

  return result;
}

LengthPoint StyleBuilderConverter::ConvertPosition(
    const StyleResolverState& state,
    const CSSValue& value) {
  const auto& pair = To<CSSValuePair>(value);
  return LengthPoint(
      ConvertPositionLength<CSSValueID::kLeft, CSSValueID::kRight>(
          state, pair.First()),
      ConvertPositionLength<CSSValueID::kTop, CSSValueID::kBottom>(
          state, pair.Second()));
}

LengthPoint StyleBuilderConverter::ConvertPositionOrAuto(
    StyleResolverState& state,
    const CSSValue& value) {
  if (value.IsValuePair()) {
    return ConvertPosition(state, value);
  }
  DCHECK(To<CSSIdentifierValue>(value).GetValueID() == CSSValueID::kAuto);
  return LengthPoint(Length::Auto(), Length::Auto());
}

LengthPoint StyleBuilderConverter::ConvertOffsetPosition(
    StyleResolverState& state,
    const CSSValue& value) {
  if (value.IsValuePair()) {
    return ConvertPosition(state, value);
  }
  if (To<CSSIdentifierValue>(value).GetValueID() == CSSValueID::kAuto) {
    return LengthPoint(Length::Auto(), Length::Auto());
  }
  return LengthPoint(Length::None(), Length::None());
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
  auto* identifier_value = DynamicTo<CSSIdentifierValue>(value);
  if (identifier_value && identifier_value->GetValueID() == CSSValueID::kNone) {
    return ComputedStyleInitialValues::InitialPerspective();
  }
  return ConvertPerspectiveLength(state, To<CSSPrimitiveValue>(value));
}

EPaintOrder StyleBuilderConverter::ConvertPaintOrder(
    StyleResolverState&,
    const CSSValue& css_paint_order) {
  if (const auto* order_type_list = DynamicTo<CSSValueList>(css_paint_order)) {
    switch (To<CSSIdentifierValue>(order_type_list->Item(0)).GetValueID()) {
      case CSSValueID::kFill:
        return order_type_list->length() > 1 ? kPaintOrderFillMarkersStroke
                                             : kPaintOrderFillStrokeMarkers;
      case CSSValueID::kStroke:
        return order_type_list->length() > 1 ? kPaintOrderStrokeMarkersFill
                                             : kPaintOrderStrokeFillMarkers;
      case CSSValueID::kMarkers:
        return order_type_list->length() > 1 ? kPaintOrderMarkersStrokeFill
                                             : kPaintOrderMarkersFillStroke;
      default:
        NOTREACHED();
    }
  }

  return kPaintOrderNormal;
}

Length StyleBuilderConverter::ConvertQuirkyLength(StyleResolverState& state,
                                                  const CSSValue& value) {
  Length length = ConvertLengthOrAuto(state, value);
  // This is only for margins which use __qem
  auto* numeric_literal = DynamicTo<CSSNumericLiteralValue>(value);
  length.SetQuirk(numeric_literal && numeric_literal->IsQuirkyEms());
  return length;
}

scoped_refptr<QuotesData> StyleBuilderConverter::ConvertQuotes(
    StyleResolverState&,
    const CSSValue& value) {
  if (const auto* list = DynamicTo<CSSValueList>(value)) {
    scoped_refptr<QuotesData> quotes = QuotesData::Create();
    for (wtf_size_t i = 0; i < list->length(); i += 2) {
      String start_quote = To<CSSStringValue>(list->Item(i)).Value();
      String end_quote = To<CSSStringValue>(list->Item(i + 1)).Value();
      quotes->AddPair(std::make_pair(start_quote, end_quote));
    }
    return quotes;
  }
  if (To<CSSIdentifierValue>(value).GetValueID() == CSSValueID::kNone) {
    return QuotesData::Create();
  }
  DCHECK_EQ(To<CSSIdentifierValue>(value).GetValueID(), CSSValueID::kAuto);
  return nullptr;
}

LengthSize StyleBuilderConverter::ConvertRadius(const StyleResolverState& state,
                                                const CSSValue& value) {
  const auto& pair = To<CSSValuePair>(value);
  Length radius_width = To<CSSPrimitiveValue>(pair.First())
                            .ConvertToLength(state.CssToLengthConversionData());
  Length radius_height =
      To<CSSPrimitiveValue>(pair.Second())
          .ConvertToLength(state.CssToLengthConversionData());
  return LengthSize(radius_width, radius_height);
}

template <typename T>
T ConvertGapDecorationPropertyValue(const StyleResolverState& state,
                                    const CSSValue& value,
                                    bool for_visited_link = false);

template <>
StyleColor ConvertGapDecorationPropertyValue<StyleColor>(
    const StyleResolverState& state,
    const CSSValue& value,
    bool for_visited_link) {
  return StyleBuilderConverter::ConvertStyleColor(state, value,
                                                  for_visited_link);
}

template <>
int ConvertGapDecorationPropertyValue<int>(const StyleResolverState& state,
                                           const CSSValue& value,
                                           bool for_visited_link) {
  return ClampTo<uint16_t>(
      StyleBuilderConverter::ConvertBorderWidth(state, value));
}

template <>
EBorderStyle ConvertGapDecorationPropertyValue<EBorderStyle>(
    const StyleResolverState& state,
    const CSSValue& value,
    bool for_visited_link) {
  return To<CSSIdentifierValue>(value).ConvertTo<blink::EBorderStyle>();
}

template <typename T>
GapDataList<T> ConvertGapDecorationDataList(const StyleResolverState& state,
                                            const CSSValue& value,
                                            bool for_visited_link = false) {
  // The `value` will not be a list in two scenarios:
  // 1. When using the legacy 'column-rule-*' properties.
  // 2. When the fast parse path is taken (see
  // CSSParserFastPaths::MaybeParseValue). In these cases, construct a
  // GapDataList with a single Value.
  if (!DynamicTo<CSSValueList>(value)) {
    return GapDataList<T>(
        ConvertGapDecorationPropertyValue<T>(state, value, for_visited_link));
  }
  CHECK(RuntimeEnabledFeatures::CSSGapDecorationEnabled());

  // The CSS Gap Decorations API accepts a space separated list of values.
  // These values can be an auto repeater, an integer repeater, or a single
  // value.
  // See: https://drafts.csswg.org/css-gaps-1/#lists-repeat
  const auto& values = To<CSSValueList>(value);

  // TODO(crbug.com/411367099): This is a temporary condition until :visited
  // restrictions are lifted through the visited partitioning work. Take this
  // out when that happens.
  if constexpr (std::is_same_v<blink::StyleColor, T>) {
    bool is_single_value =
        values.length() == 1 && !values.First().IsRepeatValue();
    // When processing `ColumnRuleColor`, we need to ensure visited styling is
    // only supported for single simple values. If we have multiple colors and
    // we're inside a link, we fallback to the default color.
    if (!is_single_value && state.InsideLink() != EInsideLink::kNotInsideLink) {
      return GapDataList<blink::StyleColor>::DefaultGapColorDataList();
    }
  }

  typename GapDataList<T>::GapDataVector gap_data_list;
  gap_data_list.ReserveInitialCapacity(values.length());

  for (const auto& curr_value : values) {
    GapData<T> gap_data;
    if (auto* gap_repeat_value =
            DynamicTo<cssvalue::CSSRepeatValue>(curr_value.Get())) {
      typename ValueRepeater<T>::VectorType gap_values;
      gap_values.ReserveInitialCapacity(gap_repeat_value->Values().length());
      for (const auto& repeat_value : gap_repeat_value->Values()) {
        gap_values.push_back(ConvertGapDecorationPropertyValue<T>(
            state, *repeat_value, for_visited_link));
      }

      std::optional<int> repeat_count = std::nullopt;
      if (!gap_repeat_value->IsAutoRepeatValue()) {
        repeat_count = gap_repeat_value->Repetitions()->ComputeInteger(
            state.CssToLengthConversionData());
      }
      ValueRepeater<T>* value_repeater = MakeGarbageCollected<ValueRepeater<T>>(
          std::move(gap_values), repeat_count);
      gap_data = GapData<T>(value_repeater);
    } else {
      gap_data = GapData<T>(ConvertGapDecorationPropertyValue<T>(
          state, *curr_value.Get(), for_visited_link));
    }

    gap_data_list.push_back(gap_data);
  }

  return GapDataList<T>(std::move(gap_data_list));
}

GapDataList<StyleColor>
StyleBuilderConverter::ConvertGapDecorationColorDataList(
    const StyleResolverState& state,
    const CSSValue& value,
    bool for_visited_link) {
  return ConvertGapDecorationDataList<blink::StyleColor>(state, value,
                                                         for_visited_link);
}

GapDataList<int> StyleBuilderConverter::ConvertGapDecorationWidthDataList(
    const StyleResolverState& state,
    const CSSValue& value) {
  return ConvertGapDecorationDataList<int>(state, value);
}

GapDataList<EBorderStyle>
StyleBuilderConverter::ConvertGapDecorationStyleDataList(
    const StyleResolverState& state,
    const CSSValue& value) {
  return ConvertGapDecorationDataList<EBorderStyle>(state, value);
}

ShadowData StyleBuilderConverter::ConvertShadow(
    const CSSToLengthConversionData& conversion_data,
    StyleResolverState* state,
    const CSSValue& value) {
  const auto& shadow = To<CSSShadowValue>(value);
  const gfx::Vector2dF offset(shadow.x->ComputeLength<float>(conversion_data),
                              shadow.y->ComputeLength<float>(conversion_data));
  float blur =
      shadow.blur ? shadow.blur->ComputeLength<float>(conversion_data) : 0;
  float spread =
      shadow.spread ? shadow.spread->ComputeLength<float>(conversion_data) : 0;
  ShadowStyle shadow_style =
      shadow.style && shadow.style->GetValueID() == CSSValueID::kInset
          ? ShadowStyle::kInset
          : ShadowStyle::kNormal;
  StyleColor color = StyleColor::CurrentColor();
  if (shadow.color) {
    if (state) {
      color = ConvertStyleColor(*state, *shadow.color);
    } else {
      // For OffScreen canvas, we default to black and only parse non
      // Document dependent CSS colors.
      TextLinkColors black_text_link_colors;
      black_text_link_colors.SetTextColor(Color::kBlack);
      black_text_link_colors.SetLinkColor(Color::kBlack);
      black_text_link_colors.SetVisitedLinkColor(Color::kBlack);
      black_text_link_colors.SetActiveLinkColor(Color::kBlack);

      const ResolveColorValueContext context{
          .conversion_data = conversion_data,
          .text_link_colors = black_text_link_colors};
      color = ResolveColorValue(*shadow.color, context);
      if (!color.IsAbsoluteColor()) {
        color = StyleColor(Color::kBlack);
      }
    }
  }
  return ShadowData(offset, blur, spread, shadow_style, color);
}

ShadowList* StyleBuilderConverter::ConvertShadowList(StyleResolverState& state,
                                                     const CSSValue& value) {
  if (auto* identifier_value = DynamicTo<CSSIdentifierValue>(value)) {
    DCHECK_EQ(identifier_value->GetValueID(), CSSValueID::kNone);
    return nullptr;
  }

  const auto& list = To<CSSValueList>(value);
  ShadowDataVector shadows;
  shadows.ReserveInitialCapacity(list.length());
  for (const auto& item : list) {
    shadows.push_back(
        ConvertShadow(state.CssToLengthConversionData(), &state, *item));
  }

  return MakeGarbageCollected<ShadowList>(std::move(shadows));
}

ShapeValue* StyleBuilderConverter::ConvertShapeValue(StyleResolverState& state,
                                                     const CSSValue& value) {
  if (auto* identifier_value = DynamicTo<CSSIdentifierValue>(value)) {
    DCHECK_EQ(identifier_value->GetValueID(), CSSValueID::kNone);
    return nullptr;
  }

  if (value.IsImageValue() || value.IsImageGeneratorValue() ||
      value.IsImageSetValue()) {
    return MakeGarbageCollected<ShapeValue>(
        state.GetStyleImage(CSSPropertyID::kShapeOutside, value));
  }

  const BasicShape* shape = nullptr;
  CSSBoxType css_box = CSSBoxType::kMissing;
  const auto& value_list = To<CSSValueList>(value);
  for (unsigned i = 0; i < value_list.length(); ++i) {
    const CSSValue& item_value = value_list.Item(i);
    if (item_value.IsBasicShapeValue()) {
      shape = BasicShapeForValue(state, item_value);
    } else {
      css_box = To<CSSIdentifierValue>(item_value).ConvertTo<CSSBoxType>();
    }
  }

  if (shape) {
    return MakeGarbageCollected<ShapeValue>(shape, css_box);
  }

  DCHECK_NE(css_box, CSSBoxType::kMissing);
  return MakeGarbageCollected<ShapeValue>(css_box);
}

Length StyleBuilderConverter::ConvertSpacing(StyleResolverState& state,
                                             const CSSValue& value) {
  auto* identifier_value = DynamicTo<CSSIdentifierValue>(value);
  if (identifier_value &&
      identifier_value->GetValueID() == CSSValueID::kNormal) {
    return Length::Fixed();
  }
  return ConvertLength(state, value);
}

SVGDashArray* StyleBuilderConverter::ConvertStrokeDasharray(
    StyleResolverState& state,
    const CSSValue& value) {
  const auto* dashes = DynamicTo<CSSValueList>(value);
  if (!dashes) {
    return nullptr;
  }
  DCHECK(dashes->length());

  SVGDashArray* array = MakeGarbageCollected<SVGDashArray>();
  array->ReserveInitialCapacity(dashes->length());
  for (wtf_size_t i = 0; i < dashes->length(); ++i) {
    array->push_back(
        ConvertLength(state, To<CSSPrimitiveValue>(dashes->Item(i))));
  }

  return array;
}

StyleViewTransitionGroup StyleBuilderConverter::ConvertViewTransitionGroup(
    StyleResolverState& state,
    const CSSValue& value) {
  if (auto* ident = DynamicTo<CSSIdentifierValue>(value)) {
    switch (ident->GetValueID()) {
      case CSSValueID::kNearest:
        return StyleViewTransitionGroup::Nearest();
      case CSSValueID::kNormal:
        return StyleViewTransitionGroup::Normal();
      case CSSValueID::kContain:
        return StyleViewTransitionGroup::Contain();
      default:
        NOTREACHED();
    }
  }
  return StyleViewTransitionGroup::Create(
      ConvertCustomIdent(state, value)->GetName());
}

StyleViewTransitionName* StyleBuilderConverter::ConvertViewTransitionName(
    StyleResolverState& state,
    const CSSValue& value) {
  state.SetHasTreeScopedReference();
  if (auto* ident = DynamicTo<CSSIdentifierValue>(value)) {
    switch (ident->GetValueID()) {
      case CSSValueID::kNone:
        return nullptr;
      case CSSValueID::kAuto:
        // TODO: tree scope for auto
        return StyleViewTransitionName::Auto(&state.GetDocument());
      case CSSValueID::kMatchElement:
        // TODO: tree scope for match-element
        return StyleViewTransitionName::MatchElement(&state.GetDocument());
      default:
        NOTREACHED();
    }
  }
  ScopedCSSName* name = ConvertCustomIdent(state, value);
  return StyleViewTransitionName::Create(name->GetName(), name->GetTreeScope());
}

ScopedCSSNameList* StyleBuilderConverter::ConvertViewTransitionClass(
    StyleResolverState& state,
    const CSSValue& value) {
  DCHECK(value.IsScopedValue());
  if (IsA<CSSIdentifierValue>(value)) {
    DCHECK_EQ(To<CSSIdentifierValue>(value).GetValueID(), CSSValueID::kNone);
    return nullptr;
  }
  DCHECK(value.IsBaseValueList());
  HeapVector<Member<const ScopedCSSName>> names;
  for (const Member<const CSSValue>& item : To<CSSValueList>(value)) {
    names.push_back(ConvertNoneOrCustomIdent(state, *item));
  }
  return MakeGarbageCollected<ScopedCSSNameList>(std::move(names));
}

ScopedCSSNameList* StyleBuilderConverter::ConvertOverscrollArea(
    StyleResolverState& state,
    const CSSValue& value) {
  DCHECK(value.IsScopedValue());
  if (const auto* identifier_value = DynamicTo<CSSIdentifierValue>(value)) {
    DCHECK_EQ(identifier_value->GetValueID(), CSSValueID::kNone);
    return nullptr;
  }
  DCHECK(value.IsBaseValueList());
  HeapVector<Member<const ScopedCSSName>> names;
  for (const Member<const CSSValue>& item : To<CSSValueList>(value)) {
    names.push_back(ConvertCustomIdent(state, *item));
  }
  return MakeGarbageCollected<ScopedCSSNameList>(std::move(names));
}

ScopedCSSName* StyleBuilderConverter::ConvertOverscrollPosition(
    StyleResolverState& state,
    const CSSValue& value) {
  DCHECK(value.IsScopedValue());
  if (const auto* identifier_value = DynamicTo<CSSIdentifierValue>(value)) {
    DCHECK_EQ(identifier_value->GetValueID(), CSSValueID::kNone);
    return nullptr;
  }
  return ConvertCustomIdent(state, value);
}

namespace {

const CSSValue& ResolveLightDarkPair(const CSSLightDarkValuePair& value,
                                     const ResolveColorValueContext& context);

StyleColor ResolveColorValueImpl(const CSSValue& value,
                                 const ResolveColorValueContext& context) {
  if (auto* color_value = DynamicTo<cssvalue::CSSColor>(value)) {
    Color result_color = color_value->Value();
    result_color.ResolveNonFiniteValues();
    return StyleColor(result_color);
  }

  if (auto* identifier_value = DynamicTo<CSSIdentifierValue>(value)) {
    CSSValueID value_id = identifier_value->GetValueID();
    if (value_id == CSSValueID::kCurrentcolor) {
      return StyleColor::CurrentColor();
    }
    if (IsQuirkOrLinkOrFocusRingColor(value_id)) {
      return StyleColor(ResolveQuirkOrLinkOrFocusRingColor(
          value_id, context.text_link_colors, context.used_color_scheme,
          context.for_visited_link));
    }
    Color color = StyleColor::ColorFromKeyword(
        value_id, context.used_color_scheme, context.color_provider,
        context.is_in_web_app_scope);
    // Preserve the identifier for system colors since this is needed by
    // 'forced colors mode'.
    if (StyleColor::IsSystemColorIncludingDeprecated(value_id)) {
      return StyleColor(color, value_id);
    }
    return StyleColor(color);
  }

  if (auto* color_mix_value = DynamicTo<cssvalue::CSSColorMixValue>(value)) {
    const StyleColor style_color1 =
        ResolveColorValueImpl(color_mix_value->Color1(), context);
    const StyleColor style_color2 =
        ResolveColorValueImpl(color_mix_value->Color2(), context);
    double alpha_multiplier = 0.0;
    double mix_amount = 0.0;
    // TODO(crbug.com/40238188): Not sure what is appropriate to return when
    // both mix amounts are zero.
    color_mix_value->NormalizePercentages(mix_amount, alpha_multiplier,
                                          context.conversion_data);
    const StyleColor::UnresolvedColorMix* unresolved_color_mix =
        MakeGarbageCollected<StyleColor::UnresolvedColorMix>(
            color_mix_value->ColorInterpolationSpace(),
            color_mix_value->HueInterpolationMethod(), style_color1,
            style_color2, mix_amount, alpha_multiplier);
    // https://drafts.csswg.org/css-color-5/#resolving-mix
    // If both parameters are resolvable at computed-value time, the color-mix
    // function should be resolved at computed-value time as well.
    // Otherwise we need to store an unresolved value on StyleColor.
    if (style_color1.IsAbsoluteColor() && style_color2.IsAbsoluteColor()) {
      return StyleColor(unresolved_color_mix->Resolve(Color()));
    } else {
      return StyleColor(unresolved_color_mix);
    }
  }

  if (auto* relative_color_value =
          DynamicTo<cssvalue::CSSRelativeColorValue>(value)) {
    const StyleColor origin_color =
        ResolveColorValueImpl(relative_color_value->OriginColor(), context);
    const StyleColor::UnresolvedRelativeColor* unresolved_relative_color =
        MakeGarbageCollected<StyleColor::UnresolvedRelativeColor>(
            origin_color, relative_color_value->ColorInterpolationSpace(),
            relative_color_value->Channel0(), relative_color_value->Channel1(),
            relative_color_value->Channel2(), relative_color_value->Alpha(),
            context.conversion_data);
    // https://drafts.csswg.org/css-color-5/#resolving-rcs
    // If the origin color is resolvable at computed-value time, the relative
    // color function should be resolved at computed-value time as well.
    // Otherwise we need to store an unresolved value on StyleColor.
    if (origin_color.IsAbsoluteColor()) {
      return StyleColor(unresolved_relative_color->Resolve(Color()));
    } else {
      return StyleColor(unresolved_relative_color);
    }
  }

  if (auto* unresolved_color_value =
          DynamicTo<cssvalue::CSSUnresolvedColorValue>(value)) {
    return StyleColor(unresolved_color_value->Resolve(context.conversion_data));
  }

  auto& light_dark_pair = To<CSSLightDarkValuePair>(value);
  const CSSValue& color_value = ResolveLightDarkPair(light_dark_pair, context);
  return ResolveColorValueImpl(color_value, context);
}

const CSSValue& ResolveLightDarkPair(
    const CSSLightDarkValuePair& light_dark_pair,
    const ResolveColorValueContext& context) {
  return context.used_color_scheme == mojom::blink::ColorScheme::kLight
             ? light_dark_pair.First()
             : light_dark_pair.Second();
}

bool ShouldConvertLegacyColorSpaceToSRGB(const CSSValue& value) {
  return value.IsRelativeColorValue() || value.IsColorMixValue();
}

}  // anonymous namespace

StyleColor ResolveColorValue(const CSSValue& value,
                             const ResolveColorValueContext& context) {
  // The rules for converting at the top level should apply transitively through
  // light-dark().
  if (auto* light_dark_pair = DynamicTo<CSSLightDarkValuePair>(value)) {
    const CSSValue& light_dark_result =
        ResolveLightDarkPair(*light_dark_pair, context);
    return ResolveColorValue(light_dark_result, context);
  }

  StyleColor result = ResolveColorValueImpl(value, context);
  if (ShouldConvertLegacyColorSpaceToSRGB(value) && result.IsAbsoluteColor()) {
    Color color = result.GetColor();
    if (Color::IsLegacyColorSpace(color.GetColorSpace())) {
      // Missing components can be carried forward when converting rgb(...) to
      // color(srgb, ...) since the two color spaces have analogous components.
      // For other legacy spaces, conversion to color(srgb, ...) requires
      // resolving missing components.
      const bool resolve_missing_components =
          (color.GetColorSpace() != Color::ColorSpace::kSRGBLegacy);
      color.ConvertToColorSpace(Color::ColorSpace::kSRGB,
                                resolve_missing_components);
      result = StyleColor(color);
    }
  }
  return result;
}

StyleColor StyleBuilderConverter::ConvertStyleColor(
    const StyleResolverState& state,
    const CSSValue& value,
    bool for_visited_link) {
  mojom::blink::ColorScheme color_scheme =
      state.StyleBuilder().UsedColorScheme();
  auto& document = state.GetDocument();
  const ResolveColorValueContext context{
      .conversion_data = state.CssToLengthConversionData(),
      .text_link_colors = document.GetTextLinkColors(),
      .used_color_scheme = color_scheme,
      .color_provider = document.GetColorProviderForPainting(color_scheme),
      .is_in_web_app_scope = document.IsInWebAppScope(),
      .for_visited_link = for_visited_link};
  return ResolveColorValue(value, context);
}

StyleAutoColor StyleBuilderConverter::ConvertStyleAutoColor(
    StyleResolverState& state,
    const CSSValue& value,
    bool for_visited_link) {
  if (auto* identifier_value = DynamicTo<CSSIdentifierValue>(value)) {
    if (identifier_value->GetValueID() == CSSValueID::kAuto) {
      return StyleAutoColor::AutoColor();
    }
  }
  return StyleAutoColor(ConvertStyleColor(state, value, for_visited_link));
}

SVGPaint StyleBuilderConverter::ConvertSVGPaint(StyleResolverState& state,
                                                const CSSValue& value,
                                                bool for_visited_link,
                                                CSSPropertyID property_id) {
  const CSSValue* local_value = &value;
  SVGPaint paint;
  if (const auto* list = DynamicTo<CSSValueList>(value)) {
    DCHECK_EQ(list->length(), 2u);
    paint.resource = ConvertElementReference(state, list->Item(0), property_id);
    local_value = &list->Item(1);
  }

  if (local_value->IsURIValue()) {
    paint.type = SVGPaintType::kUri;
    paint.resource = ConvertElementReference(state, *local_value, property_id);
  } else {
    auto* local_identifier_value = DynamicTo<CSSIdentifierValue>(local_value);
    if (local_identifier_value) {
      switch (local_identifier_value->GetValueID()) {
        case CSSValueID::kNone:
          paint.type =
              !paint.resource ? SVGPaintType::kNone : SVGPaintType::kUriNone;
          break;
        case CSSValueID::kContextFill:
          // context-fill cannot be use as a uri fallback
          DCHECK(!paint.resource);
          paint.type = SVGPaintType::kContextFill;
          state.GetDocument().CountUse(WebFeature::kSvgContextFillOrStroke);
          break;
        case CSSValueID::kContextStroke:
          // context-stroke cannot be use as a uri fallback
          DCHECK(!paint.resource);
          paint.type = SVGPaintType::kContextStroke;
          state.GetDocument().CountUse(WebFeature::kSvgContextFillOrStroke);
          break;
        default:
          // For all other keywords, try to parse as a color.
          local_identifier_value = nullptr;
          break;
      }
    }
    if (!local_identifier_value) {
      // TODO(fs): Pass along |for_visited_link|.
      paint.color = ConvertStyleColor(state, *local_value);
      paint.type =
          !paint.resource ? SVGPaintType::kColor : SVGPaintType::kUriColor;
    }
  }
  return paint;
}

// static
TextBoxEdge StyleBuilderConverter::ConvertTextBoxEdge(
    StyleResolverState& status,
    const CSSValue& value) {
  if (auto* identifier_value = DynamicTo<CSSIdentifierValue>(value)) {
    return TextBoxEdge(identifier_value->ConvertTo<TextBoxEdge::Type>());
  }

  const auto* const list = DynamicTo<CSSValueList>(&value);
  DCHECK_EQ(list->length(), 2u);
  const CSSIdentifierValue& over = To<CSSIdentifierValue>(list->Item(0));
  const CSSIdentifierValue& under = To<CSSIdentifierValue>(list->Item(1));
  return TextBoxEdge(over.ConvertTo<TextBoxEdge::Type>(),
                     under.ConvertTo<TextBoxEdge::Type>());
}

TextDecorationThickness StyleBuilderConverter::ConvertTextDecorationThickness(
    StyleResolverState& state,
    const CSSValue& value) {
  auto* identifier_value = DynamicTo<CSSIdentifierValue>(value);
  if (identifier_value &&
      identifier_value->GetValueID() == CSSValueID::kFromFont) {
    return TextDecorationThickness(identifier_value->GetValueID());
  }

  return TextDecorationThickness(ConvertLengthOrAuto(state, value));
}

TextEmphasisPosition StyleBuilderConverter::ConvertTextTextEmphasisPosition(
    StyleResolverState& state,
    const CSSValue& value) {
  const auto& list = To<CSSValueList>(value);
  CSSValueID first = To<CSSIdentifierValue>(list.Item(0)).GetValueID();
  if (list.length() < 2) {
    if (RuntimeEnabledFeatures::TextEmphasisPositionAutoEnabled() &&
        first == CSSValueID::kAuto) {
      return TextEmphasisPosition::kAuto;
    }
    if (first == CSSValueID::kOver) {
      return TextEmphasisPosition::kOverRight;
    }
    if (first == CSSValueID::kUnder) {
      return TextEmphasisPosition::kUnderRight;
    }
    return TextEmphasisPosition::kOverRight;
  }
  CSSValueID second = To<CSSIdentifierValue>(list.Item(1)).GetValueID();
  if (first == CSSValueID::kOver && second == CSSValueID::kRight) {
    return TextEmphasisPosition::kOverRight;
  }
  if (first == CSSValueID::kOver && second == CSSValueID::kLeft) {
    return TextEmphasisPosition::kOverLeft;
  }
  if (first == CSSValueID::kUnder && second == CSSValueID::kRight) {
    return TextEmphasisPosition::kUnderRight;
  }
  if (first == CSSValueID::kUnder && second == CSSValueID::kLeft) {
    return TextEmphasisPosition::kUnderLeft;
  }
  return TextEmphasisPosition::kOverRight;
}

float StyleBuilderConverter::ConvertTextStrokeWidth(StyleResolverState& state,
                                                    const CSSValue& value) {
  auto* identifier_value = DynamicTo<CSSIdentifierValue>(value);
  if (identifier_value && IsValidCSSValueID(identifier_value->GetValueID())) {
    float multiplier = ConvertLineWidth<float>(state, value);
    return CSSNumericLiteralValue::Create(multiplier / 48,
                                          CSSPrimitiveValue::UnitType::kEms)
        ->ComputeLength<float>(state.CssToLengthConversionData());
  }
  return To<CSSPrimitiveValue>(value).ComputeLength<float>(
      state.CssToLengthConversionData());
}

TextSizeAdjust StyleBuilderConverter::ConvertTextSizeAdjust(
    StyleResolverState& state,
    const CSSValue& value) {
  if (auto* identifier_value = DynamicTo<CSSIdentifierValue>(value)) {
    if (identifier_value->GetValueID() == CSSValueID::kNone) {
      return TextSizeAdjust::AdjustNone();
    }
    if (identifier_value->GetValueID() == CSSValueID::kAuto) {
      return TextSizeAdjust::AdjustAuto();
    }
  }
  const CSSPrimitiveValue& primitive_value = To<CSSPrimitiveValue>(value);
  DCHECK(primitive_value.IsPercentage());
  return TextSizeAdjust(primitive_value.ComputePercentage<float>(
                            state.CssToLengthConversionData()) /
                        100.0f);
}

TextUnderlinePosition StyleBuilderConverter::ConvertTextUnderlinePosition(
    StyleResolverState& state,
    const CSSValue& value) {
  TextUnderlinePosition flags = TextUnderlinePosition::kAuto;

  auto process = [&flags](const CSSValue& identifier) {
    flags |=
        To<CSSIdentifierValue>(identifier).ConvertTo<TextUnderlinePosition>();
  };

  if (auto* value_list = DynamicTo<CSSValueList>(value)) {
    for (auto& entry : *value_list) {
      process(*entry);
    }
  } else {
    process(value);
  }
  return flags;
}

Length StyleBuilderConverter::ConvertTextUnderlineOffset(
    StyleResolverState& state,
    const CSSValue& value) {
  return ConvertLengthOrAuto(state, value);
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
  const auto& list = To<CSSValueList>(value);
  DCHECK_GE(list.length(), 2u);
  DCHECK(list.Item(0).IsPrimitiveValue() || list.Item(0).IsIdentifierValue());
  DCHECK(list.Item(1).IsPrimitiveValue() || list.Item(1).IsIdentifierValue());
  float z = 0;
  if (list.length() == 3) {
    DCHECK(list.Item(2).IsPrimitiveValue());
    z = StyleBuilderConverter::ConvertComputedLength<float>(state,
                                                            list.Item(2));
  }

  return TransformOrigin(
      ConvertPositionLength<CSSValueID::kLeft, CSSValueID::kRight>(
          state, list.Item(0)),
      ConvertPositionLength<CSSValueID::kTop, CSSValueID::kBottom>(
          state, list.Item(1)),
      z);
}

cc::ScrollSnapType StyleBuilderConverter::ConvertSnapType(
    StyleResolverState&,
    const CSSValue& value) {
  cc::ScrollSnapType snapType =
      ComputedStyleInitialValues::InitialScrollSnapType();
  if (const auto* pair = DynamicTo<CSSValuePair>(value)) {
    snapType.is_none = false;
    snapType.axis =
        To<CSSIdentifierValue>(pair->First()).ConvertTo<cc::SnapAxis>();
    snapType.strictness =
        To<CSSIdentifierValue>(pair->Second()).ConvertTo<cc::SnapStrictness>();
    return snapType;
  }

  if (To<CSSIdentifierValue>(value).GetValueID() == CSSValueID::kNone) {
    snapType.is_none = true;
    return snapType;
  }

  snapType.is_none = false;
  snapType.axis = To<CSSIdentifierValue>(value).ConvertTo<cc::SnapAxis>();
  return snapType;
}

cc::ScrollSnapAlign StyleBuilderConverter::ConvertSnapAlign(
    StyleResolverState&,
    const CSSValue& value) {
  cc::ScrollSnapAlign snapAlign =
      ComputedStyleInitialValues::InitialScrollSnapAlign();
  if (const auto* pair = DynamicTo<CSSValuePair>(value)) {
    snapAlign.alignment_block =
        To<CSSIdentifierValue>(pair->First()).ConvertTo<cc::SnapAlignment>();
    snapAlign.alignment_inline =
        To<CSSIdentifierValue>(pair->Second()).ConvertTo<cc::SnapAlignment>();
  } else {
    snapAlign.alignment_block =
        To<CSSIdentifierValue>(value).ConvertTo<cc::SnapAlignment>();
    snapAlign.alignment_inline = snapAlign.alignment_block;
  }
  return snapAlign;
}

TranslateTransformOperation* StyleBuilderConverter::ConvertTranslate(
    StyleResolverState& state,
    const CSSValue& value) {
  if (auto* identifier_value = DynamicTo<CSSIdentifierValue>(value)) {
    DCHECK_EQ(identifier_value->GetValueID(), CSSValueID::kNone);
    return nullptr;
  }
  const auto& list = To<CSSValueList>(value);
  DCHECK_LE(list.length(), 3u);
  Length tx = ConvertLength(state, list.Item(0));
  Length ty = Length::Fixed(0);
  double tz = 0;
  if (list.length() >= 2) {
    ty = ConvertLength(state, list.Item(1));
  }
  if (list.length() == 3) {
    tz = To<CSSPrimitiveValue>(list.Item(2))
             .ComputeLength<double>(state.CssToLengthConversionData());
  }

  return MakeGarbageCollected<TranslateTransformOperation>(
      tx, ty, tz, TransformOperation::kTranslate3D);
}

Rotation StyleBuilderConverter::ConvertRotation(
    const CSSLengthResolver& length_resolver,
    const CSSValue& value) {
  if (auto* identifier_value = DynamicTo<CSSIdentifierValue>(value)) {
    DCHECK_EQ(identifier_value->GetValueID(), CSSValueID::kNone);
    return Rotation(gfx::Vector3dF(0, 0, 1), 0);
  }

  const auto& list = To<CSSValueList>(value);
  DCHECK(list.length() == 1 || list.length() == 2);
  double x = 0;
  double y = 0;
  double z = 1;
  if (list.length() == 2) {
    // axis angle
    const cssvalue::CSSAxisValue& axis =
        To<cssvalue::CSSAxisValue>(list.Item(0));
    std::tie(x, y, z) = axis.ComputeAxis(length_resolver);
  }
  const CSSPrimitiveValue& angle =
      To<CSSPrimitiveValue>(list.Item(list.length() - 1));
  return Rotation(gfx::Vector3dF(x, y, z),
                  angle.ComputeDegrees(length_resolver));
}

RotateTransformOperation* StyleBuilderConverter::ConvertRotate(
    StyleResolverState& state,
    const CSSValue& value) {
  if (auto* identifier_value = DynamicTo<CSSIdentifierValue>(value)) {
    DCHECK_EQ(identifier_value->GetValueID(), CSSValueID::kNone);
    return nullptr;
  }

  return MakeGarbageCollected<RotateTransformOperation>(
      ConvertRotation(state.CssToLengthConversionData(), value),
      TransformOperation::kRotate3D);
}

ScaleTransformOperation* StyleBuilderConverter::ConvertScale(
    StyleResolverState& state,
    const CSSValue& value) {
  if (auto* identifier_value = DynamicTo<CSSIdentifierValue>(value)) {
    DCHECK_EQ(identifier_value->GetValueID(), CSSValueID::kNone);
    return nullptr;
  }

  const auto& list = To<CSSValueList>(value);
  DCHECK_LE(list.length(), 3u);
  double sx = To<CSSPrimitiveValue>(list.Item(0))
                  .ComputeNumber(state.CssToLengthConversionData());
  double sy = sx;
  double sz = 1;
  if (list.length() >= 2) {
    sy = To<CSSPrimitiveValue>(list.Item(1))
             .ComputeNumber(state.CssToLengthConversionData());
  }
  if (list.length() == 3) {
    sz = To<CSSPrimitiveValue>(list.Item(2))
             .ComputeNumber(state.CssToLengthConversionData());
  }

  return MakeGarbageCollected<ScaleTransformOperation>(
      sx, sy, sz, TransformOperation::kScale3D);
}

RespectImageOrientationEnum StyleBuilderConverter::ConvertImageOrientation(
    StyleResolverState& state,
    const CSSValue& value) {
  // The default is kFromImage, so branch on the only other valid value, kNone.
  return To<CSSIdentifierValue>(value).GetValueID() == CSSValueID::kNone
             ? kDoNotRespectImageOrientation
             : kRespectImageOrientation;
}

StylePath* StyleBuilderConverter::ConvertPathOrNone(StyleResolverState& state,
                                                    const CSSValue& value) {
  if (auto* path_value = DynamicTo<cssvalue::CSSPathValue>(value)) {
    return path_value->GetStylePath();
  }
  DCHECK_EQ(To<CSSIdentifierValue>(value).GetValueID(), CSSValueID::kNone);
  return nullptr;
}

namespace {

OffsetPathOperation* ConvertOffsetPathValueToOperation(
    StyleResolverState& state,
    const CSSValue& value,
    CoordBox coord_box) {
  if (auto* url_value = DynamicTo<cssvalue::CSSURIValue>(value)) {
    return MakeGarbageCollected<ReferenceOffsetPathOperation>(
        url_value->ValueForSerialization(),
        state.GetSVGResource(CSSPropertyID::kOffsetPath, *url_value),
        coord_box);
  }
  return MakeGarbageCollected<ShapeOffsetPathOperation>(
      BasicShapeForValue(state, value), coord_box);
}

}  // namespace

OffsetPathOperation* StyleBuilderConverter::ConvertOffsetPath(
    StyleResolverState& state,
    const CSSValue& value) {
  if (value.IsIdentifierValue()) {
    DCHECK(To<CSSIdentifierValue>(value).GetValueID() == CSSValueID::kNone);
    // none: The element does not have an offset transform.
    return nullptr;
  }
  const auto& list = To<CSSValueList>(value);
  if (const auto* identifier = DynamicTo<CSSIdentifierValue>(list.First())) {
    // If <offset-path> is omitted, it defaults to inset(0 round X),
    // where X is the value of border-radius on the element that
    // establishes the containing block for this element.
    return MakeGarbageCollected<CoordBoxOffsetPathOperation>(
        identifier->ConvertTo<CoordBox>());
  }
  // If <coord-box> is omitted, it defaults to border-box.
  CoordBox coord_box =
      list.length() == 2
          ? To<CSSIdentifierValue>(list.Last()).ConvertTo<CoordBox>()
          : CoordBox::kBorderBox;
  return ConvertOffsetPathValueToOperation(state, list.First(), coord_box);
}

BasicShape* StyleBuilderConverter::ConvertObjectViewBox(
    StyleResolverState& state,
    const CSSValue& value) {
  if (!value.IsBasicShapeInsetValue() && !value.IsBasicShapeRectValue() &&
      !value.IsBasicShapeXYWHValue()) {
    return nullptr;
  }
  return BasicShapeForValue(state, value);
}

static const CSSValue& ComputeColorValue(
    const CSSToLengthConversionData& conversion_data,
    const CSSValue& color_value,
    const Document& document,
    mojom::blink::ColorScheme color_scheme) {
  const ResolveColorValueContext context{
      .conversion_data = conversion_data,
      .text_link_colors = document.GetTextLinkColors(),
      .used_color_scheme = color_scheme,
      .color_provider = document.GetColorProviderForPainting(color_scheme),
      .is_in_web_app_scope = document.IsInWebAppScope(),
      .for_visited_link = false};
  const StyleColor style_color = ResolveColorValue(color_value, context);
  return *style_color.ToCSSValue();
}

static const CSSValue& ComputeRegisteredPropertyValue(
    const Document& document,
    const StyleResolverState* state,
    const CSSToLengthConversionData& css_to_length_conversion_data,
    const CSSValue& value,
    const CSSParserContext* context) {
  // TODO(timloh): Images values can also contain lengths.
  if (const auto* function_value = DynamicTo<CSSFunctionValue>(value)) {
    CSSFunctionValue* new_function =
        MakeGarbageCollected<CSSFunctionValue>(function_value->FunctionType());
    for (const CSSValue* inner_value : To<CSSValueList>(value)) {
      new_function->Append(ComputeRegisteredPropertyValue(
          document, state, css_to_length_conversion_data, *inner_value,
          context));
    }
    return *new_function;
  }

  if (const auto* old_list = DynamicTo<CSSValueList>(value)) {
    CSSValueList* new_list = CSSValueList::CreateWithSeparatorFrom(*old_list);
    for (const CSSValue* inner_value : *old_list) {
      new_list->Append(ComputeRegisteredPropertyValue(
          document, state, css_to_length_conversion_data, *inner_value,
          context));
    }
    return *new_list;
  }

  if (const auto* primitive_value = DynamicTo<CSSPrimitiveValue>(value)) {
    // For simple (non-calculated) px or percentage values, we do not need to
    // convert, as the value already has the proper computed form.
    if (!primitive_value->IsCalculated() &&
        (primitive_value->IsPx() || primitive_value->IsPercentage())) {
      return value;
    }

    if (primitive_value->IsLength() || primitive_value->IsPercentage() ||
        !primitive_value->IsResolvableBeforeLayout()) {
      // Instead of the actual zoom, use 1 to avoid potential rounding errors
      Length length = primitive_value->ConvertToLength(
          css_to_length_conversion_data.Unzoomed());
      return *CSSPrimitiveValue::CreateFromLength(length, 1);
    }

    // Clamp/round calc() values according to the permitted range.
    //
    // https://drafts.csswg.org/css-values-4/#calc-type-checking
    if (primitive_value->IsNumber() && primitive_value->IsCalculated()) {
      const CSSMathFunctionValue& math_value =
          To<CSSMathFunctionValue>(*primitive_value);
      // Note that GetDoubleValue automatically clamps according to the
      // permitted range.
      return *CSSNumericLiteralValue::Create(
          math_value.ComputeNumber(css_to_length_conversion_data),
          CSSPrimitiveValue::UnitType::kNumber);
    }

    if (primitive_value->IsAngle()) {
      return *CSSNumericLiteralValue::Create(
          primitive_value->ComputeDegrees(css_to_length_conversion_data),
          CSSPrimitiveValue::UnitType::kDegrees);
    }

    if (primitive_value->IsTime()) {
      return *CSSNumericLiteralValue::Create(
          primitive_value->ComputeSeconds(css_to_length_conversion_data),
          CSSPrimitiveValue::UnitType::kSeconds);
    }

    if (primitive_value->IsResolution()) {
      return *CSSNumericLiteralValue::Create(
          primitive_value->ComputeDotsPerPixel(css_to_length_conversion_data),
          CSSPrimitiveValue::UnitType::kDotsPerPixel);
    }
  }

  mojom::blink::ColorScheme color_scheme =
      state ? state->StyleBuilder().UsedColorScheme()
            : mojom::blink::ColorScheme::kLight;

  if (auto* identifier_value = DynamicTo<CSSIdentifierValue>(value)) {
    CSSValueID value_id = identifier_value->GetValueID();
    if (value_id == CSSValueID::kCurrentcolor) {
      return value;
    }
    if (StyleColor::IsColorKeyword(value_id)) {
      return ComputeColorValue(css_to_length_conversion_data, *identifier_value,
                               document, color_scheme);
    }
  }

  if (const auto* uri_value = DynamicTo<cssvalue::CSSURIValue>(value)) {
    const KURL& base_url = context ? context->BaseURL() : KURL();
    const TextEncoding& charset = context ? context->Charset() : TextEncoding();
    return *uri_value->ComputedCSSValue(base_url, charset);
  }

  if (const auto* light_dark_pair = DynamicTo<CSSLightDarkValuePair>(value)) {
    const CSSValue& selected_value =
        color_scheme == mojom::blink::ColorScheme::kLight
            ? light_dark_pair->First()
            : light_dark_pair->Second();
    return ComputeRegisteredPropertyValue(document, state,
                                          css_to_length_conversion_data,
                                          selected_value, context);
  }

  if (auto* color_mix_value = DynamicTo<cssvalue::CSSColorMixValue>(value)) {
    return ComputeColorValue(css_to_length_conversion_data, *color_mix_value,
                             document, color_scheme);
  }

  if (auto* relative_color_value =
          DynamicTo<cssvalue::CSSRelativeColorValue>(value)) {
    return ComputeColorValue(css_to_length_conversion_data,
                             *relative_color_value, document, color_scheme);
  }

  if (auto* unresolved_color_value =
          DynamicTo<cssvalue::CSSUnresolvedColorValue>(value)) {
    return ComputeColorValue(css_to_length_conversion_data,
                             *unresolved_color_value, document, color_scheme);
  }
  if (auto* custom_ident = DynamicTo<CSSCustomIdentValue>(value)) {
    return *custom_ident->Resolve(css_to_length_conversion_data);
  }
  return value;
}

const CSSValue& StyleBuilderConverter::ConvertRegisteredPropertyInitialValue(
    Document& document,
    const CSSValue& value) {
  CSSToLengthConversionData::FontSizes font_sizes;
  CSSToLengthConversionData::LineHeightSize line_height_size;
  CSSToLengthConversionData::ViewportSize viewport_size(
      document.GetLayoutView());
  CSSToLengthConversionData::ContainerSizes container_sizes;
  CSSToLengthConversionData::AnchorData anchor_data;
  CSSToLengthConversionData::Flags ignored_flags = 0;
  CSSToLengthConversionData conversion_data(
      WritingMode::kHorizontalTb, font_sizes, line_height_size, viewport_size,
      container_sizes, anchor_data,
      /* zoom */ 1.0f, ignored_flags, /*element=*/nullptr);

  const CSSParserContext* parser_context =
      document.ElementSheet().Contents()->ParserContext();
  return ComputeRegisteredPropertyValue(document, nullptr /* state */,
                                        conversion_data, value, parser_context);
}

const CSSValue& StyleBuilderConverter::ConvertRegisteredPropertyValue(
    const StyleResolverState& state,
    const CSSValue& value,
    const CSSParserContext* parser_context) {
  return ComputeRegisteredPropertyValue(state.GetDocument(), &state,
                                        state.CssToLengthConversionData(),
                                        value, parser_context);
}

// Registered properties need to substitute as absolute values. This means
// that 'em' units (for instance) are converted to 'px ' and calc()-expressions
// are resolved. This function creates new tokens equivalent to the computed
// value of the registered property.
//
// This is necessary to make things like font-relative units in inherited
// (and registered) custom properties work correctly.
//
// https://drafts.css-houdini.org/css-properties-values-api-1/#substitution
CSSVariableData* StyleBuilderConverter::ConvertRegisteredPropertyVariableData(
    const CSSValue& value,
    bool is_animation_tainted,
    bool is_attr_tainted) {
  // TODO(andruud): Produce tokens directly from CSSValue.
  return CSSVariableData::Create(value.CssText(), is_animation_tainted,
                                 is_attr_tainted,
                                 /* needs_variable_resolution */ false);
}

namespace {
gfx::SizeF GetRatioFromList(const CSSLengthResolver& length_resolver,
                            const CSSValueList& list) {
  auto* ratio = DynamicTo<cssvalue::CSSRatioValue>(list.Item(0));
  if (!ratio) {
    DCHECK_EQ(list.length(), 2u);
    ratio = DynamicTo<cssvalue::CSSRatioValue>(list.Item(1));
  }
  DCHECK(ratio);
  return gfx::SizeF(ratio->First().ComputeNumber(length_resolver),
                    ratio->Second().ComputeNumber(length_resolver));
}

bool ListHasAuto(const CSSValueList& list) {
  // If there's only one entry, it needs to be a ratio.
  // (A single auto is handled separately)
  if (list.length() == 1u) {
    return false;
  }
  auto* auto_value = DynamicTo<CSSIdentifierValue>(list.Item(0));
  if (!auto_value) {
    auto_value = DynamicTo<CSSIdentifierValue>(list.Item(1));
  }
  DCHECK(auto_value) << "If we have two items, one of them must be auto";
  DCHECK_EQ(auto_value->GetValueID(), CSSValueID::kAuto);
  return true;
}
}  // namespace

StyleAspectRatio StyleBuilderConverter::ConvertAspectRatio(
    const StyleResolverState& state,
    const CSSValue& value) {
  auto* identifier_value = DynamicTo<CSSIdentifierValue>(value);
  if (identifier_value && identifier_value->GetValueID() == CSSValueID::kAuto) {
    return StyleAspectRatio(EAspectRatioType::kAuto, gfx::SizeF());
  }

  // (auto, (1, 2)) or ((1, 2), auto) or ((1, 2))
  const CSSValueList& list = To<CSSValueList>(value);
  DCHECK_GE(list.length(), 1u);
  DCHECK_LE(list.length(), 2u);

  bool has_auto = ListHasAuto(list);
  EAspectRatioType type =
      has_auto ? EAspectRatioType::kAutoAndRatio : EAspectRatioType::kRatio;
  gfx::SizeF ratio = GetRatioFromList(state.CssToLengthConversionData(), list);
  return StyleAspectRatio(type, ratio);
}

bool StyleBuilderConverter::ConvertInternalAlignContentBlock(
    StyleResolverState&,
    const CSSValue& value) {
  auto* identifier_value = DynamicTo<CSSIdentifierValue>(value);
  return identifier_value &&
         identifier_value->GetValueID() == CSSValueID::kCenter;
}

bool StyleBuilderConverter::ConvertInternalEmptyLineHeight(
    StyleResolverState&,
    const CSSValue& value) {
  auto* identifier_value = DynamicTo<CSSIdentifierValue>(value);
  return identifier_value &&
         identifier_value->GetValueID() == CSSValueID::kFabricated;
}

AtomicString StyleBuilderConverter::ConvertPage(StyleResolverState& state,
                                                const CSSValue& value) {
  if (auto* custom_ident_value = DynamicTo<CSSCustomIdentValue>(value)) {
    return AtomicString(custom_ident_value->Value());
  }
  DCHECK(DynamicTo<CSSIdentifierValue>(value));
  DCHECK_EQ(DynamicTo<CSSIdentifierValue>(value)->GetValueID(),
            CSSValueID::kAuto);
  return AtomicString();
}

RubyPosition StyleBuilderConverter::ConvertRubyPosition(
    StyleResolverState& state,
    const CSSValue& value) {
  if (const auto* identifier_value = DynamicTo<CSSIdentifierValue>(value)) {
    CSSValueID value_id = identifier_value->GetValueID();
    if (value_id == CSSValueID::kBefore) {
      return RubyPosition::kOver;
    }
    if (value_id == CSSValueID::kAfter) {
      return RubyPosition::kUnder;
    }
    return identifier_value->ConvertTo<blink::RubyPosition>();
  }
  NOTREACHED();
}

StyleScrollbarColor* StyleBuilderConverter::ConvertScrollbarColor(
    StyleResolverState& state,
    const CSSValue& value) {
  auto* identifier_value = DynamicTo<CSSIdentifierValue>(value);
  if (identifier_value && identifier_value->GetValueID() == CSSValueID::kAuto) {
    return nullptr;
  }

  const CSSValueList& list = To<CSSValueList>(value);
  DCHECK_GE(list.length(), 1u);
  DCHECK_LE(list.length(), 2u);
  const StyleColor thumb_color = ConvertStyleColor(state, list.First());
  const StyleColor track_color = ConvertStyleColor(state, list.Last());

  return MakeGarbageCollected<StyleScrollbarColor>(thumb_color, track_color);
}

ScrollbarGutter StyleBuilderConverter::ConvertScrollbarGutter(
    StyleResolverState& state,
    const CSSValue& value) {
  ScrollbarGutter flags = kScrollbarGutterAuto;

  auto process = [&flags](const CSSValue& identifier) {
    flags |= To<CSSIdentifierValue>(identifier).ConvertTo<ScrollbarGutter>();
  };

  if (auto* value_list = DynamicTo<CSSValueList>(value)) {
    for (auto& entry : *value_list) {
      process(*entry);
    }
  } else {
    process(value);
  }
  return flags;
}

ScopedCSSNameList* StyleBuilderConverter::ConvertContainerName(
    StyleResolverState& state,
    const CSSValue& value) {
  DCHECK(value.IsScopedValue());
  if (IsA<CSSIdentifierValue>(value)) {
    DCHECK_EQ(To<CSSIdentifierValue>(value).GetValueID(), CSSValueID::kNone);
    return nullptr;
  }
  DCHECK(value.IsBaseValueList());
  HeapVector<Member<const ScopedCSSName>> names;
  for (const Member<const CSSValue>& item : To<CSSValueList>(value)) {
    names.push_back(ConvertNoneOrCustomIdent(state, *item));
  }
  return MakeGarbageCollected<ScopedCSSNameList>(std::move(names));
}

StyleIntrinsicLength StyleBuilderConverter::ConvertIntrinsicDimension(
    const StyleResolverState& state,
    const CSSValue& value) {
  // The valid grammar for this value is the following:
  // `[ auto | from-element ]? [ none | <length [0,∞]> ]`.
  if (const CSSValueList* list = DynamicTo<CSSValueList>(value)) {
    DCHECK_EQ(list->length(), 2u);
    DCHECK(IsA<CSSIdentifierValue>(list->Item(0)));
    if (RuntimeEnabledFeatures::ResponsiveIframesEnabled()) {
      const auto& identifier = To<CSSIdentifierValue>(list->Item(0));
      if (identifier.GetValueID() == CSSValueID::kFromElement) {
        return StyleIntrinsicLength::CreateFromElement(
            ConvertLengthOrNone(state, list->Item(1)));
      }
    }
    DCHECK(To<CSSIdentifierValue>(list->Item(0)).GetValueID() ==
           CSSValueID::kAuto);
    return StyleIntrinsicLength(ConvertLengthOrNone(state, list->Item(1)),
                                {.has_auto = true});
  }

  return StyleIntrinsicLength(ConvertLengthOrNone(state, value));
}

ColorSchemeFlags StyleBuilderConverter::ExtractColorSchemes(
    const Document& document,
    const CSSValueList& scheme_list,
    Vector<AtomicString>* color_schemes) {
  ColorSchemeFlags flags =
      static_cast<ColorSchemeFlags>(ColorSchemeFlag::kNormal);
  for (auto& item : scheme_list) {
    if (const auto* custom_ident = DynamicTo<CSSCustomIdentValue>(*item)) {
      if (color_schemes) {
        color_schemes->push_back(custom_ident->Value());
      }
    } else if (const auto* ident = DynamicTo<CSSIdentifierValue>(*item)) {
      if (color_schemes) {
        color_schemes->push_back(ident->CssText());
      }
      switch (ident->GetValueID()) {
        case CSSValueID::kDark:
          flags |= static_cast<ColorSchemeFlags>(ColorSchemeFlag::kDark);
          break;
        case CSSValueID::kLight:
          flags |= static_cast<ColorSchemeFlags>(ColorSchemeFlag::kLight);
          break;
        case CSSValueID::kOnly:
          flags |= static_cast<ColorSchemeFlags>(ColorSchemeFlag::kOnly);
          break;
        default:
          break;
      }
    } else {
      NOTREACHED();
    }
  }
  return flags;
}

double StyleBuilderConverter::ConvertTimeValue(const StyleResolverState& state,
                                               const CSSValue& value) {
  return To<CSSPrimitiveValue>(value).ComputeSeconds(
      state.CssToLengthConversionData());
}

std::optional<StyleOverflowClipMargin>
StyleBuilderConverter::ConvertOverflowClipMargin(StyleResolverState& state,
                                                 const CSSValue& value) {
  const auto& css_value_list = To<CSSValueList>(value);
  DCHECK(css_value_list.length() == 1u || css_value_list.length() == 2u);

  const CSSIdentifierValue* reference_box_value = nullptr;
  const CSSPrimitiveValue* length_value = nullptr;

  if (css_value_list.Item(0).IsIdentifierValue()) {
    reference_box_value = &To<CSSIdentifierValue>(css_value_list.Item(0));
  } else {
    DCHECK(css_value_list.Item(0).IsPrimitiveValue());
    length_value = &To<CSSPrimitiveValue>(css_value_list.Item(0));
  }

  if (css_value_list.length() > 1) {
    const auto& primitive_value = css_value_list.Item(1);
    DCHECK(primitive_value.IsPrimitiveValue());
    DCHECK(!length_value);
    length_value = &To<CSSPrimitiveValue>(primitive_value);
  }

  auto reference_box = StyleOverflowClipMargin::ReferenceBox::kPaddingBox;
  if (reference_box_value) {
    switch (reference_box_value->GetValueID()) {
      case CSSValueID::kBorderBox:
        reference_box = StyleOverflowClipMargin::ReferenceBox::kBorderBox;
        break;
      case CSSValueID::kContentBox:
        reference_box = StyleOverflowClipMargin::ReferenceBox::kContentBox;
        break;
      case CSSValueID::kPaddingBox:
        reference_box = StyleOverflowClipMargin::ReferenceBox::kPaddingBox;
        break;
      default:
        NOTREACHED();
    }
  }

  LayoutUnit margin;
  if (length_value) {
    margin = StyleBuilderConverter::ConvertLayoutUnit(state, *length_value);
  }
  return StyleOverflowClipMargin(reference_box, margin);
}

Vector<TimelineAxis> StyleBuilderConverter::ConvertViewTimelineAxis(
    StyleResolverState& state,
    const CSSValue& value) {
  Vector<TimelineAxis> axes;
  for (const Member<const CSSValue>& item : To<CSSValueList>(value)) {
    axes.push_back(To<CSSIdentifierValue>(*item).ConvertTo<TimelineAxis>());
  }
  return axes;
}

TimelineInset StyleBuilderConverter::ConvertSingleTimelineInset(
    StyleResolverState& state,
    const CSSValue& value) {
  const CSSValuePair& pair = To<CSSValuePair>(value);
  Length start =
      StyleBuilderConverter::ConvertLengthOrAuto(state, pair.First());
  Length end = StyleBuilderConverter::ConvertLengthOrAuto(state, pair.Second());
  return TimelineInset(start, end);
}

Vector<TimelineInset> StyleBuilderConverter::ConvertViewTimelineInset(
    StyleResolverState& state,
    const CSSValue& value) {
  Vector<TimelineInset> insets;
  for (const Member<const CSSValue>& item : To<CSSValueList>(value)) {
    insets.push_back(ConvertSingleTimelineInset(state, *item));
  }
  return insets;
}

ScopedCSSNameList* StyleBuilderConverter::ConvertViewTimelineName(
    StyleResolverState& state,
    const CSSValue& value) {
  return ConvertNoneOrCustomIdentList(state, value);
}

ScopedCSSNameList* StyleBuilderConverter::ConvertTimelineScope(
    StyleResolverState& state,
    const CSSValue& value) {
  if (value.IsIdentifierValue()) {
    DCHECK_EQ(CSSValueID::kNone, To<CSSIdentifierValue>(value).GetValueID());
    return nullptr;
  }
  DCHECK(value.IsScopedValue());
  DCHECK(value.IsBaseValueList());
  HeapVector<Member<const ScopedCSSName>> names;
  for (const Member<const CSSValue>& item : To<CSSValueList>(value)) {
    names.push_back(ConvertCustomIdent(state, *item));
  }
  return MakeGarbageCollected<ScopedCSSNameList>(std::move(names));
}

PositionArea StyleBuilderConverter::ConvertPositionArea(
    StyleResolverState& state,
    const CSSValue& value,
    bool allow_any_keyword) {
  auto extract_position_area_span = [](CSSValueID value)
      -> std::pair<PositionAreaRegion, PositionAreaRegion> {
    PositionAreaRegion start = PositionAreaRegion::kNone;
    PositionAreaRegion end = PositionAreaRegion::kNone;
    switch (value) {
      case CSSValueID::kSpanAll:
        start = end = PositionAreaRegion::kAll;
        break;
      case CSSValueID::kCenter:
        start = end = PositionAreaRegion::kCenter;
        break;
      case CSSValueID::kLeft:
        start = end = PositionAreaRegion::kLeft;
        break;
      case CSSValueID::kRight:
        start = end = PositionAreaRegion::kRight;
        break;
      case CSSValueID::kSpanLeft:
        start = PositionAreaRegion::kLeft;
        end = PositionAreaRegion::kCenter;
        break;
      case CSSValueID::kSpanRight:
        start = PositionAreaRegion::kCenter;
        end = PositionAreaRegion::kRight;
        break;
      case CSSValueID::kXStart:
        start = end = PositionAreaRegion::kXStart;
        break;
      case CSSValueID::kXEnd:
        start = end = PositionAreaRegion::kXEnd;
        break;
      case CSSValueID::kSpanXStart:
        start = PositionAreaRegion::kXStart;
        end = PositionAreaRegion::kCenter;
        break;
      case CSSValueID::kSpanXEnd:
        start = PositionAreaRegion::kCenter;
        end = PositionAreaRegion::kXEnd;
        break;
      case CSSValueID::kSelfXStart:
        start = end = PositionAreaRegion::kSelfXStart;
        break;
      case CSSValueID::kSelfXEnd:
        start = end = PositionAreaRegion::kSelfXEnd;
        break;
      case CSSValueID::kSpanSelfXStart:
        start = PositionAreaRegion::kSelfXStart;
        end = PositionAreaRegion::kCenter;
        break;
      case CSSValueID::kSpanSelfXEnd:
        start = PositionAreaRegion::kCenter;
        end = PositionAreaRegion::kSelfXEnd;
        break;
      case CSSValueID::kTop:
        start = end = PositionAreaRegion::kTop;
        break;
      case CSSValueID::kBottom:
        start = end = PositionAreaRegion::kBottom;
        break;
      case CSSValueID::kSpanTop:
        start = PositionAreaRegion::kTop;
        end = PositionAreaRegion::kCenter;
        break;
      case CSSValueID::kSpanBottom:
        start = PositionAreaRegion::kCenter;
        end = PositionAreaRegion::kBottom;
        break;
      case CSSValueID::kYStart:
        start = end = PositionAreaRegion::kYStart;
        break;
      case CSSValueID::kYEnd:
        start = end = PositionAreaRegion::kYEnd;
        break;
      case CSSValueID::kSpanYStart:
        start = PositionAreaRegion::kYStart;
        end = PositionAreaRegion::kCenter;
        break;
      case CSSValueID::kSpanYEnd:
        start = PositionAreaRegion::kCenter;
        end = PositionAreaRegion::kYEnd;
        break;
      case CSSValueID::kSelfYStart:
        start = end = PositionAreaRegion::kSelfYStart;
        break;
      case CSSValueID::kSelfYEnd:
        start = end = PositionAreaRegion::kSelfYEnd;
        break;
      case CSSValueID::kSpanSelfYStart:
        start = PositionAreaRegion::kSelfYStart;
        end = PositionAreaRegion::kCenter;
        break;
      case CSSValueID::kSpanSelfYEnd:
        start = PositionAreaRegion::kCenter;
        end = PositionAreaRegion::kSelfYEnd;
        break;
      case CSSValueID::kBlockStart:
        start = end = PositionAreaRegion::kBlockStart;
        break;
      case CSSValueID::kBlockEnd:
        start = end = PositionAreaRegion::kBlockEnd;
        break;
      case CSSValueID::kSpanBlockStart:
        start = PositionAreaRegion::kBlockStart;
        end = PositionAreaRegion::kCenter;
        break;
      case CSSValueID::kSpanBlockEnd:
        start = PositionAreaRegion::kCenter;
        end = PositionAreaRegion::kBlockEnd;
        break;
      case CSSValueID::kSelfBlockStart:
        start = end = PositionAreaRegion::kSelfBlockStart;
        break;
      case CSSValueID::kSelfBlockEnd:
        start = end = PositionAreaRegion::kSelfBlockEnd;
        break;
      case CSSValueID::kSpanSelfBlockStart:
        start = PositionAreaRegion::kSelfBlockStart;
        end = PositionAreaRegion::kCenter;
        break;
      case CSSValueID::kSpanSelfBlockEnd:
        start = PositionAreaRegion::kCenter;
        end = PositionAreaRegion::kSelfBlockEnd;
        break;
      case CSSValueID::kInlineStart:
        start = end = PositionAreaRegion::kInlineStart;
        break;
      case CSSValueID::kInlineEnd:
        start = end = PositionAreaRegion::kInlineEnd;
        break;
      case CSSValueID::kSpanInlineStart:
        start = PositionAreaRegion::kInlineStart;
        end = PositionAreaRegion::kCenter;
        break;
      case CSSValueID::kSpanInlineEnd:
        start = PositionAreaRegion::kCenter;
        end = PositionAreaRegion::kInlineEnd;
        break;
      case CSSValueID::kSelfInlineStart:
        start = end = PositionAreaRegion::kSelfInlineStart;
        break;
      case CSSValueID::kSelfInlineEnd:
        start = end = PositionAreaRegion::kSelfInlineEnd;
        break;
      case CSSValueID::kSpanSelfInlineStart:
        start = PositionAreaRegion::kSelfInlineStart;
        end = PositionAreaRegion::kCenter;
        break;
      case CSSValueID::kSpanSelfInlineEnd:
        start = PositionAreaRegion::kCenter;
        end = PositionAreaRegion::kSelfInlineEnd;
        break;
      case CSSValueID::kStart:
        start = end = PositionAreaRegion::kStart;
        break;
      case CSSValueID::kEnd:
        start = end = PositionAreaRegion::kEnd;
        break;
      case CSSValueID::kSpanStart:
        start = PositionAreaRegion::kStart;
        end = PositionAreaRegion::kCenter;
        break;
      case CSSValueID::kSpanEnd:
        start = PositionAreaRegion::kCenter;
        end = PositionAreaRegion::kEnd;
        break;
      case CSSValueID::kSelfStart:
        start = end = PositionAreaRegion::kSelfStart;
        break;
      case CSSValueID::kSelfEnd:
        start = end = PositionAreaRegion::kSelfEnd;
        break;
      case CSSValueID::kSpanSelfStart:
        start = PositionAreaRegion::kSelfStart;
        end = PositionAreaRegion::kCenter;
        break;
      case CSSValueID::kSpanSelfEnd:
        start = PositionAreaRegion::kCenter;
        end = PositionAreaRegion::kSelfEnd;
        break;
      case CSSValueID::kAny:
        start = end = PositionAreaRegion::kAny;
        break;
      default:
        NOTREACHED();
    }
    return std::make_pair(start, end);
  };

  if (const auto* single_keyword_value = DynamicTo<CSSIdentifierValue>(value)) {
    CSSValueID single_keyword = single_keyword_value->GetValueID();
    if (single_keyword == CSSValueID::kNone) {
      return PositionArea();
    }
    PositionAreaRegion span[2];
    std::tie(span[0], span[1]) = extract_position_area_span(single_keyword);
    if (css_parsing_utils::IsRepeatedPositionAreaValue(single_keyword)) {
      return PositionArea(span[0], span[1], span[0], span[1]);
    } else {
      PositionAreaRegion default_second_span = allow_any_keyword
                                                   ? PositionAreaRegion::kAny
                                                   : PositionAreaRegion::kAll;
      return PositionArea(span[0], span[1], default_second_span,
                          default_second_span);
    }
  }

  PositionAreaRegion span[4];
  const CSSValuePair& value_pair = To<CSSValuePair>(value);
  std::tie(span[0], span[1]) = extract_position_area_span(
      To<CSSIdentifierValue>(value_pair.First()).GetValueID());
  std::tie(span[2], span[3]) = extract_position_area_span(
      To<CSSIdentifierValue>(value_pair.Second()).GetValueID());

  return PositionArea(span[0], span[1], span[2], span[3]);
}

PositionTryFallback StyleBuilderConverter::ConvertSinglePositionTryFallback(
    StyleResolverState& state,
    const CSSValue& value,
    bool allow_any_keyword_in_position_area) {
  // <'position-area'>
  if (IsA<CSSValuePair>(value) || IsA<CSSIdentifierValue>(value)) {
    return PositionTryFallback(
        ConvertPositionArea(state, value, allow_any_keyword_in_position_area));
  }
  // [<dashed-ident> || <try-tactic>]
  const ScopedCSSName* scoped_name = nullptr;
  TryTacticList tactic_list = {TryTactic::kNone};
  wtf_size_t tactic_index = 0;
  for (const auto& name_or_tactic : To<CSSValueList>(value)) {
    if (const auto* name = DynamicTo<CSSCustomIdentValue>(*name_or_tactic)) {
      scoped_name = ConvertCustomIdent(state, *name);
      continue;
    }
    CHECK_LT(tactic_index, tactic_list.size());
    tactic_list[tactic_index++] =
        To<CSSIdentifierValue>(*name_or_tactic).ConvertTo<TryTactic>();
  }
  return PositionTryFallback(scoped_name, tactic_list);
}

FitText StyleBuilderConverter::ConvertFitText(StyleResolverState& state,
                                              const CSSValue& value) {
  const auto& list = To<CSSValueList>(value);
  const auto target_id = To<CSSIdentifierValue>(list.Item(0)).GetValueID();
  FitTextTarget target = target_id == CSSValueID::kNone ? FitTextTarget::kNone
                         : target_id == CSSValueID::kPerLine
                             ? FitTextTarget::kPerLine
                             : FitTextTarget::kConsistent;
  std::optional<FitTextMethod> method;
  wtf_size_t next_index = 1;
  if (list.length() > next_index && list.Item(next_index).IsIdentifierValue()) {
    const auto method_id =
        To<CSSIdentifierValue>(list.Item(next_index)).GetValueID();
    switch (method_id) {
      case CSSValueID::kScale:
        method.emplace(FitTextMethod::kScale);
        break;
      case CSSValueID::kFontSize:
        method.emplace(FitTextMethod::kFontSize);
        break;
      case CSSValueID::kScaleInline:
        method.emplace(FitTextMethod::kScaleInline);
        break;
      case CSSValueID::kLetterSpacing:
        method.emplace(FitTextMethod::kLetterSpacing);
        break;
      default:
        NOTREACHED();
    }
    ++next_index;
  }
  std::optional<float> size_limit;
  if (list.length() > next_index) {
    FontDescription::Size parent_size(FontSizeFunctions::InitialKeywordSize(),
                                      std::numeric_limits<float>::max(), true);
    size_limit.emplace(ComputeFontSize(
        state.CssToLengthConversionData(),
        To<CSSPrimitiveValue>(list.Item(next_index)), parent_size));
  }
  return FitText(target, method, size_limit);
}

TextOverflowData StyleBuilderConverter::ConvertTextOverflow(
    StyleResolverState& state,
    const CSSValue& value) {
  if (const auto* string_value = DynamicTo<CSSStringValue>(value)) {
    return TextOverflowData(string_value->Value());
  }
  const auto& identifier_value = To<CSSIdentifierValue>(value);
  if (identifier_value.GetValueID() == CSSValueID::kEllipsis) {
    return TextOverflowData(TextOverflowData::Type::kEllipsis);
  }
  DCHECK(identifier_value.GetValueID() == CSSValueID::kClip);
  return TextOverflowData(TextOverflowData::Type::kClip);
}

ScopedCSSNameList* StyleBuilderConverter::ConvertTimelineTriggerName(
    StyleResolverState& state,
    const CSSValue& value) {
  return ConvertNoneOrCustomIdentList(state, value);
}

StyleTriggerScope StyleBuilderConverter::ConvertTriggerScope(
    StyleResolverState& state,
    const CSSValue& value) {
  return ConvertNameScope(state, value);
}

}  // namespace blink
