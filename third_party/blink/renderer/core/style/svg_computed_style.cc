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

#include "third_party/blink/renderer/core/style/style_difference.h"
#include "third_party/blink/renderer/core/style/style_svg_resource.h"

namespace blink {

static const int kPaintOrderBitwidth = 2;

SVGComputedStyle::SVGComputedStyle() {
  static SVGComputedStyle* initial_style = new SVGComputedStyle(kCreateInitial);

  fill = initial_style->fill;
  stroke = initial_style->stroke;
  stops = initial_style->stops;
  misc = initial_style->misc;
  inherited_resources = initial_style->inherited_resources;
  geometry = initial_style->geometry;
  resources = initial_style->resources;

  SetBitDefaults();
}

SVGComputedStyle::SVGComputedStyle(CreateInitialType) {
  SetBitDefaults();

  fill.Init();
  stroke.Init();
  stops.Init();
  misc.Init();
  inherited_resources.Init();
  geometry.Init();
  resources.Init();
}

SVGComputedStyle::SVGComputedStyle(const SVGComputedStyle& other)
    : RefCounted<SVGComputedStyle>() {
  fill = other.fill;
  stroke = other.stroke;
  stops = other.stops;
  misc = other.misc;
  inherited_resources = other.inherited_resources;
  geometry = other.geometry;
  resources = other.resources;

  svg_inherited_flags = other.svg_inherited_flags;
  svg_noninherited_flags = other.svg_noninherited_flags;
}

SVGComputedStyle::~SVGComputedStyle() = default;

bool SVGComputedStyle::operator==(const SVGComputedStyle& other) const {
  return InheritedEqual(other) && NonInheritedEqual(other);
}

bool SVGComputedStyle::InheritedEqual(const SVGComputedStyle& other) const {
  return fill == other.fill && stroke == other.stroke &&
         inherited_resources == other.inherited_resources &&
         svg_inherited_flags == other.svg_inherited_flags;
}

bool SVGComputedStyle::NonInheritedEqual(const SVGComputedStyle& other) const {
  return stops == other.stops && misc == other.misc &&
         geometry == other.geometry && resources == other.resources &&
         svg_noninherited_flags == other.svg_noninherited_flags;
}

void SVGComputedStyle::InheritFrom(const SVGComputedStyle& svg_inherit_parent) {
  fill = svg_inherit_parent.fill;
  stroke = svg_inherit_parent.stroke;
  inherited_resources = svg_inherit_parent.inherited_resources;

  svg_inherited_flags = svg_inherit_parent.svg_inherited_flags;
}

void SVGComputedStyle::CopyNonInheritedFromCached(
    const SVGComputedStyle& other) {
  svg_noninherited_flags = other.svg_noninherited_flags;
  stops = other.stops;
  misc = other.misc;
  geometry = other.geometry;
  resources = other.resources;
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
    style_difference.SetNeedsPaintInvalidationObject();
  } else if (DiffNeedsPaintInvalidation(other)) {
    style_difference.SetNeedsPaintInvalidationObject();
  }

  return style_difference;
}

bool SVGComputedStyle::DiffNeedsLayoutAndPaintInvalidation(
    const SVGComputedStyle& other) const {
  // If resources change, we need a relayout, as the presence of resources
  // influences the visual rect.
  if (resources != other.resources)
    return true;

  // If markers change, we need a relayout, as marker boundaries are cached in
  // LayoutSVGPath.
  if (inherited_resources != other.inherited_resources)
    return true;

  // All text related properties influence layout.
  if (svg_inherited_flags.text_anchor !=
          other.svg_inherited_flags.text_anchor ||
      svg_inherited_flags.dominant_baseline !=
          other.svg_inherited_flags.dominant_baseline ||
      svg_noninherited_flags.f.alignment_baseline !=
          other.svg_noninherited_flags.f.alignment_baseline ||
      svg_noninherited_flags.f.baseline_shift !=
          other.svg_noninherited_flags.f.baseline_shift)
    return true;

  // Text related properties influence layout.
  if (misc->baseline_shift_value != other.misc->baseline_shift_value)
    return true;

  // These properties affect the cached stroke bounding box rects.
  if (svg_inherited_flags.cap_style != other.svg_inherited_flags.cap_style ||
      svg_inherited_flags.join_style != other.svg_inherited_flags.join_style)
    return true;

  // vector-effect changes require a re-layout.
  if (svg_noninherited_flags.f.vector_effect !=
      other.svg_noninherited_flags.f.vector_effect)
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

  // The geometry properties require a re-layout.
  if (geometry.Get() != other.geometry.Get() && *geometry != *other.geometry)
    return true;

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

  // Painting related properties only need paint invalidation.
  if (misc.Get() != other.misc.Get()) {
    if (misc->flood_color != other.misc->flood_color ||
        misc->flood_color_is_current_color !=
            other.misc->flood_color_is_current_color ||
        misc->flood_opacity != other.misc->flood_opacity ||
        misc->lighting_color != other.misc->lighting_color ||
        misc->lighting_color_is_current_color !=
            other.misc->lighting_color_is_current_color)
      return true;
  }

  // If fill changes, we just need to issue paint invalidations. Fill boundaries
  // are not influenced by this, only by the Path, that LayoutSVGPath contains.
  if (fill.Get() != other.fill.Get()) {
    if (fill->paint != other.fill->paint ||
        fill->opacity != other.fill->opacity)
      return true;
  }

  // If gradient stops change, we just need to issue paint invalidations. Style
  // updates are already handled through SVGStopElement.
  if (stops != other.stops)
    return true;

  // Changes of these flags only cause paint invalidations.
  if (svg_inherited_flags.shape_rendering !=
          other.svg_inherited_flags.shape_rendering ||
      svg_inherited_flags.clip_rule != other.svg_inherited_flags.clip_rule ||
      svg_inherited_flags.fill_rule != other.svg_inherited_flags.fill_rule ||
      svg_inherited_flags.color_interpolation !=
          other.svg_inherited_flags.color_interpolation ||
      svg_inherited_flags.color_interpolation_filters !=
          other.svg_inherited_flags.color_interpolation_filters ||
      svg_inherited_flags.paint_order != other.svg_inherited_flags.paint_order)
    return true;

  if (svg_noninherited_flags.f.mask_type !=
      other.svg_noninherited_flags.f.mask_type)
    return true;

  return false;
}

unsigned PaintOrderSequence(EPaintOrderType first,
                            EPaintOrderType second,
                            EPaintOrderType third) {
  return (((third << kPaintOrderBitwidth) | second) << kPaintOrderBitwidth) |
         first;
}

EPaintOrderType SVGComputedStyle::PaintOrderType(unsigned index) const {
  unsigned pt = 0;
  DCHECK(index < ((1 << kPaintOrderBitwidth) - 1));
  switch (this->PaintOrder()) {
    case kPaintOrderNormal:
    case kPaintOrderFillStrokeMarkers:
      pt = PaintOrderSequence(PT_FILL, PT_STROKE, PT_MARKERS);
      break;
    case kPaintOrderFillMarkersStroke:
      pt = PaintOrderSequence(PT_FILL, PT_MARKERS, PT_STROKE);
      break;
    case kPaintOrderStrokeFillMarkers:
      pt = PaintOrderSequence(PT_STROKE, PT_FILL, PT_MARKERS);
      break;
    case kPaintOrderStrokeMarkersFill:
      pt = PaintOrderSequence(PT_STROKE, PT_MARKERS, PT_FILL);
      break;
    case kPaintOrderMarkersFillStroke:
      pt = PaintOrderSequence(PT_MARKERS, PT_FILL, PT_STROKE);
      break;
    case kPaintOrderMarkersStrokeFill:
      pt = PaintOrderSequence(PT_MARKERS, PT_STROKE, PT_FILL);
      break;
  }

  pt =
      (pt >> (kPaintOrderBitwidth * index)) & ((1u << kPaintOrderBitwidth) - 1);
  return (EPaintOrderType)pt;
}

void SVGComputedStyle::SetMaskerResource(
    scoped_refptr<StyleSVGResource> resource) {
  if (!(resources->masker == resource))
    resources.Access()->masker = std::move(resource);
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
