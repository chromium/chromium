/*
 * Copyright (C) 2004, 2005, 2006, 2007 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005 Rob Buis <buis@kde.org>
 * Copyright (C) 2005 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2010 Dirk Schulze <krit@webkit.org>
 * Copyright (C) 2013 Google Inc. All rights reserved.
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

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_SVG_GRAPHICS_FILTERS_SVG_FE_IMAGE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_SVG_GRAPHICS_FILTERS_SVG_FE_IMAGE_H_

#include "third_party/blink/renderer/core/dom/tree_scope.h"
#include "third_party/blink/renderer/core/svg/svg_preserve_aspect_ratio.h"
#include "third_party/blink/renderer/platform/graphics/filters/filter_effect.h"

namespace blink {

class Image;
class LayoutObject;

class FEImage final : public FilterEffect {
 public:
  FEImage(Filter*, scoped_refptr<Image>, SVGPreserveAspectRatio*);
  FEImage(Filter*, TreeScope&, const String&, SVGPreserveAspectRatio*);

  // feImage does not perform color interpolation of any kind, so doesn't
  // depend on the value of color-interpolation-filters.
  void SetOperatingInterpolationSpace(InterpolationSpace) override {}

  WTF::TextStream& ExternalRepresentation(WTF::TextStream&,
                                          int indention) const override;

  void Trace(blink::Visitor*) override;

 private:
  ~FEImage() override = default;
  LayoutObject* ReferencedLayoutObject() const;

  FilterEffectType GetFilterEffectType() const override {
    return kFilterEffectTypeImage;
  }

  FloatRect MapInputs(const FloatRect&) const override;

  sk_sp<PaintFilter> CreateImageFilter() override;
  sk_sp<PaintFilter> CreateImageFilterForLayoutObject(const LayoutObject&);

  scoped_refptr<Image> image_;

  // m_treeScope will never be a dangling reference. See
  // https://bugs.webkit.org/show_bug.cgi?id=99243
  Member<TreeScope> tree_scope_;
  String href_;
  Member<SVGPreserveAspectRatio> preserve_aspect_ratio_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_SVG_GRAPHICS_FILTERS_SVG_FE_IMAGE_H_
