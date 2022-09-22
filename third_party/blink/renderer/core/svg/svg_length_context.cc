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

#include <cmath>

#include "third_party/blink/renderer/core/css/css_primitive_value.h"
#include "third_party/blink/renderer/core/css/css_resolution_units.h"
#include "third_party/blink/renderer/core/css/css_to_length_conversion_data.h"
#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/layout/adjust_for_absolute_zoom.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/core/svg/svg_length.h"
#include "third_party/blink/renderer/core/svg/svg_svg_element.h"
#include "third_party/blink/renderer/platform/fonts/font_metrics.h"
#include "third_party/blink/renderer/platform/geometry/length_functions.h"
#include "ui/gfx/geometry/size_f.h"
#include "ui/gfx/geometry/vector2d_f.h"

namespace blink {

static inline float DimensionForLengthMode(SVGLengthMode mode,
                                           const gfx::SizeF& viewport_size) {
  switch (mode) {
    case SVGLengthMode::kWidth:
      return viewport_size.width();
    case SVGLengthMode::kHeight:
      return viewport_size.height();
    case SVGLengthMode::kOther:
      // Returns the normalized diagonal length of the viewport, as defined in
      // https://www.w3.org/TR/SVG2/coords.html#Units.
      return ClampTo<float>(std::sqrt(
          gfx::Vector2dF(viewport_size.width(), viewport_size.height())
              .LengthSquared() /
          2));
  }
  NOTREACHED();
  return 0;
}

static float ConvertValueFromPercentageToUserUnits(const SVGLength& value,
                                                   float viewport_dimension) {
  return CSSPrimitiveValue::ClampToCSSLengthRange(
      value.ScaleByPercentage(viewport_dimension));
}

static const ComputedStyle* ComputedStyleForLengthResolving(
    const SVGElement* context) {
  if (!context)
    return nullptr;

  const ContainerNode* current_context = context;
  do {
    if (current_context->GetLayoutObject())
      return current_context->GetLayoutObject()->Style();
    current_context = current_context->parentNode();
  } while (current_context);

  // We can end up here if trying to resolve values for elements in an
  // inactive document.
  return nullptr;
}

static const ComputedStyle* RootElementStyle(const Node* context) {
  if (!context)
    return nullptr;

  const Document& document = context->GetDocument();
  Node* document_element = document.documentElement();
  const ComputedStyle* document_style = document.GetComputedStyle();
  const ComputedStyle* style = document_element && context != document_element
                                   ? document_element->GetComputedStyle()
                                   : document_style;
  if (!style)
    style = document_style;
  return style;
}

static float ConvertValueFromUserUnitsToEMS(const ComputedStyle* style,
                                            float value) {
  if (!style)
    return 0;
  float font_size = style->SpecifiedFontSize();
  if (!font_size)
    return 0;
  return value / font_size;
}

static float ConvertValueFromEMSToUserUnits(const ComputedStyle* style,
                                            float value) {
  if (!style)
    return 0;
  return value * style->SpecifiedFontSize();
}

static inline float ViewportLengthPercent(const float width_or_height) {
  return width_or_height / 100;
}

static inline float ViewportMinPercent(const gfx::SizeF& viewport_size) {
  return std::min(viewport_size.width(), viewport_size.height()) / 100;
}

static inline float ViewportMaxPercent(const gfx::SizeF& viewport_size) {
  return std::max(viewport_size.width(), viewport_size.height()) / 100;
}

static inline float DimensionForViewportUnit(const SVGElement* context,
                                             CSSPrimitiveValue::UnitType unit) {
  if (!context)
    return 0;

  const Document& document = context->GetDocument();
  LocalFrameView* view = document.View();
  if (!view)
    return 0;

  const ComputedStyle* style = ComputedStyleForLengthResolving(context);
  if (!style)
    return 0;

  gfx::SizeF viewport_size(view->Width(), view->Height());

  switch (unit) {
    case CSSPrimitiveValue::UnitType::kViewportWidth:
      return ViewportLengthPercent(viewport_size.width()) /
             style->EffectiveZoom();

    case CSSPrimitiveValue::UnitType::kViewportHeight:
      return ViewportLengthPercent(viewport_size.height()) /
             style->EffectiveZoom();

    case CSSPrimitiveValue::UnitType::kViewportMin:
      return ViewportMinPercent(viewport_size) / style->EffectiveZoom();

    case CSSPrimitiveValue::UnitType::kViewportMax:
      return ViewportMaxPercent(viewport_size) / style->EffectiveZoom();
    default:
      break;
  }

  NOTREACHED();
  return 0;
}

SVGLengthContext::SVGLengthContext(const SVGElement* context)
    : context_(context) {}

gfx::RectF SVGLengthContext::ResolveRectangle(const SVGElement* context,
                                              SVGUnitTypes::SVGUnitType type,
                                              const gfx::RectF& viewport,
                                              const SVGLength& x,
                                              const SVGLength& y,
                                              const SVGLength& width,
                                              const SVGLength& height) {
  DCHECK_NE(SVGUnitTypes::kSvgUnitTypeUnknown, type);
  if (type != SVGUnitTypes::kSvgUnitTypeUserspaceonuse) {
    return gfx::RectF(
        ConvertValueFromPercentageToUserUnits(x, viewport.width()) +
            viewport.x(),
        ConvertValueFromPercentageToUserUnits(y, viewport.height()) +
            viewport.y(),
        ConvertValueFromPercentageToUserUnits(width, viewport.width()),
        ConvertValueFromPercentageToUserUnits(height, viewport.height()));
  }

  SVGLengthContext length_context(context);
  return gfx::RectF(x.Value(length_context), y.Value(length_context),
                    width.Value(length_context), height.Value(length_context));
}

gfx::PointF SVGLengthContext::ResolvePoint(const SVGElement* context,
                                           SVGUnitTypes::SVGUnitType type,
                                           const SVGLength& x,
                                           const SVGLength& y) {
  DCHECK_NE(SVGUnitTypes::kSvgUnitTypeUnknown, type);
  if (type == SVGUnitTypes::kSvgUnitTypeUserspaceonuse) {
    SVGLengthContext length_context(context);
    return gfx::PointF(x.Value(length_context), y.Value(length_context));
  }

  // FIXME: valueAsPercentage() won't be correct for eg. cm units. They need to
  // be resolved in user space and then be considered in objectBoundingBox
  // space.
  return gfx::PointF(x.ValueAsPercentage(), y.ValueAsPercentage());
}

gfx::Vector2dF SVGLengthContext::ResolveLengthPair(
    const Length& x_length,
    const Length& y_length,
    const ComputedStyle& style) const {
  gfx::SizeF viewport_size;
  if (x_length.IsPercentOrCalc() || y_length.IsPercentOrCalc()) {
    DetermineViewport(viewport_size);
    // If either |x_length| or |y_length| is 'auto', set that viewport dimension
    // to zero so that the corresponding Length resolves to zero. This matches
    // the behavior of ValueForLength() below.
    if (x_length.IsAuto())
      viewport_size.set_width(0);
    else if (y_length.IsAuto())
      viewport_size.set_height(0);
  }
  float zoom = style.EffectiveZoom();
  return gfx::Vector2dF(ValueForLength(x_length, zoom, viewport_size.width()),
                        ValueForLength(y_length, zoom, viewport_size.height()));
}

float SVGLengthContext::ResolveLength(const SVGElement* context,
                                      SVGUnitTypes::SVGUnitType type,
                                      const SVGLength& x) {
  DCHECK_NE(SVGUnitTypes::kSvgUnitTypeUnknown, type);
  if (type == SVGUnitTypes::kSvgUnitTypeUserspaceonuse) {
    SVGLengthContext length_context(context);
    return x.Value(length_context);
  }

  // FIXME: valueAsPercentage() won't be correct for eg. cm units. They need to
  // be resolved in user space and then be considered in objectBoundingBox
  // space.
  return x.ValueAsPercentage();
}

float SVGLengthContext::ValueForLength(const UnzoomedLength& unzoomed_length,
                                       SVGLengthMode mode) const {
  return ValueForLength(unzoomed_length.length(), 1, mode);
}

float SVGLengthContext::ValueForLength(const Length& length,
                                       const ComputedStyle& style,
                                       SVGLengthMode mode) const {
  return ValueForLength(length, style.EffectiveZoom(), mode);
}

float SVGLengthContext::ValueForLength(const Length& length,
                                       float zoom,
                                       SVGLengthMode mode) const {
  float dimension = 0;
  if (length.IsPercentOrCalc()) {
    gfx::SizeF viewport_size;
    DetermineViewport(viewport_size);
    // The viewport will be unaffected by zoom.
    dimension = DimensionForLengthMode(mode, viewport_size);
  }
  return ValueForLength(length, zoom, dimension);
}

float SVGLengthContext::ValueForLength(const Length& length,
                                       const ComputedStyle& style,
                                       float dimension) {
  return ValueForLength(length, style.EffectiveZoom(), dimension);
}

float SVGLengthContext::ValueForLength(const Length& length,
                                       float zoom,
                                       float dimension) {
  DCHECK_NE(zoom, 0);
  // Only "specified" lengths have meaning for SVG.
  if (!length.IsSpecified())
    return 0;
  return FloatValueForLength(length, dimension * zoom) / zoom;
}

float SVGLengthContext::ConvertValueToUserUnits(
    float value,
    SVGLengthMode mode,
    CSSPrimitiveValue::UnitType from_unit) const {
  double user_units = value;
  switch (from_unit) {
    case CSSPrimitiveValue::UnitType::kPixels:
    case CSSPrimitiveValue::UnitType::kNumber:
    case CSSPrimitiveValue::UnitType::kInteger:
    case CSSPrimitiveValue::UnitType::kUserUnits:
      user_units = value;
      break;
    case CSSPrimitiveValue::UnitType::kPercentage: {
      gfx::SizeF viewport_size;
      if (!DetermineViewport(viewport_size))
        return 0;
      user_units = value * DimensionForLengthMode(mode, viewport_size) / 100;
      break;
    }
    case CSSPrimitiveValue::UnitType::kEms:
      user_units = ConvertValueFromEMSToUserUnits(
          ComputedStyleForLengthResolving(context_), value);
      break;
    case CSSPrimitiveValue::UnitType::kExs:
      user_units = ConvertValueFromEXSToUserUnits(value);
      break;
    case CSSPrimitiveValue::UnitType::kCentimeters:
      user_units = value * kCssPixelsPerCentimeter;
      break;
    case CSSPrimitiveValue::UnitType::kMillimeters:
      user_units = value * kCssPixelsPerMillimeter;
      break;
    case CSSPrimitiveValue::UnitType::kQuarterMillimeters:
      user_units = value * kCssPixelsPerQuarterMillimeter;
      break;
    case CSSPrimitiveValue::UnitType::kInches:
      user_units = value * kCssPixelsPerInch;
      break;
    case CSSPrimitiveValue::UnitType::kPoints:
      user_units = value * kCssPixelsPerPoint;
      break;
    case CSSPrimitiveValue::UnitType::kPicas:
      user_units = value * kCssPixelsPerPica;
      break;
    case CSSPrimitiveValue::UnitType::kRems:
      user_units =
          ConvertValueFromEMSToUserUnits(RootElementStyle(context_), value);
      break;
    case CSSPrimitiveValue::UnitType::kChs:
      user_units = ConvertValueFromCHSToUserUnits(value);
      break;
    case CSSPrimitiveValue::UnitType::kIcs:
      user_units = ConvertValueFromICSToUserUnits(value);
      break;
    case CSSPrimitiveValue::UnitType::kLhs:
      user_units = ConvertValueFromLHSToUserUnits(value);
      break;
    case CSSPrimitiveValue::UnitType::kViewportWidth:
    case CSSPrimitiveValue::UnitType::kViewportHeight:
    case CSSPrimitiveValue::UnitType::kViewportMin:
    case CSSPrimitiveValue::UnitType::kViewportMax:
      user_units = value * DimensionForViewportUnit(context_, from_unit);
      break;
    case CSSPrimitiveValue::UnitType::kContainerWidth:
    case CSSPrimitiveValue::UnitType::kContainerHeight:
    case CSSPrimitiveValue::UnitType::kContainerInlineSize:
    case CSSPrimitiveValue::UnitType::kContainerBlockSize:
    case CSSPrimitiveValue::UnitType::kContainerMin:
    case CSSPrimitiveValue::UnitType::kContainerMax:
      NOTREACHED() << "Must be handled using ResolveValue";
      break;
    default:
      NOTREACHED();
      break;
  }

  // Since we mix css <length> values with svg's length values we need to
  // clamp values to the narrowest range, otherwise it can result in
  // rendering issues.
  return CSSPrimitiveValue::ClampToCSSLengthRange(user_units);
}

float SVGLengthContext::ConvertValueFromUserUnits(
    float value,
    SVGLengthMode mode,
    CSSPrimitiveValue::UnitType to_unit) const {
  switch (to_unit) {
    case CSSPrimitiveValue::UnitType::kPixels:
    case CSSPrimitiveValue::UnitType::kNumber:
    case CSSPrimitiveValue::UnitType::kInteger:
    case CSSPrimitiveValue::UnitType::kUserUnits:
      return value;
    case CSSPrimitiveValue::UnitType::kPercentage: {
      gfx::SizeF viewport_size;
      if (!DetermineViewport(viewport_size))
        return 0;
      float dimension = DimensionForLengthMode(mode, viewport_size);
      if (!dimension)
        return 0;
      // LengthTypePercentage is represented with 100% = 100.0.
      // Good for accuracy but could eventually be changed.
      return value * 100 / dimension;
    }
    case CSSPrimitiveValue::UnitType::kEms:
      return ConvertValueFromUserUnitsToEMS(
          ComputedStyleForLengthResolving(context_), value);
    case CSSPrimitiveValue::UnitType::kExs:
      return ConvertValueFromUserUnitsToEXS(value);
    case CSSPrimitiveValue::UnitType::kRems:
      return ConvertValueFromUserUnitsToEMS(RootElementStyle(context_), value);
    case CSSPrimitiveValue::UnitType::kChs:
      return ConvertValueFromUserUnitsToCHS(value);
    case CSSPrimitiveValue::UnitType::kIcs:
      return ConvertValueFromUserUnitsToICS(value);
    case CSSPrimitiveValue::UnitType::kLhs:
      return ConvertValueFromUserUnitsToLHS(value);
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
    case CSSPrimitiveValue::UnitType::kViewportWidth:
    case CSSPrimitiveValue::UnitType::kViewportHeight:
    case CSSPrimitiveValue::UnitType::kViewportMin:
    case CSSPrimitiveValue::UnitType::kViewportMax:
      return value / DimensionForViewportUnit(context_, to_unit);
    case CSSPrimitiveValue::UnitType::kContainerWidth:
    case CSSPrimitiveValue::UnitType::kContainerHeight:
    case CSSPrimitiveValue::UnitType::kContainerInlineSize:
    case CSSPrimitiveValue::UnitType::kContainerBlockSize:
    case CSSPrimitiveValue::UnitType::kContainerMin:
    case CSSPrimitiveValue::UnitType::kContainerMax:
      NOTREACHED() << "Must be handled using ResolveValue";
      break;
    default:
      break;
  }

  NOTREACHED();
  return 0;
}

float SVGLengthContext::ConvertValueFromUserUnitsToCHS(float value) const {
  const ComputedStyle* style = ComputedStyleForLengthResolving(context_);
  if (!style)
    return 0;
  const SimpleFontData* font_data = style->GetFont().PrimaryFont();
  if (!font_data)
    return 0;
  float zero_width =
      font_data->GetFontMetrics().ZeroWidth() / style->EffectiveZoom();
  if (!zero_width)
    return 0;
  return value / zero_width;
}

float SVGLengthContext::ConvertValueFromCHSToUserUnits(float value) const {
  const ComputedStyle* style = ComputedStyleForLengthResolving(context_);
  if (!style)
    return 0;
  const SimpleFontData* font_data = style->GetFont().PrimaryFont();
  if (!font_data)
    return 0;
  return value * font_data->GetFontMetrics().ZeroWidth() /
         style->EffectiveZoom();
}

float SVGLengthContext::ConvertValueFromUserUnitsToICS(float value) const {
  const ComputedStyle* style = ComputedStyleForLengthResolving(context_);
  if (!style)
    return 0;
  const SimpleFontData* font_data = style->GetFont().PrimaryFont();
  if (!font_data)
    return 0;
  float ideographic_full_width =
      font_data->GetFontMetrics().IdeographicFullWidth().value_or(
          style->ComputedFontSize()) /
      style->EffectiveZoom();
  if (!ideographic_full_width)
    return 0;
  return value / ideographic_full_width;
}

float SVGLengthContext::ConvertValueFromICSToUserUnits(float value) const {
  const ComputedStyle* style = ComputedStyleForLengthResolving(context_);
  if (!style)
    return 0;
  const SimpleFontData* font_data = style->GetFont().PrimaryFont();
  if (!font_data)
    return 0;
  return value *
         font_data->GetFontMetrics().IdeographicFullWidth().value_or(
             style->ComputedFontSize()) /
         style->EffectiveZoom();
}

float SVGLengthContext::ConvertValueFromUserUnitsToLHS(float value) const {
  const ComputedStyle* style = ComputedStyleForLengthResolving(context_);
  return value / AdjustForAbsoluteZoom::AdjustFloat(style->ComputedLineHeight(),
                                                    *style);
}

float SVGLengthContext::ConvertValueFromLHSToUserUnits(float value) const {
  const ComputedStyle* style = ComputedStyleForLengthResolving(context_);
  return value * AdjustForAbsoluteZoom::AdjustFloat(style->ComputedLineHeight(),
                                                    *style);
}

float SVGLengthContext::ConvertValueFromUserUnitsToEXS(float value) const {
  const ComputedStyle* style = ComputedStyleForLengthResolving(context_);
  if (!style)
    return 0;
  const SimpleFontData* font_data = style->GetFont().PrimaryFont();
  if (!font_data)
    return 0;
  // Use of ceil allows a pixel match to the W3Cs expected output of
  // coords-units-03-b.svg, if this causes problems in real world cases maybe it
  // would be best to remove this.
  float x_height =
      ceilf(font_data->GetFontMetrics().XHeight() / style->EffectiveZoom());
  if (!x_height)
    return 0;
  return value / x_height;
}

float SVGLengthContext::ConvertValueFromEXSToUserUnits(float value) const {
  const ComputedStyle* style = ComputedStyleForLengthResolving(context_);
  if (!style)
    return 0;
  const SimpleFontData* font_data = style->GetFont().PrimaryFont();
  if (!font_data)
    return 0;
  // Use of ceil allows a pixel match to the W3Cs expected output of
  // coords-units-03-b.svg, if this causes problems in real world cases maybe it
  // would be best to remove this.
  return value *
         ceilf(font_data->GetFontMetrics().XHeight() / style->EffectiveZoom());
}

bool SVGLengthContext::DetermineViewport(gfx::SizeF& viewport_size) const {
  if (!context_)
    return false;

  // Root <svg> element lengths are resolved against the top level viewport.
  if (context_->IsOutermostSVGSVGElement()) {
    viewport_size = To<SVGSVGElement>(context_)->CurrentViewportSize();
    return true;
  }

  // Take size from nearest viewport element.
  SVGElement* viewport_element = context_->viewportElement();
  const auto* svg = DynamicTo<SVGSVGElement>(viewport_element);
  if (!svg)
    return false;

  viewport_size = svg->CurrentViewBoxRect().size();
  if (viewport_size.IsEmpty())
    viewport_size = svg->CurrentViewportSize();

  return true;
}

float SVGLengthContext::ResolveValue(const CSSPrimitiveValue& primitive_value,
                                     SVGLengthMode mode) const {
  const ComputedStyle* style = ComputedStyleForLengthResolving(context_);
  if (!style)
    return 0;

  const ComputedStyle* root_style = RootElementStyle(context_);
  if (!root_style)
    return 0;

  DCHECK(context_);
  CSSToLengthConversionData conversion_data = CSSToLengthConversionData(
      style, style, root_style, context_->GetDocument().GetLayoutView(),
      CSSToLengthConversionData::ContainerSizes(
          context_->ParentOrShadowHostElement()),
      1.0f);
  Length length = primitive_value.ConvertToLength(conversion_data);
  return ValueForLength(length, 1.0f, mode);
}
}  // namespace blink
