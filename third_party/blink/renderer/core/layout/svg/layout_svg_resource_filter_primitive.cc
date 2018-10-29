/*
 * Copyright (C) 2010 University of Szeged
 * Copyright (C) 2010 Zoltan Herczeg
 * Copyright (C) 2011 Renata Hodovan (reni@webkit.org)
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY UNIVERSITY OF SZEGED ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL UNIVERSITY OF SZEGED OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/layout/svg/layout_svg_resource_filter_primitive.h"

#include "third_party/blink/renderer/core/svg/svg_filter_primitive_standard_attributes.h"

namespace blink {

static bool CurrentColorChanged(StyleDifference diff, const StyleColor& color) {
  return diff.TextDecorationOrColorChanged() && color.IsCurrentColor();
}

void LayoutSVGResourceFilterPrimitive::StyleDidChange(
    StyleDifference diff,
    const ComputedStyle* old_style) {
  LayoutSVGHiddenContainer::StyleDidChange(diff, old_style);

  if (!old_style)
    return;
  DCHECK(GetElement());
  SVGFilterPrimitiveStandardAttributes& element =
      ToSVGFilterPrimitiveStandardAttributes(*GetElement());
  const SVGComputedStyle& new_style = StyleRef().SvgStyle();
  if (IsSVGFEFloodElement(element) || IsSVGFEDropShadowElement(element)) {
    if (new_style.FloodColor() != old_style->SvgStyle().FloodColor() ||
        CurrentColorChanged(diff, new_style.FloodColor()))
      element.PrimitiveAttributeChanged(svg_names::kFloodColorAttr);
    if (new_style.FloodOpacity() != old_style->SvgStyle().FloodOpacity())
      element.PrimitiveAttributeChanged(svg_names::kFloodOpacityAttr);
  } else if (IsSVGFEDiffuseLightingElement(element) ||
             IsSVGFESpecularLightingElement(element)) {
    if (new_style.LightingColor() != old_style->SvgStyle().LightingColor() ||
        CurrentColorChanged(diff, new_style.LightingColor()))
      element.PrimitiveAttributeChanged(svg_names::kLightingColorAttr);
  }
  if (new_style.ColorInterpolationFilters() !=
      old_style->SvgStyle().ColorInterpolationFilters()) {
    element.PrimitiveAttributeChanged(
        svg_names::kColorInterpolationFiltersAttr);
  }
}

}  // namespace blink
