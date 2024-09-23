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

#include "base/not_fatal_until.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver.h"
#include "third_party/blink/renderer/core/layout/fragmentation_utils.h"
#include "third_party/blink/renderer/core/layout/geometry/writing_mode_converter.h"
#include "third_party/blink/renderer/core/layout/layout_multi_column_set.h"
#include "third_party/blink/renderer/core/layout/layout_multi_column_spanner_placeholder.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/layout/multi_column_fragmentainer_group.h"
#include "third_party/blink/renderer/core/layout/physical_box_fragment.h"

namespace blink {

#if DCHECK_IS_ON()
const LayoutBoxModelObject* LayoutMultiColumnFlowThread::style_changed_object_;
#endif
bool LayoutMultiColumnFlowThread::could_contain_spanners_;
bool LayoutMultiColumnFlowThread::toggle_spanners_if_needed_;

LayoutMultiColumnFlowThread::LayoutMultiColumnFlowThread()
    : last_set_worked_on_(nullptr),
      column_count_(1),
      is_being_evacuated_(false) {
  SetIsInsideFlowThread(true);
}

LayoutMultiColumnFlowThread::~LayoutMultiColumnFlowThread() = default;

void LayoutMultiColumnFlowThread::Trace(Visitor* visitor) const {
  visitor->Trace(last_set_worked_on_);
  LayoutFlowThread::Trace(visitor);
}

LayoutMultiColumnFlowThread* LayoutMultiColumnFlowThread::CreateAnonymous(
    Document& document,
    const ComputedStyle& parent_style) {
  LayoutMultiColumnFlowThread* layout_object =
      MakeGarbageCollected<LayoutMultiColumnFlowThread>();
  layout_object->SetDocumentForAnonymous(&document);
  layout_object->SetStyle(
      document.GetStyleResolver().CreateAnonymousStyleWithDisplay(
          parent_style, EDisplay::kBlock));
  return layout_object;
}

LayoutMultiColumnSet* LayoutMultiColumnFlowThread::FirstMultiColumnSet() const {
  NOT_DESTROYED();
  for (LayoutObject* sibling = NextSibling(); sibling;
       sibling = sibling->NextSibling()) {
    if (sibling->IsLayoutMultiColumnSet())
      return To<LayoutMultiColumnSet>(sibling);
  }
  return nullptr;
}

LayoutMultiColumnSet* LayoutMultiColumnFlowThread::LastMultiColumnSet() const {
  NOT_DESTROYED();
  for (LayoutObject* sibling = MultiColumnBlockFlow()->LastChild(); sibling;
       sibling = sibling->PreviousSibling()) {
    if (sibling->IsLayoutMultiColumnSet())
      return To<LayoutMultiColumnSet>(sibling);
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
// multicol container as its direct containing block. A transform is supposed to
// be a containing block for everything inside, including fixed-positioned
// elements. Letting spanners escape this containment seems strange. See
// https://github.com/w3c/csswg-drafts/issues/6805
// Finally, we may also not put spanners inside objects that don't support
// fragmentation.
bool LayoutMultiColumnFlowThread::CanContainSpannerInParentFragmentationContext(
    const LayoutObject& object) const {
  NOT_DESTROYED();
  const auto* block_flow = DynamicTo<LayoutBlockFlow>(object);
  if (!block_flow)
    return false;
  return !block_flow->CreatesNewFormattingContext() &&
         !block_flow->CanContainFixedPositionObjects() &&
         !block_flow->IsMonolithic() && !IsMultiColumnContainer(*block_flow);
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
          To<LayoutBox>(object)->SpannerPlaceholder()->FlowThread();
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
  CHECK(sibling->IsLayoutMultiColumnSpannerPlaceholder());
  LayoutBox* spanner = To<LayoutMultiColumnSpannerPlaceholder>(sibling)
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
  CHECK(sibling->IsLayoutMultiColumnSpannerPlaceholder());
  LayoutBox* spanner = To<LayoutMultiColumnSpannerPlaceholder>(sibling)
                           ->LayoutObjectInFlowThread();
  return PreviousInPreOrderSkippingOutOfFlow(
      multicol_set->MultiColumnFlowThread(), spanner);
}

LayoutMultiColumnSet* LayoutMultiColumnFlowThread::MapDescendantToColumnSet(
    LayoutObject* layout_object) const {
  NOT_DESTROYED();
  // Should not be used for spanners or content inside them.
  DCHECK(!ContainingColumnSpannerPlaceholder(layout_object));
  DCHECK_NE(layout_object, this);
  DCHECK(layout_object->IsDescendantOf(this));
  // Out-of-flow objects don't belong in column sets. DHCECK that the object is
  // contained by the flow thread, except for legends ("rendered" or
  // not). Although a rendered legend isn't part of the fragmentation context,
  // we'll let it contribute to creation of a column set, for the sake of
  // simplicity. Style and DOM changes may later on change which LEGEND child is
  // the rendered legend, and we don't want to keep track of that.
  DCHECK(layout_object->IsRenderedLegend() ||
         layout_object->ContainingBlock()->IsDescendantOf(this));
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
  NOT_DESTROYED();
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
  NOT_DESTROYED();
  LayoutBlockFlow* multicol_container = MultiColumnBlockFlow();
  DCHECK(!NextSibling());
  // Reparent children preceding the flow thread into the flow thread. It's
  // multicol content now. At this point there's obviously nothing after the
  // flow thread, but layoutObjects (column sets and spanners) will be inserted
  // there as we insert elements into the flow thread.
  multicol_container->MoveChildrenTo(this, multicol_container->FirstChild(),
                                     this, true);
}

void LayoutMultiColumnFlowThread::EvacuateAndDestroy() {
  NOT_DESTROYED();
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

  Destroy();
}

PhysicalOffset LayoutMultiColumnFlowThread::ColumnOffset(
    const PhysicalOffset& point) const {
  NOT_DESTROYED();
  return FlowThreadTranslationAtPoint(point);
}

bool LayoutMultiColumnFlowThread::IsPageLogicalHeightKnown() const {
  NOT_DESTROYED();
  return all_columns_have_known_height_;
}

PhysicalOffset LayoutMultiColumnFlowThread::FlowThreadTranslationAtOffset(
    LayoutUnit offset_in_flow_thread,
    PageBoundaryRule rule) const {
  NOT_DESTROYED();
  if (!HasValidColumnSetInfo())
    return PhysicalOffset();
  LayoutMultiColumnSet* column_set =
      ColumnSetAtBlockOffset(offset_in_flow_thread, rule);
  if (!column_set)
    return PhysicalOffset();
  return column_set->FlowThreadTranslationAtOffset(offset_in_flow_thread, rule);
}

PhysicalOffset LayoutMultiColumnFlowThread::FlowThreadTranslationAtPoint(
    const PhysicalOffset& flow_thread_point) const {
  NOT_DESTROYED();
  LayoutUnit block_offset = CreateWritingModeConverter()
                                .ToLogical(flow_thread_point, {})
                                .block_offset;

  // If block direction is flipped, points at a column boundary belong in the
  // former column, not the latter.
  PageBoundaryRule rule = HasFlippedBlocksWritingMode()
                              ? kAssociateWithFormerPage
                              : kAssociateWithLatterPage;

  return FlowThreadTranslationAtOffset(block_offset, rule);
}

PhysicalOffset LayoutMultiColumnFlowThread::VisualPointToFlowThreadPoint(
    const PhysicalOffset& visual_point) const {
  NOT_DESTROYED();
  WritingModeConverter converter(
      {StyleRef().GetWritingMode(), TextDirection::kLtr}, Size());
  LayoutUnit block_offset = converter.ToLogical(visual_point, {}).block_offset;
  const LayoutMultiColumnSet* column_set = nullptr;
  for (const LayoutMultiColumnSet* candidate = FirstMultiColumnSet(); candidate;
       candidate = candidate->NextSiblingMultiColumnSet()) {
    column_set = candidate;
    if (candidate->LogicalBottom() > block_offset)
      break;
  }
  if (!column_set) {
    return visual_point;
  }
  const PhysicalOffset flow_thread_offset = PhysicalLocation();
  const PhysicalOffset column_set_offset = column_set->PhysicalLocation();
  const PhysicalOffset point_in_set =
      visual_point + flow_thread_offset - column_set_offset;
  return converter.ToPhysical(
      column_set->VisualPointToFlowThreadPoint(point_in_set), {});
}

LayoutMultiColumnSet* LayoutMultiColumnFlowThread::ColumnSetAtBlockOffset(
    LayoutUnit offset,
    PageBoundaryRule page_boundary_rule) const {
  NOT_DESTROYED();
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
    if (multi_column_set_list_.empty())
      return nullptr;
    if (offset < LayoutUnit()) {
      column_set = multi_column_set_list_.front();
    } else {
      MultiColumnSetSearchAdapter adapter(offset);
      multi_column_set_interval_tree_
          .AllOverlapsWithAdapter<MultiColumnSetSearchAdapter>(adapter);

      // If no set was found, the offset is in the flow thread overflow.
      if (!adapter.Result() && !multi_column_set_list_.empty())
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

void LayoutMultiColumnFlowThread::ColumnRuleStyleDidChange() {
  NOT_DESTROYED();
  for (LayoutMultiColumnSet* column_set = FirstMultiColumnSet(); column_set;
       column_set = column_set->NextSiblingMultiColumnSet()) {
    column_set->SetShouldDoFullPaintInvalidation();
  }
}

bool LayoutMultiColumnFlowThread::RemoveSpannerPlaceholderIfNoLongerValid(
    LayoutBox* spanner_object_in_flow_thread) {
  NOT_DESTROYED();
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
      ->SetNeedsLayoutAndIntrinsicWidthsRecalc(
          layout_invalidation_reason::kColumnsChanged);

  // Now generate a column set for this ex-spanner, if needed and none is there
  // for us already.
  FlowThreadDescendantWasInserted(spanner_object_in_flow_thread);

  return true;
}

LayoutMultiColumnFlowThread* LayoutMultiColumnFlowThread::EnclosingFlowThread(
    AncestorSearchConstraint constraint) const {
  NOT_DESTROYED();
  if (!MultiColumnBlockFlow()->IsInsideFlowThread())
    return nullptr;
  return To<LayoutMultiColumnFlowThread>(
      LocateFlowThreadContainingBlockOf(*MultiColumnBlockFlow(), constraint));
}

void LayoutMultiColumnFlowThread::SetColumnCountFromNG(unsigned column_count) {
  NOT_DESTROYED();
  column_count_ = column_count;
}

void LayoutMultiColumnFlowThread::FinishLayoutFromNG(
    LayoutUnit flow_thread_offset) {
  NOT_DESTROYED();
  all_columns_have_known_height_ = true;
  for (LayoutBox* column_box = FirstMultiColumnBox(); column_box;
       column_box = column_box->NextSiblingMultiColumnBox()) {
    column_box->ClearNeedsLayout();
  }

  ValidateColumnSets();
  ClearNeedsLayout();
  last_set_worked_on_ = nullptr;
}

void LayoutMultiColumnFlowThread::CreateAndInsertMultiColumnSet(
    LayoutBox* insert_before) {
  NOT_DESTROYED();
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
  NOT_DESTROYED();
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
  NOT_DESTROYED();
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
  NOT_DESTROYED();
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
  for (LayoutBox* ancestor = To<LayoutBox>(descendant)->ParentBox(); ancestor;
       ancestor = ancestor->ContainingBlock()) {
    if (ancestor->IsLayoutFlowThread()) {
      DCHECK_EQ(ancestor, this);
      return true;
    }
    if (!CanContainSpannerInParentFragmentationContext(*ancestor))
      return false;
  }
  NOTREACHED_IN_MIGRATION();
  return false;
}

void LayoutMultiColumnFlowThread::AddColumnSetToThread(
    LayoutMultiColumnSet* column_set) {
  NOT_DESTROYED();
  if (LayoutMultiColumnSet* next_set =
          column_set->NextSiblingMultiColumnSet()) {
    LayoutMultiColumnSetList::iterator it =
        multi_column_set_list_.find(next_set);
    CHECK(it != multi_column_set_list_.end(), base::NotFatalUntil::M130);
    multi_column_set_list_.InsertBefore(it, column_set);
  } else {
    multi_column_set_list_.insert(column_set);
  }
}

void LayoutMultiColumnFlowThread::WillBeRemovedFromTree() {
  NOT_DESTROYED();
  // Detach all column sets from the flow thread. Cannot destroy them at this
  // point, since they are siblings of this object, and there may be pointers to
  // this object's sibling somewhere further up on the call stack.
  for (LayoutMultiColumnSet* column_set = FirstMultiColumnSet(); column_set;
       column_set = column_set->NextSiblingMultiColumnSet())
    column_set->DetachFromFlowThread();
  MultiColumnBlockFlow()->ResetMultiColumnFlowThread();
  LayoutFlowThread::WillBeRemovedFromTree();
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
  NOT_DESTROYED();
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
      CreateAndInsertSpannerPlaceholder(To<LayoutBox>(layout_object),
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
  NOT_DESTROYED();
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
    LayoutBox* sibling =
        adjacent_next_spanner_placeholder->PreviousSiblingMultiColumnBox();
    CHECK(sibling->IsLayoutMultiColumnSet());
    column_set_to_remove = To<LayoutMultiColumnSet>(sibling);
    DCHECK(
        !adjacent_previous_spanner_placeholder ||
        column_set_to_remove ==
            adjacent_previous_spanner_placeholder->NextSiblingMultiColumnBox());
  } else if (adjacent_previous_spanner_placeholder) {
    LayoutBox* sibling =
        adjacent_previous_spanner_placeholder->NextSiblingMultiColumnBox();
    CHECK(sibling->IsLayoutMultiColumnSet());
    column_set_to_remove = To<LayoutMultiColumnSet>(sibling);
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
    const LayoutBoxModelObject& object,
    const ComputedStyle& old_style,
    const ComputedStyle& new_style) {
  // If we've become (or are about to become) a container for absolutely
  // positioned descendants, or if we're no longer going to be one, we need to
  // re-evaluate the need for column sets. There may be out-of-flow descendants
  // further down that become part of the flow thread, or cease to be part of
  // the flow thread, because of this change.
  if (object.ComputeIsFixedContainer(&old_style) !=
      object.ComputeIsFixedContainer(&new_style)) {
    return true;
  }
  return old_style.GetPosition() != new_style.GetPosition();
}

static inline bool NeedsToRemoveFromFlowThread(
    const LayoutBoxModelObject& object,
    const ComputedStyle& old_style,
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
         NeedsToReinsertIntoFlowThread(object, old_style, new_style);
}

static inline bool NeedsToInsertIntoFlowThread(
    const LayoutMultiColumnFlowThread* flow_thread,
    const LayoutBoxModelObject* descendant,
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
  return NeedsToReinsertIntoFlowThread(*flow_thread, old_style, new_style);
}

void LayoutMultiColumnFlowThread::FlowThreadDescendantStyleWillChange(
    LayoutBoxModelObject* descendant,
    StyleDifference diff,
    const ComputedStyle& new_style) {
  NOT_DESTROYED();
  toggle_spanners_if_needed_ = false;
  if (NeedsToRemoveFromFlowThread(*descendant, descendant->StyleRef(),
                                  new_style)) {
    FlowThreadDescendantWillBeRemoved(descendant);
#if DCHECK_IS_ON()
    style_changed_object_ = nullptr;
#endif
    return;
  }
#if DCHECK_IS_ON()
  style_changed_object_ = descendant;
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
    LayoutBoxModelObject* descendant,
    StyleDifference diff,
    const ComputedStyle& old_style) {
  NOT_DESTROYED();

#if DCHECK_IS_ON()
  const auto* style_changed_box = style_changed_object_;
  style_changed_object_ = nullptr;
#endif

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
        To<LayoutBox>(descendant),
        NextInPreOrderAfterChildrenSkippingOutOfFlow(this, descendant));
    return;
  }

  if (!toggle_spanners_if_needed)
    return;

  if (could_contain_spanners_ ==
      CanContainSpannerInParentFragmentationContext(*descendant))
    return;

#if DCHECK_IS_ON()
  // Make sure that we were preceded by a call to
  // flowThreadDescendantStyleWillChange() with the same descendant as we have
  // now.
  if (style_changed_box)
    DCHECK_EQ(style_changed_box, descendant);
#endif

  ToggleSpannersInSubtree(descendant);
}

void LayoutMultiColumnFlowThread::ToggleSpannersInSubtree(
    LayoutBoxModelObject* descendant) {
  NOT_DESTROYED();
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
    auto& box = To<LayoutBox>(*object);
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

LayoutPoint LayoutMultiColumnFlowThread::LocationInternal() const {
  NOT_DESTROYED();
  if (!HasValidCachedGeometry() && EverHadLayout()) {
    // const_cast in order to update the cached value.
    const_cast<LayoutMultiColumnFlowThread*>(this)->UpdateGeometry();
  }
  return frame_location_;
}

PhysicalSize LayoutMultiColumnFlowThread::Size() const {
  NOT_DESTROYED();
  if (!HasValidCachedGeometry() && EverHadLayout()) {
    // const_cast in order to update the cached value.
    const_cast<LayoutMultiColumnFlowThread*>(this)->UpdateGeometry();
  }
  return frame_size_;
}

void LayoutMultiColumnFlowThread::UpdateGeometry() {
  NOT_DESTROYED();
  SetHasValidCachedGeometry(true);
  frame_location_ = LayoutPoint();
  LogicalSize thread_size;
  const LayoutBlockFlow* container = MultiColumnBlockFlow();
  if (container->PhysicalFragmentCount() == 0u) {
    frame_size_ = PhysicalSize();
    return;
  }
  const auto* first_fragment = container->GetPhysicalFragment(0);
  WritingModeConverter converter(first_fragment->Style().GetWritingDirection());
  bool has_processed_first_column_in_flow_thread = false;
  const BlockBreakToken* break_token = nullptr;
  for (const auto& container_fragment : container->PhysicalFragments()) {
    for (const auto& link : container_fragment.Children()) {
      const auto& child_fragment = To<PhysicalBoxFragment>(*link);
      if (!child_fragment.IsFragmentainerBox()) {
        continue;
      }
      LogicalSize logical_size = FragmentainerLogicalCapacity(child_fragment);
      thread_size.block_size += logical_size.block_size;
      if (!has_processed_first_column_in_flow_thread) {
        // The offset of the flow thread is the same as that of the first
        // column.
        frame_location_ = LayoutBoxUtils::ComputeLocation(
            child_fragment, link.Offset(), container_fragment, break_token);

        thread_size.inline_size = logical_size.inline_size;
        has_processed_first_column_in_flow_thread = true;
      }
    }
    break_token = container_fragment.GetBreakToken();
    if (!break_token || break_token->IsRepeated() ||
        break_token->IsAtBlockEnd()) {
      break;
    }
  }
  frame_size_ = converter.ToPhysical(thread_size);
}

}  // namespace blink
