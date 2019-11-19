/*
 * Copyright (C) 2006 Oliver Hunt <ojh16@student.canterbury.ac.nz>
 * Copyright (C) 2006 Apple Computer Inc.
 * Copyright (C) 2007 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) Research In Motion Limited 2010. All rights reserved.
 * Copyright (C) 2011 Torch Mobile (Beijing) CO. Ltd. All rights reserved.
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

#include "third_party/blink/renderer/core/layout/svg/line/svg_root_inline_box.h"

#include "third_party/blink/renderer/core/layout/api/line_layout_api_shim.h"
#include "third_party/blink/renderer/core/layout/api/line_layout_block_flow.h"
#include "third_party/blink/renderer/core/layout/api/line_layout_svg_inline_text.h"
#include "third_party/blink/renderer/core/layout/svg/layout_svg_text.h"
#include "third_party/blink/renderer/core/layout/svg/line/svg_inline_flow_box.h"
#include "third_party/blink/renderer/core/layout/svg/line/svg_inline_text_box.h"
#include "third_party/blink/renderer/core/layout/svg/svg_text_layout_engine.h"
#include "third_party/blink/renderer/core/paint/svg_root_inline_box_painter.h"

namespace blink {

void SVGRootInlineBox::Paint(const PaintInfo& paint_info,
                             const LayoutPoint& paint_offset,
                             LayoutUnit,
                             LayoutUnit) const {
  SVGRootInlineBoxPainter(*this).Paint(paint_info, paint_offset);
}

void SVGRootInlineBox::MarkDirty() {
  for (InlineBox* child = FirstChild(); child; child = child->NextOnLine())
    child->MarkDirty();
  RootInlineBox::MarkDirty();
}

void SVGRootInlineBox::ComputePerCharacterLayoutInformation() {
  LayoutSVGText& text_root =
      ToLayoutSVGText(*LineLayoutAPIShim::LayoutObjectFrom(Block()));

  const Vector<LayoutSVGInlineText*>& descendant_text_nodes =
      text_root.DescendantTextNodes();
  if (descendant_text_nodes.IsEmpty())
    return;

  if (text_root.NeedsReordering())
    ReorderValueLists();

  // Perform SVG text layout phase two (see SVGTextLayoutEngine for details).
  SVGTextLayoutEngine character_layout(descendant_text_nodes);
  character_layout.LayoutCharactersInTextBoxes(this);

  // Perform SVG text layout phase three (see SVGTextChunkBuilder for details).
  character_layout.FinishLayout();

  // Perform SVG text layout phase four
  // Position & resize all SVGInlineText/FlowBoxes in the inline box tree,
  // resize the root box as well as the LayoutSVGText parent block.
  LayoutInlineBoxes(*this);

  // Let the HTML block space originate from the local SVG coordinate space.
  LineLayoutBlockFlow parent_block = Block();
  parent_block.SetLocation(LayoutPoint());
  // The width could be any value, but set it so that a line box will mirror
  // within the childRect when its coordinates are converted between physical
  // block direction and flipped block direction, for ease of understanding of
  // flipped coordinates. The height doesn't matter.
  parent_block.SetSize(LayoutSize(X() * 2 + Width(), LayoutUnit()));

  SetLineTopBottomPositions(LogicalTop(), LogicalBottom(), LogicalTop(),
                            LogicalBottom());
}

FloatRect SVGRootInlineBox::LayoutInlineBoxes(InlineBox& box) {
  FloatRect rect;
  if (box.IsSVGInlineTextBox()) {
    rect = ToSVGInlineTextBox(box).CalculateBoundaries();
  } else {
    for (InlineBox* child = ToInlineFlowBox(box).FirstChild(); child;
         child = child->NextOnLine())
      rect.Unite(LayoutInlineBoxes(*child));
  }

  LayoutRect logical_rect(EnclosingLayoutRect(rect));
  if (!box.IsHorizontal())
    logical_rect.SetSize(logical_rect.Size().TransposedSize());

  box.SetX(logical_rect.X());
  box.SetY(logical_rect.Y());
  box.SetLogicalWidth(logical_rect.Width());
  if (box.IsSVGInlineTextBox())
    ToSVGInlineTextBox(box).SetLogicalHeight(logical_rect.Height());
  else if (box.IsSVGInlineFlowBox())
    ToSVGInlineFlowBox(box).SetLogicalHeight(logical_rect.Height());
  else
    ToSVGRootInlineBox(box).SetLogicalHeight(logical_rect.Height());

  return rect;
}

InlineBox* SVGRootInlineBox::ClosestLeafChildForPosition(
    const PhysicalOffset& point) {
  InlineBox* first_leaf = FirstLeafChild();
  InlineBox* last_leaf = LastLeafChild();
  if (first_leaf == last_leaf)
    return first_leaf;

  // FIXME: Check for vertical text!
  InlineBox* closest_leaf = nullptr;
  for (InlineBox* leaf = first_leaf; leaf; leaf = leaf->NextLeafChild()) {
    if (!leaf->IsSVGInlineTextBox())
      continue;
    if (point.top < leaf->Y())
      continue;
    if (point.left > leaf->Y() + leaf->VirtualLogicalHeight())
      continue;

    closest_leaf = leaf;
    if (point.left < leaf->X() + leaf->LogicalWidth())
      return leaf;
  }

  return closest_leaf ? closest_leaf : last_leaf;
}

static inline void SwapPositioningValuesInTextBoxes(
    SVGInlineTextBox* first_text_box,
    SVGInlineTextBox* last_text_box) {
  LineLayoutSVGInlineText first_text_node =
      LineLayoutSVGInlineText(first_text_box->GetLineLayoutItem());
  SVGCharacterDataMap& first_character_data_map =
      first_text_node.CharacterDataMap();
  SVGCharacterDataMap::iterator it_first =
      first_character_data_map.find(first_text_box->Start() + 1);
  if (it_first == first_character_data_map.end())
    return;
  LineLayoutSVGInlineText last_text_node =
      LineLayoutSVGInlineText(last_text_box->GetLineLayoutItem());
  SVGCharacterDataMap& last_character_data_map =
      last_text_node.CharacterDataMap();
  SVGCharacterDataMap::iterator it_last =
      last_character_data_map.find(last_text_box->Start() + 1);
  if (it_last == last_character_data_map.end())
    return;
  // We only want to perform the swap if both inline boxes are absolutely
  // positioned.
  std::swap(it_first->value, it_last->value);
}

static inline void ReverseInlineBoxRangeAndValueListsIfNeeded(
    Vector<InlineBox*>::iterator first,
    Vector<InlineBox*>::iterator last) {
  // This is a copy of std::reverse(first, last). It additionally assures
  // that the metrics map within the layoutObjects belonging to the
  // InlineBoxes are reordered as well.
  while (true) {
    if (first == last || first == --last)
      return;

    if ((*last)->IsSVGInlineTextBox() && (*first)->IsSVGInlineTextBox()) {
      SVGInlineTextBox* first_text_box = ToSVGInlineTextBox(*first);
      SVGInlineTextBox* last_text_box = ToSVGInlineTextBox(*last);

      // Reordering is only necessary for BiDi text that is _absolutely_
      // positioned.
      if (first_text_box->Len() == 1 &&
          first_text_box->Len() == last_text_box->Len())
        SwapPositioningValuesInTextBoxes(first_text_box, last_text_box);
    }

    InlineBox* temp = *first;
    *first = *last;
    *last = temp;
    ++first;
  }
}

void SVGRootInlineBox::ReorderValueLists() {
  Vector<InlineBox*> leaf_boxes_in_logical_order;
  CollectLeafBoxesInLogicalOrder(leaf_boxes_in_logical_order,
                                 ReverseInlineBoxRangeAndValueListsIfNeeded);
}

bool SVGRootInlineBox::NodeAtPoint(HitTestResult& result,
                                   const HitTestLocation& hit_test_location,
                                   const PhysicalOffset& accumulated_offset,
                                   LayoutUnit line_top,
                                   LayoutUnit line_bottom) {
  // Iterate the text boxes in reverse so that the top-most node will be considered first.
  for (InlineBox* leaf = LastLeafChild(); leaf; leaf = leaf->PrevLeafChild()) {
    if (!leaf->IsSVGInlineTextBox())
      continue;
    if (leaf->NodeAtPoint(result, hit_test_location, accumulated_offset,
                          line_top, line_bottom))
      return true;
  }

  return false;
}

}  // namespace blink
