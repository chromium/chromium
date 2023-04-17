// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/animation/css_interpolation_types_map.h"

#include <memory>
#include <utility>

#include "third_party/blink/public/mojom/permissions_policy/permissions_policy_feature.mojom-blink.h"
#include "third_party/blink/renderer/core/animation/css_angle_interpolation_type.h"
#include "third_party/blink/renderer/core/animation/css_aspect_ratio_interpolation_type.h"
#include "third_party/blink/renderer/core/animation/css_basic_shape_interpolation_type.h"
#include "third_party/blink/renderer/core/animation/css_border_image_length_box_interpolation_type.h"
#include "third_party/blink/renderer/core/animation/css_clip_interpolation_type.h"
#include "third_party/blink/renderer/core/animation/css_color_interpolation_type.h"
#include "third_party/blink/renderer/core/animation/css_content_visibility_interpolation_type.h"
#include "third_party/blink/renderer/core/animation/css_custom_length_interpolation_type.h"
#include "third_party/blink/renderer/core/animation/css_custom_list_interpolation_type.h"
#include "third_party/blink/renderer/core/animation/css_default_interpolation_type.h"
#include "third_party/blink/renderer/core/animation/css_display_interpolation_type.h"
#include "third_party/blink/renderer/core/animation/css_filter_list_interpolation_type.h"
#include "third_party/blink/renderer/core/animation/css_font_size_interpolation_type.h"
#include "third_party/blink/renderer/core/animation/css_font_stretch_interpolation_type.h"
#include "third_party/blink/renderer/core/animation/css_font_style_interpolation_type.h"
#include "third_party/blink/renderer/core/animation/css_font_variation_settings_interpolation_type.h"
#include "third_party/blink/renderer/core/animation/css_font_weight_interpolation_type.h"
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
#include "third_party/blink/renderer/core/animation/css_paint_interpolation_type.h"
#include "third_party/blink/renderer/core/animation/css_path_interpolation_type.h"
#include "third_party/blink/renderer/core/animation/css_percentage_interpolation_type.h"
#include "third_party/blink/renderer/core/animation/css_position_axis_list_interpolation_type.h"
#include "third_party/blink/renderer/core/animation/css_position_interpolation_type.h"
#include "third_party/blink/renderer/core/animation/css_ray_interpolation_type.h"
#include "third_party/blink/renderer/core/animation/css_resolution_interpolation_type.h"
#include "third_party/blink/renderer/core/animation/css_rotate_interpolation_type.h"
#include "third_party/blink/renderer/core/animation/css_scale_interpolation_type.h"
#include "third_party/blink/renderer/core/animation/css_shadow_list_interpolation_type.h"
#include "third_party/blink/renderer/core/animation/css_size_list_interpolation_type.h"
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
#include "third_party/blink/renderer/core/permissions_policy/layout_animations_policy.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

namespace blink {

CSSInterpolationTypesMap::CSSInterpolationTypesMap(
    const PropertyRegistry* registry,
    const Document& document)
    : document_(document), registry_(registry) {
  allow_all_animations_ = document.GetExecutionContext()->IsFeatureEnabled(
      blink::mojom::blink::DocumentPolicyFeature::kLayoutAnimations);
}

static const PropertyRegistration* GetRegistration(
    const PropertyRegistry* registry,
    const PropertyHandle& property) {
  DCHECK(property.IsCSSCustomProperty());
  if (!registry) {
    return nullptr;
  }
  return registry->Registration(property.CustomPropertyName());
}

const InterpolationTypes& CSSInterpolationTypesMap::Get(
    const PropertyHandle& property) const {
  using ApplicableTypesMap =
      HashMap<PropertyHandle, std::unique_ptr<const InterpolationTypes>>;
  DEFINE_STATIC_LOCAL(ApplicableTypesMap, all_applicable_types_map, ());
  DEFINE_STATIC_LOCAL(ApplicableTypesMap, composited_applicable_types_map, ());

  // Reduce motion currently allows no interpolation. When some properties are
  // allowed to interpolate we may need to support the combination of
  // reduce_motion && !allow_all_animations_ separately.
  DEFINE_STATIC_LOCAL(ApplicableTypesMap, reduce_motion_applicable_types_map,
                      ());

  // Custom property interpolation types may change over time so don't trust the
  // applicable_types_map without checking the registry. Also since the static
  // map is shared between documents, the registered type may be different in
  // the different documents.
  if (registry_ && property.IsCSSCustomProperty()) {
    if (const auto* registration = GetRegistration(registry_, property))
      return registration->GetInterpolationTypes();
  }
  bool reduce_motion = document_.ShouldForceReduceMotion();

  ApplicableTypesMap& applicable_types_map =
      reduce_motion ? reduce_motion_applicable_types_map
                    : (allow_all_animations_ ? all_applicable_types_map
                                             : composited_applicable_types_map);

  auto entry = applicable_types_map.find(property);
  if (entry != applicable_types_map.end())
    return *entry->value;

  std::unique_ptr<InterpolationTypes> applicable_types =
      std::make_unique<InterpolationTypes>();

  const CSSProperty& css_property = property.IsCSSProperty()
                                        ? property.GetCSSProperty()
                                        : property.PresentationAttribute();
  // We treat presentation attributes identically to their CSS property
  // equivalents when interpolating.
  PropertyHandle used_property =
      property.IsCSSProperty() ? property : PropertyHandle(css_property);

  // TODO(crbug.com/838263): Support site-defined list of acceptable properties
  // through permissions policy declarations.
  bool property_maybe_blocked_by_permissions_policy =
      LayoutAnimationsPolicy::AffectedCSSProperties().Contains(&css_property);
  if (!reduce_motion && (allow_all_animations_ ||
                         !property_maybe_blocked_by_permissions_policy)) {
    switch (css_property.PropertyID()) {
      case CSSPropertyID::kBaselineShift:
      case CSSPropertyID::kBorderBottomWidth:
      case CSSPropertyID::kBorderLeftWidth:
      case CSSPropertyID::kBorderRightWidth:
      case CSSPropertyID::kBorderTopWidth:
      case CSSPropertyID::kBottom:
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
      case CSSPropertyID::kColumnRuleWidth:
      case CSSPropertyID::kColumnWidth:
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
            std::make_unique<CSSLengthInterpolationType>(used_property));
        break;
      case CSSPropertyID::kAspectRatio:
        applicable_types->push_back(
            std::make_unique<CSSAspectRatioInterpolationType>(used_property));
        break;
      case CSSPropertyID::kGridTemplateColumns:
      case CSSPropertyID::kGridTemplateRows:
        if (RuntimeEnabledFeatures::
                CSSGridTemplatePropertyInterpolationEnabled()) {
          applicable_types->push_back(
              std::make_unique<CSSGridTemplatePropertyInterpolationType>(
                  used_property));
        }
        break;
      case CSSPropertyID::kContainIntrinsicWidth:
      case CSSPropertyID::kContainIntrinsicHeight:
        applicable_types->push_back(
            std::make_unique<CSSIntrinsicLengthInterpolationType>(
                used_property));
        break;
      case CSSPropertyID::kFlexGrow:
      case CSSPropertyID::kFlexShrink:
      case CSSPropertyID::kFillOpacity:
      case CSSPropertyID::kFloodOpacity:
      case CSSPropertyID::kFontSizeAdjust:
      case CSSPropertyID::kOpacity:
      case CSSPropertyID::kOrder:
      case CSSPropertyID::kOrphans:
      case CSSPropertyID::kShapeImageThreshold:
      case CSSPropertyID::kStopOpacity:
      case CSSPropertyID::kStrokeMiterlimit:
      case CSSPropertyID::kStrokeOpacity:
      case CSSPropertyID::kColumnCount:
      case CSSPropertyID::kTextSizeAdjust:
      case CSSPropertyID::kWidows:
      case CSSPropertyID::kZIndex:
        applicable_types->push_back(
            std::make_unique<CSSNumberInterpolationType>(used_property));
        break;
      case CSSPropertyID::kLineHeight:
      case CSSPropertyID::kTabSize:
        applicable_types->push_back(
            std::make_unique<CSSLengthInterpolationType>(used_property));
        applicable_types->push_back(
            std::make_unique<CSSNumberInterpolationType>(used_property));
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
      case CSSPropertyID::kColumnRuleColor:
      case CSSPropertyID::kWebkitTextStrokeColor:
        applicable_types->push_back(
            std::make_unique<CSSColorInterpolationType>(used_property));
        break;
      case CSSPropertyID::kFill:
      case CSSPropertyID::kStroke:
        applicable_types->push_back(
            std::make_unique<CSSPaintInterpolationType>(used_property));
        break;
      case CSSPropertyID::kOffsetPath:
        applicable_types->push_back(
            std::make_unique<CSSRayInterpolationType>(used_property));
        [[fallthrough]];
      case CSSPropertyID::kD:
        applicable_types->push_back(
            std::make_unique<CSSPathInterpolationType>(used_property));
        break;
      case CSSPropertyID::kBoxShadow:
      case CSSPropertyID::kTextShadow:
        applicable_types->push_back(
            std::make_unique<CSSShadowListInterpolationType>(used_property));
        break;
      case CSSPropertyID::kBorderImageSource:
      case CSSPropertyID::kListStyleImage:
      case CSSPropertyID::kWebkitMaskBoxImageSource:
        applicable_types->push_back(
            std::make_unique<CSSImageInterpolationType>(used_property));
        break;
      case CSSPropertyID::kBackgroundImage:
      case CSSPropertyID::kWebkitMaskImage:
        applicable_types->push_back(
            std::make_unique<CSSImageListInterpolationType>(used_property));
        break;
      case CSSPropertyID::kStrokeDasharray:
        applicable_types->push_back(
            std::make_unique<CSSLengthListInterpolationType>(used_property));
        break;
      case CSSPropertyID::kFontWeight:
        applicable_types->push_back(
            std::make_unique<CSSFontWeightInterpolationType>(used_property));
        break;
      case CSSPropertyID::kFontStretch:
        applicable_types->push_back(
            std::make_unique<CSSFontStretchInterpolationType>(used_property));
        break;
      case CSSPropertyID::kFontStyle:
        applicable_types->push_back(
            std::make_unique<CSSFontStyleInterpolationType>(used_property));
        break;
      case CSSPropertyID::kFontVariationSettings:
        applicable_types->push_back(
            std::make_unique<CSSFontVariationSettingsInterpolationType>(
                used_property));
        break;
      case CSSPropertyID::kVisibility:
        applicable_types->push_back(
            std::make_unique<CSSVisibilityInterpolationType>(used_property));
        break;
      case CSSPropertyID::kClip:
        applicable_types->push_back(
            std::make_unique<CSSClipInterpolationType>(used_property));
        break;
      case CSSPropertyID::kOffsetRotate:
        applicable_types->push_back(
            std::make_unique<CSSOffsetRotateInterpolationType>(used_property));
        break;
      case CSSPropertyID::kBackgroundPositionX:
      case CSSPropertyID::kBackgroundPositionY:
      case CSSPropertyID::kWebkitMaskPositionX:
      case CSSPropertyID::kWebkitMaskPositionY:
        applicable_types->push_back(
            std::make_unique<CSSPositionAxisListInterpolationType>(
                used_property));
        break;
      case CSSPropertyID::kObjectPosition:
      case CSSPropertyID::kOffsetAnchor:
      case CSSPropertyID::kOffsetPosition:
      case CSSPropertyID::kPerspectiveOrigin:
        applicable_types->push_back(
            std::make_unique<CSSPositionInterpolationType>(used_property));
        break;
      case CSSPropertyID::kBorderBottomLeftRadius:
      case CSSPropertyID::kBorderBottomRightRadius:
      case CSSPropertyID::kBorderTopLeftRadius:
      case CSSPropertyID::kBorderTopRightRadius:
        applicable_types->push_back(
            std::make_unique<CSSLengthPairInterpolationType>(used_property));
        break;
      case CSSPropertyID::kTranslate:
        applicable_types->push_back(
            std::make_unique<CSSTranslateInterpolationType>(used_property));
        break;
      case CSSPropertyID::kTransformOrigin:
        applicable_types->push_back(
            std::make_unique<CSSTransformOriginInterpolationType>(
                used_property));
        break;
      case CSSPropertyID::kBackgroundSize:
      case CSSPropertyID::kWebkitMaskSize:
        applicable_types->push_back(
            std::make_unique<CSSSizeListInterpolationType>(used_property));
        break;
      case CSSPropertyID::kBorderImageOutset:
      case CSSPropertyID::kBorderImageWidth:
      case CSSPropertyID::kWebkitMaskBoxImageOutset:
      case CSSPropertyID::kWebkitMaskBoxImageWidth:
        applicable_types->push_back(
            std::make_unique<CSSBorderImageLengthBoxInterpolationType>(
                used_property));
        break;
      case CSSPropertyID::kScale:
        applicable_types->push_back(
            std::make_unique<CSSScaleInterpolationType>(used_property));
        break;
      case CSSPropertyID::kFontSize:
        applicable_types->push_back(
            std::make_unique<CSSFontSizeInterpolationType>(used_property));
        break;
      case CSSPropertyID::kTextIndent:
        applicable_types->push_back(
            std::make_unique<CSSTextIndentInterpolationType>(used_property));
        break;
      case CSSPropertyID::kBorderImageSlice:
      case CSSPropertyID::kWebkitMaskBoxImageSlice:
        applicable_types->push_back(
            std::make_unique<CSSImageSliceInterpolationType>(used_property));
        break;
      case CSSPropertyID::kClipPath:
        applicable_types->push_back(
            std::make_unique<CSSBasicShapeInterpolationType>(used_property));
        applicable_types->push_back(
            std::make_unique<CSSPathInterpolationType>(used_property));
        break;
      case CSSPropertyID::kShapeOutside:
        applicable_types->push_back(
            std::make_unique<CSSBasicShapeInterpolationType>(used_property));
        break;
      case CSSPropertyID::kRotate:
        applicable_types->push_back(
            std::make_unique<CSSRotateInterpolationType>(used_property));
        break;
      case CSSPropertyID::kBackdropFilter:
      case CSSPropertyID::kFilter:
        applicable_types->push_back(
            std::make_unique<CSSFilterListInterpolationType>(used_property));
        break;
      case CSSPropertyID::kTransform:
        applicable_types->push_back(
            std::make_unique<CSSTransformInterpolationType>(used_property));
        break;
      case CSSPropertyID::kVariable:
        DCHECK_EQ(GetRegistration(registry_, property), nullptr);
        break;
      case CSSPropertyID::kObjectViewBox:
        applicable_types->push_back(
            std::make_unique<CSSBasicShapeInterpolationType>(used_property));
        break;
      case CSSPropertyID::kDisplay:
        DCHECK(RuntimeEnabledFeatures::CSSDisplayAnimationEnabled());
        applicable_types->push_back(
            std::make_unique<CSSDisplayInterpolationType>(used_property));
        break;
      case CSSPropertyID::kContentVisibility:
        DCHECK(RuntimeEnabledFeatures::CSSDisplayAnimationEnabled());
        applicable_types->push_back(
            std::make_unique<CSSContentVisibilityInterpolationType>(
                used_property));
        break;
      default:
        DCHECK(!css_property.IsInterpolable());
        break;
    }
  }

  applicable_types->push_back(
      std::make_unique<CSSDefaultInterpolationType>(used_property));

  auto add_result =
      applicable_types_map.insert(property, std::move(applicable_types));
  return *add_result.stored_value->value;
}

size_t CSSInterpolationTypesMap::Version() const {
  return registry_ ? registry_->Version() : 0;
}

static std::unique_ptr<CSSInterpolationType>
CreateInterpolationTypeForCSSSyntax(CSSSyntaxType syntax,
                                    PropertyHandle property,
                                    const PropertyRegistration& registration) {
  switch (syntax) {
    case CSSSyntaxType::kAngle:
      return std::make_unique<CSSAngleInterpolationType>(property,
                                                         &registration);
    case CSSSyntaxType::kColor:
      return std::make_unique<CSSColorInterpolationType>(property,
                                                         &registration);
    case CSSSyntaxType::kLength:
      return std::make_unique<CSSCustomLengthInterpolationType>(property,
                                                                &registration);
    case CSSSyntaxType::kLengthPercentage:
      return std::make_unique<CSSLengthInterpolationType>(property,
                                                          &registration);
    case CSSSyntaxType::kPercentage:
      return std::make_unique<CSSPercentageInterpolationType>(property,
                                                              &registration);
    case CSSSyntaxType::kNumber:
      return std::make_unique<CSSNumberInterpolationType>(property,
                                                          &registration);
    case CSSSyntaxType::kResolution:
      return std::make_unique<CSSResolutionInterpolationType>(property,
                                                              &registration);
    case CSSSyntaxType::kTime:
      return std::make_unique<CSSTimeInterpolationType>(property,
                                                        &registration);
    case CSSSyntaxType::kImage:
      // TODO(andruud): Implement smooth interpolation for gradients.
      return nullptr;
    case CSSSyntaxType::kInteger:
      return std::make_unique<CSSNumberInterpolationType>(property,
                                                          &registration, true);
    case CSSSyntaxType::kTransformFunction:
    case CSSSyntaxType::kTransformList:
      // TODO(alancutter): Support smooth interpolation of these types.
      return nullptr;
    case CSSSyntaxType::kCustomIdent:
    case CSSSyntaxType::kIdent:
    case CSSSyntaxType::kTokenStream:
    case CSSSyntaxType::kUrl:
      // Smooth interpolation not supported for these types.
      return nullptr;
    default:
      NOTREACHED();
      return nullptr;
  }
}

InterpolationTypes
CSSInterpolationTypesMap::CreateInterpolationTypesForCSSSyntax(
    const AtomicString& property_name,
    const CSSSyntaxDefinition& definition,
    const PropertyRegistration& registration) {
  PropertyHandle property(property_name);
  InterpolationTypes result;

  // All custom properties may encounter var() dependency cycles.
  result.push_back(
      std::make_unique<CSSVarCycleInterpolationType>(property, registration));

  for (const CSSSyntaxComponent& component : definition.Components()) {
    std::unique_ptr<CSSInterpolationType> interpolation_type =
        CreateInterpolationTypeForCSSSyntax(component.GetType(), property,
                                            registration);

    if (!interpolation_type)
      continue;

    if (component.IsRepeatable()) {
      interpolation_type = std::make_unique<CSSCustomListInterpolationType>(
          property, &registration, std::move(interpolation_type),
          component.GetType(), component.GetRepeat());
    }

    result.push_back(std::move(interpolation_type));
  }
  result.push_back(std::make_unique<CSSDefaultInterpolationType>(property));
  return result;
}

}  // namespace blink
