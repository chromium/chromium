// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/properties/computed_style_utils.h"

#include "third_party/blink/renderer/core/css/basic_shape_functions.h"
#include "third_party/blink/renderer/core/css/css_alternate_value.h"
#include "third_party/blink/renderer/core/css/css_border_image.h"
#include "third_party/blink/renderer/core/css/css_border_image_slice_value.h"
#include "third_party/blink/renderer/core/css/css_bracketed_value_list.h"
#include "third_party/blink/renderer/core/css/css_color.h"
#include "third_party/blink/renderer/core/css/css_counter_value.h"
#include "third_party/blink/renderer/core/css/css_custom_ident_value.h"
#include "third_party/blink/renderer/core/css/css_font_family_value.h"
#include "third_party/blink/renderer/core/css/css_font_style_range_value.h"
#include "third_party/blink/renderer/core/css/css_function_value.h"
#include "third_party/blink/renderer/core/css/css_grid_auto_repeat_value.h"
#include "third_party/blink/renderer/core/css/css_grid_integer_repeat_value.h"
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
#include "third_party/blink/renderer/core/css/properties/css_parsing_utils.h"
#include "third_party/blink/renderer/core/css/properties/longhands.h"
#include "third_party/blink/renderer/core/css/properties/shorthands.h"
#include "third_party/blink/renderer/core/css/style_color.h"
#include "third_party/blink/renderer/core/layout/layout_block.h"
#include "third_party/blink/renderer/core/layout/layout_box.h"
#include "third_party/blink/renderer/core/layout/layout_grid.h"
#include "third_party/blink/renderer/core/layout/ng/grid/layout_ng_grid_interface.h"
#include "third_party/blink/renderer/core/layout/svg/transform_helper.h"
#include "third_party/blink/renderer/core/style/computed_style_constants.h"
#include "third_party/blink/renderer/core/style/style_intrinsic_length.h"
#include "third_party/blink/renderer/core/style/style_svg_resource.h"
#include "third_party/blink/renderer/core/style_property_shorthand.h"
#include "third_party/blink/renderer/core/svg_element_type_helpers.h"
#include "third_party/blink/renderer/platform/transforms/matrix_3d_transform_operation.h"
#include "third_party/blink/renderer/platform/transforms/matrix_transform_operation.h"
#include "third_party/blink/renderer/platform/transforms/perspective_transform_operation.h"
#include "third_party/blink/renderer/platform/transforms/skew_transform_operation.h"

namespace blink {

static Length Negate(const Length& length) {
  if (length.IsCalculated()) {
    NOTREACHED();
    return length;
  }

  Length ret =
      length.GetRoundToInt()
          ? Length(-static_cast<int>(length.GetFloatValue()), length.GetType())
          : Length(-length.GetFloatValue(), length.GetType());
  ret.SetQuirk(length.Quirk());
  return ret;
}

// TODO(rjwright): make this const
CSSValue* ComputedStyleUtils::ZoomAdjustedPixelValueForLength(
    const Length& length,
    const ComputedStyle& style) {
  if (length.IsFixed())
    return ZoomAdjustedPixelValue(length.Value(), style);
  return CSSValue::Create(length, style.EffectiveZoom());
}

CSSValue* ComputedStyleUtils::ValueForPosition(const LengthPoint& position,
                                               const ComputedStyle& style) {
  DCHECK_EQ(position.X().IsAuto(), position.Y().IsAuto());
  if (position.X().IsAuto())
    return CSSIdentifierValue::Create(CSSValueID::kAuto);

  return MakeGarbageCollected<CSSValuePair>(
      ZoomAdjustedPixelValueForLength(position.X(), style),
      ZoomAdjustedPixelValueForLength(position.Y(), style),
      CSSValuePair::kKeepIdenticalValues);
}

CSSValue* ComputedStyleUtils::ValueForOffset(const ComputedStyle& style,
                                             const LayoutObject* layout_object,
                                             bool allow_visited_style) {
  CSSValueList* list = CSSValueList::CreateSpaceSeparated();
  if (RuntimeEnabledFeatures::CSSOffsetPositionAnchorEnabled()) {
    CSSValue* position = ValueForPosition(style.OffsetPosition(), style);
    auto* position_identifier_value = DynamicTo<CSSIdentifierValue>(position);
    if (!position_identifier_value)
      list->Append(*position);
    else
      DCHECK(position_identifier_value->GetValueID() == CSSValueID::kAuto);
  }

  static const CSSProperty* longhands[3] = {&GetCSSPropertyOffsetPath(),
                                            &GetCSSPropertyOffsetDistance(),
                                            &GetCSSPropertyOffsetRotate()};
  for (const CSSProperty* longhand : longhands) {
    const CSSValue* value = longhand->CSSValueFromComputedStyle(
        style, layout_object, allow_visited_style);
    DCHECK(value);
    list->Append(*value);
  }

  if (RuntimeEnabledFeatures::CSSOffsetPositionAnchorEnabled()) {
    CSSValue* anchor = ValueForPosition(style.OffsetAnchor(), style);
    auto* anchor_identifier_value = DynamicTo<CSSIdentifierValue>(anchor);
    if (!anchor_identifier_value) {
      // Add a slash before anchor.
      CSSValueList* result = CSSValueList::CreateSlashSeparated();
      result->Append(*list);
      result->Append(*anchor);
      return result;
    }
    DCHECK(anchor_identifier_value->GetValueID() == CSSValueID::kAuto);
  }
  return list;
}

CSSValue* ComputedStyleUtils::CurrentColorOrValidColor(
    const ComputedStyle& style,
    const StyleColor& color,
    CSSValuePhase value_phase) {
  return cssvalue::CSSColor::Create(
      color.Resolve(style.GetCurrentColor(), style.UsedColorScheme()));
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

const CSSValue* ComputedStyleUtils::BackgroundImageOrWebkitMaskImage(
    const ComputedStyle& style,
    bool allow_visited_style,
    const FillLayer& fill_layer) {
  CSSValueList* list = CSSValueList::CreateCommaSeparated();
  const FillLayer* curr_layer = &fill_layer;
  for (; curr_layer; curr_layer = curr_layer->Next()) {
    if (curr_layer->GetImage()) {
      list->Append(*curr_layer->GetImage()->ComputedCSSValue(
          style, allow_visited_style));
    } else {
      list->Append(*CSSIdentifierValue::Create(CSSValueID::kNone));
    }
  }
  return list;
}

const CSSValue* ComputedStyleUtils::ValueForFillSize(
    const FillSize& fill_size,
    const ComputedStyle& style) {
  if (fill_size.type == EFillSizeType::kContain)
    return CSSIdentifierValue::Create(CSSValueID::kContain);

  if (fill_size.type == EFillSizeType::kCover)
    return CSSIdentifierValue::Create(CSSValueID::kCover);

  if (fill_size.size.Height().IsAuto()) {
    return ZoomAdjustedPixelValueForLength(fill_size.size.Width(), style);
  }

  return MakeGarbageCollected<CSSValuePair>(
      ZoomAdjustedPixelValueForLength(fill_size.size.Width(), style),
      ZoomAdjustedPixelValueForLength(fill_size.size.Height(), style),
      CSSValuePair::kKeepIdenticalValues);
}

const CSSValue* ComputedStyleUtils::BackgroundImageOrWebkitMaskSize(
    const ComputedStyle& style,
    const FillLayer& fill_layer) {
  CSSValueList* list = CSSValueList::CreateCommaSeparated();
  const FillLayer* curr_layer = &fill_layer;
  for (; curr_layer; curr_layer = curr_layer->Next())
    list->Append(*ValueForFillSize(curr_layer->Size(), style));
  return list;
}

const CSSValueList* ComputedStyleUtils::CreatePositionListForLayer(
    const CSSProperty& property,
    const FillLayer& layer,
    const ComputedStyle& style) {
  CSSValueList* position_list = CSSValueList::CreateSpaceSeparated();
  if (layer.IsBackgroundXOriginSet()) {
    DCHECK(property.IDEquals(CSSPropertyID::kBackgroundPosition) ||
           property.IDEquals(CSSPropertyID::kWebkitMaskPosition));
    position_list->Append(
        *CSSIdentifierValue::Create(layer.BackgroundXOrigin()));
  }
  position_list->Append(
      *ZoomAdjustedPixelValueForLength(layer.PositionX(), style));
  if (layer.IsBackgroundYOriginSet()) {
    DCHECK(property.IDEquals(CSSPropertyID::kBackgroundPosition) ||
           property.IDEquals(CSSPropertyID::kWebkitMaskPosition));
    position_list->Append(
        *CSSIdentifierValue::Create(layer.BackgroundYOrigin()));
  }
  position_list->Append(
      *ZoomAdjustedPixelValueForLength(layer.PositionY(), style));
  return position_list;
}

const CSSValue* ComputedStyleUtils::ValueForFillRepeat(EFillRepeat x_repeat,
                                                       EFillRepeat y_repeat) {
  // For backwards compatibility, if both values are equal, just return one of
  // them. And if the two values are equivalent to repeat-x or repeat-y, just
  // return the shorthand.
  if (x_repeat == y_repeat)
    return CSSIdentifierValue::Create(x_repeat);
  if (x_repeat == EFillRepeat::kRepeatFill &&
      y_repeat == EFillRepeat::kNoRepeatFill)
    return CSSIdentifierValue::Create(CSSValueID::kRepeatX);
  if (x_repeat == EFillRepeat::kNoRepeatFill &&
      y_repeat == EFillRepeat::kRepeatFill)
    return CSSIdentifierValue::Create(CSSValueID::kRepeatY);

  CSSValueList* list = CSSValueList::CreateSpaceSeparated();
  list->Append(*CSSIdentifierValue::Create(x_repeat));
  list->Append(*CSSIdentifierValue::Create(y_repeat));
  return list;
}

const CSSValueList* ComputedStyleUtils::ValuesForBackgroundShorthand(
    const ComputedStyle& style,
    const LayoutObject* layout_object,
    bool allow_visited_style) {
  CSSValueList* result = CSSValueList::CreateCommaSeparated();
  const FillLayer* curr_layer = &style.BackgroundLayers();
  for (; curr_layer; curr_layer = curr_layer->Next()) {
    CSSValueList* list = CSSValueList::CreateSlashSeparated();
    CSSValueList* before_slash = CSSValueList::CreateSpaceSeparated();
    if (!curr_layer->Next()) {  // color only for final layer
      const CSSValue* value =
          GetCSSPropertyBackgroundColor().CSSValueFromComputedStyle(
              style, layout_object, allow_visited_style);
      DCHECK(value);
      before_slash->Append(*value);
    }
    before_slash->Append(curr_layer->GetImage()
                             ? *curr_layer->GetImage()->ComputedCSSValue(
                                   style, allow_visited_style)
                             : *CSSIdentifierValue::Create(CSSValueID::kNone));
    before_slash->Append(
        *ValueForFillRepeat(curr_layer->RepeatX(), curr_layer->RepeatY()));
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

const CSSValue* ComputedStyleUtils::BackgroundRepeatOrWebkitMaskRepeat(
    const FillLayer* curr_layer) {
  CSSValueList* list = CSSValueList::CreateCommaSeparated();
  for (; curr_layer; curr_layer = curr_layer->Next()) {
    list->Append(
        *ValueForFillRepeat(curr_layer->RepeatX(), curr_layer->RepeatY()));
  }
  return list;
}

const CSSValue* ComputedStyleUtils::BackgroundPositionOrWebkitMaskPosition(
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
  // TODO(alancutter): Make this code aware of calc lengths.
  return CSSNumericLiteralValue::Create(
      slice.Value(), slice.IsPercentOrCalc()
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

      if (slices.Left() == slices.Right())
        left = right;
      else
        left = ValueForImageSlice(slices.Left());
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

      if (box.Left() == box.Right())
        left = right;
      else
        left = ValueForBorderImageLength(box.Left(), style);
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
    bool allow_visited_style) {
  if (!image.HasImage())
    return CSSIdentifierValue::Create(CSSValueID::kNone);

  // Image first.
  CSSValue* image_value = nullptr;
  if (image.GetImage()) {
    image_value =
        image.GetImage()->ComputedCSSValue(style, allow_visited_style);
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
    bool allow_visited_style) {
  if (!reflection)
    return CSSIdentifierValue::Create(CSSValueID::kNone);

  CSSPrimitiveValue* offset = nullptr;
  // TODO(alancutter): Make this work correctly for calc lengths.
  if (reflection->Offset().IsPercent()) {
    offset = CSSNumericLiteralValue::Create(
        reflection->Offset().Percent(),
        CSSPrimitiveValue::UnitType::kPercentage);
  } else if (reflection->Offset().IsCalculated()) {
    offset = CSSMathFunctionValue::Create(reflection->Offset(),
                                          style.EffectiveZoom());
  } else {
    offset = ZoomAdjustedPixelValue(reflection->Offset().Value(), style);
  }

  CSSIdentifierValue* direction = nullptr;
  switch (reflection->Direction()) {
    case kReflectionBelow:
      direction = CSSIdentifierValue::Create(CSSValueID::kBelow);
      break;
    case kReflectionAbove:
      direction = CSSIdentifierValue::Create(CSSValueID::kAbove);
      break;
    case kReflectionLeft:
      direction = CSSIdentifierValue::Create(CSSValueID::kLeft);
      break;
    case kReflectionRight:
      direction = CSSIdentifierValue::Create(CSSValueID::kRight);
      break;
  }

  return MakeGarbageCollected<cssvalue::CSSReflectValue>(
      direction, offset,
      ValueForNinePieceImage(reflection->Mask(), style, allow_visited_style));
}

CSSValue* ComputedStyleUtils::MinWidthOrMinHeightAuto(
    const ComputedStyle& style) {
  if (style.IsFlexOrGridOrCustomItem() && !style.IsEnsuredInDisplayNone())
    return CSSIdentifierValue::Create(CSSValueID::kAuto);
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
      NOTREACHED();
      return nullptr;
  }
  DCHECK(positions.first && positions.second);

  const Length& offset = *positions.first;
  const Length& opposite = *positions.second;

  const auto* box = DynamicTo<LayoutBox>(layout_object);
  if (offset.IsPercentOrCalc() && box && layout_object->IsPositioned()) {
    LayoutUnit containing_block_size;
    if (layout_object->IsStickyPositioned()) {
      const LayoutBox* scroll_container = box->ContainingScrollContainer();
      DCHECK(scroll_container);
      bool use_inline_size =
          is_horizontal_property == scroll_container->IsHorizontalWritingMode();
      containing_block_size = use_inline_size
                                  ? scroll_container->ContentLogicalWidth()
                                  : scroll_container->ContentLogicalHeight();
    } else {
      containing_block_size =
          is_horizontal_property ==
                  layout_object->ContainingBlock()->IsHorizontalWritingMode()
              ? box->ContainingBlockLogicalWidthForContent()
              : box->ContainingBlockLogicalHeightForGetComputedStyle();
    }

    return ZoomAdjustedPixelValue(ValueForLength(offset, containing_block_size),
                                  style);
  }

  if (offset.IsAuto() && layout_object) {
    // If the property applies to a positioned element and the resolved value of
    // the display property is not none, the resolved value is the used value.
    // Position offsets have special meaning for position sticky so we return
    // auto when offset.isAuto() on a sticky position object:
    // https://crbug.com/703816.
    if (layout_object->IsRelPositioned()) {
      // If e.g. left is auto and right is not auto, then left's computed value
      // is negative right. So we get the opposite length unit and see if it is
      // auto.
      if (opposite.IsAuto()) {
        return CSSNumericLiteralValue::Create(
            0, CSSPrimitiveValue::UnitType::kPixels);
      }

      if (opposite.IsPercentOrCalc()) {
        if (box) {
          LayoutUnit containing_block_size =
              is_horizontal_property == layout_object->ContainingBlock()
                                            ->IsHorizontalWritingMode()
                  ? box->ContainingBlockLogicalWidthForContent()
                  : box->ContainingBlockLogicalHeightForGetComputedStyle();
          return ZoomAdjustedPixelValue(
              -FloatValueForLength(opposite, containing_block_size), style);
        }
        // FIXME:  fall back to auto for position:relative, display:inline
        return CSSIdentifierValue::Create(CSSValueID::kAuto);
      }

      Length negated_opposite = Negate(opposite);
      return ZoomAdjustedPixelValueForLength(negated_opposite, style);
    }

    if (layout_object->IsOutOfFlowPositioned() && layout_object->IsBox()) {
      // For fixed and absolute positioned elements, the top, left, bottom, and
      // right are defined relative to the corresponding sides of the containing
      // block.
      LayoutBlock* container = layout_object->ContainingBlock();

      // clientOffset is the distance from this object's border edge to the
      // container's padding edge. Thus it includes margins which we subtract
      // below.
      const LayoutSize client_offset =
          box->LocationOffset() -
          LayoutSize(container->ClientLeft(), container->ClientTop());
      LayoutUnit position;

      switch (property.PropertyID()) {
        case CSSPropertyID::kLeft:
          position = client_offset.Width() - box->MarginLeft();
          break;
        case CSSPropertyID::kTop:
          position = client_offset.Height() - box->MarginTop();
          break;
        case CSSPropertyID::kRight:
          position = container->ClientWidth() - box->MarginRight() -
                     (box->OffsetWidth() + client_offset.Width());
          break;
        case CSSPropertyID::kBottom:
          position = container->ClientHeight() - box->MarginBottom() -
                     (box->OffsetHeight() + client_offset.Height());
          break;
        default:
          NOTREACHED();
      }
      return ZoomAdjustedPixelValue(position, style);
    }
  }

  if (offset.IsAuto())
    return CSSIdentifierValue::Create(CSSValueID::kAuto);

  return ZoomAdjustedPixelValueForLength(offset, style);
}

CSSValueList* ComputedStyleUtils::ValueForItemPositionWithOverflowAlignment(
    const StyleSelfAlignmentData& data) {
  CSSValueList* result = CSSValueList::CreateSpaceSeparated();
  if (data.PositionType() == ItemPositionType::kLegacy)
    result->Append(*CSSIdentifierValue::Create(CSSValueID::kLegacy));
  if (data.GetPosition() == ItemPosition::kBaseline) {
    result->Append(*MakeGarbageCollected<CSSValuePair>(
        CSSIdentifierValue::Create(CSSValueID::kBaseline),
        CSSIdentifierValue::Create(CSSValueID::kBaseline),
        CSSValuePair::kDropIdenticalValues));
  } else if (data.GetPosition() == ItemPosition::kLastBaseline) {
    result->Append(*MakeGarbageCollected<CSSValuePair>(
        CSSIdentifierValue::Create(CSSValueID::kLast),
        CSSIdentifierValue::Create(CSSValueID::kBaseline),
        CSSValuePair::kDropIdenticalValues));
  } else {
    if (data.GetPosition() >= ItemPosition::kCenter &&
        data.Overflow() != OverflowAlignment::kDefault)
      result->Append(*CSSIdentifierValue::Create(data.Overflow()));
    if (data.GetPosition() == ItemPosition::kLegacy)
      result->Append(*CSSIdentifierValue::Create(CSSValueID::kNormal));
    else
      result->Append(*CSSIdentifierValue::Create(data.GetPosition()));
  }
  DCHECK_LE(result->length(), 2u);
  return result;
}

CSSValueList*
ComputedStyleUtils::ValueForContentPositionAndDistributionWithOverflowAlignment(
    const StyleContentAlignmentData& data) {
  CSSValueList* result = CSSValueList::CreateSpaceSeparated();
  // Handle content-distribution values
  if (data.Distribution() != ContentDistributionType::kDefault)
    result->Append(*CSSIdentifierValue::Create(data.Distribution()));

  // Handle content-position values (either as fallback or actual value)
  switch (data.GetPosition()) {
    case ContentPosition::kNormal:
      // Handle 'normal' value, not valid as content-distribution fallback.
      if (data.Distribution() == ContentDistributionType::kDefault) {
        result->Append(*CSSIdentifierValue::Create(CSSValueID::kNormal));
      }
      break;
    case ContentPosition::kLastBaseline:
      result->Append(*MakeGarbageCollected<CSSValuePair>(
          CSSIdentifierValue::Create(CSSValueID::kLast),
          CSSIdentifierValue::Create(CSSValueID::kBaseline),
          CSSValuePair::kDropIdenticalValues));
      break;
    default:
      // Handle overflow-alignment (only allowed for content-position values)
      if ((data.GetPosition() >= ContentPosition::kCenter ||
           data.Distribution() != ContentDistributionType::kDefault) &&
          data.Overflow() != OverflowAlignment::kDefault)
        result->Append(*CSSIdentifierValue::Create(data.Overflow()));
      result->Append(*CSSIdentifierValue::Create(data.GetPosition()));
  }

  DCHECK_GT(result->length(), 0u);
  DCHECK_LE(result->length(), 3u);
  return result;
}

CSSValue* ComputedStyleUtils::ValueForLineHeight(const ComputedStyle& style) {
  const Length& length = style.LineHeight();
  if (length.IsNegative())
    return CSSIdentifierValue::Create(CSSValueID::kNormal);

  return ZoomAdjustedPixelValue(
      FloatValueForLength(length, style.GetFontDescription().ComputedSize()),
      style);
}

CSSValue* ComputedStyleUtils::ComputedValueForLineHeight(
    const ComputedStyle& style) {
  const Length& length = style.LineHeight();
  if (length.IsNegative())
    return CSSIdentifierValue::Create(CSSValueID::kNormal);

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
  if (family == font_family_names::kCursive)
    return CSSValueID::kCursive;
  if (family == font_family_names::kFantasy)
    return CSSValueID::kFantasy;
  if (family == font_family_names::kMonospace)
    return CSSValueID::kMonospace;
  if (family == font_family_names::kSansSerif)
    return CSSValueID::kSansSerif;
  if (family == font_family_names::kSerif)
    return CSSValueID::kSerif;
  if (family == font_family_names::kSystemUi)
    return CSSValueID::kSystemUi;
  if (RuntimeEnabledFeatures::CSSFontFamilyMathEnabled() &&
      family == font_family_names::kMath)
    return CSSValueID::kMath;
  // If family does not correspond to any of the above, then it was actually
  // converted from -webkit-body by FontBuilder, so put this value back.
  // TODO(crbug.com/1065468): This trick does not work if
  // FontBuilder::StandardFontFamilyName() actually returned one of the generic
  // family above.
  return CSSValueID::kWebkitBody;
}

CSSValue* ValueForFamily(const FontFamily& family) {
  if (family.FamilyIsGeneric())
    return CSSIdentifierValue::Create(IdentifierForFamily(family.FamilyName()));
  return CSSFontFamilyValue::Create(family.FamilyName());
}

CSSValueList* ComputedStyleUtils::ValueForFontFamily(
    const FontFamily& font_family) {
  CSSValueList* list = CSSValueList::CreateCommaSeparated();
  for (const FontFamily* family = &font_family; family; family = family->Next())
    list->Append(*ValueForFamily(*family));
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

CSSPrimitiveValue* ComputedStyleUtils::ValueForFontStretch(
    const ComputedStyle& style) {
  return CSSNumericLiteralValue::Create(
      style.GetFontDescription().Stretch(),
      CSSPrimitiveValue::UnitType::kPercentage);
}

CSSValue* ComputedStyleUtils::ValueForFontStyle(const ComputedStyle& style) {
  FontSelectionValue angle = style.GetFontDescription().Style();
  if (angle == NormalSlopeValue()) {
    return CSSIdentifierValue::Create(CSSValueID::kNormal);
  }

  if (angle == ItalicSlopeValue()) {
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
      NOTREACHED();
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
      contextual_ligatures_state == FontDescription::kNormalLigaturesState)
    return CSSIdentifierValue::Create(CSSValueID::kNormal);

  if (common_ligatures_state == FontDescription::kDisabledLigaturesState &&
      discretionary_ligatures_state ==
          FontDescription::kDisabledLigaturesState &&
      historical_ligatures_state == FontDescription::kDisabledLigaturesState &&
      contextual_ligatures_state == FontDescription::kDisabledLigaturesState)
    return CSSIdentifierValue::Create(CSSValueID::kNone);

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
  if (variant_numeric.IsAllNormal())
    return CSSIdentifierValue::Create(CSSValueID::kNormal);

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
  if (variant_numeric.OrdinalValue() == FontVariantNumeric::kOrdinalOn)
    value_list->Append(*CSSIdentifierValue::Create(CSSValueID::kOrdinal));
  if (variant_numeric.SlashedZeroValue() == FontVariantNumeric::kSlashedZeroOn)
    value_list->Append(*CSSIdentifierValue::Create(CSSValueID::kSlashedZero));

  return value_list;
}

CSSValue* ComputedStyleUtils::ValueForFontVariantAlternates(
    const ComputedStyle& style) {
  FontVariantAlternates* variant_alternates =
      style.GetFontDescription().GetFontVariantAlternates();
  if (!variant_alternates || variant_alternates->IsNormal())
    return CSSIdentifierValue::Create(CSSValueID::kNormal);

  DCHECK(RuntimeEnabledFeatures::FontVariantAlternatesEnabled());

  auto make_single_ident_list = [](const AtomicString& alias) {
    CSSValueList* aliases_list = CSSValueList::CreateCommaSeparated();
    aliases_list->Append(*MakeGarbageCollected<CSSCustomIdentValue>(alias));
    return aliases_list;
  };

  CSSValueList* value_list = CSSValueList::CreateSpaceSeparated();
  if (AtomicString* opt_stylistic = variant_alternates->Stylistic()) {
    value_list->Append(*MakeGarbageCollected<cssvalue::CSSAlternateValue>(
        *MakeGarbageCollected<CSSFunctionValue>(CSSValueID::kStylistic),
        *make_single_ident_list(*opt_stylistic)));
  }
  if (variant_alternates->HistoricalForms()) {
    value_list->Append(
        *CSSIdentifierValue::Create(CSSValueID::kHistoricalForms));
  }
  if (AtomicString* opt_swash = variant_alternates->Swash()) {
    value_list->Append(*MakeGarbageCollected<cssvalue::CSSAlternateValue>(
        *MakeGarbageCollected<CSSFunctionValue>(CSSValueID::kSwash),
        *make_single_ident_list(*opt_swash)));
  }
  if (AtomicString* opt_ornaments = variant_alternates->Ornaments()) {
    value_list->Append(*MakeGarbageCollected<cssvalue::CSSAlternateValue>(
        *MakeGarbageCollected<CSSFunctionValue>(CSSValueID::kOrnaments),
        *make_single_ident_list(*opt_ornaments)));
  }
  if (AtomicString* opt_annotation = variant_alternates->Annotation()) {
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
      NOTREACHED();
      return CSSIdentifierValue::Create(CSSValueID::kNormal);
  }
}

CSSIdentifierValue* ValueForFontStretchAsKeyword(const ComputedStyle& style) {
  FontSelectionValue stretch_value = style.GetFontDescription().Stretch();
  CSSValueID value_id = CSSValueID::kInvalid;
  if (stretch_value == UltraCondensedWidthValue())
    value_id = CSSValueID::kUltraCondensed;
  if (stretch_value == UltraCondensedWidthValue())
    value_id = CSSValueID::kUltraCondensed;
  if (stretch_value == ExtraCondensedWidthValue())
    value_id = CSSValueID::kExtraCondensed;
  if (stretch_value == CondensedWidthValue())
    value_id = CSSValueID::kCondensed;
  if (stretch_value == SemiCondensedWidthValue())
    value_id = CSSValueID::kSemiCondensed;
  if (stretch_value == NormalWidthValue())
    value_id = CSSValueID::kNormal;
  if (stretch_value == SemiExpandedWidthValue())
    value_id = CSSValueID::kSemiExpanded;
  if (stretch_value == ExpandedWidthValue())
    value_id = CSSValueID::kExpanded;
  if (stretch_value == ExtraExpandedWidthValue())
    value_id = CSSValueID::kExtraExpanded;
  if (stretch_value == UltraExpandedWidthValue())
    value_id = CSSValueID::kUltraExpanded;

  if (IsValidCSSValueID(value_id))
    return CSSIdentifierValue::Create(value_id);
  return nullptr;
}

CSSValue* ComputedStyleUtils::ValueForFontVariantEastAsian(
    const ComputedStyle& style) {
  FontVariantEastAsian east_asian =
      style.GetFontDescription().VariantEastAsian();
  if (east_asian.IsAllNormal())
    return CSSIdentifierValue::Create(CSSValueID::kNormal);

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
      NOTREACHED();
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
      NOTREACHED();
  }
  if (east_asian.Ruby())
    value_list->Append(*CSSIdentifierValue::Create(CSSValueID::kRuby));
  return value_list;
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
                                  CSSValueID::kNormal))))
    return nullptr;

  if (!ValueForFontStretchAsKeyword(style))
    return nullptr;

  CSSIdentifierValue* caps_value = ValueForFontVariantCaps(style);
  if (caps_value->GetValueID() != CSSValueID::kNormal &&
      caps_value->GetValueID() != CSSValueID::kSmallCaps)
    return nullptr;
  AppendIfNotNormal(list, *caps_value);

  {
    CSSNumericLiteralValue* font_weight = ValueForFontWeight(style);
    if (font_weight->DoubleValue() != NormalWeightValue())
      list->Append(*font_weight);
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

CSSValue* SpecifiedValueForGridTrackBreadth(const GridLength& track_breadth,
                                            const ComputedStyle& style) {
  if (!track_breadth.IsLength()) {
    return CSSNumericLiteralValue::Create(
        track_breadth.Flex(), CSSPrimitiveValue::UnitType::kFraction);
  }

  const Length& track_breadth_length = track_breadth.length();
  if (track_breadth_length.IsAuto())
    return CSSIdentifierValue::Create(CSSValueID::kAuto);
  return ComputedStyleUtils::ZoomAdjustedPixelValueForLength(
      track_breadth_length, style);
}

CSSValue* ComputedStyleUtils::SpecifiedValueForGridTrackSize(
    const GridTrackSize& track_size,
    const ComputedStyle& style) {
  switch (track_size.GetType()) {
    case kLengthTrackSizing:
      return SpecifiedValueForGridTrackBreadth(track_size.MinTrackBreadth(),
                                               style);
    case kMinMaxTrackSizing: {
      if (track_size.MinTrackBreadth().IsAuto() &&
          track_size.MaxTrackBreadth().IsFlex()) {
        return CSSNumericLiteralValue::Create(
            track_size.MaxTrackBreadth().Flex(),
            CSSPrimitiveValue::UnitType::kFraction);
      }

      auto* min_max_track_breadths =
          MakeGarbageCollected<CSSFunctionValue>(CSSValueID::kMinmax);
      min_max_track_breadths->Append(*SpecifiedValueForGridTrackBreadth(
          track_size.MinTrackBreadth(), style));
      min_max_track_breadths->Append(*SpecifiedValueForGridTrackBreadth(
          track_size.MaxTrackBreadth(), style));
      return min_max_track_breadths;
    }
    case kFitContentTrackSizing: {
      auto* fit_content_track_breadth =
          MakeGarbageCollected<CSSFunctionValue>(CSSValueID::kFitContent);
      fit_content_track_breadth->Append(*SpecifiedValueForGridTrackBreadth(
          track_size.FitContentTrackBreadth(), style));
      return fit_content_track_breadth;
    }
  }
  NOTREACHED();
  return nullptr;
}

enum GridTrackListSerializationType {
  kForGridElements,
  kForNonGridElements,
  kForRepeatNonGridElements,
};

class OrderedNamedLinesCollector {
  STACK_ALLOCATED();

 public:
  OrderedNamedLinesCollector(
      const OrderedNamedGridLines& ordered_named_grid_lines,
      const OrderedNamedGridLines& ordered_named_auto_repeat_grid_lines)
      : ordered_named_grid_lines_(ordered_named_grid_lines),
        ordered_named_auto_repeat_grid_lines_(
            ordered_named_auto_repeat_grid_lines) {}
  OrderedNamedLinesCollector(const OrderedNamedLinesCollector&) = delete;
  OrderedNamedLinesCollector& operator=(const OrderedNamedLinesCollector&) =
      delete;
  virtual ~OrderedNamedLinesCollector() = default;

  bool IsEmpty() const {
    return ordered_named_grid_lines_.empty() &&
           ordered_named_auto_repeat_grid_lines_.empty();
  }
  virtual void CollectLineNamesForIndex(
      cssvalue::CSSBracketedValueList&,
      size_t index,
      GridTrackListSerializationType named_line_type = kForGridElements) const;

 protected:
  enum NamedLinesType { kNamedLines, kAutoRepeatNamedLines };
  void AppendLines(
      cssvalue::CSSBracketedValueList&,
      size_t index,
      NamedLinesType,
      GridTrackListSerializationType named_line_type = kForGridElements) const;

  const OrderedNamedGridLines& ordered_named_grid_lines_;
  const OrderedNamedGridLines& ordered_named_auto_repeat_grid_lines_;
};

class OrderedNamedLinesCollectorInsideAutoRepeat
    : public OrderedNamedLinesCollector {
 public:
  OrderedNamedLinesCollectorInsideAutoRepeat(
      const OrderedNamedGridLines& ordered_named_grid_lines,
      const OrderedNamedGridLines& ordered_named_auto_repeat_grid_lines)
      : OrderedNamedLinesCollector(ordered_named_grid_lines,
                                   ordered_named_auto_repeat_grid_lines) {}
  void CollectLineNamesForIndex(cssvalue::CSSBracketedValueList&,
                                size_t index,
                                GridTrackListSerializationType named_line_type =
                                    kForGridElements) const override;
};

class OrderedNamedLinesCollectorInGridLayout
    : public OrderedNamedLinesCollector {
 public:
  OrderedNamedLinesCollectorInGridLayout(
      const OrderedNamedGridLines& ordered_named_grid_lines,
      const OrderedNamedGridLines& ordered_named_auto_repeat_grid_lines,
      size_t insertion_point,
      size_t auto_repeat_tracks_count,
      size_t auto_repeat_track_list_length)
      : OrderedNamedLinesCollector(ordered_named_grid_lines,
                                   ordered_named_auto_repeat_grid_lines),
        insertion_point_(insertion_point),
        auto_repeat_total_tracks_(auto_repeat_tracks_count),
        auto_repeat_track_list_length_(auto_repeat_track_list_length) {}
  void CollectLineNamesForIndex(cssvalue::CSSBracketedValueList&,
                                size_t index,
                                GridTrackListSerializationType named_line_type =
                                    kForGridElements) const override;

 private:
  size_t insertion_point_;
  size_t auto_repeat_total_tracks_;
  size_t auto_repeat_track_list_length_;
};

// RJW
void OrderedNamedLinesCollector::AppendLines(
    cssvalue::CSSBracketedValueList& line_names_value,
    size_t index,
    NamedLinesType type,
    GridTrackListSerializationType named_line_type) const {
  auto iter = type == kNamedLines
                  ? ordered_named_grid_lines_.find(index)
                  : ordered_named_auto_repeat_grid_lines_.find(index);
  auto end_iter = type == kNamedLines
                      ? ordered_named_grid_lines_.end()
                      : ordered_named_auto_repeat_grid_lines_.end();
  if (iter == end_iter)
    return;

  for (auto named_grid_line : iter->value) {
    // A line name is appended when:
    // 1. The layout object is the grid.
    // 2. The layout object is not the grid and the named line isn't part of an
    // integer repeat.
    // 3. The layout object is not the grid, the named line is part of the
    // repeat and is the first time it appears in |ordered_named_grid_lines_|.
    const bool is_first_repeat =
        named_grid_line.is_in_repeat && named_grid_line.is_first_repeat;
    if (named_line_type == kForGridElements ||
        (named_line_type == kForNonGridElements &&
         !named_grid_line.is_in_repeat) ||
        (named_line_type == kForRepeatNonGridElements && is_first_repeat)) {
      line_names_value.Append(*MakeGarbageCollected<CSSCustomIdentValue>(
          AtomicString(named_grid_line.line_name)));
    }
  }
}

void OrderedNamedLinesCollector::CollectLineNamesForIndex(
    cssvalue::CSSBracketedValueList& line_names_value,
    size_t i,
    GridTrackListSerializationType named_line_type) const {
  DCHECK(!IsEmpty());
  AppendLines(line_names_value, i, kNamedLines, named_line_type);
}

void OrderedNamedLinesCollectorInsideAutoRepeat::CollectLineNamesForIndex(
    cssvalue::CSSBracketedValueList& line_names_value,
    size_t i,
    GridTrackListSerializationType named_line_type) const {
  DCHECK(!IsEmpty());
  AppendLines(line_names_value, i, kAutoRepeatNamedLines);
}

// RJW
void OrderedNamedLinesCollectorInGridLayout::CollectLineNamesForIndex(
    cssvalue::CSSBracketedValueList& line_names_value,
    size_t i,
    GridTrackListSerializationType named_line_type) const {
  DCHECK(!IsEmpty());
  if (auto_repeat_track_list_length_ == 0LU || i < insertion_point_) {
    AppendLines(line_names_value, i, kNamedLines);
    return;
  }

  DCHECK(auto_repeat_total_tracks_);

  if (i > insertion_point_ + auto_repeat_total_tracks_) {
    AppendLines(line_names_value, i - (auto_repeat_total_tracks_ - 1),
                kNamedLines);
    return;
  }

  if (i == insertion_point_) {
    AppendLines(line_names_value, i, kNamedLines);
    AppendLines(line_names_value, 0, kAutoRepeatNamedLines);
    return;
  }

  if (i == insertion_point_ + auto_repeat_total_tracks_) {
    AppendLines(line_names_value, auto_repeat_track_list_length_,
                kAutoRepeatNamedLines);
    AppendLines(line_names_value, insertion_point_ + 1, kNamedLines);
    return;
  }

  size_t auto_repeat_index_in_first_repetition =
      (i - insertion_point_) % auto_repeat_track_list_length_;
  if (!auto_repeat_index_in_first_repetition && i > insertion_point_) {
    AppendLines(line_names_value, auto_repeat_track_list_length_,
                kAutoRepeatNamedLines);
  }
  AppendLines(line_names_value, auto_repeat_index_in_first_repetition,
              kAutoRepeatNamedLines);
}

void AddValuesForNamedGridLinesAtIndex(
    OrderedNamedLinesCollector& collector,
    size_t i,
    CSSValueList& list,
    GridTrackListSerializationType named_line_type = kForGridElements) {
  if (collector.IsEmpty())
    return;

  auto* line_names = MakeGarbageCollected<cssvalue::CSSBracketedValueList>();
  collector.CollectLineNamesForIndex(*line_names, i, named_line_type);
  if (line_names->length())
    list.Append(*line_names);
}

CSSValue* ComputedStyleUtils::ValueForGridAutoTrackList(
    GridTrackSizingDirection track_direction,
    const LayoutObject* layout_object,
    const ComputedStyle& style) {
  const GridTrackList& grid_auto_track_list = track_direction == kForColumns
                                                  ? style.GridAutoColumns()
                                                  : style.GridAutoRows();
  CSSValueList* list = CSSValueList::CreateSpaceSeparated();

  if (layout_object && layout_object->IsLayoutNGGrid()) {
    const NGGridTrackList& auto_track_list = grid_auto_track_list.NGTrackList();
    if (auto_track_list.RepeaterCount() == 1) {
      for (wtf_size_t i = 0; i < auto_track_list.RepeatSize(0); ++i) {
        list->Append(*SpecifiedValueForGridTrackSize(
            auto_track_list.RepeatTrackSize(0, i), style));
      }
    }
  } else {
    const Vector<GridTrackSize, 1>& auto_track_sizes =
        grid_auto_track_list.LegacyTrackList();
    for (auto& track_size : auto_track_sizes)
      list->Append(*SpecifiedValueForGridTrackSize(track_size, style));
  }
  return list;
}

template <typename T, typename F>
void PopulateGridTrackList(CSSValueList* list,
                           OrderedNamedLinesCollector& collector,
                           const Vector<T, 1>& tracks,
                           F getTrackSize,
                           wtf_size_t start,
                           wtf_size_t end,
                           int offset = 0) {
  DCHECK_LE(start, end);
  DCHECK_LE(end, tracks.size());

  for (wtf_size_t i = start; i < end; ++i) {
    if (offset >= 0 || i >= static_cast<wtf_size_t>(-offset))
      AddValuesForNamedGridLinesAtIndex(collector, i + offset, *list);
    list->Append(*getTrackSize(tracks[i]));
  }
  if (offset >= 0 || end >= static_cast<wtf_size_t>(-offset))
    AddValuesForNamedGridLinesAtIndex(collector, end + offset, *list);
}

template <typename T, typename F>
void PopulateGridTrackList(CSSValueList* list,
                           OrderedNamedLinesCollector& collector,
                           const Vector<T, 1>& tracks,
                           F getTrackSize,
                           int offset = 0) {
  PopulateGridTrackList<T>(list, collector, tracks, getTrackSize, 0,
                           tracks.size(), offset);
}

CSSValue* ComputedStyleUtils::ValueForGridTrackList(
    GridTrackSizingDirection direction,
    const LayoutObject* layout_object,
    const ComputedStyle& style) {
  const bool is_for_columns = direction == kForColumns;
  const bool is_layout_ng = RuntimeEnabledFeatures::LayoutNGEnabled();
  const ComputedGridTrackList& computed_grid_track_list =
      is_for_columns ? style.GridTemplateColumns() : style.GridTemplateRows();
  const Vector<GridTrackSize, 1>& legacy_track_sizes =
      computed_grid_track_list.track_sizes.LegacyTrackList();
  const Vector<GridTrackSize, 1>& auto_repeat_track_sizes =
      computed_grid_track_list.auto_repeat_track_sizes;

  const bool is_layout_grid =
      layout_object && layout_object->IsLayoutGridIncludingNG();

  // Handle the 'none' case.
  bool is_track_list_empty =
      is_layout_ng
          ? !computed_grid_track_list.TrackList().RepeaterCount()
          : (legacy_track_sizes.empty() && auto_repeat_track_sizes.empty());
  if (is_layout_grid && is_track_list_empty) {
    // For grids we should consider every listed track, whether implicitly or
    // explicitly created. Empty grids have a sole grid line per axis.
    const Vector<LayoutUnit>& positions =
        is_for_columns
            ? ToInterface<LayoutNGGridInterface>(layout_object)
                  ->ColumnPositions()
            : ToInterface<LayoutNGGridInterface>(layout_object)->RowPositions();
    is_track_list_empty = positions.size() == 1;
  }

  if (is_track_list_empty)
    return CSSIdentifierValue::Create(CSSValueID::kNone);

  CSSValueList* list = CSSValueList::CreateSpaceSeparated();
  wtf_size_t auto_repeat_insertion_point =
      computed_grid_track_list.auto_repeat_insertion_point;

  if (is_layout_grid) {
    const auto* grid = ToInterface<LayoutNGGridInterface>(layout_object);
    if (computed_grid_track_list.IsSubgriddedAxis()) {
      // If the track list is subgridded, return the word 'subgrid', followed by
      // the specified named grid lines in brackets. Empty brackets are also
      // valid.
      list->Append(
          *MakeGarbageCollected<CSSIdentifierValue>(CSSValueID::kSubgrid));

      wtf_size_t subgrid_line_names_start =
          grid->ExplicitGridStartForDirection(direction);
      wtf_size_t subgrid_line_names_end =
          grid->ExplicitGridEndForDirection(direction);
      for (wtf_size_t i = subgrid_line_names_start; i <= subgrid_line_names_end;
           ++i) {
        auto iter = computed_grid_track_list.ordered_named_grid_lines.find(i);

        cssvalue::CSSBracketedValueList* value_list =
            MakeGarbageCollected<cssvalue::CSSBracketedValueList>();

        if (iter != computed_grid_track_list.ordered_named_grid_lines.end()) {
          for (auto named_grid_line : iter->value) {
            value_list->Append(*MakeGarbageCollected<CSSCustomIdentValue>(
                named_grid_line.line_name));
          }
        }
        list->Append(*value_list);
      }
      return list;
    } else {
      // If the element is a grid container, the resolved value is the used
      // value, specifying track sizes in pixels and expanding the repeat()
      // notation.
      OrderedNamedLinesCollectorInGridLayout collector(
          computed_grid_track_list.ordered_named_grid_lines,
          computed_grid_track_list.auto_repeat_ordered_named_grid_lines,
          auto_repeat_insertion_point,
          grid->AutoRepeatCountForDirection(direction),
          auto_repeat_track_sizes.size());
      auto getTrackSize = [&](const LayoutUnit& v) {
        return ZoomAdjustedPixelValue(v, style);
      };
      // Named grid line indices are relative to the explicit grid, but we are
      // including all tracks. So we need to subtract the number of leading
      // implicit tracks in order to get the proper line index.
      int offset = -base::checked_cast<int>(
          grid->ExplicitGridStartForDirection(direction));
      PopulateGridTrackList(list, collector,
                            grid->TrackSizesForComputedStyle(direction),
                            getTrackSize, offset);
      return list;
    }
  }

  // Otherwise, the resolved value is the computed value, preserving repeat().
  OrderedNamedLinesCollector collector(
      computed_grid_track_list.ordered_named_grid_lines,
      computed_grid_track_list.auto_repeat_ordered_named_grid_lines);
  auto getTrackSize = [&](const GridTrackSize& v) {
    return SpecifiedValueForGridTrackSize(v, style);
  };

  if (auto_repeat_track_sizes.empty()) {
    if (!is_layout_ng) {
      // If it's legacy grid or there's no repeat(), just add all the line names
      // and track sizes.
      PopulateGridTrackList(list, collector, legacy_track_sizes, getTrackSize);
      return list;
    }

    // TODO(ansollan): Add support for track lists with auto and integer
    // repeaters.
    wtf_size_t track_index = 0;
    auto AppendValues = [&](CSSValueList* list, const GridTrackSize& track_size,
                            GridTrackListSerializationType named_line_type) {
      AddValuesForNamedGridLinesAtIndex(collector, track_index, *list,
                                        named_line_type);
      list->Append(*getTrackSize(track_size));
      ++track_index;
    };

    const NGGridTrackList& ng_track_list = computed_grid_track_list.TrackList();
    for (wtf_size_t i = 0; i < ng_track_list.RepeaterCount(); ++i) {
      const auto repeat_type = ng_track_list.RepeatType(i);

      // Add the line names and track sizes that aren't part of the repeat.
      if (repeat_type == NGGridTrackRepeater::RepeatType::kNoRepeat) {
        AppendValues(list, ng_track_list.RepeatTrackSize(i, 0),
                     kForNonGridElements);
        continue;
      }

      // Add a CSSGridIntegerRepeatValue with the contents of the repeat().
      DCHECK_EQ(repeat_type, NGGridTrackRepeater::RepeatType::kInteger);
      const wtf_size_t number_of_repetitions = ng_track_list.RepeatCount(i, 0);
      const wtf_size_t repeat_size = ng_track_list.RepeatSize(i);
      CSSValueList* repeated_values =
          MakeGarbageCollected<cssvalue::CSSGridIntegerRepeatValue>(
              number_of_repetitions);
      AddValuesForNamedGridLinesAtIndex(collector, track_index, *list,
                                        kForNonGridElements);
      for (wtf_size_t j = 0; j < repeat_size; ++j) {
        AppendValues(repeated_values, ng_track_list.RepeatTrackSize(i, j),
                     kForRepeatNonGridElements);
      }
      AddValuesForNamedGridLinesAtIndex(
          collector, track_index, *repeated_values, kForRepeatNonGridElements);
      list->Append(*repeated_values);
      // We need to update |track_index| to skip over added grid named lines
      // that belong to the repeat we just found.
      track_index += repeat_size * (number_of_repetitions - 1);
    }
    AddValuesForNamedGridLinesAtIndex(collector, track_index, *list,
                                      kForNonGridElements);
    return list;
  }
  // Add the line names and track sizes that precede the auto repeat().
  PopulateGridTrackList(list, collector, legacy_track_sizes, getTrackSize, 0,
                        auto_repeat_insertion_point);

  // Add a CSSGridAutoRepeatValue with the contents of the auto repeat().
  CSSValueList* repeated_values =
      MakeGarbageCollected<cssvalue::CSSGridAutoRepeatValue>(
          computed_grid_track_list.auto_repeat_type == AutoRepeatType::kAutoFill
              ? CSSValueID::kAutoFill
              : CSSValueID::kAutoFit);
  OrderedNamedLinesCollectorInsideAutoRepeat repeat_collector(
      computed_grid_track_list.ordered_named_grid_lines,
      computed_grid_track_list.auto_repeat_ordered_named_grid_lines);
  PopulateGridTrackList(repeated_values, repeat_collector,
                        auto_repeat_track_sizes, getTrackSize);
  list->Append(*repeated_values);

  // Add the line names and track sizes that follow the auto repeat().
  PopulateGridTrackList(list, collector, legacy_track_sizes, getTrackSize,
                        auto_repeat_insertion_point, legacy_track_sizes.size(),
                        1);
  return list;
}

CSSValue* ComputedStyleUtils::ValueForGridPosition(
    const GridPosition& position) {
  if (position.IsAuto())
    return CSSIdentifierValue::Create(CSSValueID::kAuto);

  if (position.IsNamedGridArea())
    return MakeGarbageCollected<CSSCustomIdentValue>(position.NamedGridLine());

  CSSValueList* list = CSSValueList::CreateSpaceSeparated();
  if (position.IsSpan()) {
    list->Append(*CSSIdentifierValue::Create(CSSValueID::kSpan));
    list->Append(*CSSNumericLiteralValue::Create(
        position.SpanPosition(), CSSPrimitiveValue::UnitType::kNumber));
  } else {
    list->Append(*CSSNumericLiteralValue::Create(
        position.IntegerPosition(), CSSPrimitiveValue::UnitType::kNumber));
  }

  if (!position.NamedGridLine().IsNull()) {
    list->Append(
        *MakeGarbageCollected<CSSCustomIdentValue>(position.NamedGridLine()));
  }
  return list;
}

static bool IsSVGObjectWithWidthAndHeight(const LayoutObject& layout_object) {
  DCHECK(layout_object.IsSVGChild());
  return layout_object.IsSVGImage() ||
         layout_object.IsSVGForeignObjectIncludingNG() ||
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
  if (!layout_object.IsBox())
    return gfx::SizeF();
  const auto& box = To<LayoutBox>(layout_object);
  return gfx::SizeF(box.StyleRef().BoxSizing() == EBoxSizing::kBorderBox
                        ? box.BorderBoxRect().Size()
                        : box.ComputedCSSContentBoxRect().Size());
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
  if (EnumHasFlags(text_decoration, TextDecorationLine::kUnderline))
    list->Append(*CSSIdentifierValue::Create(CSSValueID::kUnderline));
  if (EnumHasFlags(text_decoration, TextDecorationLine::kOverline))
    list->Append(*CSSIdentifierValue::Create(CSSValueID::kOverline));
  if (EnumHasFlags(text_decoration, TextDecorationLine::kLineThrough))
    list->Append(*CSSIdentifierValue::Create(CSSValueID::kLineThrough));

  if (!list->length())
    return CSSIdentifierValue::Create(CSSValueID::kNone);
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

  NOTREACHED();
  return CSSInitialValue::Create();
}

CSSValue* ComputedStyleUtils::ValueForTextDecorationSkipInk(
    ETextDecorationSkipInk text_decoration_skip_ink) {
  if (text_decoration_skip_ink == ETextDecorationSkipInk::kNone)
    return CSSIdentifierValue::Create(CSSValueID::kNone);
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
    if ((touch_action & TouchAction::kPanX) == TouchAction::kPanX)
      list->Append(*CSSIdentifierValue::Create(CSSValueID::kPanX));
    else if ((touch_action & TouchAction::kPanLeft) != TouchAction::kNone)
      list->Append(*CSSIdentifierValue::Create(CSSValueID::kPanLeft));
    else if ((touch_action & TouchAction::kPanRight) != TouchAction::kNone)
      list->Append(*CSSIdentifierValue::Create(CSSValueID::kPanRight));
    if ((touch_action & TouchAction::kPanY) == TouchAction::kPanY)
      list->Append(*CSSIdentifierValue::Create(CSSValueID::kPanY));
    else if ((touch_action & TouchAction::kPanUp) != TouchAction::kNone)
      list->Append(*CSSIdentifierValue::Create(CSSValueID::kPanUp));
    else if ((touch_action & TouchAction::kPanDown) != TouchAction::kNone)
      list->Append(*CSSIdentifierValue::Create(CSSValueID::kPanDown));

    if ((touch_action & TouchAction::kPinchZoom) == TouchAction::kPinchZoom)
      list->Append(*CSSIdentifierValue::Create(CSSValueID::kPinchZoom));
  }

  DCHECK(list->length());
  return list;
}

CSSValue* ComputedStyleUtils::ValueForWillChange(
    const Vector<CSSPropertyID>& will_change_properties,
    bool will_change_contents,
    bool will_change_scroll_position) {
  CSSValueList* list = CSSValueList::CreateCommaSeparated();
  if (will_change_contents)
    list->Append(*CSSIdentifierValue::Create(CSSValueID::kContents));
  if (will_change_scroll_position)
    list->Append(*CSSIdentifierValue::Create(CSSValueID::kScrollPosition));
  for (wtf_size_t i = 0; i < will_change_properties.size(); ++i) {
    list->Append(
        *MakeGarbageCollected<CSSCustomIdentValue>(will_change_properties[i]));
  }
  if (!list->length())
    list->Append(*CSSIdentifierValue::Create(CSSValueID::kAuto));
  return list;
}

namespace {

template <class U>
using ItemFunc = CSSValue*(U);

template <typename T, typename U>
CSSValue* CreateAnimationValueList(const Vector<T>& values,
                                   ItemFunc<U>* item_func) {
  CSSValueList* list = CSSValueList::CreateCommaSeparated();
  for (const T& value : values) {
    list->Append(*item_func(value));
  }
  return list;
}

}  // namespace

CSSValue* ComputedStyleUtils::ValueForAnimationDelayStart(
    const Timing::Delay& delay) {
  CSSValueList* list = CSSValueList::CreateSpaceSeparated();
  if (delay.IsTimelineOffset()) {
    list->Append(*MakeGarbageCollected<CSSIdentifierValue>(delay.phase));
    list->Append(*CSSNumericLiteralValue::Create(
        delay.relative_offset * 100.0,
        CSSPrimitiveValue::UnitType::kPercentage));
  } else {
    return CSSNumericLiteralValue::Create(
        delay.AsTimeValue().InSecondsF(),
        CSSPrimitiveValue::UnitType::kSeconds);
  }
  return list;
}

CSSValue* ComputedStyleUtils::ValueForAnimationDelayStartList(
    const CSSTimingData* timing_data) {
  return CreateAnimationValueList(
      timing_data ? timing_data->DelayStartList()
                  : Vector<Timing::Delay>{CSSTimingData::InitialDelayStart()},
      &ValueForAnimationDelayStart);
}

CSSValue* ComputedStyleUtils::ValueForAnimationDelayEnd(
    const Timing::Delay& delay) {
  return ValueForAnimationDelayStart(delay);
}

CSSValue* ComputedStyleUtils::ValueForAnimationDelayEndList(
    const CSSTimingData* timing_data) {
  return CreateAnimationValueList(
      timing_data ? timing_data->DelayEndList()
                  : Vector<Timing::Delay>{CSSTimingData::InitialDelayEnd()},
      &ValueForAnimationDelayEnd);
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
      NOTREACHED();
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
    const absl::optional<double>& duration) {
  if (!duration.has_value()) {
    return CSSIdentifierValue::Create(CSSValueID::kAuto);
  }
  return CSSNumericLiteralValue::Create(duration.value(),
                                        CSSPrimitiveValue::UnitType::kSeconds);
}

CSSValue* ComputedStyleUtils::ValueForAnimationDurationList(
    const CSSTimingData* timing_data) {
  return CreateAnimationValueList(
      timing_data
          ? timing_data->DurationList()
          : Vector<absl::optional<double>>{CSSTimingData::InitialDuration()},
      &ValueForAnimationDuration);
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
      NOTREACHED();
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
  if (iteration_count == std::numeric_limits<double>::infinity())
    return CSSIdentifierValue::Create(CSSValueID::kInfinite);
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
  if (play_state == EAnimPlayState::kPlaying)
    return CSSIdentifierValue::Create(CSSValueID::kRunning);
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
            NOTREACHED();
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
      return CSSIdentifierValue::Create(CSSValueID::kLinear);
  }
}

CSSValue* ComputedStyleUtils::ValueForAnimationTimingFunctionList(
    const CSSTimingData* timing_data) {
  return CreateAnimationValueList(
      timing_data
          ? timing_data->TimingFunctionList()
          : Vector<scoped_refptr<TimingFunction>>{CSSAnimationData::
                                                      InitialTimingFunction()},
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
    return MakeGarbageCollected<cssvalue::CSSViewValue>(axis);
  }
  DCHECK(timeline.IsScroll());
  const StyleTimeline::ScrollData& scroll_data = timeline.GetScroll();
  CSSValue* axis = scroll_data.HasDefaultAxis()
                       ? nullptr
                       : CSSIdentifierValue::Create(scroll_data.GetAxis());
  CSSValue* scroller =
      scroll_data.HasDefaultScroller()
          ? nullptr
          : CSSIdentifierValue::Create(scroll_data.GetScroller());

  return MakeGarbageCollected<cssvalue::CSSScrollValue>(axis, scroller);
}

CSSValue* ComputedStyleUtils::ValueForAnimationTimelineList(
    const CSSAnimationData* animation_data) {
  return CreateAnimationValueList(
      animation_data
          ? animation_data->TimelineList()
          : Vector<StyleTimeline>{CSSAnimationData::InitialTimeline()},
      &ValueForAnimationTimeline);
}

CSSValue* ComputedStyleUtils::SingleValueForViewTimelineShorthand(
    const ScopedCSSName* name,
    TimelineAxis axis) {
  CSSValueList* list = CSSValueList::CreateSpaceSeparated();
  list->Append(*ValueForCustomIdentOrNone(name));
  if (axis != TimelineAxis::kBlock)
    list->Append(*CSSIdentifierValue::Create(axis));
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
  if (!transform_list.Operations().size())
    return CSSIdentifierValue::Create(CSSValueID::kNone);

  CSSValueList* components = CSSValueList::CreateSpaceSeparated();
  for (const auto& operation : transform_list.Operations()) {
    CSSValue* op_value = ValueForTransformOperation(*operation, zoom, box_size);
    components->Append(*op_value);
  }
  return components;
}

gfx::RectF ComputedStyleUtils::ReferenceBoxForTransform(
    const LayoutObject& layout_object,
    UsePixelSnappedBox pixel_snap_box) {
  if (layout_object.IsSVGChild())
    return TransformHelper::ComputeReferenceBox(layout_object);
  if (layout_object.IsBox()) {
    const auto& layout_box = To<LayoutBox>(layout_object);
    if (pixel_snap_box == kUsePixelSnappedBox)
      return gfx::RectF(layout_box.PixelSnappedBorderBoxRect());
    return gfx::RectF(layout_box.BorderBoxRect());
  }
  return gfx::RectF();
}

CSSValue* ComputedStyleUtils::ComputedTransformList(
    const ComputedStyle& style,
    const LayoutObject* layout_object) {
  gfx::SizeF box_size(0, 0);
  if (layout_object)
    box_size = ReferenceBoxForTransform(*layout_object).size();

  return ValueForTransformList(style.Transform(), style.EffectiveZoom(),
                               box_size);
}

CSSValue* ComputedStyleUtils::ResolvedTransform(
    const LayoutObject* layout_object,
    const ComputedStyle& style) {
  if (!layout_object || !style.HasTransformOperations())
    return CSSIdentifierValue::Create(CSSValueID::kNone);

  gfx::RectF reference_box = ReferenceBoxForTransform(*layout_object);

  gfx::Transform transform;
  style.ApplyTransform(
      transform, reference_box, ComputedStyle::kIncludeTransformOperations,
      ComputedStyle::kExcludeTransformOrigin, ComputedStyle::kExcludeMotionPath,
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
  if (property.property_type == CSSTransitionData::kTransitionNone)
    return CSSIdentifierValue::Create(CSSValueID::kNone);
  if (property.property_type == CSSTransitionData::kTransitionUnknownProperty)
    return MakeGarbageCollected<CSSCustomIdentValue>(property.property_string);
  DCHECK_EQ(property.property_type,
            CSSTransitionData::kTransitionKnownProperty);
  return MakeGarbageCollected<CSSCustomIdentValue>(
      CSSUnresolvedProperty::Get(property.unresolved_property)
          .GetPropertyNameAtomicString());
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
  NOTREACHED();
  return CSSValueID::kInvalid;
}

CSSValue* ComputedStyleUtils::ValueForContentData(const ComputedStyle& style,
                                                  bool allow_visited_style) {
  if (style.ContentPreventsBoxGeneration())
    return CSSIdentifierValue::Create(CSSValueID::kNone);
  if (style.ContentBehavesAsNormal())
    return CSSIdentifierValue::Create(CSSValueID::kNormal);

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
      list->Append(*image->ComputedCSSValue(style, allow_visited_style));
    } else if (content_data->IsText()) {
      list->Append(*MakeGarbageCollected<CSSStringValue>(
          To<TextContentData>(content_data)->GetText()));
    } else if (content_data->IsQuote()) {
      const QuoteType quote_type = To<QuoteContentData>(content_data)->Quote();
      list->Append(*CSSIdentifierValue::Create(ValueForQuoteType(quote_type)));
    } else if (content_data->IsAltText()) {
      alt_text = MakeGarbageCollected<CSSStringValue>(
          To<AltTextContentData>(content_data)->GetText());
    } else {
      NOTREACHED();
    }
  }
  DCHECK(list->length());

  outer_list->Append(*list);
  if (alt_text)
    outer_list->Append(*alt_text);
  return outer_list;
}

CSSValue* ComputedStyleUtils::ValueForCounterDirectives(
    const ComputedStyle& style,
    CounterNode::Type type) {
  const CounterDirectiveMap* map = style.GetCounterDirectives();
  if (!map)
    return CSSIdentifierValue::Create(CSSValueID::kNone);

  CSSValueList* list = CSSValueList::CreateSpaceSeparated();
  for (const auto& item : *map) {
    bool is_valid_counter_value = false;
    switch (type) {
      case CounterNode::kIncrementType:
        is_valid_counter_value = item.value.IsIncrement();
        break;
      case CounterNode::kResetType:
        is_valid_counter_value = item.value.IsReset();
        break;
      case CounterNode::kSetType:
        is_valid_counter_value = item.value.IsSet();
        break;
    }

    if (!is_valid_counter_value)
      continue;

    list->Append(*MakeGarbageCollected<CSSCustomIdentValue>(item.key));
    int32_t number = 0;
    switch (type) {
      case CounterNode::kIncrementType:
        number = item.value.IncrementValue();
        break;
      case CounterNode::kResetType:
        number = item.value.ResetValue();
        break;
      case CounterNode::kSetType:
        number = item.value.SetValue();
        break;
    }
    list->Append(*CSSNumericLiteralValue::Create(
        (double)number, CSSPrimitiveValue::UnitType::kInteger));
  }

  if (!list->length())
    return CSSIdentifierValue::Create(CSSValueID::kNone);

  return list;
}

CSSValue* ComputedStyleUtils::ValueForShape(const ComputedStyle& style,
                                            bool allow_visited_style,
                                            ShapeValue* shape_value) {
  if (!shape_value)
    return CSSIdentifierValue::Create(CSSValueID::kNone);
  if (shape_value->GetType() == ShapeValue::kBox)
    return CSSIdentifierValue::Create(shape_value->CssBox());
  if (shape_value->GetType() == ShapeValue::kImage) {
    if (shape_value->GetImage()) {
      return shape_value->GetImage()->ComputedCSSValue(style,
                                                       allow_visited_style);
    }
    return CSSIdentifierValue::Create(CSSValueID::kNone);
  }

  DCHECK_EQ(shape_value->GetType(), ShapeValue::kShape);

  CSSValueList* list = CSSValueList::CreateSpaceSeparated();
  list->Append(*ValueForBasicShape(style, shape_value->Shape()));
  if (shape_value->CssBox() != CSSBoxType::kMissing)
    list->Append(*CSSIdentifierValue::Create(shape_value->CssBox()));
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
  if (show_horizontal_top_right)
    horizontal_radii->Append(top_right_radius->Item(0));
  if (show_horizontal_bottom_right)
    horizontal_radii->Append(bottom_right_radius->Item(0));
  if (show_horizontal_bottom_left)
    horizontal_radii->Append(bottom_left_radius->Item(0));

  list->Append(*horizontal_radii);

  CSSValueList* vertical_radii = CSSValueList::CreateSpaceSeparated();
  vertical_radii->Append(top_left_radius->Item(1));
  if (show_vertical_top_right)
    vertical_radii->Append(top_right_radius->Item(1));
  if (show_vertical_bottom_right)
    vertical_radii->Append(bottom_right_radius->Item(1));
  if (show_vertical_bottom_left)
    vertical_radii->Append(bottom_left_radius->Item(1));

  if (!vertical_radii->Equals(To<CSSValueList>(list->Item(0))))
    list->Append(*vertical_radii);

  return list;
}

CSSValue* ComputedStyleUtils::StrokeDashArrayToCSSValueList(
    const SVGDashArray& dashes,
    const ComputedStyle& style) {
  if (dashes.data.empty())
    return CSSIdentifierValue::Create(CSSValueID::kNone);

  CSSValueList* list = CSSValueList::CreateCommaSeparated();
  for (const Length& dash_length : dashes.data) {
    list->Append(*ZoomAdjustedPixelValueForLength(dash_length, style));
  }

  return list;
}

CSSValue* ComputedStyleUtils::ValueForSVGPaint(const SVGPaint& paint,
                                               const ComputedStyle& style) {
  if (paint.type >= SVGPaintType::kUriNone) {
    CSSValueList* values = CSSValueList::CreateSpaceSeparated();
    values->Append(
        *MakeGarbageCollected<cssvalue::CSSURIValue>(paint.GetUrl()));
    if (paint.type == SVGPaintType::kUriNone) {
      values->Append(*CSSIdentifierValue::Create(CSSValueID::kNone));
    } else if (paint.type == SVGPaintType::kUriColor) {
      values->Append(*CurrentColorOrValidColor(style, paint.GetColor(),
                                               CSSValuePhase::kComputedValue));
    }
    return values;
  }
  if (paint.type == SVGPaintType::kNone)
    return CSSIdentifierValue::Create(CSSValueID::kNone);

  return CurrentColorOrValidColor(style, paint.GetColor(),
                                  CSSValuePhase::kComputedValue);
}

CSSValue* ComputedStyleUtils::ValueForSVGResource(
    const StyleSVGResource* resource) {
  if (resource)
    return MakeGarbageCollected<cssvalue::CSSURIValue>(resource->Url());
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
  CSSValue* color =
      CurrentColorOrValidColor(style, shadow.GetColor(), value_phase);
  return MakeGarbageCollected<CSSShadowValue>(x, y, blur, spread, shadow_style,
                                              color);
}

CSSValue* ComputedStyleUtils::ValueForShadowList(const ShadowList* shadow_list,
                                                 const ComputedStyle& style,
                                                 bool use_spread,
                                                 CSSValuePhase value_phase) {
  if (!shadow_list)
    return CSSIdentifierValue::Create(CSSValueID::kNone);

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
  if (filter_operations.Operations().empty())
    return CSSIdentifierValue::Create(CSSValueID::kNone);

  CSSValueList* list = CSSValueList::CreateSpaceSeparated();

  CSSFunctionValue* filter_value = nullptr;

  for (const auto& operation : filter_operations.Operations()) {
    FilterOperation* filter_operation = operation.Get();
    switch (filter_operation->GetType()) {
      case FilterOperation::OperationType::kReference:
        filter_value = MakeGarbageCollected<CSSFunctionValue>(CSSValueID::kUrl);
        filter_value->Append(*MakeGarbageCollected<CSSStringValue>(
            To<ReferenceFilterOperation>(filter_operation)->Url()));
        break;
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
        NOTREACHED();
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
    if (type.strictness == cc::SnapStrictness::kProximity)
      return CSSIdentifierValue::Create(type.axis);
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
  if (!object)
    return false;
  // Non-root SVG objects return the resolved value except <image>,
  // <rect> and <foreignObject> which return the used value.
  if (object->IsSVGChild())
    return IsSVGObjectWithWidthAndHeight(*object);
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
    bool allow_visited_style) {
  CSSValueList* list = CSSValueList::CreateSpaceSeparated();
  for (unsigned i = 0; i < shorthand.length(); ++i) {
    const CSSValue* value =
        shorthand.properties()[i]->CSSValueFromComputedStyle(
            style, layout_object, allow_visited_style);
    DCHECK(value);
    list->Append(*value);
  }
  return list;
}

CSSValuePair* ComputedStyleUtils::ValuesForGapShorthand(
    const StylePropertyShorthand& shorthand,
    const ComputedStyle& style,
    const LayoutObject* layout_object,
    bool allow_visited_style) {
  const CSSValue* row_gap_value =
      shorthand.properties()[0]->CSSValueFromComputedStyle(style, layout_object,
                                                           allow_visited_style);
  const CSSValue* column_gap_value =
      shorthand.properties()[1]->CSSValueFromComputedStyle(style, layout_object,
                                                           allow_visited_style);

  return MakeGarbageCollected<CSSValuePair>(row_gap_value, column_gap_value,
                                            CSSValuePair::kDropIdenticalValues);
}

CSSValueList* ComputedStyleUtils::ValuesForGridShorthand(
    const StylePropertyShorthand& shorthand,
    const ComputedStyle& style,
    const LayoutObject* layout_object,
    bool allow_visited_style) {
  CSSValueList* list = CSSValueList::CreateSlashSeparated();
  for (unsigned i = 0; i < shorthand.length(); ++i) {
    const CSSValue* value =
        shorthand.properties()[i]->CSSValueFromComputedStyle(
            style, layout_object, allow_visited_style);
    DCHECK(value);
    list->Append(*value);
  }
  return list;
}

CSSValueList* ComputedStyleUtils::ValuesForSidesShorthand(
    const StylePropertyShorthand& shorthand,
    const ComputedStyle& style,
    const LayoutObject* layout_object,
    bool allow_visited_style) {
  CSSValueList* list = CSSValueList::CreateSpaceSeparated();
  // Assume the properties are in the usual order top, right, bottom, left.
  const CSSValue* top_value =
      shorthand.properties()[0]->CSSValueFromComputedStyle(style, layout_object,
                                                           allow_visited_style);
  const CSSValue* right_value =
      shorthand.properties()[1]->CSSValueFromComputedStyle(style, layout_object,
                                                           allow_visited_style);
  const CSSValue* bottom_value =
      shorthand.properties()[2]->CSSValueFromComputedStyle(style, layout_object,
                                                           allow_visited_style);
  const CSSValue* left_value =
      shorthand.properties()[3]->CSSValueFromComputedStyle(style, layout_object,
                                                           allow_visited_style);

  // All 4 properties must be specified.
  if (!top_value || !right_value || !bottom_value || !left_value)
    return nullptr;

  bool show_left = !base::ValuesEquivalent(right_value, left_value);
  bool show_bottom =
      !base::ValuesEquivalent(top_value, bottom_value) || show_left;
  bool show_right =
      !base::ValuesEquivalent(top_value, right_value) || show_bottom;

  list->Append(*top_value);
  if (show_right)
    list->Append(*right_value);
  if (show_bottom)
    list->Append(*bottom_value);
  if (show_left)
    list->Append(*left_value);

  return list;
}

CSSValuePair* ComputedStyleUtils::ValuesForInlineBlockShorthand(
    const StylePropertyShorthand& shorthand,
    const ComputedStyle& style,
    const LayoutObject* layout_object,
    bool allow_visited_style) {
  const CSSValue* start_value =
      shorthand.properties()[0]->CSSValueFromComputedStyle(style, layout_object,
                                                           allow_visited_style);
  const CSSValue* end_value =
      shorthand.properties()[1]->CSSValueFromComputedStyle(style, layout_object,
                                                           allow_visited_style);
  // Both properties must be specified.
  if (!start_value || !end_value)
    return nullptr;

  auto* pair = MakeGarbageCollected<CSSValuePair>(
      start_value, end_value, CSSValuePair::kDropIdenticalValues);
  return pair;
}

CSSValuePair* ComputedStyleUtils::ValuesForPlaceShorthand(
    const StylePropertyShorthand& shorthand,
    const ComputedStyle& style,
    const LayoutObject* layout_object,
    bool allow_visited_style) {
  const CSSValue* align_value =
      shorthand.properties()[0]->CSSValueFromComputedStyle(style, layout_object,
                                                           allow_visited_style);
  const CSSValue* justify_value =
      shorthand.properties()[1]->CSSValueFromComputedStyle(style, layout_object,
                                                           allow_visited_style);

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
    bool allow_visited_style) {
  enum VariantShorthandCases {
    kAllNormal,
    kNoneLigatures,
    kConcatenateNonNormal
  };
  StylePropertyShorthand shorthand = fontVariantShorthand();
  VariantShorthandCases shorthand_case = kAllNormal;
  for (unsigned i = 0; i < shorthand.length(); ++i) {
    const CSSValue* value =
        shorthand.properties()[i]->CSSValueFromComputedStyle(
            style, layout_object, allow_visited_style);

    auto* identifier_value = DynamicTo<CSSIdentifierValue>(value);
    if (shorthand_case == kAllNormal && identifier_value &&
        identifier_value->GetValueID() == CSSValueID::kNone &&
        shorthand.properties()[i]->IDEquals(
            CSSPropertyID::kFontVariantLigatures)) {
      shorthand_case = kNoneLigatures;
    } else if (!(identifier_value &&
                 identifier_value->GetValueID() == CSSValueID::kNormal)) {
      shorthand_case = kConcatenateNonNormal;
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
      for (unsigned i = 0; i < shorthand.length(); ++i) {
        const CSSValue* value =
            shorthand.properties()[i]->CSSValueFromComputedStyle(
                style, layout_object, allow_visited_style);
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
    default:
      NOTREACHED();
      return nullptr;
  }
}

CSSValue* ComputedStyleUtils::ValuesForFontSynthesisProperty(
    const ComputedStyle& style,
    const LayoutObject* layout_object,
    bool allow_visited_style) {
  enum FontSynthesisShorthandCases { kAllNone, kConcatenateAuto };
  StylePropertyShorthand shorthand = fontSynthesisShorthand();
  FontSynthesisShorthandCases shorthand_case = kAllNone;
  for (unsigned i = 0; i < shorthand.length(); ++i) {
    const CSSValue* value =
        shorthand.properties()[i]->CSSValueFromComputedStyle(
            style, layout_object, allow_visited_style);
    auto* identifier_value = DynamicTo<CSSIdentifierValue>(value);
    if (shorthand.properties()[i]->IDEquals(
            CSSPropertyID::kFontSynthesisWeight) &&
        identifier_value->GetValueID() == CSSValueID::kAuto) {
      shorthand_case = kConcatenateAuto;
    } else if (shorthand.properties()[i]->IDEquals(
                   CSSPropertyID::kFontSynthesisStyle) &&
               identifier_value->GetValueID() == CSSValueID::kAuto) {
      shorthand_case = kConcatenateAuto;
    } else if (shorthand.properties()[i]->IDEquals(
                   CSSPropertyID::kFontSynthesisSmallCaps) &&
               identifier_value->GetValueID() == CSSValueID::kAuto) {
      shorthand_case = kConcatenateAuto;
    }
  }

  switch (shorthand_case) {
    case kAllNone:
      return CSSIdentifierValue::Create(CSSValueID::kNone);
    case kConcatenateAuto: {
      CSSValueList* list = CSSValueList::CreateSpaceSeparated();
      for (unsigned i = 0; i < shorthand.length(); ++i) {
        const CSSValue* value =
            shorthand.properties()[i]->CSSValueFromComputedStyle(
                style, layout_object, allow_visited_style);
        auto* identifier_value = DynamicTo<CSSIdentifierValue>(value);
        if (shorthand.properties()[i]->IDEquals(
                CSSPropertyID::kFontSynthesisWeight) &&
            identifier_value->GetValueID() == CSSValueID::kAuto) {
          list->Append(*CSSIdentifierValue::Create(CSSValueID::kWeight));
        } else if (shorthand.properties()[i]->IDEquals(
                       CSSPropertyID::kFontSynthesisStyle) &&
                   identifier_value->GetValueID() == CSSValueID::kAuto) {
          list->Append(*CSSIdentifierValue::Create(CSSValueID::kStyle));
        } else if (shorthand.properties()[i]->IDEquals(
                       CSSPropertyID::kFontSynthesisSmallCaps) &&
                   identifier_value->GetValueID() == CSSValueID::kAuto) {
          list->Append(*CSSIdentifierValue::Create(CSSValueID::kSmallCaps));
        }
      }
      return list;
    }
    default:
      NOTREACHED();
      return nullptr;
  }
}

CSSValueList* ComputedStyleUtils::ValuesForContainerShorthand(
    const ComputedStyle& style,
    const LayoutObject* layout_object,
    bool allow_visited_style) {
  CHECK_EQ(containerShorthand().length(), 2u);
  CHECK_EQ(containerShorthand().properties()[0],
           &GetCSSPropertyContainerName());
  CHECK_EQ(containerShorthand().properties()[1],
           &GetCSSPropertyContainerType());

  CSSValueList* list = CSSValueList::CreateSlashSeparated();

  const CSSValue* name =
      GetCSSPropertyContainerName().CSSValueFromComputedStyle(
          style, layout_object, allow_visited_style);
  const CSSValue* type =
      GetCSSPropertyContainerType().CSSValueFromComputedStyle(
          style, layout_object, allow_visited_style);

  DCHECK(name);
  DCHECK(type);

  list->Append(*name);

  if (!(IsA<CSSIdentifierValue>(type) &&
        To<CSSIdentifierValue>(*type).GetValueID() == CSSValueID::kNormal)) {
    list->Append(*type);
  }

  return list;
}

CSSValueList* ComputedStyleUtils::ValuesForScrollTimelineShorthand(
    const ComputedStyle& style,
    const LayoutObject* layout_object,
    bool allow_visited_style) {
  CHECK_EQ(scrollTimelineShorthand().length(), 2u);
  CHECK_EQ(scrollTimelineShorthand().properties()[0],
           &GetCSSPropertyScrollTimelineAxis());
  CHECK_EQ(scrollTimelineShorthand().properties()[1],
           &GetCSSPropertyScrollTimelineName());

  CSSValueList* list = CSSValueList::CreateSpaceSeparated();

  const CSSValue* axis =
      GetCSSPropertyScrollTimelineAxis().CSSValueFromComputedStyle(
          style, layout_object, allow_visited_style);
  const CSSValue* name =
      GetCSSPropertyScrollTimelineName().CSSValueFromComputedStyle(
          style, layout_object, allow_visited_style);

  DCHECK(axis);
  DCHECK(name);

  // Append any value that's not the initial value.
  if (To<CSSIdentifierValue>(*axis).GetValueID() != CSSValueID::kBlock) {
    list->Append(*axis);
  }
  if (!(IsA<CSSIdentifierValue>(name) &&
        To<CSSIdentifierValue>(*name).GetValueID() == CSSValueID::kNone)) {
    list->Append(*name);
  }

  // If both values were the initial value, we append axis.
  if (!list->length())
    list->Append(*axis);

  return list;
}

// Returns up to two values for 'scroll-customization' property. The values
// correspond to the customization values for 'x' and 'y' axes.
CSSValue* ComputedStyleUtils::ScrollCustomizationFlagsToCSSValue(
    scroll_customization::ScrollDirection scroll_customization) {
  CSSValueList* list = CSSValueList::CreateSpaceSeparated();
  if (scroll_customization == scroll_customization::kScrollDirectionAuto) {
    list->Append(*CSSIdentifierValue::Create(CSSValueID::kAuto));
  } else if (scroll_customization ==
             scroll_customization::kScrollDirectionNone) {
    list->Append(*CSSIdentifierValue::Create(CSSValueID::kNone));
  } else {
    if ((scroll_customization & scroll_customization::kScrollDirectionPanX) ==
        scroll_customization::kScrollDirectionPanX)
      list->Append(*CSSIdentifierValue::Create(CSSValueID::kPanX));
    else if (scroll_customization &
             scroll_customization::kScrollDirectionPanLeft)
      list->Append(*CSSIdentifierValue::Create(CSSValueID::kPanLeft));
    else if (scroll_customization &
             scroll_customization::kScrollDirectionPanRight)
      list->Append(*CSSIdentifierValue::Create(CSSValueID::kPanRight));
    if ((scroll_customization & scroll_customization::kScrollDirectionPanY) ==
        scroll_customization::kScrollDirectionPanY)
      list->Append(*CSSIdentifierValue::Create(CSSValueID::kPanY));
    else if (scroll_customization & scroll_customization::kScrollDirectionPanUp)
      list->Append(*CSSIdentifierValue::Create(CSSValueID::kPanUp));
    else if (scroll_customization &
             scroll_customization::kScrollDirectionPanDown)
      list->Append(*CSSIdentifierValue::Create(CSSValueID::kPanDown));
  }

  DCHECK(list->length());
  return list;
}

CSSValue* ComputedStyleUtils::ValueForGapLength(
    const absl::optional<Length>& gap_length,
    const ComputedStyle& style) {
  if (!gap_length)
    return CSSIdentifierValue::Create(CSSValueID::kNormal);
  return ZoomAdjustedPixelValueForLength(*gap_length, style);
}

CSSValue* ComputedStyleUtils::ValueForStyleName(const StyleName& name) {
  if (name.IsCustomIdent())
    return MakeGarbageCollected<CSSCustomIdentValue>(name.GetValue());
  return MakeGarbageCollected<CSSStringValue>(name.GetValue());
}

CSSValue* ComputedStyleUtils::ValueForStyleNameOrKeyword(
    const StyleNameOrKeyword& value) {
  if (value.IsKeyword())
    return CSSIdentifierValue::Create(value.GetKeyword());
  return ValueForStyleName(value.GetName());
}

CSSValue* ComputedStyleUtils::ValueForCustomIdentOrNone(
    const AtomicString& ident) {
  if (ident.empty())
    return CSSIdentifierValue::Create(CSSValueID::kNone);
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
    const absl::optional<StyleIntrinsicLength>& intrinsic_length) {
  CSSValue* length = nullptr;
  if (intrinsic_length) {
    length = ComputedStyleUtils::ZoomAdjustedPixelValueForLength(
        intrinsic_length->GetLength(), style);
  }

  if (!intrinsic_length)
    return CSSIdentifierValue::Create(CSSValueID::kNone);
  CSSValueList* list = CSSValueList::CreateSpaceSeparated();
  if (intrinsic_length->HasAuto())
    list->Append(*CSSIdentifierValue::Create(CSSValueID::kAuto));
  list->Append(*length);
  return list;
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
          To<CSSUnparsedValue>(style_value)->ToString());
    default:
      return std::make_unique<CrossThreadUnsupportedValue>(
          style_value->toString());
  }
}

const CSSValue* ComputedStyleUtils::ComputedPropertyValue(
    const CSSProperty& property,
    const ComputedStyle& style,
    const LayoutObject* layout_object) {
  switch (property.PropertyID()) {
    // Computed value is usually relative so that multiple fonts in child
    // elements work properly, but resolved value is always a pixel length.
    case CSSPropertyID::kLineHeight:
      return ComputedStyleUtils::ComputedValueForLineHeight(style);

    // Returns a transform list instead of converting to a (resolved) matrix.
    case CSSPropertyID::kTransform:
      return ComputedStyleUtils::ComputedTransformList(style, layout_object);

    // For the following properties, the resolved value is the used value, which
    // is not what we want. Obtain the computed value instead.
    case CSSPropertyID::kBackgroundColor:
      return ComputedStyleUtils::CurrentColorOrValidColor(
          style, style.BackgroundColor(), CSSValuePhase::kComputedValue);
    case CSSPropertyID::kBorderBlockEndColor:
    case CSSPropertyID::kBorderBlockStartColor:
    case CSSPropertyID::kBorderInlineEndColor:
    case CSSPropertyID::kBorderInlineStartColor:
      return ComputedStyleUtils::ComputedPropertyValue(
          property.ResolveDirectionAwareProperty(style.Direction(),
                                                 style.GetWritingMode()),
          style, layout_object);
    case CSSPropertyID::kBorderBottomColor:
      return ComputedStyleUtils::CurrentColorOrValidColor(
          style, style.BorderBottomColor(), CSSValuePhase::kComputedValue);
    case CSSPropertyID::kBorderLeftColor:
      return ComputedStyleUtils::CurrentColorOrValidColor(
          style, style.BorderLeftColor(), CSSValuePhase::kComputedValue);
    case CSSPropertyID::kBorderRightColor:
      return ComputedStyleUtils::CurrentColorOrValidColor(
          style, style.BorderRightColor(), CSSValuePhase::kComputedValue);
    case CSSPropertyID::kBorderTopColor:
      return ComputedStyleUtils::CurrentColorOrValidColor(
          style, style.BorderTopColor(), CSSValuePhase::kComputedValue);
    case CSSPropertyID::kBoxShadow:
      return ComputedStyleUtils::ValueForShadowList(
          style.BoxShadow(), style, true, CSSValuePhase::kComputedValue);
    case CSSPropertyID::kCaretColor:
      return ComputedStyleUtils::ValueForStyleAutoColor(
          style, style.CaretColor(), CSSValuePhase::kComputedValue);
    case CSSPropertyID::kColor:
      return ComputedStyleUtils::CurrentColorOrValidColor(
          style, style.Color(), CSSValuePhase::kComputedValue);
    case CSSPropertyID::kMinHeight: {
      if (style.MinHeight().IsAuto())
        return CSSIdentifierValue::Create(CSSValueID::kAuto);
      return property.CSSValueFromComputedStyle(
          style, /*layout_object=*/nullptr, false);
    }
    case CSSPropertyID::kMinWidth: {
      if (style.MinWidth().IsAuto())
        return CSSIdentifierValue::Create(CSSValueID::kAuto);
      return property.CSSValueFromComputedStyle(
          style, /*layout_object=*/nullptr, false);
    }
    case CSSPropertyID::kOutlineColor:
      return ComputedStyleUtils::CurrentColorOrValidColor(
          style, style.OutlineColor(), CSSValuePhase::kComputedValue);

    // For all other properties, the resolved value is either always the same
    // as the computed value (most properties), or the same as the computed
    // value when there is no layout box ('width' and friends).
    default:
      return property.CSSValueFromComputedStyle(
          style, /*layout_object=*/nullptr, false);
  }
}

}  // namespace blink
