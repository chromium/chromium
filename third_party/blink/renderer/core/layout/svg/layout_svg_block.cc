/*
 * Copyright (C) 2006 Apple Computer, Inc.
 * Copyright (C) 2007 Nikolas Zimmermann <zimmermann@kde.org>
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

#include "third_party/blink/renderer/core/layout/svg/layout_svg_block.h"

#include "third_party/blink/renderer/core/layout/geometry/transform_state.h"
#include "third_party/blink/renderer/core/layout/layout_geometry_map.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/layout/svg/layout_svg_root.h"
#include "third_party/blink/renderer/core/layout/svg/svg_layout_support.h"
#include "third_party/blink/renderer/core/layout/svg/svg_resources.h"
#include "third_party/blink/renderer/core/layout/svg/svg_resources_cache.h"
#include "third_party/blink/renderer/core/style/shadow_list.h"
#include "third_party/blink/renderer/core/svg/svg_element.h"

namespace blink {

LayoutSVGBlock::LayoutSVGBlock(SVGElement* element)
    : LayoutBlockFlow(element) {}

SVGElement* LayoutSVGBlock::GetElement() const {
  return To<SVGElement>(LayoutObject::GetNode());
}

void LayoutSVGBlock::WillBeDestroyed() {
  SVGResourcesCache::ClientDestroyed(*this);
  SVGResources::ClearClipPathFilterMask(*GetElement(), Style());
  LayoutBlockFlow::WillBeDestroyed();
}

void LayoutSVGBlock::UpdateFromStyle() {
  LayoutBlockFlow::UpdateFromStyle();
  SetFloating(false);
}

void LayoutSVGBlock::StyleDidChange(StyleDifference diff,
                                    const ComputedStyle* old_style) {
  // Since layout depends on the bounds of the filter, we need to force layout
  // when the filter changes.
  if (diff.FilterChanged())
    SetNeedsLayout(layout_invalidation_reason::kStyleChange);

  if (diff.NeedsFullLayout()) {
    SetNeedsBoundariesUpdate();
    if (diff.TransformChanged())
      SetNeedsTransformUpdate();
  }

  if (IsBlendingAllowed()) {
    bool has_blend_mode_changed =
        (old_style && old_style->HasBlendMode()) == !StyleRef().HasBlendMode();
    if (Parent() && has_blend_mode_changed) {
      Parent()->DescendantIsolationRequirementsChanged(
          StyleRef().HasBlendMode() ? kDescendantIsolationRequired
                                    : kDescendantIsolationNeedsUpdate);
    }
  }

  LayoutBlock::StyleDidChange(diff, old_style);
  SVGResources::UpdateClipPathFilterMask(*GetElement(), old_style, StyleRef());
  SVGResourcesCache::ClientStyleChanged(*this, diff, StyleRef());
}

void LayoutSVGBlock::MapLocalToAncestor(const LayoutBoxModelObject* ancestor,
                                        TransformState& transform_state,
                                        MapCoordinatesFlags flags) const {
  // Convert from local HTML coordinates to local SVG coordinates.
  transform_state.Move(PhysicalLocation());
  // Apply other mappings on local SVG coordinates.
  SVGLayoutSupport::MapLocalToAncestor(this, ancestor, transform_state, flags);
}

void LayoutSVGBlock::MapAncestorToLocal(const LayoutBoxModelObject* ancestor,
                                        TransformState& transform_state,
                                        MapCoordinatesFlags flags) const {
  if (this == ancestor)
    return;

  // Map to local SVG coordinates.
  SVGLayoutSupport::MapAncestorToLocal(*this, ancestor, transform_state, flags);
  // Convert from local SVG coordinates to local HTML coordinates.
  transform_state.Move(PhysicalLocation());
}

const LayoutObject* LayoutSVGBlock::PushMappingToContainer(
    const LayoutBoxModelObject* ancestor_to_stop_at,
    LayoutGeometryMap& geometry_map) const {
  // Convert from local HTML coordinates to local SVG coordinates.
  geometry_map.Push(this, PhysicalLocation());
  // Apply other mappings on local SVG coordinates.
  return SVGLayoutSupport::PushMappingToContainer(this, ancestor_to_stop_at,
                                                  geometry_map);
}

PhysicalRect LayoutSVGBlock::VisualRectInDocument(VisualRectFlags flags) const {
  return SVGLayoutSupport::VisualRectInAncestorSpace(*this, *View(), flags);
}

bool LayoutSVGBlock::MapToVisualRectInAncestorSpaceInternal(
    const LayoutBoxModelObject* ancestor,
    TransformState& transform_state,
    VisualRectFlags) const {
  transform_state.Flatten();
  PhysicalRect rect(LayoutRect(transform_state.LastPlanarQuad().BoundingBox()));
  // Convert from local HTML coordinates to local SVG coordinates.
  rect.Move(PhysicalLocation());
  // Apply other mappings on local SVG coordinates.
  bool retval = SVGLayoutSupport::MapToVisualRectInAncestorSpace(
      *this, ancestor, FloatRect(rect), rect);
  transform_state.SetQuad(FloatQuad(FloatRect(rect)));
  return retval;
}

bool LayoutSVGBlock::NodeAtPoint(HitTestResult&,
                                 const HitTestLocation&,
                                 const PhysicalOffset&,
                                 HitTestAction) {
  NOTREACHED();
  return false;
}

}  // namespace blink
