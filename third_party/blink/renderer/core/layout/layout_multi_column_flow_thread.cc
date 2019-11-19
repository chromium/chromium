/*
 * Copyright (C) 2012 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS IN..0TERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF  ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/layout/layout_multi_column_flow_thread.h"

#include "third_party/blink/renderer/core/layout/layout_multi_column_set.h"
#include "third_party/blink/renderer/core/layout/layout_multi_column_spanner_placeholder.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/layout/multi_column_fragmentainer_group.h"
#include "third_party/blink/renderer/core/layout/view_fragmentation_context.h"

namespace blink {

#if DCHECK_IS_ON()
const LayoutBox* LayoutMultiColumnFlowThread::style_changed_box_;
#endif
bool LayoutMultiColumnFlowThread::could_contain_spanners_;
bool LayoutMultiColumnFlowThread::toggle_spanners_if_needed_;

LayoutMultiColumnFlowThread::LayoutMultiColumnFlowThread()
    : last_set_worked_on_(nullptr),
      column_count_(1),
      column_heights_changed_(false),
      is_being_evacuated_(false) {
  SetIsInsideFlowThread(true);
}

LayoutMultiColumnFlowThread::~LayoutMultiColumnFlowThread() = default;

LayoutMultiColumnFlowThread* LayoutMultiColumnFlowThread::CreateAnonymous(
    Document& document,
    const ComputedStyle& parent_style) {
  LayoutMultiColumnFlowThread* layout_object =
      new LayoutMultiColumnFlowThread();
  layout_object->SetDocumentForAnonymous(&document);
  layout_object->SetStyle(ComputedStyle::CreateAnonymousStyleWithDisplay(
      parent_style, EDisplay::kBlock));
  return layout_object;
}

LayoutMultiColumnSet* LayoutMultiColumnFlowThread::FirstMultiColumnSet() const {
  for (LayoutObject* sibling = NextSibling(); sibling;
       sibling = sibling->NextSibling()) {
    if (sibling->IsLayoutMultiColumnSet())
      return ToLayoutMultiColumnSet(sibling);
  }
  return nullptr;
}

LayoutMultiColumnSet* LayoutMultiColumnFlowThread::LastMultiColumnSet() const {
  for (LayoutObject* sibling = MultiColumnBlockFlow()->LastChild(); sibling;
       sibling = sibling->PreviousSibling()) {
    if (sibling->IsLayoutMultiColumnSet())
      return ToLayoutMultiColumnSet(sibling);
  }
  return nullptr;
}

static inline bool IsMultiColumnContainer(const LayoutObject& object) {
  auto* block_flow = DynamicTo<LayoutBlockFlow>(object);
  if (!block_flow)
    return false;
  return block_flow->MultiColumnFlowThread();
}

// Return true if there's nothing that prevents the specified object from being
// in the ancestor chain between some column spanner and its containing multicol
// container. A column spanner needs the multicol container to be its containing
// block, so that the spanner is able to escape the flow thread. (Everything
// contained by the flow thread is split into columns, but this is precisely
// what shouldn't be done to a spanner, since it's supposed to span all
// columns.)
//
// We require that the parent of the spanner participate in the block formatting
// context established by the multicol container (i.e. that there are no BFCs or
// other formatting contexts in-between). We also require that there be no
// transforms, since transforms insist on being in the containing block chain
// for everything inside it, which conflicts with a spanners's need to have the
// multicol container as its direct containing block. We may also not put
// spanners inside objects that don't support fragmentation.
static inline bool CanContainSpannerInParentFragmentationContext(
    const LayoutObject& object) {
  const auto* block_flow = DynamicTo<LayoutBlockFlow>(object);
  if (!block_flow)
    return false;
  return !block_flow->CreatesNewFormattingContext() &&
         !block_flow->StyleRef().CanContainFixedPositionObjects(false) &&
         block_flow->GetPaginationBreakability() != LayoutBox::kForbidBreaks &&
         !IsMultiColumnContainer(*block_flow);
}

static inline bool HasAnyColumnSpanners(
    const LayoutMultiColumnFlowThread& flow_thread) {
  LayoutBox* first_box = flow_thread.FirstMultiColumnBox();
  return first_box && (first_box != flow_thread.LastMultiColumnBox() ||
                       first_box->IsLayoutMultiColumnSpannerPlaceholder());
}

// Find the next layout object that has the multicol container in its containing
// block chain, skipping nested multicol containers.
static LayoutObject* NextInPreOrderAfterChildrenSkippingOutOfFlow(
    LayoutMultiColumnFlowThread* flow_thread,
    LayoutObject* descendant) {
  DCHECK(descendant->IsDescendantOf(flow_thread));
  LayoutObject* object = descendant->NextInPreOrderAfterChildren(flow_thread);
  while (object) {
    // Walk through the siblings and find the first one which is either in-flow
    // or has this flow thread as its containing block flow thread.
    if (!object->IsOutOfFlowPositioned())
      break;
    if (object->ContainingBlock()->FlowThreadContainingBlock() == flow_thread) {
      // This out-of-flow object is still part of the flow thread, because its
      // containing block (probably relatively positioned) is part of the flow
      // thread.
      break;
    }
    object = object->NextInPreOrderAfterChildren(flow_thread);
  }
  if (!object)
    return nullptr;
#if DCHECK_IS_ON()
  // Make sure that we didn't stumble into an inner multicol container.
  for (LayoutObject* walker = object->Parent(); walker && walker != flow_thread;
       walker = walker->Parent())
    DCHECK(!IsMultiColumnContainer(*walker));
#endif
  return object;
}

// Find the previous layout object that has the multicol container in its
// containing block chain, skipping nested multicol containers.
static LayoutObject* PreviousInPreOrderSkippingOutOfFlow(
    LayoutMultiColumnFlowThread* flow_thread,
    LayoutObject* descendant) {
  DCHECK(descendant->IsDescendantOf(flow_thread));
  LayoutObject* object = descendant->PreviousInPreOrder(flow_thread);
  while (object && object != flow_thread) {
    if (object->IsColumnSpanAll()) {
      LayoutMultiColumnFlowThread* placeholder_flow_thread =
          ToLayoutBox(object)->SpannerPlaceholder()->FlowThread();
      if (placeholder_flow_thread == flow_thread)
        break;
      // We're inside an inner multicol container. We have no business there.
      // Continue on the outside.
      object = placeholder_flow_thread->Parent();
      DCHECK(object->IsDescendantOf(flow_thread));
      continue;
    }
    if (object->FlowThreadContainingBlock() == flow_thread) {
      LayoutObject* ancestor;
      for (ancestor = object->Parent();; ancestor = ancestor->Parent()) {
        if (ancestor == flow_thread)
          return object;
        if (IsMultiColumnContainer(*ancestor)) {
          // We're inside an inner multicol container. We have no business
          // there.
          break;
        }
      }
      object = ancestor;
      DCHECK(ancestor->IsDescendantOf(flow_thread));
      continue;  // Continue on the outside of the inner flow thread.
    }
    // We're inside something that's out-of-flow. Keep looking upwards and
    // backwards in the tree.
    object = object->PreviousInPreOrder(flow_thread);
  }
  if (!object || object == flow_thread)
    return nullptr;
#if DCHECK_IS_ON()
  // Make sure that we didn't stumble into an inner multicol container.
  for (LayoutObject* walker = object->Parent(); walker && walker != flow_thread;
       walker = walker->Parent())
    DCHECK(!IsMultiColumnContainer(*walker));
#endif
  return object;
}

static LayoutObject* FirstLayoutObjectInSet(
    LayoutMultiColumnSet* multicol_set) {
  LayoutBox* sibling = multicol_set->PreviousSiblingMultiColumnBox();
  if (!sibling)
    return multicol_set->FlowThread()->FirstChild();
  // Adjacent column content sets should not occur. We would have no way of
  // figuring out what each of them contains then.
  DCHECK(sibling->IsLayoutMultiColumnSpannerPlaceholder());
  LayoutBox* spanner = ToLayoutMultiColumnSpannerPlaceholder(sibling)
                           ->LayoutObjectInFlowThread();
  return NextInPreOrderAfterChildrenSkippingOutOfFlow(
      multicol_set->MultiColumnFlowThread(), spanner);
}

static LayoutObject* LastLayoutObjectInSet(LayoutMultiColumnSet* multicol_set) {
  LayoutBox* sibling = multicol_set->NextSiblingMultiColumnBox();
  // By right we should return lastLeafChild() here, but the caller doesn't
  // care, so just return nullptr.
  if (!sibling)
    return nullptr;
  // Adjacent column content sets should not occur. We would have no way of
  // figuring out what each of them contains then.
  DCHECK(sibling->IsLayoutMultiColumnSpannerPlaceholder());
  LayoutBox* spanner = ToLayoutMultiColumnSpannerPlaceholder(sibling)
                           ->LayoutObjectInFlowThread();
  return PreviousInPreOrderSkippingOutOfFlow(
      multicol_set->MultiColumnFlowThread(), spanner);
}

LayoutMultiColumnSet* LayoutMultiColumnFlowThread::MapDescendantToColumnSet(
    LayoutObject* layout_object) const {
  // Should not be used for spanners or content inside them.
  DCHECK(!ContainingColumnSpannerPlaceholder(layout_object));
  DCHECK_NE(layout_object, this);
  DCHECK(layout_object->IsDescendantOf(this));
  // Out-of-flow objects don't belong in column sets.
  DCHECK(layout_object->ContainingBlock()->IsDescendantOf(this));
  DCHECK_EQ(layout_object->FlowThreadContainingBlock(), this);
  DCHECK(!layout_object->IsLayoutMultiColumnSet());
  DCHECK(!layout_object->IsLayoutMultiColumnSpannerPlaceholder());
  LayoutMultiColumnSet* multicol_set = FirstMultiColumnSet();
  if (!multicol_set)
    return nullptr;
  if (!multicol_set->NextSiblingMultiColumnSet())
    return multicol_set;

  // This is potentially SLOW! But luckily very uncommon. You would have to
  // dynamically insert a spanner into the middle of column contents to need
  // this.
  for (; multicol_set;
       multicol_set = multicol_set->NextSiblingMultiColumnSet()) {
    LayoutObject* first_layout_object = FirstLayoutObjectInSet(multicol_set);
    LayoutObject* last_layout_object = LastLayoutObjectInSet(multicol_set);
    DCHECK(first_layout_object);

    for (LayoutObject* walker = first_layout_object; walker;
         walker = walker->NextInPreOrder(this)) {
      if (walker == layout_object)
        return multicol_set;
      if (walker == last_layout_object)
        break;
    }
  }

  return nullptr;
}

LayoutMultiColumnSpannerPlaceholder*
LayoutMultiColumnFlowThread::ContainingColumnSpannerPlaceholder(
    const LayoutObject* descendant) const {
  DCHECK(descendant->IsDescendantOf(this));

  if (!HasAnyColumnSpanners(*this))
    return nullptr;

  // We have spanners. See if the layoutObject in question is one or inside of
  // one then.
  for (const LayoutObject* ancestor = descendant; ancestor && ancestor != this;
       ancestor = ancestor->Parent()) {
    if (LayoutMultiColumnSpannerPlaceholder* placeholder =
            ancestor->SpannerPlaceholder())
      return placeholder;
  }
  return nullptr;
}

void LayoutMultiColumnFlowThread::Populate() {
  LayoutBlockFlow* multicol_container = MultiColumnBlockFlow();
  DCHECK(!NextSibling());
  // Reparent children preceding the flow thread into the flow thread. It's
  // multicol content now. At this point there's obviously nothing after the
  // flow thread, but layoutObjects (column sets and spanners) will be inserted
  // there as we insert elements into the flow thread.
  multicol_container->RemoveFloatingObjectsFromDescendants();
  multicol_container->MoveChildrenTo(this, multicol_container->FirstChild(),
                                     this, true);
}

void LayoutMultiColumnFlowThread::EvacuateAndDestroy() {
  LayoutBlockFlow* multicol_container = MultiColumnBlockFlow();
  is_being_evacuated_ = true;

  // Remove all sets and spanners.
  while (LayoutBox* column_box = FirstMultiColumnBox()) {
    DCHECK(column_box->IsAnonymous());
    column_box->Destroy();
  }

  DCHECK(!PreviousSibling());
  DCHECK(!NextSibling());

  // Finally we can promote all flow thread's children. Before we move them to
  // the flow thread's container, we need to unregister the flow thread, so that
  // they aren't just re-added again to the flow thread that we're trying to
  // empty.
  multicol_container->ResetMultiColumnFlowThread();
  MoveAllChildrenIncludingFloatsTo(multicol_container, true);

  // We used to manually nuke the line box tree here, but that should happen
  // automatically when moving children around (the code above).
  DCHECK(!FirstLineBox());

  Destroy();
}

LayoutUnit LayoutMultiColumnFlowThread::MaxColumnLogicalHeight() const {
  if (column_height_available_) {
    // If height is non-auto, it's already constrained against max-height as
    // well. Just return it.
    return column_height_available_;
  }
  const LayoutBlockFlow* multicol_block = MultiColumnBlockFlow();
  const Length& logical_max_height =
      multicol_block->StyleRef().LogicalMaxHeight();
  if (!logical_max_height.IsMaxSizeNone()) {
    LayoutUnit resolved_logical_max_height =
        multicol_block->ComputeContentLogicalHeight(
            kMaxSize, logical_max_height, LayoutUnit(-1));
    if (resolved_logical_max_height != -1)
      return resolved_logical_max_height;
  }
  return LayoutUnit::Max();
}

LayoutUnit LayoutMultiColumnFlowThread::TallestUnbreakableLogicalHeight(
    LayoutUnit offset_in_flow_thread) const {
  if (LayoutMultiColumnSet* multicol_set = ColumnSetAtBlockOffset(
          offset_in_flow_thread, kAssociateWithLatterPage))
    return multicol_set->TallestUnbreakableLogicalHeight();
  return LayoutUnit();
}

LayoutSize LayoutMultiColumnFlowThread::ColumnOffset(
    const LayoutPoint& point) const {
  return FlowThreadTranslationAtPoint(point,
                                      CoordinateSpaceConversion::kContaining);
}

bool LayoutMultiColumnFlowThread::NeedsNewWidth() const {
  LayoutUnit new_width;
  unsigned dummy_column_count;  // We only care if used column-width changes.
  CalculateColumnCountAndWidth(new_width, dummy_column_count);
  return new_width != LogicalWidth();
}

bool LayoutMultiColumnFlowThread::IsPageLogicalHeightKnown() const {
  return all_columns_have_known_height_;
}

bool LayoutMultiColumnFlowThread::MayHaveNonUniformPageLogicalHeight() const {
  const LayoutMultiColumnSet* column_set = FirstMultiColumnSet();
  if (!column_set)
    return false;
  if (column_set->NextSiblingMultiColumnSet())
    return true;
  return EnclosingFragmentationContext();
}

LayoutSize LayoutMultiColumnFlowThread::FlowThreadTranslationAtOffset(
    LayoutUnit offset_in_flow_thread,
    PageBoundaryRule rule,
    CoordinateSpaceConversion mode) const {
  if (!HasValidColumnSetInfo())
    return LayoutSize(0, 0);
  LayoutMultiColumnSet* column_set =
      ColumnSetAtBlockOffset(offset_in_flow_thread, rule);
  if (!column_set)
    return LayoutSize(0, 0);
  return column_set->FlowThreadTranslationAtOffset(offset_in_flow_thread, rule,
                                                   mode);
}

LayoutSize LayoutMultiColumnFlowThread::FlowThreadTranslationAtPoint(
    const LayoutPoint& flow_thread_point,
    CoordinateSpaceConversion mode) const {
  LayoutPoint flipped_point = DeprecatedFlipForWritingMode(flow_thread_point);
  LayoutUnit block_offset =
      IsHorizontalWritingMode() ? flipped_point.Y() : flipped_point.X();

  // If block direction is flipped, points at a column boundary belong in the
  // former column, not the latter.
  PageBoundaryRule rule = HasFlippedBlocksWritingMode()
                              ? kAssociateWithFormerPage
                              : kAssociateWithLatterPage;

  return FlowThreadTranslationAtOffset(block_offset, rule, mode);
}

LayoutPoint LayoutMultiColumnFlowThread::FlowThreadPointToVisualPoint(
    const LayoutPoint& flow_thread_point) const {
  return flow_thread_point +
         FlowThreadTranslationAtPoint(flow_thread_point,
                                      CoordinateSpaceConversion::kVisual);
}

LayoutPoint LayoutMultiColumnFlowThread::VisualPointToFlowThreadPoint(
    const LayoutPoint& visual_point) const {
  LayoutUnit block_offset =
      IsHorizontalWritingMode() ? visual_point.Y() : visual_point.X();
  const LayoutMultiColumnSet* column_set = nullptr;
  for (const LayoutMultiColumnSet* candidate = FirstMultiColumnSet(); candidate;
       candidate = candidate->NextSiblingMultiColumnSet()) {
    column_set = candidate;
    if (candidate->LogicalBottom() > block_offset)
      break;
  }
  return column_set ? column_set->VisualPointToFlowThreadPoint(ToLayoutPoint(
                          visual_point + Location() - column_set->Location()))
                    : visual_point;
}

LayoutUnit LayoutMultiColumnFlowThread::InlineBlockBaseline(
    LineDirectionMode line_direction) const {
  LayoutUnit baseline_in_flow_thread =
      LayoutFlowThread::InlineBlockBaseline(line_direction);
  LayoutMultiColumnSet* column_set =
      ColumnSetAtBlockOffset(baseline_in_flow_thread, kAssociateWithLatterPage);
  if (!column_set)
    return baseline_in_flow_thread;
  return LayoutUnit(
      (baseline_in_flow_thread -
       column_set->PageLogicalTopForOffset(baseline_in_flow_thread))
          .Ceil());
}

LayoutMultiColumnSet* LayoutMultiColumnFlowThread::ColumnSetAtBlockOffset(
    LayoutUnit offset,
    PageBoundaryRule page_boundary_rule) const {
  LayoutMultiColumnSet* column_set = last_set_worked_on_;
  if (column_set) {
    // Layout in progress. We are calculating the set heights as we speak, so
    // the column set range information is not up to date.
    while (column_set->LogicalTopInFlowThread() > offset) {
      // Sometimes we have to use a previous set. This happens when we're
      // working with a block that contains a spanner (so that there's a column
      // set both before and after the spanner, and both sets contain said
      // block).
      LayoutMultiColumnSet* previous_set =
          column_set->PreviousSiblingMultiColumnSet();
      if (!previous_set)
        break;
      column_set = previous_set;
    }
  } else {
    DCHECK(!column_sets_invalidated_);
    if (multi_column_set_list_.IsEmpty())
      return nullptr;
    if (offset < LayoutUnit()) {
      column_set = multi_column_set_list_.front();
    } else {
      MultiColumnSetSearchAdapter adapter(offset);
      multi_column_set_interval_tree_
          .AllOverlapsWithAdapter<MultiColumnSetSearchAdapter>(adapter);

      // If no set was found, the offset is in the flow thread overflow.
      if (!adapter.Result() && !multi_column_set_list_.IsEmpty())
        column_set = multi_column_set_list_.back();
      else
        column_set = adapter.Result();
    }
  }
  if (page_boundary_rule == kAssociateWithFormerPage && column_set &&
      offset == column_set->LogicalTopInFlowThread()) {
    // The column set that we found starts at the exact same flow thread offset
    // as we specified. Since we are to associate offsets at boundaries with the
    // former fragmentainer, the fragmentainer we're looking for is in the
    // previous column set.
    if (LayoutMultiColumnSet* previous_set =
            column_set->PreviousSiblingMultiColumnSet())
      column_set = previous_set;
  }
  // Avoid returning zero-height column sets, if possible. We found a column set
  // based on a flow thread coordinate. If multiple column sets share that
  // coordinate (because we have zero-height column sets between column
  // spanners, for instance), look for one that has a height. Also look ahead to
  // find a set that actually contains the coordinate. Note that when we do this
  // during layout, it means that we might return a column set that hasn't got
  // its flow thread boundaries updated yet (and thus using those from the
  // previous layout), but that's the best we can do when our engine doesn't
  // actually understand fragmentation. This may happen when there's a float
  // that's split into multiple fragments because of column spanners, and we
  // still perform all its layout at the position before the first spanner in
  // question (i.e. where only the first fragment is supposed to be laid out).
  for (LayoutMultiColumnSet* walker = column_set; walker;
       walker = walker->NextSiblingMultiColumnSet()) {
    if (!walker->IsPageLogicalHeightKnown())
      continue;
    if (page_boundary_rule == kAssociateWithFormerPage) {
      if (walker->LogicalTopInFlowThread() < offset &&
          walker->LogicalBottomInFlowThread() >= offset)
        return walker;
    } else if (walker->LogicalTopInFlowThread() <= offset &&
               walker->LogicalBottomInFlowThread() > offset) {
      return walker;
    }
  }
  return column_set;
}

void LayoutMultiColumnFlowThread::LayoutColumns(
    SubtreeLayoutScope& layout_scope) {
  // Since we ended up here, it means that the multicol container (our parent)
  // needed layout. Since contents of the multicol container are diverted to the
  // flow thread, the flow thread needs layout as well.
  layout_scope.SetChildNeedsLayout(this);

  CalculateColumnHeightAvailable();

  if (FragmentationContext* enclosing_fragmentation_context =
          EnclosingFragmentationContext()) {
    block_offset_in_enclosing_fragmentation_context_ =
        MultiColumnBlockFlow()->OffsetFromLogicalTopOfFirstPage();
    block_offset_in_enclosing_fragmentation_context_ +=
        MultiColumnBlockFlow()->BorderAndPaddingBefore();

    if (LayoutMultiColumnFlowThread* enclosing_flow_thread =
            enclosing_fragmentation_context->AssociatedFlowThread()) {
      if (LayoutMultiColumnSet* first_set = FirstMultiColumnSet()) {
        // Before we can start to lay out the contents of this multicol
        // container, we need to make sure that all ancestor multicol containers
        // have established a row to hold the first column contents of this
        // container (this multicol container may start at the beginning of a
        // new outer row). Without sufficient rows in all ancestor multicol
        // containers, we may use the wrong column height.
        LayoutUnit offset = block_offset_in_enclosing_fragmentation_context_ +
                            first_set->LogicalTopFromMulticolContentEdge();
        enclosing_flow_thread->AppendNewFragmentainerGroupIfNeeded(
            offset, kAssociateWithLatterPage);
      }
    }
  }

  // We'll start by assuming that all columns have some known height, and flip
  // it to false if we discover that this isn't the case.
  all_columns_have_known_height_ = true;

  for (LayoutBox* column_box = FirstMultiColumnBox(); column_box;
       column_box = column_box->NextSiblingMultiColumnBox()) {
    if (!column_box->IsLayoutMultiColumnSet()) {
      // No other type is expected.
      DCHECK(column_box->IsLayoutMultiColumnSpannerPlaceholder());
      continue;
    }
    LayoutMultiColumnSet* column_set = ToLayoutMultiColumnSet(column_box);
    layout_scope.SetChildNeedsLayout(column_set);
    if (!column_heights_changed_) {
      // This is the initial layout pass. We need to reset the column height,
      // because contents typically have changed.
      column_set->ResetColumnHeight();
    }
    if (all_columns_have_known_height_ &&
        !column_set->IsPageLogicalHeightKnown()) {
      // If any of the column sets requires a layout pass before it has any
      // clue about its height, we cannot fragment in this pass, just measure
      // the block sizes.
      all_columns_have_known_height_ = false;
    }
    // Since column sets are regular block flow objects, and their position is
    // changed in regular block layout code (with no means for the multicol code
    // to notice unless we add hooks there), store the previous position now. If
    // it changes in the imminent layout pass, we may have to rebalance its
    // columns.
    column_set->StoreOldPosition();
  }

  column_heights_changed_ = false;
  InvalidateColumnSets();
  UpdateLayout();
  ValidateColumnSets();
}

void LayoutMultiColumnFlowThread::ColumnRuleStyleDidChange() {
  for (LayoutMultiColumnSet* column_set = FirstMultiColumnSet(); column_set;
       column_set = column_set->NextSiblingMultiColumnSet()) {
    column_set->SetShouldDoFullPaintInvalidation(
        PaintInvalidationReason::kStyle);
  }
}

bool LayoutMultiColumnFlowThread::RemoveSpannerPlaceholderIfNoLongerValid(
    LayoutBox* spanner_object_in_flow_thread) {
  DCHECK(spanner_object_in_flow_thread->SpannerPlaceholder());
  if (DescendantIsValidColumnSpanner(spanner_object_in_flow_thread))
    return false;  // Still a valid spanner.

  // No longer a valid spanner. Get rid of the placeholder.
  DestroySpannerPlaceholder(
      spanner_object_in_flow_thread->SpannerPlaceholder());
  DCHECK(!spanner_object_in_flow_thread->SpannerPlaceholder());

  // We may have a new containing block, since we're no longer a spanner. Mark
  // it for relayout.
  spanner_object_in_flow_thread->ContainingBlock()
      ->SetNeedsLayoutAndPrefWidthsRecalc(
          layout_invalidation_reason::kColumnsChanged);

  // Now generate a column set for this ex-spanner, if needed and none is there
  // for us already.
  FlowThreadDescendantWasInserted(spanner_object_in_flow_thread);

  return true;
}

LayoutMultiColumnFlowThread* LayoutMultiColumnFlowThread::EnclosingFlowThread(
    AncestorSearchConstraint constraint) const {
  if (!MultiColumnBlockFlow()->IsInsideFlowThread())
    return nullptr;
  return ToLayoutMultiColumnFlowThread(
      LocateFlowThreadContainingBlockOf(*MultiColumnBlockFlow(), constraint));
}

FragmentationContext*
LayoutMultiColumnFlowThread::EnclosingFragmentationContext(
    AncestorSearchConstraint constraint) const {
  // If this multicol container is strictly unbreakable (due to having
  // scrollbars, for instance), it's also strictly unbreakable in any outer
  // fragmentation context. As such, what kind of fragmentation that goes on
  // inside this multicol container is completely opaque to the ancestors.
  if (constraint == kIsolateUnbreakableContainers &&
      MultiColumnBlockFlow()->GetPaginationBreakability() == kForbidBreaks)
    return nullptr;
  if (auto* enclosing_flow_thread = EnclosingFlowThread(constraint))
    return enclosing_flow_thread;
  return View()->FragmentationContext();
}

void LayoutMultiColumnFlowThread::AppendNewFragmentainerGroupIfNeeded(
    LayoutUnit offset_in_flow_thread,
    PageBoundaryRule page_boundary_rule) {
  LayoutMultiColumnSet* column_set =
      ColumnSetAtBlockOffset(offset_in_flow_thread, page_boundary_rule);
  if (!column_set->NewFragmentainerGroupsAllowed())
    return;

  if (column_set->NeedsNewFragmentainerGroupAt(offset_in_flow_thread,
                                               page_boundary_rule)) {
    // We should never create additional fragmentainer groups unless we're in a
    // nested fragmentation context.
    DCHECK(EnclosingFragmentationContext());

    // We have run out of columns here, so we need to add at least one more row
    // to hold more columns.
    LayoutMultiColumnFlowThread* enclosing_flow_thread =
        EnclosingFragmentationContext()->AssociatedFlowThread();
    do {
      if (enclosing_flow_thread) {
        // When we add a new row here, it implicitly means that we're inserting
        // another column in our enclosing multicol container. That in turn may
        // mean that we've run out of columns there too. Need to insert
        // additional rows in ancestral multicol containers before doing it in
        // the descendants, in order to get the height constraints right down
        // there.
        const MultiColumnFragmentainerGroup& last_row =
            column_set->LastFragmentainerGroup();
        // The top offset where where the new fragmentainer group will start in
        // this column set, converted to the coordinate space of the enclosing
        // multicol container.
        LayoutUnit logical_offset_in_outer =
            last_row.BlockOffsetInEnclosingFragmentationContext() +
            last_row.GroupLogicalHeight();
        enclosing_flow_thread->AppendNewFragmentainerGroupIfNeeded(
            logical_offset_in_outer, kAssociateWithLatterPage);
      }

      column_set->AppendNewFragmentainerGroup();
    } while (column_set->NeedsNewFragmentainerGroupAt(offset_in_flow_thread,
                                                      page_boundary_rule));
  }
}

void LayoutMultiColumnFlowThread::UpdateFromNG() {
  all_columns_have_known_height_ = true;
  for (LayoutBox* column_box = FirstMultiColumnBox(); column_box;
       column_box = column_box->NextSiblingMultiColumnBox()) {
    if (column_box->IsLayoutMultiColumnSet())
      ToLayoutMultiColumnSet(column_box)->UpdateFromNG();
    column_box->ClearNeedsLayout();
    column_box->UpdateAfterLayout();
  }
}

bool LayoutMultiColumnFlowThread::IsFragmentainerLogicalHeightKnown() {
  return IsPageLogicalHeightKnown();
}

LayoutUnit LayoutMultiColumnFlowThread::FragmentainerLogicalHeightAt(
    LayoutUnit block_offset) {
  DCHECK(IsPageLogicalHeightKnown());
  return PageLogicalHeightForOffset(block_offset);
}

LayoutUnit LayoutMultiColumnFlowThread::RemainingLogicalHeightAt(
    LayoutUnit block_offset) {
  DCHECK(IsPageLogicalHeightKnown());
  return PageRemainingLogicalHeightForOffset(block_offset,
                                             kAssociateWithLatterPage);
}

void LayoutMultiColumnFlowThread::CalculateColumnHeightAvailable() {
  // Calculate the non-auto content box height, or set it to 0 if it's auto. We
  // need to know this before layout, so that we can figure out where to insert
  // column breaks. We also treat LayoutView (which may be paginated, which uses
  // the multicol implementation) as having a fixed height, since its height is
  // deduced from the viewport height. We use computeLogicalHeight() to
  // calculate the content box height. That method will clamp against max-height
  // and min-height. Since we're now at the beginning of layout, and we don't
  // know the actual height of the content yet, only call that method when
  // height is definite, or we might fool ourselves into believing that columns
  // have a definite height when they in fact don't.
  LayoutBlockFlow* container = MultiColumnBlockFlow();
  LayoutUnit column_height;
  if (container->HasDefiniteLogicalHeight() || container->IsLayoutView()) {
    LogicalExtentComputedValues computed_values;
    container->ComputeLogicalHeight(LayoutUnit(), container->LogicalTop(),
                                    computed_values);
    column_height = computed_values.extent_ -
                    container->BorderAndPaddingLogicalHeight() -
                    container->ScrollbarLogicalHeight();
  }
  SetColumnHeightAvailable(std::max(column_height, LayoutUnit()));
}

void LayoutMultiColumnFlowThread::CalculateColumnCountAndWidth(
    LayoutUnit& width,
    unsigned& count) const {
  LayoutBlock* column_block = MultiColumnBlockFlow();
  const ComputedStyle* column_style = column_block->Style();
  LayoutUnit available_width = column_block->ContentLogicalWidth();
  LayoutUnit column_gap = ColumnGap(*column_style, available_width);
  LayoutUnit computed_column_width =
      max(LayoutUnit(1), LayoutUnit(column_style->ColumnWidth()));
  unsigned computed_column_count = max<int>(1, column_style->ColumnCount());

  DCHECK(!column_style->HasAutoColumnCount() ||
         !column_style->HasAutoColumnWidth());
  if (column_style->HasAutoColumnWidth() &&
      !column_style->HasAutoColumnCount()) {
    count = computed_column_count;
    width = ((available_width - ((count - 1) * column_gap)) / count)
                .ClampNegativeToZero();
  } else if (!column_style->HasAutoColumnWidth() &&
             column_style->HasAutoColumnCount()) {
    count = std::max(LayoutUnit(1), (available_width + column_gap) /
                                        (computed_column_width + column_gap))
                .ToUnsigned();
    width = ((available_width + column_gap) / count) - column_gap;
  } else {
    count = std::max(std::min(LayoutUnit(computed_column_count),
                              (available_width + column_gap) /
                                  (computed_column_width + column_gap)),
                     LayoutUnit(1))
                .ToUnsigned();
    width = ((available_width + column_gap) / count) - column_gap;
  }
}

LayoutUnit LayoutMultiColumnFlowThread::ColumnGap(const ComputedStyle& style,
                                                  LayoutUnit available_width) {
  if (style.ColumnGap().IsNormal()) {
    // "1em" is recommended as the normal gap setting. Matches <p> margins.
    return LayoutUnit(style.GetFontDescription().ComputedSize());
  }
  return ValueForLength(style.ColumnGap().GetLength(), available_width);
}

void LayoutMultiColumnFlowThread::CreateAndInsertMultiColumnSet(
    LayoutBox* insert_before) {
  LayoutBlockFlow* multicol_container = MultiColumnBlockFlow();
  LayoutMultiColumnSet* new_set = LayoutMultiColumnSet::CreateAnonymous(
      *this, multicol_container->StyleRef());
  multicol_container->LayoutBlock::AddChild(new_set, insert_before);
  InvalidateColumnSets();

  // We cannot handle immediate column set siblings (and there's no need for it,
  // either). There has to be at least one spanner separating them.
  DCHECK(!new_set->PreviousSiblingMultiColumnBox() ||
         !new_set->PreviousSiblingMultiColumnBox()->IsLayoutMultiColumnSet());
  DCHECK(!new_set->NextSiblingMultiColumnBox() ||
         !new_set->NextSiblingMultiColumnBox()->IsLayoutMultiColumnSet());
}

void LayoutMultiColumnFlowThread::CreateAndInsertSpannerPlaceholder(
    LayoutBox* spanner_object_in_flow_thread,
    LayoutObject* inserted_before_in_flow_thread) {
  LayoutBox* insert_before_column_box = nullptr;
  LayoutMultiColumnSet* set_to_split = nullptr;
  if (inserted_before_in_flow_thread) {
    // The spanner is inserted before something. Figure out what this entails.
    // If the next object is a spanner too, it means that we can simply insert a
    // new spanner placeholder in front of its placeholder.
    insert_before_column_box =
        inserted_before_in_flow_thread->SpannerPlaceholder();
    if (!insert_before_column_box) {
      // The next object isn't a spanner; it's regular column content. Examine
      // what comes right before us in the flow thread, then.
      LayoutObject* previous_layout_object =
          PreviousInPreOrderSkippingOutOfFlow(this,
                                              spanner_object_in_flow_thread);
      if (!previous_layout_object || previous_layout_object == this) {
        // The spanner is inserted as the first child of the multicol container,
        // which means that we simply insert a new spanner placeholder at the
        // beginning.
        insert_before_column_box = FirstMultiColumnBox();
      } else if (LayoutMultiColumnSpannerPlaceholder* previous_placeholder =
                     ContainingColumnSpannerPlaceholder(
                         previous_layout_object)) {
        // Before us is another spanner. We belong right after it then.
        insert_before_column_box =
            previous_placeholder->NextSiblingMultiColumnBox();
      } else {
        // We're inside regular column content with both feet. Find out which
        // column set this is. It needs to be split it into two sets, so that we
        // can insert a new spanner placeholder between them.
        set_to_split = MapDescendantToColumnSet(previous_layout_object);
        DCHECK_EQ(set_to_split,
                  MapDescendantToColumnSet(inserted_before_in_flow_thread));
        insert_before_column_box = set_to_split->NextSiblingMultiColumnBox();
        // We've found out which set that needs to be split. Now proceed to
        // inserting the spanner placeholder, and then insert a second column
        // set.
      }
    }
    DCHECK(set_to_split || insert_before_column_box);
  }

  LayoutBlockFlow* multicol_container = MultiColumnBlockFlow();
  LayoutMultiColumnSpannerPlaceholder* new_placeholder =
      LayoutMultiColumnSpannerPlaceholder::CreateAnonymous(
          multicol_container->StyleRef(), *spanner_object_in_flow_thread);
  DCHECK(!insert_before_column_box ||
         insert_before_column_box->Parent() == multicol_container);
  multicol_container->LayoutBlock::AddChild(new_placeholder,
                                            insert_before_column_box);
  spanner_object_in_flow_thread->SetSpannerPlaceholder(*new_placeholder);

  if (set_to_split)
    CreateAndInsertMultiColumnSet(insert_before_column_box);
}

void LayoutMultiColumnFlowThread::DestroySpannerPlaceholder(
    LayoutMultiColumnSpannerPlaceholder* placeholder) {
  if (LayoutBox* next_column_box = placeholder->NextSiblingMultiColumnBox()) {
    LayoutBox* previous_column_box =
        placeholder->PreviousSiblingMultiColumnBox();
    if (next_column_box && next_column_box->IsLayoutMultiColumnSet() &&
        previous_column_box && previous_column_box->IsLayoutMultiColumnSet()) {
      // Need to merge two column sets.
      next_column_box->Destroy();
      InvalidateColumnSets();
    }
  }
  placeholder->Destroy();
}

bool LayoutMultiColumnFlowThread::DescendantIsValidColumnSpanner(
    LayoutObject* descendant) const {
  // This method needs to behave correctly in the following situations:
  // - When the descendant doesn't have a spanner placeholder but should have
  //   one (return true).
  // - When the descendant doesn't have a spanner placeholder and still should
  //   not have one (return false).
  // - When the descendant has a spanner placeholder but should no longer have
  //   one (return false).
  // - When the descendant has a spanner placeholder and should still have one
  //   (return true).

  // We assume that we're inside the flow thread. This function is not to be
  // called otherwise.
  DCHECK(descendant->IsDescendantOf(this));

  // The spec says that column-span only applies to in-flow block-level
  // elements.
  if (descendant->StyleRef().GetColumnSpan() != EColumnSpan::kAll ||
      !descendant->IsBox() || descendant->IsInline() ||
      descendant->IsFloatingOrOutOfFlowPositioned())
    return false;

  if (!descendant->ContainingBlock()->IsLayoutBlockFlow()) {
    // Needs to be in a block-flow container, and not e.g. a table.
    return false;
  }

  // This looks like a spanner, but if we're inside something unbreakable or
  // something that establishes a new formatting context, it's not to be treated
  // as one.
  for (LayoutBox* ancestor = ToLayoutBox(descendant)->ParentBox(); ancestor;
       ancestor = ancestor->ContainingBlock()) {
    if (ancestor->IsLayoutFlowThread()) {
      DCHECK_EQ(ancestor, this);
      return true;
    }
    if (!CanContainSpannerInParentFragmentationContext(*ancestor))
      return false;
  }
  NOTREACHED();
  return false;
}

void LayoutMultiColumnFlowThread::AddColumnSetToThread(
    LayoutMultiColumnSet* column_set) {
  if (LayoutMultiColumnSet* next_set =
          column_set->NextSiblingMultiColumnSet()) {
    LayoutMultiColumnSetList::iterator it =
        multi_column_set_list_.find(next_set);
    DCHECK(it != multi_column_set_list_.end());
    multi_column_set_list_.InsertBefore(it, column_set);
  } else {
    multi_column_set_list_.insert(column_set);
  }
}

void LayoutMultiColumnFlowThread::WillBeRemovedFromTree() {
  // Detach all column sets from the flow thread. Cannot destroy them at this
  // point, since they are siblings of this object, and there may be pointers to
  // this object's sibling somewhere further up on the call stack.
  for (LayoutMultiColumnSet* column_set = FirstMultiColumnSet(); column_set;
       column_set = column_set->NextSiblingMultiColumnSet())
    column_set->DetachFromFlowThread();
  MultiColumnBlockFlow()->ResetMultiColumnFlowThread();
  LayoutFlowThread::WillBeRemovedFromTree();
}

void LayoutMultiColumnFlowThread::SkipColumnSpanner(
    LayoutBox* layout_object,
    LayoutUnit logical_top_in_flow_thread) {
  DCHECK(layout_object->IsColumnSpanAll());
  LayoutMultiColumnSpannerPlaceholder* placeholder =
      layout_object->SpannerPlaceholder();
  LayoutBox* previous_column_box = placeholder->PreviousSiblingMultiColumnBox();
  if (previous_column_box && previous_column_box->IsLayoutMultiColumnSet())
    ToLayoutMultiColumnSet(previous_column_box)
        ->EndFlow(logical_top_in_flow_thread);
  LayoutBox* next_column_box = placeholder->NextSiblingMultiColumnBox();
  if (next_column_box && next_column_box->IsLayoutMultiColumnSet()) {
    LayoutMultiColumnSet* next_set = ToLayoutMultiColumnSet(next_column_box);
    last_set_worked_on_ = next_set;
    next_set->BeginFlow(logical_top_in_flow_thread);
  }

  // We'll lay out of spanners after flow thread layout has finished (during
  // layout of the spanner placeholders). There may be containing blocks for
  // out-of-flow positioned descendants of the spanner in the flow thread, so
  // that out-of-flow objects inside the spanner will be laid out as part of
  // flow thread layout (even if the spanner itself won't). We need to add such
  // out-of-flow positioned objects to their containing blocks now, or they'll
  // never get laid out. Since it's non-trivial to determine if we need this,
  // and where such out-of-flow objects might be, just go through the whole
  // subtree.
  for (LayoutObject* descendant = layout_object->SlowFirstChild(); descendant;
       descendant = descendant->NextInPreOrder()) {
    if (descendant->IsBox() && descendant->IsOutOfFlowPositioned())
      descendant->ContainingBlock()->InsertPositionedObject(
          ToLayoutBox(descendant));
  }
}

bool LayoutMultiColumnFlowThread::FinishLayout() {
  all_columns_have_known_height_ = true;
  for (const auto* column_set = FirstMultiColumnSet(); column_set;
       column_set = column_set->NextSiblingMultiColumnSet()) {
    if (!column_set->IsPageLogicalHeightKnown()) {
      all_columns_have_known_height_ = false;
      break;
    }
  }
  return !ColumnHeightsChanged();
}

// When processing layout objects to remove or when processing layout objects
// that have just been inserted, certain types of objects should be skipped.
static bool ShouldSkipInsertedOrRemovedChild(
    LayoutMultiColumnFlowThread* flow_thread,
    const LayoutObject& child) {
  if (child.IsSVGChild()) {
    // Don't descend into SVG objects. What's in there is of no interest, and
    // there might even be a foreignObject there with column-span:all, which
    // doesn't apply to us.
    return true;
  }
  if (child.IsLayoutFlowThread()) {
    // Found an inner flow thread. We need to skip it and its descendants.
    return true;
  }
  if (child.IsLayoutMultiColumnSet() ||
      child.IsLayoutMultiColumnSpannerPlaceholder()) {
    // Column sets and spanner placeholders in a child multicol context don't
    // affect the parent flow thread.
    return true;
  }
  if (child.IsOutOfFlowPositioned() &&
      child.ContainingBlock()->FlowThreadContainingBlock() != flow_thread) {
    // Out-of-flow with its containing block on the outside of the multicol
    // container.
    return true;
  }
  return false;
}

void LayoutMultiColumnFlowThread::FlowThreadDescendantWasInserted(
    LayoutObject* descendant) {
  DCHECK(!is_being_evacuated_);
  // This method ensures that the list of column sets and spanner placeholders
  // reflects the multicol content after having inserted a descendant (or
  // descendant subtree). See the header file for more information. Go through
  // the subtree that was just inserted and create column sets (needed by
  // regular column content) and spanner placeholders (one needed by each
  // spanner) where needed.
  if (ShouldSkipInsertedOrRemovedChild(this, *descendant))
    return;
  LayoutObject* object_after_subtree =
      NextInPreOrderAfterChildrenSkippingOutOfFlow(this, descendant);
  LayoutObject* next;
  for (LayoutObject* layout_object = descendant; layout_object;
       layout_object = next) {
    if (layout_object != descendant &&
        ShouldSkipInsertedOrRemovedChild(this, *layout_object)) {
      next = layout_object->NextInPreOrderAfterChildren(descendant);
      continue;
    }
    next = layout_object->NextInPreOrder(descendant);
    if (ContainingColumnSpannerPlaceholder(layout_object))
      continue;  // Inside a column spanner. Nothing to do, then.
    if (DescendantIsValidColumnSpanner(layout_object)) {
      // This layoutObject is a spanner, so it needs to establish a spanner
      // placeholder.
      CreateAndInsertSpannerPlaceholder(ToLayoutBox(layout_object),
                                        object_after_subtree);
      continue;
    }
    // This layoutObject is regular column content (i.e. not a spanner). Create
    // a set if necessary.
    if (object_after_subtree) {
      if (LayoutMultiColumnSpannerPlaceholder* placeholder =
              object_after_subtree->SpannerPlaceholder()) {
        // If inserted right before a spanner, we need to make sure that there's
        // a set for us there.
        LayoutBox* previous = placeholder->PreviousSiblingMultiColumnBox();
        if (!previous || !previous->IsLayoutMultiColumnSet())
          CreateAndInsertMultiColumnSet(placeholder);
      } else {
        // Otherwise, since |objectAfterSubtree| isn't a spanner, it has to mean
        // that there's already a set for that content. We can use it for this
        // layoutObject too.
        DCHECK(MapDescendantToColumnSet(object_after_subtree));
        DCHECK_EQ(MapDescendantToColumnSet(layout_object),
                  MapDescendantToColumnSet(object_after_subtree));
      }
    } else {
      // Inserting at the end. Then we just need to make sure that there's a
      // column set at the end.
      LayoutBox* last_column_box = LastMultiColumnBox();
      if (!last_column_box || !last_column_box->IsLayoutMultiColumnSet())
        CreateAndInsertMultiColumnSet();
    }
  }
}

void LayoutMultiColumnFlowThread::FlowThreadDescendantWillBeRemoved(
    LayoutObject* descendant) {
  // This method ensures that the list of column sets and spanner placeholders
  // reflects the multicol content that we'll be left with after removal of a
  // descendant (or descendant subtree). See the header file for more
  // information. Removing content may mean that we need to remove column sets
  // and/or spanner placeholders.
  if (is_being_evacuated_)
    return;
  if (ShouldSkipInsertedOrRemovedChild(this, *descendant))
    return;
  bool had_containing_placeholder =
      ContainingColumnSpannerPlaceholder(descendant);
  bool processed_something = false;
  LayoutObject* next;
  // Remove spanner placeholders that are no longer needed, and merge column
  // sets around them.
  for (LayoutObject* layout_object = descendant; layout_object;
       layout_object = next) {
    if (layout_object != descendant &&
        ShouldSkipInsertedOrRemovedChild(this, *layout_object)) {
      next = layout_object->NextInPreOrderAfterChildren(descendant);
      continue;
    }
    processed_something = true;
    LayoutMultiColumnSpannerPlaceholder* placeholder =
        layout_object->SpannerPlaceholder();
    if (!placeholder) {
      next = layout_object->NextInPreOrder(descendant);
      continue;
    }
    next = layout_object->NextInPreOrderAfterChildren(
        descendant);  // It's a spanner. Its children are of no interest to us.
    DestroySpannerPlaceholder(placeholder);
  }
  if (had_containing_placeholder || !processed_something)
    return;  // No column content will be removed, so we can stop here.

  // Column content will be removed. Does this mean that we should destroy a
  // column set?
  LayoutMultiColumnSpannerPlaceholder* adjacent_previous_spanner_placeholder =
      nullptr;
  LayoutObject* previous_layout_object =
      PreviousInPreOrderSkippingOutOfFlow(this, descendant);
  if (previous_layout_object && previous_layout_object != this) {
    adjacent_previous_spanner_placeholder =
        ContainingColumnSpannerPlaceholder(previous_layout_object);
    if (!adjacent_previous_spanner_placeholder)
      return;  // Preceded by column content. Set still needed.
  }
  LayoutMultiColumnSpannerPlaceholder* adjacent_next_spanner_placeholder =
      nullptr;
  LayoutObject* next_layout_object =
      NextInPreOrderAfterChildrenSkippingOutOfFlow(this, descendant);
  if (next_layout_object) {
    adjacent_next_spanner_placeholder =
        ContainingColumnSpannerPlaceholder(next_layout_object);
    if (!adjacent_next_spanner_placeholder)
      return;  // Followed by column content. Set still needed.
  }
  // We have now determined that, with the removal of |descendant|, we should
  // remove a column set. Locate it and remove it. Do it without involving
  // mapDescendantToColumnSet(), as that might be very slow. Deduce the right
  // set from the spanner placeholders that we've already found.
  LayoutMultiColumnSet* column_set_to_remove;
  if (adjacent_next_spanner_placeholder) {
    column_set_to_remove = ToLayoutMultiColumnSet(
        adjacent_next_spanner_placeholder->PreviousSiblingMultiColumnBox());
    DCHECK(
        !adjacent_previous_spanner_placeholder ||
        column_set_to_remove ==
            adjacent_previous_spanner_placeholder->NextSiblingMultiColumnBox());
  } else if (adjacent_previous_spanner_placeholder) {
    column_set_to_remove = ToLayoutMultiColumnSet(
        adjacent_previous_spanner_placeholder->NextSiblingMultiColumnBox());
  } else {
    // If there were no adjacent spanners, it has to mean that there's only one
    // column set, since it's only spanners that may cause creation of
    // multiple sets.
    column_set_to_remove = FirstMultiColumnSet();
    DCHECK(column_set_to_remove);
    DCHECK(!column_set_to_remove->NextSiblingMultiColumnSet());
  }
  DCHECK(column_set_to_remove);
  column_set_to_remove->Destroy();
}

static inline bool NeedsToReinsertIntoFlowThread(
    const ComputedStyle& old_style,
    const ComputedStyle& new_style) {
  // If we've become (or are about to become) a container for absolutely
  // positioned descendants, or if we're no longer going to be one, we need to
  // re-evaluate the need for column sets. There may be out-of-flow descendants
  // further down that become part of the flow thread, or cease to be part of
  // the flow thread, because of this change.
  if (old_style.CanContainFixedPositionObjects(false) !=
      new_style.CanContainFixedPositionObjects(false))
    return true;
  return (old_style.HasInFlowPosition() &&
          new_style.GetPosition() == EPosition::kStatic) ||
         (new_style.HasInFlowPosition() &&
          old_style.GetPosition() == EPosition::kStatic);
}

static inline bool NeedsToRemoveFromFlowThread(const ComputedStyle& old_style,
                                               const ComputedStyle& new_style) {
  // This function is called BEFORE computed style update. If an in-flow
  // descendant goes out-of-flow, we may have to remove column sets and spanner
  // placeholders. Note that we may end up with false positives here, since some
  // out-of-flow descendants still need to be associated with a column set. This
  // is the case when the containing block of the soon-to-be out-of-flow
  // positioned descendant is contained by the same flow thread as the
  // descendant currently is inside. It's too early to check for that, though,
  // since the descendant at this point is still in-flow positioned. We'll
  // detect this and re-insert it into the flow thread when computed style has
  // been updated.
  return (new_style.HasOutOfFlowPosition() &&
          !old_style.HasOutOfFlowPosition()) ||
         NeedsToReinsertIntoFlowThread(old_style, new_style);
}

static inline bool NeedsToInsertIntoFlowThread(
    const LayoutMultiColumnFlowThread* flow_thread,
    const LayoutBox* descendant,
    const ComputedStyle& old_style,
    const ComputedStyle& new_style) {
  // This function is called AFTER computed style update. If an out-of-flow
  // descendant goes in-flow, we may have to insert column sets and spanner
  // placeholders.
  bool toggled_out_of_flow =
      new_style.HasOutOfFlowPosition() != old_style.HasOutOfFlowPosition();
  if (toggled_out_of_flow) {
    // If we're no longer out-of-flow, we definitely need the descendant to be
    // associated with a column set.
    if (!new_style.HasOutOfFlowPosition())
      return true;
    const auto* containing_flow_thread =
        descendant->ContainingBlock()->FlowThreadContainingBlock();
    // If an out-of-flow positioned descendant is still going to be contained by
    // this flow thread, the descendant needs to be associated with a column
    // set.
    if (containing_flow_thread == flow_thread)
      return true;
  }
  return NeedsToReinsertIntoFlowThread(old_style, new_style);
}

void LayoutMultiColumnFlowThread::FlowThreadDescendantStyleWillChange(
    LayoutBox* descendant,
    StyleDifference diff,
    const ComputedStyle& new_style) {
  toggle_spanners_if_needed_ = false;
  if (NeedsToRemoveFromFlowThread(descendant->StyleRef(), new_style)) {
    FlowThreadDescendantWillBeRemoved(descendant);
    return;
  }
#if DCHECK_IS_ON()
  style_changed_box_ = descendant;
#endif
  // Keep track of whether this object was of such a type that it could contain
  // column-span:all descendants. If the style change in progress changes this
  // state, we need to look for spanners to add or remove in the subtree of
  // |descendant|.
  toggle_spanners_if_needed_ = true;
  could_contain_spanners_ =
      CanContainSpannerInParentFragmentationContext(*descendant);
}

void LayoutMultiColumnFlowThread::FlowThreadDescendantStyleDidChange(
    LayoutBox* descendant,
    StyleDifference diff,
    const ComputedStyle& old_style) {
  bool toggle_spanners_if_needed = toggle_spanners_if_needed_;
  toggle_spanners_if_needed_ = false;

  if (NeedsToInsertIntoFlowThread(this, descendant, old_style,
                                  descendant->StyleRef())) {
    FlowThreadDescendantWasInserted(descendant);
    return;
  }
  if (DescendantIsValidColumnSpanner(descendant)) {
    // We went from being regular column content to becoming a spanner.
    DCHECK(!descendant->SpannerPlaceholder());

    // First remove this as regular column content. Note that this will walk the
    // entire subtree of |descendant|. There might be spanners there (which
    // won't be spanners anymore, since we're not allowed to nest spanners),
    // whose placeholders must die.
    FlowThreadDescendantWillBeRemoved(descendant);

    CreateAndInsertSpannerPlaceholder(
        descendant,
        NextInPreOrderAfterChildrenSkippingOutOfFlow(this, descendant));
    return;
  }

  if (!toggle_spanners_if_needed)
    return;
#if DCHECK_IS_ON()
  // Make sure that we were preceded by a call to
  // flowThreadDescendantStyleWillChange() with the same descendant as we have
  // now.
  DCHECK_EQ(style_changed_box_, descendant);
#endif

  if (could_contain_spanners_ !=
      CanContainSpannerInParentFragmentationContext(*descendant))
    ToggleSpannersInSubtree(descendant);
}

void LayoutMultiColumnFlowThread::ToggleSpannersInSubtree(
    LayoutBox* descendant) {
  DCHECK_NE(could_contain_spanners_,
            CanContainSpannerInParentFragmentationContext(*descendant));

  // If there are no spanners at all in this multicol container, there's no
  // need to look for any to remove.
  if (could_contain_spanners_ && !HasAnyColumnSpanners(*this))
    return;

  bool walk_children;
  for (LayoutObject* object = descendant->NextInPreOrder(descendant); object;
       object = walk_children
                    ? object->NextInPreOrder(descendant)
                    : object->NextInPreOrderAfterChildren(descendant)) {
    walk_children = false;
    if (!object->IsBox())
      continue;
    LayoutBox& box = ToLayoutBox(*object);
    if (could_contain_spanners_) {
      // Remove all spanners (turn them into regular column content), as we can
      // no longer contain them.
      if (box.IsColumnSpanAll()) {
        DestroySpannerPlaceholder(box.SpannerPlaceholder());
        continue;
      }
    } else if (DescendantIsValidColumnSpanner(object)) {
      // We can now contain spanners, and we found a candidate. Turn it into a
      // spanner, if it's not already one. We have to check if it's already a
      // spanner, because in some cases we incorrectly think that we need to
      // toggle spanners. One known case is when some ancestor changes
      // writing-mode (which is an inherited property). Writing mode roots
      // establish block formatting context (which means that there can be no
      // column spanners inside). When changing the style on one object in the
      // tree at a time, we're going to see writing mode roots that are not
      // going to remain writing mode roots when all objects have been updated
      // (because then all will have got the same writing mode).
      if (!box.IsColumnSpanAll()) {
        CreateAndInsertSpannerPlaceholder(
            &box, NextInPreOrderAfterChildrenSkippingOutOfFlow(this, &box));
      }
      continue;
    }
    walk_children = CanContainSpannerInParentFragmentationContext(box);
  }
}

void LayoutMultiColumnFlowThread::ComputePreferredLogicalWidths() {
  // The min/max intrinsic widths calculated really tell how much space elements
  // need when laid out inside the columns. In order to eventually end up with
  // the desired column width, we need to convert them to values pertaining to
  // the multicol container.
  auto* flow = MultiColumnBlockFlow();
  const ComputedStyle* multicol_style = flow->Style();
  LayoutUnit column_count(
      multicol_style->HasAutoColumnCount() ? 1 : multicol_style->ColumnCount());
  LayoutUnit gap_extra((column_count - 1) *
                       ColumnGap(*multicol_style, LayoutUnit()));

  if (flow->HasOverrideIntrinsicContentLogicalWidth()) {
    min_preferred_logical_width_ = max_preferred_logical_width_ =
        flow->OverrideIntrinsicContentLogicalWidth();
    ClearPreferredLogicalWidthsDirty();
  } else if (flow->ShouldApplySizeContainment()) {
    min_preferred_logical_width_ = max_preferred_logical_width_ = LayoutUnit();
    ClearPreferredLogicalWidthsDirty();
  } else {
    // Calculate and set new min_preferred_logical_width_ and
    // max_preferred_logical_width_.
    LayoutFlowThread::ComputePreferredLogicalWidths();
  }

  LayoutUnit column_width;
  if (multicol_style->HasAutoColumnWidth()) {
    min_preferred_logical_width_ =
        min_preferred_logical_width_ * column_count + gap_extra;
  } else {
    column_width = LayoutUnit(multicol_style->ColumnWidth());
    min_preferred_logical_width_ =
        std::min(min_preferred_logical_width_, column_width);
  }
  // Note that if column-count is auto here, we should resolve it to calculate
  // the maximum intrinsic width, instead of pretending that it's 1. The only
  // way to do that is by performing a layout pass, but this is not an
  // appropriate time or place for layout. The good news is that if height is
  // unconstrained and there are no explicit breaks, the resolved column-count
  // really should be 1.
  max_preferred_logical_width_ =
      std::max(max_preferred_logical_width_, column_width) * column_count +
      gap_extra;
}

void LayoutMultiColumnFlowThread::ComputeLogicalHeight(
    LayoutUnit logical_height,
    LayoutUnit logical_top,
    LogicalExtentComputedValues& computed_values) const {
  // We simply remain at our intrinsic height.
  computed_values.extent_ = logical_height;
  computed_values.position_ = logical_top;
}

void LayoutMultiColumnFlowThread::UpdateLogicalWidth() {
  LayoutUnit column_width;
  CalculateColumnCountAndWidth(column_width, column_count_);
  SetLogicalWidth(column_width);
}

void LayoutMultiColumnFlowThread::UpdateLayout() {
  DCHECK(!last_set_worked_on_);
  last_set_worked_on_ = FirstMultiColumnSet();
  if (last_set_worked_on_)
    last_set_worked_on_->BeginFlow(LayoutUnit());
  LayoutFlowThread::UpdateLayout();
  if (LayoutMultiColumnSet* last_set = LastMultiColumnSet()) {
    DCHECK_EQ(last_set, last_set_worked_on_);
    if (!last_set->NextSiblingMultiColumnSet()) {
      // Include trailing overflow in the last column set (also if the last set
      // is followed by one or more spanner placeholders). The idea is that we
      // will generate additional columns and pages to hold that overflow,
      // since people do write bad content like <body style="height:0px"> in
      // multi-column layouts.
      // TODO(mstensho): Once we support nested multicol, adding in overflow
      // here may result in the need for creating additional rows, since there
      // may not be enough space remaining in the currently last row.
      LayoutRect layout_rect = LayoutOverflowRect();
      LayoutUnit logical_bottom_in_flow_thread =
          IsHorizontalWritingMode() ? layout_rect.MaxY() : layout_rect.MaxX();
      DCHECK_GE(logical_bottom_in_flow_thread, LogicalHeight());
      last_set->EndFlow(logical_bottom_in_flow_thread);
    }
  }
  last_set_worked_on_ = nullptr;
}

void LayoutMultiColumnFlowThread::ContentWasLaidOut(
    LayoutUnit logical_bottom_in_flow_thread_after_pagination) {
  // Check if we need another fragmentainer group. If we've run out of columns
  // in the last fragmentainer group (column row), we need to insert another
  // fragmentainer group to hold more columns.

  // First figure out if there's any chance that we're nested at all. If we can
  // be sure that we're not, bail early. This code is run very often, and since
  // locating a containing flow thread has some cost (depending on tree depth),
  // avoid calling enclosingFragmentationContext() right away. This test may
  // give some false positives (hence the "mayBe"), if we're in an out-of-flow
  // subtree and have an outer multicol container that doesn't affect us, but
  // that's okay. We'll discover that further down the road when trying to
  // locate our enclosing flow thread for real.
  bool may_be_nested = MultiColumnBlockFlow()->IsInsideFlowThread() ||
                       View()->FragmentationContext();
  if (!may_be_nested)
    return;
  AppendNewFragmentainerGroupIfNeeded(
      logical_bottom_in_flow_thread_after_pagination, kAssociateWithFormerPage);
}

bool LayoutMultiColumnFlowThread::CanSkipLayout(const LayoutBox& root) const {
  // Objects containing spanners is all we need to worry about, so if there are
  // no spanners at all in this multicol container, we can just return the good
  // news right away.
  if (!HasAnyColumnSpanners(*this))
    return true;

  LayoutObject* next;
  for (const LayoutObject* object = &root; object; object = next) {
    if (object->IsColumnSpanAll()) {
      // A spanner potentially ends one fragmentainer group and begins a new
      // one, and thus determines the flow thread portion bottom and top of
      // adjacent fragmentainer groups. It's just too hard to guess these values
      // without laying out.
      return false;
    }
    if (CanContainSpannerInParentFragmentationContext(*object))
      next = object->NextInPreOrder(&root);
    else
      next = object->NextInPreOrderAfterChildren(&root);
  }
  return true;
}

MultiColumnLayoutState LayoutMultiColumnFlowThread::GetMultiColumnLayoutState()
    const {
  return MultiColumnLayoutState(last_set_worked_on_);
}

void LayoutMultiColumnFlowThread::RestoreMultiColumnLayoutState(
    const MultiColumnLayoutState& state) {
  last_set_worked_on_ = state.ColumnSet();
}

}  // namespace blink
