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
#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_cursor.h"
#include "third_party/blink/renderer/core/layout/ng/svg/layout_ng_svg_text.h"
#include "third_party/blink/renderer/core/layout/svg/layout_svg_inline_text.h"
#include "third_party/blink/renderer/core/layout/svg/layout_svg_resource_container.h"
#include "third_party/blink/renderer/core/layout/svg/layout_svg_text.h"
#include "third_party/blink/renderer/core/layout/svg/line/svg_inline_flow_box.h"
#include "third_party/blink/renderer/core/layout/svg/svg_layout_support.h"
#include "third_party/blink/renderer/core/layout/svg/svg_resources.h"
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
    // https://svgwg.org/svg2-draft/linking.html#AElement
    // any element or text allowed by its parent's content model, ...
    if (Parent()) {
      if (!Parent()->IsChildAllowed(child, style))
        return false;
    }
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
  InlineFlowBox* box =
      MakeGarbageCollected<SVGInlineFlowBox>(LineLayoutItem(this));
  box->SetHasVirtualLogicalHeight();
  return box;
}

bool LayoutSVGInline::IsObjectBoundingBoxValid() const {
  if (IsInLayoutNGInlineFormattingContext()) {
    NGInlineCursor cursor;
    cursor.MoveToIncludingCulledInline(*this);
    return cursor.IsNotNull();
  }
  return FirstLineBox();
}

// static
void LayoutSVGInline::ObjectBoundingBoxForCursor(NGInlineCursor& cursor,
                                                 gfx::RectF& bounds) {
  for (; cursor; cursor.MoveToNextForSameLayoutObject()) {
    const NGFragmentItem& item = *cursor.CurrentItem();
    if (item.Type() == NGFragmentItem::kSvgText) {
      bounds.Union(cursor.Current().ObjectBoundingBox(cursor));
    } else if (NGInlineCursor descendants = cursor.CursorForDescendants()) {
      for (; descendants; descendants.MoveToNext()) {
        const NGFragmentItem& descendant_item = *descendants.CurrentItem();
        if (descendant_item.Type() == NGFragmentItem::kSvgText)
          bounds.Union(descendants.Current().ObjectBoundingBox(cursor));
      }
    }
  }
}

gfx::RectF LayoutSVGInline::ObjectBoundingBox() const {
  NOT_DESTROYED();
  gfx::RectF bounds;
  if (IsInLayoutNGInlineFormattingContext()) {
    NGInlineCursor cursor;
    cursor.MoveToIncludingCulledInline(*this);
    ObjectBoundingBoxForCursor(cursor, bounds);
    return bounds;
  }
  for (InlineFlowBox* box : *LineBoxes())
    bounds.Union(gfx::RectF(box->FrameRect()));
  return bounds;
}

gfx::RectF LayoutSVGInline::StrokeBoundingBox() const {
  NOT_DESTROYED();
  if (!IsObjectBoundingBoxValid())
    return gfx::RectF();
  return SVGLayoutSupport::ExtendTextBBoxWithStroke(*this, ObjectBoundingBox());
}

gfx::RectF LayoutSVGInline::VisualRectInLocalSVGCoordinates() const {
  NOT_DESTROYED();
  if (!IsObjectBoundingBoxValid())
    return gfx::RectF();
  return SVGLayoutSupport::ComputeVisualRectForText(*this, ObjectBoundingBox());
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

void LayoutSVGInline::AbsoluteQuads(Vector<gfx::QuadF>& quads,
                                    MapCoordinatesFlags mode) const {
  NOT_DESTROYED();
  if (IsInLayoutNGInlineFormattingContext()) {
    NGInlineCursor cursor;
    for (cursor.MoveToIncludingCulledInline(*this); cursor;
         cursor.MoveToNextForSameLayoutObject()) {
      const NGFragmentItem& item = *cursor.CurrentItem();
      if (item.Type() == NGFragmentItem::kSvgText) {
        quads.push_back(LocalToAbsoluteQuad(
            gfx::QuadF(SVGLayoutSupport::ExtendTextBBoxWithStroke(
                *this, cursor.Current().ObjectBoundingBox(cursor))),
            mode));
      }
    }
    return;
  }
  for (InlineFlowBox* box : *LineBoxes()) {
    gfx::RectF box_rect(box->FrameRect());
    quads.push_back(LocalToAbsoluteQuad(
        gfx::QuadF(SVGLayoutSupport::ExtendTextBBoxWithStroke(*this, box_rect)),
        mode));
  }
}

void LayoutSVGInline::AddOutlineRects(Vector<PhysicalRect>& rect_list,
                                      OutlineInfo* info,
                                      const PhysicalOffset& additional_offset,
                                      NGOutlineType outline_type) const {
  if (!IsInLayoutNGInlineFormattingContext()) {
    LayoutInline::AddOutlineRects(rect_list, nullptr, additional_offset,
                                  outline_type);
  } else {
    auto rect = PhysicalRect::EnclosingRect(ObjectBoundingBox());
    rect.Move(additional_offset);
    rect_list.push_back(rect);
  }
  if (info)
    *info = OutlineInfo::GetUnzoomedFromStyle(StyleRef());
}

void LayoutSVGInline::WillBeDestroyed() {
  NOT_DESTROYED();
  SVGResources::ClearEffects(*this);
  SVGResources::ClearPaints(*this, Style());
  LayoutInline::WillBeDestroyed();
}

void LayoutSVGInline::StyleDidChange(StyleDifference diff,
                                     const ComputedStyle* old_style) {
  NOT_DESTROYED();
  if (diff.HasDifference()) {
    if (auto* svg_text = DynamicTo<LayoutNGSVGText>(
            LayoutSVGText::LocateLayoutSVGTextAncestor(this))) {
      if (svg_text->NeedsTextMetricsUpdate())
        diff.SetNeedsFullLayout();
    }
  }
  LayoutInline::StyleDidChange(diff, old_style);

  if (diff.NeedsFullLayout())
    SetNeedsBoundariesUpdate();

  SVGResources::UpdateEffects(*this, diff, old_style);
  SVGResources::UpdatePaints(*this, old_style, StyleRef());

  if (!Parent())
    return;
  if (diff.HasDifference())
    LayoutSVGResourceContainer::StyleChanged(*this, diff);
}

void LayoutSVGInline::AddChild(LayoutObject* child,
                               LayoutObject* before_child) {
  NOT_DESTROYED();
  LayoutInline::AddChild(child, before_child);
  LayoutSVGText::NotifySubtreeStructureChanged(
      this, layout_invalidation_reason::kChildChanged);
}

void LayoutSVGInline::RemoveChild(LayoutObject* child) {
  NOT_DESTROYED();
  LayoutSVGText::NotifySubtreeStructureChanged(
      this, layout_invalidation_reason::kChildChanged);
  LayoutInline::RemoveChild(child);
}

void LayoutSVGInline::InsertedIntoTree() {
  NOT_DESTROYED();
  LayoutInline::InsertedIntoTree();
  LayoutSVGResourceContainer::MarkForLayoutAndParentResourceInvalidation(*this,
                                                                         false);
  if (StyleRef().HasSVGEffect())
    SetNeedsPaintPropertyUpdate();
}

void LayoutSVGInline::WillBeRemovedFromTree() {
  NOT_DESTROYED();
  LayoutSVGResourceContainer::MarkForLayoutAndParentResourceInvalidation(*this,
                                                                         false);
  if (StyleRef().HasSVGEffect())
    SetNeedsPaintPropertyUpdate();
  LayoutInline::WillBeRemovedFromTree();
}

}  // namespace blink
