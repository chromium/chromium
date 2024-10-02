/*
 * Copyright (C) 2004, 2005, 2007 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006 Rob Buis <buis@kde.org>
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

#include "third_party/blink/renderer/core/svg/svg_fe_composite_element.h"

#include "third_party/blink/renderer/core/svg/graphics/filters/svg_filter_builder.h"
#include "third_party/blink/renderer/core/svg/svg_animated_number.h"
#include "third_party/blink/renderer/core/svg/svg_animated_string.h"
#include "third_party/blink/renderer/core/svg/svg_enumeration_map.h"
#include "third_party/blink/renderer/core/svg_names.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

template <>
const SVGEnumerationMap& GetEnumerationMap<CompositeOperationType>() {
  static const SVGEnumerationMap::Entry enum_items[] = {
      {FECOMPOSITE_OPERATOR_OVER, "over"},
      {FECOMPOSITE_OPERATOR_IN, "in"},
      {FECOMPOSITE_OPERATOR_OUT, "out"},
      {FECOMPOSITE_OPERATOR_ATOP, "atop"},
      {FECOMPOSITE_OPERATOR_XOR, "xor"},
      {FECOMPOSITE_OPERATOR_ARITHMETIC, "arithmetic"},
      {FECOMPOSITE_OPERATOR_LIGHTER, "lighter"},
  };
  static const SVGEnumerationMap entries(enum_items,
                                         FECOMPOSITE_OPERATOR_ARITHMETIC);
  return entries;
}

SVGFECompositeElement::SVGFECompositeElement(Document& document)
    : SVGFilterPrimitiveStandardAttributes(svg_names::kFECompositeTag,
                                           document),
      k1_(MakeGarbageCollected<SVGAnimatedNumber>(this,
                                                  svg_names::kK1Attr,
                                                  0.0f)),
      k2_(MakeGarbageCollected<SVGAnimatedNumber>(this,
                                                  svg_names::kK2Attr,
                                                  0.0f)),
      k3_(MakeGarbageCollected<SVGAnimatedNumber>(this,
                                                  svg_names::kK3Attr,
                                                  0.0f)),
      k4_(MakeGarbageCollected<SVGAnimatedNumber>(this,
                                                  svg_names::kK4Attr,
                                                  0.0f)),
      in1_(MakeGarbageCollected<SVGAnimatedString>(this, svg_names::kInAttr)),
      in2_(MakeGarbageCollected<SVGAnimatedString>(this, svg_names::kIn2Attr)),
      svg_operator_(
          MakeGarbageCollected<SVGAnimatedEnumeration<CompositeOperationType>>(
              this,
              svg_names::kOperatorAttr,
              FECOMPOSITE_OPERATOR_OVER)) {}

void SVGFECompositeElement::Trace(Visitor* visitor) const {
  visitor->Trace(k1_);
  visitor->Trace(k2_);
  visitor->Trace(k3_);
  visitor->Trace(k4_);
  visitor->Trace(in1_);
  visitor->Trace(in2_);
  visitor->Trace(svg_operator_);
  SVGFilterPrimitiveStandardAttributes::Trace(visitor);
}

bool SVGFECompositeElement::SetFilterEffectAttribute(
    FilterEffect* effect,
    const QualifiedName& attr_name) {
  FEComposite* composite = static_cast<FEComposite*>(effect);
  if (attr_name == svg_names::kOperatorAttr)
    return composite->SetOperation(svg_operator_->CurrentEnumValue());
  if (attr_name == svg_names::kK1Attr)
    return composite->SetK1(k1_->CurrentValue()->Value());
  if (attr_name == svg_names::kK2Attr)
    return composite->SetK2(k2_->CurrentValue()->Value());
  if (attr_name == svg_names::kK3Attr)
    return composite->SetK3(k3_->CurrentValue()->Value());
  if (attr_name == svg_names::kK4Attr)
    return composite->SetK4(k4_->CurrentValue()->Value());

  return SVGFilterPrimitiveStandardAttributes::SetFilterEffectAttribute(
      effect, attr_name);
}

void SVGFECompositeElement::SvgAttributeChanged(
    const SvgAttributeChangedParams& params) {
  const QualifiedName& attr_name = params.name;
  if (attr_name == svg_names::kOperatorAttr ||
      attr_name == svg_names::kK1Attr || attr_name == svg_names::kK2Attr ||
      attr_name == svg_names::kK3Attr || attr_name == svg_names::kK4Attr) {
    PrimitiveAttributeChanged(attr_name);
    return;
  }

  if (attr_name == svg_names::kInAttr || attr_name == svg_names::kIn2Attr) {
    Invalidate();
    return;
  }

  SVGFilterPrimitiveStandardAttributes::SvgAttributeChanged(params);
}

FilterEffect* SVGFECompositeElement::Build(SVGFilterBuilder* filter_builder,
                                           Filter* filter) {
  FilterEffect* input1 = filter_builder->GetEffectById(
      AtomicString(in1_->CurrentValue()->Value()));
  FilterEffect* input2 = filter_builder->GetEffectById(
      AtomicString(in2_->CurrentValue()->Value()));
  DCHECK(input1);
  DCHECK(input2);

  auto* effect = MakeGarbageCollected<FEComposite>(
      filter, svg_operator_->CurrentEnumValue(), k1_->CurrentValue()->Value(),
      k2_->CurrentValue()->Value(), k3_->CurrentValue()->Value(),
      k4_->CurrentValue()->Value());
  FilterEffectVector& input_effects = effect->InputEffects();
  input_effects.reserve(2);
  input_effects.push_back(input1);
  input_effects.push_back(input2);
  return effect;
}

SVGAnimatedPropertyBase* SVGFECompositeElement::PropertyFromAttribute(
    const QualifiedName& attribute_name) const {
  if (attribute_name == svg_names::kK1Attr) {
    return k1_.Get();
  } else if (attribute_name == svg_names::kK2Attr) {
    return k2_.Get();
  } else if (attribute_name == svg_names::kK3Attr) {
    return k3_.Get();
  } else if (attribute_name == svg_names::kK4Attr) {
    return k4_.Get();
  } else if (attribute_name == svg_names::kInAttr) {
    return in1_.Get();
  } else if (attribute_name == svg_names::kIn2Attr) {
    return in2_.Get();
  } else if (attribute_name == svg_names::kOperatorAttr) {
    return svg_operator_.Get();
  } else {
    return SVGFilterPrimitiveStandardAttributes::PropertyFromAttribute(
        attribute_name);
  }
}

void SVGFECompositeElement::SynchronizeAllSVGAttributes() const {
  SVGAnimatedPropertyBase* attrs[]{k1_.Get(),          k2_.Get(),  k3_.Get(),
                                   k4_.Get(),          in1_.Get(), in2_.Get(),
                                   svg_operator_.Get()};
  SynchronizeListOfSVGAttributes(attrs);
  SVGFilterPrimitiveStandardAttributes::SynchronizeAllSVGAttributes();
}

}  // namespace blink
