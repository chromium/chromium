/*
 * Copyright (C) 2004, 2005, 2007 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2008 Rob Buis <buis@kde.org>
 * Copyright (C) 2005, 2007 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2009 Google, Inc.
 * Copyright (C) 2009 Dirk Schulze <krit@webkit.org>
 * Copyright (C) Research In Motion Limited 2010. All rights reserved.
 * Copyright (C) 2009 Jeff Schiller <codedread@gmail.com>
 * Copyright (C) 2011 Renata Hodovan <reni@webkit.org>
 * Copyright (C) 2011 University of Szeged
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

#include "third_party/blink/renderer/core/layout/svg/layout_svg_path.h"

#include "third_party/blink/renderer/core/layout/svg/layout_svg_resource_marker.h"
#include "third_party/blink/renderer/core/layout/svg/svg_resources.h"
#include "third_party/blink/renderer/core/layout/svg/svg_resources_cache.h"
#include "third_party/blink/renderer/core/svg/svg_geometry_element.h"

namespace blink {

LayoutSVGPath::LayoutSVGPath(SVGGeometryElement* node)
    // <line> elements have no joins and thus needn't care about miters.
    : LayoutSVGShape(node, IsA<SVGLineElement>(node) ? kNoMiters : kComplex) {}

LayoutSVGPath::~LayoutSVGPath() = default;

void LayoutSVGPath::StyleDidChange(StyleDifference diff,
                                   const ComputedStyle* old_style) {
  LayoutSVGShape::StyleDidChange(diff, old_style);
  SVGResources::UpdateMarkers(*GetElement(), old_style, StyleRef());
}

void LayoutSVGPath::WillBeDestroyed() {
  SVGResources::ClearMarkers(*GetElement(), Style());
  LayoutSVGShape::WillBeDestroyed();
}

void LayoutSVGPath::UpdateShapeFromElement() {
  LayoutSVGShape::UpdateShapeFromElement();
  UpdateMarkers();
}

const StylePath* LayoutSVGPath::GetStylePath() const {
  if (!IsA<SVGPathElement>(*GetElement()))
    return nullptr;
  return StyleRef().SvgStyle().D();
}

void LayoutSVGPath::UpdateMarkers() {
  marker_positions_.clear();

  if (!StyleRef().SvgStyle().HasMarkers() ||
      !SVGResources::SupportsMarkers(*To<SVGGraphicsElement>(GetElement())))
    return;

  SVGResources* resources =
      SVGResourcesCache::CachedResourcesForLayoutObject(*this);
  if (!resources)
    return;

  LayoutSVGResourceMarker* marker_start = resources->MarkerStart();
  LayoutSVGResourceMarker* marker_mid = resources->MarkerMid();
  LayoutSVGResourceMarker* marker_end = resources->MarkerEnd();
  if (!(marker_start || marker_mid || marker_end))
    return;

  SVGMarkerDataBuilder builder(marker_positions_);
  if (const StylePath* style_path = GetStylePath())
    builder.Build(style_path->ByteStream());
  else
    builder.Build(GetPath());

  if (marker_positions_.IsEmpty())
    return;

  const float stroke_width = StrokeWidthForMarkerUnits();
  FloatRect boundaries;
  for (const auto& position : marker_positions_) {
    if (LayoutSVGResourceMarker* marker =
            position.SelectMarker(marker_start, marker_mid, marker_end)) {
      boundaries.Unite(marker->MarkerBoundaries(
          marker->MarkerTransformation(position, stroke_width)));
    }
  }

  stroke_bounding_box_.Unite(boundaries);
}

}  // namespace blink
