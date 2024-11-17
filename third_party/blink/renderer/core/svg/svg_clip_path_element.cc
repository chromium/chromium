/*
 * Copyright (C) 2004, 2005, 2007, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006, 2007, 2008 Rob Buis <buis@kde.org>
 * Copyright (C) Research In Motion Limited 2009-2010. All rights reserved.
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

#include "third_party/blink/renderer/core/svg/svg_clip_path_element.h"

#include "third_party/blink/renderer/core/layout/svg/layout_svg_resource_clipper.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

SVGClipPathElement::SVGClipPathElement(Document& document)
    : SVGTransformableElement(svg_names::kClipPathTag, document),
      clip_path_units_(MakeGarbageCollected<
                       SVGAnimatedEnumeration<SVGUnitTypes::SVGUnitType>>(
          this,
          svg_names::kClipPathUnitsAttr,
          SVGUnitTypes::kSvgUnitTypeUserspaceonuse)) {}

void SVGClipPathElement::Trace(Visitor* visitor) const {
  visitor->Trace(clip_path_units_);
  SVGTransformableElement::Trace(visitor);
}

void SVGClipPathElement::SvgAttributeChanged(
    const SvgAttributeChangedParams& params) {
  if (params.name == svg_names::kClipPathUnitsAttr) {
    auto* layout_object = To<LayoutSVGResourceContainer>(GetLayoutObject());
    if (layout_object) {
      layout_object->InvalidateCache();
    }
    return;
  }
  SVGTransformableElement::SvgAttributeChanged(params);
}

void SVGClipPathElement::ChildrenChanged(const ChildrenChange& change) {
  SVGTransformableElement::ChildrenChanged(change);

  if (change.ByParser())
    return;

  auto* layout_object = To<LayoutSVGResourceContainer>(GetLayoutObject());
  if (layout_object) {
    layout_object->InvalidateCache();
  }
}

LayoutObject* SVGClipPathElement::CreateLayoutObject(const ComputedStyle&) {
  return MakeGarbageCollected<LayoutSVGResourceClipper>(this);
}

SVGAnimatedPropertyBase* SVGClipPathElement::PropertyFromAttribute(
    const QualifiedName& attribute_name) const {
  if (attribute_name == svg_names::kClipPathUnitsAttr) {
    return clip_path_units_.Get();
  }
  return SVGTransformableElement::PropertyFromAttribute(attribute_name);
}

void SVGClipPathElement::SynchronizeAllSVGAttributes() const {
  SVGAnimatedPropertyBase* attrs[]{clip_path_units_.Get()};
  SynchronizeListOfSVGAttributes(attrs);
  SVGTransformableElement::SynchronizeAllSVGAttributes();
}

}  // namespace blink
