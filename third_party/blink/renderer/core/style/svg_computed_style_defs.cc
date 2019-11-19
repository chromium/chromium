/*
    Copyright (C) 2004, 2005, 2007 Nikolas Zimmermann <zimmermann@kde.org>
                  2004, 2005, 2007 Rob Buis <buis@kde.org>
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

#include "third_party/blink/renderer/core/style/svg_computed_style_defs.h"

#include "third_party/blink/renderer/core/style/data_equivalency.h"
#include "third_party/blink/renderer/core/style/style_svg_resource.h"
#include "third_party/blink/renderer/core/style/svg_computed_style.h"

namespace blink {

SVGPaint::SVGPaint() : type(SVG_PAINTTYPE_NONE) {}
SVGPaint::SVGPaint(Color color) : color(color), type(SVG_PAINTTYPE_RGBCOLOR) {}
SVGPaint::SVGPaint(const SVGPaint& paint) = default;

SVGPaint::~SVGPaint() = default;

SVGPaint& SVGPaint::operator=(const SVGPaint& paint) = default;

bool SVGPaint::operator==(const SVGPaint& other) const {
  return type == other.type && color == other.color &&
         DataEquivalent(resource, other.resource);
}

const AtomicString& SVGPaint::GetUrl() const {
  return Resource()->Url();
}

StyleFillData::StyleFillData()
    : opacity(SVGComputedStyle::InitialFillOpacity()),
      paint(SVGComputedStyle::InitialFillPaint()),
      visited_link_paint(SVGComputedStyle::InitialFillPaint()) {}

StyleFillData::StyleFillData(const StyleFillData& other)
    : RefCounted<StyleFillData>(),
      opacity(other.opacity),
      paint(other.paint),
      visited_link_paint(other.visited_link_paint) {}

bool StyleFillData::operator==(const StyleFillData& other) const {
  return opacity == other.opacity && paint == other.paint &&
         visited_link_paint == other.visited_link_paint;
}

StyleStrokeData::StyleStrokeData()
    : opacity(SVGComputedStyle::InitialStrokeOpacity()),
      miter_limit(SVGComputedStyle::InitialStrokeMiterLimit()),
      width(SVGComputedStyle::InitialStrokeWidth()),
      dash_offset(SVGComputedStyle::InitialStrokeDashOffset()),
      dash_array(SVGComputedStyle::InitialStrokeDashArray()),
      paint(SVGComputedStyle::InitialStrokePaint()),
      visited_link_paint(SVGComputedStyle::InitialStrokePaint()) {}

StyleStrokeData::StyleStrokeData(const StyleStrokeData& other)
    : RefCounted<StyleStrokeData>(),
      opacity(other.opacity),
      miter_limit(other.miter_limit),
      width(other.width),
      dash_offset(other.dash_offset),
      dash_array(other.dash_array),
      paint(other.paint),
      visited_link_paint(other.visited_link_paint) {}

bool StyleStrokeData::operator==(const StyleStrokeData& other) const {
  return width == other.width && opacity == other.opacity &&
         miter_limit == other.miter_limit && dash_offset == other.dash_offset &&
         dash_array->data == other.dash_array->data && paint == other.paint &&
         visited_link_paint == other.visited_link_paint;
}

StyleStopData::StyleStopData()
    : color(SVGComputedStyle::InitialStopColor()),
      opacity(SVGComputedStyle::InitialStopOpacity()) {}

StyleStopData::StyleStopData(const StyleStopData& other)
    : RefCounted<StyleStopData>(), color(other.color), opacity(other.opacity) {}

bool StyleStopData::operator==(const StyleStopData& other) const {
  return color == other.color && opacity == other.opacity;
}

StyleMiscData::StyleMiscData()
    : baseline_shift_value(SVGComputedStyle::InitialBaselineShiftValue()),
      flood_color(SVGComputedStyle::InitialFloodColor()),
      lighting_color(SVGComputedStyle::InitialLightingColor()),
      flood_opacity(SVGComputedStyle::InitialFloodOpacity()),
      flood_color_is_current_color(false),
      lighting_color_is_current_color(false) {}

StyleMiscData::StyleMiscData(const StyleMiscData& other)
    : RefCounted<StyleMiscData>(),
      baseline_shift_value(other.baseline_shift_value),
      flood_color(other.flood_color),
      lighting_color(other.lighting_color),
      flood_opacity(other.flood_opacity),
      flood_color_is_current_color(other.flood_color_is_current_color),
      lighting_color_is_current_color(other.lighting_color_is_current_color) {}

bool StyleMiscData::operator==(const StyleMiscData& other) const {
  return flood_color == other.flood_color &&
         lighting_color == other.lighting_color &&
         baseline_shift_value == other.baseline_shift_value &&
         flood_opacity == other.flood_opacity &&
         flood_color_is_current_color == other.flood_color_is_current_color &&
         lighting_color_is_current_color ==
             other.lighting_color_is_current_color;
}

StyleResourceData::StyleResourceData()
    : masker(SVGComputedStyle::InitialMaskerResource()) {}

StyleResourceData::StyleResourceData(const StyleResourceData& other)
    : RefCounted<StyleResourceData>(), masker(other.masker) {}

StyleResourceData::~StyleResourceData() = default;

bool StyleResourceData::operator==(const StyleResourceData& other) const {
  return DataEquivalent(masker, other.masker);
}

StyleInheritedResourceData::StyleInheritedResourceData()
    : marker_start(SVGComputedStyle::InitialMarkerStartResource()),
      marker_mid(SVGComputedStyle::InitialMarkerMidResource()),
      marker_end(SVGComputedStyle::InitialMarkerEndResource()) {}

StyleInheritedResourceData::StyleInheritedResourceData(
    const StyleInheritedResourceData& other)
    : RefCounted<StyleInheritedResourceData>(),
      marker_start(other.marker_start),
      marker_mid(other.marker_mid),
      marker_end(other.marker_end) {}

StyleInheritedResourceData::~StyleInheritedResourceData() = default;

bool StyleInheritedResourceData::operator==(
    const StyleInheritedResourceData& other) const {
  return DataEquivalent(marker_start, other.marker_start) &&
         DataEquivalent(marker_mid, other.marker_mid) &&
         DataEquivalent(marker_end, other.marker_end);
}

StyleGeometryData::StyleGeometryData()
    : d(SVGComputedStyle::InitialD()),
      cx(SVGComputedStyle::InitialCx()),
      cy(SVGComputedStyle::InitialCy()),
      x(SVGComputedStyle::InitialX()),
      y(SVGComputedStyle::InitialY()),
      r(SVGComputedStyle::InitialR()),
      rx(SVGComputedStyle::InitialRx()),
      ry(SVGComputedStyle::InitialRy()) {}

inline StyleGeometryData::StyleGeometryData(const StyleGeometryData& other)
    : RefCounted<StyleGeometryData>(),
      d(other.d),
      cx(other.cx),
      cy(other.cy),
      x(other.x),
      y(other.y),
      r(other.r),
      rx(other.rx),
      ry(other.ry) {}

scoped_refptr<StyleGeometryData> StyleGeometryData::Copy() const {
  return base::AdoptRef(new StyleGeometryData(*this));
}

bool StyleGeometryData::operator==(const StyleGeometryData& other) const {
  return x == other.x && y == other.y && r == other.r && rx == other.rx &&
         ry == other.ry && cx == other.cx && cy == other.cy &&
         DataEquivalent(d, other.d);
}

}  // namespace blink
