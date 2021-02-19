/*
    Copyright (C) 2004, 2005, 2007 Nikolas Zimmermann <zimmermann@kde.org>
                  2004, 2005, 2010 Rob Buis <buis@kde.org>
    Copyright (C) Research In Motion Limited 2010. All rights reserved.

    Based on khtml code by:
    Copyright (C) 1999 Antti Koivisto (koivisto@kde.org)
    Copyright (C) 1999-2003 Lars Knoll (knoll@kde.org)
    Copyright (C) 2002-2003 Dirk Mueller (mueller@kde.org)
    Copyright (C) 2002 Apple Computer, Inc.

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#include "third_party/blink/renderer/core/style/svg_computed_style.h"

#include "third_party/blink/renderer/core/style/data_equivalency.h"
#include "third_party/blink/renderer/core/style/style_difference.h"
#include "third_party/blink/renderer/core/style/style_svg_resource.h"

namespace blink {

SVGComputedStyle::SVGComputedStyle() {
  static SVGComputedStyle* initial_style = new SVGComputedStyle(kCreateInitial);

  stroke = initial_style->stroke;
  inherited_resources = initial_style->inherited_resources;

  SetBitDefaults();
}

SVGComputedStyle::SVGComputedStyle(CreateInitialType) {
  SetBitDefaults();

  stroke.Init();
  inherited_resources.Init();
}

SVGComputedStyle::SVGComputedStyle(const SVGComputedStyle& other)
    : RefCounted<SVGComputedStyle>() {
  stroke = other.stroke;
  inherited_resources = other.inherited_resources;

  svg_inherited_flags = other.svg_inherited_flags;
}

SVGComputedStyle::~SVGComputedStyle() = default;

bool SVGComputedStyle::operator==(const SVGComputedStyle& other) const {
  return InheritedEqual(other);
}

bool SVGComputedStyle::InheritedEqual(const SVGComputedStyle& other) const {
  return stroke == other.stroke &&
         inherited_resources == other.inherited_resources &&
         svg_inherited_flags == other.svg_inherited_flags;
}

void SVGComputedStyle::InheritFrom(const SVGComputedStyle& svg_inherit_parent) {
  stroke = svg_inherit_parent.stroke;
  inherited_resources = svg_inherit_parent.inherited_resources;

  svg_inherited_flags = svg_inherit_parent.svg_inherited_flags;
}

scoped_refptr<SVGDashArray> SVGComputedStyle::InitialStrokeDashArray() {
  DEFINE_STATIC_REF(SVGDashArray, initial_dash_array,
                    base::MakeRefCounted<SVGDashArray>());
  return initial_dash_array;
}

StyleDifference SVGComputedStyle::Diff(const SVGComputedStyle& other) const {
  StyleDifference style_difference;

  if (DiffNeedsLayoutAndPaintInvalidation(other)) {
    style_difference.SetNeedsFullLayout();
    style_difference.SetNeedsPaintInvalidation();
  } else if (DiffNeedsPaintInvalidation(other)) {
    style_difference.SetNeedsPaintInvalidation();
  }

  return style_difference;
}

bool SVGComputedStyle::DiffNeedsLayoutAndPaintInvalidation(
    const SVGComputedStyle& other) const {
  // If markers change, we need a relayout, as marker boundaries are cached in
  // LayoutSVGPath.
  if (inherited_resources != other.inherited_resources)
    return true;

  // These properties affect the cached stroke bounding box rects.
  if (svg_inherited_flags.cap_style != other.svg_inherited_flags.cap_style ||
      svg_inherited_flags.join_style != other.svg_inherited_flags.join_style)
    return true;

  // Some stroke properties require relayouts as the cached stroke boundaries
  // need to be recalculated.
  if (stroke.Get() != other.stroke.Get()) {
    if (stroke->width != other.stroke->width ||
        stroke->miter_limit != other.stroke->miter_limit)
      return true;
    // If the stroke is toggled from/to 'none' we need to relayout, because the
    // stroke shape will have changed.
    if (stroke->paint.IsNone() != other.stroke->paint.IsNone())
      return true;
    // If the dash array is toggled from/to 'none' we need to relayout, because
    // some shapes will decide on which codepath to use based on the presence
    // of a dash array.
    if (stroke->dash_array->data.IsEmpty() !=
        other.stroke->dash_array->data.IsEmpty())
      return true;
  }

  return false;
}

bool SVGComputedStyle::DiffNeedsPaintInvalidation(
    const SVGComputedStyle& other) const {
  if (stroke.Get() != other.stroke.Get()) {
    if (stroke->paint != other.stroke->paint ||
        stroke->opacity != other.stroke->opacity ||
        stroke->visited_link_paint != other.stroke->visited_link_paint)
      return true;
    // Changes to the dash effect only require a repaint because we don't
    // include it when computing (approximating) the stroke boundaries during
    // layout.
    if (stroke->dash_offset != other.stroke->dash_offset ||
        stroke->dash_array->data != other.stroke->dash_array->data)
      return true;
  }

  return false;
}

void SVGComputedStyle::SetMarkerStartResource(
    scoped_refptr<StyleSVGResource> resource) {
  if (!(inherited_resources->marker_start == resource))
    inherited_resources.Access()->marker_start = std::move(resource);
}

void SVGComputedStyle::SetMarkerMidResource(
    scoped_refptr<StyleSVGResource> resource) {
  if (!(inherited_resources->marker_mid == resource))
    inherited_resources.Access()->marker_mid = std::move(resource);
}

void SVGComputedStyle::SetMarkerEndResource(
    scoped_refptr<StyleSVGResource> resource) {
  if (!(inherited_resources->marker_end == resource))
    inherited_resources.Access()->marker_end = std::move(resource);
}

}  // namespace blink
