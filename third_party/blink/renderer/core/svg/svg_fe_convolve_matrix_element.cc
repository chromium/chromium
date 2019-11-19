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

#include "third_party/blink/renderer/core/svg/svg_fe_convolve_matrix_element.h"

#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/svg/graphics/filters/svg_filter_builder.h"
#include "third_party/blink/renderer/core/svg/svg_enumeration_map.h"
#include "third_party/blink/renderer/core/svg_names.h"
#include "third_party/blink/renderer/platform/geometry/int_point.h"
#include "third_party/blink/renderer/platform/geometry/int_size.h"
#include "third_party/blink/renderer/platform/heap/heap.h"

namespace blink {

template <>
const SVGEnumerationMap& GetEnumerationMap<EdgeModeType>() {
  static const SVGEnumerationMap::Entry enum_items[] = {
      {EDGEMODE_DUPLICATE, "duplicate"},
      {EDGEMODE_WRAP, "wrap"},
      {EDGEMODE_NONE, "none"},
  };
  static const SVGEnumerationMap entries(enum_items);
  return entries;
}

class SVGAnimatedOrder : public SVGAnimatedIntegerOptionalInteger {
 public:
  SVGAnimatedOrder(SVGElement* context_element)
      : SVGAnimatedIntegerOptionalInteger(context_element,
                                          svg_names::kOrderAttr,
                                          3) {}

  SVGParsingError AttributeChanged(const String&) override;

 protected:
  static SVGParsingError CheckValue(SVGParsingError parse_status, int value) {
    if (parse_status != SVGParseStatus::kNoError)
      return parse_status;
    if (value < 0)
      return SVGParseStatus::kNegativeValue;
    if (value == 0)
      return SVGParseStatus::kZeroValue;
    return SVGParseStatus::kNoError;
  }
};

SVGParsingError SVGAnimatedOrder::AttributeChanged(const String& value) {
  SVGParsingError parse_status =
      SVGAnimatedIntegerOptionalInteger::AttributeChanged(value);
  // Check for semantic errors.
  parse_status = CheckValue(parse_status, FirstInteger()->BaseValue()->Value());
  parse_status =
      CheckValue(parse_status, SecondInteger()->BaseValue()->Value());
  return parse_status;
}

SVGFEConvolveMatrixElement::SVGFEConvolveMatrixElement(Document& document)
    : SVGFilterPrimitiveStandardAttributes(svg_names::kFEConvolveMatrixTag,
                                           document),
      bias_(MakeGarbageCollected<SVGAnimatedNumber>(this,
                                                    svg_names::kBiasAttr,
                                                    0.0f)),
      divisor_(MakeGarbageCollected<SVGAnimatedNumber>(this,
                                                       svg_names::kDivisorAttr,
                                                       1)),
      in1_(MakeGarbageCollected<SVGAnimatedString>(this, svg_names::kInAttr)),
      edge_mode_(MakeGarbageCollected<SVGAnimatedEnumeration<EdgeModeType>>(
          this,
          svg_names::kEdgeModeAttr,
          EDGEMODE_DUPLICATE)),
      kernel_matrix_(MakeGarbageCollected<SVGAnimatedNumberList>(
          this,
          svg_names::kKernelMatrixAttr)),
      kernel_unit_length_(MakeGarbageCollected<SVGAnimatedNumberOptionalNumber>(
          this,
          svg_names::kKernelUnitLengthAttr,
          0.0f)),
      order_(MakeGarbageCollected<SVGAnimatedOrder>(this)),
      preserve_alpha_(MakeGarbageCollected<SVGAnimatedBoolean>(
          this,
          svg_names::kPreserveAlphaAttr)),
      target_x_(
          MakeGarbageCollected<SVGAnimatedInteger>(this,
                                                   svg_names::kTargetXAttr,
                                                   0)),
      target_y_(
          MakeGarbageCollected<SVGAnimatedInteger>(this,
                                                   svg_names::kTargetYAttr,
                                                   0)) {
  AddToPropertyMap(preserve_alpha_);
  AddToPropertyMap(divisor_);
  AddToPropertyMap(bias_);
  AddToPropertyMap(kernel_unit_length_);
  AddToPropertyMap(kernel_matrix_);
  AddToPropertyMap(in1_);
  AddToPropertyMap(edge_mode_);
  AddToPropertyMap(order_);
  AddToPropertyMap(target_x_);
  AddToPropertyMap(target_y_);
}

void SVGFEConvolveMatrixElement::Trace(blink::Visitor* visitor) {
  visitor->Trace(bias_);
  visitor->Trace(divisor_);
  visitor->Trace(in1_);
  visitor->Trace(edge_mode_);
  visitor->Trace(kernel_matrix_);
  visitor->Trace(kernel_unit_length_);
  visitor->Trace(order_);
  visitor->Trace(preserve_alpha_);
  visitor->Trace(target_x_);
  visitor->Trace(target_y_);
  SVGFilterPrimitiveStandardAttributes::Trace(visitor);
}

IntSize SVGFEConvolveMatrixElement::MatrixOrder() const {
  if (!order_->IsSpecified())
    return IntSize(3, 3);
  return IntSize(orderX()->CurrentValue()->Value(),
                 orderY()->CurrentValue()->Value());
}

IntPoint SVGFEConvolveMatrixElement::TargetPoint() const {
  IntSize order = MatrixOrder();
  IntPoint target(target_x_->CurrentValue()->Value(),
                  target_y_->CurrentValue()->Value());
  // The spec says the default value is: targetX = floor ( orderX / 2 ))
  if (!target_x_->IsSpecified())
    target.SetX(order.Width() / 2);
  // The spec says the default value is: targetY = floor ( orderY / 2 ))
  if (!target_y_->IsSpecified())
    target.SetY(order.Height() / 2);
  return target;
}

float SVGFEConvolveMatrixElement::ComputeDivisor() const {
  if (divisor_->IsSpecified())
    return divisor_->CurrentValue()->Value();
  float divisor_value = 0;
  SVGNumberList* kernel_matrix = kernel_matrix_->CurrentValue();
  size_t kernel_matrix_size = kernel_matrix->length();
  for (size_t i = 0; i < kernel_matrix_size; ++i)
    divisor_value += kernel_matrix->at(i)->Value();
  return divisor_value ? divisor_value : 1;
}

bool SVGFEConvolveMatrixElement::SetFilterEffectAttribute(
    FilterEffect* effect,
    const QualifiedName& attr_name) {
  FEConvolveMatrix* convolve_matrix = static_cast<FEConvolveMatrix*>(effect);
  if (attr_name == svg_names::kEdgeModeAttr)
    return convolve_matrix->SetEdgeMode(
        edge_mode_->CurrentValue()->EnumValue());
  if (attr_name == svg_names::kDivisorAttr)
    return convolve_matrix->SetDivisor(ComputeDivisor());
  if (attr_name == svg_names::kBiasAttr)
    return convolve_matrix->SetBias(bias_->CurrentValue()->Value());
  if (attr_name == svg_names::kTargetXAttr ||
      attr_name == svg_names::kTargetYAttr)
    return convolve_matrix->SetTargetOffset(TargetPoint());
  if (attr_name == svg_names::kPreserveAlphaAttr)
    return convolve_matrix->SetPreserveAlpha(
        preserve_alpha_->CurrentValue()->Value());
  return SVGFilterPrimitiveStandardAttributes::SetFilterEffectAttribute(
      effect, attr_name);
}

void SVGFEConvolveMatrixElement::SvgAttributeChanged(
    const QualifiedName& attr_name) {
  if (attr_name == svg_names::kEdgeModeAttr ||
      attr_name == svg_names::kDivisorAttr ||
      attr_name == svg_names::kBiasAttr ||
      attr_name == svg_names::kTargetXAttr ||
      attr_name == svg_names::kTargetYAttr ||
      attr_name == svg_names::kPreserveAlphaAttr) {
    SVGElement::InvalidationGuard invalidation_guard(this);
    PrimitiveAttributeChanged(attr_name);
    return;
  }

  if (attr_name == svg_names::kInAttr || attr_name == svg_names::kOrderAttr ||
      attr_name == svg_names::kKernelMatrixAttr) {
    SVGElement::InvalidationGuard invalidation_guard(this);
    Invalidate();
    return;
  }

  SVGFilterPrimitiveStandardAttributes::SvgAttributeChanged(attr_name);
}

FilterEffect* SVGFEConvolveMatrixElement::Build(
    SVGFilterBuilder* filter_builder,
    Filter* filter) {
  FilterEffect* input1 = filter_builder->GetEffectById(
      AtomicString(in1_->CurrentValue()->Value()));
  DCHECK(input1);

  auto* effect = MakeGarbageCollected<FEConvolveMatrix>(
      filter, MatrixOrder(), ComputeDivisor(), bias_->CurrentValue()->Value(),
      TargetPoint(), edge_mode_->CurrentValue()->EnumValue(),
      preserve_alpha_->CurrentValue()->Value(),
      kernel_matrix_->CurrentValue()->ToFloatVector());
  effect->InputEffects().push_back(input1);
  return effect;
}

}  // namespace blink
