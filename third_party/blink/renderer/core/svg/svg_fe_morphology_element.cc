/*
 * Copyright (C) 2009 Dirk Schulze <krit@webkit.org>
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

#include "third_party/blink/renderer/core/svg/svg_fe_morphology_element.h"

#include "third_party/blink/renderer/core/svg/graphics/filters/svg_filter_builder.h"
#include "third_party/blink/renderer/core/svg_names.h"

namespace blink {

template <>
const SVGEnumerationStringEntries&
GetStaticStringEntries<MorphologyOperatorType>() {
  DEFINE_STATIC_LOCAL(SVGEnumerationStringEntries, entries, ());
  if (entries.IsEmpty()) {
    entries.push_back(std::make_pair(FEMORPHOLOGY_OPERATOR_ERODE, "erode"));
    entries.push_back(std::make_pair(FEMORPHOLOGY_OPERATOR_DILATE, "dilate"));
  }
  return entries;
}

inline SVGFEMorphologyElement::SVGFEMorphologyElement(Document& document)
    : SVGFilterPrimitiveStandardAttributes(svg_names::kFEMorphologyTag,
                                           document),
      radius_(SVGAnimatedNumberOptionalNumber::Create(this,
                                                      svg_names::kRadiusAttr,
                                                      0.0f)),
      in1_(SVGAnimatedString::Create(this, svg_names::kInAttr)),
      svg_operator_(SVGAnimatedEnumeration<MorphologyOperatorType>::Create(
          this,
          svg_names::kOperatorAttr,
          FEMORPHOLOGY_OPERATOR_ERODE)) {
  AddToPropertyMap(radius_);
  AddToPropertyMap(in1_);
  AddToPropertyMap(svg_operator_);
}

void SVGFEMorphologyElement::Trace(blink::Visitor* visitor) {
  visitor->Trace(radius_);
  visitor->Trace(in1_);
  visitor->Trace(svg_operator_);
  SVGFilterPrimitiveStandardAttributes::Trace(visitor);
}

DEFINE_NODE_FACTORY(SVGFEMorphologyElement)

bool SVGFEMorphologyElement::SetFilterEffectAttribute(
    FilterEffect* effect,
    const QualifiedName& attr_name) {
  FEMorphology* morphology = static_cast<FEMorphology*>(effect);
  if (attr_name == svg_names::kOperatorAttr)
    return morphology->SetMorphologyOperator(
        svg_operator_->CurrentValue()->EnumValue());
  if (attr_name == svg_names::kRadiusAttr) {
    // Both setRadius functions should be evaluated separately.
    bool is_radius_x_changed =
        morphology->SetRadiusX(radiusX()->CurrentValue()->Value());
    bool is_radius_y_changed =
        morphology->SetRadiusY(radiusY()->CurrentValue()->Value());
    return is_radius_x_changed || is_radius_y_changed;
  }
  return SVGFilterPrimitiveStandardAttributes::SetFilterEffectAttribute(
      effect, attr_name);
}

void SVGFEMorphologyElement::SvgAttributeChanged(
    const QualifiedName& attr_name) {
  if (attr_name == svg_names::kOperatorAttr ||
      attr_name == svg_names::kRadiusAttr) {
    SVGElement::InvalidationGuard invalidation_guard(this);
    PrimitiveAttributeChanged(attr_name);
    return;
  }

  if (attr_name == svg_names::kInAttr) {
    SVGElement::InvalidationGuard invalidation_guard(this);
    Invalidate();
    return;
  }

  SVGFilterPrimitiveStandardAttributes::SvgAttributeChanged(attr_name);
}

FilterEffect* SVGFEMorphologyElement::Build(SVGFilterBuilder* filter_builder,
                                            Filter* filter) {
  FilterEffect* input1 = filter_builder->GetEffectById(
      AtomicString(in1_->CurrentValue()->Value()));

  if (!input1)
    return nullptr;

  // "A negative or zero value disables the effect of the given filter
  // primitive (i.e., the result is the filter input image)."
  // https://drafts.fxtf.org/filter-effects/#element-attrdef-femorphology-radius
  //
  // (This is handled by FEMorphology)
  float x_radius = radiusX()->CurrentValue()->Value();
  float y_radius = radiusY()->CurrentValue()->Value();
  FilterEffect* effect = FEMorphology::Create(
      filter, svg_operator_->CurrentValue()->EnumValue(), x_radius, y_radius);
  effect->InputEffects().push_back(input1);
  return effect;
}

}  // namespace blink
