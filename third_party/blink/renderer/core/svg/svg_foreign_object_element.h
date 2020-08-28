/*
 * Copyright (C) 2006 Apple Inc. All rights reserved.
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

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_SVG_SVG_FOREIGN_OBJECT_ELEMENT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_SVG_SVG_FOREIGN_OBJECT_ELEMENT_H_

#include "third_party/blink/renderer/core/svg/svg_graphics_element.h"
#include "third_party/blink/renderer/platform/heap/handle.h"

namespace blink {

class SVGAnimatedLength;

class SVGForeignObjectElement final : public SVGGraphicsElement {
  DEFINE_WRAPPERTYPEINFO();

 public:
  explicit SVGForeignObjectElement(Document&);

  SVGAnimatedLength* x() const { return x_.Get(); }
  SVGAnimatedLength* y() const { return y_.Get(); }
  SVGAnimatedLength* width() const { return width_.Get(); }
  SVGAnimatedLength* height() const { return height_.Get(); }

  void Trace(Visitor*) const override;

 private:
  void CollectStyleForPresentationAttribute(
      const QualifiedName&,
      const AtomicString&,
      MutableCSSPropertyValueSet*) override;
  void SvgAttributeChanged(const QualifiedName&) override;

  LayoutObject* CreateLayoutObject(const ComputedStyle&, LegacyLayout) override;

  // As long as the foreignObject element itself creates a legacy layout object,
  // we need to use legacy layout for the entire block formatting context
  // established by the foreignObject. For simplicity, just force legacy for the
  // entire subtree.
  bool TypeShouldForceLegacyLayout() const override { return true; }

  bool SelfHasRelativeLengths() const override;

  Member<SVGAnimatedLength> x_;
  Member<SVGAnimatedLength> y_;
  Member<SVGAnimatedLength> width_;
  Member<SVGAnimatedLength> height_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_SVG_SVG_FOREIGN_OBJECT_ELEMENT_H_
