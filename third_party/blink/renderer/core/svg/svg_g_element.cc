/*
 * Copyright (C) 2004, 2005, 2007, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006 Rob Buis <buis@kde.org>
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

#include "third_party/blink/renderer/core/svg/svg_g_element.h"

#include "third_party/blink/renderer/core/layout/svg/layout_svg_hidden_container.h"
#include "third_party/blink/renderer/core/layout/svg/layout_svg_transformable_container.h"
#include "third_party/blink/renderer/core/svg_names.h"

namespace blink {

SVGGElement::SVGGElement(Document& document, ConstructionType construction_type)
    : SVGGraphicsElement(svg_names::kGTag, document, construction_type) {}

LayoutObject* SVGGElement::CreateLayoutObject(const ComputedStyle& style) {
  // SVG 1.1 testsuite explicitly uses constructs like
  // <g display="none"><linearGradient>
  // We still have to create layoutObjects for the <g> & <linearGradient>
  // element, though the subtree may be hidden - we only want the resource
  // layoutObjects to exist so they can be referenced from somewhere else.
  if (style.Display() == EDisplay::kNone)
    return MakeGarbageCollected<LayoutSVGHiddenContainer>(this);
  if (style.Display() == EDisplay::kContents)
    return nullptr;
  return MakeGarbageCollected<LayoutSVGTransformableContainer>(this);
}

bool SVGGElement::LayoutObjectIsNeeded(const DisplayStyle&) const {
  // Unlike SVGElement::layoutObjectIsNeeded(), we still create layoutObjects,
  // even if display is set to 'none' - which is special to SVG <g> container
  // elements.
  return IsValid() && HasSVGParent();
}

}  // namespace blink
