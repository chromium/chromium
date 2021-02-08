/*
 * Copyright (C) Research In Motion Limited 2010. All rights reserved.
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

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_SVG_SVG_RESOURCES_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_SVG_SVG_RESOURCES_H_

#include "third_party/blink/renderer/core/svg/svg_resource_client.h"
#include "third_party/blink/renderer/platform/geometry/float_rect.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

class CompositorFilterOperations;
class ComputedStyle;
class FilterEffectBuilder;
class LayoutObject;
class ReferenceFilterOperation;
class SVGElement;
class SVGElementResourceClient;

// Holds a set of resources associated with a LayoutObject
class SVGResources {
  STATIC_ONLY(SVGResources);

 public:
  static SVGElementResourceClient* GetClient(const LayoutObject&);
  static FloatRect ReferenceBoxForEffects(const LayoutObject&);

  static void UpdateClipPathFilterMask(SVGElement&,
                                       const ComputedStyle* old_style,
                                       const ComputedStyle&);
  static void ClearClipPathFilterMask(SVGElement&, const ComputedStyle*);
  static void UpdatePaints(SVGElement&,
                           const ComputedStyle* old_style,
                           const ComputedStyle&);
  static void ClearPaints(SVGElement&, const ComputedStyle*);
  static void UpdateMarkers(SVGElement&,
                            const ComputedStyle* old_style,
                            const ComputedStyle&);
  static void ClearMarkers(SVGElement&, const ComputedStyle*);
};

class SVGElementResourceClient final
    : public GarbageCollected<SVGElementResourceClient>,
      public SVGResourceClient {
 public:
  explicit SVGElementResourceClient(SVGElement*);

  void ResourceContentChanged(SVGResource*) override;

  void FilterPrimitiveChanged(SVGResource*,
                              SVGFilterPrimitiveStandardAttributes& primitive,
                              const QualifiedName& attribute) override;

  void UpdateFilterData(CompositorFilterOperations&);
  void InvalidateFilterData();
  void MarkFilterDataDirty();

  void Trace(Visitor*) const override;

 private:
  class FilterData;

  static FilterData* CreateFilterDataWithNodeMap(
      FilterEffectBuilder&,
      const ReferenceFilterOperation&);

  Member<SVGElement> element_;
  Member<FilterData> filter_data_;
  bool filter_data_dirty_;
};

// Helper class for handling invalidation of resources (generally after the
// reference box of a LayoutObject may have changed).
class SVGResourceInvalidator {
  STACK_ALLOCATED();

 public:
  explicit SVGResourceInvalidator(LayoutObject& object);

  // Invalidate any associated clip-path/mask/filter.
  void InvalidateEffects();

  // Invalidate any associated paints (fill/stroke).
  void InvalidatePaints();

 private:
  LayoutObject& object_;
};

}  // namespace blink

#endif
