/*
 * Copyright (C) 2006 Oliver Hunt <ojh16@student.canterbury.ac.nz>
 * Copyright (C) 2006 Apple Inc. All rights reserved.
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

#include "third_party/blink/renderer/core/layout/svg/layout_svg_inline.h"

#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/layout/svg/layout_svg_text.h"
#include "third_party/blink/renderer/core/layout/svg/line/svg_inline_flow_box.h"
#include "third_party/blink/renderer/core/layout/svg/svg_layout_support.h"
#include "third_party/blink/renderer/core/layout/svg/svg_resources.h"
#include "third_party/blink/renderer/core/layout/svg/svg_resources_cache.h"
#include "third_party/blink/renderer/core/paint/compositing/compositing_reason_finder.h"
#include "third_party/blink/renderer/core/svg/svg_a_element.h"

namespace blink {

bool LayoutSVGInline::IsChildAllowed(LayoutObject* child,
                                     const ComputedStyle& style) const {
  NOT_DESTROYED();
  if (child->IsText())
    return SVGLayoutSupport::IsLayoutableTextNode(child);

  if (IsA<SVGAElement>(*GetNode())) {
    Node* child_node = child->GetNode();
    // Disallow direct descendant 'a'.
    if (child_node && IsA<SVGAElement>(*child_node))
      return false;
  }

  if (!child->IsSVGInline() && !child->IsSVGInlineText())
    return false;

  return LayoutInline::IsChildAllowed(child, style);
}

LayoutSVGInline::LayoutSVGInline(Element* element) : LayoutInline(element) {
  SetAlwaysCreateLineBoxes();
}

InlineFlowBox* LayoutSVGInline::CreateInlineFlowBox() {
  NOT_DESTROYED();
  InlineFlowBox* box = new SVGInlineFlowBox(LineLayoutItem(this));
  box->SetHasVirtualLogicalHeight();
  return box;
}

FloatRect LayoutSVGInline::ObjectBoundingBox() const {
  NOT_DESTROYED();
  FloatRect bounds;
  for (InlineFlowBox* box : *LineBoxes())
    bounds.Unite(FloatRect(box->FrameRect()));
  return bounds;
}

FloatRect LayoutSVGInline::StrokeBoundingBox() const {
  NOT_DESTROYED();
  if (!FirstLineBox())
    return FloatRect();
  return SVGLayoutSupport::ExtendTextBBoxWithStroke(*this, ObjectBoundingBox());
}

FloatRect LayoutSVGInline::VisualRectInLocalSVGCoordinates() const {
  NOT_DESTROYED();
  if (!FirstLineBox())
    return FloatRect();
  const LayoutSVGText* text_root =
      LayoutSVGText::LocateLayoutSVGTextAncestor(this);
  if (!text_root)
    return FloatRect();
  return SVGLayoutSupport::ComputeVisualRectForText(
      *this, ObjectBoundingBox(), text_root->ObjectBoundingBox());
}

PhysicalRect LayoutSVGInline::VisualRectInDocument(
    VisualRectFlags flags) const {
  NOT_DESTROYED();
  return SVGLayoutSupport::VisualRectInAncestorSpace(*this, *View(), flags);
}

void LayoutSVGInline::MapLocalToAncestor(const LayoutBoxModelObject* ancestor,
                                         TransformState& transform_state,
                                         MapCoordinatesFlags flags) const {
  NOT_DESTROYED();
  SVGLayoutSupport::MapLocalToAncestor(this, ancestor, transform_state, flags);
}

const LayoutObject* LayoutSVGInline::PushMappingToContainer(
    const LayoutBoxModelObject* ancestor_to_stop_at,
    LayoutGeometryMap& geometry_map) const {
  NOT_DESTROYED();
  return SVGLayoutSupport::PushMappingToContainer(this, ancestor_to_stop_at,
                                                  geometry_map);
}

void LayoutSVGInline::AbsoluteQuads(Vector<FloatQuad>& quads,
                                    MapCoordinatesFlags mode) const {
  NOT_DESTROYED();
  for (InlineFlowBox* box : *LineBoxes()) {
    FloatRect box_rect(box->FrameRect());
    quads.push_back(LocalToAbsoluteQuad(
        SVGLayoutSupport::ExtendTextBBoxWithStroke(*this, box_rect), mode));
  }
}

void LayoutSVGInline::WillBeDestroyed() {
  NOT_DESTROYED();
  SVGResourcesCache::ClientDestroyed(*this);
  SVGResources::ClearClipPathFilterMask(To<SVGElement>(*GetNode()), Style());
  SVGResources::ClearPaints(To<SVGElement>(*GetNode()), Style());
  LayoutInline::WillBeDestroyed();
}

void LayoutSVGInline::StyleDidChange(StyleDifference diff,
                                     const ComputedStyle* old_style) {
  NOT_DESTROYED();
  // Since layout depends on the bounds of the filter, we need to force layout
  // when the filter changes.
  if (diff.FilterChanged())
    SetNeedsLayout(layout_invalidation_reason::kStyleChange);

  if (diff.NeedsFullLayout())
    SetNeedsBoundariesUpdate();

  if (diff.CompositingReasonsChanged())
    SVGLayoutSupport::NotifySVGRootOfChangedCompositingReasons(this);

  LayoutInline::StyleDidChange(diff, old_style);
  SVGResources::UpdateClipPathFilterMask(To<SVGElement>(*GetNode()), old_style,
                                         StyleRef());
  SVGResources::UpdatePaints(To<SVGElement>(*GetNode()), old_style, StyleRef());
  SVGResourcesCache::ClientStyleChanged(*this, diff, StyleRef());
}

void LayoutSVGInline::AddChild(LayoutObject* child,
                               LayoutObject* before_child) {
  NOT_DESTROYED();
  LayoutInline::AddChild(child, before_child);
  SVGResourcesCache::ClientWasAddedToTree(*child);
  LayoutSVGText::NotifySubtreeStructureChanged(
      this, layout_invalidation_reason::kChildChanged);
}

void LayoutSVGInline::RemoveChild(LayoutObject* child) {
  NOT_DESTROYED();
  SVGResourcesCache::ClientWillBeRemovedFromTree(*child);
  LayoutSVGText::NotifySubtreeStructureChanged(
      this, layout_invalidation_reason::kChildChanged);
  LayoutInline::RemoveChild(child);
}

void LayoutSVGInline::InsertedIntoTree() {
  NOT_DESTROYED();
  LayoutInline::InsertedIntoTree();
  if (CompositingReasonFinder::DirectReasonsForSVGChildPaintProperties(*this) !=
      CompositingReason::kNone) {
    SVGLayoutSupport::NotifySVGRootOfChangedCompositingReasons(this);
  }
}

void LayoutSVGInline::WillBeRemovedFromTree() {
  NOT_DESTROYED();
  LayoutInline::WillBeRemovedFromTree();
  if (CompositingReasonFinder::DirectReasonsForSVGChildPaintProperties(*this) !=
      CompositingReason::kNone) {
    SVGLayoutSupport::NotifySVGRootOfChangedCompositingReasons(this);
  }
}

}  // namespace blink
