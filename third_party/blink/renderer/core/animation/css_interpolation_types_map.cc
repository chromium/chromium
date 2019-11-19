// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/animation/css_interpolation_types_map.h"

#include <memory>
#include <utility>

#include "third_party/blink/public/mojom/feature_policy/feature_policy_feature.mojom-blink.h"
#include "third_party/blink/renderer/core/animation/css_angle_interpolation_type.h"
#include "third_party/blink/renderer/core/animation/css_basic_shape_interpolation_type.h"
#include "third_party/blink/renderer/core/animation/css_border_image_length_box_interpolation_type.h"
#include "third_party/blink/renderer/core/animation/css_clip_interpolation_type.h"
#include "third_party/blink/renderer/core/animation/css_color_interpolation_type.h"
#include "third_party/blink/renderer/core/animation/css_custom_length_interpolation_type.h"
#include "third_party/blink/renderer/core/animation/css_custom_list_interpolation_type.h"
#include "third_party/blink/renderer/core/animation/css_default_interpolation_type.h"
#include "third_party/blink/renderer/core/animation/css_filter_list_interpolation_type.h"
#include "third_party/blink/renderer/core/animation/css_font_size_interpolation_type.h"
#include "third_party/blink/renderer/core/animation/css_font_variation_settings_interpolation_type.h"
#include "third_party/blink/renderer/core/animation/css_font_weight_interpolation_type.h"
#include "third_party/blink/renderer/core/animation/css_image_interpolation_type.h"
#include "third_party/blink/renderer/core/animation/css_image_list_interpolation_type.h"
#include "third_party/blink/renderer/core/animation/css_image_slice_interpolation_type.h"
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
#include "third_party/blink/renderer/core/css/css_syntax_definition.h"
#include "third_party/blink/renderer/core/css/properties/css_property.h"
#include "third_party/blink/renderer/core/css/property_registry.h"
#include "third_party/blink/renderer/core/feature_policy/layout_animations_policy.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"

namespace blink {

CSSInterpolationTypesMap::CSSInterpolationTypesMap(
    const PropertyRegistry* registry,
    const Document& document)
    : registry_(registry) {
  allow_all_animations_ = document.IsFeatureEnabled(
      blink::mojom::FeaturePolicyFeature::kLayoutAnimations);
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
  // TODO(iclelland): Combine these two hashmaps into a single map on
  // std::pair<bool,property>
  DEFINE_STATIC_LOCAL(ApplicableTypesMap, all_applicable_types_map, ());
  DEFINE_STATIC_LOCAL(ApplicableTypesMap, composited_applicable_types_map, ());

  ApplicableTypesMap& applicable_types_map =
      allow_all_animations_ ? all_applicable_types_map
                            : composited_applicable_types_map;

  auto entry = applicable_types_map.find(property);
  bool found_entry = entry != applicable_types_map.end();

  // Custom property interpolation types may change over time so don't trust the
  // applicableTypesMap without checking the registry.
  if (registry_ && property.IsCSSCustomProperty()) {
    const auto* registration = GetRegistration(registry_.Get(), property);
    if (registration) {
      if (found_entry) {
        applicable_types_map.erase(entry);
      }
      return registration->GetInterpolationTypes();
    }
  }

  if (found_entry) {
    return *entry->value;
  }

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
  // through feature policy declarations.
  bool property_maybe_blocked_by_feature_policy =
      LayoutAnimationsPolicy::AffectedCSSProperties().Contains(&css_property);
  if (allow_all_animations_ || !property_maybe_blocked_by_feature_policy) {
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
        applicable_types->push_back(
            std::make_unique<CSSLengthInterpolationType>(used_property));
        applicable_types->push_back(
            std::make_unique<CSSNumberInterpolationType>(used_property));
        break;
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
        FALLTHROUGH;
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
        DCHECK_EQ(GetRegistration(registry_.Get(), property), nullptr);
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
  // Property registrations are never removed so the number of registered
  // custom properties is equivalent to how many changes there have been to the
  // property registry.
  return registry_ ? registry_->RegistrationCount() : 0;
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
