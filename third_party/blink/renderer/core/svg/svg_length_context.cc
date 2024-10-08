/*
 * Copyright (C) 2004, 2005, 2006 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006, 2007 Rob Buis <buis@kde.org>
 * Copyright (C) 2007 Apple Inc. All rights reserved.
 * Copyright (C) Research In Motion Limited 2011. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "third_party/blink/renderer/core/svg/svg_length_context.h"

#include "third_party/blink/renderer/core/css/css_math_function_value.h"
#include "third_party/blink/renderer/core/css/css_resolution_units.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver.h"
#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/core/svg/svg_element.h"
#include "third_party/blink/renderer/core/svg/svg_length_functions.h"
#include "ui/gfx/geometry/size_f.h"

namespace blink {

namespace {

const ComputedStyle* RootElementStyle(const Element& element) {
  if (auto* document_element = element.GetDocument().documentElement()) {
    if (element != document_element) {
      return document_element->GetComputedStyle();
    }
  }
  return nullptr;
}

}  // namespace

SVGLengthConversionData::SVGLengthConversionData(const Element& context,
                                                 const ComputedStyle& style)
    : CSSToLengthConversionData(style,
                                &style,
                                RootElementStyle(context),
                                CSSToLengthConversionData::ViewportSize(
                                    context.GetDocument().GetLayoutView()),
                                CSSToLengthConversionData::ContainerSizes(
                                    context.ParentOrShadowHostElement()),
                                CSSToLengthConversionData::AnchorData(),
                                1.0f,
                                ignored_flags_) {}

SVGLengthConversionData::SVGLengthConversionData(const LayoutObject& object)
    : SVGLengthConversionData(To<Element>(*object.GetNode()),
                              object.StyleRef()) {}

SVGLengthContext::SVGLengthContext(const SVGElement* context)
    : context_(context) {}

const ComputedStyle* SVGLengthContext::ComputedStyleForLengthResolving(
    const SVGElement& context) {
  const ContainerNode* current_context = &context;
  do {
    if (current_context->GetLayoutObject()) {
      return current_context->GetLayoutObject()->Style();
    }
    current_context = current_context->parentNode();
  } while (current_context);

  Document& document = context.GetDocument();
  // Detached documents does not have initial style.
  if (document.IsDetached()) {
    return nullptr;
  }
  // We can end up here if trying to resolve values for elements in an
  // inactive document.
  return ComputedStyle::GetInitialStyleSingleton();
}

float SVGLengthContext::ResolveValue(const CSSMathFunctionValue& math_function,
                                     SVGLengthMode mode) const {
  if (!context_) {
    return 0;
  }
  const ComputedStyle* style = ComputedStyleForLengthResolving(*context_);
  if (!style) {
    return 0;
  }
  const SVGLengthConversionData conversion_data(*context_, *style);
  const Length& length = math_function.ConvertToLength(conversion_data);
  const SVGViewportResolver viewport_resolver(*context_);
  return ValueForLength(length, viewport_resolver, 1.0f, mode);
}

double SVGLengthContext::ConvertValueToUserUnitsUnclamped(
    float value,
    SVGLengthMode mode,
    CSSPrimitiveValue::UnitType from_unit) const {
  // Handle absolute units.
  switch (from_unit) {
    case CSSPrimitiveValue::UnitType::kPixels:
    case CSSPrimitiveValue::UnitType::kNumber:
    case CSSPrimitiveValue::UnitType::kInteger:
    case CSSPrimitiveValue::UnitType::kUserUnits:
      return value;
    case CSSPrimitiveValue::UnitType::kCentimeters:
      return value * kCssPixelsPerCentimeter;
    case CSSPrimitiveValue::UnitType::kMillimeters:
      return value * kCssPixelsPerMillimeter;
    case CSSPrimitiveValue::UnitType::kQuarterMillimeters:
      return value * kCssPixelsPerQuarterMillimeter;
    case CSSPrimitiveValue::UnitType::kInches:
      return value * kCssPixelsPerInch;
    case CSSPrimitiveValue::UnitType::kPoints:
      return value * kCssPixelsPerPoint;
    case CSSPrimitiveValue::UnitType::kPicas:
      return value * kCssPixelsPerPica;
    default:
      break;
  }
  if (!context_) {
    return 0;
  }
  // Handle the percentage unit.
  if (from_unit == CSSPrimitiveValue::UnitType::kPercentage) {
    const float dimension =
        SVGViewportResolver(*context_).ViewportDimension(mode);
    return value * dimension / 100;
  }
  // For remaining units, just instantiate a CSSToLengthConversionData object
  // and use that for resolving.
  const ComputedStyle* style = ComputedStyleForLengthResolving(*context_);
  if (!style) {
    return 0;
  }
  const SVGLengthConversionData conversion_data(*context_, *style);
  return conversion_data.ZoomedComputedPixels(value, from_unit);
}

float SVGLengthContext::ConvertValueToUserUnits(
    float value,
    SVGLengthMode mode,
    CSSPrimitiveValue::UnitType from_unit) const {
  // Since we mix css <length> values with svg's length values we need to
  // clamp values to the narrowest range, otherwise it can result in
  // rendering issues.
  return CSSPrimitiveValue::ClampToCSSLengthRange(
      ConvertValueToUserUnitsUnclamped(value, mode, from_unit));
}

float SVGLengthContext::ConvertValueFromUserUnits(
    float value,
    SVGLengthMode mode,
    CSSPrimitiveValue::UnitType to_unit) const {
  // Handle absolute units.
  switch (to_unit) {
    case CSSPrimitiveValue::UnitType::kPixels:
    case CSSPrimitiveValue::UnitType::kNumber:
    case CSSPrimitiveValue::UnitType::kInteger:
    case CSSPrimitiveValue::UnitType::kUserUnits:
      return value;
    case CSSPrimitiveValue::UnitType::kCentimeters:
      return value / kCssPixelsPerCentimeter;
    case CSSPrimitiveValue::UnitType::kMillimeters:
      return value / kCssPixelsPerMillimeter;
    case CSSPrimitiveValue::UnitType::kQuarterMillimeters:
      return value / kCssPixelsPerQuarterMillimeter;
    case CSSPrimitiveValue::UnitType::kInches:
      return value / kCssPixelsPerInch;
    case CSSPrimitiveValue::UnitType::kPoints:
      return value / kCssPixelsPerPoint;
    case CSSPrimitiveValue::UnitType::kPicas:
      return value / kCssPixelsPerPica;
    default:
      break;
  }
  if (!context_) {
    return 0;
  }
  // Handle the percentage unit.
  if (to_unit == CSSPrimitiveValue::UnitType::kPercentage) {
    const float dimension =
        SVGViewportResolver(*context_).ViewportDimension(mode);
    if (!dimension) {
      return 0;
    }
    // LengthTypePercentage is represented with 100% = 100.0.
    // Good for accuracy but could eventually be changed.
    return value * 100 / dimension;
  }
  // For remaining units, just instantiate a CSSToLengthConversionData object
  // and use that for resolving.
  const ComputedStyle* style = ComputedStyleForLengthResolving(*context_);
  if (!style) {
    return 0;
  }
  const SVGLengthConversionData conversion_data(*context_, *style);
  const double reference = conversion_data.ZoomedComputedPixels(1, to_unit);
  if (!reference) {
    return 0;
  }
  return ClampTo<float>(value / reference);
}

}  // namespace blink
