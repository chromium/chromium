// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "third_party/blink/renderer/core/css/properties/computed_style_utils.h"

#include "base/memory/values_equivalent.h"
#include "third_party/blink/renderer/core/css/basic_shape_functions.h"
#include "third_party/blink/renderer/core/css/css_alternate_value.h"
#include "third_party/blink/renderer/core/css/css_border_image.h"
#include "third_party/blink/renderer/core/css/css_border_image_slice_value.h"
#include "third_party/blink/renderer/core/css/css_bracketed_value_list.h"
#include "third_party/blink/renderer/core/css/css_color.h"
#include "third_party/blink/renderer/core/css/css_color_mix_value.h"
#include "third_party/blink/renderer/core/css/css_content_distribution_value.h"
#include "third_party/blink/renderer/core/css/css_counter_value.h"
#include "third_party/blink/renderer/core/css/css_custom_ident_value.h"
#include "third_party/blink/renderer/core/css/css_font_family_value.h"
#include "third_party/blink/renderer/core/css/css_font_feature_value.h"
#include "third_party/blink/renderer/core/css/css_font_style_range_value.h"
#include "third_party/blink/renderer/core/css/css_font_variation_value.h"
#include "third_party/blink/renderer/core/css/css_function_value.h"
#include "third_party/blink/renderer/core/css/css_grid_auto_repeat_value.h"
#include "third_party/blink/renderer/core/css/css_grid_integer_repeat_value.h"
#include "third_party/blink/renderer/core/css/css_grid_template_areas_value.h"
#include "third_party/blink/renderer/core/css/css_initial_value.h"
#include "third_party/blink/renderer/core/css/css_math_function_value.h"
#include "third_party/blink/renderer/core/css/css_numeric_literal_value.h"
#include "third_party/blink/renderer/core/css/css_primitive_value_mappings.h"
#include "third_party/blink/renderer/core/css/css_quad_value.h"
#include "third_party/blink/renderer/core/css/css_reflect_value.h"
#include "third_party/blink/renderer/core/css/css_scroll_value.h"
#include "third_party/blink/renderer/core/css/css_shadow_value.h"
#include "third_party/blink/renderer/core/css/css_string_value.h"
#include "third_party/blink/renderer/core/css/css_timing_function_value.h"
#include "third_party/blink/renderer/core/css/css_uri_value.h"
#include "third_party/blink/renderer/core/css/css_value.h"
#include "third_party/blink/renderer/core/css/css_value_list.h"
#include "third_party/blink/renderer/core/css/css_value_pair.h"
#include "third_party/blink/renderer/core/css/css_view_value.h"
#include "third_party/blink/renderer/core/css/cssom/cross_thread_color_value.h"
#include "third_party/blink/renderer/core/css/cssom/cross_thread_keyword_value.h"
#include "third_party/blink/renderer/core/css/cssom/cross_thread_unit_value.h"
#include "third_party/blink/renderer/core/css/cssom/cross_thread_unparsed_value.h"
#include "third_party/blink/renderer/core/css/cssom/cross_thread_unsupported_value.h"
#include "third_party/blink/renderer/core/css/cssom/css_keyword_value.h"
#include "third_party/blink/renderer/core/css/cssom/css_unit_value.h"
#include "third_party/blink/renderer/core/css/cssom/css_unparsed_value.h"
#include "third_party/blink/renderer/core/css/cssom/css_unsupported_color.h"
#include "third_party/blink/renderer/core/css/cssom_utils.h"
#include "third_party/blink/renderer/core/css/properties/css_parsing_utils.h"
#include "third_party/blink/renderer/core/css/properties/longhands.h"
#include "third_party/blink/renderer/core/css/properties/shorthands.h"
#include "third_party/blink/renderer/core/css/style_color.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/layout/grid/layout_grid.h"
#include "third_party/blink/renderer/core/layout/layout_block.h"
#include "third_party/blink/renderer/core/layout/layout_box.h"
#include "third_party/blink/renderer/core/layout/svg/transform_helper.h"
#include "third_party/blink/renderer/core/style/computed_style_constants.h"
#include "third_party/blink/renderer/core/style/position_area.h"
#include "third_party/blink/renderer/core/style/style_intrinsic_length.h"
#include "third_party/blink/renderer/core/style/style_svg_resource.h"
#include "third_party/blink/renderer/core/style_property_shorthand.h"
#include "third_party/blink/renderer/core/svg/svg_rect_element.h"
#include "third_party/blink/renderer/core/svg_element_type_helpers.h"
#include "third_party/blink/renderer/platform/animation/timing_function.h"
#include "third_party/blink/renderer/platform/fonts/font_optical_sizing.h"
#include "third_party/blink/renderer/platform/fonts/font_palette.h"
#include "third_party/blink/renderer/platform/fonts/font_variant_emoji.h"
#include "third_party/blink/renderer/platform/fonts/opentype/font_settings.h"
#include "third_party/blink/renderer/platform/transforms/matrix_3d_transform_operation.h"
#include "third_party/blink/renderer/platform/transforms/matrix_transform_operation.h"
#include "third_party/blink/renderer/platform/transforms/perspective_transform_operation.h"
#include "third_party/blink/renderer/platform/transforms/skew_transform_operation.h"

namespace blink {

namespace {

const double kFinalStatePercentage = 100.0;
const double kMiddleStatePercentage = 50.0;

CSSValue* ConvertFontPaletteToCSSValue(const blink::FontPalette* palette) {
  switch (palette->GetPaletteNameKind()) {
    case blink::FontPalette::kNormalPalette:
      return CSSIdentifierValue::Create(CSSValueID::kNormal);
    case blink::FontPalette::kLightPalette:
      return CSSIdentifierValue::Create(CSSValueID::kLight);
    case blink::FontPalette::kDarkPalette:
      return CSSIdentifierValue::Create(CSSValueID::kDark);
    case blink::FontPalette::kCustomPalette:
      return MakeGarbageCollected<CSSCustomIdentValue>(
          palette->GetPaletteValuesName());
    case blink::FontPalette::kInterpolablePalette: {
      // TODO(crbug.com/1400620): Change the serialization of palette-mix()
      // function to match color-mix(), i.e.: palette-mix() =
      // palette-mix(<color-interpolation-method> , [ [normal | light | dark |
      // <palette-identifier> | <palette-mix()> ] && <percentage [0,100]>?
      // ]#{2})
      CSSFunctionValue* result =
          MakeGarbageCollected<CSSFunctionValue>(CSSValueID::kPaletteMix);

      CSSValueList* color_space_css_value_list =
          CSSValueList::CreateSpaceSeparated();
      color_space_css_value_list->Append(
          *MakeGarbageCollected<CSSCustomIdentValue>(AtomicString("in")));
      if (palette->GetHueInterpolationMethod().has_value()) {
        color_space_css_value_list->Append(
            *MakeGarbageCollected<CSSCustomIdentValue>(
                AtomicString(Color::SerializeInterpolationSpace(
                    palette->GetColorInterpolationSpace(),
                    *palette->GetHueInterpolationMethod()))));
      } else {
        color_space_css_value_list->Append(
            *MakeGarbageCollected<CSSCustomIdentValue>(
                AtomicString(Color::SerializeInterpolationSpace(
                    palette->GetColorInterpolationSpace()))));
      }
      result->Append(*color_space_css_value_list);

      double start_percentage_ = palette->GetStartPercentage();
      double end_percentage_ = palette->GetEndPercentage();

      CSSValueList* start_palette_with_percentage =
          CSSValueList::CreateSpaceSeparated();
      CSSValue* start = ConvertFontPaletteToCSSValue(palette->GetStart().get());
      start_palette_with_percentage->Append(*start);
      // Percentages in the palette-mix() function should be serialized the same
      // way they are serialized in color-mix() function. If the first
      // percentage is equal 50% and the two specified percentages add to 100%,
      // we should skip the first percentage in the serialization. Second
      // percentage should be skipped if it equals to 50%, or the two specified
      // percentages add to 100%. Compare:
      // https://drafts.csswg.org/css-color-5/#serial-color-mix.
      if (start_percentage_ + end_percentage_ != kFinalStatePercentage ||
          start_percentage_ != kMiddleStatePercentage) {
        CSSValue* param = CSSNumericLiteralValue::Create(
            start_percentage_, CSSPrimitiveValue::UnitType::kPercentage);
        start_palette_with_percentage->Append(*param);
      }
      result->Append(*start_palette_with_percentage);

      CSSValueList* end_palette_with_percentage =
          CSSValueList::CreateSpaceSeparated();
      CSSValue* end = ConvertFontPaletteToCSSValue(palette->GetEnd().get());
      if (*start == *end) {
        return start;
      }
      end_palette_with_percentage->Append(*end);
      if (start_percentage_ + end_percentage_ != kFinalStatePercentage) {
        CSSValue* param = CSSNumericLiteralValue::Create(
            end_percentage_, CSSPrimitiveValue::UnitType::kPercentage);
        end_palette_with_percentage->Append(*param);
      }
      result->Append(*end_palette_with_percentage);

      return result;
    }
    default:
      NOTREACHED_IN_MIGRATION();
  }
  return CSSIdentifierValue::Create(CSSValueID::kNormal);
}

}  // namespace

static Length Negate(const Length& length) {
  if (length.IsCalculated()) {
    NOTREACHED_IN_MIGRATION();
    return length;
  }

  Length ret = Length(-length.GetFloatValue(), length.GetType());
  ret.SetQuirk(length.Quirk());
  return ret;
}

// TODO(rjwright): make this const
CSSValue* ComputedStyleUtils::ZoomAdjustedPixelValueForLength(
    const Length& length,
    const ComputedStyle& style) {
  if (length.IsFixed()) {
    return ZoomAdjustedPixelValue(length.Value(), style);
  }
  return CSSValue::Create(length, style.EffectiveZoom());
}

CSSValue* ComputedStyleUtils::ValueForPosition(const LengthPoint& position,
                                               const ComputedStyle& style) {
  if (position.X().IsAuto()) {
    return CSSIdentifierValue::Create(CSSValueID::kAuto);
  }
  if (position.X().IsNone()) {
    return CSSIdentifierValue::Create(CSSValueID::kNormal);
  }

  return MakeGarbageCollected<CSSValuePair>(
      ZoomAdjustedPixelValueForLength(position.X(), style),
      ZoomAdjustedPixelValueForLength(position.Y(), style),
      CSSValuePair::kKeepIdenticalValues);
}

CSSValue* ComputedStyleUtils::ValueForOffset(const ComputedStyle& style,
                                             const LayoutObject* layout_object,
                                             bool allow_visited_style,
                                             CSSValuePhase value_phase) {
  CSSValueList* list = CSSValueList::CreateSpaceSeparated();
  CSSValue* position = ValueForPosition(style.OffsetPosition(), style);
  auto* position_identifier_value = DynamicTo<CSSIdentifierValue>(position);
  if (!position_identifier_value ||
      position_identifier_value->GetValueID() == CSSValueID::kAuto) {
    list->Append(*position);
  } else {
    DCHECK_EQ(position_identifier_value->GetValueID(), CSSValueID::kNormal);
  }

  static const CSSProperty* longhands[3] = {&GetCSSPropertyOffsetPath(),
                                            &GetCSSPropertyOffsetDistance(),
                                            &GetCSSPropertyOffsetRotate()};
  for (const CSSProperty* longhand : longhands) {
    const CSSValue* value = longhand->CSSValueFromComputedStyle(
        style, layout_object, allow_visited_style, value_phase);
    DCHECK(value);
    list->Append(*value);
  }

  CSSValue* anchor = ValueForPosition(style.OffsetAnchor(), style);
  auto* anchor_identifier_value = DynamicTo<CSSIdentifierValue>(anchor);
  if (!anchor_identifier_value) {
    // Add a slash before anchor.
    CSSValueList* result = CSSValueList::CreateSlashSeparated();
    result->Append(*list);
    result->Append(*anchor);
    return result;
  }
  DCHECK_EQ(anchor_identifier_value->GetValueID(), CSSValueID::kAuto);
  return list;
}

const CSSValue* ComputedStyleUtils::ValueForColor(
    const StyleColor& style_color) {
  if (style_color.IsUnresolvedColorFunction()) {
    return style_color.GetUnresolvedColorFunction().ToCSSValue();
  }
  if (style_color.IsCurrentColor()) {
    return CSSIdentifierValue::Create(CSSValueID::kCurrentcolor);
  }
  return cssvalue::CSSColor::Create(style_color.GetColor());
}

const CSSValue* ComputedStyleUtils::ValueForColor(
    const StyleColor& style_color,
    const ComputedStyle& style,
    const Color* override_current_color,
    CSSValuePhase value_phase) {
  const Color current_color = override_current_color ? *override_current_color
                                                     : style.GetCurrentColor();
  return cssvalue::CSSColor::Create(
      style_color.Resolve(current_color, style.UsedColorScheme()));
}

const CSSValue* ComputedStyleUtils::CurrentColorOrValidColor(
    const ComputedStyle& style,
    const StyleColor& color,
    CSSValuePhase value_phase) {
  return ValueForColor(color, style, nullptr, value_phase);
}

const blink::Color ComputedStyleUtils::BorderSideColor(
    const ComputedStyle& style,
    const StyleColor& color,
    EBorderStyle border_style,
    bool visited_link,
    bool* is_current_color) {
  Color current_color;
  if (visited_link) {
    current_color = style.GetInternalVisitedCurrentColor();
  } else if (border_style == EBorderStyle::kInset ||
             border_style == EBorderStyle::kOutset ||
             border_style == EBorderStyle::kRidge ||
             border_style == EBorderStyle::kGroove) {
    // FIXME: Treating styled borders with initial color differently causes
    // problems, see crbug.com/316559, crbug.com/276231
    current_color = blink::Color(238, 238, 238);
  } else {
    current_color = style.GetCurrentColor();
  }
  return color.Resolve(current_color, style.UsedColorScheme(),
                       is_current_color);
}

const CSSValue* ComputedStyleUtils::BackgroundImageOrMaskImage(
    const ComputedStyle& style,
    bool allow_visited_style,
    const FillLayer& fill_layer,
    CSSValuePhase value_phase) {
  CSSValueList* list = CSSValueList::CreateCommaSeparated();
  const FillLayer* curr_layer = &fill_layer;
  for (; curr_layer; curr_layer = curr_layer->Next()) {
    if (curr_layer->GetImage()) {
      list->Append(*curr_layer->GetImage()->ComputedCSSValue(
          style, allow_visited_style, value_phase));
    } else {
      list->Append(*CSSIdentifierValue::Create(CSSValueID::kNone));
    }
  }
  return list;
}

const CSSValue* ComputedStyleUtils::ValueForFillSize(
    const FillSize& fill_size,
    const ComputedStyle& style) {
  if (fill_size.type == EFillSizeType::kContain) {
    return CSSIdentifierValue::Create(CSSValueID::kContain);
  }

  if (fill_size.type == EFillSizeType::kCover) {
    return CSSIdentifierValue::Create(CSSValueID::kCover);
  }

  if (fill_size.size.Height().IsAuto()) {
    return ZoomAdjustedPixelValueForLength(fill_size.size.Width(), style);
  }

  return MakeGarbageCollected<CSSValuePair>(
      ZoomAdjustedPixelValueForLength(fill_size.size.Width(), style),
      ZoomAdjustedPixelValueForLength(fill_size.size.Height(), style),
      CSSValuePair::kKeepIdenticalValues);
}

const CSSValue* ComputedStyleUtils::BackgroundImageOrMaskSize(
    const ComputedStyle& style,
    const FillLayer& fill_layer) {
  CSSValueList* list = CSSValueList::CreateCommaSeparated();
  const FillLayer* curr_layer = &fill_layer;
  for (; curr_layer; curr_layer = curr_layer->Next()) {
    list->Append(*ValueForFillSize(curr_layer->Size(), style));
  }
  return list;
}

const CSSValueList* ComputedStyleUtils::CreatePositionListForLayer(
    const CSSProperty& property,
    const FillLayer& layer,
    const ComputedStyle& style) {
  CSSValueList* position_list = CSSValueList::CreateSpaceSeparated();
  if (layer.IsBackgroundXOriginSet()) {
    DCHECK(property.IDEquals(CSSPropertyID::kBackgroundPosition) ||
           property.IDEquals(CSSPropertyID::kMaskPosition));
    position_list->Append(
        *CSSIdentifierValue::Create(layer.BackgroundXOrigin()));
  }
  position_list->Append(
      *ZoomAdjustedPixelValueForLength(layer.PositionX(), style));
  if (layer.IsBackgroundYOriginSet()) {
    DCHECK(property.IDEquals(CSSPropertyID::kBackgroundPosition) ||
           property.IDEquals(CSSPropertyID::kMaskPosition));
    position_list->Append(
        *CSSIdentifierValue::Create(layer.BackgroundYOrigin()));
  }
  position_list->Append(
      *ZoomAdjustedPixelValueForLength(layer.PositionY(), style));
  return position_list;
}

const CSSValue* ComputedStyleUtils::ValueForFillRepeat(
    const FillLayer* curr_layer) {
  const auto& fill_repeat = curr_layer->Repeat();

  return MakeGarbageCollected<CSSRepeatStyleValue>(
      CSSIdentifierValue::Create(fill_repeat.x),
      CSSIdentifierValue::Create(fill_repeat.y));
}

const CSSValue* ComputedStyleUtils::RepeatStyle(const FillLayer* curr_layer) {
  CSSValueList* list = CSSValueList::CreateCommaSeparated();

  for (; curr_layer; curr_layer = curr_layer->Next()) {
    list->Append(*ValueForFillRepeat(curr_layer));
  }

  return list;
}

const CSSValue* ComputedStyleUtils::MaskMode(const FillLayer* curr_layer) {
  CSSValueList* list = CSSValueList::CreateCommaSeparated();
  for (; curr_layer; curr_layer = curr_layer->Next()) {
    list->Append(*CSSIdentifierValue::Create(curr_layer->MaskMode()));
  }
  return list;
}

const CSSValueList* ComputedStyleUtils::ValuesForBackgroundShorthand(
    const ComputedStyle& style,
    const LayoutObject* layout_object,
    bool allow_visited_style,
    CSSValuePhase value_phase) {
  CSSValueList* result = CSSValueList::CreateCommaSeparated();
  const FillLayer* curr_layer = &style.BackgroundLayers();
  for (; curr_layer; curr_layer = curr_layer->Next()) {
    CSSValueList* list = CSSValueList::CreateSlashSeparated();
    CSSValueList* before_slash = CSSValueList::CreateSpaceSeparated();
    if (!curr_layer->Next()) {  // color only for final layer
      const CSSValue* value =
          GetCSSPropertyBackgroundColor().CSSValueFromComputedStyle(
              style, layout_object, allow_visited_style, value_phase);
      DCHECK(value);
      before_slash->Append(*value);
    }
    before_slash->Append(curr_layer->GetImage()
                             ? *curr_layer->GetImage()->ComputedCSSValue(
                                   style, allow_visited_style, value_phase)
                             : *CSSIdentifierValue::Create(CSSValueID::kNone));
    before_slash->Append(*ValueForFillRepeat(curr_layer));
    before_slash->Append(*CSSIdentifierValue::Create(curr_layer->Attachment()));
    before_slash->Append(*CreatePositionListForLayer(
        GetCSSPropertyBackgroundPosition(), *curr_layer, style));
    list->Append(*before_slash);
    CSSValueList* after_slash = CSSValueList::CreateSpaceSeparated();
    after_slash->Append(*ValueForFillSize(curr_layer->Size(), style));
    after_slash->Append(*CSSIdentifierValue::Create(curr_layer->Origin()));
    after_slash->Append(*CSSIdentifierValue::Create(curr_layer->Clip()));
    list->Append(*after_slash);
    result->Append(*list);
  }
  return result;
}

namespace {

// Append clip and origin vals (https://drafts.fxtf.org/css-masking/#the-mask):
// * If one <geometry-box> value and the no-clip keyword are present then
//   <geometry-box> sets mask-origin and no-clip sets mask-clip to that value.
// * If one <geometry-box> value and no no-clip keyword are present then
//   <geometry-box> sets both mask-origin and mask-clip to that value.
// * If two <geometry-box> values are present, then the first sets mask-origin
//   and the second mask-clip.
// Additionally, simplifies when possible.
void AppendValuesForMaskClipAndOrigin(CSSValueList* result_list,
                                      EFillBox origin,
                                      EFillBox clip) {
  if (origin == clip) {
    // If both values are border-box, omit everything as it is the default.
    if (origin == EFillBox::kBorder) {
      return;
    }
    // If the values are the same, only emit one value. Note that mask-origin
    // does not support no-clip, so there is no need to consider no-clip
    // special cases.
    result_list->Append(*CSSIdentifierValue::Create(origin));
  } else if (origin == EFillBox::kBorder && clip == EFillBox::kNoClip) {
    // Mask-origin does not support no-clip, so mask-origin can be omitted if it
    // is the default.
    result_list->Append(*CSSIdentifierValue::Create(clip));
  } else {
    result_list->Append(*CSSIdentifierValue::Create(origin));
    result_list->Append(*CSSIdentifierValue::Create(clip));
  }
}

}  // namespace

const CSSValueList* ComputedStyleUtils::ValuesForMaskShorthand(
    const StylePropertyShorthand&,
    const ComputedStyle& style,
    const LayoutObject* layout_object,
    bool allow_visited_style,
    CSSValuePhase value_phase) {
  // Canonical order (https://drafts.fxtf.org/css-masking/#typedef-mask-layer):
  //   <mask-reference>              ||
  //   <position> [ / <bg-size> ]?   ||
  //   <repeat-style>                ||
  //   <geometry-box>                ||
  //   [ <geometry-box> | no-clip ]  ||
  //   <compositing-operator>        ||
  //   <masking-mode>
  // The logic below omits initial values due to the following spec:
  // https://drafts.csswg.org/cssom/#serialize-a-css-value
  // "If component values can be omitted or replaced with a shorter
  // representation without changing the meaning of the value, omit/replace
  // them".
  CSSValueList* result = CSSValueList::CreateCommaSeparated();
  const FillLayer* layer = &style.MaskLayers();
  for (; layer; layer = layer->Next()) {
    CSSValueList* list = CSSValueList::CreateSpaceSeparated();
    // <mask-reference>
    if (layer->GetImage()) {
      list->Append(*layer->GetImage()->ComputedCSSValue(
          style, allow_visited_style, value_phase));
    }
    // <position> [ / <bg-size> ]?
    if (layer->PositionX() !=
            FillLayer::InitialFillPositionX(EFillLayerType::kMask) ||
        layer->PositionY() !=
            FillLayer::InitialFillPositionY(EFillLayerType::kMask) ||
        layer->Size() != FillLayer::InitialFillSize(EFillLayerType::kMask)) {
      CSSValueList* position_size_list = CSSValueList::CreateSlashSeparated();
      position_size_list->Append(*CreatePositionListForLayer(
          GetCSSPropertyMaskPosition(), *layer, style));
      if (layer->Size() != FillLayer::InitialFillSize(EFillLayerType::kMask)) {
        position_size_list->Append(*ValueForFillSize(layer->Size(), style));
      }
      list->Append(*position_size_list);
    }
    // <repeat-style>
    if (layer->Repeat() !=
        FillLayer::InitialFillRepeat(EFillLayerType::kMask)) {
      list->Append(*ValueForFillRepeat(layer));
    }
    // <geometry-box>
    // [ <geometry-box> | no-clip ]
    AppendValuesForMaskClipAndOrigin(list, layer->Origin(), layer->Clip());
    // <compositing-operator>
    if (layer->CompositingOperator() !=
        FillLayer::InitialFillCompositingOperator(EFillLayerType::kMask)) {
      list->Append(*CSSIdentifierValue::Create(layer->CompositingOperator()));
    }
    // <masking-mode>
    if (layer->MaskMode() !=
        FillLayer::InitialFillMaskMode(EFillLayerType::kMask)) {
      list->Append(*CSSIdentifierValue::Create(layer->MaskMode()));
    }

    if (list->length()) {
      result->Append(*list);
    } else {
      result->Append(*CSSIdentifierValue::Create(CSSValueID::kNone));
    }
  }
  return result;
}

const CSSValue* ComputedStyleUtils::BackgroundPositionOrMaskPosition(
    const CSSProperty& resolved_property,
    const ComputedStyle& style,
    const FillLayer* curr_layer) {
  CSSValueList* list = CSSValueList::CreateCommaSeparated();
  for (; curr_layer; curr_layer = curr_layer->Next()) {
    list->Append(
        *CreatePositionListForLayer(resolved_property, *curr_layer, style));
  }
  return list;
}

const CSSValue* ComputedStyleUtils::BackgroundPositionXOrWebkitMaskPositionX(
    const ComputedStyle& style,
    const FillLayer* curr_layer) {
  CSSValueList* list = CSSValueList::CreateCommaSeparated();
  for (; curr_layer; curr_layer = curr_layer->Next()) {
    const Length& from_edge = curr_layer->PositionX();
    if (curr_layer->BackgroundXOrigin() == BackgroundEdgeOrigin::kRight) {
      // TODO(crbug.com/610627): This should use two-value syntax once the
      // parser accepts it.
      list->Append(*ZoomAdjustedPixelValueForLength(
          from_edge.SubtractFromOneHundredPercent(), style));
    } else {
      list->Append(*ZoomAdjustedPixelValueForLength(from_edge, style));
    }
  }
  return list;
}

const CSSValue* ComputedStyleUtils::BackgroundPositionYOrWebkitMaskPositionY(
    const ComputedStyle& style,
    const FillLayer* curr_layer) {
  CSSValueList* list = CSSValueList::CreateCommaSeparated();
  for (; curr_layer; curr_layer = curr_layer->Next()) {
    const Length& from_edge = curr_layer->PositionY();
    if (curr_layer->BackgroundYOrigin() == BackgroundEdgeOrigin::kBottom) {
      // TODO(crbug.com/610627): This should use two-value syntax once the
      // parser accepts it.
      list->Append(*ZoomAdjustedPixelValueForLength(
          from_edge.SubtractFromOneHundredPercent(), style));
    } else {
      list->Append(*ZoomAdjustedPixelValueForLength(from_edge, style));
    }
  }
  return list;
}

static CSSNumericLiteralValue* ValueForImageSlice(const Length& slice) {
  CHECK(slice.IsPercent() || slice.IsFixed());
  return CSSNumericLiteralValue::Create(
      slice.Value(), slice.IsPercent()
                         ? CSSPrimitiveValue::UnitType::kPercentage
                         : CSSPrimitiveValue::UnitType::kNumber);
}

cssvalue::CSSBorderImageSliceValue*
ComputedStyleUtils::ValueForNinePieceImageSlice(const NinePieceImage& image) {
  const LengthBox& slices = image.ImageSlices();

  // Create the slices.
  CSSPrimitiveValue* top = ValueForImageSlice(slices.Top());

  CSSPrimitiveValue* right = nullptr;
  CSSPrimitiveValue* bottom = nullptr;
  CSSPrimitiveValue* left = nullptr;
  if (slices.Right() == slices.Top() && slices.Bottom() == slices.Top() &&
      slices.Left() == slices.Top()) {
    right = top;
    bottom = top;
    left = top;
  } else {
    right = ValueForImageSlice(slices.Right());

    if (slices.Bottom() == slices.Top() && slices.Right() == slices.Left()) {
      bottom = top;
      left = right;
    } else {
      bottom = ValueForImageSlice(slices.Bottom());

      if (slices.Left() == slices.Right()) {
        left = right;
      } else {
        left = ValueForImageSlice(slices.Left());
      }
    }
  }

  return MakeGarbageCollected<cssvalue::CSSBorderImageSliceValue>(
      MakeGarbageCollected<CSSQuadValue>(top, right, bottom, left,
                                         CSSQuadValue::kSerializeAsQuad),
      image.Fill());
}

CSSValue* ValueForBorderImageLength(
    const BorderImageLength& border_image_length,
    const ComputedStyle& style) {
  if (border_image_length.IsNumber()) {
    return CSSNumericLiteralValue::Create(border_image_length.Number(),
                                          CSSPrimitiveValue::UnitType::kNumber);
  }
  return CSSValue::Create(border_image_length.length(), style.EffectiveZoom());
}

CSSQuadValue* ComputedStyleUtils::ValueForNinePieceImageQuad(
    const BorderImageLengthBox& box,
    const ComputedStyle& style) {
  // Create the slices.
  CSSValue* top = nullptr;
  CSSValue* right = nullptr;
  CSSValue* bottom = nullptr;
  CSSValue* left = nullptr;

  top = ValueForBorderImageLength(box.Top(), style);

  if (box.Right() == box.Top() && box.Bottom() == box.Top() &&
      box.Left() == box.Top()) {
    right = top;
    bottom = top;
    left = top;
  } else {
    right = ValueForBorderImageLength(box.Right(), style);

    if (box.Bottom() == box.Top() && box.Right() == box.Left()) {
      bottom = top;
      left = right;
    } else {
      bottom = ValueForBorderImageLength(box.Bottom(), style);

      if (box.Left() == box.Right()) {
        left = right;
      } else {
        left = ValueForBorderImageLength(box.Left(), style);
      }
    }
  }
  return MakeGarbageCollected<CSSQuadValue>(top, right, bottom, left,
                                            CSSQuadValue::kSerializeAsQuad);
}

CSSValueID ValueForRepeatRule(int rule) {
  switch (rule) {
    case kRepeatImageRule:
      return CSSValueID::kRepeat;
    case kRoundImageRule:
      return CSSValueID::kRound;
    case kSpaceImageRule:
      return CSSValueID::kSpace;
    default:
      return CSSValueID::kStretch;
  }
}

CSSValue* ComputedStyleUtils::ValueForNinePieceImageRepeat(
    const NinePieceImage& image) {
  CSSIdentifierValue* horizontal_repeat = nullptr;
  CSSIdentifierValue* vertical_repeat = nullptr;

  horizontal_repeat =
      CSSIdentifierValue::Create(ValueForRepeatRule(image.HorizontalRule()));
  if (image.HorizontalRule() == image.VerticalRule()) {
    vertical_repeat = horizontal_repeat;
  } else {
    vertical_repeat =
        CSSIdentifierValue::Create(ValueForRepeatRule(image.VerticalRule()));
  }
  return MakeGarbageCollected<CSSValuePair>(horizontal_repeat, vertical_repeat,
                                            CSSValuePair::kDropIdenticalValues);
}

CSSValue* ComputedStyleUtils::ValueForNinePieceImage(
    const NinePieceImage& image,
    const ComputedStyle& style,
    bool allow_visited_style,
    CSSValuePhase value_phase) {
  if (!image.HasImage()) {
    return CSSIdentifierValue::Create(CSSValueID::kNone);
  }

  // Image first.
  CSSValue* image_value = nullptr;
  if (image.GetImage()) {
    image_value = image.GetImage()->ComputedCSSValue(style, allow_visited_style,
                                                     value_phase);
  }

  // Create the image slice.
  cssvalue::CSSBorderImageSliceValue* image_slices =
      ValueForNinePieceImageSlice(image);

  // Create the border area slices.
  CSSValue* border_slices =
      ValueForNinePieceImageQuad(image.BorderSlices(), style);

  // Create the border outset.
  CSSValue* outset = ValueForNinePieceImageQuad(image.Outset(), style);

  // Create the repeat rules.
  CSSValue* repeat = ValueForNinePieceImageRepeat(image);

  return CreateBorderImageValue(image_value, image_slices, border_slices,
                                outset, repeat);
}

CSSValue* ComputedStyleUtils::ValueForReflection(
    const StyleReflection* reflection,
    const ComputedStyle& style,
    bool allow_visited_style,
    CSSValuePhase value_phase) {
  if (!reflection) {
    return CSSIdentifierValue::Create(CSSValueID::kNone);
  }

  auto* direction = CSSIdentifierValue::Create(reflection->Direction());
  auto* offset = CSSPrimitiveValue::CreateFromLength(reflection->Offset(),
                                                     style.EffectiveZoom());
  return MakeGarbageCollected<cssvalue::CSSReflectValue>(
      direction, offset,
      ValueForNinePieceImage(reflection->Mask(), style, allow_visited_style,
                             value_phase));
}

CSSValue* ComputedStyleUtils::MinWidthOrMinHeightAuto(
    const ComputedStyle& style) {
  if (style.IsFlexOrGridOrCustomItem() && !style.IsEnsuredInDisplayNone()) {
    return CSSIdentifierValue::Create(CSSValueID::kAuto);
  }
  return ZoomAdjustedPixelValue(0, style);
}

CSSValue* ComputedStyleUtils::ValueForPositionOffset(
    const ComputedStyle& style,
    const CSSProperty& property,
    const LayoutObject* layout_object) {
  std::pair<const Length*, const Length*> positions;
  bool is_horizontal_property;
  switch (property.PropertyID()) {
    case CSSPropertyID::kLeft:
      positions = std::make_pair(&style.Left(), &style.Right());
      is_horizontal_property = true;
      break;
    case CSSPropertyID::kRight:
      positions = std::make_pair(&style.Right(), &style.Left());
      is_horizontal_property = true;
      break;
    case CSSPropertyID::kTop:
      positions = std::make_pair(&style.Top(), &style.Bottom());
      is_horizontal_property = false;
      break;
    case CSSPropertyID::kBottom:
      positions = std::make_pair(&style.Bottom(), &style.Top());
      is_horizontal_property = false;
      break;
    default:
      NOTREACHED_IN_MIGRATION();
      return nullptr;
  }
  DCHECK(positions.first && positions.second);

  const Length& offset = *positions.first;
  const Length& opposite = *positions.second;
  const auto* box = DynamicTo<LayoutBox>(layout_object);

  // In this case, the used value is the computed value, so we resolve directly.
  if (offset.IsFixed()) {
    return ZoomAdjustedPixelValueForLength(offset, style);
  }

  if (box && box->IsOutOfFlowPositioned()) {
    // LayoutBox::OutOfFlowInsetsForGetComputedStyle() are relative to the
    // container's writing direction. Convert it to physical.
    const PhysicalBoxStrut& insets =
        box->OutOfFlowInsetsForGetComputedStyle().ConvertToPhysical(
            box->ContainingBlock()->StyleRef().GetWritingDirection());
    LayoutUnit inset;
    switch (property.PropertyID()) {
      case CSSPropertyID::kLeft:
        inset = insets.left;
        break;
      case CSSPropertyID::kTop:
        inset = insets.top;
        break;
      case CSSPropertyID::kRight:
        inset = insets.right;
        break;
      case CSSPropertyID::kBottom:
        inset = insets.bottom;
        break;
      default:
        NOTREACHED_IN_MIGRATION();
    }
    return ZoomAdjustedPixelValue(inset, style);
  }

  if ((offset.IsPercent() || offset.IsCalculated()) && box &&
      box->IsPositioned()) {
    LayoutUnit containing_block_size;
    if (box->IsStickyPositioned()) {
      const LayoutBox* scroll_container = box->ContainingScrollContainer();
      DCHECK(scroll_container);
      bool use_inline_size =
          is_horizontal_property == scroll_container->IsHorizontalWritingMode();
      containing_block_size = use_inline_size
                                  ? scroll_container->ContentLogicalWidth()
                                  : scroll_container->ContentLogicalHeight();
      UseCounter::Count(box->GetDocument(),
                        WebFeature::kPercentOrCalcStickyUsedOffset);
    } else {
      DCHECK(box->IsRelPositioned());
      containing_block_size =
          is_horizontal_property ==
                  box->ContainingBlock()->IsHorizontalWritingMode()
              ? box->ContainingBlockLogicalWidthForContent()
              : box->ContainingBlockLogicalHeightForRelPositioned();
      UseCounter::Count(box->GetDocument(),
                        WebFeature::kPercentOrCalcRelativeUsedOffset);
    }

    return ZoomAdjustedPixelValue(ValueForLength(offset, containing_block_size),
                                  style);
  }

  if (offset.IsAuto() && layout_object && layout_object->IsRelPositioned()) {
    UseCounter::Count(layout_object->GetDocument(),
                      WebFeature::kAutoRelativeUsedOffset);
    // If e.g. left is auto and right is not auto, then left's computed value
    // is negative right. So we get the opposite length unit and see if it is
    // auto.
    if (opposite.IsAuto()) {
      return CSSNumericLiteralValue::Create(
          0, CSSPrimitiveValue::UnitType::kPixels);
    }

    if (opposite.IsPercent() || opposite.IsCalculated()) {
      if (box) {
        LayoutUnit containing_block_size =
            is_horizontal_property ==
                    layout_object->ContainingBlock()->IsHorizontalWritingMode()
                ? box->ContainingBlockLogicalWidthForContent()
                : box->ContainingBlockLogicalHeightForRelPositioned();
        return ZoomAdjustedPixelValue(
            -FloatValueForLength(opposite, containing_block_size), style);
      }
      // FIXME:  fall back to auto for position:relative, display:inline
      return CSSIdentifierValue::Create(CSSValueID::kAuto);
    }

    Length negated_opposite = Negate(opposite);
    return ZoomAdjustedPixelValueForLength(negated_opposite, style);
  }

  if (offset.IsAuto()) {
    return CSSIdentifierValue::Create(CSSValueID::kAuto);
  }

  // Fixed lengths must have been handled by previous branches.
  CHECK(!offset.IsFixed());
  return ZoomAdjustedPixelValueForLength(offset, style);
}

CSSValue* ComputedStyleUtils::ValueForItemPositionWithOverflowAlignment(
    const StyleSelfAlignmentData& data) {
  if (data.PositionType() == ItemPositionType::kLegacy) {
    // Legacy is only for justify-items and may only be created with the
    // positions "left", "right", or "center". See
    // JustifyItems::ParseSingleValue.
    DCHECK(data.GetPosition() == ItemPosition::kLeft ||
           data.GetPosition() == ItemPosition::kRight ||
           data.GetPosition() == ItemPosition::kCenter)
        << "Unexpected position: " << (unsigned)data.GetPosition();
    DCHECK_EQ(data.Overflow(), OverflowAlignment::kDefault);
    return MakeGarbageCollected<CSSValuePair>(
        CSSIdentifierValue::Create(CSSValueID::kLegacy),
        CSSIdentifierValue::Create(data.GetPosition()),
        CSSValuePair::kDropIdenticalValues);
  }

  if (data.GetPosition() == ItemPosition::kBaseline) {
    return CSSIdentifierValue::Create(CSSValueID::kBaseline);
  } else if (data.GetPosition() == ItemPosition::kLastBaseline) {
    return MakeGarbageCollected<CSSValuePair>(
        CSSIdentifierValue::Create(CSSValueID::kLast),
        CSSIdentifierValue::Create(CSSValueID::kBaseline),
        CSSValuePair::kDropIdenticalValues);
  } else {
    auto* position = data.GetPosition() == ItemPosition::kLegacy
                         ? CSSIdentifierValue::Create(CSSValueID::kNormal)
                         : CSSIdentifierValue::Create(data.GetPosition());
    if (data.GetPosition() >= ItemPosition::kCenter &&
        data.Overflow() != OverflowAlignment::kDefault) {
      return MakeGarbageCollected<CSSValuePair>(
          CSSIdentifierValue::Create(data.Overflow()), position,
          CSSValuePair::kDropIdenticalValues);
    }
    return position;
  }
}

cssvalue::CSSContentDistributionValue*
ComputedStyleUtils::ValueForContentPositionAndDistributionWithOverflowAlignment(
    const StyleContentAlignmentData& data) {
  CSSValueID distribution = CSSValueID::kInvalid;
  CSSValueID position = CSSValueID::kInvalid;
  CSSValueID overflow = CSSValueID::kInvalid;

  // Handle content-distribution values
  if (data.Distribution() != ContentDistributionType::kDefault) {
    distribution = CSSIdentifierValue(data.Distribution()).GetValueID();
  }

  // Handle content-position values (either as fallback or actual value)
  switch (data.GetPosition()) {
    case ContentPosition::kNormal:
      // Handle 'normal' value, not valid as content-distribution fallback.
      if (data.Distribution() == ContentDistributionType::kDefault) {
        position = CSSValueID::kNormal;
      }
      break;
    case ContentPosition::kLastBaseline:
      position = CSSValueID::kLastBaseline;
      break;
    default:
      // Handle overflow-alignment (only allowed for content-position values)
      if ((data.GetPosition() >= ContentPosition::kCenter ||
           data.Distribution() != ContentDistributionType::kDefault) &&
          data.Overflow() != OverflowAlignment::kDefault) {
        overflow = CSSIdentifierValue::Create(data.Overflow())->GetValueID();
      }
      position = CSSIdentifierValue::Create(data.GetPosition())->GetValueID();
  }

  return MakeGarbageCollected<cssvalue::CSSContentDistributionValue>(
      distribution, position, overflow);
}

CSSValue* ComputedStyleUtils::ValueForLineHeight(const ComputedStyle& style) {
  const Length& length = style.LineHeight();
  if (length.IsAuto()) {
    return CSSIdentifierValue::Create(CSSValueID::kNormal);
  }

  return ZoomAdjustedPixelValue(
      FloatValueForLength(length, style.GetFontDescription().ComputedSize()),
      style);
}

CSSValue* ComputedStyleUtils::ComputedValueForLineHeight(
    const ComputedStyle& style) {
  const Length& length = style.LineHeight();
  if (length.IsAuto()) {
    return CSSIdentifierValue::Create(CSSValueID::kNormal);
  }

  if (length.IsPercent()) {
    return CSSNumericLiteralValue::Create(length.GetFloatValue() / 100.0,
                                          CSSPrimitiveValue::UnitType::kNumber);
  } else {
    return ZoomAdjustedPixelValue(
        FloatValueForLength(length, style.GetFontDescription().ComputedSize()),
        style);
  }
}

CSSValueID IdentifierForFamily(const AtomicString& family) {
  if (family == font_family_names::kCursive) {
    return CSSValueID::kCursive;
  }
  if (family == font_family_names::kFantasy) {
    return CSSValueID::kFantasy;
  }
  if (family == font_family_names::kMonospace) {
    return CSSValueID::kMonospace;
  }
  if (family == font_family_names::kSansSerif) {
    return CSSValueID::kSansSerif;
  }
  if (family == font_family_names::kSerif) {
    return CSSValueID::kSerif;
  }
  if (family == font_family_names::kSystemUi) {
    return CSSValueID::kSystemUi;
  }
  if (family == font_family_names::kMath) {
    return CSSValueID::kMath;
  }
  // If family does not correspond to any of the above, then it was actually
  // converted from -webkit-body by FontBuilder, so put this value back.
  // TODO(crbug.com/1065468): This trick does not work if
  // FontBuilder::StandardFontFamilyName() actually returned one of the generic
  // family above.
  return CSSValueID::kWebkitBody;
}

CSSValue* ValueForFamily(const FontFamily& family) {
  if (family.FamilyIsGeneric()) {
    return CSSIdentifierValue::Create(IdentifierForFamily(family.FamilyName()));
  }
  return CSSFontFamilyValue::Create(family.FamilyName());
}

CSSValueList* ComputedStyleUtils::ValueForFontFamily(
    const FontFamily& font_family) {
  CSSValueList* list = CSSValueList::CreateCommaSeparated();
  for (const FontFamily* family = &font_family; family;
       family = family->Next()) {
    list->Append(*ValueForFamily(*family));
  }
  return list;
}

CSSValueList* ComputedStyleUtils::ValueForFontFamily(
    const ComputedStyle& style) {
  return ComputedStyleUtils::ValueForFontFamily(
      style.GetFontDescription().Family());
}

CSSPrimitiveValue* ComputedStyleUtils::ValueForFontSize(
    const ComputedStyle& style) {
  return ZoomAdjustedPixelValue(style.GetFontDescription().ComputedSize(),
                                style);
}

CSSValue* ComputedStyleUtils::ValueForFontSizeAdjust(
    const ComputedStyle& style) {
  if (!style.HasFontSizeAdjust() ||
      style.FontSizeAdjust().Value() == FontSizeAdjust::kFontSizeAdjustNone) {
    return CSSIdentifierValue::Create(CSSValueID::kNone);
  }

  // A resolved value is to be returned. Compare CSS WG discussion.
  // https://github.com/w3c/csswg-drafts/issues/9050
  FontSizeAdjust font_size_adjust = style.FontSizeAdjust();
  if (font_size_adjust.GetMetric() == FontSizeAdjust::Metric::kExHeight) {
    return CSSNumericLiteralValue::Create(style.FontSizeAdjust().Value(),
                                          CSSPrimitiveValue::UnitType::kNumber);
  }

  return MakeGarbageCollected<CSSValuePair>(
      CSSIdentifierValue::Create(font_size_adjust.GetMetric()),
      CSSNumericLiteralValue::Create(style.FontSizeAdjust().Value(),
                                     CSSPrimitiveValue::UnitType::kNumber),
      CSSValuePair::kKeepIdenticalValues);
}

CSSPrimitiveValue* ComputedStyleUtils::ValueForFontStretch(
    const ComputedStyle& style) {
  return CSSNumericLiteralValue::Create(
      style.GetFontDescription().Stretch(),
      CSSPrimitiveValue::UnitType::kPercentage);
}

CSSValue* ComputedStyleUtils::ValueForFontStyle(const ComputedStyle& style) {
  FontSelectionValue angle = style.GetFontDescription().Style();
  if (angle == kNormalSlopeValue) {
    return CSSIdentifierValue::Create(CSSValueID::kNormal);
  }

  if (angle == kItalicSlopeValue) {
    return CSSIdentifierValue::Create(CSSValueID::kItalic);
  }

  // The spec says: 'The lack of a number represents an angle of
  // "20deg"', but since we compute that to 'italic' (handled above),
  // we don't perform any special treatment of that value here.
  CSSValueList* oblique_values = CSSValueList::CreateSpaceSeparated();
  oblique_values->Append(*CSSNumericLiteralValue::Create(
      angle, CSSPrimitiveValue::UnitType::kDegrees));
  return MakeGarbageCollected<cssvalue::CSSFontStyleRangeValue>(
      *CSSIdentifierValue::Create(CSSValueID::kOblique), *oblique_values);
}

CSSNumericLiteralValue* ComputedStyleUtils::ValueForFontWeight(
    const ComputedStyle& style) {
  return CSSNumericLiteralValue::Create(style.GetFontDescription().Weight(),
                                        CSSPrimitiveValue::UnitType::kNumber);
}

CSSIdentifierValue* ComputedStyleUtils::ValueForFontVariantCaps(
    const ComputedStyle& style) {
  FontDescription::FontVariantCaps variant_caps =
      style.GetFontDescription().VariantCaps();
  switch (variant_caps) {
    case FontDescription::kCapsNormal:
      return CSSIdentifierValue::Create(CSSValueID::kNormal);
    case FontDescription::kSmallCaps:
      return CSSIdentifierValue::Create(CSSValueID::kSmallCaps);
    case FontDescription::kAllSmallCaps:
      return CSSIdentifierValue::Create(CSSValueID::kAllSmallCaps);
    case FontDescription::kPetiteCaps:
      return CSSIdentifierValue::Create(CSSValueID::kPetiteCaps);
    case FontDescription::kAllPetiteCaps:
      return CSSIdentifierValue::Create(CSSValueID::kAllPetiteCaps);
    case FontDescription::kUnicase:
      return CSSIdentifierValue::Create(CSSValueID::kUnicase);
    case FontDescription::kTitlingCaps:
      return CSSIdentifierValue::Create(CSSValueID::kTitlingCaps);
    default:
      NOTREACHED_IN_MIGRATION();
      return nullptr;
  }
}

CSSValue* ComputedStyleUtils::ValueForFontVariantLigatures(
    const ComputedStyle& style) {
  FontDescription::LigaturesState common_ligatures_state =
      style.GetFontDescription().CommonLigaturesState();
  FontDescription::LigaturesState discretionary_ligatures_state =
      style.GetFontDescription().DiscretionaryLigaturesState();
  FontDescription::LigaturesState historical_ligatures_state =
      style.GetFontDescription().HistoricalLigaturesState();
  FontDescription::LigaturesState contextual_ligatures_state =
      style.GetFontDescription().ContextualLigaturesState();
  if (common_ligatures_state == FontDescription::kNormalLigaturesState &&
      discretionary_ligatures_state == FontDescription::kNormalLigaturesState &&
      historical_ligatures_state == FontDescription::kNormalLigaturesState &&
      contextual_ligatures_state == FontDescription::kNormalLigaturesState) {
    return CSSIdentifierValue::Create(CSSValueID::kNormal);
  }

  if (common_ligatures_state == FontDescription::kDisabledLigaturesState &&
      discretionary_ligatures_state ==
          FontDescription::kDisabledLigaturesState &&
      historical_ligatures_state == FontDescription::kDisabledLigaturesState &&
      contextual_ligatures_state == FontDescription::kDisabledLigaturesState) {
    return CSSIdentifierValue::Create(CSSValueID::kNone);
  }

  CSSValueList* value_list = CSSValueList::CreateSpaceSeparated();
  if (common_ligatures_state != FontDescription::kNormalLigaturesState) {
    value_list->Append(*CSSIdentifierValue::Create(
        common_ligatures_state == FontDescription::kDisabledLigaturesState
            ? CSSValueID::kNoCommonLigatures
            : CSSValueID::kCommonLigatures));
  }
  if (discretionary_ligatures_state != FontDescription::kNormalLigaturesState) {
    value_list->Append(*CSSIdentifierValue::Create(
        discretionary_ligatures_state ==
                FontDescription::kDisabledLigaturesState
            ? CSSValueID::kNoDiscretionaryLigatures
            : CSSValueID::kDiscretionaryLigatures));
  }
  if (historical_ligatures_state != FontDescription::kNormalLigaturesState) {
    value_list->Append(*CSSIdentifierValue::Create(
        historical_ligatures_state == FontDescription::kDisabledLigaturesState
            ? CSSValueID::kNoHistoricalLigatures
            : CSSValueID::kHistoricalLigatures));
  }
  if (contextual_ligatures_state != FontDescription::kNormalLigaturesState) {
    value_list->Append(*CSSIdentifierValue::Create(
        contextual_ligatures_state == FontDescription::kDisabledLigaturesState
            ? CSSValueID::kNoContextual
            : CSSValueID::kContextual));
  }
  return value_list;
}

CSSValue* ComputedStyleUtils::ValueForFontVariantNumeric(
    const ComputedStyle& style) {
  FontVariantNumeric variant_numeric =
      style.GetFontDescription().VariantNumeric();
  if (variant_numeric.IsAllNormal()) {
    return CSSIdentifierValue::Create(CSSValueID::kNormal);
  }

  CSSValueList* value_list = CSSValueList::CreateSpaceSeparated();
  if (variant_numeric.NumericFigureValue() !=
      FontVariantNumeric::kNormalFigure) {
    value_list->Append(*CSSIdentifierValue::Create(
        variant_numeric.NumericFigureValue() == FontVariantNumeric::kLiningNums
            ? CSSValueID::kLiningNums
            : CSSValueID::kOldstyleNums));
  }
  if (variant_numeric.NumericSpacingValue() !=
      FontVariantNumeric::kNormalSpacing) {
    value_list->Append(*CSSIdentifierValue::Create(
        variant_numeric.NumericSpacingValue() ==
                FontVariantNumeric::kProportionalNums
            ? CSSValueID::kProportionalNums
            : CSSValueID::kTabularNums));
  }
  if (variant_numeric.NumericFractionValue() !=
      FontVariantNumeric::kNormalFraction) {
    value_list->Append(*CSSIdentifierValue::Create(
        variant_numeric.NumericFractionValue() ==
                FontVariantNumeric::kDiagonalFractions
            ? CSSValueID::kDiagonalFractions
            : CSSValueID::kStackedFractions));
  }
  if (variant_numeric.OrdinalValue() == FontVariantNumeric::kOrdinalOn) {
    value_list->Append(*CSSIdentifierValue::Create(CSSValueID::kOrdinal));
  }
  if (variant_numeric.SlashedZeroValue() ==
      FontVariantNumeric::kSlashedZeroOn) {
    value_list->Append(*CSSIdentifierValue::Create(CSSValueID::kSlashedZero));
  }

  return value_list;
}

CSSValue* ComputedStyleUtils::ValueForFontVariantAlternates(
    const ComputedStyle& style) {
  const FontVariantAlternates* variant_alternates =
      style.GetFontDescription().GetFontVariantAlternates();
  if (!variant_alternates || variant_alternates->IsNormal()) {
    return CSSIdentifierValue::Create(CSSValueID::kNormal);
  }

  auto make_single_ident_list = [](const AtomicString& alias) {
    CSSValueList* aliases_list = CSSValueList::CreateCommaSeparated();
    aliases_list->Append(*MakeGarbageCollected<CSSCustomIdentValue>(alias));
    return aliases_list;
  };

  CSSValueList* value_list = CSSValueList::CreateSpaceSeparated();
  if (const AtomicString* opt_stylistic = variant_alternates->Stylistic()) {
    value_list->Append(*MakeGarbageCollected<cssvalue::CSSAlternateValue>(
        *MakeGarbageCollected<CSSFunctionValue>(CSSValueID::kStylistic),
        *make_single_ident_list(*opt_stylistic)));
  }
  if (variant_alternates->HistoricalForms()) {
    value_list->Append(
        *CSSIdentifierValue::Create(CSSValueID::kHistoricalForms));
  }
  if (const AtomicString* opt_swash = variant_alternates->Swash()) {
    value_list->Append(*MakeGarbageCollected<cssvalue::CSSAlternateValue>(
        *MakeGarbageCollected<CSSFunctionValue>(CSSValueID::kSwash),
        *make_single_ident_list(*opt_swash)));
  }
  if (const AtomicString* opt_ornaments = variant_alternates->Ornaments()) {
    value_list->Append(*MakeGarbageCollected<cssvalue::CSSAlternateValue>(
        *MakeGarbageCollected<CSSFunctionValue>(CSSValueID::kOrnaments),
        *make_single_ident_list(*opt_ornaments)));
  }
  if (const AtomicString* opt_annotation = variant_alternates->Annotation()) {
    value_list->Append(*MakeGarbageCollected<cssvalue::CSSAlternateValue>(
        *MakeGarbageCollected<CSSFunctionValue>(CSSValueID::kAnnotation),
        *make_single_ident_list(*opt_annotation)));
  }

  if (!variant_alternates->Styleset().empty()) {
    CSSValueList* aliases_list = CSSValueList::CreateCommaSeparated();
    for (auto alias : variant_alternates->Styleset()) {
      aliases_list->Append(*MakeGarbageCollected<CSSCustomIdentValue>(alias));
    }
    value_list->Append(*MakeGarbageCollected<cssvalue::CSSAlternateValue>(
        *MakeGarbageCollected<CSSFunctionValue>(CSSValueID::kStyleset),
        *aliases_list));
  }
  if (!variant_alternates->CharacterVariant().empty()) {
    CSSValueList* aliases_list = CSSValueList::CreateCommaSeparated();
    for (auto alias : variant_alternates->CharacterVariant()) {
      aliases_list->Append(*MakeGarbageCollected<CSSCustomIdentValue>(alias));
    }
    value_list->Append(*MakeGarbageCollected<cssvalue::CSSAlternateValue>(
        *MakeGarbageCollected<CSSFunctionValue>(CSSValueID::kCharacterVariant),
        *aliases_list));
  }

  DCHECK(value_list->length());
  return value_list;
}

CSSIdentifierValue* ComputedStyleUtils::ValueForFontVariantPosition(
    const ComputedStyle& style) {
  FontDescription::FontVariantPosition variant_position =
      style.GetFontDescription().VariantPosition();
  switch (variant_position) {
    case FontDescription::kNormalVariantPosition:
      return CSSIdentifierValue::Create(CSSValueID::kNormal);
    case FontDescription::kSubVariantPosition:
      return CSSIdentifierValue::Create(CSSValueID::kSub);
    case FontDescription::kSuperVariantPosition:
      return CSSIdentifierValue::Create(CSSValueID::kSuper);
    default:
      NOTREACHED_IN_MIGRATION();
      return CSSIdentifierValue::Create(CSSValueID::kNormal);
  }
}

CSSIdentifierValue* ComputedStyleUtils::ValueForFontKerning(
    const ComputedStyle& style) {
  FontDescription::Kerning kerning = style.GetFontDescription().GetKerning();
  switch (kerning) {
    case FontDescription::kAutoKerning:
      return CSSIdentifierValue::Create(CSSValueID::kAuto);
    case FontDescription::kNormalKerning:
      return CSSIdentifierValue::Create(CSSValueID::kNormal);
    case FontDescription::kNoneKerning:
      return CSSIdentifierValue::Create(CSSValueID::kNone);
    default:
      NOTREACHED_IN_MIGRATION();
      return CSSIdentifierValue::Create(CSSValueID::kAuto);
  }
}

CSSIdentifierValue* ComputedStyleUtils::ValueForFontOpticalSizing(
    const ComputedStyle& style) {
  OpticalSizing optical_sizing = style.GetFontDescription().FontOpticalSizing();
  switch (optical_sizing) {
    case kAutoOpticalSizing:
      return CSSIdentifierValue::Create(CSSValueID::kAuto);
    case kNoneOpticalSizing:
      return CSSIdentifierValue::Create(CSSValueID::kNone);
    default:
      return CSSIdentifierValue::Create(CSSValueID::kAuto);
  }
}

CSSIdentifierValue* ValueForFontStretchAsKeyword(const ComputedStyle& style) {
  FontSelectionValue stretch_value = style.GetFontDescription().Stretch();
  CSSValueID value_id = CSSValueID::kInvalid;
  if (stretch_value == kUltraCondensedWidthValue) {
    value_id = CSSValueID::kUltraCondensed;
  }
  if (stretch_value == kUltraCondensedWidthValue) {
    value_id = CSSValueID::kUltraCondensed;
  }
  if (stretch_value == kExtraCondensedWidthValue) {
    value_id = CSSValueID::kExtraCondensed;
  }
  if (stretch_value == kCondensedWidthValue) {
    value_id = CSSValueID::kCondensed;
  }
  if (stretch_value == kSemiCondensedWidthValue) {
    value_id = CSSValueID::kSemiCondensed;
  }
  if (stretch_value == kNormalWidthValue) {
    value_id = CSSValueID::kNormal;
  }
  if (stretch_value == kSemiExpandedWidthValue) {
    value_id = CSSValueID::kSemiExpanded;
  }
  if (stretch_value == kExpandedWidthValue) {
    value_id = CSSValueID::kExpanded;
  }
  if (stretch_value == kExtraExpandedWidthValue) {
    value_id = CSSValueID::kExtraExpanded;
  }
  if (stretch_value == kUltraExpandedWidthValue) {
    value_id = CSSValueID::kUltraExpanded;
  }

  if (IsValidCSSValueID(value_id)) {
    return CSSIdentifierValue::Create(value_id);
  }
  return nullptr;
}

CSSValue* ComputedStyleUtils::ValueForFontVariantEastAsian(
    const ComputedStyle& style) {
  FontVariantEastAsian east_asian =
      style.GetFontDescription().VariantEastAsian();
  if (east_asian.IsAllNormal()) {
    return CSSIdentifierValue::Create(CSSValueID::kNormal);
  }

  CSSValueList* value_list = CSSValueList::CreateSpaceSeparated();
  switch (east_asian.Form()) {
    case FontVariantEastAsian::kNormalForm:
      break;
    case FontVariantEastAsian::kJis78:
      value_list->Append(*CSSIdentifierValue::Create(CSSValueID::kJis78));
      break;
    case FontVariantEastAsian::kJis83:
      value_list->Append(*CSSIdentifierValue::Create(CSSValueID::kJis83));
      break;
    case FontVariantEastAsian::kJis90:
      value_list->Append(*CSSIdentifierValue::Create(CSSValueID::kJis90));
      break;
    case FontVariantEastAsian::kJis04:
      value_list->Append(*CSSIdentifierValue::Create(CSSValueID::kJis04));
      break;
    case FontVariantEastAsian::kSimplified:
      value_list->Append(*CSSIdentifierValue::Create(CSSValueID::kSimplified));
      break;
    case FontVariantEastAsian::kTraditional:
      value_list->Append(*CSSIdentifierValue::Create(CSSValueID::kTraditional));
      break;
    default:
      NOTREACHED_IN_MIGRATION();
  }
  switch (east_asian.Width()) {
    case FontVariantEastAsian::kNormalWidth:
      break;
    case FontVariantEastAsian::kFullWidth:
      value_list->Append(*CSSIdentifierValue::Create(CSSValueID::kFullWidth));
      break;
    case FontVariantEastAsian::kProportionalWidth:
      value_list->Append(
          *CSSIdentifierValue::Create(CSSValueID::kProportionalWidth));
      break;
    default:
      NOTREACHED_IN_MIGRATION();
  }
  if (east_asian.Ruby()) {
    value_list->Append(*CSSIdentifierValue::Create(CSSValueID::kRuby));
  }
  return value_list;
}

CSSValue* ComputedStyleUtils::ValueForFontFeatureSettings(
    const ComputedStyle& style) {
  const blink::FontFeatureSettings* feature_settings =
      style.GetFontDescription().FeatureSettings();
  if (!feature_settings || !feature_settings->size()) {
    return CSSIdentifierValue::Create(CSSValueID::kNormal);
  }
  CSSValueList* list = CSSValueList::CreateCommaSeparated();
  for (wtf_size_t i = 0; i < feature_settings->size(); ++i) {
    const FontFeature& feature = feature_settings->at(i);
    auto* feature_value = MakeGarbageCollected<cssvalue::CSSFontFeatureValue>(
        feature.TagString(), feature.Value());
    list->Append(*feature_value);
  }
  return list;
}

CSSValue* ComputedStyleUtils::ValueForFontVariationSettings(
    const ComputedStyle& style) {
  const blink::FontVariationSettings* variation_settings =
      style.GetFontDescription().VariationSettings();
  if (!variation_settings || !variation_settings->size()) {
    return CSSIdentifierValue::Create(CSSValueID::kNormal);
  }
  CSSValueList* list = CSSValueList::CreateCommaSeparated();
  for (wtf_size_t i = 0; i < variation_settings->size(); ++i) {
    const FontVariationAxis& variation_axis = variation_settings->at(i);
    cssvalue::CSSFontVariationValue* variation_value =
        MakeGarbageCollected<cssvalue::CSSFontVariationValue>(
            variation_axis.TagString(), variation_axis.Value());
    list->Append(*variation_value);
  }
  return list;
}

CSSValue* ComputedStyleUtils::ValueForFontPalette(const ComputedStyle& style) {
  const blink::FontPalette* palette =
      style.GetFontDescription().GetFontPalette();

  if (!palette) {
    return CSSIdentifierValue::Create(CSSValueID::kNormal);
  }

  return ConvertFontPaletteToCSSValue(palette);
}

CSSValue* ComputedStyleUtils::ValueForFont(const ComputedStyle& style) {
  auto AppendIfNotNormal = [](CSSValueList* list, const CSSValue& value) {
    auto* identifier_value = DynamicTo<CSSIdentifierValue>(value);
    if (identifier_value &&
        identifier_value->GetValueID() == CSSValueID::kNormal) {
      return;
    }

    list->Append(value);
  };

  CSSValueList* list = CSSValueList::CreateSpaceSeparated();

  AppendIfNotNormal(list, *ValueForFontStyle(style));

  // Check that non-initial font-variant subproperties are not conflicting with
  // this serialization.
  CSSValue* ligatures_value = ValueForFontVariantLigatures(style);
  CSSValue* numeric_value = ValueForFontVariantNumeric(style);
  CSSValue* east_asian_value = ValueForFontVariantEastAsian(style);
  CSSValue* feature_settings = ValueForFontFeatureSettings(style);
  CSSValue* variation_settings = ValueForFontVariationSettings(style);
  CSSValue* variant_alternative = ValueForFontVariantAlternates(style);
  // FIXME: Use DataEquivalent<CSSValue>(...) once http://crbug.com/729447 is
  // resolved.
  if (!base::ValuesEquivalent(ligatures_value,
                              static_cast<CSSValue*>(CSSIdentifierValue::Create(
                                  CSSValueID::kNormal))) ||
      !base::ValuesEquivalent(numeric_value,
                              static_cast<CSSValue*>(CSSIdentifierValue::Create(
                                  CSSValueID::kNormal))) ||
      !base::ValuesEquivalent(east_asian_value,
                              static_cast<CSSValue*>(CSSIdentifierValue::Create(
                                  CSSValueID::kNormal))) ||
      !base::ValuesEquivalent(feature_settings,
                              static_cast<CSSValue*>(CSSIdentifierValue::Create(
                                  CSSValueID::kNormal))) ||
      !base::ValuesEquivalent(variation_settings,
                              static_cast<CSSValue*>(CSSIdentifierValue::Create(
                                  CSSValueID::kNormal))) ||
      !base::ValuesEquivalent(variant_alternative,
                              static_cast<CSSValue*>(CSSIdentifierValue::Create(
                                  CSSValueID::kNormal)))) {
    return nullptr;
  }

  FontDescription::Kerning kerning = style.GetFontDescription().GetKerning();
  FontDescription::FontVariantPosition variant_position =
      style.GetFontDescription().VariantPosition();
  FontVariantEmoji variant_emoji = style.GetFontDescription().VariantEmoji();
  OpticalSizing optical_sizing = style.GetFontDescription().FontOpticalSizing();

  if (kerning != FontDescription::kAutoKerning ||
      optical_sizing != kAutoOpticalSizing ||
      (RuntimeEnabledFeatures::CSSFontSizeAdjustEnabled() &&
       style.GetFontDescription().HasSizeAdjust()) ||
      variant_position != FontDescription::kNormalVariantPosition ||
      (RuntimeEnabledFeatures::FontVariantEmojiEnabled() &&
       variant_emoji != kNormalVariantEmoji)) {
    return nullptr;
  }

  if (!ValueForFontStretchAsKeyword(style)) {
    return nullptr;
  }

  CSSIdentifierValue* caps_value = ValueForFontVariantCaps(style);
  if (caps_value->GetValueID() != CSSValueID::kNormal &&
      caps_value->GetValueID() != CSSValueID::kSmallCaps) {
    return nullptr;
  }
  AppendIfNotNormal(list, *caps_value);

  {
    CSSNumericLiteralValue* font_weight = ValueForFontWeight(style);
    if (font_weight->DoubleValue() != kNormalWeightValue) {
      list->Append(*font_weight);
    }
  }

  AppendIfNotNormal(list, *ValueForFontStretchAsKeyword(style));

  {
    CSSValue* line_height = ValueForLineHeight(style);
    auto* identifier_line_height = DynamicTo<CSSIdentifierValue>(line_height);
    if (identifier_line_height &&
        identifier_line_height->GetValueID() == CSSValueID::kNormal) {
      list->Append(*ValueForFontSize(style));
    } else {
      // Add a slash between size and line-height.
      CSSValueList* size_and_line_height = CSSValueList::CreateSlashSeparated();
      size_and_line_height->Append(*ValueForFontSize(style));
      size_and_line_height->Append(*line_height);

      list->Append(*size_and_line_height);
    }
  }

  list->Append(*ValueForFontFamily(style));

  return list;
}

CSSValue* ComputedStyleUtils::SpecifiedValueForGridTrackSize(
    const GridTrackSize& track_size,
    const ComputedStyle& style) {
  switch (track_size.GetType()) {
    case kLengthTrackSizing:
      return ComputedStyleUtils::ZoomAdjustedPixelValueForLength(
          track_size.MinTrackBreadth(), style);
    case kMinMaxTrackSizing: {
      if (track_size.MinTrackBreadth().IsAuto() &&
          track_size.MaxTrackBreadth().IsFlex()) {
        return ComputedStyleUtils::ZoomAdjustedPixelValueForLength(
            track_size.MaxTrackBreadth(), style);
      }

      auto* min_max_track_breadths =
          MakeGarbageCollected<CSSFunctionValue>(CSSValueID::kMinmax);
      min_max_track_breadths->Append(
          *ComputedStyleUtils::ZoomAdjustedPixelValueForLength(
              track_size.MinTrackBreadth(), style));
      min_max_track_breadths->Append(
          *ComputedStyleUtils::ZoomAdjustedPixelValueForLength(
              track_size.MaxTrackBreadth(), style));
      return min_max_track_breadths;
    }
    case kFitContentTrackSizing: {
      auto* fit_content_track_breadth =
          MakeGarbageCollected<CSSFunctionValue>(CSSValueID::kFitContent);
      fit_content_track_breadth->Append(
          *ComputedStyleUtils::ZoomAdjustedPixelValueForLength(
              track_size.FitContentTrackBreadth(), style));
      return fit_content_track_breadth;
    }
  }
  NOTREACHED_IN_MIGRATION();
  return nullptr;
}

enum class NamedLinesType { kNamedLines, kAutoRepeatNamedLines };
class OrderedNamedLinesCollector {
  STACK_ALLOCATED();

 public:
  OrderedNamedLinesCollector(
      const OrderedNamedGridLines& ordered_named_grid_lines,
      const OrderedNamedGridLines& ordered_named_auto_repeat_grid_lines,
      bool is_subgridded_track,
      bool is_layout_grid)
      : ordered_named_grid_lines_(ordered_named_grid_lines),
        ordered_named_auto_repeat_grid_lines_(
            ordered_named_auto_repeat_grid_lines),
        is_subgridded_axis_(is_subgridded_track),
        is_layout_grid_(is_layout_grid) {}
  OrderedNamedLinesCollector(const OrderedNamedLinesCollector&) = delete;
  OrderedNamedLinesCollector& operator=(const OrderedNamedLinesCollector&) =
      delete;
  virtual ~OrderedNamedLinesCollector() = default;

  bool IsEmpty() const {
    return ordered_named_grid_lines_.empty() &&
           ordered_named_auto_repeat_grid_lines_.empty();
  }
  bool IsSubgriddedAxis() const { return is_subgridded_axis_; }
  wtf_size_t InsertionPoint() const { return insertion_point_; }
  bool HasAutoRepeatNamedLinesSpecified() const {
    return AutoRepeatNamedLinesCount() > 0;
  }
  wtf_size_t AutoRepeatNamedLinesCount() const {
    return ordered_named_auto_repeat_grid_lines_.size();
  }
  // A collapsed auto repeat track is a specified auto-repeat track that was
  // clamped to zero repeats. This can only happen for subgrids, as
  // standalone grids guarantee a minimum of 1 repeat. This also requires that
  // auto repetitions were computed, which only happens for layout grids.
  bool HasCollapsedAutoRepeatNamedLines() const {
    return is_layout_grid_ && IsSubgriddedAxis() &&
           HasAutoRepeatNamedLinesSpecified() && auto_repeat_total_tracks_ == 0;
  }
  virtual void CollectLineNamesForIndex(cssvalue::CSSBracketedValueList&,
                                        wtf_size_t index,
                                        NamedLinesType type,
                                        bool is_in_repeat) const;

 protected:
  void AppendLines(cssvalue::CSSBracketedValueList&,
                   wtf_size_t index,
                   NamedLinesType,
                   bool is_in_repeat = false) const;

  const OrderedNamedGridLines& ordered_named_grid_lines_;
  const OrderedNamedGridLines& ordered_named_auto_repeat_grid_lines_;

  // The auto-repeat index.
  wtf_size_t insertion_point_{0};

  // The total number of auto-repeat tracks, factoring in the number of
  // repetitions (e.g. `repeat(auto-fit, [a][b])` with `auto-fit` calculated at
  // 3 repetitions would be 6).
  wtf_size_t auto_repeat_total_tracks_{0};

  // The size of one auto repeat track (e.g. `repeat(auto-fit, [a][b])` has an
  // auto repeat track list length of 2, regardless of the number of repetitions
  // computed for `auto-fit`).
  wtf_size_t auto_repeat_track_list_length_{0};

  // Whether the track definition has `subgrid` specified.
  bool is_subgridded_axis_{false};

  // Whether the element associated with the track collection performs grid
  // layout.
  bool is_layout_grid_{false};
};

class OrderedNamedLinesCollectorInGridLayout
    : public OrderedNamedLinesCollector {
 public:
  OrderedNamedLinesCollectorInGridLayout(
      const OrderedNamedGridLines& ordered_named_grid_lines,
      const OrderedNamedGridLines& ordered_named_auto_repeat_grid_lines,
      wtf_size_t insertion_point,
      wtf_size_t auto_repeat_total_tracks,
      wtf_size_t auto_repeat_track_list_length,
      bool is_subgridded_track)
      : OrderedNamedLinesCollector(ordered_named_grid_lines,
                                   ordered_named_auto_repeat_grid_lines,
                                   is_subgridded_track,
                                   /* is_layout_grid */ true) {
    insertion_point_ = insertion_point;
    auto_repeat_total_tracks_ = auto_repeat_total_tracks;
    auto_repeat_track_list_length_ = auto_repeat_track_list_length;
  }
  void CollectLineNamesForIndex(cssvalue::CSSBracketedValueList&,
                                wtf_size_t index,
                                NamedLinesType type,
                                bool is_in_repeat) const override;
};

void OrderedNamedLinesCollector::AppendLines(
    cssvalue::CSSBracketedValueList& line_names_value,
    wtf_size_t index,
    NamedLinesType type,
    bool is_in_repeat) const {
  const bool is_auto = type == NamedLinesType::kAutoRepeatNamedLines;
  auto iter = is_auto ? ordered_named_auto_repeat_grid_lines_.find(index)
                      : ordered_named_grid_lines_.find(index);
  auto end_iter = is_auto ? ordered_named_auto_repeat_grid_lines_.end()
                          : ordered_named_grid_lines_.end();
  if (iter == end_iter) {
    return;
  }

  for (auto named_grid_line : iter->value) {
    // For layout grids, insert all values. For non-layout grids, in order to
    // round-trip repeaters, we need to prevent inserting certain line names.
    // In particular, don't insert lines from repeaters if we're not in a
    // repeater, and only add the first repeat.
    const bool is_not_in_repeat =
        !is_in_repeat && !named_grid_line.is_in_repeat;
    const bool is_valid_repeat_line =
        is_in_repeat && named_grid_line.is_first_repeat;
    if (is_layout_grid_ || is_not_in_repeat || is_valid_repeat_line) {
      line_names_value.Append(*MakeGarbageCollected<CSSCustomIdentValue>(
          AtomicString(named_grid_line.line_name)));
    }
  }
}

void OrderedNamedLinesCollector::CollectLineNamesForIndex(
    cssvalue::CSSBracketedValueList& line_names_value,
    wtf_size_t i,
    NamedLinesType type,
    bool is_in_repeat) const {
  DCHECK(IsSubgriddedAxis() || !IsEmpty());
  AppendLines(line_names_value, i, type, is_in_repeat);
}

void OrderedNamedLinesCollectorInGridLayout::CollectLineNamesForIndex(
    cssvalue::CSSBracketedValueList& line_names_value,
    wtf_size_t i,
    NamedLinesType type,
    bool is_in_repeat) const {
  DCHECK(IsSubgriddedAxis() || !IsEmpty());

  // Handle lines before the auto repeat insertion point. If we don't have any
  // auto repeat tracks, we can skip all of the auto repeat logic below.
  if (auto_repeat_total_tracks_ == 0LU || i < insertion_point_) {
    AppendLines(line_names_value, i, NamedLinesType::kNamedLines);
    return;
  }

  DCHECK(auto_repeat_total_tracks_);

  // Handle tracks after the auto repeaters.
  if (i > insertion_point_ + auto_repeat_total_tracks_) {
    AppendLines(line_names_value, i - (auto_repeat_total_tracks_ - 1),
                NamedLinesType::kNamedLines);
    return;
  }

  // Handle the auto repeat track at the insertion point.
  if (i == insertion_point_) {
    AppendLines(line_names_value, i, NamedLinesType::kNamedLines);
    AppendLines(line_names_value, 0, NamedLinesType::kAutoRepeatNamedLines);
    return;
  }

  // Handle the final auto repeat track.
  if (i == insertion_point_ + auto_repeat_total_tracks_) {
    AppendLines(line_names_value, auto_repeat_track_list_length_,
                NamedLinesType::kAutoRepeatNamedLines);
    AppendLines(line_names_value, insertion_point_ + 1,
                NamedLinesType::kNamedLines);
    return;
  }

  // Handle tracks in the auto repeat range.
  wtf_size_t auto_repeat_index_in_first_repetition =
      (i - insertion_point_) % auto_repeat_track_list_length_;
  if (!auto_repeat_index_in_first_repetition && i > insertion_point_) {
    AppendLines(line_names_value, auto_repeat_track_list_length_,
                NamedLinesType::kAutoRepeatNamedLines);
  }
  AppendLines(line_names_value, auto_repeat_index_in_first_repetition,
              NamedLinesType::kAutoRepeatNamedLines);
}

void AddValuesForNamedGridLinesAtIndex(OrderedNamedLinesCollector& collector,
                                       wtf_size_t i,
                                       CSSValueList& list,
                                       NamedLinesType type,
                                       bool is_in_repeat = false) {
  if (collector.IsSubgriddedAxis()) {
    // Skip collapsed lines at the auto repeat insertion point.
    if (i == collector.InsertionPoint() &&
        collector.HasCollapsedAutoRepeatNamedLines()) {
      return;
    }
  } else if (collector.IsEmpty()) {
    return;
  }

  auto* line_names = MakeGarbageCollected<cssvalue::CSSBracketedValueList>();
  collector.CollectLineNamesForIndex(*line_names, i, type, is_in_repeat);

  // Subgridded track listings include empty lines per
  // https://www.w3.org/TR/css-grid-2/#resolved-track-list-subgrid.
  if (line_names->length() || collector.IsSubgriddedAxis()) {
    list.Append(*line_names);
  }
}

CSSValue* ComputedStyleUtils::ValueForGridAutoTrackList(
    GridTrackSizingDirection track_direction,
    const LayoutObject* layout_object,
    const ComputedStyle& style) {
  CSSValueList* list = CSSValueList::CreateSpaceSeparated();
  const NGGridTrackList& auto_track_list = track_direction == kForColumns
                                               ? style.GridAutoColumns()
                                               : style.GridAutoRows();
  if (auto_track_list.RepeaterCount() == 1) {
    for (wtf_size_t i = 0; i < auto_track_list.RepeatSize(0); ++i) {
      list->Append(*SpecifiedValueForGridTrackSize(
          auto_track_list.RepeatTrackSize(0, i), style));
    }
  }
  return list;
}

void PopulateGridTrackListUsedValues(CSSValueList* list,
                                     OrderedNamedLinesCollector& collector,
                                     const Vector<LayoutUnit, 1>& tracks,
                                     const ComputedStyle& style,
                                     wtf_size_t start,
                                     wtf_size_t end,
                                     int offset,
                                     bool discard_line_names) {
  DCHECK_LE(start, end);
  if (collector.HasCollapsedAutoRepeatNamedLines()) {
    // If the collector has a collapsed auto-repeat track, we need to adjust
    // `end` to account for it, as `AddValuesForNamedGridLinesAtIndex` expects
    // `insertion_point` to consume one track. Since there can only be one
    // auto-repeat track, this will always need to be incremented by one.
    if (collector.InsertionPoint() < end) {
      ++end;
    }
  }
  for (wtf_size_t i = start; i < end; ++i) {
    if (!discard_line_names &&
        (offset >= 0 || i >= static_cast<wtf_size_t>(-offset))) {
      AddValuesForNamedGridLinesAtIndex(collector, i + offset, *list,
                                        NamedLinesType::kNamedLines);
    }
    // Subgrids do not include sizes in the track listing.
    if (!collector.IsSubgriddedAxis()) {
      DCHECK_LE(i, tracks.size());
      list->Append(*ZoomAdjustedPixelValue(tracks[i], style));
    }
  }
  // Subgrid track names are always relative to offset 0, so they can ignore the
  // tracks after the offset.
  if (!collector.IsSubgriddedAxis() &&
      (!discard_line_names &&
       (offset >= 0 || end >= static_cast<wtf_size_t>(-offset)))) {
    AddValuesForNamedGridLinesAtIndex(collector, end + offset, *list,
                                      NamedLinesType::kNamedLines);
  }
}

void PopulateNonRepeater(CSSValueList* list,
                         OrderedNamedLinesCollector& collector,
                         const blink::NGGridTrackList& track_list,
                         wtf_size_t repeater_index,
                         wtf_size_t track_index,
                         const ComputedStyle& style) {
  DCHECK_EQ(track_list.RepeatType(repeater_index),
            NGGridTrackRepeater::RepeatType::kNoRepeat);

  AddValuesForNamedGridLinesAtIndex(collector, track_index, *list,
                                    NamedLinesType::kNamedLines);
  // Subgrid definitions do not include track sizes.
  if (!track_list.IsSubgriddedAxis()) {
    list->Append(*ComputedStyleUtils::SpecifiedValueForGridTrackSize(
        track_list.RepeatTrackSize(repeater_index, 0), style));
  }
}

void PopulateAutoRepeater(CSSValueList* list,
                          OrderedNamedLinesCollector& collector,
                          const blink::NGGridTrackList& track_list,
                          wtf_size_t repeater_index,
                          const ComputedStyle& style) {
  blink::NGGridTrackRepeater::RepeatType repeat_type =
      track_list.RepeatType(repeater_index);
  DCHECK(repeat_type == NGGridTrackRepeater::RepeatType::kAutoFill ||
         repeat_type == NGGridTrackRepeater::RepeatType::kAutoFit);

  const bool is_subgrid = track_list.IsSubgriddedAxis();
  CSSValueList* repeated_values;
  wtf_size_t repeat_size = is_subgrid
                               ? track_list.LineNameIndicesCount(repeater_index)
                               : track_list.RepeatSize(repeater_index);

  repeated_values = MakeGarbageCollected<cssvalue::CSSGridAutoRepeatValue>(
      repeat_type == NGGridTrackRepeater::RepeatType::kAutoFill
          ? CSSValueID::kAutoFill
          : CSSValueID::kAutoFit);

  // Unlike integer repeats, line names for auto repeats start at index 0 and go
  // to `repeat_size`. This is because auto repeat named lines are in their own
  // line name collection, while line names for integer repeats are expanded and
  // interspersed with non-repeaters in the track list.
  for (wtf_size_t i = 0; i < repeat_size; ++i) {
    AddValuesForNamedGridLinesAtIndex(collector, i, *repeated_values,
                                      NamedLinesType::kAutoRepeatNamedLines);

    // Subgrids do not support track sizes.
    if (!is_subgrid) {
      const GridTrackSize& track_size =
          track_list.RepeatTrackSize(repeater_index, i);
      repeated_values->Append(
          *ComputedStyleUtils::SpecifiedValueForGridTrackSize(track_size,
                                                              style));
    }
  }

  // Add any additional auto repeat line names after size definitions.
  for (wtf_size_t i = repeat_size; i < collector.AutoRepeatNamedLinesCount();
       ++i) {
    AddValuesForNamedGridLinesAtIndex(collector, i, *repeated_values,
                                      NamedLinesType::kAutoRepeatNamedLines);
  }
  // Subgrids allow for empty line definitions.
  if (is_subgrid && repeat_size == 0) {
    repeated_values->Append(
        *MakeGarbageCollected<cssvalue::CSSBracketedValueList>());
  }

  list->Append(*repeated_values);
}

// Returns the number of tracks populated after expanding repetitions.
wtf_size_t PopulateIntegerRepeater(CSSValueList* list,
                                   OrderedNamedLinesCollector& collector,
                                   const blink::NGGridTrackList& track_list,
                                   wtf_size_t repeater_index,
                                   wtf_size_t track_index,
                                   const ComputedStyle& style) {
  const bool is_subgrid = track_list.IsSubgriddedAxis();
  CSSValueList* repeated_values;
  wtf_size_t number_of_repetitions = track_list.RepeatCount(repeater_index, 0);
  wtf_size_t repeat_size = is_subgrid
                               ? track_list.LineNameIndicesCount(repeater_index)
                               : track_list.RepeatSize(repeater_index);

  repeated_values = MakeGarbageCollected<cssvalue::CSSGridIntegerRepeatValue>(
      number_of_repetitions);

  // Line names for integer repeats get expanded and interspersed with
  // non-repeaters in the track list.
  for (wtf_size_t i = 0; i < repeat_size; ++i) {
    AddValuesForNamedGridLinesAtIndex(
        collector, track_index + i, *repeated_values,
        NamedLinesType::kNamedLines, /* is_in_repeat */ true);

    // Subgrids do not support track sizes.
    if (!is_subgrid) {
      const GridTrackSize& track_size =
          track_list.RepeatTrackSize(repeater_index, i);
      repeated_values->Append(
          *ComputedStyleUtils::SpecifiedValueForGridTrackSize(track_size,
                                                              style));
    }
  }

  // Standalone grids may have line names after track sizes.
  if (!is_subgrid) {
    AddValuesForNamedGridLinesAtIndex(
        collector, track_index + repeat_size, *repeated_values,
        NamedLinesType::kNamedLines, /* is_in_repeat */ true);
  } else if (repeat_size == 0) {
    // Subgrids allow for empty line definitions.
    repeated_values->Append(
        *MakeGarbageCollected<cssvalue::CSSBracketedValueList>());
  }

  list->Append(*repeated_values);

  return repeat_size * number_of_repetitions;
}

void PopulateGridTrackListComputedValues(
    CSSValueList* list,
    OrderedNamedLinesCollector& collector,
    const blink::NGGridTrackList& track_list,
    const ComputedStyle& style) {
  const bool is_subgrid = collector.IsSubgriddedAxis();
  wtf_size_t track_index = 0;

  // Iterate over each repeater. This will cover all tracks because even non
  // repeats will add repeaters of type `kNoRepeat` to their track list.
  for (wtf_size_t i = 0; i < track_list.RepeaterCount(); ++i) {
    switch (track_list.RepeatType(i)) {
      case NGGridTrackRepeater::RepeatType::kNoRepeat:
        PopulateNonRepeater(list, collector, track_list, i, track_index, style);

        // Non repeaters always consume one track index.
        ++track_index;
        break;

      case NGGridTrackRepeater::RepeatType::kInteger:
        // Standalone grids can have line names between sizes and repeaters.
        if (!is_subgrid) {
          AddValuesForNamedGridLinesAtIndex(collector, track_index, *list,
                                            NamedLinesType::kNamedLines);
        }
        // `PopulateIntegerRepeater` will return the number of tracks populated.
        // We need to update `track_index` by this value, as the track list
        // has expanded integer repeaters and interspersed them with
        // non-repeaters.
        track_index += PopulateIntegerRepeater(list, collector, track_list, i,
                                               track_index, style);
        break;

      case NGGridTrackRepeater::RepeatType::kAutoFill:
      case NGGridTrackRepeater::RepeatType::kAutoFit:
        // Standalone grids can have line names between sizes and repeaters.
        if (!is_subgrid) {
          AddValuesForNamedGridLinesAtIndex(collector, track_index, *list,
                                            NamedLinesType::kNamedLines);
        }
        PopulateAutoRepeater(list, collector, track_list, i, style);

        // Auto repeaters always consume one track index.
        ++track_index;
        break;
      default:
        NOTREACHED_IN_MIGRATION();
    }
  }
  // Standalone grids can have line names after sizes and repeaters.
  if (!is_subgrid) {
    AddValuesForNamedGridLinesAtIndex(collector, track_index, *list,
                                      NamedLinesType::kNamedLines);
  }
}

CSSValue* ComputedStyleUtils::ValueForGridTrackList(
    GridTrackSizingDirection direction,
    const LayoutObject* layout_object,
    const ComputedStyle& style,
    bool force_computed_value) {
  const bool is_for_columns = direction == kForColumns;
  const ComputedGridTrackList& computed_grid_track_list =
      is_for_columns ? style.GridTemplateColumns() : style.GridTemplateRows();
  const auto* grid = DynamicTo<LayoutGrid>(layout_object);

  // Handle the 'none' case.
  bool is_track_list_empty =
      !computed_grid_track_list.track_list.RepeaterCount();
  if (grid && is_track_list_empty) {
    // For grids we should consider every listed track, whether implicitly or
    // explicitly created. Empty grids have a sole grid line per axis.
    const Vector<LayoutUnit>& positions =
        is_for_columns ? grid->ColumnPositions() : grid->RowPositions();
    is_track_list_empty = positions.size() == 1;
  }

  const bool is_subgrid_specified = computed_grid_track_list.IsSubgriddedAxis();
  const bool is_subgrid_valid =
      (grid && grid->HasCachedPlacementData())
          ? grid->CachedPlacementData().SubgridSpanSize(direction) != kNotFound
          : false;
  const bool is_subgrid = is_subgrid_specified && is_subgrid_valid;

  // Standalone grids with empty track lists should compute to `none`, but
  // this is not the case for subgrids. Subgrids need to account for the
  // following: "If there is no parent grid, this value is equivalent to the
  // initial value, `none`, and the grid container is not a subgrid."
  // https://www.w3.org/TR/css-grid-2/#subgrid-listing
  //
  // Interestingly, specifying `subgrid` on a non-grid *will* compute to
  // `subgrid` syntax.
  CSSValueList* list = CSSValueList::CreateSpaceSeparated();
  if (is_subgrid || (is_subgrid_specified && !grid)) {
    list->Append(
        *MakeGarbageCollected<CSSIdentifierValue>(CSSValueID::kSubgrid));
  } else if (!is_subgrid_specified && is_track_list_empty) {
    return CSSIdentifierValue::Create(CSSValueID::kNone);
  }

  wtf_size_t auto_repeat_insertion_point =
      computed_grid_track_list.auto_repeat_insertion_point;
  const NGGridTrackList& ng_track_list = computed_grid_track_list.track_list;

  // "Note: In general, resolved values are the computed values, except for a
  // small list of legacy 2.1 properties. However, compatibility with early
  // implementations of this module requires us to define grid-template-rows and
  // grid-template-columns as returning used values."
  //
  // https://www.w3.org/TR/css-grid-2/#resolved-track-list-standalone
  //
  // Default to the used value if it's a layout grid, unless
  // `force_computed_value` is set (which is used for `grid-template`). Non
  // layout-grids will always report the computed value.
  if (grid && !force_computed_value) {
    // The number of auto repeat tracks. For 'repeat(auto-fill, [x][y])' this
    // will be 2, regardless of what auto-fill computes to. For subgrids, use
    // the number of grid line names specified on the track definition. For
    // standalone grids, this will be the number of track sizes, as this can
    // can differ from the count on the track definition.
    wtf_size_t auto_repeat_track_list_length =
        ng_track_list.AutoRepeatTrackCount();

    // Standalone grids will report the track sizes in the computed style
    // string, so base the start and end indices on it.
    auto track_sizes = grid->TrackSizesForComputedStyle(direction);
    wtf_size_t start_index = 0;
    wtf_size_t end_index = track_sizes.size();
    if (is_subgrid) {
      // For subgrids, track sizes are not supported. Instead, calculate the end
      // index by subtracting the grid end from its start.
      start_index = grid->ExplicitGridStartForDirection(direction);
      end_index = grid->ExplicitGridEndForDirection(direction) -
                  grid->ExplicitGridStartForDirection(direction) + 1;
    }
    // If the element is a grid container, the resolved value is the used value,
    // specifying track sizes in pixels (if it's a standalone grid), and
    // expanding the repeat() notation.
    OrderedNamedLinesCollectorInGridLayout collector(
        computed_grid_track_list.ordered_named_grid_lines,
        computed_grid_track_list.auto_repeat_ordered_named_grid_lines,
        auto_repeat_insertion_point,
        grid->AutoRepeatCountForDirection(direction),
        auto_repeat_track_list_length, is_subgrid);
    // Named grid line indices are relative to the explicit grid, but we are
    // including all tracks. So we need to subtract the number of leading
    // implicit tracks in order to get the proper line index. This is ignored
    // for subgrids because they only have explicit tracks.
    int offset = -base::checked_cast<int>(
        grid->ExplicitGridStartForDirection(direction));

    // If `subgrid` is specified in `grid-template-rows/columns`, but the
    // element is not a valid subgrid, computed style should behave as if it's a
    // standalone grid. It should also drop any line names specified in the
    // invalid subgrid rows/column definitions. See
    // https://github.com/w3c/csswg-drafts/issues/9015.
    const bool discard_line_names =
        grid && is_subgrid_specified && !is_subgrid_valid;
    PopulateGridTrackListUsedValues(list, collector, track_sizes, style,
                                    start_index, end_index, offset,
                                    discard_line_names);
    return list;
  }

  OrderedNamedLinesCollector collector(
      computed_grid_track_list.ordered_named_grid_lines,
      computed_grid_track_list.auto_repeat_ordered_named_grid_lines,
      is_subgrid_specified, !!grid);
  PopulateGridTrackListComputedValues(list, collector, ng_track_list, style);
  return list;
}

CSSValue* ComputedStyleUtils::ValueForGridPosition(
    const GridPosition& position) {
  if (position.IsAuto()) {
    return CSSIdentifierValue::Create(CSSValueID::kAuto);
  }

  if (position.IsNamedGridArea()) {
    return MakeGarbageCollected<CSSCustomIdentValue>(position.NamedGridLine());
  }

  CSSValueList* list = CSSValueList::CreateSpaceSeparated();
  const bool has_named_line = !position.NamedGridLine().IsNull();
  if (position.IsSpan()) {
    list->Append(*CSSIdentifierValue::Create(CSSValueID::kSpan));

    // Do not include the numeric value of 1 if followed by a line name.
    if (position.SpanPosition() != 1 || !has_named_line) {
      list->Append(*CSSNumericLiteralValue::Create(
          position.SpanPosition(), CSSPrimitiveValue::UnitType::kNumber));
    }
  } else {
    list->Append(*CSSNumericLiteralValue::Create(
        position.IntegerPosition(), CSSPrimitiveValue::UnitType::kNumber));
  }

  if (has_named_line) {
    list->Append(
        *MakeGarbageCollected<CSSCustomIdentValue>(position.NamedGridLine()));
  }
  return list;
}

CSSValue* ComputedStyleUtils::ValueForMasonrySlack(
    const std::optional<Length>& slack_length,
    const ComputedStyle& style) {
  return slack_length ? ZoomAdjustedPixelValueForLength(*slack_length, style)
                      : CSSIdentifierValue::Create(CSSValueID::kNormal);
}

CSSValue* ComputedStyleUtils::ValueForMasonryTrackList(
    const LayoutObject* layout_object,
    const ComputedStyle& style) {
  const auto& computed_track_list = style.MasonryTemplateTracks();
  DCHECK_GT(computed_track_list.track_list.RepeaterCount(), 0u);

  auto* list = CSSValueList::CreateSpaceSeparated();
  OrderedNamedLinesCollector collector(
      computed_track_list.ordered_named_grid_lines,
      computed_track_list.auto_repeat_ordered_named_grid_lines,
      computed_track_list.IsSubgriddedAxis(), /*is_layout_grid=*/false);

  PopulateGridTrackListComputedValues(list, collector,
                                      computed_track_list.track_list, style);
  return list;
}

static bool IsSVGObjectWithWidthAndHeight(const LayoutObject& layout_object) {
  DCHECK(layout_object.IsSVGChild());
  return layout_object.IsSVGImage() || layout_object.IsSVGForeignObject() ||
         (layout_object.IsSVGShape() &&
          IsA<SVGRectElement>(layout_object.GetNode()));
}

gfx::SizeF ComputedStyleUtils::UsedBoxSize(const LayoutObject& layout_object) {
  if (layout_object.IsSVGChild() &&
      IsSVGObjectWithWidthAndHeight(layout_object)) {
    gfx::SizeF size = layout_object.ObjectBoundingBox().size();
    // The object bounding box does not have zoom applied. Multiply with zoom
    // here since we'll divide by it when we produce the CSS value.
    size.Scale(layout_object.StyleRef().EffectiveZoom());
    return size;
  }
  if (!layout_object.IsBox()) {
    return gfx::SizeF();
  }
  const auto& box = To<LayoutBox>(layout_object);
  return gfx::SizeF(box.StyleRef().BoxSizing() == EBoxSizing::kBorderBox
                        ? box.PhysicalBorderBoxRect().size
                        : box.ComputedCSSContentBoxRect().size);
}

CSSValue* ComputedStyleUtils::RenderTextDecorationFlagsToCSSValue(
    TextDecorationLine text_decoration) {
  switch (text_decoration) {
    case TextDecorationLine::kNone:
      return CSSIdentifierValue::Create(CSSValueID::kNone);
    case TextDecorationLine::kSpellingError:
      return CSSIdentifierValue::Create(CSSValueID::kSpellingError);
    case TextDecorationLine::kGrammarError:
      return CSSIdentifierValue::Create(CSSValueID::kGrammarError);
    default:
      break;
  }

  // Blink value is ignored.
  CSSValueList* list = CSSValueList::CreateSpaceSeparated();
  if (EnumHasFlags(text_decoration, TextDecorationLine::kUnderline)) {
    list->Append(*CSSIdentifierValue::Create(CSSValueID::kUnderline));
  }
  if (EnumHasFlags(text_decoration, TextDecorationLine::kOverline)) {
    list->Append(*CSSIdentifierValue::Create(CSSValueID::kOverline));
  }
  if (EnumHasFlags(text_decoration, TextDecorationLine::kLineThrough)) {
    list->Append(*CSSIdentifierValue::Create(CSSValueID::kLineThrough));
  }

  if (!list->length()) {
    return CSSIdentifierValue::Create(CSSValueID::kNone);
  }
  return list;
}

CSSValue* ComputedStyleUtils::ValueForTextDecorationStyle(
    ETextDecorationStyle text_decoration_style) {
  switch (text_decoration_style) {
    case ETextDecorationStyle::kSolid:
      return CSSIdentifierValue::Create(CSSValueID::kSolid);
    case ETextDecorationStyle::kDouble:
      return CSSIdentifierValue::Create(CSSValueID::kDouble);
    case ETextDecorationStyle::kDotted:
      return CSSIdentifierValue::Create(CSSValueID::kDotted);
    case ETextDecorationStyle::kDashed:
      return CSSIdentifierValue::Create(CSSValueID::kDashed);
    case ETextDecorationStyle::kWavy:
      return CSSIdentifierValue::Create(CSSValueID::kWavy);
  }

  NOTREACHED_IN_MIGRATION();
  return CSSInitialValue::Create();
}

CSSValue* ComputedStyleUtils::ValueForTextDecorationSkipInk(
    ETextDecorationSkipInk text_decoration_skip_ink) {
  if (text_decoration_skip_ink == ETextDecorationSkipInk::kNone) {
    return CSSIdentifierValue::Create(CSSValueID::kNone);
  }
  return CSSIdentifierValue::Create(CSSValueID::kAuto);
}

CSSValue* ComputedStyleUtils::TouchActionFlagsToCSSValue(
    TouchAction touch_action) {
  CSSValueList* list = CSSValueList::CreateSpaceSeparated();
  if (touch_action == TouchAction::kAuto) {
    list->Append(*CSSIdentifierValue::Create(CSSValueID::kAuto));
  } else if (touch_action == TouchAction::kNone) {
    list->Append(*CSSIdentifierValue::Create(CSSValueID::kNone));
  } else if (touch_action == TouchAction::kManipulation) {
    list->Append(*CSSIdentifierValue::Create(CSSValueID::kManipulation));
  } else {
    if ((touch_action & TouchAction::kPanX) == TouchAction::kPanX) {
      list->Append(*CSSIdentifierValue::Create(CSSValueID::kPanX));
    } else if ((touch_action & TouchAction::kPanLeft) != TouchAction::kNone) {
      list->Append(*CSSIdentifierValue::Create(CSSValueID::kPanLeft));
    } else if ((touch_action & TouchAction::kPanRight) != TouchAction::kNone) {
      list->Append(*CSSIdentifierValue::Create(CSSValueID::kPanRight));
    }
    if ((touch_action & TouchAction::kPanY) == TouchAction::kPanY) {
      list->Append(*CSSIdentifierValue::Create(CSSValueID::kPanY));
    } else if ((touch_action & TouchAction::kPanUp) != TouchAction::kNone) {
      list->Append(*CSSIdentifierValue::Create(CSSValueID::kPanUp));
    } else if ((touch_action & TouchAction::kPanDown) != TouchAction::kNone) {
      list->Append(*CSSIdentifierValue::Create(CSSValueID::kPanDown));
    }

    if ((touch_action & TouchAction::kPinchZoom) == TouchAction::kPinchZoom) {
      list->Append(*CSSIdentifierValue::Create(CSSValueID::kPinchZoom));
    }
  }

  DCHECK(list->length());
  return list;
}

CSSValue* ComputedStyleUtils::ValueForWillChange(
    const Vector<CSSPropertyID>& will_change_properties,
    bool will_change_contents,
    bool will_change_scroll_position) {
  CSSValueList* list = CSSValueList::CreateCommaSeparated();
  if (will_change_contents) {
    list->Append(*CSSIdentifierValue::Create(CSSValueID::kContents));
  }
  if (will_change_scroll_position) {
    list->Append(*CSSIdentifierValue::Create(CSSValueID::kScrollPosition));
  }
  for (wtf_size_t i = 0; i < will_change_properties.size(); ++i) {
    list->Append(
        *MakeGarbageCollected<CSSCustomIdentValue>(will_change_properties[i]));
  }
  if (!list->length()) {
    list->Append(*CSSIdentifierValue::Create(CSSValueID::kAuto));
  }
  return list;
}

namespace {

template <typename T, wtf_size_t C, typename Func, typename... Args>
CSSValue* CreateAnimationValueList(const Vector<T, C>& values,
                                   Func item_func,
                                   Args&&... args) {
  CSSValueList* list = CSSValueList::CreateCommaSeparated();
  for (const T& value : values) {
    list->Append(*item_func(value, std::forward<Args>(args)...));
  }
  return list;
}

}  // namespace

CSSValue* ComputedStyleUtils::ValueForAnimationDelay(
    const Timing::Delay& delay) {
  return CSSNumericLiteralValue::Create(delay.AsTimeValue().InSecondsF(),
                                        CSSPrimitiveValue::UnitType::kSeconds);
}

CSSValue* ComputedStyleUtils::ValueForAnimationDelayList(
    const CSSTimingData* timing_data) {
  return CreateAnimationValueList(
      timing_data
          ? timing_data->DelayStartList()
          : Vector<Timing::Delay, 1>{CSSTimingData::InitialDelayStart()},
      &ValueForAnimationDelay);
}

CSSValue* ComputedStyleUtils::ValueForAnimationDirection(
    Timing::PlaybackDirection direction) {
  switch (direction) {
    case Timing::PlaybackDirection::NORMAL:
      return CSSIdentifierValue::Create(CSSValueID::kNormal);
    case Timing::PlaybackDirection::ALTERNATE_NORMAL:
      return CSSIdentifierValue::Create(CSSValueID::kAlternate);
    case Timing::PlaybackDirection::REVERSE:
      return CSSIdentifierValue::Create(CSSValueID::kReverse);
    case Timing::PlaybackDirection::ALTERNATE_REVERSE:
      return CSSIdentifierValue::Create(CSSValueID::kAlternateReverse);
    default:
      NOTREACHED_IN_MIGRATION();
      return nullptr;
  }
}

CSSValue* ComputedStyleUtils::ValueForAnimationDirectionList(
    const CSSAnimationData* animation_data) {
  return CreateAnimationValueList(
      animation_data
          ? animation_data->DirectionList()
          : Vector<Timing::PlaybackDirection>{CSSAnimationData::
                                                  InitialDirection()},
      &ValueForAnimationDirection);
}

CSSValue* ComputedStyleUtils::ValueForAnimationDuration(
    const std::optional<double>& duration,
    bool resolve_auto_to_zero) {
  std::optional<double> resolved_duration =
      (!duration.has_value() && resolve_auto_to_zero) ? 0 : duration;
  if (!resolved_duration.has_value()) {
    return CSSIdentifierValue::Create(CSSValueID::kAuto);
  }
  return CSSNumericLiteralValue::Create(resolved_duration.value(),
                                        CSSPrimitiveValue::UnitType::kSeconds);
}

CSSValue* ComputedStyleUtils::ValueForAnimationDurationList(
    const CSSAnimationData* animation_data,
    CSSValuePhase phase) {
  // https://drafts.csswg.org/css-animations-2/#animation-duration
  // For backwards-compatibility with Level 1, when the computed value of
  // animation-timeline is auto (i.e. only one list value, and that value being
  // auto), the resolved value of auto for animation-duration is 0s whenever its
  // used value would also be 0s.
  bool resolve_auto_to_zero =
      (phase == CSSValuePhase::kResolvedValue) &&
      (!animation_data || animation_data->HasSingleInitialTimeline());
  return CreateAnimationValueList(
      animation_data ? animation_data->DurationList()
                     : Vector<std::optional<double>,
                              1>{CSSAnimationData::InitialDuration()},
      ValueForAnimationDuration, resolve_auto_to_zero);
}

CSSValue* ComputedStyleUtils::ValueForAnimationDurationList(
    const CSSTransitionData* transition_data) {
  return CreateAnimationValueList(
      transition_data ? transition_data->DurationList()
                      : Vector<std::optional<double>,
                               1>{CSSTransitionData::InitialDuration()},
      ValueForAnimationDuration,
      /* resolve_auto_to_zero */ false);
}

CSSValue* ComputedStyleUtils::ValueForAnimationFillMode(
    Timing::FillMode fill_mode) {
  switch (fill_mode) {
    case Timing::FillMode::NONE:
      return CSSIdentifierValue::Create(CSSValueID::kNone);
    case Timing::FillMode::FORWARDS:
      return CSSIdentifierValue::Create(CSSValueID::kForwards);
    case Timing::FillMode::BACKWARDS:
      return CSSIdentifierValue::Create(CSSValueID::kBackwards);
    case Timing::FillMode::BOTH:
      return CSSIdentifierValue::Create(CSSValueID::kBoth);
    default:
      NOTREACHED_IN_MIGRATION();
      return nullptr;
  }
}

CSSValue* ComputedStyleUtils::ValueForAnimationFillModeList(
    const CSSAnimationData* animation_data) {
  return CreateAnimationValueList(
      animation_data
          ? animation_data->FillModeList()
          : Vector<Timing::FillMode>{CSSAnimationData::InitialFillMode()},
      &ValueForAnimationFillMode);
}

CSSValue* ComputedStyleUtils::ValueForAnimationIterationCount(
    double iteration_count) {
  if (iteration_count == std::numeric_limits<double>::infinity()) {
    return CSSIdentifierValue::Create(CSSValueID::kInfinite);
  }
  return CSSNumericLiteralValue::Create(iteration_count,
                                        CSSPrimitiveValue::UnitType::kNumber);
}

CSSValue* ComputedStyleUtils::ValueForAnimationIterationCountList(
    const CSSAnimationData* animation_data) {
  return CreateAnimationValueList(
      animation_data
          ? animation_data->IterationCountList()
          : Vector<double>{CSSAnimationData::InitialIterationCount()},
      &ValueForAnimationIterationCount);
}

CSSValue* ComputedStyleUtils::ValueForAnimationPlayState(
    EAnimPlayState play_state) {
  if (play_state == EAnimPlayState::kPlaying) {
    return CSSIdentifierValue::Create(CSSValueID::kRunning);
  }
  DCHECK_EQ(play_state, EAnimPlayState::kPaused);
  return CSSIdentifierValue::Create(CSSValueID::kPaused);
}

CSSValue* ComputedStyleUtils::ValueForAnimationPlayStateList(
    const CSSAnimationData* animation_data) {
  return CreateAnimationValueList(
      animation_data
          ? animation_data->PlayStateList()
          : Vector<EAnimPlayState>{CSSAnimationData::InitialPlayState()},
      &ValueForAnimationPlayState);
}

namespace {

CSSValue* ValueForAnimationRange(const std::optional<TimelineOffset>& offset,
                                 const ComputedStyle& style,
                                 const Length& default_offset) {
  if (!offset.has_value()) {
    return MakeGarbageCollected<CSSIdentifierValue>(CSSValueID::kNormal);
  }
  CSSValueList* list = CSSValueList::CreateSpaceSeparated();
  if (offset->name != TimelineOffset::NamedRange::kNone) {
    list->Append(*MakeGarbageCollected<CSSIdentifierValue>(offset->name));
  }
  if (offset->offset != default_offset) {
    list->Append(*ComputedStyleUtils::ZoomAdjustedPixelValueForLength(
        offset->offset, style));
  }
  return list;
}

}  // namespace

CSSValue* ComputedStyleUtils::ValueForAnimationRangeStart(
    const std::optional<TimelineOffset>& offset,
    const ComputedStyle& style) {
  return ValueForAnimationRange(offset, style, Length::Percent(0.0));
}

CSSValue* ComputedStyleUtils::ValueForAnimationRangeStartList(
    const CSSAnimationData* animation_data,
    const ComputedStyle& style) {
  return CreateAnimationValueList(
      animation_data
          ? animation_data->RangeStartList()
          : Vector<std::optional<TimelineOffset>>{CSSAnimationData::
                                                      InitialRangeStart()},
      &ValueForAnimationRangeStart, style);
}

CSSValue* ComputedStyleUtils::ValueForAnimationRangeEnd(
    const std::optional<TimelineOffset>& offset,
    const ComputedStyle& style) {
  return ValueForAnimationRange(offset, style, Length::Percent(100.0));
}

CSSValue* ComputedStyleUtils::ValueForAnimationRangeEndList(
    const CSSAnimationData* animation_data,
    const ComputedStyle& style) {
  return CreateAnimationValueList(
      animation_data
          ? animation_data->RangeEndList()
          : Vector<std::optional<TimelineOffset>>{CSSAnimationData::
                                                      InitialRangeEnd()},
      &ValueForAnimationRangeEnd, style);
}

CSSValue* ComputedStyleUtils::ValueForAnimationTimingFunction(
    const scoped_refptr<TimingFunction>& timing_function) {
  switch (timing_function->GetType()) {
    case TimingFunction::Type::CUBIC_BEZIER: {
      const auto* bezier_timing_function =
          To<CubicBezierTimingFunction>(timing_function.get());
      if (bezier_timing_function->GetEaseType() !=
          CubicBezierTimingFunction::EaseType::CUSTOM) {
        CSSValueID value_id = CSSValueID::kInvalid;
        switch (bezier_timing_function->GetEaseType()) {
          case CubicBezierTimingFunction::EaseType::EASE:
            value_id = CSSValueID::kEase;
            break;
          case CubicBezierTimingFunction::EaseType::EASE_IN:
            value_id = CSSValueID::kEaseIn;
            break;
          case CubicBezierTimingFunction::EaseType::EASE_OUT:
            value_id = CSSValueID::kEaseOut;
            break;
          case CubicBezierTimingFunction::EaseType::EASE_IN_OUT:
            value_id = CSSValueID::kEaseInOut;
            break;
          default:
            NOTREACHED_IN_MIGRATION();
            return nullptr;
        }
        return CSSIdentifierValue::Create(value_id);
      }
      return MakeGarbageCollected<cssvalue::CSSCubicBezierTimingFunctionValue>(
          bezier_timing_function->X1(), bezier_timing_function->Y1(),
          bezier_timing_function->X2(), bezier_timing_function->Y2());
    }

    case TimingFunction::Type::STEPS: {
      const auto* steps_timing_function =
          To<StepsTimingFunction>(timing_function.get());
      StepsTimingFunction::StepPosition position =
          steps_timing_function->GetStepPosition();
      int steps = steps_timing_function->NumberOfSteps();

      // Canonical form of step timing function is step(n, type) or step(n) even
      // if initially parsed as step-start or step-end.
      return MakeGarbageCollected<cssvalue::CSSStepsTimingFunctionValue>(
          steps, position);
    }

    default:
      const auto* linear_timing_function =
          To<LinearTimingFunction>(timing_function.get());
      if (linear_timing_function->IsTrivial()) {
        return CSSIdentifierValue::Create(CSSValueID::kLinear);
      }
      return MakeGarbageCollected<cssvalue::CSSLinearTimingFunctionValue>(
          linear_timing_function->Points());
  }
}

CSSValue* ComputedStyleUtils::ValueForAnimationTimingFunctionList(
    const CSSTimingData* timing_data) {
  return CreateAnimationValueList(
      timing_data ? timing_data->TimingFunctionList()
                  : Vector<scoped_refptr<TimingFunction>,
                           1>{CSSAnimationData::InitialTimingFunction()},
      &ValueForAnimationTimingFunction);
}

CSSValue* ComputedStyleUtils::ValueForAnimationTimeline(
    const StyleTimeline& timeline) {
  if (timeline.IsKeyword()) {
    DCHECK(timeline.GetKeyword() == CSSValueID::kAuto ||
           timeline.GetKeyword() == CSSValueID::kNone);
    return CSSIdentifierValue::Create(timeline.GetKeyword());
  }
  if (timeline.IsName()) {
    const ScopedCSSName& scoped_name = timeline.GetName();
    const AtomicString& name = scoped_name.GetName();
    // Serialize as <string> if the value is not a valid <custom-ident>.
    if (css_parsing_utils::IsCSSWideKeyword(name) ||
        EqualIgnoringASCIICase(name, "auto") ||
        EqualIgnoringASCIICase(name, "none")) {
      return MakeGarbageCollected<CSSStringValue>(name);
    }
    return MakeGarbageCollected<CSSCustomIdentValue>(name);
  }
  if (timeline.IsView()) {
    const StyleTimeline::ViewData& view_data = timeline.GetView();
    CSSValue* axis = view_data.HasDefaultAxis()
                         ? nullptr
                         : CSSIdentifierValue::Create(view_data.GetAxis());
    auto* inset =
        view_data.HasDefaultInset()
            ? nullptr
            : MakeGarbageCollected<CSSValuePair>(
                  CSSValue::Create(view_data.GetInset().GetStart(), 1),
                  CSSValue::Create(view_data.GetInset().GetEnd(), 1),
                  CSSValuePair::kDropIdenticalValues);
    return MakeGarbageCollected<cssvalue::CSSViewValue>(axis, inset);
  }
  DCHECK(timeline.IsScroll());
  const StyleTimeline::ScrollData& scroll_data = timeline.GetScroll();
  CSSValue* scroller =
      scroll_data.HasDefaultScroller()
          ? nullptr
          : CSSIdentifierValue::Create(scroll_data.GetScroller());
  CSSValue* axis = scroll_data.HasDefaultAxis()
                       ? nullptr
                       : CSSIdentifierValue::Create(scroll_data.GetAxis());

  return MakeGarbageCollected<cssvalue::CSSScrollValue>(scroller, axis);
}

CSSValue* ComputedStyleUtils::ValueForAnimationTimelineList(
    const CSSAnimationData* animation_data) {
  return CreateAnimationValueList(
      animation_data
          ? animation_data->TimelineList()
          : Vector<StyleTimeline>{CSSAnimationData::InitialTimeline()},
      &ValueForAnimationTimeline);
}

CSSValue* ComputedStyleUtils::ValueForTimelineInset(
    const TimelineInset& inset,
    const ComputedStyle& style) {
  return MakeGarbageCollected<CSSValuePair>(
      ComputedStyleUtils::ZoomAdjustedPixelValueForLength(inset.GetStart(),
                                                          style),
      ComputedStyleUtils::ZoomAdjustedPixelValueForLength(inset.GetEnd(),
                                                          style),
      CSSValuePair::kDropIdenticalValues);
}

CSSValue* ComputedStyleUtils::SingleValueForTimelineShorthand(
    const ScopedCSSName* name,
    TimelineAxis axis,
    std::optional<TimelineInset> inset,
    const ComputedStyle& style) {
  CSSValueList* list = CSSValueList::CreateSpaceSeparated();
  list->Append(*ValueForCustomIdentOrNone(name));
  if (axis != TimelineAxis::kBlock) {
    list->Append(*CSSIdentifierValue::Create(axis));
  }
  if (inset.value_or(TimelineInset()) != TimelineInset()) {
    list->Append(*ValueForTimelineInset(inset.value(), style));
  }
  return list;
}

CSSValueList* ComputedStyleUtils::ValuesForBorderRadiusCorner(
    const LengthSize& radius,
    const ComputedStyle& style) {
  CSSValueList* list = CSSValueList::CreateSpaceSeparated();
  if (radius.Width().IsPercent()) {
    list->Append(*CSSNumericLiteralValue::Create(
        radius.Width().Percent(), CSSPrimitiveValue::UnitType::kPercentage));
  } else {
    list->Append(*ZoomAdjustedPixelValueForLength(radius.Width(), style));
  }
  if (radius.Height().IsPercent()) {
    list->Append(*CSSNumericLiteralValue::Create(
        radius.Height().Percent(), CSSPrimitiveValue::UnitType::kPercentage));
  } else {
    list->Append(*ZoomAdjustedPixelValueForLength(radius.Height(), style));
  }
  return list;
}

CSSValue* ComputedStyleUtils::ValueForBorderRadiusCorner(
    const LengthSize& radius,
    const ComputedStyle& style) {
  return MakeGarbageCollected<CSSValuePair>(
      ZoomAdjustedPixelValueForLength(radius.Width(), style),
      ZoomAdjustedPixelValueForLength(radius.Height(), style),
      CSSValuePair::kDropIdenticalValues);
}

CSSFunctionValue* ComputedStyleUtils::ValueForTransform(
    const gfx::Transform& matrix,
    float zoom,
    bool force_matrix3d) {
  if (matrix.Is2dTransform() && !force_matrix3d) {
    auto* result = MakeGarbageCollected<CSSFunctionValue>(CSSValueID::kMatrix);
    // CSS matrix values are returned in column-major order.
    auto unzoomed = AffineTransform::FromTransform(matrix).Zoom(1.f / zoom);
    for (double value : {unzoomed.A(), unzoomed.B(), unzoomed.C(), unzoomed.D(),
                         unzoomed.E(), unzoomed.F()}) {
      result->Append(*CSSNumericLiteralValue::Create(
          value, CSSPrimitiveValue::UnitType::kNumber));
    }
    return result;
  } else {
    CSSFunctionValue* result =
        MakeGarbageCollected<CSSFunctionValue>(CSSValueID::kMatrix3d);
    // CSS matrix values are returned in column-major order.
    auto unzoomed = matrix;
    unzoomed.Zoom(1.f / zoom);
    for (int i = 0; i < 16; i++) {
      result->Append(*CSSNumericLiteralValue::Create(
          unzoomed.ColMajorData(i), CSSPrimitiveValue::UnitType::kNumber));
    }
    return result;
  }
}

CSSValueID ComputedStyleUtils::CSSValueIDForScaleOperation(
    const TransformOperation::OperationType type) {
  switch (type) {
    case TransformOperation::kScaleX:
      return CSSValueID::kScaleX;
    case TransformOperation::kScaleY:
      return CSSValueID::kScaleY;
    case TransformOperation::kScaleZ:
      return CSSValueID::kScaleZ;
    case TransformOperation::kScale3D:
      return CSSValueID::kScale3d;
    default:
      DCHECK(type == TransformOperation::kScale);
      return CSSValueID::kScale;
  }
}

CSSValueID ComputedStyleUtils::CSSValueIDForTranslateOperation(
    const TransformOperation::OperationType type) {
  switch (type) {
    case TransformOperation::kTranslateX:
      return CSSValueID::kTranslateX;
    case TransformOperation::kTranslateY:
      return CSSValueID::kTranslateY;
    case TransformOperation::kTranslateZ:
      return CSSValueID::kTranslateZ;
    case TransformOperation::kTranslate3D:
      return CSSValueID::kTranslate3d;
    default:
      DCHECK(type == TransformOperation::kTranslate);
      return CSSValueID::kTranslate;
  }
}

CSSValueID ComputedStyleUtils::CSSValueIDForRotateOperation(
    const TransformOperation::OperationType type) {
  switch (type) {
    case TransformOperation::kRotateX:
      return CSSValueID::kRotateX;
    case TransformOperation::kRotateY:
      return CSSValueID::kRotateY;
    case TransformOperation::kRotateZ:
      return CSSValueID::kRotateZ;
    case TransformOperation::kRotate3D:
      return CSSValueID::kRotate3d;
    default:
      return CSSValueID::kRotate;
  }
}

// We collapse functions like translateX into translate, since we will reify
// them as a translate anyway.
CSSFunctionValue* ComputedStyleUtils::ValueForTransformOperation(
    const TransformOperation& operation,
    float zoom,
    gfx::SizeF box_size) {
  switch (operation.GetType()) {
    case TransformOperation::kScaleX:
    case TransformOperation::kScaleY:
    case TransformOperation::kScaleZ:
    case TransformOperation::kScale:
    case TransformOperation::kScale3D: {
      const auto& scale = To<ScaleTransformOperation>(operation);

      CSSValueID id = CSSValueIDForScaleOperation(operation.GetType());

      CSSFunctionValue* result = MakeGarbageCollected<CSSFunctionValue>(id);
      if (id == CSSValueID::kScaleX || id == CSSValueID::kScale ||
          id == CSSValueID::kScale3d) {
        result->Append(*CSSNumericLiteralValue::Create(
            scale.X(), CSSPrimitiveValue::UnitType::kNumber));
      }
      if (id == CSSValueID::kScaleY ||
          (id == CSSValueID::kScale && scale.Y() != scale.X()) ||
          id == CSSValueID::kScale3d) {
        result->Append(*CSSNumericLiteralValue::Create(
            scale.Y(), CSSPrimitiveValue::UnitType::kNumber));
      }
      if (id == CSSValueID::kScale3d || id == CSSValueID::kScaleZ) {
        result->Append(*CSSNumericLiteralValue::Create(
            scale.Z(), CSSPrimitiveValue::UnitType::kNumber));
      }
      return result;
    }
    case TransformOperation::kTranslateX:
    case TransformOperation::kTranslateY:
    case TransformOperation::kTranslateZ:
    case TransformOperation::kTranslate:
    case TransformOperation::kTranslate3D: {
      const auto& translate = To<TranslateTransformOperation>(operation);

      CSSValueID id = CSSValueIDForTranslateOperation(operation.GetType());

      CSSFunctionValue* result = MakeGarbageCollected<CSSFunctionValue>(id);
      if (id == CSSValueID::kTranslateX || id == CSSValueID::kTranslate ||
          id == CSSValueID::kTranslate3d) {
        result->Append(
            *CSSPrimitiveValue::CreateFromLength(translate.X(), zoom));
      }
      if (id == CSSValueID::kTranslateY ||
          (id == CSSValueID::kTranslate && (translate.Y().Value() != 0.f)) ||
          id == CSSValueID::kTranslate3d) {
        result->Append(
            *CSSPrimitiveValue::CreateFromLength(translate.Y(), zoom));
      }
      if (id == CSSValueID::kTranslate3d || id == CSSValueID::kTranslateZ) {
        // Since this is pixel length, we must unzoom (CreateFromLength above
        // does the division internally).
        result->Append(*CSSNumericLiteralValue::Create(
            translate.Z() / zoom, CSSPrimitiveValue::UnitType::kPixels));
      }
      return result;
    }
    case TransformOperation::kRotateX:
    case TransformOperation::kRotateY:
    case TransformOperation::kRotateZ:
    case TransformOperation::kRotate3D:
    case TransformOperation::kRotate: {
      const auto& rotate = To<RotateTransformOperation>(operation);
      CSSValueID id = CSSValueIDForRotateOperation(operation.GetType());

      CSSFunctionValue* result = MakeGarbageCollected<CSSFunctionValue>(id);
      if (id == CSSValueID::kRotate3d) {
        result->Append(*CSSNumericLiteralValue::Create(
            rotate.X(), CSSPrimitiveValue::UnitType::kNumber));
        result->Append(*CSSNumericLiteralValue::Create(
            rotate.Y(), CSSPrimitiveValue::UnitType::kNumber));
        result->Append(*CSSNumericLiteralValue::Create(
            rotate.Z(), CSSPrimitiveValue::UnitType::kNumber));
      }
      result->Append(*CSSNumericLiteralValue::Create(
          rotate.Angle(), CSSPrimitiveValue::UnitType::kDegrees));
      return result;
    }
    case TransformOperation::kRotateAroundOrigin: {
      // TODO(https://github.com/w3c/csswg-drafts/issues/5011):
      // Update this once there is consensus.
      gfx::Transform matrix;
      operation.Apply(matrix, gfx::SizeF(0, 0));
      return ValueForTransform(matrix, zoom,
                               /*force_matrix3d=*/false);
    }
    case TransformOperation::kSkewX: {
      const auto& skew = To<SkewTransformOperation>(operation);
      auto* result = MakeGarbageCollected<CSSFunctionValue>(CSSValueID::kSkewX);
      result->Append(*CSSNumericLiteralValue::Create(
          skew.AngleX(), CSSPrimitiveValue::UnitType::kDegrees));
      return result;
    }
    case TransformOperation::kSkewY: {
      const auto& skew = To<SkewTransformOperation>(operation);
      auto* result = MakeGarbageCollected<CSSFunctionValue>(CSSValueID::kSkewY);
      result->Append(*CSSNumericLiteralValue::Create(
          skew.AngleY(), CSSPrimitiveValue::UnitType::kDegrees));
      return result;
    }
    case TransformOperation::kSkew: {
      const auto& skew = To<SkewTransformOperation>(operation);
      auto* result = MakeGarbageCollected<CSSFunctionValue>(CSSValueID::kSkew);
      result->Append(*CSSNumericLiteralValue::Create(
          skew.AngleX(), CSSPrimitiveValue::UnitType::kDegrees));
      result->Append(*CSSNumericLiteralValue::Create(
          skew.AngleY(), CSSPrimitiveValue::UnitType::kDegrees));
      return result;
    }
    case TransformOperation::kPerspective: {
      const auto& perspective = To<PerspectiveTransformOperation>(operation);
      auto* result =
          MakeGarbageCollected<CSSFunctionValue>(CSSValueID::kPerspective);
      if (perspective.Perspective()) {
        result->Append(*CSSNumericLiteralValue::Create(
            *perspective.Perspective() / zoom,
            CSSPrimitiveValue::UnitType::kPixels));
      } else {
        result->Append(*CSSIdentifierValue::Create(CSSValueID::kNone));
      }
      return result;
    }
    case TransformOperation::kMatrix: {
      const auto& matrix = To<MatrixTransformOperation>(operation).Matrix();
      return ValueForTransform(matrix, zoom,
                               /*force_matrix3d=*/false);
    }
    case TransformOperation::kMatrix3D: {
      const auto& matrix = To<Matrix3DTransformOperation>(operation).Matrix();
      // Force matrix3d serialization
      return ValueForTransform(matrix, zoom,
                               /*force_matrix3d=*/true);
    }
    case TransformOperation::kInterpolated:
      // TODO(https://github.com/w3c/csswg-drafts/issues/2854):
      // Deferred interpolations are currently unreperesentable in CSS.
      // This currently converts the operation to a matrix, using box_size if
      // provided, 0x0 if not (returning all but the relative translate
      // portion of the transform). Update this once the spec is updated.
      gfx::Transform matrix;
      operation.Apply(matrix, box_size);
      return ValueForTransform(matrix, zoom,
                               /*force_matrix3d=*/false);
  }
}

CSSValue* ComputedStyleUtils::ValueForTransformList(
    const TransformOperations& transform_list,
    float zoom,
    gfx::SizeF box_size) {
  if (!transform_list.Operations().size()) {
    return CSSIdentifierValue::Create(CSSValueID::kNone);
  }

  CSSValueList* components = CSSValueList::CreateSpaceSeparated();
  for (const auto& operation : transform_list.Operations()) {
    CSSValue* op_value = ValueForTransformOperation(*operation, zoom, box_size);
    components->Append(*op_value);
  }
  return components;
}

CSSValue* ComputedStyleUtils::ValueForTransformFunction(
    const TransformOperations& transform_list) {
  CHECK_EQ(transform_list.Operations().size(), 1u);
  return ValueForTransformOperation(*transform_list.Operations()[0], 1,
                                    gfx::SizeF());
}

gfx::RectF ComputedStyleUtils::ReferenceBoxForTransform(
    const LayoutObject& layout_object) {
  if (layout_object.IsSVGChild()) {
    return TransformHelper::ComputeReferenceBox(layout_object);
  }
  if (const auto* layout_box = DynamicTo<LayoutBox>(layout_object)) {
    return gfx::RectF(layout_box->PhysicalBorderBoxRect());
  }
  return gfx::RectF();
}

CSSValue* ComputedStyleUtils::ComputedTransformList(
    const ComputedStyle& style,
    const LayoutObject* layout_object) {
  gfx::SizeF box_size(0, 0);
  if (layout_object) {
    box_size = ReferenceBoxForTransform(*layout_object).size();
  }

  return ValueForTransformList(style.Transform(), style.EffectiveZoom(),
                               box_size);
}

CSSValue* ComputedStyleUtils::ResolvedTransform(
    const LayoutObject* layout_object,
    const ComputedStyle& style) {
  if (!layout_object || !style.HasTransformOperations()) {
    return CSSIdentifierValue::Create(CSSValueID::kNone);
  }

  gfx::RectF reference_box = ReferenceBoxForTransform(*layout_object);

  const auto* layout_box = layout_object->IsSVGChild()
                               ? nullptr
                               : DynamicTo<LayoutBox>(*layout_object);

  gfx::Transform transform;
  style.ApplyTransform(transform, layout_box, reference_box,
                       ComputedStyle::kIncludeTransformOperations,
                       ComputedStyle::kExcludeTransformOrigin,
                       ComputedStyle::kExcludeMotionPath,
                       ComputedStyle::kExcludeIndependentTransformProperties);

  // FIXME: Need to print out individual functions
  // (https://bugs.webkit.org/show_bug.cgi?id=23924)
  CSSValueList* list = CSSValueList::CreateSpaceSeparated();
  list->Append(*ValueForTransform(transform, style.EffectiveZoom(),
                                  /*force_matrix3d=*/false));

  return list;
}

CSSValue* ComputedStyleUtils::CreateTransitionPropertyValue(
    const CSSTransitionData::TransitionProperty& property) {
  if (property.property_type == CSSTransitionData::kTransitionNone) {
    return CSSIdentifierValue::Create(CSSValueID::kNone);
  }
  if (property.property_type == CSSTransitionData::kTransitionUnknownProperty) {
    return MakeGarbageCollected<CSSCustomIdentValue>(property.property_string);
  }
  DCHECK_EQ(property.property_type,
            CSSTransitionData::kTransitionKnownProperty);
  return MakeGarbageCollected<CSSCustomIdentValue>(
      CSSUnresolvedProperty::Get(property.unresolved_property)
          .GetPropertyNameAtomicString());
}

CSSValue* ComputedStyleUtils::CreateTransitionBehaviorValue(
    const CSSTransitionData::TransitionBehavior& type) {
  switch (type) {
    case CSSTransitionData::TransitionBehavior::kNormal:
      return CSSIdentifierValue::Create(CSSValueID::kNormal);
    case CSSTransitionData::TransitionBehavior::kAllowDiscrete:
      return CSSIdentifierValue::Create(CSSValueID::kAllowDiscrete);
  }
  NOTREACHED_IN_MIGRATION()
      << " Unrecognized type: " << static_cast<unsigned>(type);
  return nullptr;
}

CSSValue* ComputedStyleUtils::ValueForTransitionProperty(
    const CSSTransitionData* transition_data) {
  CSSValueList* list = CSSValueList::CreateCommaSeparated();
  if (transition_data) {
    for (wtf_size_t i = 0; i < transition_data->PropertyList().size(); ++i) {
      list->Append(
          *CreateTransitionPropertyValue(transition_data->PropertyList()[i]));
    }
  } else {
    list->Append(*CSSIdentifierValue::Create(CSSValueID::kAll));
  }
  return list;
}

CSSValue* ComputedStyleUtils::ValueForTransitionBehavior(
    const CSSTransitionData* transition_data) {
  CSSValueList* list = CSSValueList::CreateCommaSeparated();
  if (transition_data) {
    for (const auto& mode : transition_data->BehaviorList()) {
      list->Append(*CreateTransitionBehaviorValue(mode));
    }
  } else {
    list->Append(*CSSIdentifierValue::Create(CSSValueID::kNormal));
  }
  return list;
}

CSSValueID ValueForQuoteType(const QuoteType quote_type) {
  switch (quote_type) {
    case QuoteType::kNoOpen:
      return CSSValueID::kNoOpenQuote;
    case QuoteType::kNoClose:
      return CSSValueID::kNoCloseQuote;
    case QuoteType::kClose:
      return CSSValueID::kCloseQuote;
    case QuoteType::kOpen:
      return CSSValueID::kOpenQuote;
  }
  NOTREACHED_IN_MIGRATION();
  return CSSValueID::kInvalid;
}

CSSValue* ComputedStyleUtils::ValueForContentData(const ComputedStyle& style,
                                                  bool allow_visited_style,
                                                  CSSValuePhase value_phase) {
  if (style.ContentPreventsBoxGeneration()) {
    return CSSIdentifierValue::Create(CSSValueID::kNone);
  }
  if (style.ContentBehavesAsNormal()) {
    return CSSIdentifierValue::Create(CSSValueID::kNormal);
  }

  CSSValueList* outer_list = CSSValueList::CreateSlashSeparated();
  CSSValueList* list = CSSValueList::CreateSpaceSeparated();

  // Alternative text optionally specified after a forward slash appearing after
  // the last content list item.
  CSSStringValue* alt_text = nullptr;
  for (const ContentData* content_data = style.GetContentData(); content_data;
       content_data = content_data->Next()) {
    if (content_data->IsCounter()) {
      const CounterContentData& counter = To<CounterContentData>(*content_data);
      auto* identifier =
          MakeGarbageCollected<CSSCustomIdentValue>(counter.Identifier());
      auto* separator =
          MakeGarbageCollected<CSSStringValue>(counter.Separator());
      auto* list_style =
          MakeGarbageCollected<CSSCustomIdentValue>(counter.ListStyle());
      list->Append(*MakeGarbageCollected<cssvalue::CSSCounterValue>(
          identifier, list_style, separator));
    } else if (content_data->IsImage()) {
      const StyleImage* image = To<ImageContentData>(content_data)->GetImage();
      DCHECK(image);
      list->Append(
          *image->ComputedCSSValue(style, allow_visited_style, value_phase));
    } else if (content_data->IsText()) {
      list->Append(*MakeGarbageCollected<CSSStringValue>(
          To<TextContentData>(content_data)->GetText()));
    } else if (content_data->IsQuote()) {
      const QuoteType quote_type = To<QuoteContentData>(content_data)->Quote();
      list->Append(*CSSIdentifierValue::Create(ValueForQuoteType(quote_type)));
    } else if (content_data->IsAltText()) {
      alt_text = MakeGarbageCollected<CSSStringValue>(
          To<AltTextContentData>(content_data)->ConcatenateAltText());
      break;
    } else {
      NOTREACHED_IN_MIGRATION();
    }
  }
  DCHECK(list->length());

  outer_list->Append(*list);
  if (alt_text) {
    outer_list->Append(*alt_text);
  }
  return outer_list;
}

CSSValue* ComputedStyleUtils::ValueForCounterDirectives(
    const ComputedStyle& style,
    CountersAttachmentContext::Type type) {
  const CounterDirectiveMap* map = style.GetCounterDirectives();
  if (!map) {
    return CSSIdentifierValue::Create(CSSValueID::kNone);
  }

  CSSValueList* list = CSSValueList::CreateSpaceSeparated();
  for (const auto& item : *map) {
    bool is_valid_counter_value = false;
    switch (type) {
      case CountersAttachmentContext::Type::kIncrementType:
        is_valid_counter_value = item.value.IsIncrement();
        break;
      case CountersAttachmentContext::Type::kResetType:
        is_valid_counter_value = item.value.IsReset();
        break;
      case CountersAttachmentContext::Type::kSetType:
        is_valid_counter_value = item.value.IsSet();
        break;
    }

    if (!is_valid_counter_value) {
      continue;
    }

    int32_t number = 0;
    switch (type) {
      case CountersAttachmentContext::Type::kIncrementType:
        number = item.value.IncrementValue();
        break;
      case CountersAttachmentContext::Type::kResetType:
        number = item.value.ResetValue();
        break;
      case CountersAttachmentContext::Type::kSetType:
        number = item.value.SetValue();
        break;
    }
    list->Append(*MakeGarbageCollected<CSSValuePair>(
        MakeGarbageCollected<CSSCustomIdentValue>(item.key),
        CSSNumericLiteralValue::Create((double)number,
                                       CSSPrimitiveValue::UnitType::kInteger),
        CSSValuePair::IdenticalValuesPolicy::kDropIdenticalValues));
  }

  if (!list->length()) {
    return CSSIdentifierValue::Create(CSSValueID::kNone);
  }

  return list;
}

CSSValue* ComputedStyleUtils::ValueForShape(const ComputedStyle& style,
                                            bool allow_visited_style,
                                            ShapeValue* shape_value,
                                            CSSValuePhase value_phase) {
  if (!shape_value) {
    return CSSIdentifierValue::Create(CSSValueID::kNone);
  }
  if (shape_value->GetType() == ShapeValue::kBox) {
    return CSSIdentifierValue::Create(shape_value->CssBox());
  }
  if (shape_value->GetType() == ShapeValue::kImage) {
    if (shape_value->GetImage()) {
      return shape_value->GetImage()->ComputedCSSValue(
          style, allow_visited_style, value_phase);
    }
    return CSSIdentifierValue::Create(CSSValueID::kNone);
  }

  DCHECK_EQ(shape_value->GetType(), ShapeValue::kShape);

  CSSValueList* list = CSSValueList::CreateSpaceSeparated();
  list->Append(*ValueForBasicShape(style, shape_value->Shape()));
  if (shape_value->CssBox() != CSSBoxType::kMissing) {
    list->Append(*CSSIdentifierValue::Create(shape_value->CssBox()));
  }
  return list;
}

CSSValueList* ComputedStyleUtils::ValueForBorderRadiusShorthand(
    const ComputedStyle& style) {
  CSSValueList* list = CSSValueList::CreateSlashSeparated();

  bool show_horizontal_bottom_left = style.BorderTopRightRadius().Width() !=
                                     style.BorderBottomLeftRadius().Width();
  bool show_horizontal_bottom_right =
      show_horizontal_bottom_left || (style.BorderBottomRightRadius().Width() !=
                                      style.BorderTopLeftRadius().Width());
  bool show_horizontal_top_right =
      show_horizontal_bottom_right || (style.BorderTopRightRadius().Width() !=
                                       style.BorderTopLeftRadius().Width());

  bool show_vertical_bottom_left = style.BorderTopRightRadius().Height() !=
                                   style.BorderBottomLeftRadius().Height();
  bool show_vertical_bottom_right =
      show_vertical_bottom_left || (style.BorderBottomRightRadius().Height() !=
                                    style.BorderTopLeftRadius().Height());
  bool show_vertical_top_right =
      show_vertical_bottom_right || (style.BorderTopRightRadius().Height() !=
                                     style.BorderTopLeftRadius().Height());

  CSSValueList* top_left_radius =
      ValuesForBorderRadiusCorner(style.BorderTopLeftRadius(), style);
  CSSValueList* top_right_radius =
      ValuesForBorderRadiusCorner(style.BorderTopRightRadius(), style);
  CSSValueList* bottom_right_radius =
      ValuesForBorderRadiusCorner(style.BorderBottomRightRadius(), style);
  CSSValueList* bottom_left_radius =
      ValuesForBorderRadiusCorner(style.BorderBottomLeftRadius(), style);

  CSSValueList* horizontal_radii = CSSValueList::CreateSpaceSeparated();
  horizontal_radii->Append(top_left_radius->Item(0));
  if (show_horizontal_top_right) {
    horizontal_radii->Append(top_right_radius->Item(0));
  }
  if (show_horizontal_bottom_right) {
    horizontal_radii->Append(bottom_right_radius->Item(0));
  }
  if (show_horizontal_bottom_left) {
    horizontal_radii->Append(bottom_left_radius->Item(0));
  }

  list->Append(*horizontal_radii);

  CSSValueList* vertical_radii = CSSValueList::CreateSpaceSeparated();
  vertical_radii->Append(top_left_radius->Item(1));
  if (show_vertical_top_right) {
    vertical_radii->Append(top_right_radius->Item(1));
  }
  if (show_vertical_bottom_right) {
    vertical_radii->Append(bottom_right_radius->Item(1));
  }
  if (show_vertical_bottom_left) {
    vertical_radii->Append(bottom_left_radius->Item(1));
  }

  if (!vertical_radii->Equals(To<CSSValueList>(list->Item(0)))) {
    list->Append(*vertical_radii);
  }

  return list;
}

CSSValue* ComputedStyleUtils::StrokeDashArrayToCSSValueList(
    const SVGDashArray& dashes,
    const ComputedStyle& style) {
  if (dashes.data.empty()) {
    return CSSIdentifierValue::Create(CSSValueID::kNone);
  }

  CSSValueList* list = CSSValueList::CreateCommaSeparated();
  for (const Length& dash_length : dashes.data) {
    list->Append(*ZoomAdjustedPixelValueForLength(dash_length, style));
  }

  return list;
}

const CSSValue* ComputedStyleUtils::ValueForSVGPaint(
    const SVGPaint& paint,
    const ComputedStyle& style) {
  switch (paint.type) {
    case SVGPaintType::kColor:
      return CurrentColorOrValidColor(style, paint.GetColor(),
                                      CSSValuePhase::kComputedValue);
    case SVGPaintType::kNone:
      return CSSIdentifierValue::Create(CSSValueID::kNone);
    case SVGPaintType::kUriNone:
    case SVGPaintType::kUriColor: {
      CSSValueList* values = CSSValueList::CreateSpaceSeparated();
      values->Append(*MakeGarbageCollected<cssvalue::CSSURIValue>(
          CSSUrlData(paint.GetUrl())));
      values->Append(
          paint.type == SVGPaintType::kUriNone
              ? *CSSIdentifierValue::Create(CSSValueID::kNone)
              : *CurrentColorOrValidColor(style, paint.GetColor(),
                                          CSSValuePhase::kComputedValue));
      return values;
    }
    case SVGPaintType::kUri:
      return MakeGarbageCollected<cssvalue::CSSURIValue>(
          CSSUrlData(paint.GetUrl()));
    case SVGPaintType::kContextFill:
      return CSSIdentifierValue::Create(CSSValueID::kContextFill);
    case SVGPaintType::kContextStroke:
      return CSSIdentifierValue::Create(CSSValueID::kContextStroke);
  }
}

CSSValue* ComputedStyleUtils::ValueForSVGResource(
    const StyleSVGResource* resource) {
  if (resource) {
    return MakeGarbageCollected<cssvalue::CSSURIValue>(
        CSSUrlData(resource->Url()));
  }
  return CSSIdentifierValue::Create(CSSValueID::kNone);
}

CSSValue* ComputedStyleUtils::ValueForShadowData(const ShadowData& shadow,
                                                 const ComputedStyle& style,
                                                 bool use_spread,
                                                 CSSValuePhase value_phase) {
  CSSPrimitiveValue* x = ZoomAdjustedPixelValue(shadow.X(), style);
  CSSPrimitiveValue* y = ZoomAdjustedPixelValue(shadow.Y(), style);
  CSSPrimitiveValue* blur = ZoomAdjustedPixelValue(shadow.Blur(), style);
  CSSPrimitiveValue* spread =
      use_spread ? ZoomAdjustedPixelValue(shadow.Spread(), style) : nullptr;
  CSSIdentifierValue* shadow_style =
      shadow.Style() == ShadowStyle::kNormal
          ? nullptr
          : CSSIdentifierValue::Create(CSSValueID::kInset);
  const CSSValue* color =
      CurrentColorOrValidColor(style, shadow.GetColor(), value_phase);
  return MakeGarbageCollected<CSSShadowValue>(x, y, blur, spread, shadow_style,
                                              color);
}

CSSValue* ComputedStyleUtils::ValueForShadowList(const ShadowList* shadow_list,
                                                 const ComputedStyle& style,
                                                 bool use_spread,
                                                 CSSValuePhase value_phase) {
  if (!shadow_list) {
    return CSSIdentifierValue::Create(CSSValueID::kNone);
  }

  CSSValueList* list = CSSValueList::CreateCommaSeparated();
  wtf_size_t shadow_count = shadow_list->Shadows().size();
  for (wtf_size_t i = 0; i < shadow_count; ++i) {
    list->Append(*ValueForShadowData(shadow_list->Shadows()[i], style,
                                     use_spread, value_phase));
  }
  return list;
}

CSSValue* ComputedStyleUtils::ValueForFilter(
    const ComputedStyle& style,
    const FilterOperations& filter_operations) {
  if (filter_operations.Operations().empty()) {
    return CSSIdentifierValue::Create(CSSValueID::kNone);
  }

  CSSValueList* list = CSSValueList::CreateSpaceSeparated();

  CSSFunctionValue* filter_value = nullptr;

  for (const auto& operation : filter_operations.Operations()) {
    FilterOperation* filter_operation = operation.Get();
    switch (filter_operation->GetType()) {
      case FilterOperation::OperationType::kReference:
        list->Append(*MakeGarbageCollected<cssvalue::CSSURIValue>(
            CSSUrlData(AtomicString(
                To<ReferenceFilterOperation>(filter_operation)->Url()))));
        continue;
      case FilterOperation::OperationType::kGrayscale:
        filter_value =
            MakeGarbageCollected<CSSFunctionValue>(CSSValueID::kGrayscale);
        filter_value->Append(*CSSNumericLiteralValue::Create(
            To<BasicColorMatrixFilterOperation>(filter_operation)->Amount(),
            CSSPrimitiveValue::UnitType::kNumber));
        break;
      case FilterOperation::OperationType::kSepia:
        filter_value =
            MakeGarbageCollected<CSSFunctionValue>(CSSValueID::kSepia);
        filter_value->Append(*CSSNumericLiteralValue::Create(
            To<BasicColorMatrixFilterOperation>(filter_operation)->Amount(),
            CSSPrimitiveValue::UnitType::kNumber));
        break;
      case FilterOperation::OperationType::kSaturate:
        filter_value =
            MakeGarbageCollected<CSSFunctionValue>(CSSValueID::kSaturate);
        filter_value->Append(*CSSNumericLiteralValue::Create(
            To<BasicColorMatrixFilterOperation>(filter_operation)->Amount(),
            CSSPrimitiveValue::UnitType::kNumber));
        break;
      case FilterOperation::OperationType::kHueRotate:
        filter_value =
            MakeGarbageCollected<CSSFunctionValue>(CSSValueID::kHueRotate);
        filter_value->Append(*CSSNumericLiteralValue::Create(
            To<BasicColorMatrixFilterOperation>(filter_operation)->Amount(),
            CSSPrimitiveValue::UnitType::kDegrees));
        break;
      case FilterOperation::OperationType::kInvert:
        filter_value =
            MakeGarbageCollected<CSSFunctionValue>(CSSValueID::kInvert);
        filter_value->Append(*CSSNumericLiteralValue::Create(
            To<BasicComponentTransferFilterOperation>(filter_operation)
                ->Amount(),
            CSSPrimitiveValue::UnitType::kNumber));
        break;
      case FilterOperation::OperationType::kOpacity:
        filter_value =
            MakeGarbageCollected<CSSFunctionValue>(CSSValueID::kOpacity);
        filter_value->Append(*CSSNumericLiteralValue::Create(
            To<BasicComponentTransferFilterOperation>(filter_operation)
                ->Amount(),
            CSSPrimitiveValue::UnitType::kNumber));
        break;
      case FilterOperation::OperationType::kBrightness:
        filter_value =
            MakeGarbageCollected<CSSFunctionValue>(CSSValueID::kBrightness);
        filter_value->Append(*CSSNumericLiteralValue::Create(
            To<BasicComponentTransferFilterOperation>(filter_operation)
                ->Amount(),
            CSSPrimitiveValue::UnitType::kNumber));
        break;
      case FilterOperation::OperationType::kContrast:
        filter_value =
            MakeGarbageCollected<CSSFunctionValue>(CSSValueID::kContrast);
        filter_value->Append(*CSSNumericLiteralValue::Create(
            To<BasicComponentTransferFilterOperation>(filter_operation)
                ->Amount(),
            CSSPrimitiveValue::UnitType::kNumber));
        break;
      case FilterOperation::OperationType::kBlur:
        filter_value =
            MakeGarbageCollected<CSSFunctionValue>(CSSValueID::kBlur);
        filter_value->Append(*ZoomAdjustedPixelValue(
            To<BlurFilterOperation>(filter_operation)->StdDeviation().Value(),
            style));
        break;
      case FilterOperation::OperationType::kDropShadow: {
        const auto& drop_shadow_operation =
            To<DropShadowFilterOperation>(*filter_operation);
        filter_value =
            MakeGarbageCollected<CSSFunctionValue>(CSSValueID::kDropShadow);
        // We want our computed style to look like that of a text shadow (has
        // neither spread nor inset style).
        filter_value->Append(
            *ValueForShadowData(drop_shadow_operation.Shadow(), style, false,
                                CSSValuePhase::kComputedValue));
        break;
      }
      default:
        NOTREACHED_IN_MIGRATION();
        break;
    }
    list->Append(*filter_value);
  }

  return list;
}

CSSValue* ComputedStyleUtils::ValueForScrollSnapType(
    const cc::ScrollSnapType& type,
    const ComputedStyle& style) {
  if (!type.is_none) {
    if (type.strictness == cc::SnapStrictness::kProximity) {
      return CSSIdentifierValue::Create(type.axis);
    }
    return MakeGarbageCollected<CSSValuePair>(
        CSSIdentifierValue::Create(type.axis),
        CSSIdentifierValue::Create(type.strictness),
        CSSValuePair::kDropIdenticalValues);
  }
  return CSSIdentifierValue::Create(CSSValueID::kNone);
}

CSSValue* ComputedStyleUtils::ValueForScrollSnapAlign(
    const cc::ScrollSnapAlign& align,
    const ComputedStyle& style) {
  return MakeGarbageCollected<CSSValuePair>(
      CSSIdentifierValue::Create(align.alignment_block),
      CSSIdentifierValue::Create(align.alignment_inline),
      CSSValuePair::kDropIdenticalValues);
}

// Returns a suitable value for the page-break-(before|after) property, given
// the computed value of the more general break-(before|after) property.
CSSValue* ComputedStyleUtils::ValueForPageBreakBetween(
    EBreakBetween break_value) {
  switch (break_value) {
    case EBreakBetween::kAvoidColumn:
    case EBreakBetween::kColumn:
    case EBreakBetween::kRecto:
    case EBreakBetween::kVerso:
    case EBreakBetween::kAvoidPage:
      return nullptr;
    case EBreakBetween::kPage:
      return CSSIdentifierValue::Create(CSSValueID::kAlways);
    default:
      return CSSIdentifierValue::Create(break_value);
  }
}

// Returns a suitable value for the -webkit-column-break-(before|after)
// property, given the computed value of the more general break-(before|after)
// property.
CSSValue* ComputedStyleUtils::ValueForWebkitColumnBreakBetween(
    EBreakBetween break_value) {
  switch (break_value) {
    case EBreakBetween::kAvoidPage:
    case EBreakBetween::kLeft:
    case EBreakBetween::kPage:
    case EBreakBetween::kRecto:
    case EBreakBetween::kRight:
    case EBreakBetween::kVerso:
      return nullptr;
    case EBreakBetween::kColumn:
      return CSSIdentifierValue::Create(CSSValueID::kAlways);
    case EBreakBetween::kAvoidColumn:
      return CSSIdentifierValue::Create(CSSValueID::kAvoid);
    default:
      return CSSIdentifierValue::Create(break_value);
  }
}

// Returns a suitable value for the page-break-inside property, given the
// computed value of the more general break-inside property.
CSSValue* ComputedStyleUtils::ValueForPageBreakInside(
    EBreakInside break_value) {
  switch (break_value) {
    case EBreakInside::kAvoidColumn:
      return nullptr;
    case EBreakInside::kAvoidPage:
      return CSSIdentifierValue::Create(CSSValueID::kAvoid);
    default:
      return CSSIdentifierValue::Create(break_value);
  }
}

// Returns a suitable value for the -webkit-column-break-inside property, given
// the computed value of the more general break-inside property.
CSSValue* ComputedStyleUtils::ValueForWebkitColumnBreakInside(
    EBreakInside break_value) {
  switch (break_value) {
    case EBreakInside::kAvoidPage:
      return nullptr;
    case EBreakInside::kAvoidColumn:
      return CSSIdentifierValue::Create(CSSValueID::kAvoid);
    default:
      return CSSIdentifierValue::Create(break_value);
  }
}

// https://drafts.csswg.org/cssom/#resolved-value
//
// For 'width' and 'height':
//
// If the property applies to the element or pseudo-element and the resolved
// value of the display property is not none or contents, then the resolved
// value is the used value. Otherwise the resolved value is the computed value
// (https://drafts.csswg.org/css-cascade-4/#computed-value).
//
// (Note that the computed value exists even when the property does not apply.)
bool ComputedStyleUtils::WidthOrHeightShouldReturnUsedValue(
    const LayoutObject* object) {
  // The display property is 'none'.
  if (!object) {
    return false;
  }
  // Non-root SVG objects return the resolved value except <image>,
  // <rect> and <foreignObject> which return the used value.
  if (object->IsSVGChild()) {
    return IsSVGObjectWithWidthAndHeight(*object);
  }
  // According to
  // http://www.w3.org/TR/CSS2/visudet.html#the-width-property and
  // http://www.w3.org/TR/CSS2/visudet.html#the-height-property, the "width" or
  // "height" property does not apply to non-atomic inline elements.
  return object->IsAtomicInlineLevel() || !object->IsInline();
}

CSSValueList* ComputedStyleUtils::ValuesForShorthandProperty(
    const StylePropertyShorthand& shorthand,
    const ComputedStyle& style,
    const LayoutObject* layout_object,
    bool allow_visited_style,
    CSSValuePhase value_phase) {
  CSSValueList* list = CSSValueList::CreateSpaceSeparated();
  for (const CSSProperty* const longhand : shorthand.properties()) {
    const CSSValue* value = longhand->CSSValueFromComputedStyle(
        style, layout_object, allow_visited_style, value_phase);
    DCHECK(value);
    list->Append(*value);
  }
  return list;
}

CSSValuePair* ComputedStyleUtils::ValuesForGapShorthand(
    const StylePropertyShorthand& shorthand,
    const ComputedStyle& style,
    const LayoutObject* layout_object,
    bool allow_visited_style,
    CSSValuePhase value_phase) {
  const CSSValue* row_gap_value =
      shorthand.properties()[0]->CSSValueFromComputedStyle(
          style, layout_object, allow_visited_style, value_phase);
  const CSSValue* column_gap_value =
      shorthand.properties()[1]->CSSValueFromComputedStyle(
          style, layout_object, allow_visited_style, value_phase);

  return MakeGarbageCollected<CSSValuePair>(row_gap_value, column_gap_value,
                                            CSSValuePair::kDropIdenticalValues);
}

CSSValueList* ComputedStyleUtils::ValuesForGridShorthand(
    const StylePropertyShorthand& shorthand,
    const ComputedStyle& style,
    const LayoutObject* layout_object,
    bool allow_visited_style,
    CSSValuePhase value_phase) {
  // Trailing non-initial values should be dropped.
  unsigned last_index = shorthand.length();
  // Work backwards to determine the final non-initial index. For grid
  // shorthands, we can drop all trailing `none` and `auto` values.
  for (; last_index > 1; --last_index) {
    const CSSValue* value =
        shorthand.properties()[last_index - 1]->CSSValueFromComputedStyle(
            style, layout_object, allow_visited_style, value_phase);
    if ((!IsA<CSSIdentifierValue>(value) ||
         (To<CSSIdentifierValue>(value)->GetValueID() != CSSValueID::kNone))) {
      break;
    }
  }

  CSSValueList* list = CSSValueList::CreateSlashSeparated();
  for (unsigned i = 0; i < last_index; ++i) {
    const CSSValue* value =
        shorthand.properties()[i]->CSSValueFromComputedStyle(
            style, layout_object, allow_visited_style, value_phase);
    DCHECK(value);
    list->Append(*value);
  }
  return list;
}

CSSValueList* ComputedStyleUtils::ValuesForGridAreaShorthand(
    const StylePropertyShorthand& shorthand,
    const ComputedStyle& style,
    const LayoutObject* layout_object,
    bool allow_visited_style,
    CSSValuePhase value_phase) {
  DCHECK_EQ(shorthand.length(), 4u);

  const CSSValue* grid_row_start =
      shorthand.properties()[0]->CSSValueFromComputedStyle(
          style, layout_object, allow_visited_style, value_phase);
  const CSSValue* grid_column_start =
      shorthand.properties()[1]->CSSValueFromComputedStyle(
          style, layout_object, allow_visited_style, value_phase);
  const CSSValue* grid_row_end =
      shorthand.properties()[2]->CSSValueFromComputedStyle(
          style, layout_object, allow_visited_style, value_phase);
  const CSSValue* grid_column_end =
      shorthand.properties()[3]->CSSValueFromComputedStyle(
          style, layout_object, allow_visited_style, value_phase);

  // `grid-row-end` depends on `grid-row-start`, and `grid-column-end` depends
  // on on `grid-column-start`, but what's not consistent is that
  // `grid-column-start` has a dependency on `grid-row-start`. For more details,
  // see https://www.w3.org/TR/css-grid-2/#placement-shorthands
  const bool include_column_start =
      CSSOMUtils::IncludeDependentGridLineEndValue(grid_row_start,
                                                   grid_column_start);
  const bool include_row_end = CSSOMUtils::IncludeDependentGridLineEndValue(
      grid_row_start, grid_row_end);
  const bool include_column_end = CSSOMUtils::IncludeDependentGridLineEndValue(
      grid_column_start, grid_column_end);

  CSSValueList* list = CSSValueList::CreateSlashSeparated();

  // `grid-row-start` is always included.
  list->Append(*grid_row_start);

  // If `IncludeDependentGridLineEndValue` returns true for a property,
  // all preceding values must be included.
  if (include_column_start || include_row_end || include_column_end) {
    list->Append(*grid_column_start);
  }
  if (include_row_end || include_column_end) {
    list->Append(*grid_row_end);
  }
  if (include_column_end) {
    list->Append(*grid_column_end);
  }

  return list;
}

CSSValueList* ComputedStyleUtils::ValuesForGridLineShorthand(
    const StylePropertyShorthand& shorthand,
    const ComputedStyle& style,
    const LayoutObject* layout_object,
    bool allow_visited_style,
    CSSValuePhase value_phase) {
  DCHECK_EQ(shorthand.length(), 2u);

  const CSSValue* line_start =
      shorthand.properties()[0]->CSSValueFromComputedStyle(
          style, layout_object, allow_visited_style, value_phase);
  const CSSValue* line_end =
      shorthand.properties()[1]->CSSValueFromComputedStyle(
          style, layout_object, allow_visited_style, value_phase);
  CSSValueList* list = CSSValueList::CreateSlashSeparated();

  // `grid-line-start` is always included.
  list->Append(*line_start);
  if (CSSOMUtils::IncludeDependentGridLineEndValue(line_start, line_end)) {
    list->Append(*line_end);
  }

  return list;
}

CSSValueList* ComputedStyleUtils::ValuesForGridTemplateShorthand(
    const StylePropertyShorthand& shorthand,
    const ComputedStyle& style,
    const LayoutObject* layout_object,
    bool allow_visited_style,
    CSSValuePhase value_phase) {
  DCHECK_EQ(shorthand.length(), 3u);

  // "Note: In general, resolved values are the computed values, except for a
  // small list of legacy 2.1 properties. However, compatibility with early
  // implementations of this module requires us to define grid-template-rows and
  // grid-template-columns as returning used values."
  //
  // https://www.w3.org/TR/css-grid-2/#resolved-track-list-standalone
  //
  // For `grid-template`, this doesn't apply, so we shouldn't be returning used
  // values. The following method mostly mirrors
  // `StylePropertySerializer::GetShorthandValueForGridTemplate`, except it
  // produces a `CSSValueList` instead of a String.
  const CSSValue* template_rows_computed =
      ValueForGridTrackList(kForRows, layout_object, style,
                            /* force_computed_values */ true);
  const CSSValue* template_columns_computed =
      ValueForGridTrackList(kForColumns, layout_object, style,
                            /* force_computed_values */ true);

  const CSSValue* template_row_values =
      shorthand.properties()[0]->CSSValueFromComputedStyle(
          style, layout_object, allow_visited_style, value_phase);
  const CSSValue* template_column_values =
      shorthand.properties()[1]->CSSValueFromComputedStyle(
          style, layout_object, allow_visited_style, value_phase);
  const CSSValue* template_area_values =
      shorthand.properties()[2]->CSSValueFromComputedStyle(
          style, layout_object, allow_visited_style, value_phase);

  // Implicit tracks will generate an empty list from `ValueForGridTrackList`,
  // as they don't create repeaters. In this case, they will already be
  // equivalent to the expected computed value (since implicit tracks don't
  // generate repeaters and are always fixed sizes). So in that case, we can
  // simply use the values directly from the shorthand.
  return CSSOMUtils::ComputedValueForGridTemplateShorthand(
      CSSOMUtils::IsEmptyValueList(template_rows_computed)
          ? template_row_values
          : template_rows_computed,
      CSSOMUtils::IsEmptyValueList(template_columns_computed)
          ? template_column_values
          : template_columns_computed,
      template_area_values);
}

CSSValueList* ComputedStyleUtils::ValuesForSidesShorthand(
    const StylePropertyShorthand& shorthand,
    const ComputedStyle& style,
    const LayoutObject* layout_object,
    bool allow_visited_style,
    CSSValuePhase value_phase) {
  CSSValueList* list = CSSValueList::CreateSpaceSeparated();
  // Assume the properties are in the usual order top, right, bottom, left.
  const CSSValue* top_value =
      shorthand.properties()[0]->CSSValueFromComputedStyle(
          style, layout_object, allow_visited_style, value_phase);
  const CSSValue* right_value =
      shorthand.properties()[1]->CSSValueFromComputedStyle(
          style, layout_object, allow_visited_style, value_phase);
  const CSSValue* bottom_value =
      shorthand.properties()[2]->CSSValueFromComputedStyle(
          style, layout_object, allow_visited_style, value_phase);
  const CSSValue* left_value =
      shorthand.properties()[3]->CSSValueFromComputedStyle(
          style, layout_object, allow_visited_style, value_phase);

  // All 4 properties must be specified.
  if (!top_value || !right_value || !bottom_value || !left_value) {
    return nullptr;
  }

  bool show_left = !base::ValuesEquivalent(right_value, left_value);
  bool show_bottom =
      !base::ValuesEquivalent(top_value, bottom_value) || show_left;
  bool show_right =
      !base::ValuesEquivalent(top_value, right_value) || show_bottom;

  list->Append(*top_value);
  if (show_right) {
    list->Append(*right_value);
  }
  if (show_bottom) {
    list->Append(*bottom_value);
  }
  if (show_left) {
    list->Append(*left_value);
  }

  return list;
}

CSSValuePair* ComputedStyleUtils::ValuesForInlineBlockShorthand(
    const StylePropertyShorthand& shorthand,
    const ComputedStyle& style,
    const LayoutObject* layout_object,
    bool allow_visited_style,
    CSSValuePhase value_phase) {
  const CSSValue* start_value =
      shorthand.properties()[0]->CSSValueFromComputedStyle(
          style, layout_object, allow_visited_style, value_phase);
  const CSSValue* end_value =
      shorthand.properties()[1]->CSSValueFromComputedStyle(
          style, layout_object, allow_visited_style, value_phase);
  // Both properties must be specified.
  if (!start_value || !end_value) {
    return nullptr;
  }

  auto* pair = MakeGarbageCollected<CSSValuePair>(
      start_value, end_value, CSSValuePair::kDropIdenticalValues);
  return pair;
}

CSSValuePair* ComputedStyleUtils::ValuesForPlaceShorthand(
    const StylePropertyShorthand& shorthand,
    const ComputedStyle& style,
    const LayoutObject* layout_object,
    bool allow_visited_style,
    CSSValuePhase value_phase) {
  const CSSValue* align_value =
      shorthand.properties()[0]->CSSValueFromComputedStyle(
          style, layout_object, allow_visited_style, value_phase);
  const CSSValue* justify_value =
      shorthand.properties()[1]->CSSValueFromComputedStyle(
          style, layout_object, allow_visited_style, value_phase);

  return MakeGarbageCollected<CSSValuePair>(align_value, justify_value,
                                            CSSValuePair::kDropIdenticalValues);
}

static CSSValue* ExpandNoneLigaturesValue() {
  CSSValueList* list = CSSValueList::CreateSpaceSeparated();
  list->Append(*CSSIdentifierValue::Create(CSSValueID::kNoCommonLigatures));
  list->Append(
      *CSSIdentifierValue::Create(CSSValueID::kNoDiscretionaryLigatures));
  list->Append(*CSSIdentifierValue::Create(CSSValueID::kNoHistoricalLigatures));
  list->Append(*CSSIdentifierValue::Create(CSSValueID::kNoContextual));
  return list;
}

CSSValue* ComputedStyleUtils::ValuesForFontVariantProperty(
    const ComputedStyle& style,
    const LayoutObject* layout_object,
    bool allow_visited_style,
    CSSValuePhase value_phase) {
  enum VariantShorthandCases {
    kAllNormal,
    kNoneLigatures,
    kConcatenateNonNormal,
    kEmptyString
  };
  StylePropertyShorthand shorthand = fontVariantShorthand();
  VariantShorthandCases shorthand_case = kAllNormal;
  for (const CSSProperty* const longhand : shorthand.properties()) {
    const CSSValue* value = longhand->CSSValueFromComputedStyle(
        style, layout_object, allow_visited_style, value_phase);

    auto* identifier_value = DynamicTo<CSSIdentifierValue>(value);
    if (shorthand_case == kAllNormal && identifier_value &&
        identifier_value->GetValueID() == CSSValueID::kNone &&
        longhand->IDEquals(CSSPropertyID::kFontVariantLigatures)) {
      shorthand_case = kNoneLigatures;
    } else if (!(identifier_value &&
                 identifier_value->GetValueID() == CSSValueID::kNormal)) {
      shorthand_case = shorthand_case == kNoneLigatures ? kEmptyString
                                                        : kConcatenateNonNormal;
      break;
    }
  }

  switch (shorthand_case) {
    case kAllNormal:
      return CSSIdentifierValue::Create(CSSValueID::kNormal);
    case kNoneLigatures:
      return CSSIdentifierValue::Create(CSSValueID::kNone);
    case kConcatenateNonNormal: {
      CSSValueList* list = CSSValueList::CreateSpaceSeparated();
      for (const CSSProperty* const longhand : shorthand.properties()) {
        const CSSValue* value = longhand->CSSValueFromComputedStyle(
            style, layout_object, allow_visited_style, value_phase);
        DCHECK(value);
        auto* identifier_value = DynamicTo<CSSIdentifierValue>(value);
        if (identifier_value &&
            identifier_value->GetValueID() == CSSValueID::kNone) {
          list->Append(*ExpandNoneLigaturesValue());
        } else if (!(identifier_value &&
                     identifier_value->GetValueID() == CSSValueID::kNormal)) {
          list->Append(*value);
        }
      }
      return list;
    }
    case kEmptyString:
      return nullptr;
    default:
      NOTREACHED_IN_MIGRATION();
      return nullptr;
  }
}

CSSValue* ComputedStyleUtils::ValuesForFontSynthesisProperty(
    const ComputedStyle& style,
    const LayoutObject* layout_object,
    bool allow_visited_style,
    CSSValuePhase value_phase) {
  enum FontSynthesisShorthandCases { kAllNone, kConcatenateAuto };
  StylePropertyShorthand shorthand = fontSynthesisShorthand();
  FontSynthesisShorthandCases shorthand_case = kAllNone;
  for (const CSSProperty* const longhand : shorthand.properties()) {
    const CSSValue* value = longhand->CSSValueFromComputedStyle(
        style, layout_object, allow_visited_style, value_phase);
    auto* identifier_value = DynamicTo<CSSIdentifierValue>(value);
    if (longhand->IDEquals(CSSPropertyID::kFontSynthesisWeight) &&
        identifier_value->GetValueID() == CSSValueID::kAuto) {
      shorthand_case = kConcatenateAuto;
    } else if (longhand->IDEquals(CSSPropertyID::kFontSynthesisStyle) &&
               identifier_value->GetValueID() == CSSValueID::kAuto) {
      shorthand_case = kConcatenateAuto;
    } else if (longhand->IDEquals(CSSPropertyID::kFontSynthesisSmallCaps) &&
               identifier_value->GetValueID() == CSSValueID::kAuto) {
      shorthand_case = kConcatenateAuto;
    }
  }

  switch (shorthand_case) {
    case kAllNone:
      return CSSIdentifierValue::Create(CSSValueID::kNone);
    case kConcatenateAuto: {
      CSSValueList* list = CSSValueList::CreateSpaceSeparated();
      for (const CSSProperty* const longhand : shorthand.properties()) {
        const CSSValue* value = longhand->CSSValueFromComputedStyle(
            style, layout_object, allow_visited_style, value_phase);
        auto* identifier_value = DynamicTo<CSSIdentifierValue>(value);
        if (longhand->IDEquals(CSSPropertyID::kFontSynthesisWeight) &&
            identifier_value->GetValueID() == CSSValueID::kAuto) {
          list->Append(*CSSIdentifierValue::Create(CSSValueID::kWeight));
        } else if (longhand->IDEquals(CSSPropertyID::kFontSynthesisStyle) &&
                   identifier_value->GetValueID() == CSSValueID::kAuto) {
          list->Append(*CSSIdentifierValue::Create(CSSValueID::kStyle));
        } else if (longhand->IDEquals(CSSPropertyID::kFontSynthesisSmallCaps) &&
                   identifier_value->GetValueID() == CSSValueID::kAuto) {
          list->Append(*CSSIdentifierValue::Create(CSSValueID::kSmallCaps));
        }
      }
      return list;
    }
    default:
      NOTREACHED_IN_MIGRATION();
      return nullptr;
  }
}

CSSValueList* ComputedStyleUtils::ValuesForContainerShorthand(
    const ComputedStyle& style,
    const LayoutObject* layout_object,
    bool allow_visited_style,
    CSSValuePhase value_phase) {
  CHECK_EQ(containerShorthand().length(), 2u);
  CHECK_EQ(containerShorthand().properties()[0],
           &GetCSSPropertyContainerName());
  CHECK_EQ(containerShorthand().properties()[1],
           &GetCSSPropertyContainerType());

  CSSValueList* list = CSSValueList::CreateSlashSeparated();

  const CSSValue* name =
      GetCSSPropertyContainerName().CSSValueFromComputedStyle(
          style, layout_object, allow_visited_style, value_phase);
  const CSSValue* type =
      GetCSSPropertyContainerType().CSSValueFromComputedStyle(
          style, layout_object, allow_visited_style, value_phase);

  DCHECK(name);
  DCHECK(type);

  list->Append(*name);

  if (const auto* ident_value = DynamicTo<CSSIdentifierValue>(*type);
      !ident_value || ident_value->GetValueID() != CSSValueID::kNormal) {
    list->Append(*type);
  }

  return list;
}

CSSValue* ComputedStyleUtils::ValueForGapLength(
    const std::optional<Length>& gap_length,
    const ComputedStyle& style) {
  if (!gap_length) {
    return CSSIdentifierValue::Create(CSSValueID::kNormal);
  }
  return ZoomAdjustedPixelValueForLength(*gap_length, style);
}

CSSValue* ComputedStyleUtils::ValueForStyleName(const StyleName& name) {
  if (name.IsCustomIdent()) {
    return MakeGarbageCollected<CSSCustomIdentValue>(name.GetValue());
  }
  return MakeGarbageCollected<CSSStringValue>(name.GetValue());
}

CSSValue* ComputedStyleUtils::ValueForStyleNameOrKeyword(
    const StyleNameOrKeyword& value) {
  if (value.IsKeyword()) {
    return CSSIdentifierValue::Create(value.GetKeyword());
  }
  return ValueForStyleName(value.GetName());
}

CSSValue* ComputedStyleUtils::ValueForCustomIdentOrNone(
    const AtomicString& ident) {
  if (ident.empty()) {
    return CSSIdentifierValue::Create(CSSValueID::kNone);
  }
  return MakeGarbageCollected<CSSCustomIdentValue>(ident);
}

CSSValue* ComputedStyleUtils::ValueForCustomIdentOrNone(
    const ScopedCSSName* name) {
  return ValueForCustomIdentOrNone(name ? name->GetName() : g_null_atom);
}

const CSSValue* ComputedStyleUtils::ValueForStyleAutoColor(
    const ComputedStyle& style,
    const StyleAutoColor& color,
    CSSValuePhase value_phase) {
  if (color.IsAutoColor()) {
    return cssvalue::CSSColor::Create(StyleColor::CurrentColor().Resolve(
        style.GetCurrentColor(), style.UsedColorScheme()));
  }
  return ComputedStyleUtils::CurrentColorOrValidColor(
      style, color.ToStyleColor(), value_phase);
}

CSSValue* ComputedStyleUtils::ValueForIntrinsicLength(
    const ComputedStyle& style,
    const StyleIntrinsicLength& intrinsic_length) {
  if (intrinsic_length.IsNoOp()) {
    return CSSIdentifierValue::Create(CSSValueID::kNone);
  }

  CSSValueList* list = CSSValueList::CreateSpaceSeparated();
  if (intrinsic_length.HasAuto()) {
    list->Append(*CSSIdentifierValue::Create(CSSValueID::kAuto));
  }

  if (const std::optional<Length>& length = intrinsic_length.GetLength()) {
    list->Append(*ZoomAdjustedPixelValueForLength(*length, style));
  } else {
    list->Append(*CSSIdentifierValue::Create(CSSValueID::kNone));
  }
  return list;
}

CSSValue* ComputedStyleUtils::ValueForScrollStart(const ComputedStyle& style,
                                                  const ScrollStartData& data) {
  if (data.value_type == ScrollStartValueType::kLengthOrPercentage) {
    return ComputedStyleUtils::ZoomAdjustedPixelValueForLength(data.value,
                                                               style);
  }
  return CSSIdentifierValue::Create(data.value_type);
}

namespace {

CSSIdentifierValue* PositionAreaSpanToCSSIdentifierValue(
    PositionAreaRegion span_start,
    PositionAreaRegion span_end) {
  if (span_start == span_end) {
    return CSSIdentifierValue::Create(span_start);
  }
  CHECK(span_start == PositionAreaRegion::kCenter ||
        span_end == PositionAreaRegion::kCenter);
  PositionAreaRegion span_towards =
      span_start == PositionAreaRegion::kCenter ? span_end : span_start;
  CSSValueID value_id = CSSValueID::kSpanAll;
  switch (span_towards) {
    case PositionAreaRegion::kLeft:
      value_id = CSSValueID::kSpanLeft;
      break;
    case PositionAreaRegion::kRight:
      value_id = CSSValueID::kSpanRight;
      break;
    case PositionAreaRegion::kXStart:
      value_id = CSSValueID::kSpanXStart;
      break;
    case PositionAreaRegion::kXEnd:
      value_id = CSSValueID::kSpanXEnd;
      break;
    case PositionAreaRegion::kXSelfStart:
      value_id = CSSValueID::kSpanXSelfStart;
      break;
    case PositionAreaRegion::kXSelfEnd:
      value_id = CSSValueID::kSpanXSelfEnd;
      break;
    case PositionAreaRegion::kTop:
      value_id = CSSValueID::kSpanTop;
      break;
    case PositionAreaRegion::kBottom:
      value_id = CSSValueID::kSpanBottom;
      break;
    case PositionAreaRegion::kYStart:
      value_id = CSSValueID::kSpanYStart;
      break;
    case PositionAreaRegion::kYEnd:
      value_id = CSSValueID::kSpanYEnd;
      break;
    case PositionAreaRegion::kYSelfStart:
      value_id = CSSValueID::kSpanYSelfStart;
      break;
    case PositionAreaRegion::kYSelfEnd:
      value_id = CSSValueID::kSpanYSelfEnd;
      break;
    case PositionAreaRegion::kBlockStart:
      value_id = CSSValueID::kSpanBlockStart;
      break;
    case PositionAreaRegion::kBlockEnd:
      value_id = CSSValueID::kSpanBlockEnd;
      break;
    case PositionAreaRegion::kSelfBlockStart:
      value_id = CSSValueID::kSpanSelfBlockStart;
      break;
    case PositionAreaRegion::kSelfBlockEnd:
      value_id = CSSValueID::kSpanSelfBlockEnd;
      break;
    case PositionAreaRegion::kInlineStart:
      value_id = CSSValueID::kSpanInlineStart;
      break;
    case PositionAreaRegion::kInlineEnd:
      value_id = CSSValueID::kSpanInlineEnd;
      break;
    case PositionAreaRegion::kSelfInlineStart:
      value_id = CSSValueID::kSpanSelfInlineStart;
      break;
    case PositionAreaRegion::kSelfInlineEnd:
      value_id = CSSValueID::kSpanSelfInlineEnd;
      break;
    case PositionAreaRegion::kStart:
      value_id = CSSValueID::kSpanStart;
      break;
    case PositionAreaRegion::kEnd:
      value_id = CSSValueID::kSpanEnd;
      break;
    case PositionAreaRegion::kSelfStart:
      value_id = CSSValueID::kSpanSelfStart;
      break;
    case PositionAreaRegion::kSelfEnd:
      value_id = CSSValueID::kSpanSelfEnd;
      break;
    case PositionAreaRegion::kNone:
    case PositionAreaRegion::kAll:
    case PositionAreaRegion::kCenter:
      // Should have been handled above
      NOTREACHED_IN_MIGRATION();
      break;
  }
  return CSSIdentifierValue::Create(value_id);
}

}  // namespace

CSSValue* ComputedStyleUtils::ValueForPositionArea(
    const blink::PositionArea& area) {
  if (area.FirstStart() == PositionAreaRegion::kNone) {
    return CSSIdentifierValue::Create(CSSValueID::kNone);
  }
  CSSIdentifierValue* first_value =
      PositionAreaSpanToCSSIdentifierValue(area.FirstStart(), area.FirstEnd());
  CSSIdentifierValue* second_value = PositionAreaSpanToCSSIdentifierValue(
      area.SecondStart(), area.SecondEnd());

  CSSValueID second_default = CSSValueID::kSpanAll;
  CSSValueID first_value_id = first_value->GetValueID();

  if (css_parsing_utils::IsRepeatedPositionAreaValue(first_value_id)) {
    second_default = first_value_id;
  }
  if (second_value->GetValueID() == second_default) {
    return first_value;
  }
  return MakeGarbageCollected<CSSValuePair>(first_value, second_value,
                                            CSSValuePair::kDropIdenticalValues);
}

std::unique_ptr<CrossThreadStyleValue>
ComputedStyleUtils::CrossThreadStyleValueFromCSSStyleValue(
    CSSStyleValue* style_value) {
  switch (style_value->GetType()) {
    case CSSStyleValue::StyleValueType::kKeywordType:
      return std::make_unique<CrossThreadKeywordValue>(
          To<CSSKeywordValue>(style_value)->value());
    case CSSStyleValue::StyleValueType::kUnitType:
      return std::make_unique<CrossThreadUnitValue>(
          To<CSSUnitValue>(style_value)->value(),
          To<CSSUnitValue>(style_value)->GetInternalUnit());
    case CSSStyleValue::StyleValueType::kUnsupportedColorType:
      return std::make_unique<CrossThreadColorValue>(
          To<CSSUnsupportedColor>(style_value)->Value());
    case CSSStyleValue::StyleValueType::kUnparsedType:
      return std::make_unique<CrossThreadUnparsedValue>(
          To<CSSUnparsedValue>(style_value)->ToUnparsedString());
    default:
      return std::make_unique<CrossThreadUnsupportedValue>(
          style_value->toString());
  }
}

const CSSValue* ComputedStyleUtils::ComputedPropertyValue(
    const CSSProperty& property,
    const ComputedStyle& style,
    const LayoutObject* layout_object) {
  return property.CSSValueFromComputedStyle(style, layout_object, false,
                                            CSSValuePhase::kComputedValue);
}

CSSValue* ComputedStyleUtils::ValueForPositionTryFallbacks(
    const PositionTryFallbacks& fallbacks) {
  CSSValueList* fallback_list = CSSValueList::CreateCommaSeparated();
  for (const PositionTryFallback& fallback : fallbacks.GetFallbacks()) {
    if (!fallback.GetPositionArea().IsNone()) {
      if (RuntimeEnabledFeatures::CSSPositionAreaValueEnabled()) {
        // <position-area>
        fallback_list->Append(
            *ValueForPositionArea(fallback.GetPositionArea()));
      } else {
        // position-area( <position-area> )
        auto* function =
            MakeGarbageCollected<CSSFunctionValue>(CSSValueID::kPositionArea);
        function->Append(*ValueForPositionArea(fallback.GetPositionArea()));
        fallback_list->Append(*function);
      }
      continue;
    }
    // [<dashed-ident> || <try-tactic>]
    CSSValueList* fallback_value = CSSValueList::CreateSpaceSeparated();
    if (const ScopedCSSName* name = fallback.GetPositionTryName()) {
      fallback_value->Append(*MakeGarbageCollected<CSSCustomIdentValue>(*name));
    }
    const TryTacticList& tactic_list = fallback.GetTryTactic();
    for (TryTactic tactic : tactic_list) {
      if (tactic != TryTactic::kNone) {
        fallback_value->Append(*CSSIdentifierValue::Create(tactic));
      }
    }
    fallback_list->Append(*fallback_value);
  }
  return fallback_list;
}

}  // namespace blink
