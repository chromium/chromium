// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/animation/interpolation_types_map.h"

#include <memory>
#include <utility>

#include "third_party/blink/renderer/core/animation/css_angle_interpolation_type.h"
#include "third_party/blink/renderer/core/animation/css_aspect_ratio_interpolation_type.h"
#include "third_party/blink/renderer/core/animation/css_basic_shape_interpolation_type.h"
#include "third_party/blink/renderer/core/animation/css_border_image_length_box_interpolation_type.h"
#include "third_party/blink/renderer/core/animation/css_border_shape_interpolation_type.h"
#include "third_party/blink/renderer/core/animation/css_clip_interpolation_type.h"
#include "third_party/blink/renderer/core/animation/css_color_interpolation_type.h"
#include "third_party/blink/renderer/core/animation/css_content_visibility_interpolation_type.h"
#include "third_party/blink/renderer/core/animation/css_custom_length_interpolation_type.h"
#include "third_party/blink/renderer/core/animation/css_custom_list_interpolation_type.h"
#include "third_party/blink/renderer/core/animation/css_custom_transform_function_interpolation_type.h"
#include "third_party/blink/renderer/core/animation/css_custom_transform_interpolation_type.h"
#include "third_party/blink/renderer/core/animation/css_default_interpolation_type.h"
#include "third_party/blink/renderer/core/animation/css_display_interpolation_type.h"
#include "third_party/blink/renderer/core/animation/css_dynamic_range_limit_interpolation_type.h"
#include "third_party/blink/renderer/core/animation/css_filter_list_interpolation_type.h"
#include "third_party/blink/renderer/core/animation/css_font_palette_interpolation_type.h"
#include "third_party/blink/renderer/core/animation/css_font_size_adjust_interpolation_type.h"
#include "third_party/blink/renderer/core/animation/css_font_size_interpolation_type.h"
#include "third_party/blink/renderer/core/animation/css_font_stretch_interpolation_type.h"
#include "third_party/blink/renderer/core/animation/css_font_style_interpolation_type.h"
#include "third_party/blink/renderer/core/animation/css_font_variation_settings_interpolation_type.h"
#include "third_party/blink/renderer/core/animation/css_font_weight_interpolation_type.h"
#include "third_party/blink/renderer/core/animation/css_gap_color_list_interpolation_type.h"
#include "third_party/blink/renderer/core/animation/css_gap_length_list_interpolation_type.h"
#include "third_party/blink/renderer/core/animation/css_grid_template_property_interpolation_type.h"
#include "third_party/blink/renderer/core/animation/css_image_interpolation_type.h"
#include "third_party/blink/renderer/core/animation/css_image_list_interpolation_type.h"
#include "third_party/blink/renderer/core/animation/css_image_slice_interpolation_type.h"
#include "third_party/blink/renderer/core/animation/css_intrinsic_length_interpolation_type.h"
#include "third_party/blink/renderer/core/animation/css_length_interpolation_type.h"
#include "third_party/blink/renderer/core/animation/css_length_list_interpolation_type.h"
#include "third_party/blink/renderer/core/animation/css_length_pair_interpolation_type.h"
#include "third_party/blink/renderer/core/animation/css_number_interpolation_type.h"
#include "third_party/blink/renderer/core/animation/css_offset_rotate_interpolation_type.h"
#include "third_party/blink/renderer/core/animation/css_overlay_interpolation_type.h"
#include "third_party/blink/renderer/core/animation/css_paint_interpolation_type.h"
#include "third_party/blink/renderer/core/animation/css_path_interpolation_type.h"
#include "third_party/blink/renderer/core/animation/css_percentage_interpolation_type.h"
#include "third_party/blink/renderer/core/animation/css_position_axis_list_interpolation_type.h"
#include "third_party/blink/renderer/core/animation/css_position_interpolation_type.h"
#include "third_party/blink/renderer/core/animation/css_ray_interpolation_type.h"
#include "third_party/blink/renderer/core/animation/css_resolution_interpolation_type.h"
#include "third_party/blink/renderer/core/animation/css_rotate_interpolation_type.h"
#include "third_party/blink/renderer/core/animation/css_scale_interpolation_type.h"
#include "third_party/blink/renderer/core/animation/css_scrollbar_color_interpolation_type.h"
#include "third_party/blink/renderer/core/animation/css_shadow_list_interpolation_type.h"
#include "third_party/blink/renderer/core/animation/css_shape_interpolation_type.h"
#include "third_party/blink/renderer/core/animation/css_size_list_interpolation_type.h"
#include "third_party/blink/renderer/core/animation/css_superellipse_interpolation_type.h"
#include "third_party/blink/renderer/core/animation/css_text_indent_interpolation_type.h"
#include "third_party/blink/renderer/core/animation/css_time_interpolation_type.h"
#include "third_party/blink/renderer/core/animation/css_transform_interpolation_type.h"
#include "third_party/blink/renderer/core/animation/css_transform_origin_interpolation_type.h"
#include "third_party/blink/renderer/core/animation/css_translate_interpolation_type.h"
#include "third_party/blink/renderer/core/animation/css_var_cycle_interpolation_type.h"
#include "third_party/blink/renderer/core/animation/css_visibility_interpolation_type.h"
#include "third_party/blink/renderer/core/css/css_property_names.h"
#include "third_party/blink/renderer/core/css/css_syntax_definition.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_context.h"
#include "third_party/blink/renderer/core/css/properties/css_property.h"
#include "third_party/blink/renderer/core/css/property_registry.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_map.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

namespace blink {

InterpolationTypesMap::InterpolationTypesMap(const PropertyRegistry* registry,
                                             const Document& document)
    : document_(document), registry_(registry) {}

static const PropertyRegistration* GetRegistration(
    const PropertyRegistry* registry,
    const PropertyHandle& property) {
  DCHECK(property.IsCSSCustomProperty());
  if (!registry) {
    return nullptr;
  }
  return registry->Registration(property.CustomPropertyName());
}

const InterpolationTypes* InterpolationTypesMap::Get(
    const PropertyHandle& property) const {
  using ApplicableTypesMap =
      GCedHeapHashMap<PropertyHandle, Member<InterpolationTypes>>;
  DEFINE_STATIC_LOCAL(Persistent<ApplicableTypesMap>, all_applicable_types_map,
                      (MakeGarbageCollected<ApplicableTypesMap>()));

  DEFINE_STATIC_LOCAL(Persistent<ApplicableTypesMap>,
                      reduce_motion_applicable_types_map,
                      (MakeGarbageCollected<ApplicableTypesMap>()));

  // Custom property interpolation types may change over time so don't trust the
  // applicable_types_map without checking the registry. Also since the static
  // map is shared between documents, the registered type may be different in
  // the different documents.
  if (registry_ && property.IsCSSCustomProperty()) {
    if (const auto* registration = GetRegistration(registry_, property)) {
      return registration->GetInterpolationTypes();
    }
  }
  bool reduce_motion = document_.ShouldForceReduceMotion();

  ApplicableTypesMap& applicable_types_map =
      reduce_motion ? *reduce_motion_applicable_types_map
                    : *all_applicable_types_map;

  auto entry = applicable_types_map.find(property);
  if (entry != applicable_types_map.end()) {
    return entry->value;
  }

  InterpolationTypes* applicable_types(
      MakeGarbageCollected<InterpolationTypes>());

  const CSSProperty& css_property = property.GetCSSProperty();

  if (!reduce_motion) {
    switch (css_property.PropertyID()) {
      case CSSPropertyID::kBaselineShift:
      case CSSPropertyID::kBorderBottomWidth:
      case CSSPropertyID::kBorderLeftWidth:
      case CSSPropertyID::kBorderRightWidth:
      case CSSPropertyID::kBorderTopWidth:
      case CSSPropertyID::kBottom:
      case CSSPropertyID::kColumnRuleEdgeInsetEnd:
      case CSSPropertyID::kRowRuleEdgeInsetEnd:
      case CSSPropertyID::kColumnRuleEdgeInsetStart:
      case CSSPropertyID::kRowRuleEdgeInsetStart:
      case CSSPropertyID::kColumnRuleInteriorInsetEnd:
      case CSSPropertyID::kRowRuleInteriorInsetEnd:
      case CSSPropertyID::kColumnRuleInteriorInsetStart:
      case CSSPropertyID::kRowRuleInteriorInsetStart:
      case CSSPropertyID::kCx:
      case CSSPropertyID::kCy:
      case CSSPropertyID::kFlexBasis:
      case CSSPropertyID::kHeight:
      case CSSPropertyID::kLeft:
      case CSSPropertyID::kLetterSpacing:
      case CSSPropertyID::kMarginBottom:
      case CSSPropertyID::kMarginLeft:
      case CSSPropertyID::kMarginRight:
      case CSSPropertyID::kMarginTop:
      case CSSPropertyID::kMaxHeight:
      case CSSPropertyID::kMaxWidth:
      case CSSPropertyID::kMinHeight:
      case CSSPropertyID::kMinWidth:
      case CSSPropertyID::kOffsetDistance:
      case CSSPropertyID::kOutlineOffset:
      case CSSPropertyID::kOutlineWidth:
      case CSSPropertyID::kPaddingBottom:
      case CSSPropertyID::kPaddingLeft:
      case CSSPropertyID::kPaddingRight:
      case CSSPropertyID::kPaddingTop:
      case CSSPropertyID::kPerspective:
      case CSSPropertyID::kR:
      case CSSPropertyID::kRight:
      case CSSPropertyID::kRx:
      case CSSPropertyID::kRy:
      case CSSPropertyID::kShapeMargin:
      case CSSPropertyID::kStrokeDashoffset:
      case CSSPropertyID::kStrokeWidth:
      case CSSPropertyID::kTextDecorationThickness:
      case CSSPropertyID::kTextUnderlineOffset:
      case CSSPropertyID::kTop:
      case CSSPropertyID::kVerticalAlign:
      case CSSPropertyID::kWebkitBorderHorizontalSpacing:
      case CSSPropertyID::kWebkitBorderVerticalSpacing:
      case CSSPropertyID::kColumnGap:
      case CSSPropertyID::kRowGap:
      case CSSPropertyID::kColumnWidth:
      case CSSPropertyID::kColumnHeight:
      case CSSPropertyID::kWebkitPerspectiveOriginX:
      case CSSPropertyID::kWebkitPerspectiveOriginY:
      case CSSPropertyID::kWebkitTransformOriginX:
      case CSSPropertyID::kWebkitTransformOriginY:
      case CSSPropertyID::kWebkitTransformOriginZ:
      case CSSPropertyID::kWidth:
      case CSSPropertyID::kWordSpacing:
      case CSSPropertyID::kX:
      case CSSPropertyID::kY:
        applicable_types->push_back(
            MakeGarbageCollected<CSSLengthInterpolationType>(property));
        break;
      case CSSPropertyID::kAspectRatio:
        applicable_types->push_back(
            MakeGarbageCollected<CSSAspectRatioInterpolationType>(property));
        break;
      case CSSPropertyID::kGridTemplateColumns:
      case CSSPropertyID::kGridTemplateRows:
        applicable_types->push_back(
            MakeGarbageCollected<CSSGridTemplatePropertyInterpolationType>(
                property));
        break;
      case CSSPropertyID::kColumnRuleColor:
      case CSSPropertyID::kRowRuleColor:
        if (RuntimeEnabledFeatures::CSSGapDecorationEnabled()) {
          applicable_types->push_back(
              MakeGarbageCollected<CSSGapColorListInterpolationType>(property));
          break;
        }
        applicable_types->push_back(
            MakeGarbageCollected<CSSColorInterpolationType>(property));
        break;
      case CSSPropertyID::kColumnRuleWidth:
      case CSSPropertyID::kRowRuleWidth:
        if (RuntimeEnabledFeatures::CSSGapDecorationEnabled()) {
          applicable_types->push_back(
              MakeGarbageCollected<CSSGapLengthListInterpolationType>(
                  property));
        } else {
          applicable_types->push_back(
              MakeGarbageCollected<CSSLengthInterpolationType>(property));
        }
        break;
      case CSSPropertyID::kContainIntrinsicWidth:
      case CSSPropertyID::kContainIntrinsicHeight:
        applicable_types->push_back(
            MakeGarbageCollected<CSSIntrinsicLengthInterpolationType>(
                property));
        break;
      case CSSPropertyID::kDynamicRangeLimit:
        if (RuntimeEnabledFeatures::CSSDynamicRangeLimitEnabled()) {
          applicable_types->push_back(
              MakeGarbageCollected<CSSDynamicRangeLimitInterpolationType>(
                  property));
        }
        break;
      case CSSPropertyID::kFlexGrow:
      case CSSPropertyID::kFlexShrink:
      case CSSPropertyID::kFillOpacity:
      case CSSPropertyID::kFloodOpacity:
      case CSSPropertyID::kOpacity:
      case CSSPropertyID::kOrder:
      case CSSPropertyID::kOrphans:
      case CSSPropertyID::kReadingOrder:
      case CSSPropertyID::kShapeImageThreshold:
      case CSSPropertyID::kStopOpacity:
      case CSSPropertyID::kStrokeMiterlimit:
      case CSSPropertyID::kStrokeOpacity:
      case CSSPropertyID::kColumnCount:
      case CSSPropertyID::kWidows:
      case CSSPropertyID::kZIndex:
        applicable_types->push_back(
            MakeGarbageCollected<CSSNumberInterpolationType>(property));
        break;
      case CSSPropertyID::kCornerTopLeftShape:
      case CSSPropertyID::kCornerTopRightShape:
      case CSSPropertyID::kCornerBottomLeftShape:
      case CSSPropertyID::kCornerBottomRightShape:
        applicable_types->push_back(
            MakeGarbageCollected<CSSSuperellipseInterpolationType>(property));
        break;
      case CSSPropertyID::kTextSizeAdjust:
        applicable_types->push_back(
            MakeGarbageCollected<CSSPercentageInterpolationType>(property));
        break;
      case CSSPropertyID::kLineHeight:
      case CSSPropertyID::kTabSize:
        applicable_types->push_back(
            MakeGarbageCollected<CSSLengthInterpolationType>(property));
        applicable_types->push_back(
            MakeGarbageCollected<CSSNumberInterpolationType>(property));
        break;
      case CSSPropertyID::kInterestDelayStart:
      case CSSPropertyID::kInterestDelayEnd:
        applicable_types->push_back(
            MakeGarbageCollected<CSSTimeInterpolationType>(property));
        break;
      case CSSPropertyID::kAccentColor:
      case CSSPropertyID::kBackgroundColor:
      case CSSPropertyID::kBorderBottomColor:
      case CSSPropertyID::kBorderLeftColor:
      case CSSPropertyID::kBorderRightColor:
      case CSSPropertyID::kBorderTopColor:
      case CSSPropertyID::kCaretColor:
      case CSSPropertyID::kColor:
      case CSSPropertyID::kFloodColor:
      case CSSPropertyID::kLightingColor:
      case CSSPropertyID::kOutlineColor:
      case CSSPropertyID::kStopColor:
      case CSSPropertyID::kTextDecorationColor:
      case CSSPropertyID::kTextEmphasisColor:
      case CSSPropertyID::kWebkitTextStrokeColor:
        applicable_types->push_back(
            MakeGarbageCollected<CSSColorInterpolationType>(property));
        break;
      case CSSPropertyID::kFill:
      case CSSPropertyID::kStroke:
        applicable_types->push_back(
            MakeGarbageCollected<CSSPaintInterpolationType>(property));
        break;
      case CSSPropertyID::kOffsetPath:
        applicable_types->push_back(
            MakeGarbageCollected<CSSBasicShapeInterpolationType>(property));
        applicable_types->push_back(
            MakeGarbageCollected<CSSRayInterpolationType>(property));
        applicable_types->push_back(
            MakeGarbageCollected<CSSPathInterpolationType>(property));
        applicable_types->push_back(
            MakeGarbageCollected<CSSShapeInterpolationType>(property));
        break;
      case CSSPropertyID::kD:
        applicable_types->push_back(
            MakeGarbageCollected<CSSPathInterpolationType>(property));
        break;
      case CSSPropertyID::kBoxShadow:
      case CSSPropertyID::kTextShadow:
        applicable_types->push_back(
            MakeGarbageCollected<CSSShadowListInterpolationType>(property));
        break;
      case CSSPropertyID::kBorderImageSource:
      case CSSPropertyID::kListStyleImage:
      case CSSPropertyID::kWebkitMaskBoxImageSource:
        applicable_types->push_back(
            MakeGarbageCollected<CSSImageInterpolationType>(property));
        break;
      case CSSPropertyID::kBackgroundImage:
        applicable_types->push_back(
            MakeGarbageCollected<CSSImageListInterpolationType>(property));
        break;
      case CSSPropertyID::kStrokeDasharray:
        applicable_types->push_back(
            MakeGarbageCollected<CSSLengthListInterpolationType>(property));
        break;
      case CSSPropertyID::kFontWeight:
        applicable_types->push_back(
            MakeGarbageCollected<CSSFontWeightInterpolationType>(property));
        break;
      case CSSPropertyID::kFontStretch:
        applicable_types->push_back(
            MakeGarbageCollected<CSSFontStretchInterpolationType>(property));
        break;
      case CSSPropertyID::kFontStyle:
        applicable_types->push_back(
            MakeGarbageCollected<CSSFontStyleInterpolationType>(property));
        break;
      case CSSPropertyID::kFontVariationSettings:
        applicable_types->push_back(
            MakeGarbageCollected<CSSFontVariationSettingsInterpolationType>(
                property));
        break;
      case blink::CSSPropertyID::kFontPalette:
        applicable_types->push_back(
            MakeGarbageCollected<CSSFontPaletteInterpolationType>(property));
        break;
      case CSSPropertyID::kVisibility:
        applicable_types->push_back(
            MakeGarbageCollected<CSSVisibilityInterpolationType>(property));
        break;
      case CSSPropertyID::kClip:
        applicable_types->push_back(
            MakeGarbageCollected<CSSClipInterpolationType>(property));
        break;
      case CSSPropertyID::kOffsetRotate:
        applicable_types->push_back(
            MakeGarbageCollected<CSSOffsetRotateInterpolationType>(property));
        break;
      case CSSPropertyID::kBackgroundPositionX:
      case CSSPropertyID::kBackgroundPositionY:
      case CSSPropertyID::kWebkitMaskPositionX:
      case CSSPropertyID::kWebkitMaskPositionY:
        applicable_types->push_back(
            MakeGarbageCollected<CSSPositionAxisListInterpolationType>(
                property));
        break;
      case CSSPropertyID::kObjectPosition:
      case CSSPropertyID::kOffsetAnchor:
      case CSSPropertyID::kOffsetPosition:
      case CSSPropertyID::kPerspectiveOrigin:
        applicable_types->push_back(
            MakeGarbageCollected<CSSPositionInterpolationType>(property));
        break;
      case CSSPropertyID::kBorderBottomLeftRadius:
      case CSSPropertyID::kBorderBottomRightRadius:
      case CSSPropertyID::kBorderTopLeftRadius:
      case CSSPropertyID::kBorderTopRightRadius:
        applicable_types->push_back(
            MakeGarbageCollected<CSSLengthPairInterpolationType>(property));
        break;
      case CSSPropertyID::kTranslate:
        applicable_types->push_back(
            MakeGarbageCollected<CSSTranslateInterpolationType>(property));
        break;
      case CSSPropertyID::kTransformOrigin:
        applicable_types->push_back(
            MakeGarbageCollected<CSSTransformOriginInterpolationType>(
                property));
        break;
      case CSSPropertyID::kBackgroundSize:
      case CSSPropertyID::kMaskSize:
        applicable_types->push_back(
            MakeGarbageCollected<CSSSizeListInterpolationType>(property));
        break;
      case CSSPropertyID::kBorderImageOutset:
      case CSSPropertyID::kBorderImageWidth:
      case CSSPropertyID::kWebkitMaskBoxImageOutset:
      case CSSPropertyID::kWebkitMaskBoxImageWidth:
        applicable_types->push_back(
            MakeGarbageCollected<CSSBorderImageLengthBoxInterpolationType>(
                property));
        break;
      case CSSPropertyID::kScale:
        applicable_types->push_back(
            MakeGarbageCollected<CSSScaleInterpolationType>(property));
        break;
      case CSSPropertyID::kFontSize:
        applicable_types->push_back(
            MakeGarbageCollected<CSSFontSizeInterpolationType>(property));
        break;
      case CSSPropertyID::kFontSizeAdjust:
        applicable_types->push_back(
            MakeGarbageCollected<CSSFontSizeAdjustInterpolationType>(property));
        break;
      case CSSPropertyID::kTextIndent:
        applicable_types->push_back(
            MakeGarbageCollected<CSSTextIndentInterpolationType>(property));
        break;
      case CSSPropertyID::kBorderImageSlice:
      case CSSPropertyID::kWebkitMaskBoxImageSlice:
        applicable_types->push_back(
            MakeGarbageCollected<CSSImageSliceInterpolationType>(property));
        break;
      case CSSPropertyID::kBorderShape:
        applicable_types->push_back(
            MakeGarbageCollected<CSSBorderShapeInterpolationType>(property));
        break;
      case CSSPropertyID::kClipPath:
        applicable_types->push_back(
            MakeGarbageCollected<CSSBasicShapeInterpolationType>(property));
        applicable_types->push_back(
            MakeGarbageCollected<CSSPathInterpolationType>(property));
        applicable_types->push_back(
            MakeGarbageCollected<CSSShapeInterpolationType>(property));
        break;
      case CSSPropertyID::kShapeOutside:
        applicable_types->push_back(
            MakeGarbageCollected<CSSBasicShapeInterpolationType>(property));
        break;
      case CSSPropertyID::kRotate:
        applicable_types->push_back(
            MakeGarbageCollected<CSSRotateInterpolationType>(property));
        break;
      case CSSPropertyID::kBackdropFilter:
      case CSSPropertyID::kFilter:
        applicable_types->push_back(
            MakeGarbageCollected<CSSFilterListInterpolationType>(property));
        break;
      case CSSPropertyID::kTransform:
        applicable_types->push_back(
            MakeGarbageCollected<CSSTransformInterpolationType>(property));
        break;
      case CSSPropertyID::kVariable:
        DCHECK_EQ(GetRegistration(registry_, property), nullptr);
        break;
      case CSSPropertyID::kObjectViewBox:
        applicable_types->push_back(
            MakeGarbageCollected<CSSBasicShapeInterpolationType>(property));
        break;
      case CSSPropertyID::kDisplay:
        applicable_types->push_back(
            MakeGarbageCollected<CSSDisplayInterpolationType>(property));
        break;
      case CSSPropertyID::kContentVisibility:
        applicable_types->push_back(
            MakeGarbageCollected<CSSContentVisibilityInterpolationType>(
                property));
        break;
      case CSSPropertyID::kOverlay:
        applicable_types->push_back(
            MakeGarbageCollected<CSSOverlayInterpolationType>(property));
        break;
      case CSSPropertyID::kScrollbarColor:
        applicable_types->push_back(
            MakeGarbageCollected<CSSScrollbarColorInterpolationType>(property));
        break;
      default:
        DCHECK(!css_property.IsInterpolable());
        break;
    }
  }

  applicable_types->push_back(
      MakeGarbageCollected<CSSDefaultInterpolationType>(property));

  auto add_result = applicable_types_map.insert(property, applicable_types);
  return add_result.stored_value->value;
}

size_t InterpolationTypesMap::Version() const {
  return registry_ ? registry_->Version() : 0;
}

static const CSSInterpolationType* CreateInterpolationTypeForCSSSyntax(
    const CSSSyntaxComponent syntax,
    PropertyHandle property,
    const PropertyRegistration& registration) {
  switch (syntax.GetType()) {
    case CSSSyntaxType::kAngle:
      return MakeGarbageCollected<CSSAngleInterpolationType>(property,
                                                             &registration);
    case CSSSyntaxType::kColor:
      return MakeGarbageCollected<CSSColorInterpolationType>(property,
                                                             &registration);
    case CSSSyntaxType::kLength:
      return MakeGarbageCollected<CSSCustomLengthInterpolationType>(
          property, &registration);
    case CSSSyntaxType::kLengthPercentage:
      return MakeGarbageCollected<CSSLengthInterpolationType>(property,
                                                              &registration);
    case CSSSyntaxType::kPercentage:
      return MakeGarbageCollected<CSSPercentageInterpolationType>(
          property, &registration);
    case CSSSyntaxType::kNumber:
      return MakeGarbageCollected<CSSNumberInterpolationType>(property,
                                                              &registration);
    case CSSSyntaxType::kResolution:
      return MakeGarbageCollected<CSSResolutionInterpolationType>(
          property, &registration);
    case CSSSyntaxType::kTime:
      return MakeGarbageCollected<CSSTimeInterpolationType>(property,
                                                            &registration);
    case CSSSyntaxType::kImage:
      // TODO(andruud): Implement smooth interpolation for gradients.
      return nullptr;
    case CSSSyntaxType::kInteger:
      return MakeGarbageCollected<CSSNumberInterpolationType>(
          property, &registration, true);
    case CSSSyntaxType::kTransformFunction:
      if (!syntax.IsRepeatable() ||
          syntax.GetRepeat() == CSSSyntaxRepeat::kCommaSeparated) {
        // <transform-function> needs an interpolation type different from
        // <transform-function>+ and <transform-list> as it can only use a
        // single function representation for interpolation and composition.
        return MakeGarbageCollected<
            CSSCustomTransformFunctionInterpolationType>(property,
                                                         &registration);
      }
      [[fallthrough]];
    case CSSSyntaxType::kTransformList:
      return MakeGarbageCollected<CSSCustomTransformInterpolationType>(
          property, &registration);
    case CSSSyntaxType::kCustomIdent:
    case CSSSyntaxType::kIdent:
    case CSSSyntaxType::kTokenStream:
    case CSSSyntaxType::kUrl:
      // Smooth interpolation not supported for these types.
      return nullptr;
    case CSSSyntaxType::kString:
      // Smooth interpolation not supported for <string> type.
      return nullptr;
    default:
      NOTREACHED();
  }
}

InterpolationTypes* InterpolationTypesMap::CreateInterpolationTypesForCSSSyntax(
    const AtomicString& property_name,
    const CSSSyntaxDefinition& definition,
    const PropertyRegistration& registration) {
  PropertyHandle property(property_name);
  InterpolationTypes* result = MakeGarbageCollected<InterpolationTypes>();

  // All custom properties may encounter var() dependency cycles.
  result->push_back(MakeGarbageCollected<CSSVarCycleInterpolationType>(
      property, registration));

  for (const CSSSyntaxComponent& component : definition.Components()) {
    const CSSInterpolationType* interpolation_type =
        CreateInterpolationTypeForCSSSyntax(component, property, registration);

    if (!interpolation_type) {
      continue;
    }

    if (component.IsRepeatable() &&
        (component.GetType() != CSSSyntaxType::kTransformFunction ||
         component.GetRepeat() != CSSSyntaxRepeat::kSpaceSeparated)) {
      interpolation_type = MakeGarbageCollected<CSSCustomListInterpolationType>(
          property, &registration, std::move(interpolation_type),
          component.GetType(), component.GetRepeat());
    }

    result->push_back(std::move(interpolation_type));
  }
  result->push_back(
      MakeGarbageCollected<CSSDefaultInterpolationType>(property));
  return result;
}

}  // namespace blink
