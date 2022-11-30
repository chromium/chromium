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
#include "third_party/blink/renderer/core/svg/svg_geometry_element.h"

namespace blink {

namespace {

bool SupportsMarkers(const SVGGeometryElement& element) {
  return element.HasTagName(svg_names::kLineTag) ||
         element.HasTagName(svg_names::kPathTag) ||
         element.HasTagName(svg_names::kPolygonTag) ||
         element.HasTagName(svg_names::kPolylineTag);
}

}  // namespace

LayoutSVGPath::LayoutSVGPath(SVGGeometryElement* node)
    // <line> elements have no joins and thus needn't care about miters.
    : LayoutSVGShape(node, IsA<SVGLineElement>(node) ? kNoMiters : kComplex) {
  DCHECK(SupportsMarkers(*node));
}

LayoutSVGPath::~LayoutSVGPath() = default;

void LayoutSVGPath::StyleDidChange(StyleDifference diff,
                                   const ComputedStyle* old_style) {
  NOT_DESTROYED();
  LayoutSVGShape::StyleDidChange(diff, old_style);
  SVGResources::UpdateMarkers(*this, old_style);
}

void LayoutSVGPath::WillBeDestroyed() {
  NOT_DESTROYED();
  SVGResources::ClearMarkers(*this, Style());
  LayoutSVGShape::WillBeDestroyed();
}

void LayoutSVGPath::UpdateShapeFromElement() {
  NOT_DESTROYED();
  LayoutSVGShape::UpdateShapeFromElement();
  UpdateMarkers();
}

const StylePath* LayoutSVGPath::GetStylePath() const {
  NOT_DESTROYED();
  if (!IsA<SVGPathElement>(*GetElement()))
    return nullptr;
  return StyleRef().D();
}

void LayoutSVGPath::UpdateMarkers() {
  NOT_DESTROYED();
  marker_positions_.clear();

  const ComputedStyle& style = StyleRef();
  if (!style.HasMarkers())
    return;
  SVGElementResourceClient* client = SVGResources::GetClient(*this);
  if (!client)
    return;
  auto* marker_start = GetSVGResourceAsType<LayoutSVGResourceMarker>(
      *client, style.MarkerStartResource());
  auto* marker_mid = GetSVGResourceAsType<LayoutSVGResourceMarker>(
      *client, style.MarkerMidResource());
  auto* marker_end = GetSVGResourceAsType<LayoutSVGResourceMarker>(
      *client, style.MarkerEndResource());
  if (!(marker_start || marker_mid || marker_end))
    return;

  SVGMarkerDataBuilder builder(marker_positions_);
  if (const StylePath* style_path = GetStylePath())
    builder.Build(style_path->ByteStream());
  else
    builder.Build(GetPath());

  if (marker_positions_.empty())
    return;

  const float stroke_width = StrokeWidthForMarkerUnits();
  gfx::RectF boundaries;
  for (const auto& position : marker_positions_) {
    if (LayoutSVGResourceMarker* marker =
            position.SelectMarker(marker_start, marker_mid, marker_end)) {
      boundaries.Union(marker->MarkerBoundaries(
          marker->MarkerTransformation(position, stroke_width)));
    }
  }

  stroke_bounding_box_.Union(boundaries);
}

}  // namespace blink
