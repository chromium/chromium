/*
 * Copyright (C) 2004, 2005 Nikolas Zimmermann <zimmermann@kde.org>
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

#include "third_party/blink/renderer/core/svg/svg_symbol_element.h"

#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/layout/svg/layout_svg_hidden_container.h"
#include "third_party/blink/renderer/core/layout/svg/layout_svg_viewport_container.h"
#include "third_party/blink/renderer/core/svg/svg_animated_length.h"
#include "third_party/blink/renderer/core/svg/svg_animated_preserve_aspect_ratio.h"
#include "third_party/blink/renderer/core/svg/svg_animated_rect.h"
#include "third_party/blink/renderer/core/svg_names.h"

namespace blink {

SVGSymbolElement::SVGSymbolElement(Document& document)
    : SVGViewportContainerElement(svg_names::kSymbolTag, document) {}

void SVGSymbolElement::Trace(Visitor* visitor) const {
  SVGViewportContainerElement::Trace(visitor);
}

LayoutObject* SVGSymbolElement::CreateLayoutObject(const ComputedStyle&) {
  if (InUseShadowTree() && IsAtShadowBoundary(this)) {
    return MakeGarbageCollected<LayoutSVGViewportContainer>(this);
  }
  return MakeGarbageCollected<LayoutSVGHiddenContainer>(this);
}

}  // namespace blink
