// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/animation/svg_interpolation_types_map.h"

#include <memory>
#include <utility>

#include "third_party/blink/renderer/core/animation/svg_angle_interpolation_type.h"
#include "third_party/blink/renderer/core/animation/svg_integer_interpolation_type.h"
#include "third_party/blink/renderer/core/animation/svg_integer_optional_integer_interpolation_type.h"
#include "third_party/blink/renderer/core/animation/svg_length_interpolation_type.h"
#include "third_party/blink/renderer/core/animation/svg_length_list_interpolation_type.h"
#include "third_party/blink/renderer/core/animation/svg_number_interpolation_type.h"
#include "third_party/blink/renderer/core/animation/svg_number_list_interpolation_type.h"
#include "third_party/blink/renderer/core/animation/svg_number_optional_number_interpolation_type.h"
#include "third_party/blink/renderer/core/animation/svg_path_interpolation_type.h"
#include "third_party/blink/renderer/core/animation/svg_point_list_interpolation_type.h"
#include "third_party/blink/renderer/core/animation/svg_rect_interpolation_type.h"
#include "third_party/blink/renderer/core/animation/svg_transform_list_interpolation_type.h"
#include "third_party/blink/renderer/core/animation/svg_value_interpolation_type.h"
#include "third_party/blink/renderer/core/html_names.h"

namespace blink {

const InterpolationTypes& SVGInterpolationTypesMap::Get(
    const PropertyHandle& property) const {
  using ApplicableTypesMap =
      HashMap<PropertyHandle, std::unique_ptr<const InterpolationTypes>>;
  DEFINE_STATIC_LOCAL(ApplicableTypesMap, applicable_types_map, ());
  auto entry = applicable_types_map.find(property);
  if (entry != applicable_types_map.end())
    return *entry->value.get();

  std::unique_ptr<InterpolationTypes> applicable_types =
      std::make_unique<InterpolationTypes>();

  const QualifiedName& attribute = property.SvgAttribute();
  if (attribute == svg_names::kOrientAttr) {
    applicable_types->push_back(
        std::make_unique<SVGAngleInterpolationType>(attribute));
  } else if (attribute == svg_names::kNumOctavesAttr ||
             attribute == svg_names::kTargetXAttr ||
             attribute == svg_names::kTargetYAttr) {
    applicable_types->push_back(
        std::make_unique<SVGIntegerInterpolationType>(attribute));
  } else if (attribute == svg_names::kOrderAttr) {
    applicable_types->push_back(
        std::make_unique<SVGIntegerOptionalIntegerInterpolationType>(
            attribute));
  } else if (attribute == svg_names::kCxAttr ||
             attribute == svg_names::kCyAttr ||
             attribute == svg_names::kFxAttr ||
             attribute == svg_names::kFyAttr ||
             attribute == svg_names::kHeightAttr ||
             attribute == svg_names::kMarkerHeightAttr ||
             attribute == svg_names::kMarkerWidthAttr ||
             attribute == svg_names::kRAttr ||
             attribute == svg_names::kRefXAttr ||
             attribute == svg_names::kRefYAttr ||
             attribute == svg_names::kRxAttr ||
             attribute == svg_names::kRyAttr ||
             attribute == svg_names::kStartOffsetAttr ||
             attribute == svg_names::kTextLengthAttr ||
             attribute == svg_names::kWidthAttr ||
             attribute == svg_names::kX1Attr ||
             attribute == svg_names::kX2Attr ||
             attribute == svg_names::kY1Attr ||
             attribute == svg_names::kY2Attr) {
    applicable_types->push_back(
        std::make_unique<SVGLengthInterpolationType>(attribute));
  } else if (attribute == svg_names::kDxAttr ||
             attribute == svg_names::kDyAttr) {
    applicable_types->push_back(
        std::make_unique<SVGNumberInterpolationType>(attribute));
    applicable_types->push_back(
        std::make_unique<SVGLengthListInterpolationType>(attribute));
  } else if (attribute == svg_names::kXAttr || attribute == svg_names::kYAttr) {
    applicable_types->push_back(
        std::make_unique<SVGLengthInterpolationType>(attribute));
    applicable_types->push_back(
        std::make_unique<SVGLengthListInterpolationType>(attribute));
  } else if (attribute == svg_names::kAmplitudeAttr ||
             attribute == svg_names::kAzimuthAttr ||
             attribute == svg_names::kBiasAttr ||
             attribute == svg_names::kDiffuseConstantAttr ||
             attribute == svg_names::kDivisorAttr ||
             attribute == svg_names::kElevationAttr ||
             attribute == svg_names::kExponentAttr ||
             attribute == svg_names::kInterceptAttr ||
             attribute == svg_names::kK1Attr ||
             attribute == svg_names::kK2Attr ||
             attribute == svg_names::kK3Attr ||
             attribute == svg_names::kK4Attr ||
             attribute == svg_names::kLimitingConeAngleAttr ||
             attribute == svg_names::kOffsetAttr ||
             attribute == svg_names::kPathLengthAttr ||
             attribute == svg_names::kPointsAtXAttr ||
             attribute == svg_names::kPointsAtYAttr ||
             attribute == svg_names::kPointsAtZAttr ||
             attribute == svg_names::kScaleAttr ||
             attribute == svg_names::kSeedAttr ||
             attribute == svg_names::kSlopeAttr ||
             attribute == svg_names::kSpecularConstantAttr ||
             attribute == svg_names::kSpecularExponentAttr ||
             attribute == svg_names::kSurfaceScaleAttr ||
             attribute == svg_names::kZAttr) {
    applicable_types->push_back(
        std::make_unique<SVGNumberInterpolationType>(attribute));
  } else if (attribute == svg_names::kKernelMatrixAttr ||
             attribute == svg_names::kRotateAttr ||
             attribute == svg_names::kTableValuesAttr ||
             attribute == svg_names::kValuesAttr) {
    applicable_types->push_back(
        std::make_unique<SVGNumberListInterpolationType>(attribute));
  } else if (attribute == svg_names::kBaseFrequencyAttr ||
             attribute == svg_names::kKernelUnitLengthAttr ||
             attribute == svg_names::kRadiusAttr ||
             attribute == svg_names::kStdDeviationAttr) {
    applicable_types->push_back(
        std::make_unique<SVGNumberOptionalNumberInterpolationType>(attribute));
  } else if (attribute == svg_names::kDAttr) {
    applicable_types->push_back(
        std::make_unique<SVGPathInterpolationType>(attribute));
  } else if (attribute == svg_names::kPointsAttr) {
    applicable_types->push_back(
        std::make_unique<SVGPointListInterpolationType>(attribute));
  } else if (attribute == svg_names::kViewBoxAttr) {
    applicable_types->push_back(
        std::make_unique<SVGRectInterpolationType>(attribute));
  } else if (attribute == svg_names::kGradientTransformAttr ||
             attribute == svg_names::kPatternTransformAttr ||
             attribute == svg_names::kTransformAttr) {
    applicable_types->push_back(
        std::make_unique<SVGTransformListInterpolationType>(attribute));
  } else if (attribute == html_names::kClassAttr ||
             attribute == svg_names::kClipPathUnitsAttr ||
             attribute == svg_names::kEdgeModeAttr ||
             attribute == svg_names::kFilterUnitsAttr ||
             attribute == svg_names::kGradientUnitsAttr ||
             attribute == svg_names::kHrefAttr ||
             attribute == svg_names::kInAttr ||
             attribute == svg_names::kIn2Attr ||
             attribute == svg_names::kLengthAdjustAttr ||
             attribute == svg_names::kMarkerUnitsAttr ||
             attribute == svg_names::kMaskContentUnitsAttr ||
             attribute == svg_names::kMaskUnitsAttr ||
             attribute == svg_names::kMethodAttr ||
             attribute == svg_names::kModeAttr ||
             attribute == svg_names::kOperatorAttr ||
             attribute == svg_names::kPatternContentUnitsAttr ||
             attribute == svg_names::kPatternUnitsAttr ||
             attribute == svg_names::kPreserveAlphaAttr ||
             attribute == svg_names::kPreserveAspectRatioAttr ||
             attribute == svg_names::kPrimitiveUnitsAttr ||
             attribute == svg_names::kResultAttr ||
             attribute == svg_names::kSpacingAttr ||
             attribute == svg_names::kSpreadMethodAttr ||
             attribute == svg_names::kStitchTilesAttr ||
             attribute == svg_names::kTargetAttr ||
             attribute == svg_names::kTypeAttr ||
             attribute == svg_names::kXChannelSelectorAttr ||
             attribute == svg_names::kYChannelSelectorAttr) {
    // Use default SVGValueInterpolationType.
  } else {
    NOTREACHED_IN_MIGRATION();
  }

  applicable_types->push_back(
      std::make_unique<SVGValueInterpolationType>(attribute));

  auto add_result =
      applicable_types_map.insert(property, std::move(applicable_types));
  return *add_result.stored_value->value.get();
}

}  // namespace blink
