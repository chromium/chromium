/*
 * Copyright (C) 2004, 2005, 2006, 2007 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005 Rob Buis <buis@kde.org>
 * Copyright (C) 2005 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2009 Dirk Schulze <krit@webkit.org>
 * Copyright (C) Research In Motion Limited 2010. All rights reserved.
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

#include "third_party/blink/renderer/core/layout/svg/layout_svg_resource_filter.h"

#include "third_party/blink/renderer/core/dom/element_traversal.h"
#include "third_party/blink/renderer/core/svg/svg_animated_length.h"
#include "third_party/blink/renderer/core/svg/svg_fe_image_element.h"
#include "third_party/blink/renderer/core/svg/svg_filter_element.h"

namespace blink {

LayoutSVGResourceFilter::LayoutSVGResourceFilter(SVGFilterElement* node)
    : LayoutSVGResourceContainer(node) {}

LayoutSVGResourceFilter::~LayoutSVGResourceFilter() = default;

bool LayoutSVGResourceFilter::IsChildAllowed(LayoutObject* child,
                                             const ComputedStyle&) const {
  NOT_DESTROYED();
  return child->IsSVGFilterPrimitive();
}

void LayoutSVGResourceFilter::RemoveAllClientsFromCache() {
  NOT_DESTROYED();
  MarkAllClientsForInvalidation(kPaintInvalidation | kFilterCacheInvalidation);
}

gfx::RectF LayoutSVGResourceFilter::ResourceBoundingBox(
    const gfx::RectF& reference_box) const {
  NOT_DESTROYED();
  const auto* filter_element = To<SVGFilterElement>(GetElement());
  return ResolveRectangle(*filter_element, FilterUnits(), reference_box);
}

SVGUnitTypes::SVGUnitType LayoutSVGResourceFilter::FilterUnits() const {
  NOT_DESTROYED();
  return To<SVGFilterElement>(GetElement())->filterUnits()->CurrentEnumValue();
}

SVGUnitTypes::SVGUnitType LayoutSVGResourceFilter::PrimitiveUnits() const {
  NOT_DESTROYED();
  return To<SVGFilterElement>(GetElement())
      ->primitiveUnits()
      ->CurrentEnumValue();
}

bool LayoutSVGResourceFilter::FindCycleFromSelf() const {
  NOT_DESTROYED();
  // Traverse and check all <feImage> 'href' element references.
  for (auto& feimage_element :
       Traversal<SVGFEImageElement>::ChildrenOf(*GetElement())) {
    const SVGElement* target = feimage_element.TargetElement();
    if (!target)
      continue;
    const LayoutObject* target_layout_object = target->GetLayoutObject();
    if (!target_layout_object)
      continue;
    if (FindCycleInSubtree(*target_layout_object))
      return true;
  }
  return false;
}

}  // namespace blink
