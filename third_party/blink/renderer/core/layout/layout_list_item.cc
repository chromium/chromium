/**
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2003, 2004, 2005, 2006, 2010 Apple Inc. All rights reserved.
 * Copyright (C) 2006 Andrew Wellington (proton@wiretapped.net)
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

#include "third_party/blink/renderer/core/layout/layout_list_item.h"

#include "third_party/blink/renderer/core/dom/flat_tree_traversal.h"
#include "third_party/blink/renderer/core/html/html_li_element.h"
#include "third_party/blink/renderer/core/html/html_olist_element.h"
#include "third_party/blink/renderer/core/layout/layout_list_marker.h"
#include "third_party/blink/renderer/core/layout/layout_object_inlines.h"
#include "third_party/blink/renderer/core/layout/layout_outside_list_marker.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/layout/list_marker.h"
#include "third_party/blink/renderer/core/paint/list_item_painter.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

LayoutListItem::LayoutListItem(Element* element)
    : LayoutBlockFlow(element),
      need_block_direction_align_(false) {
  SetInline(false);

  SetConsumesSubtreeChangeNotification();
  RegisterSubtreeChangeListenerOnDescendants(true);
  View()->AddLayoutListItem();
}

void LayoutListItem::WillBeDestroyed() {
  NOT_DESTROYED();
  if (View())
    View()->RemoveLayoutListItem();
  LayoutBlockFlow::WillBeDestroyed();
}

void LayoutListItem::StyleDidChange(StyleDifference diff,
                                    const ComputedStyle* old_style) {
  NOT_DESTROYED();
  LayoutBlockFlow::StyleDidChange(diff, old_style);

  StyleImage* current_image = StyleRef().ListStyleImage();
  if (old_style && (StyleRef().ListStyleType() ||
                    (current_image && !current_image->ErrorOccurred()))) {
    // The old_style check makes sure we don't enter here when attaching the
    // LayoutObject.
    DCHECK(GetDocument().InStyleRecalc());
    DCHECK(!GetDocument().GetStyleEngine().InRebuildLayoutTree());
    // We may enter here when propagating writing-mode and direction from body
    // to the root element after layout tree rebuild. Skip NotifyOfSubtreeChange
    // for that case.
    if (GetDocument().documentElement() != GetNode() ||
        GetDocument().GetStyleEngine().NeedsStyleRecalc()) {
      NotifyOfSubtreeChange();
    }
  }

  LayoutObject* marker = Marker();
  if (!marker)
    return;

  auto* legacy_marker = DynamicTo<LayoutListMarker>(marker);
  ListMarker* list_marker = legacy_marker ? nullptr : ListMarker::Get(marker);
  DCHECK(legacy_marker || list_marker);

  if (legacy_marker)
    legacy_marker->UpdateMarkerImageIfNeeded(current_image);
  else
    list_marker->UpdateMarkerContentIfNeeded(*marker);

  if (old_style) {
    const ListStyleTypeData* old_list_style_type = old_style->ListStyleType();
    const ListStyleTypeData* new_list_style_type = StyleRef().ListStyleType();
    if (old_list_style_type != new_list_style_type &&
        (!old_list_style_type || !new_list_style_type ||
         *old_list_style_type != *new_list_style_type)) {
      if (legacy_marker)
        legacy_marker->ListStyleTypeChanged();
      else
        list_marker->ListStyleTypeChanged(*marker);
    }
  }
}

void LayoutListItem::UpdateCounterStyle() {
  NOT_DESTROYED();

  if (!StyleRef().ListStyleType() ||
      StyleRef().ListStyleType()->IsCounterStyleReferenceValid(GetDocument())) {
    return;
  }

  LayoutObject* marker = Marker();
  if (!marker)
    return;

  if (auto* legacy_marker = DynamicTo<LayoutListMarker>(marker)) {
    legacy_marker->CounterStyleChanged();
    return;
  }

  ListMarker::Get(marker)->CounterStyleChanged(*marker);
}

void LayoutListItem::InsertedIntoTree() {
  NOT_DESTROYED();
  LayoutBlockFlow::InsertedIntoTree();

  ListItemOrdinal::ItemInsertedOrRemoved(this);
}

void LayoutListItem::WillBeRemovedFromTree() {
  NOT_DESTROYED();
  LayoutBlockFlow::WillBeRemovedFromTree();

  ListItemOrdinal::ItemInsertedOrRemoved(this);
}

void LayoutListItem::SubtreeDidChange() {
  NOT_DESTROYED();
  LayoutObject* marker = Marker();
  if (!marker)
    return;

  if (auto* legacy_marker = DynamicTo<LayoutListMarker>(marker))
    legacy_marker->UpdateMarkerImageIfNeeded(StyleRef().ListStyleImage());
  else if (ListMarker* list_marker = ListMarker::Get(marker))
    list_marker->UpdateMarkerContentIfNeeded(*marker);
  else
    NOTREACHED();

  if (!UpdateMarkerLocation())
    return;

  // If the marker is inside we need to redo the preferred width calculations
  // as the size of the item now includes the size of the list marker.
  if (marker->IsInsideListMarker())
    SetIntrinsicLogicalWidthsDirty();
}

int LayoutListItem::Value() const {
  NOT_DESTROYED();
  DCHECK(GetNode());
  return ordinal_.Value(*GetNode());
}

bool LayoutListItem::IsEmpty() const {
  NOT_DESTROYED();
  return LastChild() == Marker();
}

void LayoutListItem::UpdateMarkerTextIfNeeded() {
  NOT_DESTROYED();
  LayoutObject* marker = Marker();
  if (ListMarker* list_marker = ListMarker::Get(marker))
    list_marker->UpdateMarkerTextIfNeeded(*marker);
}

namespace {

LayoutObject* GetParentOfFirstLineBox(LayoutBlockFlow* curr) {
  LayoutObject* first_child = curr->FirstChild();
  if (!first_child)
    return nullptr;

  bool in_quirks_mode = curr->GetDocument().InQuirksMode();
  for (LayoutObject* curr_child = first_child; curr_child;
       curr_child = curr_child->NextSibling()) {
    if (curr_child->IsOutsideListMarker())
      continue;

    // Moving a legacy marker inside an NG object is not supported.
    if (curr_child->IsLayoutNGObject())
      continue;

    if (curr_child->IsInline() &&
        (!curr_child->IsLayoutInline() ||
         curr->GeneratesLineBoxesForInlineChild(curr_child)))
      return curr;

    if (curr_child->IsFloating() || curr_child->IsOutOfFlowPositioned())
      continue;

    if (curr->IsScrollContainer())
      return curr;

    auto* child_block_flow = DynamicTo<LayoutBlockFlow>(curr_child);
    if (!child_block_flow ||
        (curr_child->IsBox() && To<LayoutBox>(curr_child)->IsWritingModeRoot()))
      return curr_child;

    if (curr->IsListItem() && in_quirks_mode && curr_child->GetNode() &&
        (IsA<HTMLUListElement>(*curr_child->GetNode()) ||
         IsA<HTMLOListElement>(*curr_child->GetNode())))
      break;

    LayoutObject* line_box = GetParentOfFirstLineBox(child_block_flow);
    if (line_box)
      return line_box;
  }

  return nullptr;
}

LayoutObject* FirstNonMarkerChild(LayoutObject* parent) {
  LayoutObject* result = parent->SlowFirstChild();
  while (result && result->IsListMarker())
    result = result->NextSibling();
  return result;
}

void ForceLogicalHeight(LayoutObject& layout_object, const Length& height) {
  DCHECK(layout_object.IsAnonymous());
  if (layout_object.StyleRef().LogicalHeight() == height)
    return;

  ComputedStyleBuilder builder(layout_object.StyleRef());
  if (layout_object.IsHorizontalWritingMode()) {
    builder.SetHeight(height);
  } else {
    builder.SetWidth(height);
  }
  layout_object.SetStyle(builder.TakeStyle(),
                         LayoutObject::ApplyStyleChanges::kNo);
}

}  // namespace

// 1. Place marker as a child of <li>. Make sure don't share parent with empty
// inline elements which don't generate InlineBox.
// 2. Manage the logicalHeight of marker_container(marker's anonymous parent):
// If marker is the only child of marker_container, set LogicalHeight of
// marker_container to 0px; else restore it to LogicalHeight of <li>.
bool LayoutListItem::PrepareForBlockDirectionAlign(
    const LayoutObject* line_box_parent) {
  NOT_DESTROYED();
  LayoutObject* marker = Marker();
  LayoutObject* marker_parent = marker->Parent();
  bool is_inside = marker->IsInsideListMarker();
  // Deal with the situation of layout tree changed.
  if (marker_parent && marker_parent->IsAnonymous()) {
    bool marker_parent_has_lines =
        line_box_parent && line_box_parent->IsDescendantOf(marker_parent);
    // When list-position-style change from outside to inside, we need to
    // restore LogicalHeight to auto. So add is_inside.
    if (is_inside || marker_parent_has_lines) {
      // Set marker_container's LogicalHeight to auto.
      if (marker_parent->StyleRef().LogicalHeight().IsZero())
        ForceLogicalHeight(*marker_parent, Length());

      // If marker_parent_has_lines and the marker is outside, we need to move
      // the marker into another parent with 'height: 0' to avoid generating a
      // new empty line in cases like <li><span><div>text<div><span></li>
      // If the marker is inside and there are inline contents, we want them to
      // share the same block container to avoid a line break between them.
      if (is_inside != marker_parent_has_lines) {
        marker->Remove();
        marker_parent = nullptr;
      }
    } else if (line_box_parent) {
      ForceLogicalHeight(*marker_parent, Length::Fixed(0));
    }
  }

  // Create marker_container, set its height to 0px, and add it to li.
  if (!marker_parent) {
    LayoutObject* before_child = FirstNonMarkerChild(this);
    if (!is_inside && before_child && !before_child->IsInline()) {
      // Create marker_container and set its LogicalHeight to 0px.
      LayoutBlock* marker_container = CreateAnonymousBlock();
      if (line_box_parent)
        ForceLogicalHeight(*marker_container, Length::Fixed(0));
      marker_container->AddChild(marker, FirstNonMarkerChild(marker_container));
      AddChild(marker_container, before_child);
    } else {
      AddChild(marker, before_child);
    }

    if (marker->IsListMarkerForNormalContent())
      To<LayoutListMarker>(marker)->UpdateMargins();
    else if (marker->IsOutsideListMarkerForCustomContent())
      To<LayoutOutsideListMarker>(marker)->UpdateMargins();
    return true;
  }
  return false;
}

static bool IsFirstLeafChild(LayoutObject* container, LayoutObject* child) {
  while (child && child != container) {
    LayoutObject* parent = child->Parent();
    if (parent && child != parent->SlowFirstChild()) {
      return false;
    }
    child = parent;
  }
  return true;
}

bool LayoutListItem::UpdateMarkerLocation() {
  NOT_DESTROYED();
  DCHECK(Marker());

  LayoutObject* marker = Marker();
  LayoutObject* marker_parent = marker->Parent();
  LayoutObject* line_box_parent = nullptr;

  // Make sure a marker originated by a ::before or ::after precedes the
  // generated contents.
  if (IsPseudoElement()) {
    LayoutObject* first_child = marker_parent->SlowFirstChild();
    if (marker != first_child) {
      marker->Remove();
      AddChild(marker, first_child);
    }
  }

  if (marker->IsOutsideListMarker())
    line_box_parent = GetParentOfFirstLineBox(this);
  if (line_box_parent &&
      (line_box_parent->IsScrollContainer() ||
       !line_box_parent->IsLayoutBlockFlow() ||
       (line_box_parent->IsBox() &&
        To<LayoutBox>(line_box_parent)->IsWritingModeRoot())))
    need_block_direction_align_ = true;
  if (need_block_direction_align_)
    return PrepareForBlockDirectionAlign(line_box_parent);

  // list-style-position:inside makes the ::marker pseudo an ordinary
  // position:static element that should be attached to LayoutListItem block.
  // list-style-position:outside marker can't find its line_box_parent,
  // it should be attached to LayoutListItem block too.
  if (!line_box_parent) {
    // If the marker is currently contained inside an anonymous box, then we
    // are the only item in that anonymous box (since no line box parent was
    // found). It's ok to just leave the marker where it is in this case.
    // Also ok to leave it in a flow thread.
    if (marker_parent && (marker_parent->IsAnonymousBlock() ||
                          marker_parent->IsLayoutFlowThread())) {
      line_box_parent = marker_parent;
      // We could use marker_parent as line_box_parent only if marker is the
      // first leaf child of list item.
      if (!IsFirstLeafChild(this, marker_parent))
        line_box_parent = this;
    } else {
      line_box_parent = this;
    }
  }

  if (!marker_parent || marker_parent != line_box_parent) {
    marker->Remove();
    line_box_parent->AddChild(marker, FirstNonMarkerChild(line_box_parent));
    // TODO(rhogan): line_box_parent and marker_parent may be deleted by
    // AddChild, so they are not safe to reference here. Once we have a safe way
    // of referencing them delete marker_parent if it is an empty anonymous
    // block.
    if (marker->IsListMarkerForNormalContent())
      To<LayoutListMarker>(marker)->UpdateMargins();
    else if (marker->IsOutsideListMarkerForCustomContent())
      To<LayoutOutsideListMarker>(marker)->UpdateMargins();
    return true;
  }

  return false;
}

void LayoutListItem::RecalcVisualOverflow() {
  NOT_DESTROYED();
  RecalcChildVisualOverflow();
  RecalcSelfVisualOverflow();
}

void LayoutListItem::ComputeVisualOverflow(bool recompute_floats) {
  NOT_DESTROYED();
  LayoutRect previous_visual_overflow_rect = VisualOverflowRect();
  ClearVisualOverflow();

  AddVisualOverflowFromChildren();
  AddVisualEffectOverflow();

  if (recompute_floats || CreatesNewFormattingContext() ||
      HasSelfPaintingLayer())
    AddVisualOverflowFromFloats();

  if (VisualOverflowRect() != previous_visual_overflow_rect) {
    InvalidateIntersectionObserverCachedRects();
    SetShouldCheckForPaintInvalidation();
    GetFrameView()->SetIntersectionObservationState(LocalFrameView::kDesired);
  }
}

void LayoutListItem::AddLayoutOverflowFromChildren() {
  NOT_DESTROYED();
  LayoutBlockFlow::AddLayoutOverflowFromChildren();
  UpdateOverflow();
}

// Align marker_inline_box in block direction according to line_box_root's
// baseline.
void LayoutListItem::AlignMarkerInBlockDirection() {
  NOT_DESTROYED();
  // Specify wether need to restore to the original baseline which is the
  // baseline of marker parent. Because we might adjust the position at the last
  // layout pass. So if there's no line box in line_box_parent make sure it
  // back to its original position.
  bool back_to_original_baseline = false;
  DCHECK(Marker()->IsOutsideListMarker());
  auto* marker = To<LayoutBox>(Marker());
  LayoutObject* line_box_parent = GetParentOfFirstLineBox(this);
  LayoutBox* line_box_parent_block = nullptr;
  if (!line_box_parent || !line_box_parent->IsBox()) {
    back_to_original_baseline = true;
  } else {
    line_box_parent_block = To<LayoutBox>(line_box_parent);
    // Don't align marker if line_box_parent has a different writing-mode.
    // Just let marker positioned at the left-top of line_box_parent.
    if (line_box_parent_block->IsWritingModeRoot())
      back_to_original_baseline = true;
  }

  InlineBox* marker_inline_box = marker->InlineBoxWrapper();
  RootInlineBox& marker_root = marker_inline_box->Root();
  auto* line_box_parent_block_flow =
      DynamicTo<LayoutBlockFlow>(line_box_parent_block);
  if (line_box_parent_block && line_box_parent_block_flow) {
    // If marker and line_box_parent_block share a same RootInlineBox, no need
    // to align marker.
    if (line_box_parent_block_flow->FirstRootBox() == &marker_root)
      return;
  }

  LayoutUnit offset;
  if (!back_to_original_baseline)
    offset = line_box_parent_block->FirstLineBoxBaseline();

  if (back_to_original_baseline || offset == -1) {
    line_box_parent_block = marker->ContainingBlock();
    offset = line_box_parent_block->FirstLineBoxBaseline();
  }

  if (offset != -1) {
    for (LayoutBox* o = line_box_parent_block; o != this; o = o->ParentBox())
      offset += o->LogicalTop();

    // Compute marker_inline_box's baseline.
    // We shouldn't use FirstLineBoxBaseline here. FirstLineBoxBaseline is
    // the baseline of root. We should compute marker_inline_box's baseline
    // instead. BaselinePosition is workable when marker is an image.
    // However, when marker is text, BaselinePosition contains lineheight
    // information. So use marker_font_metrics.Ascent when marker is text.
    bool is_image = marker->IsListMarkerForNormalContent()
                        ? To<LayoutListMarker>(marker)->IsImage()
                        : To<LayoutOutsideListMarker>(marker)->IsMarkerImage();
    if (is_image) {
      offset -= marker_inline_box->BaselinePosition(marker_root.BaselineType());
    } else {
      const SimpleFontData* marker_font_data =
          marker->Style(true)->GetFont().PrimaryFont();
      if (marker_font_data) {
        const FontMetrics& marker_font_metrics =
            marker_font_data->GetFontMetrics();
        offset -= marker_font_metrics.Ascent(marker_root.BaselineType());
      }
    }
    offset -= marker_inline_box->LogicalTop();

    for (LayoutBox* o = marker->ParentBox(); o != this; o = o->ParentBox()) {
      offset -= o->LogicalTop();
    }

    if (offset)
      marker_inline_box->MoveInBlockDirection(offset);
  }
}

void LayoutListItem::UpdateOverflow() {
  NOT_DESTROYED();
  LayoutObject* marker_object = Marker();
  if (!marker_object || !marker_object->Parent() ||
      !marker_object->Parent()->IsBox() || marker_object->IsInsideListMarker())
    return;

  DCHECK(marker_object->IsOutsideListMarker());
  auto* marker = To<LayoutBox>(marker_object);
  if (!marker->InlineBoxWrapper())
    return;

  if (need_block_direction_align_)
    AlignMarkerInBlockDirection();

  LayoutUnit marker_old_logical_left = marker->LogicalLeft();
  LayoutUnit block_offset;
  LayoutUnit line_offset;
  for (LayoutBox* o = marker->ParentBox(); o != this; o = o->ParentBox()) {
    block_offset += o->LogicalTop();
    line_offset += o->LogicalLeft();
  }

  bool adjust_overflow = false;
  LayoutUnit marker_logical_left;
  InlineBox* marker_inline_box = marker->InlineBoxWrapper();
  RootInlineBox& root = marker_inline_box->Root();
  bool hit_self_painting_layer = false;

  LayoutUnit line_top = root.LineTop();
  LayoutUnit line_bottom = root.LineBottom();

  // We figured out the inline position of the marker before laying out the
  // line so that floats later in the line don't interfere with it. However
  // if the line has shifted down then that position will be too far out.
  // So we always take the lowest value of (1) the position of the marker
  // if we calculate it now and (2) the inline position we calculated before
  // laying out the line.
  // TODO(jchaffraix): Propagating the overflow to the line boxes seems
  // pretty wrong (https://crbug.com/554160).
  // FIXME: Need to account for relative positioning in the layout overflow.
  LayoutUnit marker_line_offset =
      marker->IsListMarkerForNormalContent()
          ? To<LayoutListMarker>(marker)->ListItemInlineStartOffset()
          : To<LayoutOutsideListMarker>(marker)->ListItemInlineStartOffset();
  if (StyleRef().IsLeftToRightDirection()) {
    marker_line_offset =
        std::min(marker_line_offset,
                 LogicalLeftOffsetForLine(marker->LogicalTop(),
                                          kDoNotIndentText, LayoutUnit()));
    marker_logical_left = marker_line_offset - line_offset - PaddingStart() -
                          BorderStart() + marker->MarginStart();

    marker_inline_box->MoveInInlineDirection(marker_logical_left -
                                             marker_old_logical_left);

    for (InlineFlowBox* box = marker_inline_box->Parent(); box;
         box = box->Parent()) {
      box->AddReplacedChildrenVisualOverflow(line_top, line_bottom);
      LayoutRect new_logical_visual_overflow_rect =
          box->LogicalVisualOverflowRect(line_top, line_bottom);
      if (marker_logical_left < new_logical_visual_overflow_rect.X() &&
          !hit_self_painting_layer) {
        new_logical_visual_overflow_rect.SetWidth(
            new_logical_visual_overflow_rect.MaxX() - marker_logical_left);
        new_logical_visual_overflow_rect.SetX(marker_logical_left);
        if (box == root)
          adjust_overflow = true;
      }
      box->OverrideVisualOverflowFromLogicalRect(
          new_logical_visual_overflow_rect, line_top, line_bottom);

      if (box->BoxModelObject().HasSelfPaintingLayer())
        hit_self_painting_layer = true;
      LayoutRect new_logical_layout_overflow_rect =
          box->LogicalLayoutOverflowRect(line_top, line_bottom);
      if (marker_logical_left < new_logical_layout_overflow_rect.X()) {
        new_logical_layout_overflow_rect.SetWidth(
            new_logical_layout_overflow_rect.MaxX() - marker_logical_left);
        new_logical_layout_overflow_rect.SetX(marker_logical_left);
        if (box == root)
          adjust_overflow = true;
      }
      box->OverrideLayoutOverflowFromLogicalRect(
          new_logical_layout_overflow_rect, line_top, line_bottom);
    }
  } else {
    marker_line_offset =
        std::max(marker_line_offset,
                 LogicalRightOffsetForLine(marker->LogicalTop(),
                                           kDoNotIndentText, LayoutUnit()));
    marker_logical_left = marker_line_offset - line_offset + PaddingStart() +
                          BorderStart() + marker->MarginEnd();

    marker_inline_box->MoveInInlineDirection(marker_logical_left -
                                             marker_old_logical_left);

    for (InlineFlowBox* box = marker_inline_box->Parent(); box;
         box = box->Parent()) {
      box->AddReplacedChildrenVisualOverflow(line_top, line_bottom);
      LayoutRect new_logical_visual_overflow_rect =
          box->LogicalVisualOverflowRect(line_top, line_bottom);
      if (marker_logical_left + marker->LogicalWidth() >
              new_logical_visual_overflow_rect.MaxX() &&
          !hit_self_painting_layer) {
        new_logical_visual_overflow_rect.SetWidth(
            marker_logical_left + marker->LogicalWidth() -
            new_logical_visual_overflow_rect.X());
        if (box == root)
          adjust_overflow = true;
      }
      box->OverrideVisualOverflowFromLogicalRect(
          new_logical_visual_overflow_rect, line_top, line_bottom);

      if (box->BoxModelObject().HasSelfPaintingLayer())
        hit_self_painting_layer = true;
      LayoutRect new_logical_layout_overflow_rect =
          box->LogicalLayoutOverflowRect(line_top, line_bottom);
      if (marker_logical_left + marker->LogicalWidth() >
          new_logical_layout_overflow_rect.MaxX()) {
        new_logical_layout_overflow_rect.SetWidth(
            marker_logical_left + marker->LogicalWidth() -
            new_logical_layout_overflow_rect.X());
        if (box == root)
          adjust_overflow = true;
      }
      box->OverrideLayoutOverflowFromLogicalRect(
          new_logical_layout_overflow_rect, line_top, line_bottom);
    }
  }

  if (adjust_overflow) {
    // AlignMarkerInBlockDirection and pagination_strut might move root or
    // marker_inline_box in block direction. We should add marker_inline_box
    // top when propagate overflow.
    LayoutRect marker_rect(
        LayoutPoint(marker_logical_left + line_offset,
                    block_offset + marker_inline_box->LogicalTop()),
        marker->Size());
    if (!StyleRef().IsHorizontalWritingMode())
      marker_rect = marker_rect.TransposedRect();
    LayoutBox* object = marker;

    bool found_self_painting_layer = false;
    do {
      object = object->ParentBox();
      auto* layout_block_object = DynamicTo<LayoutBlock>(object);
      if (layout_block_object) {
        if (!found_self_painting_layer)
          layout_block_object->AddContentsVisualOverflow(marker_rect);
        layout_block_object->AddLayoutOverflow(marker_rect);
      }

      if (object->ShouldClipOverflowAlongBothAxis())
        break;

      if (object->HasSelfPaintingLayer())
        found_self_painting_layer = true;

      marker_rect.MoveBy(-object->Location());
    } while (object != this);
  }
}

void LayoutListItem::Paint(const PaintInfo& paint_info) const {
  NOT_DESTROYED();
  ListItemPainter(*this).Paint(paint_info);
}

void LayoutListItem::OrdinalValueChanged() {
  NOT_DESTROYED();
  LayoutObject* marker = Marker();
  if (ListMarker* list_marker = ListMarker::Get(marker)) {
    list_marker->OrdinalValueChanged(*marker);
  } else if (marker) {
    DCHECK(marker->IsListMarkerForNormalContent());
    marker->SetNeedsLayoutAndIntrinsicWidthsRecalcAndFullPaintInvalidation(
        layout_invalidation_reason::kListValueChange);
  }
}

void LayoutListItem::UpdateLayout() {
  NOT_DESTROYED();
  LayoutObject* marker = Marker();
  if (ListMarker* list_marker = ListMarker::Get(marker))
    list_marker->UpdateMarkerTextIfNeeded(*marker);
  LayoutBlockFlow::UpdateLayout();
}

}  // namespace blink
