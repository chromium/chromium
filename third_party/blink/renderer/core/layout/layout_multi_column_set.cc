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
#include "third_party/blink/renderer/core/layout/layout_box_utils.h"
#include "third_party/blink/renderer/core/layout/layout_multi_column_flow_thread.h"
#include "third_party/blink/renderer/core/layout/layout_multi_column_spanner_placeholder.h"
#include "third_party/blink/renderer/core/layout/multi_column_fragmentainer_group.h"
#include "third_party/blink/renderer/core/layout/physical_box_fragment.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

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

}  // namespace

LayoutMultiColumnSet::LayoutMultiColumnSet(LayoutFlowThread* flow_thread)
    : LayoutBlockFlow(nullptr),
      fragmentainer_groups_(*this),
      flow_thread_(flow_thread) {
  DCHECK(!RuntimeEnabledFeatures::FlowThreadLessEnabled());
}

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

PhysicalSize LayoutMultiColumnSet::Size() const {
  NOT_DESTROYED();
  return frame_size_;
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
