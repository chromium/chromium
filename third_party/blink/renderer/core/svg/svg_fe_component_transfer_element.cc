/*
 * Copyright (C) 2004, 2005, 2007 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005 Rob Buis <buis@kde.org>
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

#include "third_party/blink/renderer/core/svg/svg_fe_component_transfer_element.h"

#include "third_party/blink/renderer/core/dom/element_traversal.h"
#include "third_party/blink/renderer/core/svg/graphics/filters/svg_filter_builder.h"
#include "third_party/blink/renderer/core/svg/svg_animated_string.h"
#include "third_party/blink/renderer/core/svg/svg_fe_func_a_element.h"
#include "third_party/blink/renderer/core/svg/svg_fe_func_b_element.h"
#include "third_party/blink/renderer/core/svg/svg_fe_func_g_element.h"
#include "third_party/blink/renderer/core/svg/svg_fe_func_r_element.h"
#include "third_party/blink/renderer/core/svg_names.h"
#include "third_party/blink/renderer/platform/graphics/filters/fe_component_transfer.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

SVGFEComponentTransferElement::SVGFEComponentTransferElement(Document& document)
    : SVGFilterPrimitiveStandardAttributes(svg_names::kFEComponentTransferTag,
                                           document),
      in1_(MakeGarbageCollected<SVGAnimatedString>(this, svg_names::kInAttr)) {}

void SVGFEComponentTransferElement::Trace(Visitor* visitor) const {
  visitor->Trace(in1_);
  SVGFilterPrimitiveStandardAttributes::Trace(visitor);
}

void SVGFEComponentTransferElement::SvgAttributeChanged(
    const SvgAttributeChangedParams& params) {
  if (params.name == svg_names::kInAttr) {
    Invalidate();
    return;
  }

  SVGFilterPrimitiveStandardAttributes::SvgAttributeChanged(params);
}

FilterEffect* SVGFEComponentTransferElement::Build(
    SVGFilterBuilder* filter_builder,
    Filter* filter) {
  FilterEffect* input1 = filter_builder->GetEffectById(
      AtomicString(in1_->CurrentValue()->Value()));
  DCHECK(input1);

  ComponentTransferFunction red;
  ComponentTransferFunction green;
  ComponentTransferFunction blue;
  ComponentTransferFunction alpha;

  for (SVGElement* element = Traversal<SVGElement>::FirstChild(*this); element;
       element = Traversal<SVGElement>::NextSibling(*element)) {
    if (auto* func_r = DynamicTo<SVGFEFuncRElement>(*element))
      red = func_r->TransferFunction();
    else if (auto* func_g = DynamicTo<SVGFEFuncGElement>(*element))
      green = func_g->TransferFunction();
    else if (auto* func_b = DynamicTo<SVGFEFuncBElement>(*element))
      blue = func_b->TransferFunction();
    else if (auto* func_a = DynamicTo<SVGFEFuncAElement>(*element))
      alpha = func_a->TransferFunction();
  }

  auto* effect = MakeGarbageCollected<FEComponentTransfer>(filter, red, green,
                                                           blue, alpha);
  effect->InputEffects().push_back(input1);
  return effect;
}

SVGAnimatedPropertyBase* SVGFEComponentTransferElement::PropertyFromAttribute(
    const QualifiedName& attribute_name) const {
  if (attribute_name == svg_names::kInAttr) {
    return in1_.Get();
  } else {
    return SVGFilterPrimitiveStandardAttributes::PropertyFromAttribute(
        attribute_name);
  }
}

void SVGFEComponentTransferElement::SynchronizeAllSVGAttributes() const {
  SVGAnimatedPropertyBase* attrs[]{in1_.Get()};
  SynchronizeListOfSVGAttributes(attrs);
  SVGFilterPrimitiveStandardAttributes::SynchronizeAllSVGAttributes();
}

}  // namespace blink
