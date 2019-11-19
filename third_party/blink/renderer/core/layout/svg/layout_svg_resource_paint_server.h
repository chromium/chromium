/*
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

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_SVG_LAYOUT_SVG_RESOURCE_PAINT_SERVER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_SVG_LAYOUT_SVG_RESOURCE_PAINT_SERVER_H_

#include "third_party/blink/renderer/core/layout/svg/layout_svg_resource_container.h"
#include "third_party/blink/renderer/platform/graphics/color.h"
#include "third_party/blink/renderer/platform/graphics/gradient.h"
#include "third_party/blink/renderer/platform/graphics/pattern.h"
#include "third_party/blink/renderer/platform/transforms/affine_transform.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

enum LayoutSVGResourceMode {
  kApplyToFillMode,
  kApplyToStrokeMode,
};

class LayoutObject;
class LayoutSVGResourcePaintServer;
class ComputedStyle;

class SVGPaintServer {
  STACK_ALLOCATED();

 public:
  explicit SVGPaintServer(Color);
  SVGPaintServer(scoped_refptr<Gradient>, const AffineTransform&);
  SVGPaintServer(scoped_refptr<Pattern>, const AffineTransform&);

  static SVGPaintServer RequestForLayoutObject(const LayoutObject&,
                                               const ComputedStyle&,
                                               LayoutSVGResourceMode);
  static bool ExistsForLayoutObject(const LayoutObject&,
                                    const ComputedStyle&,
                                    LayoutSVGResourceMode);

  void ApplyToPaintFlags(PaintFlags&, float alpha);

  static SVGPaintServer Invalid() {
    return SVGPaintServer(Color(Color::kTransparent));
  }
  bool IsValid() const { return color_ != Color::kTransparent; }

  bool IsTransformDependent() const { return gradient_ || pattern_; }
  void PrependTransform(const AffineTransform&);

 private:
  scoped_refptr<Gradient> gradient_;
  scoped_refptr<Pattern> pattern_;
  AffineTransform transform_;  // Used for gradient/pattern shaders.
  Color color_;
};

// If |SVGPaintDescription::hasFallback| is true, |SVGPaintDescription::color|
// is set to a fallback color.
struct SVGPaintDescription {
  STACK_ALLOCATED();

 public:
  SVGPaintDescription()
      : resource(nullptr), is_valid(false), has_fallback(false) {}
  SVGPaintDescription(Color color)
      : resource(nullptr), color(color), is_valid(true), has_fallback(false) {}
  SVGPaintDescription(LayoutSVGResourcePaintServer* resource)
      : resource(resource), is_valid(true), has_fallback(false) {
    DCHECK(resource);
  }
  SVGPaintDescription(LayoutSVGResourcePaintServer* resource,
                      Color fallback_color)
      : resource(resource),
        color(fallback_color),
        is_valid(true),
        has_fallback(true) {
    DCHECK(resource);
  }

  LayoutSVGResourcePaintServer* resource;
  Color color;
  bool is_valid;
  bool has_fallback;
};

class LayoutSVGResourcePaintServer : public LayoutSVGResourceContainer {
 public:
  LayoutSVGResourcePaintServer(SVGElement*);
  ~LayoutSVGResourcePaintServer() override;

  virtual SVGPaintServer PreparePaintServer(
      const SVGResourceClient&,
      const FloatRect& object_bounding_box) = 0;

  // Helper utilities used in to access the underlying resources for DRT.
  static SVGPaintDescription RequestPaintDescription(const LayoutObject&,
                                                     const ComputedStyle&,
                                                     LayoutSVGResourceMode);
};

DEFINE_TYPE_CASTS(LayoutSVGResourcePaintServer,
                  LayoutSVGResourceContainer,
                  resource,
                  resource->IsSVGPaintServer(),
                  resource.IsSVGPaintServer());

}  // namespace blink

#endif
