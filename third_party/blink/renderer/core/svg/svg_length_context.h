/*
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

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_SVG_SVG_LENGTH_CONTEXT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_SVG_SVG_LENGTH_CONTEXT_H_

#include "third_party/blink/renderer/core/css/css_primitive_value.h"
#include "third_party/blink/renderer/core/svg/svg_unit_types.h"
#include "third_party/blink/renderer/platform/geometry/float_rect.h"
#include "third_party/blink/renderer/platform/geometry/length.h"

namespace blink {

class ComputedStyle;
class SVGElement;
class SVGLength;
class UnzoomedLength;

enum class SVGLengthMode { kWidth, kHeight, kOther };

class SVGLengthContext {
  STACK_ALLOCATED();

 public:
  explicit SVGLengthContext(const SVGElement*);

  template <typename T>
  static FloatRect ResolveRectangle(const T* context,
                                    SVGUnitTypes::SVGUnitType type,
                                    const FloatRect& viewport) {
    return ResolveRectangle(
        context, type, viewport, *context->x()->CurrentValue(),
        *context->y()->CurrentValue(), *context->width()->CurrentValue(),
        *context->height()->CurrentValue());
  }

  static FloatRect ResolveRectangle(const SVGElement*,
                                    SVGUnitTypes::SVGUnitType,
                                    const FloatRect& viewport,
                                    const SVGLength& x,
                                    const SVGLength& y,
                                    const SVGLength& width,
                                    const SVGLength& height);
  static FloatPoint ResolvePoint(const SVGElement*,
                                 SVGUnitTypes::SVGUnitType,
                                 const SVGLength& x,
                                 const SVGLength& y);
  static float ResolveLength(const SVGElement*,
                             SVGUnitTypes::SVGUnitType,
                             const SVGLength&);
  FloatPoint ResolveLengthPair(const Length& x_length,
                               const Length& y_length,
                               const ComputedStyle&) const;

  float ConvertValueToUserUnits(float,
                                SVGLengthMode,
                                CSSPrimitiveValue::UnitType from_unit) const;
  float ConvertValueFromUserUnits(float,
                                  SVGLengthMode,
                                  CSSPrimitiveValue::UnitType to_unit) const;

  float ValueForLength(const UnzoomedLength&,
                       SVGLengthMode = SVGLengthMode::kOther) const;
  float ValueForLength(const Length&,
                       const ComputedStyle&,
                       SVGLengthMode = SVGLengthMode::kOther) const;
  static float ValueForLength(const Length&,
                              const ComputedStyle&,
                              float dimension);

  bool DetermineViewport(FloatSize&) const;
  float ResolveValue(const CSSPrimitiveValue&, SVGLengthMode) const;

 private:
  float ValueForLength(const Length&, float zoom, SVGLengthMode) const;
  static float ValueForLength(const Length&, float zoom, float dimension);

  float ConvertValueFromUserUnitsToEXS(float value) const;
  float ConvertValueFromEXSToUserUnits(float value) const;

  float ConvertValueFromUserUnitsToCHS(float value) const;
  float ConvertValueFromCHSToUserUnits(float value) const;

  Member<const SVGElement> context_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_SVG_SVG_LENGTH_CONTEXT_H_
