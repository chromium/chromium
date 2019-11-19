/*
 * Copyright (C) 2004, 2005, 2006, 2007 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005 Rob Buis <buis@kde.org>
 * Copyright (C) 2005 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2009 Dirk Schulze <krit@webkit.org>
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

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_SVG_LAYOUT_SVG_RESOURCE_FILTER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_SVG_LAYOUT_SVG_RESOURCE_FILTER_H_

#include "third_party/blink/renderer/core/layout/svg/layout_svg_resource_container.h"
#include "third_party/blink/renderer/core/svg/svg_unit_types.h"

namespace blink {

class FilterEffect;
class SVGFilterElement;
class SVGFilterGraphNodeMap;
class SVGFilterPrimitiveStandardAttributes;

class FilterData final : public GarbageCollected<FilterData> {
 public:
  /*
   * The state transitions should follow the following:
   * Initial->RecordingContent->ReadyToPaint->PaintingFilter->ReadyToPaint
   *              |     ^                       |     ^
   *              v     |                       v     |
   *     RecordingContentCycleDetected     PaintingFilterCycle
   */
  enum FilterDataState {
    kInitial,
    kRecordingContent,
    kRecordingContentCycleDetected,
    kReadyToPaint,
    kPaintingFilter,
    kPaintingFilterCycleDetected
  };

  FilterData() : state_(kInitial) {}

  void Dispose();

  void Trace(blink::Visitor*);

  Member<FilterEffect> last_effect;
  Member<SVGFilterGraphNodeMap> node_map;
  FilterDataState state_;
};

class LayoutSVGResourceFilter final : public LayoutSVGResourceContainer {
 public:
  explicit LayoutSVGResourceFilter(SVGFilterElement*);
  ~LayoutSVGResourceFilter() override;

  bool IsChildAllowed(LayoutObject*, const ComputedStyle&) const override;

  const char* GetName() const override { return "LayoutSVGResourceFilter"; }
  bool IsOfType(LayoutObjectType type) const override {
    return type == kLayoutObjectSVGResourceFilter ||
           LayoutSVGResourceContainer::IsOfType(type);
  }

  void RemoveAllClientsFromCache(bool mark_for_invalidation = true) override;
  bool RemoveClientFromCache(SVGResourceClient&) override;

  FloatRect ResourceBoundingBox(const FloatRect& reference_box) const;

  SVGUnitTypes::SVGUnitType FilterUnits() const;
  SVGUnitTypes::SVGUnitType PrimitiveUnits() const;

  void PrimitiveAttributeChanged(SVGFilterPrimitiveStandardAttributes&,
                                 const QualifiedName&);

  static const LayoutSVGResourceType kResourceType = kFilterResourceType;
  LayoutSVGResourceType ResourceType() const override { return kResourceType; }

  FilterData* GetFilterDataForClient(const SVGResourceClient* client) {
    return filter_->at(const_cast<SVGResourceClient*>(client));
  }
  void SetFilterDataForClient(const SVGResourceClient* client,
                              FilterData* filter_data) {
    filter_->Set(const_cast<SVGResourceClient*>(client), filter_data);
  }

 protected:
  void WillBeDestroyed() override;

 private:
  void DisposeFilterMap();

  using FilterMap = HeapHashMap<Member<SVGResourceClient>, Member<FilterData>>;
  Persistent<FilterMap> filter_;
};

DEFINE_LAYOUT_OBJECT_TYPE_CASTS(LayoutSVGResourceFilter, IsSVGResourceFilter());

}  // namespace blink

#endif
