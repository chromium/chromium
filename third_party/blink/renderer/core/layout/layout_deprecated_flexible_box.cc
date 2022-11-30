/*
 * This file is part of the layout object implementation for KHTML.
 *
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2003 Apple Computer, Inc.
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
 *
 */

#include "third_party/blink/renderer/core/layout/layout_deprecated_flexible_box.h"

#include <algorithm>
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/layout/api/line_layout_block_flow.h"
#include "third_party/blink/renderer/core/layout/layout_object_inlines.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/layout/text_autosizer.h"
#include "third_party/blink/renderer/core/layout/text_run_constructor.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"
#include "third_party/blink/renderer/platform/fonts/font.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"
#include "third_party/blink/renderer/platform/wtf/text/character_names.h"

namespace blink {

// Helper methods for obtaining the last line, computing line counts and heights
// for line counts
// (crawling into blocks).
static bool ShouldCheckLines(LayoutBlockFlow* block_flow) {
  return !block_flow->IsFloatingOrOutOfFlowPositioned() &&
         block_flow->StyleRef().Height().IsAuto();
}

static int GetHeightForLineCount(const LayoutBlockFlow* block_flow,
                                 int line_count,
                                 bool include_bottom,
                                 int& count) {
  if (block_flow->ChildrenInline()) {
    for (RootInlineBox* box = block_flow->FirstRootBox(); box;
         box = box->NextRootBox()) {
      if (++count == line_count)
        return (box->LineBottomWithLeading() +
                (include_bottom ? (block_flow->BorderBottom() +
                                   block_flow->PaddingBottom())
                                : LayoutUnit()))
            .ToInt();
    }
    return -1;
  }

  LayoutBox* normal_flow_child_without_lines = nullptr;
  for (LayoutBox* obj = block_flow->FirstChildBox(); obj;
       obj = obj->NextSiblingBox()) {
    auto* child_block_flow = DynamicTo<LayoutBlockFlow>(obj);
    if (child_block_flow && ShouldCheckLines(child_block_flow)) {
      int result =
          GetHeightForLineCount(child_block_flow, line_count, false, count);
      if (result != -1)
        return (result + obj->Location().Y() +
                (include_bottom ? (child_block_flow->BorderBottom() +
                                   child_block_flow->PaddingBottom())
                                : LayoutUnit()))
            .ToInt();
    } else if (!obj->IsFloatingOrOutOfFlowPositioned()) {
      normal_flow_child_without_lines = obj;
    }
  }
  if (normal_flow_child_without_lines && line_count == 0)
    return (normal_flow_child_without_lines->Location().Y() +
            normal_flow_child_without_lines->Size().Height())
        .ToInt();

  return -1;
}

static RootInlineBox* LineAtIndex(const LayoutBlockFlow* block_flow, int i) {
  DCHECK_GE(i, 0);

  if (block_flow->ChildrenInline()) {
    for (RootInlineBox* box = block_flow->FirstRootBox(); box;
         box = box->NextRootBox()) {
      if (!i--)
        return box;
    }
    return nullptr;
  }
  for (LayoutObject* child = block_flow->FirstChild(); child;
       child = child->NextSibling()) {
    auto* child_block_flow = DynamicTo<LayoutBlockFlow>(child);
    if (!child_block_flow)
      continue;
    if (!ShouldCheckLines(child_block_flow))
      continue;
    if (RootInlineBox* box = LineAtIndex(child_block_flow, i))
      return box;
  }

  return nullptr;
}

static int LineCount(const LayoutBlockFlow* block_flow,
                     const RootInlineBox* stop_root_inline_box = nullptr,
                     bool* found = nullptr) {
  int count = 0;
  if (block_flow->ChildrenInline()) {
    for (RootInlineBox* box = block_flow->FirstRootBox(); box;
         box = box->NextRootBox()) {
      count++;
      if (box == stop_root_inline_box) {
        if (found)
          *found = true;
        break;
      }
    }
    return count;
  }
  for (LayoutObject* obj = block_flow->FirstChild(); obj;
       obj = obj->NextSibling()) {
    auto* child_block_flow = DynamicTo<LayoutBlockFlow>(obj);
    if (!child_block_flow)
      continue;
    if (!ShouldCheckLines(child_block_flow))
      continue;
    bool recursive_found = false;
    count +=
        LineCount(child_block_flow, stop_root_inline_box, &recursive_found);
    if (recursive_found) {
      if (found)
        *found = true;
      break;
    }
  }
  return count;
}

static void ClearTruncation(LayoutBlockFlow* block_flow) {
  if (block_flow->ChildrenInline() && block_flow->HasMarkupTruncation()) {
    block_flow->SetHasMarkupTruncation(false);
    for (RootInlineBox* box = block_flow->FirstRootBox(); box;
         box = box->NextRootBox())
      box->ClearTruncation();
    return;
  }
  for (LayoutObject* obj = block_flow->FirstChild(); obj;
       obj = obj->NextSibling()) {
    auto* child_block_flow = DynamicTo<LayoutBlockFlow>(obj);
    if (!child_block_flow)
      continue;
    if (ShouldCheckLines(child_block_flow))
      ClearTruncation(child_block_flow);
  }
}

LayoutDeprecatedFlexibleBox::LayoutDeprecatedFlexibleBox(Element* element)
    : LayoutBlock(element) {
  DCHECK(!ChildrenInline());
}

LayoutDeprecatedFlexibleBox::~LayoutDeprecatedFlexibleBox() = default;

static LayoutUnit MarginWidthForChild(LayoutBox* child) {
  // A margin basically has three types: fixed, percentage, and auto (variable).
  // Auto and percentage margins simply become 0 when computing min/max width.
  // Fixed margins can be added in as is.
  const Length& margin_left = child->StyleRef().MarginLeft();
  const Length& margin_right = child->StyleRef().MarginRight();
  LayoutUnit margin;
  if (margin_left.IsFixed())
    margin += margin_left.Value();
  if (margin_right.IsFixed())
    margin += margin_right.Value();
  return margin;
}

MinMaxSizes LayoutDeprecatedFlexibleBox::ComputeIntrinsicLogicalWidths() const {
  NOT_DESTROYED();
  MinMaxSizes sizes;
  for (LayoutBox* child = FirstChildBox(); child;
       child = child->NextSiblingBox()) {
    if (child->IsOutOfFlowPositioned())
      continue;

    MinMaxSizes child_sizes = child->PreferredLogicalWidths();
    child_sizes += MarginWidthForChild(child);

    sizes.Encompass(child_sizes);
  }

  sizes.max_size = std::max(sizes.min_size, sizes.max_size);
  sizes +=
      BorderAndPaddingLogicalWidth() + ComputeLogicalScrollbars().InlineSum();
  return sizes;
}

void LayoutDeprecatedFlexibleBox::UpdateBlockLayout(bool relayout_children) {
  NOT_DESTROYED();
  DCHECK(NeedsLayout());
  DCHECK_EQ(StyleRef().BoxOrient(), EBoxOrient::kVertical);
  DCHECK(StyleRef().HasLineClamp());
  UseCounter::Count(GetDocument(), WebFeature::kWebkitBoxLayout);

  if (!relayout_children && SimplifiedLayout())
    return;

  {
    // LayoutState needs this deliberate scope to pop before paint invalidation.
    LayoutState state(*this);

    LayoutSize previous_size = Size();

    UpdateLogicalWidth();
    UpdateLogicalHeight();

    TextAutosizer::LayoutScope text_autosizer_layout_scope(this);

    if (previous_size != Size())
      relayout_children = true;

    SetHeight(LayoutUnit());

    LayoutVerticalBox(relayout_children);

    LayoutUnit old_client_after_edge = ClientLogicalBottom();
    UpdateLogicalHeight();

    if (previous_size.Height() != Size().Height())
      relayout_children = true;

    LayoutPositionedObjects(relayout_children || IsDocumentElement());

    ComputeLayoutOverflow(old_client_after_edge);
  }

  UpdateAfterLayout();

  ClearNeedsLayout();
}

void LayoutDeprecatedFlexibleBox::LayoutVerticalBox(bool relayout_children) {
  NOT_DESTROYED();
  LayoutUnit to_add =
      BorderBottom() + PaddingBottom() + ComputeScrollbars().bottom;

  // We confine the line clamp ugliness to vertical flexible boxes (thus keeping
  // it out of mainstream block layout); this is not really part of the XUL box
  // model.
  ApplyLineClamp(relayout_children);

  PaintLayerScrollableArea::DelayScrollOffsetClampScope delay_clamp_scope;

  SetHeight(BorderTop() + PaddingTop() + ComputeScrollbars().top);
  LayoutUnit min_height = Size().Height() + to_add;

  for (LayoutBox* child = FirstChildBox(); child;
       child = child->NextSiblingBox()) {
    if (child->IsOutOfFlowPositioned()) {
      child->ContainingBlock()->InsertPositionedObject(child);
      PaintLayer* child_layer = child->Layer();
      child_layer->SetStaticInlinePosition(BorderStart() + PaddingStart());
      if (child_layer->StaticBlockPosition() != Size().Height()) {
        child_layer->SetStaticBlockPosition(Size().Height());
        if (child->StyleRef().HasStaticBlockPosition(
                StyleRef().IsHorizontalWritingMode()))
          child->SetChildNeedsLayout(kMarkOnlyThis);
      }
      continue;
    }

    // Compute the child's vertical margins.
    child->ComputeAndSetBlockDirectionMargins(this);

    // Add in the child's marginTop to our height.
    SetHeight(Size().Height() + child->MarginTop());

    SubtreeLayoutScope layout_scope(*child);
    if (!child->NeedsLayout())
      MarkChildForPaginationRelayoutIfNeeded(*child, layout_scope);

    // Now do a layout.
    child->LayoutIfNeeded();

    // Place the child.
    LayoutUnit child_x = BorderLeft() + PaddingLeft();
    if (StyleRef().IsLeftToRightDirection()) {
      child_x += child->MarginLeft();
    } else {
      child_x += ContentWidth() - child->MarginRight() - child->Size().Width();
    }
    // TODO(crbug.com/370010): Investigate if this can be removed based on
    // other flags.
    child->SetShouldCheckForPaintInvalidation();
    child->SetLocation(LayoutPoint(child_x, Size().Height()));

    SetHeight(Size().Height() + child->Size().Height() + child->MarginBottom());

    if (View()->GetLayoutState()->IsPaginated())
      UpdateFragmentationInfoForChild(*child);
  }

  if (!FirstChildBox() && HasLineIfEmpty()) {
    SetHeight(Size().Height() + LineHeight(true,
                                           StyleRef().IsHorizontalWritingMode()
                                               ? kHorizontalLine
                                               : kVerticalLine,
                                           kPositionOfInteriorLineBoxes));
  }

  SetHeight(Size().Height() + to_add);

  // Negative margins can cause our height to shrink below our minimal height
  // (border/padding).  If this happens, ensure that the computed height is
  // increased to the minimal height.
  if (Size().Height() < min_height)
    SetHeight(min_height);

  // Now we have to calc our height, so we know how much space we have
  // remaining.
  LayoutUnit old_height = Size().Height();
  UpdateLogicalHeight();

  // So that the computeLogicalHeight in layoutBlock() knows to relayout
  // positioned objects because of a height change, we revert our height back
  // to the intrinsic height before returning.
  if (old_height != Size().Height())
    SetHeight(old_height);
}

void LayoutDeprecatedFlexibleBox::ApplyLineClamp(bool relayout_children) {
  NOT_DESTROYED();
  int max_line_count = 0;
  for (LayoutBox* child = FirstChildBox(); child;
       child = child->NextSiblingBox()) {
    if (child->IsOutOfFlowPositioned())
      continue;

    child->ClearOverrideSize();
    if (relayout_children ||
        (child->IsAtomicInlineLevel() &&
         (child->StyleRef().Width().IsPercentOrCalc() ||
          child->StyleRef().Height().IsPercentOrCalc())) ||
        (child->StyleRef().Height().IsAuto() && child->IsLayoutBlock())) {
      child->SetChildNeedsLayout(kMarkOnlyThis);

      // Dirty all the positioned objects.
      auto* child_block_flow = DynamicTo<LayoutBlockFlow>(child);
      if (child_block_flow) {
        child_block_flow->MarkPositionedObjectsForLayout();
        ClearTruncation(child_block_flow);
      }
    }
    child->LayoutIfNeeded();
    auto* child_block_flow = DynamicTo<LayoutBlockFlow>(child);
    if (child->StyleRef().Height().IsAuto() && child_block_flow) {
      max_line_count = std::max(max_line_count, LineCount(child_block_flow));
    }
  }

  // Get the number of lines and then alter all block flow children with auto
  // height to use the
  // specified height. We always try to leave room for at least one line.
  int num_visible_lines = StyleRef().LineClamp();
  DCHECK_GT(num_visible_lines, 0);

  if (num_visible_lines >= max_line_count)
    return;

  for (LayoutBox* child = FirstChildBox(); child;
       child = child->NextSiblingBox()) {
    auto* block_child = DynamicTo<LayoutBlockFlow>(child);
    if (child->IsOutOfFlowPositioned() ||
        !child->StyleRef().Height().IsAuto() || !block_child)
      continue;

    int line_count = blink::LineCount(block_child);
    if (line_count <= num_visible_lines)
      continue;

    int dummy_count = 0;
    LayoutUnit new_height(GetHeightForLineCount(block_child, num_visible_lines,
                                                true, dummy_count));
    if (new_height == child->Size().Height())
      continue;

    child->SetOverrideLogicalHeight(new_height);
    child->ForceLayout();

    // FIXME: For now don't support RTL.
    if (StyleRef().Direction() != TextDirection::kLtr)
      continue;

    // Get the last line
    RootInlineBox* last_line = LineAtIndex(block_child, line_count - 1);
    if (!last_line)
      continue;

    RootInlineBox* last_visible_line =
        LineAtIndex(block_child, num_visible_lines - 1);
    if (!last_visible_line)
      continue;

    DEFINE_STATIC_LOCAL(AtomicString, ellipsis_str,
                        (&kHorizontalEllipsisCharacter, 1));
    const Font& font = Style(num_visible_lines == 1)->GetFont();
    float total_width =
        font.Width(ConstructTextRun(font, &kHorizontalEllipsisCharacter, 1,
                                    StyleRef(), StyleRef().Direction()));

    // See if this width can be accommodated on the last visible line
    LineLayoutBlockFlow dest_block = last_visible_line->Block();
    LineLayoutBlockFlow src_block = last_line->Block();

    // FIXME: Directions of src/destBlock could be different from our direction
    // and from one another.
    if (!src_block.StyleRef().IsLeftToRightDirection())
      continue;

    bool left_to_right = dest_block.StyleRef().IsLeftToRightDirection();
    if (!left_to_right)
      continue;

    LayoutUnit block_right_edge = dest_block.LogicalRightOffsetForLine(
        last_visible_line->Y(), kDoNotIndentText);
    if (!last_visible_line->LineCanAccommodateEllipsis(
            left_to_right, block_right_edge,
            last_visible_line->X() + last_visible_line->LogicalWidth(),
            LayoutUnit(total_width)))
      continue;

    // Let the truncation code kick in.
    // FIXME: the text alignment should be recomputed after the width changes
    // due to truncation.
    LayoutUnit block_left_edge = dest_block.LogicalLeftOffsetForLine(
        last_visible_line->Y(), kDoNotIndentText);
    InlineBox* box_truncation_starts_at = nullptr;
    last_visible_line->PlaceEllipsis(ellipsis_str, left_to_right,
                                     block_left_edge, block_right_edge,
                                     LayoutUnit(total_width), LayoutUnit(),
                                     &box_truncation_starts_at, kForceEllipsis);
    dest_block.SetHasMarkupTruncation(true);
  }
}

}  // namespace blink
