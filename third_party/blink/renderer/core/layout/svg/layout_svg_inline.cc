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
#include "third_party/blink/renderer/core/svg/svg_a_element.h"

namespace blink {

bool LayoutSVGInline::IsChildAllowed(LayoutObject* child,
                                     const ComputedStyle& style) const {
  if (child->IsText())
    return SVGLayoutSupport::IsLayoutableTextNode(child);

  if (IsSVGAElement(*GetNode())) {
    Node* child_node = child->GetNode();
    // Disallow direct descendant 'a'.
    if (child_node && IsSVGAElement(*child_node))
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
  InlineFlowBox* box = new SVGInlineFlowBox(LineLayoutItem(this));
  box->SetHasVirtualLogicalHeight();
  return box;
}

FloatRect LayoutSVGInline::ObjectBoundingBox() const {
  if (const LayoutSVGText* text_root =
          LayoutSVGText::LocateLayoutSVGTextAncestor(this))
    return text_root->ObjectBoundingBox();

  return FloatRect();
}

FloatRect LayoutSVGInline::StrokeBoundingBox() const {
  if (const LayoutSVGText* text_root =
          LayoutSVGText::LocateLayoutSVGTextAncestor(this))
    return text_root->StrokeBoundingBox();

  return FloatRect();
}

FloatRect LayoutSVGInline::VisualRectInLocalSVGCoordinates() const {
  if (const LayoutSVGText* text_root =
          LayoutSVGText::LocateLayoutSVGTextAncestor(this))
    return text_root->VisualRectInLocalSVGCoordinates();

  return FloatRect();
}

PhysicalRect LayoutSVGInline::VisualRectInDocument(
    VisualRectFlags flags) const {
  return SVGLayoutSupport::VisualRectInAncestorSpace(*this, *View(), flags);
}

void LayoutSVGInline::MapLocalToAncestor(const LayoutBoxModelObject* ancestor,
                                         TransformState& transform_state,
                                         MapCoordinatesFlags flags) const {
  SVGLayoutSupport::MapLocalToAncestor(this, ancestor, transform_state, flags);
}

const LayoutObject* LayoutSVGInline::PushMappingToContainer(
    const LayoutBoxModelObject* ancestor_to_stop_at,
    LayoutGeometryMap& geometry_map) const {
  return SVGLayoutSupport::PushMappingToContainer(this, ancestor_to_stop_at,
                                                  geometry_map);
}

void LayoutSVGInline::AbsoluteQuads(Vector<FloatQuad>& quads,
                                    MapCoordinatesFlags mode) const {
  const LayoutSVGText* text_root =
      LayoutSVGText::LocateLayoutSVGTextAncestor(this);
  if (!text_root)
    return;

  FloatRect text_bounding_box = text_root->StrokeBoundingBox();
  for (InlineFlowBox* box : *LineBoxes()) {
    quads.push_back(LocalToAbsoluteQuad(
        FloatRect(text_bounding_box.X() + box->X().ToFloat(),
                  text_bounding_box.Y() + box->Y().ToFloat(),
                  box->Width().ToFloat(), box->Height().ToFloat()),
        mode));
  }
}

void LayoutSVGInline::WillBeDestroyed() {
  SVGResourcesCache::ClientDestroyed(*this);
  SVGResources::ClearClipPathFilterMask(To<SVGElement>(*GetNode()), Style());
  SVGResources::ClearPaints(To<SVGElement>(*GetNode()), Style());
  LayoutInline::WillBeDestroyed();
}

void LayoutSVGInline::StyleDidChange(StyleDifference diff,
                                     const ComputedStyle* old_style) {
  // Since layout depends on the bounds of the filter, we need to force layout
  // when the filter changes.
  if (diff.FilterChanged())
    SetNeedsLayout(layout_invalidation_reason::kStyleChange);

  if (diff.NeedsFullLayout())
    SetNeedsBoundariesUpdate();

  LayoutInline::StyleDidChange(diff, old_style);
  SVGResources::UpdateClipPathFilterMask(To<SVGElement>(*GetNode()), old_style,
                                         StyleRef());
  SVGResources::UpdatePaints(To<SVGElement>(*GetNode()), old_style, StyleRef());
  SVGResourcesCache::ClientStyleChanged(*this, diff, StyleRef());
}

void LayoutSVGInline::AddChild(LayoutObject* child,
                               LayoutObject* before_child) {
  LayoutInline::AddChild(child, before_child);
  SVGResourcesCache::ClientWasAddedToTree(*child, child->StyleRef());

  if (LayoutSVGText* text_layout_object =
          LayoutSVGText::LocateLayoutSVGTextAncestor(this))
    text_layout_object->SubtreeChildWasAdded();
}

void LayoutSVGInline::RemoveChild(LayoutObject* child) {
  SVGResourcesCache::ClientWillBeRemovedFromTree(*child);

  if (LayoutSVGText* text_layout_object =
          LayoutSVGText::LocateLayoutSVGTextAncestor(this))
    text_layout_object->SubtreeChildWillBeRemoved();
  LayoutInline::RemoveChild(child);
}

}  // namespace blink
