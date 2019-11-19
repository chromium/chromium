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

#include "third_party/blink/renderer/core/svg/svg_fe_color_matrix_element.h"

#include "third_party/blink/renderer/core/svg/graphics/filters/svg_filter_builder.h"
#include "third_party/blink/renderer/core/svg/svg_enumeration_map.h"
#include "third_party/blink/renderer/core/svg_names.h"
#include "third_party/blink/renderer/platform/heap/heap.h"

namespace blink {

template <>
const SVGEnumerationMap& GetEnumerationMap<ColorMatrixType>() {
  static const SVGEnumerationMap::Entry enum_items[] = {
      {FECOLORMATRIX_TYPE_MATRIX, "matrix"},
      {FECOLORMATRIX_TYPE_SATURATE, "saturate"},
      {FECOLORMATRIX_TYPE_HUEROTATE, "hueRotate"},
      {FECOLORMATRIX_TYPE_LUMINANCETOALPHA, "luminanceToAlpha"},
  };
  static const SVGEnumerationMap entries(enum_items);
  return entries;
}

SVGFEColorMatrixElement::SVGFEColorMatrixElement(Document& document)
    : SVGFilterPrimitiveStandardAttributes(svg_names::kFEColorMatrixTag,
                                           document),
      values_(
          MakeGarbageCollected<SVGAnimatedNumberList>(this,
                                                      svg_names::kValuesAttr)),
      in1_(MakeGarbageCollected<SVGAnimatedString>(this, svg_names::kInAttr)),
      type_(MakeGarbageCollected<SVGAnimatedEnumeration<ColorMatrixType>>(
          this,
          svg_names::kTypeAttr,
          FECOLORMATRIX_TYPE_MATRIX)) {
  AddToPropertyMap(values_);
  AddToPropertyMap(in1_);
  AddToPropertyMap(type_);
}

void SVGFEColorMatrixElement::Trace(blink::Visitor* visitor) {
  visitor->Trace(values_);
  visitor->Trace(in1_);
  visitor->Trace(type_);
  SVGFilterPrimitiveStandardAttributes::Trace(visitor);
}

bool SVGFEColorMatrixElement::SetFilterEffectAttribute(
    FilterEffect* effect,
    const QualifiedName& attr_name) {
  FEColorMatrix* color_matrix = static_cast<FEColorMatrix*>(effect);
  if (attr_name == svg_names::kTypeAttr)
    return color_matrix->SetType(type_->CurrentValue()->EnumValue());
  if (attr_name == svg_names::kValuesAttr)
    return color_matrix->SetValues(values_->CurrentValue()->ToFloatVector());

  return SVGFilterPrimitiveStandardAttributes::SetFilterEffectAttribute(
      effect, attr_name);
}

void SVGFEColorMatrixElement::SvgAttributeChanged(
    const QualifiedName& attr_name) {
  if (attr_name == svg_names::kTypeAttr ||
      attr_name == svg_names::kValuesAttr) {
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

FilterEffect* SVGFEColorMatrixElement::Build(SVGFilterBuilder* filter_builder,
                                             Filter* filter) {
  FilterEffect* input1 = filter_builder->GetEffectById(
      AtomicString(in1_->CurrentValue()->Value()));
  DCHECK(input1);

  ColorMatrixType filter_type = type_->CurrentValue()->EnumValue();
  Vector<float> filter_values = values_->CurrentValue()->ToFloatVector();
  auto* effect =
      MakeGarbageCollected<FEColorMatrix>(filter, filter_type, filter_values);
  effect->InputEffects().push_back(input1);
  return effect;
}

}  // namespace blink
