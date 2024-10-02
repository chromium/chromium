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

#include "third_party/blink/renderer/core/svg/svg_animated_number.h"
#include "third_party/blink/renderer/core/svg/svg_animated_number_list.h"
#include "third_party/blink/renderer/core/svg/svg_enumeration_map.h"
#include "third_party/blink/renderer/core/svg/svg_fe_component_transfer_element.h"
#include "third_party/blink/renderer/core/svg/svg_number_list.h"
#include "third_party/blink/renderer/core/svg_names.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

template <>
const SVGEnumerationMap& GetEnumerationMap<ComponentTransferType>() {
  static const SVGEnumerationMap::Entry enum_items[] = {
      {FECOMPONENTTRANSFER_TYPE_IDENTITY, "identity"},
      {FECOMPONENTTRANSFER_TYPE_TABLE, "table"},
      {FECOMPONENTTRANSFER_TYPE_DISCRETE, "discrete"},
      {FECOMPONENTTRANSFER_TYPE_LINEAR, "linear"},
      {FECOMPONENTTRANSFER_TYPE_GAMMA, "gamma"},
  };
  static const SVGEnumerationMap entries(enum_items);
  return entries;
}

SVGComponentTransferFunctionElement::SVGComponentTransferFunctionElement(
    const QualifiedName& tag_name,
    Document& document)
    : SVGElement(tag_name, document),
      table_values_(MakeGarbageCollected<SVGAnimatedNumberList>(
          this,
          svg_names::kTableValuesAttr)),
      slope_(MakeGarbageCollected<SVGAnimatedNumber>(this,
                                                     svg_names::kSlopeAttr,
                                                     1)),
      intercept_(
          MakeGarbageCollected<SVGAnimatedNumber>(this,
                                                  svg_names::kInterceptAttr,
                                                  0.0f)),
      amplitude_(
          MakeGarbageCollected<SVGAnimatedNumber>(this,
                                                  svg_names::kAmplitudeAttr,
                                                  1)),
      exponent_(
          MakeGarbageCollected<SVGAnimatedNumber>(this,
                                                  svg_names::kExponentAttr,
                                                  1)),
      offset_(MakeGarbageCollected<SVGAnimatedNumber>(this,
                                                      svg_names::kOffsetAttr,
                                                      0.0f)),
      type_(MakeGarbageCollected<SVGAnimatedEnumeration<ComponentTransferType>>(
          this,
          svg_names::kTypeAttr,
          FECOMPONENTTRANSFER_TYPE_IDENTITY)) {}

void SVGComponentTransferFunctionElement::Trace(Visitor* visitor) const {
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
    const SvgAttributeChangedParams& params) {
  const QualifiedName& attr_name = params.name;
  if (attr_name == svg_names::kTypeAttr ||
      attr_name == svg_names::kTableValuesAttr ||
      attr_name == svg_names::kSlopeAttr ||
      attr_name == svg_names::kInterceptAttr ||
      attr_name == svg_names::kAmplitudeAttr ||
      attr_name == svg_names::kExponentAttr ||
      attr_name == svg_names::kOffsetAttr) {
    InvalidateFilterPrimitiveParent(*this);
    return;
  }

  SVGElement::SvgAttributeChanged(params);
}

ComponentTransferFunction
SVGComponentTransferFunctionElement::TransferFunction() const {
  ComponentTransferFunction func;
  func.type = type_->CurrentEnumValue();
  func.slope = slope_->CurrentValue()->Value();
  func.intercept = intercept_->CurrentValue()->Value();
  func.amplitude = amplitude_->CurrentValue()->Value();
  func.exponent = exponent_->CurrentValue()->Value();
  func.offset = offset_->CurrentValue()->Value();
  func.table_values = table_values_->CurrentValue()->ToFloatVector();
  return func;
}

SVGAnimatedPropertyBase*
SVGComponentTransferFunctionElement::PropertyFromAttribute(
    const QualifiedName& attribute_name) const {
  if (attribute_name == svg_names::kTableValuesAttr) {
    return table_values_.Get();
  } else if (attribute_name == svg_names::kSlopeAttr) {
    return slope_.Get();
  } else if (attribute_name == svg_names::kInterceptAttr) {
    return intercept_.Get();
  } else if (attribute_name == svg_names::kAmplitudeAttr) {
    return amplitude_.Get();
  } else if (attribute_name == svg_names::kExponentAttr) {
    return exponent_.Get();
  } else if (attribute_name == svg_names::kOffsetAttr) {
    return offset_.Get();
  } else if (attribute_name == svg_names::kTypeAttr) {
    return type_.Get();
  } else {
    return SVGElement::PropertyFromAttribute(attribute_name);
  }
}

void SVGComponentTransferFunctionElement::SynchronizeAllSVGAttributes() const {
  SVGAnimatedPropertyBase* attrs[]{
      table_values_.Get(), slope_.Get(),  intercept_.Get(), amplitude_.Get(),
      exponent_.Get(),     offset_.Get(), type_.Get()};
  SynchronizeListOfSVGAttributes(attrs);
  SVGElement::SynchronizeAllSVGAttributes();
}

}  // namespace blink
