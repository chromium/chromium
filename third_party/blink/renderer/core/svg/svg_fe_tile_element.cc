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

#include "third_party/blink/renderer/core/svg/svg_fe_tile_element.h"

#include "third_party/blink/renderer/core/svg/graphics/filters/svg_filter_builder.h"
#include "third_party/blink/renderer/core/svg_names.h"
#include "third_party/blink/renderer/platform/graphics/filters/fe_tile.h"
#include "third_party/blink/renderer/platform/heap/heap.h"

namespace blink {

SVGFETileElement::SVGFETileElement(Document& document)
    : SVGFilterPrimitiveStandardAttributes(svg_names::kFETileTag, document),
      in1_(MakeGarbageCollected<SVGAnimatedString>(this, svg_names::kInAttr)) {
  AddToPropertyMap(in1_);
}

void SVGFETileElement::Trace(blink::Visitor* visitor) {
  visitor->Trace(in1_);
  SVGFilterPrimitiveStandardAttributes::Trace(visitor);
}

void SVGFETileElement::SvgAttributeChanged(const QualifiedName& attr_name) {
  if (attr_name == svg_names::kInAttr) {
    SVGElement::InvalidationGuard invalidation_guard(this);
    Invalidate();
    return;
  }

  SVGFilterPrimitiveStandardAttributes::SvgAttributeChanged(attr_name);
}

FilterEffect* SVGFETileElement::Build(SVGFilterBuilder* filter_builder,
                                      Filter* filter) {
  FilterEffect* input1 = filter_builder->GetEffectById(
      AtomicString(in1_->CurrentValue()->Value()));
  DCHECK(input1);

  auto* effect = MakeGarbageCollected<FETile>(filter);
  effect->InputEffects().push_back(input1);
  return effect;
}

}  // namespace blink
