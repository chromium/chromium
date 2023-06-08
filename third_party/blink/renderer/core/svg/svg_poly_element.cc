/*
 * Copyright (C) 2004, 2005, 2006, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006, 2007 Rob Buis <buis@kde.org>
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

#include "third_party/blink/renderer/core/svg/svg_poly_element.h"

#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/svg/svg_animated_point_list.h"
#include "third_party/blink/renderer/platform/graphics/path.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

SVGPolyElement::SVGPolyElement(const QualifiedName& tag_name,
                               Document& document)
    : SVGGeometryElement(tag_name, document),
      points_(MakeGarbageCollected<SVGAnimatedPointList>(
          this,
          svg_names::kPointsAttr,
          MakeGarbageCollected<SVGPointList>())) {}

SVGPointListTearOff* SVGPolyElement::pointsFromJavascript() {
  return points_->baseVal();
}

SVGPointListTearOff* SVGPolyElement::animatedPoints() {
  return points_->animVal();
}

void SVGPolyElement::Trace(Visitor* visitor) const {
  visitor->Trace(points_);
  SVGGeometryElement::Trace(visitor);
}

Path SVGPolyElement::AsPathFromPoints() const {
  Path path;
  DCHECK(GetComputedStyle());

  const SVGPointList* points_value = Points()->CurrentValue();
  if (points_value->IsEmpty())
    return path;

  auto it = points_value->begin();
  auto it_end = points_value->end();
  DCHECK(it != it_end);
  path.MoveTo((*it)->Value());
  ++it;

  for (; it != it_end; ++it)
    path.AddLineTo((*it)->Value());

  return path;
}

void SVGPolyElement::SvgAttributeChanged(
    const SvgAttributeChangedParams& params) {
  if (params.name == svg_names::kPointsAttr) {
    GeometryAttributeChanged();
    return;
  }

  SVGGeometryElement::SvgAttributeChanged(params);
}

SVGAnimatedPropertyBase* SVGPolyElement::PropertyFromAttribute(
    const QualifiedName& attribute_name) const {
  if (attribute_name == svg_names::kPointsAttr) {
    return points_.Get();
  } else {
    return SVGGeometryElement::PropertyFromAttribute(attribute_name);
  }
}

void SVGPolyElement::SynchronizeAllSVGAttributes() const {
  SVGAnimatedPropertyBase* attrs[]{points_.Get()};
  SynchronizeListOfSVGAttributes(attrs);
  SVGGeometryElement::SynchronizeAllSVGAttributes();
}

}  // namespace blink
