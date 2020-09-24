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

#include "third_party/blink/renderer/core/layout/svg/layout_svg_filter_primitive.h"

#include "third_party/blink/renderer/core/svg/svg_filter_primitive_standard_attributes.h"

namespace blink {

LayoutSVGFilterPrimitive::LayoutSVGFilterPrimitive(
    SVGFilterPrimitiveStandardAttributes* filter_primitive_element)
    : LayoutObject(filter_primitive_element) {}

static bool CurrentColorChanged(StyleDifference diff, const StyleColor& color) {
  return diff.TextDecorationOrColorChanged() && color.IsCurrentColor();
}

static void CheckForColorChange(SVGFilterPrimitiveStandardAttributes& element,
                                const QualifiedName& attr_name,
                                StyleDifference diff,
                                const StyleColor& old_color,
                                const StyleColor& new_color) {
  // If the <color> change from/to 'currentcolor' then invalidate the filter
  // chain so that it is rebuilt. (Makes sure the 'tainted' flag is
  // propagated.)
  if (new_color.IsCurrentColor() != old_color.IsCurrentColor()) {
    element.Invalidate();
    return;
  }
  if (new_color != old_color || CurrentColorChanged(diff, new_color))
    element.PrimitiveAttributeChanged(attr_name);
}

void LayoutSVGFilterPrimitive::StyleDidChange(StyleDifference diff,
                                              const ComputedStyle* old_style) {
  if (!old_style)
    return;
  auto& element = To<SVGFilterPrimitiveStandardAttributes>(*GetNode());
  const SVGComputedStyle& new_style = StyleRef().SvgStyle();
  if (IsA<SVGFEFloodElement>(element) || IsA<SVGFEDropShadowElement>(element)) {
    CheckForColorChange(element, svg_names::kFloodColorAttr, diff,
                        old_style->SvgStyle().FloodColor(),
                        new_style.FloodColor());
    if (new_style.FloodOpacity() != old_style->SvgStyle().FloodOpacity())
      element.PrimitiveAttributeChanged(svg_names::kFloodOpacityAttr);
  } else if (IsA<SVGFEDiffuseLightingElement>(element) ||
             IsA<SVGFESpecularLightingElement>(element)) {
    CheckForColorChange(element, svg_names::kLightingColorAttr, diff,
                        old_style->SvgStyle().LightingColor(),
                        new_style.LightingColor());
  }
  if (new_style.ColorInterpolationFilters() !=
      old_style->SvgStyle().ColorInterpolationFilters()) {
    element.PrimitiveAttributeChanged(
        svg_names::kColorInterpolationFiltersAttr);
  }
}

void LayoutSVGFilterPrimitive::UpdateLayout() {
  ClearNeedsLayout();
}

}  // namespace blink
