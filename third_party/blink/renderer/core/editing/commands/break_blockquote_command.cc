/*
 * Copyright (C) 2005 Apple Computer, Inc.  All rights reserved.
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

#include "third_party/blink/renderer/core/editing/commands/break_blockquote_command.h"

#include "third_party/blink/renderer/core/dom/node_traversal.h"
#include "third_party/blink/renderer/core/dom/text.h"
#include "third_party/blink/renderer/core/editing/commands/delete_selection_options.h"
#include "third_party/blink/renderer/core/editing/commands/editing_commands_utilities.h"
#include "third_party/blink/renderer/core/editing/editing_utilities.h"
#include "third_party/blink/renderer/core/editing/selection_template.h"
#include "third_party/blink/renderer/core/editing/visible_position.h"
#include "third_party/blink/renderer/core/editing/visible_selection.h"
#include "third_party/blink/renderer/core/editing/visible_units.h"
#include "third_party/blink/renderer/core/html/html_br_element.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/html/html_li_element.h"
#include "third_party/blink/renderer/core/html/html_olist_element.h"
#include "third_party/blink/renderer/core/html/html_quote_element.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/layout/list/layout_list_item.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

namespace {

std::optional<int> GetListItemNumber(const Node* node) {
  if (!node)
    return std::nullopt;
  // Because of elements with "display:list-item" has list item number,
  // we use layout object instead of checking |HTMLLIElement|.
  if (const auto* list_item =
          DynamicTo<LayoutListItem>(node->GetLayoutObject())) {
    return list_item->Value();
  }
  return std::nullopt;
}

bool IsFirstVisiblePositionInNode(const VisiblePosition& visible_position,
                                  const ContainerNode* node) {
  if (visible_position.IsNull())
    return false;

  if (!visible_position.DeepEquivalent().ComputeContainerNode()->IsDescendantOf(
          node))
    return false;

  VisiblePosition previous = PreviousPositionOf(visible_position);
  return previous.IsNull() ||
         !previous.DeepEquivalent().AnchorNode()->IsDescendantOf(node);
}

bool IsLastVisiblePositionInNode(const VisiblePosition& visible_position,
                                 const ContainerNode* node) {
  if (visible_position.IsNull())
    return false;

  if (!visible_position.DeepEquivalent().ComputeContainerNode()->IsDescendantOf(
          node))
    return false;

  VisiblePosition next = NextPositionOf(visible_position);
  return next.IsNull() ||
         !next.DeepEquivalent().AnchorNode()->IsDescendantOf(node);
}

}  // namespace

BreakBlockquoteCommand::BreakBlockquoteCommand(Document& document)
    : CompositeEditCommand(document) {}

static HTMLQuoteElement* TopBlockquoteOf(const Position& start) {
  // This is a position equivalent to the caret.  We use |downstream()| so that
  // |position| will be in the first node that we need to move (there are a few
  // exceptions to this, see |doApply|).
  const Position& position = MostForwardCaretPosition(start);
  return To<HTMLQuoteElement>(
      HighestEnclosingNodeOfType(position, IsMailHTMLBlockquoteElement));
}

void BreakBlockquoteCommand::DoApply(EditingState* editing_state) {
  if (EndingSelection().IsNone())
    return;

  if (!TopBlockquoteOf(EndingVisibleSelection().Start()))
    return;

  // Delete the current selection.
  if (EndingSelection().IsRange()) {
    if (!DeleteSelection(editing_state, DeleteSelectionOptions::Builder()
                                            .SetExpandForSpecialElements(true)
                                            .SetSanitizeMarkup(true)
                                            .Build()))
      return;
  }

  // This is a scenario that should never happen, but we want to
  // make sure we don't dereference a null pointer below.

  DCHECK(!EndingSelection().IsNone());

  if (EndingSelection().IsNone())
    return;

  const VisibleSelection& visible_selection = EndingVisibleSelection();
  VisiblePosition visible_pos = visible_selection.VisibleStart();

  // pos is a position equivalent to the caret.  We use downstream() so that pos
  // will be in the first node that we need to move (there are a few exceptions
  // to this, see below).
  Position pos = MostForwardCaretPosition(visible_selection.Start());

  // Find the top-most blockquote from the start.
  HTMLQuoteElement* const top_blockquote =
      TopBlockquoteOf(visible_selection.Start());
  if (!top_blockquote || !top_blockquote->parentNode())
    return;

  auto* break_element = MakeGarbageCollected<HTMLBRElement>(GetDocument());

  bool is_last_vis_pos_in_node =
      IsLastVisiblePositionInNode(visible_pos, top_blockquote);

  // If the position is at the beginning of the top quoted content, we don't
  // need to break the quote. Instead, insert the break before the blockquote,
  // unless the position is as the end of the the quoted content.
  if (IsFirstVisiblePositionInNode(visible_pos, top_blockquote) &&
      !is_last_vis_pos_in_node) {
    InsertNodeBefore(break_element, top_blockquote, editing_state);
    if (editing_state->IsAborted())
      return;
    SetEndingSelection(SelectionForUndoStep::From(
        SelectionInDOMTree::Builder()
            .Collapse(Position::BeforeNode(*break_element))
            .Build()));
    RebalanceWhitespace();
    return;
  }

  // Insert a break after the top blockquote.
  InsertNodeAfter(break_element, top_blockquote, editing_state);
  if (editing_state->IsAborted())
    return;

  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kEditing);

  // If we're inserting the break at the end of the quoted content, we don't
  // need to break the quote.
  if (is_last_vis_pos_in_node) {
    SetEndingSelection(SelectionForUndoStep::From(
        SelectionInDOMTree::Builder()
            .Collapse(Position::BeforeNode(*break_element))
            .Build()));
    RebalanceWhitespace();
    return;
  }

  // Don't move a line break just after the caret.  Doing so would create an
  // extra, empty paragraph in the new blockquote.
  if (LineBreakExistsAtVisiblePosition(visible_pos)) {
    pos = NextPositionOf(pos, PositionMoveType::kGraphemeCluster);
  }

  // Adjust the position so we don't split at the beginning of a quote.
  while (IsFirstVisiblePositionInNode(CreateVisiblePosition(pos),
                                      To<HTMLQuoteElement>(EnclosingNodeOfType(
                                          pos, IsMailHTMLBlockquoteElement)))) {
    pos = PreviousPositionOf(pos, PositionMoveType::kGraphemeCluster);
  }

  // startNode is the first node that we need to move to the new blockquote.
  Node* start_node = pos.AnchorNode();
  DCHECK(start_node);

  // Split at pos if in the middle of a text node.
  if (auto* text_node = DynamicTo<Text>(start_node)) {
    int text_offset = pos.ComputeOffsetInContainerNode();
    if ((unsigned)text_offset >= text_node->length()) {
      start_node = NodeTraversal::Next(*start_node);
      DCHECK(start_node);
    } else if (text_offset > 0) {
      SplitTextNode(text_node, text_offset);
    }
  } else if (pos.ComputeEditingOffset() > 0) {
    Node* child_at_offset =
        NodeTraversal::ChildAt(*start_node, pos.ComputeEditingOffset());
    start_node =
        child_at_offset ? child_at_offset : NodeTraversal::Next(*start_node);
    DCHECK(start_node);
  }

  // If there's nothing inside topBlockquote to move, we're finished.
  if (!start_node->IsDescendantOf(top_blockquote)) {
    SetEndingSelection(SelectionForUndoStep::From(
        SelectionInDOMTree::Builder()
            .Collapse(FirstPositionInOrBeforeNode(*start_node))
            .Build()));
    return;
  }

  // Build up list of ancestors in between the start node and the top
  // blockquote.
  HeapVector<Member<Element>> ancestors;
  for (Element* node = start_node->parentElement();
       node && node != top_blockquote; node = node->parentElement())
    ancestors.push_back(node);

  // Insert a clone of the top blockquote after the break.
  Element& cloned_blockquote = top_blockquote->CloneWithoutChildren();
  InsertNodeAfter(&cloned_blockquote, break_element, editing_state);
  if (editing_state->IsAborted())
    return;

  // Clone startNode's ancestors into the cloned blockquote.
  // On exiting this loop, clonedAncestor is the lowest ancestor
  // that was cloned (i.e. the clone of either ancestors.last()
  // or clonedBlockquote if ancestors is empty).
  Element* cloned_ancestor = &cloned_blockquote;
  for (wtf_size_t i = ancestors.size(); i != 0; --i) {
    Element& cloned_child = ancestors[i - 1]->CloneWithoutChildren();
    // Preserve list item numbering in cloned lists.
    if (IsA<HTMLOListElement>(cloned_child)) {
      Node* list_child_node = i > 1 ? ancestors[i - 2].Get() : start_node;
      // The first child of the cloned list might not be a list item element,
      // find the first one so that we know where to start numbering.
      while (list_child_node && !IsA<HTMLLIElement>(*list_child_node))
        list_child_node = list_child_node->nextSibling();
      if (auto list_item_number = GetListItemNumber(list_child_node)) {
        SetNodeAttribute(&cloned_child, html_names::kStartAttr,
                         AtomicString::Number(*list_item_number));
      }
    }

    AppendNode(&cloned_child, cloned_ancestor, editing_state);
    if (editing_state->IsAborted())
      return;
    cloned_ancestor = &cloned_child;
  }

  MoveRemainingSiblingsToNewParent(start_node, nullptr, cloned_ancestor,
                                   editing_state);
  if (editing_state->IsAborted())
    return;

  if (!ancestors.empty()) {
    // Split the tree up the ancestor chain until the topBlockquote
    // Throughout this loop, clonedParent is the clone of ancestor's parent.
    // This is so we can clone ancestor's siblings and place the clones
    // into the clone corresponding to the ancestor's parent.
    Element* ancestor = nullptr;
    Element* cloned_parent = nullptr;
    for (ancestor = ancestors.front(),
        cloned_parent = cloned_ancestor->parentElement();
         ancestor && ancestor != top_blockquote;
         ancestor = ancestor->parentElement(),
        cloned_parent = cloned_parent->parentElement()) {
      MoveRemainingSiblingsToNewParent(ancestor->nextSibling(), nullptr,
                                       cloned_parent, editing_state);
      if (editing_state->IsAborted())
        return;
    }

    // If the startNode's original parent is now empty, remove it
    Element* original_parent = ancestors.front().Get();
    if (!original_parent->HasChildren()) {
      RemoveNode(original_parent, editing_state);
      if (editing_state->IsAborted())
        return;
    }
  }

  // Make sure the cloned block quote renders.
  AddBlockPlaceholderIfNeeded(&cloned_blockquote, editing_state);
  if (editing_state->IsAborted())
    return;

  // Put the selection right before the break.
  SetEndingSelection(SelectionForUndoStep::From(
      SelectionInDOMTree::Builder()
          .Collapse(Position::BeforeNode(*break_element))
          .Build()));
  RebalanceWhitespace();
}

}  // namespace blink
