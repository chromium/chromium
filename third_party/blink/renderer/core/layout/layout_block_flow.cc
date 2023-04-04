/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/layout/layout_block_flow.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "third_party/blink/renderer/core/dom/first_letter_pseudo_element.h"
#include "third_party/blink/renderer/core/editing/editing_behavior.h"
#include "third_party/blink/renderer/core/editing/editor.h"
#include "third_party/blink/renderer/core/editing/position_with_affinity.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/html/shadow/shadow_element_names.h"
#include "third_party/blink/renderer/core/layout/hit_test_location.h"
#include "third_party/blink/renderer/core/layout/layout_flow_thread.h"
#include "third_party/blink/renderer/core/layout/layout_inline.h"
#include "third_party/blink/renderer/core/layout/layout_multi_column_flow_thread.h"
#include "third_party/blink/renderer/core/layout/layout_multi_column_spanner_placeholder.h"
#include "third_party/blink/renderer/core/layout/layout_object_factory.h"
#include "third_party/blink/renderer/core/layout/layout_object_inlines.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/layout/line/glyph_overflow.h"
#include "third_party/blink/renderer/core/layout/line/inline_iterator.h"
#include "third_party/blink/renderer/core/layout/line/inline_text_box.h"
#include "third_party/blink/renderer/core/layout/line/line_width.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_cursor.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_offset_mapping.h"
#include "third_party/blink/renderer/core/layout/ng/layout_ng_block_flow.h"
#include "third_party/blink/renderer/core/layout/ng/legacy_layout_tree_walking.h"
#include "third_party/blink/renderer/core/layout/ng/ng_absolute_utils.h"
#include "third_party/blink/renderer/core/layout/ng/ng_box_fragment.h"
#include "third_party/blink/renderer/core/layout/ng/ng_fragmentation_utils.h"
#include "third_party/blink/renderer/core/layout/ng/ng_layout_result.h"
#include "third_party/blink/renderer/core/layout/ng/ng_physical_box_fragment.h"
#include "third_party/blink/renderer/core/layout/ng/ng_unpositioned_float.h"
#include "third_party/blink/renderer/core/layout/shapes/shape_outside_info.h"
#include "third_party/blink/renderer/core/layout/text_autosizer.h"
#include "third_party/blink/renderer/core/paint/block_flow_paint_invalidator.h"
#include "third_party/blink/renderer/core/paint/ng/ng_inline_paint_context.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"
#include "third_party/blink/renderer/platform/heap/collection_support/clear_collection_scope.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/size_assertions.h"

namespace blink {

struct SameSizeAsLayoutBlockFlow : public LayoutBlock {
  LineBoxList line_boxes;
  Member<void*> member;
};

ASSERT_SIZE(LayoutBlockFlow, SameSizeAsLayoutBlockFlow);

LayoutBlockFlow::LayoutBlockFlow(ContainerNode* node) : LayoutBlock(node) {
  SetChildrenInline(true);
}

#if DCHECK_IS_ON()
LayoutBlockFlow::~LayoutBlockFlow() {
  line_boxes_.AssertIsEmpty();
}
#else
LayoutBlockFlow::~LayoutBlockFlow() = default;
#endif

LayoutBlockFlow* LayoutBlockFlow::CreateAnonymous(
    Document* document,
    scoped_refptr<const ComputedStyle> style) {
  LayoutBlockFlow* layout_block_flow =
      LayoutObjectFactory::CreateBlockFlow(*document, *style);
  layout_block_flow->SetDocumentForAnonymous(document);
  layout_block_flow->SetStyle(style);
  return layout_block_flow;
}

bool LayoutBlockFlow::IsInitialLetterBox() const {
  return IsA<FirstLetterPseudoElement>(GetNode()) &&
         !StyleRef().InitialLetter().IsNormal();
}

DISABLE_CFI_PERF
void LayoutBlockFlow::UpdateBlockLayout(bool relayout_children) {
  NOT_DESTROYED();

  // TODO(1229581): Remove this logic.
  NOTREACHED_NORETURN();

  DCHECK(NeedsLayout());
  DCHECK(IsInlineBlockOrInlineTable() || !IsInline());

  ClearOffsetMappingIfNeeded();

  SubtreeLayoutScope layout_scope(*this);

  bool logical_width_changed = UpdateLogicalWidthAndColumnWidth();
  relayout_children |= logical_width_changed;

  TextAutosizer::LayoutScope text_autosizer_layout_scope(this, &layout_scope);

  bool intrinsic_logical_widths_were_dirty = IntrinsicLogicalWidthsDirty();

  do {
    LayoutChildren(relayout_children, layout_scope);

    if (!intrinsic_logical_widths_were_dirty && IntrinsicLogicalWidthsDirty()) {
      // The only thing that should dirty preferred widths at this point is the
      // addition of overflow:auto scrollbars in a descendant. To avoid a
      // potential infinite loop, run layout again with auto scrollbars frozen
      // in their current state.
      PaintLayerScrollableArea::FreezeScrollbarsScope freeze_scrollbars;
      relayout_children |= UpdateLogicalWidthAndColumnWidth();
      LayoutChildren(relayout_children, layout_scope);
    }

    break;
  } while (true);

  // Remember the automatic logical height we got from laying out the children.
  LayoutUnit unconstrained_client_after_edge = ClientLogicalBottom();

  // Adjust logical height to satisfy whatever computed style requires.
  UpdateLogicalHeight();

  // Add overflow from children.
  ComputeLayoutOverflow(unconstrained_client_after_edge, false);

  descendants_with_floats_marked_for_layout_ = false;

  UpdateAfterLayout();

  ClearNeedsLayout();
}

DISABLE_CFI_PERF
void LayoutBlockFlow::ResetLayout() {
  NOT_DESTROYED();
  DCHECK(!IsLayoutNGObject()) << this;
  if (!FirstChild() && !IsAnonymousBlock())
    SetChildrenInline(true);

  // Text truncation kicks in if overflow isn't visible and text-overflow isn't
  // 'clip'. If this is an anonymous block, we have to examine the parent.
  // FIXME: CSS3 says that descendants that are clipped must also know how to
  // truncate. This is insanely difficult to figure out in general (especially
  // in the middle of doing layout), so we only handle the simple case of an
  // anonymous block truncating when its parent is clipped.
  // Walk all the lines and delete our ellipsis line boxes if they exist.
  if (ChildrenInline() && ShouldTruncateOverflowingText())
    DeleteEllipsisLineBoxes();
}

DISABLE_CFI_PERF
void LayoutBlockFlow::LayoutChildren(bool relayout_children,
                                     SubtreeLayoutScope& layout_scope) {
  NOT_DESTROYED();
  ResetLayout();

  if (ChildLayoutBlockedByDisplayLock())
    return;

  LayoutUnit before_edge = BorderBefore() + PaddingBefore();
  LayoutUnit after_edge = BorderAfter() + PaddingAfter();

  NGBoxStrut scrollbars = ComputeLogicalScrollbars();
  before_edge += scrollbars.block_start;
  after_edge += scrollbars.block_end;

  SetLogicalHeight(before_edge);

  if (ChildrenInline())
    LayoutInlineChildren(relayout_children, after_edge);
  else
    LayoutBlockChildren(relayout_children, layout_scope, before_edge,
                        after_edge);

  NotifyDisplayLockDidLayoutChildren();
}

DISABLE_CFI_PERF
void LayoutBlockFlow::DetermineLogicalLeftPositionForChild(LayoutBox& child) {
  NOT_DESTROYED();
  LayoutUnit start_position = BorderStart() + PaddingStart();
  LayoutUnit total_available_logical_width =
      BorderAndPaddingLogicalWidth() + AvailableLogicalWidth();

  if (ShouldPlaceBlockDirectionScrollbarOnLogicalLeft())
    start_position -= LogicalLeftScrollbarWidth();
  else
    start_position += LogicalLeftScrollbarWidth();

  LayoutUnit child_margin_start = MarginStartForChild(child);
  LayoutUnit new_position = start_position + child_margin_start;

  SetLogicalLeftForChild(child, StyleRef().IsLeftToRightDirection()
                                    ? new_position
                                    : total_available_logical_width -
                                          new_position -
                                          LogicalWidthForChild(child));
}

void LayoutBlockFlow::SetLogicalLeftForChild(LayoutBox& child,
                                             LayoutUnit logical_left) {
  NOT_DESTROYED();
  LayoutPoint new_location(child.Location());
  if (IsHorizontalWritingMode()) {
    new_location.SetX(logical_left);
  } else {
    new_location.SetY(logical_left);
  }
  child.SetLocationAndUpdateOverflowControlsIfNeeded(new_location);
}

void LayoutBlockFlow::SetLogicalTopForChild(LayoutBox& child,
                                            LayoutUnit logical_top) {
  NOT_DESTROYED();
  if (IsHorizontalWritingMode()) {
    child.SetY(logical_top);
  } else {
    child.SetX(logical_top);
  }
}

bool LayoutBlockFlow::PositionAndLayoutOnceIfNeeded(
    LayoutBox& child,
    LayoutUnit new_logical_top) {
  NOT_DESTROYED();
  SetLogicalTopForChild(child, new_logical_top);

  SubtreeLayoutScope layout_scope(child);
  auto child_needs_layout = [&child] {
    if (!child.NeedsLayout())
      return false;
    return child.SelfNeedsLayout() || !child.ChildLayoutBlockedByDisplayLock();
  };

  bool needed_layout = child_needs_layout();
  if (needed_layout)
    child.UpdateLayout();
  return needed_layout;
}

void LayoutBlockFlow::LayoutBlockChild(LayoutBox& child) {
  NOT_DESTROYED();

  // The child is a normal flow object. Compute the margins we will use for
  // collapsing now.
  child.ComputeAndSetBlockDirectionMargins(this);

  // Try to guess our correct logical top position. In most cases this guess
  // will be correct. Only if we're wrong (when we compute the real logical top
  // position) will we have to potentially relayout.
  LayoutUnit logical_top_estimate = LogicalHeight();

  // Use the estimated block position and lay out the child if needed. After
  // child layout, when we have enough information to perform proper margin
  // collapsing, we may have to reposition and lay out again if the estimate was
  // wrong.
  PositionAndLayoutOnceIfNeeded(child, logical_top_estimate);

  // Now determine the correct ypos based off examination of collapsing margin
  // values.
  LayoutUnit new_logical_top = LogicalHeight();

  // Clearance or margin collapsing may have given us a new logical top, in
  // which case we may have to reposition and possibly relayout as well.  If we
  // determined during child layout that we need to insert a break to honor
  // widows, we also need to relayout.
  if (new_logical_top != logical_top_estimate || child.NeedsLayout()) {
    PositionAndLayoutOnceIfNeeded(child, new_logical_top);
  }

  // Now place the child in the correct left position
  DetermineLogicalLeftPositionForChild(child);

  // Update our height now that the child has been placed in the correct
  // position.
  SetLogicalHeight(LogicalHeight() + LogicalHeightForChild(child));
}

void LayoutBlockFlow::LayoutBlockChildren(bool relayout_children,
                                          SubtreeLayoutScope& layout_scope,
                                          LayoutUnit before_edge,
                                          LayoutUnit after_edge) {
  NOT_DESTROYED();

  for (auto* child = FirstChild(); child; child = child->NextSibling()) {
    child->SetShouldCheckForPaintInvalidation();

    LayoutBox* box = To<LayoutBox>(child);
    UpdateBlockChildDirtyBitsBeforeLayout(relayout_children, *box);

    if (box->IsOutOfFlowPositioned()) {
      box->ContainingBlock()->InsertPositionedObject(box);
      AdjustPositionedBlock(*box);
      continue;
    }
    if (box->IsFloating()) {
      NOTREACHED();
      continue;
    }

    // Lay out the child.
    LayoutBlockChild(*box);
  }
}

void LayoutBlockFlow::AdjustPositionedBlock(LayoutBox& child) {
  NOT_DESTROYED();
  LayoutUnit logical_top = LogicalHeight();

  UpdateStaticInlinePositionForChild(child, logical_top);

  PaintLayer* child_layer = child.Layer();
  if (child_layer->StaticBlockPosition() != logical_top)
    child_layer->SetStaticBlockPosition(logical_top);
}

void LayoutBlockFlow::AddVisualOverflowFromFloats(
    const NGPhysicalFragment& fragment) {
  NOT_DESTROYED();
  DCHECK(!NeedsLayout());
  DCHECK(!ChildPrePaintBlockedByDisplayLock());
  DCHECK(fragment.HasFloatingDescendantsForPaint());

  for (const NGLink& child : fragment.PostLayoutChildren()) {
    if (child->HasSelfPaintingLayer())
      continue;

    if (child->IsFloating()) {
      AddVisualOverflowFromChild(To<LayoutBox>(*child->GetLayoutObject()));
      continue;
    }

    if (const NGPhysicalFragment* child_container = child.get()) {
      if (child_container->HasFloatingDescendantsForPaint() &&
          !child_container->IsFormattingContextRoot())
        AddVisualOverflowFromFloats(*child_container);
    }
  }
}

void LayoutBlockFlow::ComputeVisualOverflow(bool recompute_floats) {
  NOT_DESTROYED();
  DCHECK(!SelfNeedsLayout());

  LayoutRect previous_visual_overflow_rect = VisualOverflowRectAllowingUnset();
  ClearVisualOverflow();
  AddVisualOverflowFromChildren();
  AddVisualEffectOverflow();

  if (VisualOverflowRect() != previous_visual_overflow_rect) {
    InvalidateIntersectionObserverCachedRects();
    SetShouldCheckForPaintInvalidation();
    GetFrameView()->SetIntersectionObservationState(LocalFrameView::kDesired);
  }
}

RootInlineBox* LayoutBlockFlow::CreateAndAppendRootInlineBox() {
  NOT_DESTROYED();
  RootInlineBox* root_box = CreateRootInlineBox();
  line_boxes_.AppendLineBox(root_box);

  return root_box;
}

// Note: When this function is called from |LayoutInline::SplitFlow()|, some
// fragments point to destroyed |LayoutObject|.
void LayoutBlockFlow::DeleteLineBoxTree() {
  NOT_DESTROYED();

  line_boxes_.DeleteLineBoxTree();
}

int LayoutBlockFlow::LineCount(
    const RootInlineBox* stop_root_inline_box) const {
  NOT_DESTROYED();
#if DCHECK_IS_ON()
  DCHECK(!stop_root_inline_box ||
         stop_root_inline_box->Block().DebugPointer() == this);
#endif
  if (!ChildrenInline())
    return 0;

  int count = 0;
  for (const RootInlineBox* box = FirstRootBox(); box;
       box = box->NextRootBox()) {
    count++;
    if (box == stop_root_inline_box)
      break;
  }
  return count;
}

LayoutUnit LayoutBlockFlow::FirstLineBoxBaseline() const {
  NOT_DESTROYED();
  if (!ChildrenInline())
    return LayoutBlock::FirstLineBoxBaseline();
  if (const absl::optional<LayoutUnit> baseline =
          FirstLineBoxBaselineOverride())
    return *baseline;
  if (FirstLineBox()) {
    const SimpleFontData* font_data = Style(true)->GetFont().PrimaryFont();
    DCHECK(font_data);
    if (!font_data)
      return LayoutUnit(-1);
    // fontMetrics 'ascent' is the distance above the baseline to the 'over'
    // edge, which is 'top' for horizontal and 'right' for vertical-lr and
    // vertical-rl. However, firstLineBox()->logicalTop() gives the offset from
    // the 'left' edge for vertical-lr, hence we need to use the Font Metrics
    // 'descent' instead. The result should be handled accordingly by the caller
    // as a 'descent' value, in order to compute properly the max baseline.
    if (StyleRef().IsFlippedLinesWritingMode()) {
      return FirstLineBox()->LogicalTop() + font_data->GetFontMetrics().Descent(
                                                FirstRootBox()->BaselineType());
    }
    return FirstLineBox()->LogicalTop() +
           font_data->GetFontMetrics().Ascent(FirstRootBox()->BaselineType());
  }
  return EmptyLineBaseline(IsHorizontalWritingMode() ? kHorizontalLine
                                                     : kVerticalLine);
}

LayoutUnit LayoutBlockFlow::InlineBlockBaseline(
    LineDirectionMode line_direction) const {
  NOT_DESTROYED();
  if (!ChildrenInline())
    return LayoutBlock::InlineBlockBaseline(line_direction);
  if (const absl::optional<LayoutUnit> baseline =
          InlineBlockBaselineOverride(line_direction))
    return *baseline;
  if (LastLineBox()) {
    const SimpleFontData* font_data =
        Style(LastLineBox() == FirstLineBox())->GetFont().PrimaryFont();
    DCHECK(font_data);
    if (!font_data)
      return LayoutUnit(-1);
    // InlineFlowBox::placeBoxesInBlockDirection will flip lines in
    // case of verticalLR mode, so we can assume verticalRL for now.
    if (StyleRef().IsFlippedLinesWritingMode()) {
      return LogicalHeight() - LastLineBox()->LogicalBottom() +
             font_data->GetFontMetrics().Ascent(LastRootBox()->BaselineType());
    }
    return LastLineBox()->LogicalTop() +
           font_data->GetFontMetrics().Ascent(LastRootBox()->BaselineType());
  }
  return EmptyLineBaseline(line_direction);
}

void LayoutBlockFlow::WillBeDestroyed() {
  NOT_DESTROYED();
  // Make sure to destroy anonymous children first while they are still
  // connected to the rest of the tree, so that they will properly dirty line
  // boxes that they are removed from. Effects that do :before/:after only on
  // hover could crash otherwise.
  Children()->DestroyLeftoverChildren();

  if (!DocumentBeingDestroyed()) {
    // TODO(mstensho): figure out if we need this. We have no test coverage for
    // it. It looks like all line boxes have been removed at this point.
    if (FirstLineBox()) {
      // If we are an anonymous block, then our line boxes might have children
      // that will outlast this block. In the non-anonymous block case those
      // children will be destroyed by the time we return from this function.
      if (IsAnonymousBlock()) {
        for (InlineFlowBox* box : *LineBoxes()) {
          while (InlineBox* child_box = box->FirstChild())
            child_box->Remove();
        }
      }
    }
  }

  line_boxes_.DeleteLineBoxes();

  LayoutBlock::WillBeDestroyed();
}

void LayoutBlockFlow::UpdateBlockChildDirtyBitsBeforeLayout(
    bool relayout_children,
    LayoutBox& child) {
  NOT_DESTROYED();
  if (auto* placeholder = DynamicTo<LayoutMultiColumnSpannerPlaceholder>(child))
    placeholder->MarkForLayoutIfObjectInFlowThreadNeedsLayout();
  LayoutBlock::UpdateBlockChildDirtyBitsBeforeLayout(relayout_children, child);
}

void LayoutBlockFlow::UpdateStaticInlinePositionForChild(
    LayoutBox& child,
    LayoutUnit logical_top,
    IndentTextOrNot indent_text) {
  NOT_DESTROYED();
  if (child.StyleRef().IsOriginalDisplayInlineType())
    SetStaticInlinePositionForChild(
        child, StartAlignedOffsetForLine(logical_top, indent_text));
  else
    SetStaticInlinePositionForChild(child, StartOffsetForContent());
}

void LayoutBlockFlow::SetStaticInlinePositionForChild(
    LayoutBox& child,
    LayoutUnit inline_position) {
  NOT_DESTROYED();
  child.Layer()->SetStaticInlinePosition(inline_position);
}

void LayoutBlockFlow::AddChild(LayoutObject* new_child,
                               LayoutObject* before_child) {
  NOT_DESTROYED();
  if (LayoutMultiColumnFlowThread* flow_thread = MultiColumnFlowThread()) {
    if (before_child == flow_thread)
      before_child = flow_thread->FirstChild();
    DCHECK(!before_child || before_child->IsDescendantOf(flow_thread));
    flow_thread->AddChild(new_child, before_child);
    return;
  }

  if (before_child && before_child->Parent() != this) {
    AddChildBeforeDescendant(new_child, before_child);
    return;
  }

  bool made_boxes_non_inline = false;

  // A block has to either have all of its children inline, or all of its
  // children as blocks.
  // So, if our children are currently inline and a block child has to be
  // inserted, we move all our inline children into anonymous block boxes.
  bool child_is_block_level =
      !new_child->IsInline() && !new_child->IsFloatingOrOutOfFlowPositioned();

  if (ChildrenInline()) {
    if (child_is_block_level) {
      // Wrap the inline content in anonymous blocks, to allow for the new block
      // child to be inserted.
      MakeChildrenNonInline(before_child);
      made_boxes_non_inline = true;

      if (before_child && before_child->Parent() != this) {
        before_child = before_child->Parent();
        DCHECK(before_child->IsAnonymousBlock());
        DCHECK_EQ(before_child->Parent(), this);
      }
    }
  } else if (!child_is_block_level) {
    // This block has block children. We may want to put the new child into an
    // anomyous block. Floats and out-of-flow children may live among either
    // block or inline children, so for such children, only put them inside an
    // anonymous block if one already exists. If the child is inline, on the
    // other hand, we *have to* put it inside an anonymous block, so create a
    // new one if there is none for us there already.
    LayoutObject* after_child =
        before_child ? before_child->PreviousSibling() : LastChild();

    if (after_child && after_child->IsAnonymousBlock()) {
      after_child->AddChild(new_child);
      return;
    }

    // LayoutNGOutsideListMarker is out-of-flow for the tree building purpose,
    // and that is not inline level, but IsInline().
    if (new_child->IsInline() && !new_child->IsLayoutNGOutsideListMarker()) {
      // No suitable existing anonymous box - create a new one.
      auto* new_block = To<LayoutBlockFlow>(CreateAnonymousBlock());
      LayoutBox::AddChild(new_block, before_child);
      // Reparent adjacent floating or out-of-flow siblings to the new box.
      new_block->ReparentPrecedingFloatingOrOutOfFlowSiblings();
      new_block->AddChild(new_child);
      new_block->ReparentSubsequentFloatingOrOutOfFlowSiblings();
      return;
    }
  }

  // Skip the LayoutBlock override, since that one deals with anonymous child
  // insertion in a way that isn't sufficient for us, and can only cause trouble
  // at this point.
  LayoutBox::AddChild(new_child, before_child);
  auto* parent_layout_block = DynamicTo<LayoutBlock>(Parent());
  if (made_boxes_non_inline && IsAnonymousBlock() && parent_layout_block) {
    parent_layout_block->RemoveLeftoverAnonymousBlock(this);
    // |this| may be dead now.
  }
}

static bool IsMergeableAnonymousBlock(const LayoutBlockFlow* block) {
  return block->IsAnonymousBlock() && !block->BeingDestroyed() &&
         !block->IsRubyRun() && !block->IsRubyBase();
}

void LayoutBlockFlow::RemoveChild(LayoutObject* old_child) {
  NOT_DESTROYED();
  // No need to waste time in merging or removing empty anonymous blocks.
  // We can just bail out if our document is getting destroyed.
  if (DocumentBeingDestroyed()) {
    LayoutBox::RemoveChild(old_child);
    return;
  }

  // If this child is a block, and if our previous and next siblings are both
  // anonymous blocks with inline content, then we can go ahead and fold the
  // inline content back together. If only one of the siblings is such an
  // anonymous blocks, check if the other sibling (and any of *its* siblings)
  // are floating or out-of-flow positioned. In that case, they should be moved
  // into the anonymous block.
  LayoutObject* prev = old_child->PreviousSibling();
  LayoutObject* next = old_child->NextSibling();
  bool merged_anonymous_blocks = false;
  if (prev && next && !old_child->IsInline()) {
    auto* prev_block_flow = DynamicTo<LayoutBlockFlow>(prev);
    auto* next_block_flow = DynamicTo<LayoutBlockFlow>(next);
    if (prev_block_flow && next_block_flow &&
        prev_block_flow->MergeSiblingContiguousAnonymousBlock(
            next_block_flow)) {
      merged_anonymous_blocks = true;
      next = nullptr;
    } else if (prev_block_flow && IsMergeableAnonymousBlock(prev_block_flow)) {
      // The previous sibling is anonymous. Scan the next siblings and reparent
      // any floating or out-of-flow positioned objects into the end of the
      // previous anonymous block.
      while (next && next->IsFloatingOrOutOfFlowPositioned()) {
        LayoutObject* sibling = next->NextSibling();
        MoveChildTo(prev_block_flow, next, nullptr, false);
        next = sibling;
      }
    } else if (next_block_flow && IsMergeableAnonymousBlock(next_block_flow)) {
      // The next sibling is anonymous. Scan the previous siblings and reparent
      // any floating or out-of-flow positioned objects into the start of the
      // next anonymous block.
      while (prev && prev->IsFloatingOrOutOfFlowPositioned()) {
        LayoutObject* sibling = prev->PreviousSibling();
        MoveChildTo(next_block_flow, prev, next_block_flow->FirstChild(),
                    false);
        prev = sibling;
      }
    }
  }

  LayoutBlock::RemoveChild(old_child);

  LayoutObject* child = prev ? prev : next;
  auto* child_block_flow = DynamicTo<LayoutBlockFlow>(child);
  if (child && child_block_flow && !child->PreviousSibling() &&
      !child->NextSibling()) {
    // If the removal has knocked us down to containing only a single anonymous
    // box we can go ahead and pull the content right back up into our
    // box.
    if (merged_anonymous_blocks || IsMergeableAnonymousBlock(child_block_flow))
      CollapseAnonymousBlockChild(child_block_flow);
  }

  if (!FirstChild()) {
    // If this was our last child be sure to clear out our line boxes.
    if (ChildrenInline())
      DeleteLineBoxTree();
  } else if (!BeingDestroyed() &&
             !old_child->IsFloatingOrOutOfFlowPositioned() &&
             !old_child->IsAnonymousBlock()) {
    // If the child we're removing means that we can now treat all children as
    // inline without the need for anonymous blocks, then do that.
    MakeChildrenInlineIfPossible();
  }
}

bool LayoutBlockFlow::CreatesAnonymousWrapper() const {
  return IsLayoutFlowThread() && Parent()->IsLayoutNGObject();
}

void LayoutBlockFlow::MoveAllChildrenIncludingFloatsTo(
    LayoutBlock* to_block,
    bool full_remove_insert) {
  NOT_DESTROYED();
  auto* to_block_flow = To<LayoutBlockFlow>(to_block);

  DCHECK(full_remove_insert ||
         to_block_flow->ChildrenInline() == ChildrenInline());

  MoveAllChildrenTo(to_block_flow, full_remove_insert);
}

void LayoutBlockFlow::ChildBecameFloatingOrOutOfFlow(LayoutBox* child) {
  NOT_DESTROYED();
  MakeChildrenInlineIfPossible();

  // Reparent the child to an adjacent anonymous block if one is available.
  LayoutObject* prev = child->PreviousSibling();
  auto* new_container = DynamicTo<LayoutBlockFlow>(prev);
  if (prev && prev->IsAnonymousBlock() && new_container) {
    MoveChildTo(new_container, child, nullptr, false);
    // The anonymous block we've moved to may now be adjacent to former siblings
    // of ours that it can contain also.
    new_container->ReparentSubsequentFloatingOrOutOfFlowSiblings();
    return;
  }
  LayoutObject* next = child->NextSibling();
  new_container = DynamicTo<LayoutBlockFlow>(next);
  if (next && next->IsAnonymousBlock() && next->IsLayoutBlockFlow()) {
    MoveChildTo(new_container, child, new_container->FirstChild(), false);
  }
}

static bool AllowsCollapseAnonymousBlockChild(const LayoutBlockFlow& parent,
                                              const LayoutBlockFlow& child) {
  // It's possible that this block's destruction may have been triggered by the
  // child's removal. Just bail if the anonymous child block is already being
  // destroyed. See crbug.com/282088
  if (child.BeingDestroyed())
    return false;
  // Ruby elements use anonymous wrappers for ruby runs and ruby bases by
  // design, so we don't remove them.
  if (child.IsRubyRun() || child.IsRubyBase())
    return false;
  if (IsA<LayoutMultiColumnFlowThread>(parent) &&
      parent.Parent()->IsLayoutNGObject() && child.ChildrenInline()) {
    // The test[1] reaches here.
    // [1] "fast/multicol/dynamic/remove-spanner-in-content.html"
    return false;
  }
  return true;
}

void LayoutBlockFlow::CollapseAnonymousBlockChild(LayoutBlockFlow* child) {
  NOT_DESTROYED();
  if (!AllowsCollapseAnonymousBlockChild(*this, *child))
    return;
  SetNeedsLayoutAndIntrinsicWidthsRecalcAndFullPaintInvalidation(
      layout_invalidation_reason::kChildAnonymousBlockChanged);

  child->MoveAllChildrenTo(this, child->NextSibling(), child->HasLayer());
  SetChildrenInline(child->ChildrenInline());

  Children()->RemoveChildNode(this, child, child->HasLayer());
  child->Destroy();
}

bool LayoutBlockFlow::MergeSiblingContiguousAnonymousBlock(
    LayoutBlockFlow* sibling_that_may_be_deleted) {
  NOT_DESTROYED();
  // Note: |this| and |siblingThatMayBeDeleted| may not be adjacent siblings at
  // this point. There may be an object between them which is about to be
  // removed.

  if (!IsMergeableAnonymousBlock(this) ||
      !IsMergeableAnonymousBlock(sibling_that_may_be_deleted))
    return false;

  SetNeedsLayoutAndIntrinsicWidthsRecalcAndFullPaintInvalidation(
      layout_invalidation_reason::kAnonymousBlockChange);

  // If the inlineness of children of the two block don't match, we'd need
  // special code here (but there should be no need for it).
  DCHECK_EQ(sibling_that_may_be_deleted->ChildrenInline(), ChildrenInline());

  // Take all the children out of the |next| block and put them in the |prev|
  // block. If there are paint layers involved, or if we're part of a flow
  // thread, we need to notify the layout tree about the movement.
  bool full_remove_insert = sibling_that_may_be_deleted->HasLayer() ||
                            HasLayer() ||
                            sibling_that_may_be_deleted->IsInsideFlowThread();
  sibling_that_may_be_deleted->MoveAllChildrenIncludingFloatsTo(
      this, full_remove_insert);
  // Delete the now-empty block's lines and nuke it.
  sibling_that_may_be_deleted->DeleteLineBoxTree();
  sibling_that_may_be_deleted->Destroy();
  return true;
}

void LayoutBlockFlow::ReparentSubsequentFloatingOrOutOfFlowSiblings() {
  NOT_DESTROYED();
  auto* parent_block_flow = DynamicTo<LayoutBlockFlow>(Parent());
  if (!parent_block_flow)
    return;
  if (BeingDestroyed() || DocumentBeingDestroyed())
    return;
  LayoutObject* child = NextSibling();
  while (child && child->IsFloatingOrOutOfFlowPositioned()) {
    LayoutObject* sibling = child->NextSibling();
    parent_block_flow->MoveChildTo(this, child, nullptr, false);
    child = sibling;
  }

  if (LayoutObject* next = NextSibling()) {
    auto* next_block_flow = DynamicTo<LayoutBlockFlow>(next);
    if (next_block_flow)
      MergeSiblingContiguousAnonymousBlock(next_block_flow);
  }
}

void LayoutBlockFlow::ReparentPrecedingFloatingOrOutOfFlowSiblings() {
  NOT_DESTROYED();
  auto* parent_block_flow = DynamicTo<LayoutBlockFlow>(Parent());
  if (!parent_block_flow)
    return;
  if (BeingDestroyed() || DocumentBeingDestroyed())
    return;
  LayoutObject* child = PreviousSibling();
  while (child && child->IsFloatingOrOutOfFlowPositioned()) {
    LayoutObject* sibling = child->PreviousSibling();
    parent_block_flow->MoveChildTo(this, child, FirstChild(), false);
    child = sibling;
  }
}

static bool AllowsInlineChildren(const LayoutBlockFlow& block_flow) {
  // Collapsing away anonymous wrappers isn't relevant for the children of
  // anonymous blocks, unless they are ruby bases.
  if (block_flow.IsAnonymousBlock() && !block_flow.IsRubyBase())
    return false;
  if (IsA<LayoutMultiColumnFlowThread>(block_flow) &&
      block_flow.Parent()->IsLayoutNGObject())
    return false;
  return true;
}

void LayoutBlockFlow::MakeChildrenInlineIfPossible() {
  NOT_DESTROYED();
  if (!AllowsInlineChildren(*this))
    return;

  HeapVector<Member<LayoutBlockFlow>, 3> blocks_to_remove;
  for (LayoutObject* child = FirstChild(); child;
       child = child->NextSibling()) {
    if (child->IsFloating())
      continue;
    if (child->IsOutOfFlowPositioned())
      continue;

    // There are still block children in the container, so any anonymous
    // wrappers are still needed.
    auto* child_block_flow = DynamicTo<LayoutBlockFlow>(child);
    if (!child->IsAnonymousBlock() || !child_block_flow)
      return;
    // If one of the children is being destroyed then it is unsafe to clean up
    // anonymous wrappers as the
    // entire branch may be being destroyed.
    if (child_block_flow->BeingDestroyed())
      return;
    // We are only interested in removing anonymous wrappers if there are inline
    // siblings underneath them.
    if (!child->ChildrenInline())
      return;
    // Ruby elements use anonymous wrappers for ruby runs and ruby bases by
    // design, so we don't remove them.
    if (child->IsRubyRun() || child->IsRubyBase())
      return;

    blocks_to_remove.push_back(child_block_flow);
  }

  for (LayoutBlockFlow* child : blocks_to_remove)
    CollapseAnonymousBlockChild(child);
  SetChildrenInline(true);
}

static void GetInlineRun(LayoutObject* start,
                         LayoutObject* boundary,
                         LayoutObject*& inline_run_start,
                         LayoutObject*& inline_run_end) {
  // Beginning at |start| we find the largest contiguous run of inlines that
  // we can.  We denote the run with start and end points, |inlineRunStart|
  // and |inlineRunEnd|.  Note that these two values may be the same if
  // we encounter only one inline.
  //
  // We skip any non-inlines we encounter as long as we haven't found any
  // inlines yet.
  //
  // |boundary| indicates a non-inclusive boundary point.  Regardless of whether
  // |boundary| is inline or not, we will not include it in a run with inlines
  // before it. It's as though we encountered a non-inline.

  // Start by skipping as many non-inlines as we can.
  LayoutObject* curr = start;

  // LayoutNGOutsideListMarker is out-of-flow for the tree building purpose.
  // Skip here because it's the first child.
  if (curr && curr->IsLayoutNGOutsideListMarker())
    curr = curr->NextSibling();

  bool saw_inline;
  do {
    while (curr &&
           !(curr->IsInline() || curr->IsFloatingOrOutOfFlowPositioned()))
      curr = curr->NextSibling();

    inline_run_start = inline_run_end = curr;

    if (!curr)
      return;  // No more inline children to be found.

    saw_inline = curr->IsInline();

    curr = curr->NextSibling();
    while (curr &&
           (curr->IsInline() || curr->IsFloatingOrOutOfFlowPositioned()) &&
           (curr != boundary)) {
      inline_run_end = curr;
      if (curr->IsInline())
        saw_inline = true;
      curr = curr->NextSibling();
    }
  } while (!saw_inline);
}

void LayoutBlockFlow::MakeChildrenNonInline(LayoutObject* insertion_point) {
  NOT_DESTROYED();

  // makeChildrenNonInline takes a block whose children are *all* inline and it
  // makes sure that inline children are coalesced under anonymous blocks.
  // If |insertionPoint| is defined, then it represents the insertion point for
  // the new block child that is causing us to have to wrap all the inlines.
  // This means that we cannot coalesce inlines before |insertionPoint| with
  // inlines following |insertionPoint|, because the new child is going to be
  // inserted in between the inlines, splitting them.
  DCHECK(!IsInline() || IsAtomicInlineLevel());
  DCHECK(!insertion_point || insertion_point->Parent() == this);

  SetChildrenInline(false);
  ClearNGInlineNodeData();

  LayoutObject* child = FirstChild();
  if (!child)
    return;

  DeleteLineBoxTree();

  while (child) {
    LayoutObject* inline_run_start;
    LayoutObject* inline_run_end;
    GetInlineRun(child, insertion_point, inline_run_start, inline_run_end);

    if (!inline_run_start)
      break;

    child = inline_run_end->NextSibling();

    LayoutBlock* block = CreateAnonymousBlock();
    Children()->InsertChildNode(this, block, inline_run_start);
    MoveChildrenTo(block, inline_run_start, child);
  }

#if DCHECK_IS_ON()
  for (LayoutObject* c = FirstChild(); c; c = c->NextSibling())
    DCHECK(!c->IsInline() || c->IsLayoutNGOutsideListMarker());
#endif

  SetShouldDoFullPaintInvalidation();
}

void LayoutBlockFlow::ChildBecameNonInline(LayoutObject*) {
  NOT_DESTROYED();
  MakeChildrenNonInline();
  auto* parent_layout_block = DynamicTo<LayoutBlock>(Parent());
  if (IsAnonymousBlock() && parent_layout_block)
    parent_layout_block->RemoveLeftoverAnonymousBlock(this);
  // |this| may be dead here
}

LayoutUnit LayoutBlockFlow::LogicalLeftOffsetForPositioningFloat(
    LayoutUnit logical_top,
    LayoutUnit fixed_offset,
    LayoutUnit* height_remaining) const {
  NOT_DESTROYED();
  LayoutUnit offset = fixed_offset;
  return AdjustLogicalLeftOffsetForLine(offset, kDoNotIndentText);
}

LayoutUnit LayoutBlockFlow::LogicalRightOffsetForPositioningFloat(
    LayoutUnit logical_top,
    LayoutUnit fixed_offset,
    LayoutUnit* height_remaining) const {
  NOT_DESTROYED();
  LayoutUnit offset = fixed_offset;
  return AdjustLogicalRightOffsetForLine(offset, kDoNotIndentText);
}

LayoutUnit LayoutBlockFlow::AdjustLogicalLeftOffsetForLine(
    LayoutUnit offset_from_floats,
    IndentTextOrNot apply_text_indent) const {
  NOT_DESTROYED();
  LayoutUnit left = offset_from_floats;

  if (apply_text_indent == kIndentText && StyleRef().IsLeftToRightDirection())
    left += TextIndentOffset();

  return left;
}

LayoutUnit LayoutBlockFlow::AdjustLogicalRightOffsetForLine(
    LayoutUnit offset_from_floats,
    IndentTextOrNot apply_text_indent) const {
  NOT_DESTROYED();
  LayoutUnit right = offset_from_floats;

  if (apply_text_indent == kIndentText && !StyleRef().IsLeftToRightDirection())
    right -= TextIndentOffset();

  return right;
}

Node* LayoutBlockFlow::NodeForHitTest() const {
  NOT_DESTROYED();
  // If we are in the margins of block elements that are part of a
  // block-in-inline we're actually still inside the enclosing element
  // that was split. Use the appropriate inner node.
  if (UNLIKELY(IsBlockInInline())) {
    DCHECK(Parent());
    DCHECK(Parent()->IsLayoutInline());
    return Parent()->NodeForHitTest();
  }
  return LayoutBlock::NodeForHitTest();
}

bool LayoutBlockFlow::HitTestChildren(HitTestResult& result,
                                      const HitTestLocation& hit_test_location,
                                      const PhysicalOffset& accumulated_offset,
                                      HitTestPhase phase) {
  NOT_DESTROYED();
  PhysicalOffset scrolled_offset = accumulated_offset;
  if (IsScrollContainer())
    scrolled_offset -= PhysicalOffset(PixelSnappedScrolledContentOffset());

  if (ChildrenInline()) {
    if (line_boxes_.HitTest(LineLayoutBoxModel(this), result, hit_test_location,
                            scrolled_offset, phase)) {
      UpdateHitTestResult(result,
                          hit_test_location.Point() - accumulated_offset);
      return true;
    }
  } else if (LayoutBlock::HitTestChildren(result, hit_test_location,
                                          accumulated_offset, phase)) {
    return true;
  }

  return false;
}

bool LayoutBlockFlow::AllowsColumns() const {
  // Ruby elements manage child insertion in a special way, and would mess up
  // insertion of the flow thread. The flow thread needs to be a direct child of
  // the multicol block (|this|).
  if (IsRuby())
    return false;

  // We don't allow custom layout and multicol on the same object. This is
  // similar to not allowing it for flexbox, grids and tables (although those
  // don't create LayoutBlockFlow, so we don't need to check for those here).
  if (StyleRef().IsDisplayLayoutCustomBox())
    return false;

  // MathML layout objects don't support multicol.
  if (IsMathML())
    return false;

  return true;
}

void LayoutBlockFlow::MoveChildrenTo(LayoutBoxModelObject* to_box_model_object,
                                     LayoutObject* start_child,
                                     LayoutObject* end_child,
                                     LayoutObject* before_child,
                                     bool full_remove_insert) {
  NOT_DESTROYED();
  if (ChildrenInline())
    DeleteLineBoxTree();
  LayoutBoxModelObject::MoveChildrenTo(to_box_model_object, start_child,
                                       end_child, before_child,
                                       full_remove_insert);
}

RootInlineBox* LayoutBlockFlow::CreateRootInlineBox() {
  NOT_DESTROYED();
  return MakeGarbageCollected<RootInlineBox>(LineLayoutItem(this));
}

void LayoutBlockFlow::CreateOrDestroyMultiColumnFlowThreadIfNeeded(
    const ComputedStyle* old_style) {
  NOT_DESTROYED();
  bool specifies_columns = StyleRef().SpecifiesColumns();

  if (MultiColumnFlowThread()) {
    DCHECK(old_style);
    if (specifies_columns != old_style->SpecifiesColumns()) {
      // If we're no longer to be multicol/paged, destroy the flow thread. Also
      // destroy it when switching between multicol and paged, since that
      // affects the column set structure (multicol containers may have
      // spanners, paged containers may not).
      MultiColumnFlowThread()->EvacuateAndDestroy();
      DCHECK(!MultiColumnFlowThread());
    }
    return;
  }

  if (!specifies_columns)
    return;

  if (IsListItemIncludingNG())
    UseCounter::Count(GetDocument(), WebFeature::kMultiColAndListItem);

  if (!AllowsColumns())
    return;

  // Fieldsets look for a legend special child (layoutSpecialExcludedChild()).
  // We currently only support one special child per layout object, and the
  // flow thread would make for a second one.
  // For LayoutNG, the multi-column display type will be applied to the
  // anonymous content box. Thus, the flow thread should be added to the
  // anonymous content box instead of the fieldset itself.
  if (IsFieldset()) {
    return;
  }

  // Form controls are replaced content (also when implemented as a regular
  // block), and are therefore not supposed to support multicol.
  const auto* element = DynamicTo<Element>(GetNode());
  if (element && element->IsFormControlElement())
    return;

  auto* flow_thread =
      LayoutMultiColumnFlowThread::CreateAnonymous(GetDocument(), StyleRef());
  AddChild(flow_thread);
  if (IsLayoutNGObject()) {
    // For simplicity of layout algorithm, we assume flow thread having block
    // level children only.
    // For example, we can handle them in same way:
    //   <div style="columns:3">abc<br>def<br>ghi<br></div>
    //   <div style="columns:3"><div>abc<br>def<br>ghi<br></div></div>
    flow_thread->SetChildrenInline(false);
  }

  // Check that addChild() put the flow thread as a direct child, and didn't do
  // fancy things.
  DCHECK_EQ(flow_thread->Parent(), this);

  flow_thread->Populate();

  LayoutBlockFlowRareData& rare_data = EnsureRareData();
  DCHECK(!rare_data.multi_column_flow_thread_);
  rare_data.multi_column_flow_thread_ = flow_thread;
}

LayoutBlockFlow::LayoutBlockFlowRareData& LayoutBlockFlow::EnsureRareData() {
  NOT_DESTROYED();
  if (rare_data_)
    return *rare_data_;

  rare_data_ = MakeGarbageCollected<LayoutBlockFlowRareData>(this);
  return *rare_data_;
}

void LayoutBlockFlow::SimplifiedNormalFlowInlineLayout() {
  NOT_DESTROYED();
  DCHECK(ChildrenInline());
  HeapLinkedHashSet<Member<RootInlineBox>> line_boxes;
  ClearCollectionScope<HeapLinkedHashSet<Member<RootInlineBox>>> scope(
      &line_boxes);
  for (InlineWalker walker(LineLayoutBlockFlow(this)); !walker.AtEnd();
       walker.Advance()) {
    LayoutObject* o = walker.Current().GetLayoutObject();
    if (!o->IsOutOfFlowPositioned() &&
        (o->IsAtomicInlineLevel() || o->IsFloating())) {
      o->LayoutIfNeeded();
      if (To<LayoutBox>(o)->InlineBoxWrapper()) {
        RootInlineBox& box = To<LayoutBox>(o)->InlineBoxWrapper()->Root();
        line_boxes.insert(&box);
      }
    } else if (o->IsText() ||
               (o->IsLayoutInline() && !walker.AtEndOfInline())) {
      o->ClearNeedsLayout();
    }
  }

  // FIXME: Glyph overflow will get lost in this case, but not really a big
  // deal.
  GlyphOverflowAndFallbackFontsMap text_box_data_map;
  for (auto box : line_boxes) {
    box->ComputeOverflow(box->LineTop(), box->LineBottom(), text_box_data_map);
  }
}

RecalcLayoutOverflowResult
LayoutBlockFlow::RecalcInlineChildrenLayoutOverflow() {
  NOT_DESTROYED();
  DCHECK(ChildrenInline());
  RecalcLayoutOverflowResult result;
  HeapHashSet<Member<RootInlineBox>> line_boxes;
  ClearCollectionScope<HeapHashSet<Member<RootInlineBox>>> scope(&line_boxes);
  for (InlineWalker walker(LineLayoutBlockFlow(this)); !walker.AtEnd();
       walker.Advance()) {
    LayoutObject* layout_object = walker.Current().GetLayoutObject();
    if (layout_object->IsOutOfFlowPositioned())
      continue;

    result.Unite(layout_object->RecalcLayoutOverflow());

    // TODO(chrishtr): should this be IsBox()? Non-blocks can be inline and
    // have line box wrappers.
    if (auto* layout_block_object = DynamicTo<LayoutBlock>(layout_object)) {
      if (InlineBox* inline_box_wrapper =
              layout_block_object->InlineBoxWrapper())
        line_boxes.insert(&inline_box_wrapper->Root());
    }
  }

  // FIXME: Glyph overflow will get lost in this case, but not really a big
  // deal.
  GlyphOverflowAndFallbackFontsMap text_box_data_map;
  for (auto box : line_boxes) {
    box->ClearKnownToHaveNoOverflow();
    box->ComputeOverflow(box->LineTop(), box->LineBottom(), text_box_data_map);
  }
  return result;
}

void LayoutBlockFlow::RecalcInlineChildrenVisualOverflow() {
  NOT_DESTROYED();
  DCHECK(ChildrenInline());

  // TODO(crbug.com/1144203): This code path should be switch to
  // |RecalcFragmentsVisualOverflow|.
  if (CanUseFragmentsForVisualOverflow()) {
    for (const NGPhysicalBoxFragment& fragment : PhysicalFragments()) {
      if (const NGFragmentItems* items = fragment.Items()) {
        NGInlineCursor cursor(fragment, *items);
        NGInlinePaintContext inline_context;
        NGFragmentItem::RecalcInkOverflowForCursor(&cursor, &inline_context);
      }
      // Even if this turned out to be an inline formatting context with
      // fragment items (handled above), we need to handle floating descendants.
      // If a float is block-fragmented, it is resumed as a regular box fragment
      // child, rather than becoming a fragment item.
      if (fragment.HasFloatingDescendantsForPaint())
        RecalcFloatingDescendantsVisualOverflow(fragment);
    }
    return;
  }

  for (InlineWalker walker(LineLayoutBlockFlow(this)); !walker.AtEnd();
       walker.Advance()) {
    LayoutObject* layout_object = walker.Current().GetLayoutObject();
    layout_object->RecalcNormalFlowChildVisualOverflowIfNeeded();
  }

  // Child inline boxes' self visual overflow is already computed at the same
  // time as layout overflow. But we need to add replaced children visual rects.
  for (RootInlineBox* box = FirstRootBox(); box; box = box->NextRootBox())
    box->AddReplacedChildrenVisualOverflow(box->LineTop(), box->LineBottom());
}

void LayoutBlockFlow::RecalcFloatingDescendantsVisualOverflow(
    const NGPhysicalFragment& fragment) {
  DCHECK(fragment.HasFloatingDescendantsForPaint());

  for (const NGLink& child : fragment.PostLayoutChildren()) {
    if (child->IsFloating()) {
      child->GetMutableLayoutObject()
          ->RecalcNormalFlowChildVisualOverflowIfNeeded();
      continue;
    }

    if (const NGPhysicalFragment* child_container_fragment = child.get()) {
      if (child_container_fragment->HasFloatingDescendantsForPaint())
        RecalcFloatingDescendantsVisualOverflow(*child_container_fragment);
    }
  }
}

PositionWithAffinity LayoutBlockFlow::PositionForPoint(
    const PhysicalOffset& point) const {
  NOT_DESTROYED();
  // NG codepath requires |kPrePaintClean|.
  // |SelectionModifier| calls this only in legacy codepath.
  DCHECK(!IsLayoutNGObject() || GetDocument().Lifecycle().GetState() >=
                                    DocumentLifecycle::kPrePaintClean);

  if (IsAtomicInlineLevel()) {
    PositionWithAffinity position =
        PositionForPointIfOutsideAtomicInlineLevel(point);
    if (!position.IsNull())
      return position;
  }
  if (!ChildrenInline())
    return LayoutBlock::PositionForPoint(point);

  PhysicalOffset point_in_contents = point;
  OffsetForContents(point_in_contents);
  LayoutPoint point_in_logical_contents = FlipForWritingMode(point_in_contents);
  if (!IsHorizontalWritingMode())
    point_in_logical_contents = point_in_logical_contents.TransposedPoint();

  if (!FirstRootBox())
    return CreatePositionWithAffinity(0);

  bool lines_are_flipped = StyleRef().IsFlippedLinesWritingMode();
  bool blocks_are_flipped = StyleRef().IsFlippedBlocksWritingMode();

  // look for the closest line box in the root box which is at the passed-in y
  // coordinate
  InlineBox* closest_box = nullptr;
  RootInlineBox* first_root_box_with_children = nullptr;
  RootInlineBox* last_root_box_with_children = nullptr;
  for (RootInlineBox* root = FirstRootBox(); root; root = root->NextRootBox()) {
    if (!root->FirstLeafChild())
      continue;
    if (!first_root_box_with_children)
      first_root_box_with_children = root;

    if (!lines_are_flipped && root->IsFirstAfterPageBreak() &&
        (point_in_logical_contents.Y() < root->LineTopWithLeading() ||
         (blocks_are_flipped &&
          point_in_logical_contents.Y() == root->LineTopWithLeading())))
      break;

    last_root_box_with_children = root;

    // check if this root line box is located at this y coordinate
    if (point_in_logical_contents.Y() < root->SelectionBottom() ||
        (blocks_are_flipped &&
         point_in_logical_contents.Y() == root->SelectionBottom())) {
      if (lines_are_flipped) {
        RootInlineBox* next_root_box_with_children = root->NextRootBox();
        while (next_root_box_with_children &&
               !next_root_box_with_children->FirstLeafChild())
          next_root_box_with_children =
              next_root_box_with_children->NextRootBox();

        if (next_root_box_with_children &&
            next_root_box_with_children->IsFirstAfterPageBreak() &&
            (point_in_logical_contents.Y() >
                 next_root_box_with_children->LineTopWithLeading() ||
             (!blocks_are_flipped &&
              point_in_logical_contents.Y() ==
                  next_root_box_with_children->LineTopWithLeading())))
          continue;
      }
      closest_box = root->ClosestLeafChildForLogicalLeftPosition(
          point_in_logical_contents.X());
      if (closest_box)
        break;
    }
  }

  const bool move_caret_to_boundary =
      ShouldMoveCaretToHorizontalBoundaryWhenPastTopOrBottom();
  if (!move_caret_to_boundary && !closest_box && last_root_box_with_children) {
    // y coordinate is below last root line box, pretend we hit it
    closest_box =
        last_root_box_with_children->ClosestLeafChildForLogicalLeftPosition(
            point_in_logical_contents.X());
  }

  if (closest_box) {
    if (move_caret_to_boundary) {
      LayoutUnit first_root_box_with_children_top =
          std::min<LayoutUnit>(first_root_box_with_children->SelectionTop(),
                               first_root_box_with_children->LogicalTop());
      if (point_in_logical_contents.Y() < first_root_box_with_children_top ||
          (blocks_are_flipped &&
           point_in_logical_contents.Y() == first_root_box_with_children_top)) {
        InlineBox* box = first_root_box_with_children->FirstLeafChild();
        if (box->IsLineBreak()) {
          if (InlineBox* new_box = box->NextLeafChildIgnoringLineBreak())
            box = new_box;
        }
        // y coordinate is above first root line box, so return the start of the
        // first
        return PositionWithAffinity(PositionForBox(box, true));
      }
    }

    if (closest_box->GetLineLayoutItem().IsAtomicInlineLevel()) {
      // We want to pass the original point other than a corrected one.
      LayoutPoint adjusted_point(point_in_logical_contents);
      if (!IsHorizontalWritingMode())
        adjusted_point = adjusted_point.TransposedPoint();
      return PositionForPointRespectingEditingBoundaries(
          LineLayoutBox(closest_box->GetLineLayoutItem()),
          FlipForWritingMode(adjusted_point));
    }

    // pass the box a top position that is inside it
    LayoutPoint adjusted_point(point_in_logical_contents.X(),
                               closest_box->Root().BlockDirectionPointInLine());
    if (!IsHorizontalWritingMode())
      adjusted_point = adjusted_point.TransposedPoint();
    return closest_box->GetLineLayoutItem().PositionForPoint(
        FlipForWritingMode(adjusted_point));
  }

  if (last_root_box_with_children) {
    // We hit this case for Mac behavior when the Y coordinate is below the last
    // box.
    DCHECK(move_caret_to_boundary);
    if (const InlineBox* logically_last_box =
            last_root_box_with_children->GetLogicalEndNonPseudoBox()) {
      // TODO(layout-dev): Change |PositionForBox()| to take |const InlineBox*|.
      return PositionWithAffinity(
          PositionForBox(const_cast<InlineBox*>(logically_last_box), false));
    }
  }

  // Can't reach this. We have a root line box, but it has no kids.
  // FIXME: This should NOTREACHED(), but clicking on placeholder text
  // seems to hit this code path.
  return CreatePositionWithAffinity(0);
}

bool LayoutBlockFlow::ShouldMoveCaretToHorizontalBoundaryWhenPastTopOrBottom()
    const {
  NOT_DESTROYED();
  return GetDocument()
      .GetFrame()
      ->GetEditor()
      .Behavior()
      .ShouldMoveCaretToHorizontalBoundaryWhenPastTopOrBottom();
}

#if DCHECK_IS_ON()

void LayoutBlockFlow::ShowLineTreeAndMark(const InlineBox* marked_box1,
                                          const char* marked_label1,
                                          const InlineBox* marked_box2,
                                          const char* marked_label2,
                                          const LayoutObject* obj) const {
  NOT_DESTROYED();
  if (getenv("RUNNING_UNDER_RR")) {
    // Printing timestamps requires an IPC to get the local time, which
    // does not work in an rr replay session. Just disable timestamp printing,
    // which we don't care about anyway.
    logging::SetLogItems(true, true, false, false);
  }

  StringBuilder string_blockflow;
  DumpLayoutObject(string_blockflow, true, kShowTreeCharacterOffset);
  for (const RootInlineBox* root = FirstRootBox(); root;
       root = root->NextRootBox()) {
    root->DumpLineTreeAndMark(string_blockflow, marked_box1, marked_label1,
                              marked_box2, marked_label2, obj, 1);
  }
  DLOG(INFO) << "\n" << string_blockflow.ToString().Utf8();
}

#endif

void LayoutBlockFlow::AddOutlineRects(
    Vector<PhysicalRect>& rects,
    OutlineInfo* info,
    const PhysicalOffset& additional_offset,
    NGOutlineType include_block_overflows) const {
  NOT_DESTROYED();

  LayoutBlock::AddOutlineRects(rects, info, additional_offset,
                               include_block_overflows);

  if (ShouldIncludeBlockVisualOverflow(include_block_overflows) &&
      !HasNonVisibleOverflow() && !HasControlClip()) {
    for (RootInlineBox* curr = FirstRootBox(); curr;
         curr = curr->NextRootBox()) {
      LayoutUnit flipped_left = curr->X();
      LayoutUnit flipped_right = curr->X() + curr->Width();
      LayoutUnit top = curr->Y();
      LayoutUnit bottom = curr->Y() + curr->Height();
      if (IsHorizontalWritingMode()) {
        top = std::max(curr->LineTop(), top);
        bottom = std::min(curr->LineBottom(), bottom);
      } else {
        flipped_left = std::max(curr->LineTop(), flipped_left);
        flipped_right = std::min(curr->LineBottom(), flipped_right);
      }
      LayoutRect rect(flipped_left, top, flipped_right - flipped_left,
                      bottom - top);
      if (!rect.IsEmpty()) {
        PhysicalRect physical_rect = FlipForWritingMode(rect);
        physical_rect.Move(additional_offset);
        rects.push_back(physical_rect);
      }
    }
  }
}

void LayoutBlockFlow::InvalidateDisplayItemClients(
    PaintInvalidationReason invalidation_reason) const {
  NOT_DESTROYED();
  BlockFlowPaintInvalidator(*this).InvalidateDisplayItemClients(
      invalidation_reason);
}

LayoutBlockFlow::LayoutBlockFlowRareData::LayoutBlockFlowRareData(
    const LayoutBlockFlow* block) {}

LayoutBlockFlow::LayoutBlockFlowRareData::~LayoutBlockFlowRareData() = default;

void LayoutBlockFlow::LayoutBlockFlowRareData::Trace(Visitor* visitor) const {
  visitor->Trace(multi_column_flow_thread_);
  visitor->Trace(offset_mapping_);
}

void LayoutBlockFlow::ClearOffsetMappingIfNeeded() {
  NOT_DESTROYED();
  DCHECK(!IsLayoutNGObject());
  if (!rare_data_)
    return;
  rare_data_->offset_mapping_.Clear();
}

const NGOffsetMapping* LayoutBlockFlow::GetOffsetMapping() const {
  NOT_DESTROYED();
  DCHECK(!IsLayoutNGObject());
  CHECK(!SelfNeedsLayout());
  CHECK(!NeedsLayout() || ChildLayoutBlockedByDisplayLock());
  return rare_data_ ? rare_data_->offset_mapping_ : nullptr;
}

void LayoutBlockFlow::SetOffsetMapping(NGOffsetMapping* offset_mapping) {
  NOT_DESTROYED();
  DCHECK(!IsLayoutNGObject());
  DCHECK(offset_mapping);
  EnsureRareData().offset_mapping_ = offset_mapping;
}

}  // namespace blink
