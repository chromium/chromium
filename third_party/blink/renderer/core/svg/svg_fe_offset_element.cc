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

#include "third_party/blink/renderer/core/svg/svg_fe_offset_element.h"

#include "third_party/blink/renderer/core/svg/graphics/filters/svg_filter_builder.h"
#include "third_party/blink/renderer/core/svg_names.h"
#include "third_party/blink/renderer/platform/graphics/filters/fe_offset.h"
#include "third_party/blink/renderer/platform/heap/heap.h"

namespace blink {

SVGFEOffsetElement::SVGFEOffsetElement(Document& document)
    : SVGFilterPrimitiveStandardAttributes(svg_names::kFEOffsetTag, document),
      dx_(MakeGarbageCollected<SVGAnimatedNumber>(this,
                                                  svg_names::kDxAttr,
                                                  0.0f)),
      dy_(MakeGarbageCollected<SVGAnimatedNumber>(this,
                                                  svg_names::kDyAttr,
                                                  0.0f)),
      in1_(MakeGarbageCollected<SVGAnimatedString>(this, svg_names::kInAttr)) {
  AddToPropertyMap(dx_);
  AddToPropertyMap(dy_);
  AddToPropertyMap(in1_);
}

void SVGFEOffsetElement::Trace(blink::Visitor* visitor) {
  visitor->Trace(dx_);
  visitor->Trace(dy_);
  visitor->Trace(in1_);
  SVGFilterPrimitiveStandardAttributes::Trace(visitor);
}

void SVGFEOffsetElement::SvgAttributeChanged(const QualifiedName& attr_name) {
  if (attr_name == svg_names::kInAttr || attr_name == svg_names::kDxAttr ||
      attr_name == svg_names::kDyAttr) {
    SVGElement::InvalidationGuard invalidation_guard(this);
    Invalidate();
    return;
  }

  SVGFilterPrimitiveStandardAttributes::SvgAttributeChanged(attr_name);
}

FilterEffect* SVGFEOffsetElement::Build(SVGFilterBuilder* filter_builder,
                                        Filter* filter) {
  FilterEffect* input1 = filter_builder->GetEffectById(
      AtomicString(in1_->CurrentValue()->Value()));
  DCHECK(input1);

  auto* effect = MakeGarbageCollected<FEOffset>(
      filter, dx_->CurrentValue()->Value(), dy_->CurrentValue()->Value());
  effect->InputEffects().push_back(input1);
  return effect;
}

}  // namespace blink
