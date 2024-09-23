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

#include "third_party/blink/renderer/core/svg/svg_length_functions.h"

#include <cmath>

#include "third_party/blink/renderer/core/layout/svg/layout_svg_hidden_container.h"
#include "third_party/blink/renderer/core/layout/svg/layout_svg_root.h"
#include "third_party/blink/renderer/core/layout/svg/layout_svg_viewport_container.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/core/svg/svg_element.h"
#include "third_party/blink/renderer/core/svg/svg_symbol_element.h"
#include "third_party/blink/renderer/platform/geometry/length.h"
#include "third_party/blink/renderer/platform/geometry/length_functions.h"
#include "ui/gfx/geometry/size_f.h"
#include "ui/gfx/geometry/vector2d_f.h"

namespace blink {

SVGViewportResolver::SVGViewportResolver(const SVGElement& context)
    : SVGViewportResolver(context.GetLayoutObject()) {}

gfx::SizeF SVGViewportResolver::ResolveViewport() const {
  if (!context_object_) {
    return gfx::SizeF();
  }
  // Root <svg> element lengths are resolved against the top level viewport.
  if (auto* svg_root = DynamicTo<LayoutSVGRoot>(*context_object_)) {
    return svg_root->ViewportSize();
  }
  // Find the nearest viewport object and get the relevant viewport size.
  for (const LayoutObject* object = context_object_->Parent(); object;
       object = object->Parent()) {
    if (auto* outer_svg = DynamicTo<LayoutSVGRoot>(*object)) {
      gfx::SizeF viewbox_size = outer_svg->ViewBoxRect().size();
      if (!viewbox_size.IsEmpty()) {
        return viewbox_size;
      }
      return outer_svg->ViewportSize();
    }
    if (auto* inner_svg = DynamicTo<LayoutSVGViewportContainer>(*object)) {
      gfx::SizeF viewbox_size = inner_svg->ViewBoxRect().size();
      if (!viewbox_size.IsEmpty()) {
        return viewbox_size;
      }
      return inner_svg->Viewport().size();
    }
    if (auto* hidden_container = DynamicTo<LayoutSVGHiddenContainer>(*object)) {
      if (IsA<SVGSymbolElement>(*hidden_container->GetElement())) {
        return gfx::SizeF();
      }
    }
  }
  return gfx::SizeF();
}

float SVGViewportResolver::ViewportDimension(SVGLengthMode mode) const {
  gfx::SizeF viewport_size = ResolveViewport();
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
  NOTREACHED_IN_MIGRATION();
  return 0;
}

float ValueForLength(const Length& length, float zoom, float dimension) {
  DCHECK_NE(zoom, 0);
  // Only "specified" lengths have meaning for SVG.
  if (!length.IsSpecified()) {
    return 0;
  }
  return FloatValueForLength(length, dimension * zoom) / zoom;
}

float ValueForLength(const Length& length,
                     const ComputedStyle& style,
                     float dimension) {
  return ValueForLength(length, style.EffectiveZoom(), dimension);
}

float ValueForLength(const Length& length,
                     const SVGViewportResolver& viewport_resolver,
                     float zoom,
                     SVGLengthMode mode) {
  // The viewport will be unaffected by zoom.
  const float dimension = length.MayHavePercentDependence()
                              ? viewport_resolver.ViewportDimension(mode)
                              : 0;
  return ValueForLength(length, zoom, dimension);
}

float ValueForLength(const Length& length,
                     const SVGViewportResolver& viewport_resolver,
                     const ComputedStyle& style,
                     SVGLengthMode mode) {
  return ValueForLength(length, viewport_resolver, style.EffectiveZoom(), mode);
}

float ValueForLength(const UnzoomedLength& unzoomed_length,
                     const SVGViewportResolver& viewport_resolver,
                     SVGLengthMode mode) {
  return ValueForLength(unzoomed_length.length(), viewport_resolver, 1, mode);
}

gfx::Vector2dF VectorForLengthPair(const Length& x_length,
                                   const Length& y_length,
                                   float zoom,
                                   const gfx::SizeF& viewport_size) {
  gfx::SizeF viewport_size_considering_auto = viewport_size;
  // If either `x_length` or `y_length` is 'auto', set that viewport dimension
  // to zero so that the corresponding Length resolves to zero. This matches
  // the behavior of ValueForLength() below.
  if (x_length.IsAuto()) {
    viewport_size_considering_auto.set_width(0);
  }
  if (y_length.IsAuto()) {
    viewport_size_considering_auto.set_height(0);
  }
  return gfx::Vector2dF(
      ValueForLength(x_length, zoom, viewport_size_considering_auto.width()),
      ValueForLength(y_length, zoom, viewport_size_considering_auto.height()));
}

gfx::Vector2dF VectorForLengthPair(const Length& x_length,
                                   const Length& y_length,
                                   const SVGViewportResolver& viewport_resolver,
                                   const ComputedStyle& style) {
  gfx::SizeF viewport_size;
  if (x_length.MayHavePercentDependence() ||
      y_length.MayHavePercentDependence()) {
    viewport_size = viewport_resolver.ResolveViewport();
  }
  return VectorForLengthPair(x_length, y_length, style.EffectiveZoom(),
                             viewport_size);
}

}  // namespace blink
