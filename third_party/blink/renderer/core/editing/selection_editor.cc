/*
 * Copyright (C) 2004, 2008, 2009, 2010 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
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

#include "third_party/blink/renderer/core/editing/selection_editor.h"

#include "third_party/blink/renderer/core/dom/node_with_index.h"
#include "third_party/blink/renderer/core/dom/text.h"
#include "third_party/blink/renderer/core/editing/editing_behavior.h"
#include "third_party/blink/renderer/core/editing/editing_utilities.h"
#include "third_party/blink/renderer/core/editing/editor.h"
#include "third_party/blink/renderer/core/editing/ephemeral_range.h"
#include "third_party/blink/renderer/core/editing/frame_caret.h"
#include "third_party/blink/renderer/core/editing/local_caret_rect.h"
#include "third_party/blink/renderer/core/editing/position_with_affinity.h"
#include "third_party/blink/renderer/core/editing/selection_adjuster.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"

namespace blink {

SelectionEditor::SelectionEditor(LocalFrame& frame) : frame_(frame) {
  ClearVisibleSelection();
}

SelectionEditor::~SelectionEditor() = default;

void SelectionEditor::AssertSelectionValid() const {
#if DCHECK_IS_ON()
  // Since We don't track dom tree version during attribute changes, we can't
  // use it for validity of |selection_|.
  const_cast<SelectionEditor*>(this)->selection_.dom_tree_version_ =
      GetDocument().DomTreeVersion();
#endif
  selection_.AssertValidFor(GetDocument());
}

void SelectionEditor::ClearVisibleSelection() {
  selection_ = SelectionInDOMTree();
  cached_visible_selection_in_dom_tree_ = VisibleSelection();
  cached_visible_selection_in_flat_tree_ = VisibleSelectionInFlatTree();
  cached_visible_selection_in_dom_tree_is_dirty_ = true;
  cached_visible_selection_in_flat_tree_is_dirty_ = true;
}

void SelectionEditor::Dispose() {
  ClearDocumentCachedRange();
  ClearVisibleSelection();
}

Document& SelectionEditor::GetDocument() const {
  DCHECK(SynchronousMutationObserver::GetDocument());
  return *SynchronousMutationObserver::GetDocument();
}

VisibleSelection SelectionEditor::ComputeVisibleSelectionInDOMTree() const {
  DCHECK_EQ(GetFrame()->GetDocument(), GetDocument());
  DCHECK_EQ(GetFrame(), GetDocument().GetFrame());
  UpdateCachedVisibleSelectionIfNeeded();
  if (cached_visible_selection_in_dom_tree_.IsNone())
    return cached_visible_selection_in_dom_tree_;
  DCHECK_EQ(cached_visible_selection_in_dom_tree_.Anchor().GetDocument(),
            GetDocument());
  return cached_visible_selection_in_dom_tree_;
}

VisibleSelectionInFlatTree SelectionEditor::ComputeVisibleSelectionInFlatTree()
    const {
  DCHECK_EQ(GetFrame()->GetDocument(), GetDocument());
  DCHECK_EQ(GetFrame(), GetDocument().GetFrame());
  UpdateCachedVisibleSelectionInFlatTreeIfNeeded();
  if (cached_visible_selection_in_flat_tree_.IsNone())
    return cached_visible_selection_in_flat_tree_;
  DCHECK_EQ(cached_visible_selection_in_flat_tree_.Anchor().GetDocument(),
            GetDocument());
  return cached_visible_selection_in_flat_tree_;
}

bool SelectionEditor::ComputeAbsoluteBounds(gfx::Rect& anchor,
                                            gfx::Rect& focus) const {
  DCHECK_EQ(GetFrame()->GetDocument(), GetDocument());
  DCHECK_EQ(GetFrame(), GetDocument().GetFrame());
  UpdateCachedAbsoluteBoundsIfNeeded();
  if (!has_selection_bounds_)
    return has_selection_bounds_;
  anchor = cached_anchor_bounds_;
  focus = cached_focus_bounds_;
  return has_selection_bounds_;
}

const SelectionInDOMTree& SelectionEditor::GetSelectionInDOMTree() const {
  AssertSelectionValid();
  return selection_;
}

void SelectionEditor::MarkCacheDirty() {
  if (!cached_visible_selection_in_dom_tree_is_dirty_) {
    cached_visible_selection_in_dom_tree_ = VisibleSelection();
    cached_visible_selection_in_dom_tree_is_dirty_ = true;
  }
  if (!cached_visible_selection_in_flat_tree_is_dirty_) {
    cached_visible_selection_in_flat_tree_ = VisibleSelectionInFlatTree();
    cached_visible_selection_in_flat_tree_is_dirty_ = true;
  }
  if (!cached_absolute_bounds_are_dirty_) {
    cached_absolute_bounds_are_dirty_ = true;
    has_selection_bounds_ = false;
    cached_anchor_bounds_ = gfx::Rect();
    cached_focus_bounds_ = gfx::Rect();
  }
}

void SelectionEditor::SetSelectionAndEndTyping(
    const SelectionInDOMTree& new_selection) {
  new_selection.AssertValidFor(GetDocument());
  DCHECK_NE(selection_, new_selection);
  ClearDocumentCachedRange();
  MarkCacheDirty();
  selection_ = new_selection;
}

void SelectionEditor::DidChangeChildren(const ContainerNode&,
                                        const ContainerNode::ChildrenChange&) {
  selection_.ResetDirectionCache();
  MarkCacheDirty();
  DidFinishDOMMutation();
}

void SelectionEditor::DidFinishTextChange(const Position& new_anchor,
                                          const Position& new_focus) {
  if (new_anchor == selection_.anchor_ && new_focus == selection_.focus_) {
    DidFinishDOMMutation();
    return;
  }
  selection_.anchor_ = new_anchor;
  selection_.focus_ = new_focus;
  selection_.ResetDirectionCache();
  MarkCacheDirty();
  DidFinishDOMMutation();
}

void SelectionEditor::DidFinishDOMMutation() {
  AssertSelectionValid();
}

void SelectionEditor::DidAttachDocument(Document* document) {
  DCHECK(document);
  DCHECK(!SynchronousMutationObserver::GetDocument())
      << SynchronousMutationObserver::GetDocument();
#if DCHECK_IS_ON()
  style_version_for_dom_tree_ = static_cast<uint64_t>(-1);
  style_version_for_flat_tree_ = static_cast<uint64_t>(-1);
#endif
  ClearVisibleSelection();
  SetDocument(document);
}

void SelectionEditor::ContextDestroyed() {
  Dispose();
#if DCHECK_IS_ON()
  style_version_for_dom_tree_ = static_cast<uint64_t>(-1);
  style_version_for_flat_tree_ = static_cast<uint64_t>(-1);
  style_version_for_absolute_bounds_ = static_cast<uint64_t>(-1);
#endif
  selection_ = SelectionInDOMTree();
  cached_visible_selection_in_dom_tree_ = VisibleSelection();
  cached_visible_selection_in_flat_tree_ = VisibleSelectionInFlatTree();
  cached_visible_selection_in_dom_tree_is_dirty_ = true;
  cached_visible_selection_in_flat_tree_is_dirty_ = true;
  cached_absolute_bounds_are_dirty_ = true;
  has_selection_bounds_ = false;
  cached_anchor_bounds_ = gfx::Rect();
  cached_focus_bounds_ = gfx::Rect();
}

static Position ComputePositionForChildrenRemoval(const Position& position,
                                                  ContainerNode& container) {
  Node* node = position.ComputeContainerNode();
#if DCHECK_IS_ON()
  DCHECK(node) << position;
#else
  // TODO(https://crbug.com/882592): Once we know the root cause, we should
  // get rid of following if-statement.
  if (!node)
    return position;
#endif
  if (!container.ContainsIncludingHostElements(*node))
    return position;
  if (auto* element = DynamicTo<Element>(container)) {
    if (auto* shadow_root = element->GetShadowRoot()) {
      // Removal of light children does not affect position in the
      // shadow tree.
      if (shadow_root->ContainsIncludingHostElements(*node))
        return position;
    }
  }
  return Position::FirstPositionInNode(container);
}

void SelectionEditor::NodeChildrenWillBeRemoved(ContainerNode& container) {
  if (selection_.IsNone())
    return;
  const Position old_anchor = selection_.anchor_;
  const Position old_focus = selection_.focus_;
  const Position& new_anchor =
      ComputePositionForChildrenRemoval(old_anchor, container);
  const Position& new_focus =
      ComputePositionForChildrenRemoval(old_focus, container);
  if (new_anchor == old_anchor && new_focus == old_focus) {
    return;
  }
  selection_ = SelectionInDOMTree::Builder()
                   .SetBaseAndExtent(new_anchor, new_focus)
                   .Build();
  MarkCacheDirty();
}

void SelectionEditor::NodeWillBeRemoved(Node& node_to_be_removed) {
  if (selection_.IsNone())
    return;
  const Position old_anchor = selection_.anchor_;
  const Position old_focus = selection_.focus_;
  const Position& new_anchor =
      ComputePositionForNodeRemoval(old_anchor, node_to_be_removed);
  const Position& new_focus =
      ComputePositionForNodeRemoval(old_focus, node_to_be_removed);
  if (new_anchor == old_anchor && new_focus == old_focus) {
    return;
  }
  selection_ = SelectionInDOMTree::Builder()
                   .SetBaseAndExtent(new_anchor, new_focus)
                   .Build();
  MarkCacheDirty();
}

static Position UpdatePositionAfterAdoptingTextReplacement(
    const Position& position,
    CharacterData* node,
    unsigned offset,
    unsigned old_length,
    unsigned new_length) {
  if (position.AnchorNode() != node)
    return position;

  if (position.IsBeforeAnchor()) {
    return UpdatePositionAfterAdoptingTextReplacement(
        Position(node, 0), node, offset, old_length, new_length);
  }
  if (position.IsAfterAnchor()) {
    return UpdatePositionAfterAdoptingTextReplacement(
        Position(node, old_length), node, offset, old_length, new_length);
  }

  // See:
  // http://www.w3.org/TR/DOM-Level-2-Traversal-Range/ranges.html#Level-2-Range-Mutation
  DCHECK_GE(position.OffsetInContainerNode(), 0);
  unsigned position_offset =
      static_cast<unsigned>(position.OffsetInContainerNode());
  // Replacing text can be viewed as a deletion followed by insertion.
  if (position_offset >= offset && position_offset <= offset + old_length)
    position_offset = offset;

  // Adjust the offset if the position is after the end of the deleted contents
  // (positionOffset > offset + oldLength) to avoid having a stale offset.
  if (position_offset > offset + old_length)
    position_offset = position_offset - old_length + new_length;

  // Due to case folding
  // (http://unicode.org/Public/UCD/latest/ucd/CaseFolding.txt), LayoutText
  // length may be different from Text length.  A correct implementation would
  // translate the LayoutText offset to a Text offset; this is just a safety
  // precaution to avoid offset values that run off the end of the Text.
  if (position_offset > node->length())
    position_offset = node->length();

  return Position(node, position_offset);
}

void SelectionEditor::DidUpdateCharacterData(CharacterData* node,
                                             unsigned offset,
                                             unsigned old_length,
                                             unsigned new_length) {
  // The fragment check is a performance optimization. See
  // http://trac.webkit.org/changeset/30062.
  if (selection_.IsNone() || !node || !node->isConnected()) {
    DidFinishDOMMutation();
    return;
  }
  const Position& new_anchor = UpdatePositionAfterAdoptingTextReplacement(
      selection_.anchor_, node, offset, old_length, new_length);
  const Position& new_focus = UpdatePositionAfterAdoptingTextReplacement(
      selection_.focus_, node, offset, old_length, new_length);
  DidFinishTextChange(new_anchor, new_focus);
}

static Position UpdatePostionAfterAdoptingTextNodesMerged(
    const Position& position,
    const Text& merged_node,
    const NodeWithIndex& node_to_be_removed_with_index,
    unsigned old_length) {
  Node* const anchor_node = position.AnchorNode();
  const Node& node_to_be_removed = node_to_be_removed_with_index.GetNode();
  switch (position.AnchorType()) {
    case PositionAnchorType::kAfterChildren:
      return position;
    case PositionAnchorType::kBeforeAnchor:
      if (anchor_node == node_to_be_removed)
        return Position(merged_node, merged_node.length());
      return position;
    case PositionAnchorType::kAfterAnchor:
      if (anchor_node == node_to_be_removed)
        return Position(merged_node, merged_node.length());
      if (anchor_node == merged_node)
        return Position(merged_node, old_length);
      return position;
    case PositionAnchorType::kOffsetInAnchor: {
      const int offset = position.OffsetInContainerNode();
      if (anchor_node == node_to_be_removed)
        return Position(merged_node, old_length + offset);
      if (anchor_node == node_to_be_removed.parentNode() &&
          offset == node_to_be_removed_with_index.Index()) {
        return Position(merged_node, old_length);
      }
      return position;
    }
  }
  NOTREACHED_IN_MIGRATION() << position;
  return position;
}

void SelectionEditor::DidMergeTextNodes(
    const Text& merged_node,
    const NodeWithIndex& node_to_be_removed_with_index,
    unsigned old_length) {
  if (selection_.IsNone()) {
    DidFinishDOMMutation();
    return;
  }
  const Position& new_anchor = UpdatePostionAfterAdoptingTextNodesMerged(
      selection_.anchor_, merged_node, node_to_be_removed_with_index,
      old_length);
  const Position& new_focus = UpdatePostionAfterAdoptingTextNodesMerged(
      selection_.focus_, merged_node, node_to_be_removed_with_index,
      old_length);
  DidFinishTextChange(new_anchor, new_focus);
}

static Position UpdatePostionAfterAdoptingTextNodeSplit(
    const Position& position,
    const Text& old_node) {
  if (!position.AnchorNode() || position.AnchorNode() != &old_node ||
      !position.IsOffsetInAnchor())
    return position;
  // See:
  // http://www.w3.org/TR/DOM-Level-2-Traversal-Range/ranges.html#Level-2-Range-Mutation
  DCHECK_GE(position.OffsetInContainerNode(), 0);
  unsigned position_offset =
      static_cast<unsigned>(position.OffsetInContainerNode());
  unsigned old_length = old_node.length();
  if (position_offset <= old_length)
    return position;
  return Position(To<Text>(old_node.nextSibling()),
                  position_offset - old_length);
}

void SelectionEditor::DidSplitTextNode(const Text& old_node) {
  if (selection_.IsNone() || !old_node.isConnected()) {
    DidFinishDOMMutation();
    return;
  }
  const Position& new_anchor =
      UpdatePostionAfterAdoptingTextNodeSplit(selection_.anchor_, old_node);
  const Position& new_focus =
      UpdatePostionAfterAdoptingTextNodeSplit(selection_.focus_, old_node);
  DidFinishTextChange(new_anchor, new_focus);
}

bool SelectionEditor::ShouldAlwaysUseDirectionalSelection() const {
  return GetFrame()
      ->GetEditor()
      .Behavior()
      .ShouldConsiderSelectionAsDirectional();
}

bool SelectionEditor::NeedsUpdateVisibleSelection() const {
#if DCHECK_IS_ON()
  // Verify that cache has been marked dirty on style changes
  DCHECK(cached_visible_selection_in_dom_tree_is_dirty_ ||
         style_version_for_dom_tree_ == GetDocument().StyleVersion());
#endif
  return cached_visible_selection_in_dom_tree_is_dirty_;
}

void SelectionEditor::UpdateCachedVisibleSelectionIfNeeded() const {
  // Note: Since we |FrameCaret::updateApperance()| is called from
  // |FrameView::performPostLayoutTasks()|, we check lifecycle against
  // |AfterPerformLayout| instead of |LayoutClean|.
  CHECK_GE(GetDocument().Lifecycle().GetState(),
           DocumentLifecycle::kAfterPerformLayout);
  AssertSelectionValid();
  if (!NeedsUpdateVisibleSelection())
    return;
#if DCHECK_IS_ON()
  style_version_for_dom_tree_ = GetDocument().StyleVersion();
#endif
  cached_visible_selection_in_dom_tree_is_dirty_ = false;
  cached_visible_selection_in_dom_tree_ = CreateVisibleSelection(selection_);
  if (!cached_visible_selection_in_dom_tree_.IsNone())
    return;
#if DCHECK_IS_ON()
  style_version_for_flat_tree_ = GetDocument().StyleVersion();
#endif
  cached_visible_selection_in_flat_tree_is_dirty_ = false;
  cached_visible_selection_in_flat_tree_ = VisibleSelectionInFlatTree();
}

bool SelectionEditor::NeedsUpdateVisibleSelectionInFlatTree() const {
#if DCHECK_IS_ON()
  // Verify that cache has been marked dirty on style changes
  DCHECK(cached_visible_selection_in_flat_tree_is_dirty_ ||
         style_version_for_flat_tree_ == GetDocument().StyleVersion());
#endif
  return cached_visible_selection_in_flat_tree_is_dirty_;
}

void SelectionEditor::UpdateCachedVisibleSelectionInFlatTreeIfNeeded() const {
  // Note: Since we |FrameCaret::updateApperance()| is called from
  // |FrameView::performPostLayoutTasks()|, we check lifecycle against
  // |AfterPerformLayout| instead of |LayoutClean|.
  DCHECK_GE(GetDocument().Lifecycle().GetState(),
            DocumentLifecycle::kAfterPerformLayout);
  AssertSelectionValid();
  if (!NeedsUpdateVisibleSelectionInFlatTree())
    return;
#if DCHECK_IS_ON()
  style_version_for_flat_tree_ = GetDocument().StyleVersion();
#endif
  cached_visible_selection_in_flat_tree_is_dirty_ = false;
  cached_visible_selection_in_flat_tree_ =
      CreateVisibleSelection(ConvertToSelectionInFlatTree(selection_));
  if (!cached_visible_selection_in_flat_tree_.IsNone())
    return;
#if DCHECK_IS_ON()
  style_version_for_dom_tree_ = GetDocument().StyleVersion();
#endif
  cached_visible_selection_in_dom_tree_is_dirty_ = false;
  cached_visible_selection_in_dom_tree_ = VisibleSelection();
}

bool SelectionEditor::NeedsUpdateAbsoluteBounds() const {
#if DCHECK_IS_ON()
  // Verify that cache has been marked dirty on style changes
  DCHECK(cached_absolute_bounds_are_dirty_ ||
         style_version_for_absolute_bounds_ == GetDocument().StyleVersion());
#endif
  return cached_absolute_bounds_are_dirty_;
}

void SelectionEditor::UpdateCachedAbsoluteBoundsIfNeeded() const {
  // Note: Since we |FrameCaret::updateApperance()| is called from
  // |FrameView::performPostLayoutTasks()|, we check lifecycle against
  // |AfterPerformLayout| instead of |LayoutClean|.
  DCHECK_GE(GetDocument().Lifecycle().GetState(),
            DocumentLifecycle::kAfterPerformLayout);
  AssertSelectionValid();
  if (!NeedsUpdateAbsoluteBounds())
    return;

  DocumentLifecycle::DisallowTransitionScope disallow_transition(
      frame_->GetDocument()->Lifecycle());

#if DCHECK_IS_ON()
  style_version_for_absolute_bounds_ = GetDocument().StyleVersion();
#endif
  cached_absolute_bounds_are_dirty_ = false;

  const VisibleSelection selection = ComputeVisibleSelectionInDOMTree();

  if (selection.IsCaret()) {
    DCHECK(selection.IsValidFor(*frame_->GetDocument()));
    const PositionWithAffinity caret(selection.Start(), selection.Affinity());
    cached_anchor_bounds_ = cached_focus_bounds_ = AbsoluteCaretBoundsOf(caret);
  } else {
    const EphemeralRange selected_range =
        selection.ToNormalizedEphemeralRange();
    if (selected_range.IsNull()) {
      has_selection_bounds_ = false;
      return;
    }
    cached_anchor_bounds_ =
        FirstRectForRange(EphemeralRange(selected_range.StartPosition()));
    cached_focus_bounds_ =
        FirstRectForRange(EphemeralRange(selected_range.EndPosition()));
  }

  if (!selection.IsAnchorFirst()) {
    std::swap(cached_anchor_bounds_, cached_focus_bounds_);
  }

  has_selection_bounds_ = true;
}

void SelectionEditor::CacheRangeOfDocument(Range* range) {
  cached_range_ = range;
}

Range* SelectionEditor::DocumentCachedRange() const {
  return cached_range_.Get();
}

void SelectionEditor::ClearDocumentCachedRange() {
  cached_range_ = nullptr;
}

void SelectionEditor::Trace(Visitor* visitor) const {
  visitor->Trace(frame_);
  visitor->Trace(selection_);
  visitor->Trace(cached_visible_selection_in_dom_tree_);
  visitor->Trace(cached_visible_selection_in_flat_tree_);
  visitor->Trace(cached_range_);
  SynchronousMutationObserver::Trace(visitor);
}

}  // namespace blink
