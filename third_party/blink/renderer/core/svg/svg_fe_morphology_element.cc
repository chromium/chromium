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
#include "third_party/blink/renderer/core/svg/svg_animated_number_optional_number.h"
#include "third_party/blink/renderer/core/svg/svg_animated_string.h"
#include "third_party/blink/renderer/core/svg/svg_enumeration_map.h"
#include "third_party/blink/renderer/core/svg_names.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

template <>
const SVGEnumerationMap& GetEnumerationMap<MorphologyOperatorType>() {
  static const SVGEnumerationMap::Entry enum_items[] = {
      {FEMORPHOLOGY_OPERATOR_ERODE, "erode"},
      {FEMORPHOLOGY_OPERATOR_DILATE, "dilate"},
  };
  static const SVGEnumerationMap entries(enum_items);
  return entries;
}

SVGFEMorphologyElement::SVGFEMorphologyElement(Document& document)
    : SVGFilterPrimitiveStandardAttributes(svg_names::kFEMorphologyTag,
                                           document),
      radius_(MakeGarbageCollected<SVGAnimatedNumberOptionalNumber>(
          this,
          svg_names::kRadiusAttr,
          0.0f)),
      in1_(MakeGarbageCollected<SVGAnimatedString>(this, svg_names::kInAttr)),
      svg_operator_(
          MakeGarbageCollected<SVGAnimatedEnumeration<MorphologyOperatorType>>(
              this,
              svg_names::kOperatorAttr,
              FEMORPHOLOGY_OPERATOR_ERODE)) {}

SVGAnimatedNumber* SVGFEMorphologyElement::radiusX() {
  return radius_->FirstNumber();
}

SVGAnimatedNumber* SVGFEMorphologyElement::radiusY() {
  return radius_->SecondNumber();
}

void SVGFEMorphologyElement::Trace(Visitor* visitor) const {
  visitor->Trace(radius_);
  visitor->Trace(in1_);
  visitor->Trace(svg_operator_);
  SVGFilterPrimitiveStandardAttributes::Trace(visitor);
}

bool SVGFEMorphologyElement::SetFilterEffectAttribute(
    FilterEffect* effect,
    const QualifiedName& attr_name) {
  FEMorphology* morphology = static_cast<FEMorphology*>(effect);
  if (attr_name == svg_names::kOperatorAttr)
    return morphology->SetMorphologyOperator(svg_operator_->CurrentEnumValue());
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
    const SvgAttributeChangedParams& params) {
  const QualifiedName& attr_name = params.name;
  if (attr_name == svg_names::kOperatorAttr ||
      attr_name == svg_names::kRadiusAttr) {
    PrimitiveAttributeChanged(attr_name);
    return;
  }

  if (attr_name == svg_names::kInAttr) {
    Invalidate();
    return;
  }

  SVGFilterPrimitiveStandardAttributes::SvgAttributeChanged(params);
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
  auto* effect = MakeGarbageCollected<FEMorphology>(
      filter, svg_operator_->CurrentEnumValue(), x_radius, y_radius);
  effect->InputEffects().push_back(input1);
  return effect;
}

SVGAnimatedPropertyBase* SVGFEMorphologyElement::PropertyFromAttribute(
    const QualifiedName& attribute_name) const {
  if (attribute_name == svg_names::kRadiusAttr) {
    return radius_.Get();
  } else if (attribute_name == svg_names::kInAttr) {
    return in1_.Get();
  } else if (attribute_name == svg_names::kOperatorAttr) {
    return svg_operator_.Get();
  } else {
    return SVGFilterPrimitiveStandardAttributes::PropertyFromAttribute(
        attribute_name);
  }
}

void SVGFEMorphologyElement::SynchronizeAllSVGAttributes() const {
  SVGAnimatedPropertyBase* attrs[]{radius_.Get(), in1_.Get(),
                                   svg_operator_.Get()};
  SynchronizeListOfSVGAttributes(attrs);
  SVGFilterPrimitiveStandardAttributes::SynchronizeAllSVGAttributes();
}

}  // namespace blink
