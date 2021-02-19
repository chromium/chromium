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

  inherited_resources = initial_style->inherited_resources;
}

SVGComputedStyle::SVGComputedStyle(CreateInitialType) {
  inherited_resources.Init();
}

SVGComputedStyle::SVGComputedStyle(const SVGComputedStyle& other)
    : RefCounted<SVGComputedStyle>() {
  inherited_resources = other.inherited_resources;
}

SVGComputedStyle::~SVGComputedStyle() = default;

bool SVGComputedStyle::operator==(const SVGComputedStyle& other) const {
  return InheritedEqual(other);
}

bool SVGComputedStyle::InheritedEqual(const SVGComputedStyle& other) const {
  return inherited_resources == other.inherited_resources;
}

void SVGComputedStyle::InheritFrom(const SVGComputedStyle& svg_inherit_parent) {
  inherited_resources = svg_inherit_parent.inherited_resources;
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

  return false;
}

bool SVGComputedStyle::DiffNeedsPaintInvalidation(
    const SVGComputedStyle& other) const {
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
