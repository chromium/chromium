// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_SVG_SVG_VIEWPORT_CONTAINER_ELEMENT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_SVG_SVG_VIEWPORT_CONTAINER_ELEMENT_H_

#include "third_party/blink/renderer/core/svg/svg_fit_to_view_box.h"
#include "third_party/blink/renderer/core/svg/svg_graphics_element.h"

namespace blink {

class SVGAnimatedLength;

// Common base class for elements which may be rendered with a
// LayoutSVGViewportContainer. That is, <svg> and <symbol>.
class SVGViewportContainerElement : public SVGGraphicsElement,
                                    public SVGFitToViewBox {
 protected:
  SVGViewportContainerElement(const QualifiedName& tag_name,
                              Document& document);

 public:
  virtual const SVGRect& CurrentViewBox() const;
  virtual gfx::RectF CurrentViewBoxRect() const;
  virtual AffineTransform ViewBoxToViewTransform(
      const gfx::SizeF& viewport_size) const;

  bool HasEmptyViewBox() const;

  SVGAnimatedLength* GetX() const { return x_.Get(); }
  SVGAnimatedLength* GetY() const { return y_.Get(); }
  SVGAnimatedLength* GetWidth() const { return width_.Get(); }
  SVGAnimatedLength* GetHeight() const { return height_.Get(); }

 protected:
  SVGAnimatedPropertyBase* PropertyFromAttribute(
      const QualifiedName& attribute_name) const override;
  void SynchronizeAllSVGAttributes() const override;
  void SvgAttributeChanged(const SvgAttributeChangedParams&) override;
  bool SelfHasRelativeLengths() const override;
  virtual const SVGPreserveAspectRatio* CurrentPreserveAspectRatio() const;
  void Trace(Visitor* visitor) const override;

  Member<SVGAnimatedLength> x_;
  Member<SVGAnimatedLength> y_;
  Member<SVGAnimatedLength> width_;
  Member<SVGAnimatedLength> height_;

 private:
  bool IsViewportContainerElement() const override { return true; }
};

template <>
struct DowncastTraits<SVGViewportContainerElement> {
  static bool AllowFrom(const Node& node) {
    auto* svg_element = DynamicTo<SVGElement>(node);
    return svg_element && AllowFrom(*svg_element);
  }
  static bool AllowFrom(const SVGElement& svg_element) {
    return svg_element.IsViewportContainerElement();
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_SVG_SVG_VIEWPORT_CONTAINER_ELEMENT_H_
