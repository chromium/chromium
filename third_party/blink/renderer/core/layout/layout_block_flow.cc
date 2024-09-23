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
#include "third_party/blink/renderer/core/layout/absolute_utils.h"
#include "third_party/blink/renderer/core/layout/fragmentation_utils.h"
#include "third_party/blink/renderer/core/layout/hit_test_location.h"
#include "third_party/blink/renderer/core/layout/inline/inline_cursor.h"
#include "third_party/blink/renderer/core/layout/inline/offset_mapping.h"
#include "third_party/blink/renderer/core/layout/layout_block_flow.h"
#include "third_party/blink/renderer/core/layout/layout_flow_thread.h"
#include "third_party/blink/renderer/core/layout/layout_inline.h"
#include "third_party/blink/renderer/core/layout/layout_multi_column_flow_thread.h"
#include "third_party/blink/renderer/core/layout/layout_multi_column_spanner_placeholder.h"
#include "third_party/blink/renderer/core/layout/layout_object_inlines.h"
#include "third_party/blink/renderer/core/layout/layout_result.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/layout/legacy_layout_tree_walking.h"
#include "third_party/blink/renderer/core/layout/logical_box_fragment.h"
#include "third_party/blink/renderer/core/layout/physical_box_fragment.h"
#include "third_party/blink/renderer/core/layout/shapes/shape_outside_info.h"
#include "third_party/blink/renderer/core/layout/table/layout_table.h"
#include "third_party/blink/renderer/core/layout/text_autosizer.h"
#include "third_party/blink/renderer/core/layout/unpositioned_float.h"
#include "third_party/blink/renderer/core/paint/box_fragment_painter.h"
#include "third_party/blink/renderer/core/paint/inline_paint_context.h"
#include "third_party/blink/renderer/core/paint/object_paint_invalidator.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"
#include "third_party/blink/renderer/platform/heap/collection_support/clear_collection_scope.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/size_assertions.h"

namespace blink {

struct SameSizeAsLayoutBlockFlow : public LayoutBlock {
  Member<void*> member;
  Member<void*> inline_node_data;
};

ASSERT_SIZE(LayoutBlockFlow, SameSizeAsLayoutBlockFlow);

LayoutBlockFlow::LayoutBlockFlow(ContainerNode* node) : LayoutBlock(node) {
  SetChildrenInline(true);
}

LayoutBlockFlow::~LayoutBlockFlow() = default;

LayoutBlockFlow* LayoutBlockFlow::CreateAnonymous(Document* document,
                                                  const ComputedStyle* style) {
  auto* layout_block_flow = MakeGarbageCollected<LayoutBlockFlow>(nullptr);
  layout_block_flow->SetDocumentForAnonymous(document);
  layout_block_flow->SetStyle(style);
  return layout_block_flow;
}

bool LayoutBlockFlow::IsInitialLetterBox() const {
  return IsA<FirstLetterPseudoElement>(GetNode()) &&
         !StyleRef().InitialLetter().IsNormal();
}

bool LayoutBlockFlow::CanContainFirstFormattedLine() const {
  NOT_DESTROYED();
  // The 'text-indent' only affects a line if it is the first formatted
  // line of an element. For example, the first line of an anonymous block
  // box is only affected if it is the first child of its parent element.
  // https://drafts.csswg.org/css-text-3/#text-indent-property
  return !IsAnonymousBlock() || !PreviousSibling() || IsFlexItem() ||
         IsGridItem();
}

void LayoutBlockFlow::WillBeDestroyed() {
  NOT_DESTROYED();
  // Make sure to destroy anonymous children first while they are still
  // connected to the rest of the tree, so that they will properly dirty line
  // boxes that they are removed from. Effects that do :before/:after only on
  // hover could crash otherwise.
  Children()->DestroyLeftoverChildren();

  LayoutBlock::WillBeDestroyed();
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
  const bool child_is_inline_level =
      new_child->IsInline() ||
      (LayoutObject::RequiresAnonymousTableWrappers(new_child) &&
       LayoutTable::ShouldCreateInlineAnonymous(*this));
  bool child_is_block_level =
      !child_is_inline_level && !new_child->IsFloatingOrOutOfFlowPositioned();

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

    // LayoutOutsideListMarker is out-of-flow for the tree building purpose,
    // and that is not inline level, but IsInline().
    if (new_child->IsInline() && !new_child->IsLayoutOutsideListMarker()) {
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
         !block->IsRubyColumn() && !block->IsRubyBase() &&
         !block->IsViewTransitionRoot();
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
  if (child_block_flow && !child_block_flow->PreviousSibling() &&
      !child_block_flow->NextSibling()) {
    // If the removal has knocked us down to containing only a single anonymous
    // box we can go ahead and pull the content right back up into our
    // box.
    if (merged_anonymous_blocks || IsMergeableAnonymousBlock(child_block_flow))
      CollapseAnonymousBlockChild(child_block_flow);
  }

  if (FirstChild() && !BeingDestroyed() &&
      !old_child->IsFloatingOrOutOfFlowPositioned() &&
      !old_child->IsAnonymousBlock()) {
    // If the child we're removing means that we can now treat all children as
    // inline without the need for anonymous blocks, then do that.
    MakeChildrenInlineIfPossible();
  }
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
  if (IsAnonymousBlock()) {
    if (auto* parent_inline = DynamicTo<LayoutInline>(Parent())) {
      // The child used to be an in-flow block-in-inline, which requires an
      // anonymous wrapper (|this|). It is no longer needed for this child, so
      // unless there are other siblings there that still require it, it needs
      // to be destroyed (i.e. |this| will be destroyed).
      parent_inline->BlockInInlineBecameFloatingOrOutOfFlow(this);
      return;
    }
  }

  MakeChildrenInlineIfPossible();

  // Reparent the child to an adjacent anonymous block if one is available.
  auto* prev = DynamicTo<LayoutBlockFlow>(child->PreviousSibling());
  if (prev && prev->IsAnonymousBlock()) {
    MoveChildTo(prev, child, nullptr, false);
    // The anonymous block we've moved to may now be adjacent to former siblings
    // of ours that it can contain also.
    prev->ReparentSubsequentFloatingOrOutOfFlowSiblings();
    return;
  }
  auto* next = DynamicTo<LayoutBlockFlow>(child->NextSibling());
  if (next && next->IsAnonymousBlock()) {
    MoveChildTo(next, child, next->FirstChild(), false);
  }
}

static bool AllowsCollapseAnonymousBlockChild(const LayoutBlockFlow& parent,
                                              const LayoutBlockFlow& child) {
  // It's possible that this block's destruction may have been triggered by the
  // child's removal. Just bail if the anonymous child block is already being
  // destroyed. See crbug.com/282088
  if (child.BeingDestroyed())
    return false;
  // Ruby elements use anonymous wrappers for ruby columns and ruby bases by
  // design, so we don't remove them.
  if (child.IsRubyColumn() || child.IsRubyBase()) {
    return false;
  }
  // The ViewTransitionRoot is also anonymous by design and shouldn't be
  // elided.
  if (child.IsViewTransitionRoot()) {
    return false;
  }
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
    // Ruby elements use anonymous wrappers for ruby columns and ruby bases by
    // design, so we don't remove them.
    if (child->IsRubyColumn() || child->IsRubyBase()) {
      return;
    }

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

  // LayoutOutsideListMarker is out-of-flow for the tree building purpose.
  // Skip here because it's the first child.
  if (curr && curr->IsLayoutOutsideListMarker()) {
    curr = curr->NextSibling();
  }

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
  ClearInlineNodeData();

  LayoutObject* child = FirstChild();
  if (!child)
    return;

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
    DCHECK(!c->IsInline() || c->IsLayoutOutsideListMarker());
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

bool LayoutBlockFlow::ShouldTruncateOverflowingText() const {
  NOT_DESTROYED();
  const LayoutObject* object_to_check = this;
  if (IsAnonymousBlock()) {
    const LayoutObject* parent = Parent();
    if (!parent || !parent->BehavesLikeBlockContainer()) {
      return false;
    }
    object_to_check = parent;
  }
  return object_to_check->HasNonVisibleOverflow() &&
         object_to_check->StyleRef().TextOverflow() != ETextOverflow::kClip;
}

Node* LayoutBlockFlow::NodeForHitTest() const {
  NOT_DESTROYED();
  // If we are in the margins of block elements that are part of a
  // block-in-inline we're actually still inside the enclosing element
  // that was split. Use the appropriate inner node.
  if (IsBlockInInline()) [[unlikely]] {
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

  // TODO(1229581): Layout objects that don't allow fragment traversal for paint
  // and hit-testing (see CanTraversePhysicalFragments()) still end up here. We
  // may even end up here if ChildrenInline(). That's just the initial state of
  // a block, though. As soon as a non-fragment-traversale object gets children,
  // they will be blocks, and *they* will be fragment-traversable.
  DCHECK(!ChildrenInline() || !FirstChild());
  if (!ChildrenInline() &&
      LayoutBlock::HitTestChildren(result, hit_test_location,
                                   accumulated_offset, phase)) {
    return true;
  }

  return false;
}

void LayoutBlockFlow::AddOutlineRects(
    OutlineRectCollector& collector,
    LayoutObject::OutlineInfo* info,
    const PhysicalOffset& additional_offset,
    OutlineType include_block_overflows) const {
  NOT_DESTROYED();

  // TODO(crbug.com/40155711): Currently |PhysicalBoxFragment| does not support
  // NG block fragmentation. Fallback to the legacy code path.
  if (PhysicalFragmentCount() == 1) {
    const PhysicalBoxFragment* fragment = GetPhysicalFragment(0);
    if (fragment->HasItems()) {
      fragment->AddSelfOutlineRects(additional_offset, include_block_overflows,
                                    collector, info);
      return;
    }
  }

  LayoutBlock::AddOutlineRects(collector, info, additional_offset,
                               include_block_overflows);
}

void LayoutBlockFlow::DirtyLinesFromChangedChild(LayoutObject* child) {
  NOT_DESTROYED();

  // We need to dirty line box fragments only if the child is once laid out in
  // LayoutNG inline formatting context. New objects are handled in
  // InlineNode::MarkLineBoxesDirty().
  if (child->IsInLayoutNGInlineFormattingContext()) {
    FragmentItems::DirtyLinesFromChangedChild(*child, *this);
  }
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

  if (IsListItem()) {
    UseCounter::Count(GetDocument(), WebFeature::kMultiColAndListItem);
  }

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

  DCHECK(!multi_column_flow_thread_);
  multi_column_flow_thread_ = flow_thread;
}

void LayoutBlockFlow::SetShouldDoFullPaintInvalidationForFirstLine() {
  NOT_DESTROYED();
  DCHECK(ChildrenInline());

  const auto fragments = PhysicalFragments();
  if (fragments.IsEmpty()) {
    return;
  }
  for (const PhysicalBoxFragment& fragment : fragments) {
    InlineCursor first_line(fragment);
    if (!first_line) {
      continue;
    }
    first_line.MoveToFirstLine();
    if (!first_line) {
      continue;
    }
    if (first_line.Current().UsesFirstLineStyle()) {
      // Mark all descendants of the first line if first-line style.
      for (InlineCursor descendants = first_line.CursorForDescendants();
           descendants; descendants.MoveToNext()) {
        const FragmentItem* item = descendants.Current().Item();
        if (item->IsLayoutObjectDestroyedOrMoved()) [[unlikely]] {
          descendants.MoveToNextSkippingChildren();
          continue;
        }
        LayoutObject* layout_object = item->GetMutableLayoutObject();
        DCHECK(layout_object);
        layout_object->StyleRef().ClearCachedPseudoElementStyles();
        layout_object->SetShouldDoFullPaintInvalidation();
      }
      StyleRef().ClearCachedPseudoElementStyles();
      SetShouldDoFullPaintInvalidation();
      return;
    }
  }
}

PositionWithAffinity LayoutBlockFlow::PositionForPoint(
    const PhysicalOffset& point) const {
  NOT_DESTROYED();
  DCHECK_GE(GetDocument().Lifecycle().GetState(),
            DocumentLifecycle::kPrePaintClean);

  if (IsAtomicInlineLevel()) {
    PositionWithAffinity position =
        PositionForPointIfOutsideAtomicInlineLevel(point);
    if (!position.IsNull())
      return position;
  }
  if (!ChildrenInline())
    return LayoutBlock::PositionForPoint(point);

  if (PhysicalFragmentCount()) {
    return PositionForPointInFragments(point);
  }

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

void LayoutBlockFlow::InvalidateDisplayItemClients(
    PaintInvalidationReason invalidation_reason) const {
  NOT_DESTROYED();
  LayoutBlock::InvalidateDisplayItemClients(invalidation_reason);

  InlineCursor cursor(*this);
  if (!cursor) {
    return;
  }

  ObjectPaintInvalidator paint_invalidator(*this);
  // Line boxes record hit test data (see BoxFragmentPainter::PaintLineBox)
  // and should be invalidated if they change.
  bool invalidate_all_lines =
      HasEffectiveAllowedTouchAction() || InsideBlockingWheelEventHandler();

  for (cursor.MoveToFirstLine(); cursor; cursor.MoveToNextLine()) {
    // The first line LineBoxFragment paints the ::first-line background.
    // Because it may be expensive to figure out if the first line is affected
    // by any ::first-line selectors at all, we just invalidate
    // unconditionally which is typically cheaper.
    if (invalidate_all_lines || cursor.Current().UsesFirstLineStyle()) {
      DCHECK(cursor.Current().GetDisplayItemClient());
      paint_invalidator.InvalidateDisplayItemClient(
          *cursor.Current().GetDisplayItemClient(), invalidation_reason);
    }
    if (!invalidate_all_lines) {
      break;
    }
  }
}

}  // namespace blink
