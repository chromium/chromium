/*
 * Copyright (C) 2004, 2005, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006, 2007 Rob Buis <buis@kde.org>
 * Copyright (C) 2014 Google, Inc.
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

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_SVG_SVG_GRAPHICS_ELEMENT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_SVG_SVG_GRAPHICS_ELEMENT_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/svg/svg_element.h"
#include "third_party/blink/renderer/core/svg/svg_tests.h"
#include "third_party/blink/renderer/platform/heap/handle.h"

namespace blink {

class AffineTransform;
class FloatRect;
class SVGAnimatedTransformList;
class SVGMatrixTearOff;
class SVGRectTearOff;

class CORE_EXPORT SVGGraphicsElement : public SVGElement, public SVGTests {
  DEFINE_WRAPPERTYPEINFO();

 public:
  ~SVGGraphicsElement() override;

  SVGMatrixTearOff* getCTM();
  SVGMatrixTearOff* getScreenCTM();

  SVGElement* nearestViewportElement() const;
  SVGElement* farthestViewportElement() const;

  AffineTransform LocalCoordinateSpaceTransform(CTMScope) const override;
  AffineTransform* AnimateMotionTransform() override;

  virtual FloatRect GetBBox();
  SVGRectTearOff* getBBoxFromJavascript();

  bool IsValid() const final { return SVGTests::IsValid(); }

  SVGAnimatedTransformList* transform() { return transform_.Get(); }
  const SVGAnimatedTransformList* transform() const { return transform_.Get(); }

  AffineTransform ComputeCTM(
      CTMScope mode,
      const SVGGraphicsElement* ancestor = nullptr) const;

  void Trace(Visitor*) const override;

 protected:
  SVGGraphicsElement(const QualifiedName&,
                     Document&,
                     ConstructionType = kCreateSVGElement);

  bool SupportsFocus() const override {
    return Element::SupportsFocus() || HasFocusEventListeners();
  }

  void CollectStyleForPresentationAttribute(
      const QualifiedName&,
      const AtomicString&,
      MutableCSSPropertyValueSet*) override;
  void SvgAttributeChanged(const QualifiedName&) override;

  Member<SVGAnimatedTransformList> transform_;

 private:
  bool IsSVGGraphicsElement() const final { return true; }
};

template <>
inline bool IsElementOfType<const SVGGraphicsElement>(const Node& node) {
  return IsA<SVGGraphicsElement>(node);
}
template <>
struct DowncastTraits<SVGGraphicsElement> {
  static bool AllowFrom(const Node& node) {
    auto* svg_element = DynamicTo<SVGElement>(node);
    return svg_element && AllowFrom(*svg_element);
  }
  static bool AllowFrom(const SVGElement& svg_element) {
    return svg_element.IsSVGGraphicsElement();
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_SVG_SVG_GRAPHICS_ELEMENT_H_
