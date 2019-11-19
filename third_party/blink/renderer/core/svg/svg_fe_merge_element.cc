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

#include "third_party/blink/renderer/core/svg/svg_fe_merge_element.h"

#include "third_party/blink/renderer/core/dom/element_traversal.h"
#include "third_party/blink/renderer/core/svg/graphics/filters/svg_filter_builder.h"
#include "third_party/blink/renderer/core/svg/svg_fe_merge_node_element.h"
#include "third_party/blink/renderer/core/svg_names.h"
#include "third_party/blink/renderer/platform/graphics/filters/fe_merge.h"

namespace blink {

SVGFEMergeElement::SVGFEMergeElement(Document& document)
    : SVGFilterPrimitiveStandardAttributes(svg_names::kFEMergeTag, document) {}

FilterEffect* SVGFEMergeElement::Build(SVGFilterBuilder* filter_builder,
                                       Filter* filter) {
  FilterEffect* effect = MakeGarbageCollected<FEMerge>(filter);
  FilterEffectVector& merge_inputs = effect->InputEffects();
  for (SVGFEMergeNodeElement& merge_node :
       Traversal<SVGFEMergeNodeElement>::ChildrenOf(*this)) {
    FilterEffect* merge_effect = filter_builder->GetEffectById(
        AtomicString(merge_node.in1()->CurrentValue()->Value()));
    DCHECK(merge_effect);
    merge_inputs.push_back(merge_effect);
  }
  return effect;
}

}  // namespace blink
