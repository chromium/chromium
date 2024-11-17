/*
 * Copyright (C) 2012 Apple Inc.  All rights reserved.
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
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/layout/layout_multi_column_set.h"

#include "third_party/blink/renderer/core/css/resolver/style_resolver.h"
#include "third_party/blink/renderer/core/editing/position_with_affinity.h"
#include "third_party/blink/renderer/core/layout/fragmentation_utils.h"
#include "third_party/blink/renderer/core/layout/geometry/box_strut.h"
#include "third_party/blink/renderer/core/layout/layout_multi_column_flow_thread.h"
#include "third_party/blink/renderer/core/layout/layout_multi_column_spanner_placeholder.h"
#include "third_party/blink/renderer/core/layout/multi_column_fragmentainer_group.h"
#include "third_party/blink/renderer/core/layout/physical_box_fragment.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"

namespace blink {

namespace {

// A helper class to access all child fragments of all fragments of a single
// multi-column container. This class ignores repeated fragments.
class ChildFragmentIterator {
  STACK_ALLOCATED();

 public:
  explicit ChildFragmentIterator(const LayoutBlockFlow& container)
      : container_(container) {
    DCHECK(container.IsFragmentationContextRoot());
    SkipEmptyFragments();
  }

  bool IsValid() const {
    if (fragment_index_ >= container_.PhysicalFragmentCount()) {
      return false;
    }
    const auto* break_token = CurrentFragment()->GetBreakToken();
    return !break_token || !break_token->IsRepeated();
  }

  bool NextChild() {
    DCHECK(IsValid());
    if (++child_index_ >= CurrentFragment()->Children().size()) {
      child_index_ = 0;
      ++fragment_index_;
      SkipEmptyFragments();
    }
    return IsValid();
  }

  const PhysicalBoxFragment* operator->() const {
    DCHECK(IsValid());
    return To<PhysicalBoxFragment>(
        CurrentFragment()->Children()[child_index_].get());
  }
  const PhysicalBoxFragment& operator*() const {
    DCHECK(IsValid());
    return To<PhysicalBoxFragment>(
        *CurrentFragment()->Children()[child_index_]);
  }
  PhysicalOffset Offset() const {
    DCHECK(IsValid());
    return CurrentFragment()->Children()[child_index_].Offset();
  }

  wtf_size_t FragmentIndex() const { return fragment_index_; }

 private:
  const PhysicalBoxFragment* CurrentFragment() const {
    return container_.GetPhysicalFragment(fragment_index_);
  }

  void SkipEmptyFragments() {
    DCHECK_EQ(child_index_, 0u);
    while (IsValid() && CurrentFragment()->Children().size() == 0u) {
      ++fragment_index_;
    }
  }

  const LayoutBlockFlow& container_;
  wtf_size_t fragment_index_ = 0;
  wtf_size_t child_index_ = 0;
};

LayoutPoint ComputeLocation(const PhysicalBoxFragment& column_box,
                            PhysicalOffset column_offset,
                            LayoutUnit set_inline_size,
                            const LayoutBlockFlow& container,
                            wtf_size_t fragment_index,
                            const PhysicalBoxStrut& border_padding_scrollbar) {
  const PhysicalBoxFragment* container_fragment =
      container.GetPhysicalFragment(fragment_index);
  WritingModeConverter converter(
      container_fragment->Style().GetWritingDirection(),
      container_fragment->Size());
  // The inline-offset will be the content-box edge of the multicol container,
  // and the block-offset will be the block-offset of the column itself. It
  // doesn't matter which column from the same row we use, since all columns
  // have the same block-offset and block-size (so just use the first one).
  LogicalOffset logical_offset(
      border_padding_scrollbar.ConvertToLogical(converter.GetWritingDirection())
          .inline_start,
      converter.ToLogical(column_offset, column_box.Size()).block_offset);
  LogicalSize column_set_logical_size(
      set_inline_size, converter.ToLogical(column_box.Size()).block_size);
  PhysicalOffset physical_offset = converter.ToPhysical(
      logical_offset, converter.ToPhysical(column_set_logical_size));
  const BlockBreakToken* previous_container_break_token = nullptr;
  if (fragment_index > 0) {
    previous_container_break_token =
        container.GetPhysicalFragment(fragment_index - 1)->GetBreakToken();
  }
  // We have calculated the physical offset relative to the border edge of
  // this multicol container fragment. We'll now convert it to a legacy
  // engine LayoutPoint, which will also take care of converting it into the
  // flow thread coordinate space, if we happen to be nested inside another
  // fragmentation context.
  return LayoutBoxUtils::ComputeLocation(
      column_box, physical_offset,
      *container.GetPhysicalFragment(fragment_index),
      previous_container_break_token);
}

}  // namespace

LayoutMultiColumnSet::LayoutMultiColumnSet(LayoutFlowThread* flow_thread)
    : LayoutBlockFlow(nullptr),
      fragmentainer_groups_(*this),
      flow_thread_(flow_thread) {}

LayoutMultiColumnSet* LayoutMultiColumnSet::CreateAnonymous(
    LayoutFlowThread& flow_thread,
    const ComputedStyle& parent_style) {
  Document& document = flow_thread.GetDocument();
  LayoutMultiColumnSet* layout_object =
      MakeGarbageCollected<LayoutMultiColumnSet>(&flow_thread);
  layout_object->SetDocumentForAnonymous(&document);
  layout_object->SetStyle(
      document.GetStyleResolver().CreateAnonymousStyleWithDisplay(
          parent_style, EDisplay::kBlock));
  return layout_object;
}

void LayoutMultiColumnSet::Trace(Visitor* visitor) const {
  visitor->Trace(fragmentainer_groups_);
  visitor->Trace(flow_thread_);
  LayoutBlockFlow::Trace(visitor);
}

bool LayoutMultiColumnSet::IsLayoutNGObject() const {
  NOT_DESTROYED();
  return false;
}

unsigned LayoutMultiColumnSet::FragmentainerGroupIndexAtFlowThreadOffset(
    LayoutUnit flow_thread_offset,
    PageBoundaryRule rule) const {
  NOT_DESTROYED();
  UpdateGeometryIfNeeded();
  DCHECK_GT(fragmentainer_groups_.size(), 0u);
  if (flow_thread_offset <= 0)
    return 0;
  for (unsigned index = 0; index < fragmentainer_groups_.size(); index++) {
    const auto& row = fragmentainer_groups_[index];
    if (rule == kAssociateWithLatterPage) {
      if (row.LogicalTopInFlowThread() <= flow_thread_offset &&
          row.LogicalBottomInFlowThread() > flow_thread_offset)
        return index;
    } else if (row.LogicalTopInFlowThread() < flow_thread_offset &&
               row.LogicalBottomInFlowThread() >= flow_thread_offset) {
      return index;
    }
  }
  return fragmentainer_groups_.size() - 1;
}

const MultiColumnFragmentainerGroup&
LayoutMultiColumnSet::FragmentainerGroupAtVisualPoint(
    const LogicalOffset& visual_point) const {
  NOT_DESTROYED();
  UpdateGeometryIfNeeded();
  DCHECK_GT(fragmentainer_groups_.size(), 0u);
  LayoutUnit block_offset = visual_point.block_offset;
  for (unsigned index = 0; index < fragmentainer_groups_.size(); index++) {
    const auto& row = fragmentainer_groups_[index];
    if (row.LogicalTop() + row.GroupLogicalHeight() > block_offset)
      return row;
  }
  return fragmentainer_groups_.Last();
}

bool LayoutMultiColumnSet::IsPageLogicalHeightKnown() const {
  NOT_DESTROYED();
  return FirstFragmentainerGroup().IsLogicalHeightKnown();
}

LayoutMultiColumnSet* LayoutMultiColumnSet::NextSiblingMultiColumnSet() const {
  NOT_DESTROYED();
  for (LayoutObject* sibling = NextSibling(); sibling;
       sibling = sibling->NextSibling()) {
    if (sibling->IsLayoutMultiColumnSet())
      return To<LayoutMultiColumnSet>(sibling);
  }
  return nullptr;
}

LayoutMultiColumnSet* LayoutMultiColumnSet::PreviousSiblingMultiColumnSet()
    const {
  NOT_DESTROYED();
  for (LayoutObject* sibling = PreviousSibling(); sibling;
       sibling = sibling->PreviousSibling()) {
    if (sibling->IsLayoutMultiColumnSet())
      return To<LayoutMultiColumnSet>(sibling);
  }
  return nullptr;
}

MultiColumnFragmentainerGroup&
LayoutMultiColumnSet::AppendNewFragmentainerGroup() {
  NOT_DESTROYED();
  MultiColumnFragmentainerGroup new_group(*this);
  {  // Extra scope here for previousGroup; it's potentially invalid once we
     // modify the m_fragmentainerGroups Vector.
    MultiColumnFragmentainerGroup& previous_group =
        fragmentainer_groups_.Last();

    // This is the flow thread block offset where |previousGroup| ends and
    // |newGroup| takes over.
    LayoutUnit block_offset_in_flow_thread =
        previous_group.LogicalTopInFlowThread() +
        FragmentainerGroupCapacity(previous_group);
    previous_group.SetLogicalBottomInFlowThread(block_offset_in_flow_thread);
    new_group.SetLogicalTopInFlowThread(block_offset_in_flow_thread);
    new_group.SetLogicalTop(previous_group.LogicalTop() +
                            previous_group.GroupLogicalHeight());
    new_group.ResetColumnHeight();
  }
  fragmentainer_groups_.Append(new_group);
  return fragmentainer_groups_.Last();
}

LayoutUnit LayoutMultiColumnSet::LogicalTopInFlowThread() const {
  NOT_DESTROYED();
  return FirstFragmentainerGroup().LogicalTopInFlowThread();
}

LayoutUnit LayoutMultiColumnSet::LogicalBottomInFlowThread() const {
  NOT_DESTROYED();
  return LastFragmentainerGroup().LogicalBottomInFlowThread();
}

PhysicalOffset LayoutMultiColumnSet::FlowThreadTranslationAtOffset(
    LayoutUnit block_offset,
    PageBoundaryRule rule) const {
  NOT_DESTROYED();
  return FragmentainerGroupAtFlowThreadOffset(block_offset, rule)
      .FlowThreadTranslationAtOffset(block_offset, rule);
}

LogicalOffset LayoutMultiColumnSet::VisualPointToFlowThreadPoint(
    const PhysicalOffset& visual_point) const {
  NOT_DESTROYED();
  LogicalOffset logical_point =
      CreateWritingModeConverter().ToLogical(visual_point, {});
  const MultiColumnFragmentainerGroup& row =
      FragmentainerGroupAtVisualPoint(logical_point);
  return row.VisualPointToFlowThreadPoint(logical_point -
                                          row.OffsetFromColumnSet());
}

void LayoutMultiColumnSet::ResetColumnHeight() {
  NOT_DESTROYED();
  fragmentainer_groups_.DeleteExtraGroups();
  fragmentainer_groups_.First().ResetColumnHeight();
}

void LayoutMultiColumnSet::StyleDidChange(StyleDifference diff,
                                          const ComputedStyle* old_style) {
  NOT_DESTROYED();
  LayoutBlockFlow::StyleDidChange(diff, old_style);

  // column-rule is specified on the parent (the multicol container) of this
  // object, but it's the column sets that are in charge of painting them.
  // A column rule is pretty much like any other box decoration, like borders.
  // We need to say that we have box decorations here, so that the columnn set
  // is invalidated when it gets laid out. We cannot check here whether the
  // multicol container actually has a visible column rule or not, because we
  // may not have been inserted into the tree yet. Painting a column set is
  // cheap anyway, because the only thing it can paint is the column rule, while
  // actual multicol content is handled by the flow thread.
  SetHasBoxDecorationBackground(true);
}

LayoutUnit LayoutMultiColumnSet::ColumnGap() const {
  NOT_DESTROYED();
  LayoutBlockFlow* parent_block = MultiColumnBlockFlow();

  if (const std::optional<Length>& column_gap =
          parent_block->StyleRef().ColumnGap()) {
    return ValueForLength(*column_gap, AvailableLogicalWidth());
  }

  // "1em" is recommended as the normal gap setting. Matches <p> margins.
  return LayoutUnit(
      parent_block->StyleRef().GetFontDescription().ComputedPixelSize());
}

unsigned LayoutMultiColumnSet::ActualColumnCount() const {
  NOT_DESTROYED();
  // FIXME: remove this method. It's a meaningless question to ask the set "how
  // many columns do you actually have?", since that may vary for each row.
  return FirstFragmentainerGroup().ActualColumnCount();
}

PhysicalRect LayoutMultiColumnSet::FragmentsBoundingBox(
    const PhysicalRect& bounding_box_in_flow_thread) const {
  NOT_DESTROYED();
  UpdateGeometryIfNeeded();
  PhysicalRect result;
  for (const auto& group : fragmentainer_groups_)
    result.Unite(group.FragmentsBoundingBox(bounding_box_in_flow_thread));
  return result;
}

void LayoutMultiColumnSet::InsertedIntoTree() {
  NOT_DESTROYED();
  LayoutBlockFlow::InsertedIntoTree();
  AttachToFlowThread();
}

void LayoutMultiColumnSet::WillBeRemovedFromTree() {
  NOT_DESTROYED();
  LayoutBlockFlow::WillBeRemovedFromTree();
  DetachFromFlowThread();
}

LayoutPoint LayoutMultiColumnSet::LocationInternal() const {
  NOT_DESTROYED();
  UpdateGeometryIfNeeded();
  return frame_location_;
}

PhysicalSize LayoutMultiColumnSet::Size() const {
  NOT_DESTROYED();
  UpdateGeometryIfNeeded();
  return frame_size_;
}

void LayoutMultiColumnSet::UpdateGeometryIfNeeded() const {
  if (!HasValidCachedGeometry() && EverHadLayout()) {
    // const_cast in order to update the cached value.
    const_cast<LayoutMultiColumnSet*>(this)->UpdateGeometry();
  }
}

void LayoutMultiColumnSet::UpdateGeometry() {
  NOT_DESTROYED();
  DCHECK(!HasValidCachedGeometry());
  SetHasValidCachedGeometry(true);
  frame_location_ = LayoutPoint();
  ResetColumnHeight();
  const LayoutBlockFlow* container = MultiColumnBlockFlow();
  DCHECK_GT(container->PhysicalFragmentCount(), 0u);

  const auto* first_fragment = container->GetPhysicalFragment(0);
  WritingMode writing_mode = first_fragment->Style().GetWritingMode();
  PhysicalBoxStrut border_padding_scrollbar = first_fragment->Borders() +
                                              first_fragment->Padding() +
                                              container->ComputeScrollbars();

  // Set the inline-size to that of the content-box of the multicol container.
  PhysicalSize content_size =
      first_fragment->Size() -
      PhysicalSize(border_padding_scrollbar.HorizontalSum(),
                   border_padding_scrollbar.VerticalSum());
  LogicalSize logical_size;
  logical_size.inline_size =
      content_size.ConvertToLogical(writing_mode).inline_size;

  // TODO(layout-dev): Ideally we should not depend on the layout tree structure
  // because it may be different from the tree for the physical fragments.
  const auto* previous_placeholder =
      DynamicTo<LayoutMultiColumnSpannerPlaceholder>(PreviousSibling());
  bool seen_previous_placeholder = !previous_placeholder;
  ChildFragmentIterator iter(*container);
  LayoutUnit flow_thread_offset;

  // Skip until a column box after previous_placeholder.
  for (; iter.IsValid(); iter.NextChild()) {
    if (!iter->IsFragmentainerBox()) {
      if (iter->IsLayoutObjectDestroyedOrMoved()) {
        continue;
      }
      const auto* child_box = To<LayoutBox>(iter->GetLayoutObject());
      if (child_box->IsColumnSpanAll()) {
        if (seen_previous_placeholder) {
          // The legacy tree builder (the flow thread code) sometimes
          // incorrectly keeps column sets that shouldn't be there anymore. If
          // we have two column spanners, that are in fact adjacent, even though
          // there's a spurious column set between them, the column set hasn't
          // been initialized correctly (since we still have a
          // pending_column_set at this point). Say hello to the column set that
          // shouldn't exist, so that it gets some initialization.
          SetIsIgnoredByNG();
          frame_size_ = ToPhysicalSize(logical_size, writing_mode);
          return;
        }
        if (previous_placeholder &&
            child_box == previous_placeholder->LayoutObjectInFlowThread()) {
          seen_previous_placeholder = true;
        }
      }
      continue;
    }
    if (seen_previous_placeholder) {
      break;
    }
    flow_thread_offset += FragmentainerLogicalCapacity(*iter).block_size;
  }
  if (!iter.IsValid()) {
    SetIsIgnoredByNG();
    frame_size_ = ToPhysicalSize(logical_size, writing_mode);
    return;
  }
  // Found the first column box after previous_placeholder.

  frame_location_ = ComputeLocation(
      *iter, iter.Offset(), logical_size.inline_size, *container,
      iter.FragmentIndex(), border_padding_scrollbar);

  while (true) {
    LogicalSize fragmentainer_logical_size =
        FragmentainerLogicalCapacity(*iter);
    LastFragmentainerGroup().SetLogicalTopInFlowThread(flow_thread_offset);
    logical_size.block_size += fragmentainer_logical_size.block_size;
    flow_thread_offset += fragmentainer_logical_size.block_size;
    LastFragmentainerGroup().SetColumnBlockSizeFromNG(
        fragmentainer_logical_size.block_size);

    // Handle following fragmentainer boxes in the current container fragment.
    wtf_size_t fragment_index = iter.FragmentIndex();
    bool should_expand_last_set = false;
    while (iter.NextChild() && iter.FragmentIndex() == fragment_index) {
      if (iter->IsFragmentainerBox()) {
        LayoutUnit column_size = FragmentainerLogicalCapacity(*iter).block_size;
        flow_thread_offset += column_size;
        if (should_expand_last_set) {
          LastFragmentainerGroup().ExtendColumnBlockSizeFromNG(column_size);
          should_expand_last_set = false;
        }
      } else {
        if (iter->IsColumnSpanAll()) {
          const auto* placeholder =
              iter->GetLayoutObject()->SpannerPlaceholder();
          // If there is no column set after the spanner, we should expand the
          // last column set (if any) to encompass any columns that were created
          // after the spanner. Only do this if we're actually past the last
          // column set, though. We may have adjacent spanner placeholders,
          // because the legacy and NG engines disagree on whether there's
          // column content in-between (NG will create column content if the
          // parent block of a spanner has trailing margin / border / padding,
          // while legacy does not).
          if (placeholder && !placeholder->NextSiblingMultiColumnBox()) {
            should_expand_last_set = true;
            continue;
          }
        }
        break;
      }
    }
    LastFragmentainerGroup().SetLogicalBottomInFlowThread(flow_thread_offset);

    if (!iter.IsValid()) {
      break;
    }
    if (iter.FragmentIndex() == fragment_index || !iter->IsFragmentainerBox()) {
      // Found a physical fragment with !IsFragmentainerBox().
      break;
    }
    AppendNewFragmentainerGroup();
  }
  frame_size_ = ToPhysicalSize(logical_size, writing_mode);
}

void LayoutMultiColumnSet::AttachToFlowThread() {
  NOT_DESTROYED();
  if (DocumentBeingDestroyed())
    return;

  if (!flow_thread_)
    return;

  flow_thread_->AddColumnSetToThread(this);
}

void LayoutMultiColumnSet::DetachFromFlowThread() {
  NOT_DESTROYED();
  if (flow_thread_) {
    flow_thread_->RemoveColumnSetFromThread(this);
    flow_thread_ = nullptr;
  }
}

void LayoutMultiColumnSet::SetIsIgnoredByNG() {
  NOT_DESTROYED();
  fragmentainer_groups_.First().SetColumnBlockSizeFromNG(LayoutUnit());
}

}  // namespace blink
