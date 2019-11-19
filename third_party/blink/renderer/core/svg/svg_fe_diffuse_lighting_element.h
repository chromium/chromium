/*
 * Copyright (C) 2004, 2005, 2007 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005 Rob Buis <buis@kde.org>
 * Copyright (C) 2005 Oliver Hunt <oliver@nerget.com>
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

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_SVG_SVG_FE_DIFFUSE_LIGHTING_ELEMENT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_SVG_SVG_FE_DIFFUSE_LIGHTING_ELEMENT_H_

#include "third_party/blink/renderer/core/svg/svg_animated_number_optional_number.h"
#include "third_party/blink/renderer/core/svg/svg_fe_light_element.h"
#include "third_party/blink/renderer/core/svg/svg_filter_primitive_standard_attributes.h"
#include "third_party/blink/renderer/platform/heap/handle.h"

namespace blink {

class SVGFEDiffuseLightingElement final
    : public SVGFilterPrimitiveStandardAttributes {
  DEFINE_WRAPPERTYPEINFO();

 public:
  explicit SVGFEDiffuseLightingElement(Document&);

  void LightElementAttributeChanged(const SVGFELightElement*,
                                    const QualifiedName&);

  SVGAnimatedNumber* diffuseConstant() { return diffuse_constant_.Get(); }
  SVGAnimatedNumber* surfaceScale() { return surface_scale_.Get(); }
  SVGAnimatedNumber* kernelUnitLengthX() {
    return kernel_unit_length_->FirstNumber();
  }
  SVGAnimatedNumber* kernelUnitLengthY() {
    return kernel_unit_length_->SecondNumber();
  }
  SVGAnimatedString* in1() { return in1_.Get(); }

  void Trace(blink::Visitor*) override;

 private:
  bool SetFilterEffectAttribute(FilterEffect*, const QualifiedName&) override;
  void SvgAttributeChanged(const QualifiedName&) override;
  FilterEffect* Build(SVGFilterBuilder*, Filter*) override;
  bool TaintsOrigin() const override;

  Member<SVGAnimatedNumber> diffuse_constant_;
  Member<SVGAnimatedNumber> surface_scale_;
  Member<SVGAnimatedNumberOptionalNumber> kernel_unit_length_;
  Member<SVGAnimatedString> in1_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_SVG_SVG_FE_DIFFUSE_LIGHTING_ELEMENT_H_
