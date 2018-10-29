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

#include "third_party/blink/renderer/core/svg/svg_component_transfer_function_element.h"

#include "third_party/blink/renderer/core/dom/attribute.h"
#include "third_party/blink/renderer/core/svg/svg_fe_component_transfer_element.h"
#include "third_party/blink/renderer/core/svg/svg_number_list.h"
#include "third_party/blink/renderer/core/svg_names.h"

namespace blink {

template <>
const SVGEnumerationStringEntries&
GetStaticStringEntries<ComponentTransferType>() {
  DEFINE_STATIC_LOCAL(SVGEnumerationStringEntries, entries, ());
  if (entries.IsEmpty()) {
    entries.push_back(
        std::make_pair(FECOMPONENTTRANSFER_TYPE_IDENTITY, "identity"));
    entries.push_back(std::make_pair(FECOMPONENTTRANSFER_TYPE_TABLE, "table"));
    entries.push_back(
        std::make_pair(FECOMPONENTTRANSFER_TYPE_DISCRETE, "discrete"));
    entries.push_back(
        std::make_pair(FECOMPONENTTRANSFER_TYPE_LINEAR, "linear"));
    entries.push_back(std::make_pair(FECOMPONENTTRANSFER_TYPE_GAMMA, "gamma"));
  }
  return entries;
}

SVGComponentTransferFunctionElement::SVGComponentTransferFunctionElement(
    const QualifiedName& tag_name,
    Document& document)
    : SVGElement(tag_name, document),
      table_values_(
          SVGAnimatedNumberList::Create(this, svg_names::kTableValuesAttr)),
      slope_(SVGAnimatedNumber::Create(this, svg_names::kSlopeAttr, 1)),
      intercept_(
          SVGAnimatedNumber::Create(this, svg_names::kInterceptAttr, 0.0f)),
      amplitude_(SVGAnimatedNumber::Create(this, svg_names::kAmplitudeAttr, 1)),
      exponent_(SVGAnimatedNumber::Create(this, svg_names::kExponentAttr, 1)),
      offset_(SVGAnimatedNumber::Create(this, svg_names::kOffsetAttr, 0.0f)),
      type_(SVGAnimatedEnumeration<ComponentTransferType>::Create(
          this,
          svg_names::kTypeAttr,
          FECOMPONENTTRANSFER_TYPE_IDENTITY)) {
  AddToPropertyMap(table_values_);
  AddToPropertyMap(slope_);
  AddToPropertyMap(intercept_);
  AddToPropertyMap(amplitude_);
  AddToPropertyMap(exponent_);
  AddToPropertyMap(offset_);
  AddToPropertyMap(type_);
}

void SVGComponentTransferFunctionElement::Trace(blink::Visitor* visitor) {
  visitor->Trace(table_values_);
  visitor->Trace(slope_);
  visitor->Trace(intercept_);
  visitor->Trace(amplitude_);
  visitor->Trace(exponent_);
  visitor->Trace(offset_);
  visitor->Trace(type_);
  SVGElement::Trace(visitor);
}

void SVGComponentTransferFunctionElement::SvgAttributeChanged(
    const QualifiedName& attr_name) {
  if (attr_name == svg_names::kTypeAttr ||
      attr_name == svg_names::kTableValuesAttr ||
      attr_name == svg_names::kSlopeAttr ||
      attr_name == svg_names::kInterceptAttr ||
      attr_name == svg_names::kAmplitudeAttr ||
      attr_name == svg_names::kExponentAttr ||
      attr_name == svg_names::kOffsetAttr) {
    SVGElement::InvalidationGuard invalidation_guard(this);
    InvalidateFilterPrimitiveParent(*this);
    return;
  }

  SVGElement::SvgAttributeChanged(attr_name);
}

ComponentTransferFunction
SVGComponentTransferFunctionElement::TransferFunction() const {
  ComponentTransferFunction func;
  func.type = type_->CurrentValue()->EnumValue();
  func.slope = slope_->CurrentValue()->Value();
  func.intercept = intercept_->CurrentValue()->Value();
  func.amplitude = amplitude_->CurrentValue()->Value();
  func.exponent = exponent_->CurrentValue()->Value();
  func.offset = offset_->CurrentValue()->Value();
  func.table_values = table_values_->CurrentValue()->ToFloatVector();
  return func;
}

}  // namespace blink
