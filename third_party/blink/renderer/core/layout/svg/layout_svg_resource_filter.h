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

class SVGFilterElement;

class LayoutSVGResourceFilter final : public LayoutSVGResourceContainer {
 public:
  explicit LayoutSVGResourceFilter(SVGFilterElement*);
  ~LayoutSVGResourceFilter() override;

  bool IsChildAllowed(LayoutObject*, const ComputedStyle&) const override;

  const char* GetName() const override { return "LayoutSVGResourceFilter"; }

  void RemoveAllClientsFromCache() override;

  FloatRect ResourceBoundingBox(const FloatRect& reference_box) const;

  SVGUnitTypes::SVGUnitType FilterUnits() const;
  SVGUnitTypes::SVGUnitType PrimitiveUnits() const;

  static const LayoutSVGResourceType kResourceType = kFilterResourceType;
  LayoutSVGResourceType ResourceType() const override { return kResourceType; }

 private:
  bool FindCycleFromSelf(SVGResourcesCycleSolver&) const override;
};

// Get the LayoutSVGResourceFilter from the 'filter' property iff the 'filter'
// is a single url(...) reference.
LayoutSVGResourceFilter* GetFilterResourceForSVG(const ComputedStyle&);

DEFINE_LAYOUT_SVG_RESOURCE_TYPE_CASTS(LayoutSVGResourceFilter,
                                      kFilterResourceType);

}  // namespace blink

#endif
