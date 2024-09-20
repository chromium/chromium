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

#include "third_party/blink/renderer/core/editing/commands/delete_selection_command.h"

#include "base/ranges/algorithm.h"
#include "build/build_config.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/node_traversal.h"
#include "third_party/blink/renderer/core/dom/text.h"
#include "third_party/blink/renderer/core/editing/commands/editing_commands_utilities.h"
#include "third_party/blink/renderer/core/editing/editing_boundary.h"
#include "third_party/blink/renderer/core/editing/editing_utilities.h"
#include "third_party/blink/renderer/core/editing/editor.h"
#include "third_party/blink/renderer/core/editing/ephemeral_range.h"
#include "third_party/blink/renderer/core/editing/local_caret_rect.h"
#include "third_party/blink/renderer/core/editing/relocatable_position.h"
#include "third_party/blink/renderer/core/editing/selection_template.h"
#include "third_party/blink/renderer/core/editing/visible_position.h"
#include "third_party/blink/renderer/core/editing/visible_units.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/html/forms/html_input_element.h"
#include "third_party/blink/renderer/core/html/html_br_element.h"
#include "third_party/blink/renderer/core/html/html_hr_element.h"
#include "third_party/blink/renderer/core/html/html_link_element.h"
#include "third_party/blink/renderer/core/html/html_style_element.h"
#include "third_party/blink/renderer/core/html/html_table_element.h"
#include "third_party/blink/renderer/core/html/html_table_row_element.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/layout/layout_box.h"
#include "third_party/blink/renderer/core/layout/layout_text.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

static bool IsTableCellEmpty(Node* cell) {
  DCHECK(cell);
  DCHECK(IsTableCell(cell)) << cell;
  return VisiblePosition::FirstPositionInNode(*cell).DeepEquivalent() ==
         VisiblePosition::LastPositionInNode(*cell).DeepEquivalent();
}

static bool IsTableRowEmpty(Node* row) {
  if (!IsA<HTMLTableRowElement>(row))
    return false;

  row->GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kEditing);
  for (Node* child = row->firstChild(); child; child = child->nextSibling()) {
    if (IsTableCell(child) && !IsTableCellEmpty(child))
      return false;
  }
  return true;
}

static bool CanMergeListElements(Element* first_list, Element* second_list) {
  if (!first_list || !second_list || first_list == second_list)
    return false;

  return CanMergeLists(*first_list, *second_list);
}

DeleteSelectionCommand::DeleteSelectionCommand(
    Document& document,
    const DeleteSelectionOptions& options,
    InputEvent::InputType input_type,
    const Position& reference_move_position)
    : CompositeEditCommand(document),
      options_(options),
      has_selection_to_delete_(false),
      merge_blocks_after_delete_(options.IsMergeBlocksAfterDelete()),
      input_type_(input_type),
      reference_move_position_(reference_move_position) {}

DeleteSelectionCommand::DeleteSelectionCommand(
    const SelectionForUndoStep& selection,
    const DeleteSelectionOptions& options,
    InputEvent::InputType input_type)
    : CompositeEditCommand(*selection.Anchor().GetDocument()),
      options_(options),
      has_selection_to_delete_(true),
      merge_blocks_after_delete_(options.IsMergeBlocksAfterDelete()),
      input_type_(input_type),
      selection_to_delete_(selection) {}

void DeleteSelectionCommand::InitializeStartEnd(Position& start,
                                                Position& end) {
  DCHECK(!GetDocument().NeedsLayoutTreeUpdate());
  DocumentLifecycle::DisallowTransitionScope disallow_transition(
      GetDocument().Lifecycle());

  HTMLElement* start_special_container = nullptr;
  HTMLElement* end_special_container = nullptr;

  start = selection_to_delete_.Start();
  end = selection_to_delete_.End();

  // For HRs, we'll get a position at (HR,1) when hitting delete from the
  // beginning of the previous line, or (HR,0) when forward deleting, but in
  // these cases, we want to delete it, so manually expand the selection
  if (IsA<HTMLHRElement>(*start.AnchorNode()))
    start = Position::BeforeNode(*start.AnchorNode());
  else if (IsA<HTMLHRElement>(*end.AnchorNode()))
    end = Position::AfterNode(*end.AnchorNode());

  // FIXME: This is only used so that moveParagraphs can avoid the bugs in
  // special element expansion.
  if (!options_.IsExpandForSpecialElements())
    return;

  while (true) {
    start_special_container = nullptr;
    end_special_container = nullptr;

    Position s =
        PositionBeforeContainingSpecialElement(start, &start_special_container);
    Position e =
        PositionAfterContainingSpecialElement(end, &end_special_container);

    if (!start_special_container && !end_special_container)
      break;

    if (CreateVisiblePosition(start).DeepEquivalent() !=
            CreateVisiblePosition(selection_to_delete_.Start())
                .DeepEquivalent() ||
        CreateVisiblePosition(end).DeepEquivalent() !=
            CreateVisiblePosition(selection_to_delete_.End()).DeepEquivalent())
      break;

    // If we're going to expand to include the startSpecialContainer, it must be
    // fully selected.
    if (start_special_container && !end_special_container &&
        ComparePositions(Position::InParentAfterNode(*start_special_container),
                         end) > -1)
      break;

    // If we're going to expand to include the endSpecialContainer, it must be
    // fully selected.
    if (end_special_container && !start_special_container &&
        ComparePositions(
            start, Position::InParentBeforeNode(*end_special_container)) > -1)
      break;

    if (start_special_container &&
        start_special_container->IsDescendantOf(end_special_container)) {
      // Don't adjust the end yet, it is the end of a special element that
      // contains the start special element (which may or may not be fully
      // selected).
      start = s;
    } else if (end_special_container &&
               end_special_container->IsDescendantOf(start_special_container)) {
      // Don't adjust the start yet, it is the start of a special element that
      // contains the end special element (which may or may not be fully
      // selected).
      end = e;
    } else {
      start = s;
      end = e;
    }
  }
}

void DeleteSelectionCommand::SetStartingSelectionOnSmartDelete(
    const Position& start,
    const Position& end) {
  DCHECK(!GetDocument().NeedsLayoutTreeUpdate());
  DocumentLifecycle::DisallowTransitionScope disallow_transition(
      GetDocument().Lifecycle());

  const bool is_base_first = StartingSelection().IsAnchorFirst();
  // TODO(yosin): We should not call |createVisiblePosition()| here and use
  // |start| and |end| as base/extent since |VisibleSelection| also calls
  // |createVisiblePosition()| during construction.
  // Because of |newBase.affinity()| can be |Upstream|, we can't simply
  // use |start| and |end| here.
  VisiblePosition new_base = CreateVisiblePosition(is_base_first ? start : end);
  VisiblePosition new_extent =
      CreateVisiblePosition(is_base_first ? end : start);
  SelectionInDOMTree::Builder builder;
  builder.SetAffinity(new_base.Affinity())
      .SetBaseAndExtentDeprecated(new_base.DeepEquivalent(),
                                  new_extent.DeepEquivalent());
  const VisibleSelection& visible_selection =
      CreateVisibleSelection(builder.Build());
  SetStartingSelection(
      SelectionForUndoStep::From(visible_selection.AsSelection()));
}

// This assumes that it starts in editable content.
static Position TrailingWhitespacePosition(const Position& position,
                                           WhitespacePositionOption option) {
  DCHECK(!NeedsLayoutTreeUpdate(position));
  DCHECK(IsEditablePosition(position)) << position;
  if (position.IsNull())
    return Position();

  const VisiblePosition visible_position = CreateVisiblePosition(position);
  const UChar character_after_visible_position =
      CharacterAfter(visible_position);
  const bool is_space =
      option == kConsiderNonCollapsibleWhitespace
          ? (IsSpaceOrNewline(character_after_visible_position) ||
             character_after_visible_position == kNoBreakSpaceCharacter)
          : IsCollapsibleWhitespace(character_after_visible_position);
  // The space must not be in another paragraph and it must be editable.
  if (is_space && !IsEndOfParagraph(visible_position) &&
      NextPositionOf(visible_position, kCannotCrossEditingBoundary).IsNotNull())
    return position;
  return Position();
}

// Workaround: GCC fails to resolve overloaded template functions, passed as
// parameters of EnclosingNodeType. But it works wrapping that in a utility
// function.
#if defined(COMPILER_GCC)
static bool IsHTMLTableRowElement(const blink::Node* node) {
  return IsA<HTMLTableRowElement>(node);
}
#endif

void DeleteSelectionCommand::InitializePositionData(
    EditingState* editing_state) {
  DCHECK(!GetDocument().NeedsLayoutTreeUpdate());
  DocumentLifecycle::DisallowTransitionScope disallow_transition(
      GetDocument().Lifecycle());

  Position start, end;
  InitializeStartEnd(start, end);
  DCHECK(start.IsNotNull());
  DCHECK(end.IsNotNull());
  if (!IsEditablePosition(start)) {
    editing_state->Abort();
    return;
  }
  if (!IsEditablePosition(end)) {
    if (!RuntimeEnabledFeatures::
            HandleDeletionWithNonEditableContentAtBlockBoundaryEnabled() ||
        !(end.IsAfterAnchor() ||
          Position::LastPositionInNode(*(end.AnchorNode()))
              .IsEquivalent(end))) {
      Node* highest_root = HighestEditableRoot(start);
      DCHECK(highest_root);
      end = LastEditablePositionBeforePositionInRoot(end, *highest_root);
    }
  }

  upstream_start_ = MostBackwardCaretPosition(start);
  downstream_start_ = MostForwardCaretPosition(start);
  upstream_end_ = MostBackwardCaretPosition(end);
  downstream_end_ = MostForwardCaretPosition(end);

  start_root_ = RootEditableElementOf(start);
  end_root_ = RootEditableElementOf(end);

#if defined(COMPILER_GCC)
  // Workaround. See declaration of IsHTMLTableRowElement
  start_table_row_ = To<HTMLTableRowElement>(
      EnclosingNodeOfType(start, &IsHTMLTableRowElement));
  end_table_row_ =
      To<HTMLTableRowElement>(EnclosingNodeOfType(end, &IsHTMLTableRowElement));
#else
  start_table_row_ = To<HTMLTableRowElement>(
      EnclosingNodeOfType(start, &IsA<HTMLTableRowElement>));
  end_table_row_ = To<HTMLTableRowElement>(
      EnclosingNodeOfType(end, &IsA<HTMLTableRowElement>));
#endif

  // Don't move content out of a table cell.
  // If the cell is non-editable, enclosingNodeOfType won't return it by
  // default, so tell that function that we don't care if it returns
  // non-editable nodes.
  Node* start_cell = EnclosingNodeOfType(upstream_start_, &IsTableCell,
                                         kCanCrossEditingBoundary);
  Node* end_cell = EnclosingNodeOfType(downstream_end_, &IsTableCell,
                                       kCanCrossEditingBoundary);
  // FIXME: This isn't right.  A borderless table with two rows and a single
  // column would appear as two paragraphs.
  if (end_cell && end_cell != start_cell)
    merge_blocks_after_delete_ = false;

  // Usually the start and the end of the selection to delete are pulled
  // together as a result of the deletion. Sometimes they aren't (like when no
  // merge is requested), so we must choose one position to hold the caret
  // and receive the placeholder after deletion.
  VisiblePosition visible_end = CreateVisiblePosition(downstream_end_);
  if (merge_blocks_after_delete_ && !IsEndOfParagraph(visible_end))
    ending_position_ = downstream_end_;
  else
    ending_position_ = downstream_start_;

  // We don't want to merge into a block if it will mean changing the quote
  // level of content after deleting selections that contain a whole number
  // paragraphs plus a line break, since it is unclear to most users that such a
  // selection actually ends at the start of the next paragraph. This matches
  // TextEdit behavior for indented paragraphs.
  // Only apply this rule if the endingSelection is a range selection.  If it is
  // a caret, then other operations have created the selection we're deleting
  // (like the process of creating a selection to delete during a backspace),
  // and the user isn't in the situation described above.
  if (NumEnclosingMailBlockquotes(start) != NumEnclosingMailBlockquotes(end) &&
      IsStartOfParagraph(visible_end) &&
      IsStartOfParagraph(CreateVisiblePosition(start)) &&
      EndingSelection().IsRange()) {
    merge_blocks_after_delete_ = false;
    prune_start_block_if_necessary_ = true;
  }

  // Handle leading and trailing whitespace, as well as smart delete adjustments
  // to the selection
  leading_whitespace_ = LeadingCollapsibleWhitespacePosition(
      upstream_start_, selection_to_delete_.Affinity());
  trailing_whitespace_ =
      IsEditablePosition(downstream_end_)
          ? TrailingWhitespacePosition(downstream_end_,
                                       kNotConsiderNonCollapsibleWhitespace)
          : Position();

  if (options_.IsSmartDelete()) {
    // skip smart delete if the selection to delete already starts or ends with
    // whitespace
    Position pos =
        CreateVisiblePosition(upstream_start_, selection_to_delete_.Affinity())
            .DeepEquivalent();
    bool skip_smart_delete =
        TrailingWhitespacePosition(pos, kConsiderNonCollapsibleWhitespace)
            .IsNotNull();
    if (!skip_smart_delete) {
      skip_smart_delete = LeadingCollapsibleWhitespacePosition(
                              downstream_end_, TextAffinity::kDefault,
                              kConsiderNonCollapsibleWhitespace)
                              .IsNotNull();
    }

    // extend selection upstream if there is whitespace there
    bool has_leading_whitespace_before_adjustment =
        LeadingCollapsibleWhitespacePosition(upstream_start_,
                                             selection_to_delete_.Affinity(),
                                             kConsiderNonCollapsibleWhitespace)
            .IsNotNull();
    if (!skip_smart_delete && has_leading_whitespace_before_adjustment) {
      VisiblePosition visible_pos =
          PreviousPositionOf(CreateVisiblePosition(upstream_start_));
      pos = visible_pos.DeepEquivalent();
      // Expand out one character upstream for smart delete and recalculate
      // positions based on this change.
      upstream_start_ = MostBackwardCaretPosition(pos);
      downstream_start_ = MostForwardCaretPosition(pos);
      leading_whitespace_ = LeadingCollapsibleWhitespacePosition(
          upstream_start_, visible_pos.Affinity());

      SetStartingSelectionOnSmartDelete(upstream_start_, upstream_end_);
    }

    // trailing whitespace is only considered for smart delete if there is no
    // leading whitespace, as in the case where you double-click the first word
    // of a paragraph.
    if (!skip_smart_delete && !has_leading_whitespace_before_adjustment &&
        TrailingWhitespacePosition(downstream_end_,
                                   kConsiderNonCollapsibleWhitespace)
            .IsNotNull()) {
      // Expand out one character downstream for smart delete and recalculate
      // positions based on this change.
      pos = NextPositionOf(CreateVisiblePosition(downstream_end_))
                .DeepEquivalent();
      upstream_end_ = MostBackwardCaretPosition(pos);
      downstream_end_ = MostForwardCaretPosition(pos);
      trailing_whitespace_ = TrailingWhitespacePosition(
          downstream_end_, kNotConsiderNonCollapsibleWhitespace);

      SetStartingSelectionOnSmartDelete(downstream_start_, downstream_end_);
    }
  }

  // We must pass call parentAnchoredEquivalent on the positions since some
  // editing positions that appear inside their nodes aren't really inside them.
  // [hr, 0] is one example.
  // FIXME: parentAnchoredEquivalent should eventually be moved into enclosing
  // element getters like the one below, since editing functions should
  // obviously accept editing positions.
  // FIXME: Passing false to enclosingNodeOfType tells it that it's OK to return
  // a non-editable node.  This was done to match existing behavior, but it
  // seems wrong.
  start_block_ =
      EnclosingNodeOfType(downstream_start_.ParentAnchoredEquivalent(),
                          &IsEnclosingBlock, kCanCrossEditingBoundary);
  end_block_ = EnclosingNodeOfType(upstream_end_.ParentAnchoredEquivalent(),
                                   &IsEnclosingBlock, kCanCrossEditingBoundary);
}

// We don't want to inherit style from an element which can't have contents.
static bool ShouldNotInheritStyleFrom(const Node& node) {
  return !node.CanContainRangeEndPoint();
}

void DeleteSelectionCommand::SaveTypingStyleState() {
  // A common case is deleting characters that are all from the same text node.
  // In that case, the style at the start of the selection before deletion will
  // be the same as the style at the start of the selection after deletion
  // (since those two positions will be identical). Therefore there is no need
  // to save the typing style at the start of the selection, nor is there a
  // reason to compute the style at the start of the selection after deletion
  // (see the early return in calculateTypingStyleAfterDelete).
  if (upstream_start_.AnchorNode() == downstream_end_.AnchorNode() &&
      upstream_start_.AnchorNode()->IsTextNode())
    return;

  if (ShouldNotInheritStyleFrom(*selection_to_delete_.Start().AnchorNode()))
    return;

  // Figure out the typing style in effect before the delete is done.
  typing_style_ = MakeGarbageCollected<EditingStyle>(
      selection_to_delete_.Start(), EditingStyle::kEditingPropertiesInEffect);
  typing_style_->RemoveStyleAddedByElement(
      EnclosingAnchorElement(selection_to_delete_.Start()));

  // If we're deleting into a Mail blockquote, save the style at end() instead
  // of start(). We'll use this later in computeTypingStyleAfterDelete if we end
  // up outside of a Mail blockquote
  if (EnclosingNodeOfType(selection_to_delete_.Start(),
                          IsMailHTMLBlockquoteElement)) {
    delete_into_blockquote_style_ =
        MakeGarbageCollected<EditingStyle>(selection_to_delete_.End());
    return;
  }
  delete_into_blockquote_style_ = nullptr;
}

bool DeleteSelectionCommand::HandleSpecialCaseBRDelete(
    EditingState* editing_state) {
  Node* node_after_upstream_start = upstream_start_.ComputeNodeAfterPosition();
  Node* node_after_downstream_start =
      downstream_start_.ComputeNodeAfterPosition();
  // Upstream end will appear before BR due to canonicalization
  Node* node_after_upstream_end = upstream_end_.ComputeNodeAfterPosition();

  if (!node_after_upstream_start || !node_after_downstream_start)
    return false;

  // Check for special-case where the selection contains only a BR on a line by
  // itself after another BR.
  bool upstream_start_is_br = IsA<HTMLBRElement>(*node_after_upstream_start);
  bool downstream_start_is_br =
      IsA<HTMLBRElement>(*node_after_downstream_start);
  bool is_br_on_line_by_itself =
      upstream_start_is_br && downstream_start_is_br &&
      node_after_downstream_start == node_after_upstream_end;
  if (is_br_on_line_by_itself) {
    RemoveNode(node_after_downstream_start, editing_state);
    return true;
  }

  // FIXME: This code doesn't belong in here.
  // We detect the case where the start is an empty line consisting of BR not
  // wrapped in a block element.
  if (upstream_start_is_br && downstream_start_is_br) {
    GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kEditing);
    if (!(IsStartOfBlock(
              VisiblePosition::BeforeNode(*node_after_upstream_start)) &&
          IsEndOfBlock(
              VisiblePosition::AfterNode(*node_after_upstream_start)))) {
      starts_at_empty_line_ = true;
      ending_position_ = downstream_end_;
    }
  }

  return false;
}

static Position FirstEditablePositionInNode(Node* node) {
  DCHECK(node);
  Node* next = node;
  while (next && !IsEditable(*next))
    next = NodeTraversal::Next(*next, node);
  return next ? FirstPositionInOrBeforeNode(*next) : Position();
}

void DeleteSelectionCommand::RemoveNode(
    Node* node,
    EditingState* editing_state,
    ShouldAssumeContentIsAlwaysEditable
        should_assume_content_is_always_editable) {
  if (!node)
    return;

  if (start_root_ != end_root_ && !(node->IsDescendantOf(start_root_.Get()) &&
                                    node->IsDescendantOf(end_root_.Get()))) {
    // If a node is not in both the start and end editable roots, remove it only
    // if its inside an editable region.
    if (!IsEditable(*node->parentNode())) {
      // Don't remove non-editable atomic nodes.
      if (!node->hasChildren())
        return;
      // Search this non-editable region for editable regions to empty.
      // Don't remove editable regions that are inside non-editable ones, just
      // clear them.
      RemoveAllChildrenIfPossible(To<ContainerNode>(node), editing_state,
                                  should_assume_content_is_always_editable);
      return;
    }
  }

  if (IsTableStructureNode(node) || IsRootEditableElement(*node)) {
    // Do not remove an element of table structure; remove its contents.
    // Likewise for the root editable element.
    RemoveAllChildrenIfPossible(To<ContainerNode>(node), editing_state,
                                should_assume_content_is_always_editable);
    if (editing_state->IsAborted())
      return;

    // Make sure empty cell has some height, if a placeholder can be inserted.
    GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kEditing);
    LayoutObject* r = node->GetLayoutObject();
    if (r && r->IsTableCell() && To<LayoutBox>(r)->ContentHeight() <= 0) {
      Position first_editable_position = FirstEditablePositionInNode(node);
      if (first_editable_position.IsNotNull())
        InsertBlockPlaceholder(first_editable_position, editing_state);
    }
    return;
  }

  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kEditing);
  if (node == start_block_) {
    VisiblePosition previous = PreviousPositionOf(
        VisiblePosition::FirstPositionInNode(*start_block_.Get()));
    if (previous.IsNotNull() && !IsEndOfBlock(previous))
      need_placeholder_ = true;
  }
  if (node == end_block_) {
    VisiblePosition next =
        NextPositionOf(VisiblePosition::LastPositionInNode(*end_block_.Get()));
    if (next.IsNotNull() && !IsStartOfBlock(next))
      need_placeholder_ = true;
  }

  // FIXME: Update the endpoints of the range being deleted.
  ending_position_ = ComputePositionForNodeRemoval(ending_position_, *node);
  leading_whitespace_ =
      ComputePositionForNodeRemoval(leading_whitespace_, *node);
  trailing_whitespace_ =
      ComputePositionForNodeRemoval(trailing_whitespace_, *node);

  CompositeEditCommand::RemoveNode(node, editing_state,
                                   should_assume_content_is_always_editable);
}

void DeleteSelectionCommand::RemoveCompletelySelectedNodes(
    Node* start_node,
    EditingState* editing_state) {
  HeapVector<Member<Node>> nodes_to_be_removed;
  Node* node = start_node;

  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kEditing);

  // Collecting nodes that can be removed from |start_node|.
  while (node && node != downstream_end_.AnchorNode()) {
    if (ComparePositions(FirstPositionInOrBeforeNode(*node), downstream_end_) >=
        0)
      break;

    if (!downstream_end_.AnchorNode()->IsDescendantOf(node)) {
      nodes_to_be_removed.push_back(node);
      node = NodeTraversal::NextSkippingChildren(*node);
      continue;
    }

    Node& last_within_or_self_node = NodeTraversal::LastWithinOrSelf(*node);
    if (downstream_end_.AnchorNode() == last_within_or_self_node &&
        downstream_end_.ComputeEditingOffset() >=
            CaretMaxOffset(&last_within_or_self_node)) {
      nodes_to_be_removed.push_back(node);
      break;
    }

    node = NodeTraversal::Next(*node);
  }

  // Update leading, trailing whitespace position.
  if (!nodes_to_be_removed.empty()) {
    leading_whitespace_ = ComputePositionForNodeRemoval(
        leading_whitespace_, *(nodes_to_be_removed[0].Get()));
    trailing_whitespace_ = ComputePositionForNodeRemoval(
        trailing_whitespace_,
        *(nodes_to_be_removed[nodes_to_be_removed.size() - 1].Get()));
  }

  // Check if place holder is needed before actually removing nodes because
  // this requires document.NeedsLayoutTreeUpdate() returning false.
  if (!need_placeholder_) {
    need_placeholder_ =
        base::ranges::any_of(nodes_to_be_removed, [&](Node* node) {
          if (node == start_block_) {
            VisiblePosition previous = PreviousPositionOf(
                VisiblePosition::FirstPositionInNode(*start_block_.Get()));
            if (previous.IsNotNull() && !IsEndOfBlock(previous))
              return true;
          }
          if (node == end_block_) {
            VisiblePosition next = NextPositionOf(
                VisiblePosition::LastPositionInNode(*end_block_.Get()));
            if (next.IsNotNull() && !IsStartOfBlock(next))
              return true;
          }
          return false;
        });
  }

  // Actually remove the nodes in |nodes_to_be_removed|.
  for (Node* node_to_be_removed : nodes_to_be_removed) {
    if (!downstream_end_.AnchorNode()->IsDescendantOf(node_to_be_removed)) {
      downstream_end_ =
          ComputePositionForNodeRemoval(downstream_end_, *(node_to_be_removed));
    }

    if (start_root_ != end_root_ &&
        !(node_to_be_removed->IsDescendantOf(start_root_.Get()) &&
          node_to_be_removed->IsDescendantOf(end_root_.Get()))) {
      // If a node is not in both the start and end editable roots, remove it
      // only if its inside an editable region.
      if (!IsEditable(*node_to_be_removed->parentNode())) {
        // Don't remove non-editable atomic nodes.
        if (!node_to_be_removed->hasChildren())
          continue;
        // Search this non-editable region for editable regions to empty.
        // Don't remove editable regions that are inside non-editable ones, just
        // clear them.
        RemoveAllChildrenIfPossible(To<ContainerNode>(node_to_be_removed),
                                    editing_state,
                                    kDoNotAssumeContentIsAlwaysEditable);
        if (editing_state->IsAborted())
          return;

        continue;
      }
    }

    if (IsTableStructureNode(node_to_be_removed) ||
        IsRootEditableElement(*node_to_be_removed)) {
      // Do not remove an element of table structure; remove its contents.
      // Likewise for the root editable element.
      RemoveAllChildrenIfPossible(To<ContainerNode>(node_to_be_removed),
                                  editing_state,
                                  kDoNotAssumeContentIsAlwaysEditable);
      if (editing_state->IsAborted())
        return;

      // Make sure empty cell has some height, if a placeholder can be inserted.
      GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kEditing);
      LayoutObject* layout_obj = node_to_be_removed->GetLayoutObject();
      if (layout_obj && layout_obj->IsTableCell() &&
          To<LayoutBox>(layout_obj)->ContentHeight() <= 0) {
        Position first_editable_position =
            FirstEditablePositionInNode(node_to_be_removed);
        if (first_editable_position.IsNotNull())
          InsertBlockPlaceholder(first_editable_position, editing_state);
      }
      continue;
    }

    ending_position_ =
        ComputePositionForNodeRemoval(ending_position_, *node_to_be_removed);
    CompositeEditCommand::RemoveNode(node_to_be_removed, editing_state,
                                     kDoNotAssumeContentIsAlwaysEditable);
    if (editing_state->IsAborted())
      return;
  }
}

static void UpdatePositionForTextRemoval(Text* node,
                                         int offset,
                                         int count,
                                         Position& position) {
  if (!position.IsOffsetInAnchor() || position.ComputeContainerNode() != node)
    return;

  if (position.OffsetInContainerNode() > offset + count)
    position = Position(position.ComputeContainerNode(),
                        position.OffsetInContainerNode() - count);
  else if (position.OffsetInContainerNode() > offset)
    position = Position(position.ComputeContainerNode(), offset);
}

void DeleteSelectionCommand::DeleteTextFromNode(Text* node,
                                                unsigned offset,
                                                unsigned count) {
  // FIXME: Update the endpoints of the range being deleted.
  UpdatePositionForTextRemoval(node, offset, count, ending_position_);
  UpdatePositionForTextRemoval(node, offset, count, leading_whitespace_);
  UpdatePositionForTextRemoval(node, offset, count, trailing_whitespace_);
  UpdatePositionForTextRemoval(node, offset, count, downstream_end_);

  CompositeEditCommand::DeleteTextFromNode(node, offset, count);
}

void DeleteSelectionCommand::
    MakeStylingElementsDirectChildrenOfEditableRootToPreventStyleLoss(
        EditingState* editing_state) {
  Range* range = CreateRange(CreateVisibleSelection(selection_to_delete_)
                                 .ToNormalizedEphemeralRange());
  Node* node = range->FirstNode();
  while (node && node != range->PastLastNode()) {
    Node* next_node = NodeTraversal::Next(*node);
    if (IsA<HTMLStyleElement>(*node) || IsA<HTMLLinkElement>(*node)) {
      next_node = NodeTraversal::NextSkippingChildren(*node);
      Element* element = RootEditableElement(*node);
      if (element) {
        RemoveNode(node, editing_state);
        if (editing_state->IsAborted())
          return;
        AppendNode(node, element, editing_state);
        if (editing_state->IsAborted())
          return;
      }
    }
    node = next_node;
  }
}

void DeleteSelectionCommand::HandleGeneralDelete(EditingState* editing_state) {
  if (upstream_start_.IsNull())
    return;

  int start_offset = upstream_start_.ComputeEditingOffset();
  Node* start_node = upstream_start_.AnchorNode();
  DCHECK(start_node);

  MakeStylingElementsDirectChildrenOfEditableRootToPreventStyleLoss(
      editing_state);
  if (editing_state->IsAborted())
    return;

  // Never remove the start block unless it's a table, in which case we won't
  // merge content in.
  if (start_node == start_block_.Get() && !start_offset &&
      CanHaveChildrenForEditing(start_node) &&
      !IsA<HTMLTableElement>(*start_node)) {
    start_offset = 0;
    start_node = NodeTraversal::Next(*start_node);
    if (!start_node)
      return;
  }

  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kEditing);
  auto* text = DynamicTo<Text>(start_node);
  if (start_offset >= CaretMaxOffset(start_node) && text) {
    if (text->length() > static_cast<unsigned>(CaretMaxOffset(start_node))) {
      DeleteTextFromNode(text, CaretMaxOffset(start_node),
                         text->length() - CaretMaxOffset(start_node));
    }
  }

  if (start_offset >= EditingStrategy::LastOffsetForEditing(start_node)) {
    start_node = NodeTraversal::NextSkippingChildren(*start_node);
    start_offset = 0;
  }

  // Done adjusting the start.  See if we're all done.
  if (!start_node)
    return;

  if (start_node == downstream_end_.AnchorNode()) {
    if (downstream_end_.ComputeEditingOffset() - start_offset > 0) {
      if (auto* text_node_to_trim = DynamicTo<Text>(start_node)) {
        // in a text node that needs to be trimmed
        DeleteTextFromNode(
            text_node_to_trim, start_offset,
            downstream_end_.ComputeOffsetInContainerNode() - start_offset);
      } else {
        RelocatablePosition* relocatable_downstream_end =
            MakeGarbageCollected<RelocatablePosition>(downstream_end_);
        RemoveChildrenInRange(start_node, start_offset,
                              downstream_end_.ComputeEditingOffset(),
                              editing_state);
        if (editing_state->IsAborted())
          return;
        ending_position_ = upstream_start_;
        downstream_end_ = relocatable_downstream_end->GetPosition();
      }
      // We should update layout to associate |start_node| to layout object.
      GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kEditing);
    }

    // The selection to delete is all in one node.
    if (!start_node->GetLayoutObject() ||
        (!start_offset && downstream_end_.AtLastEditingPositionForNode())) {
      RemoveNode(start_node, editing_state);
      if (editing_state->IsAborted())
        return;
    }
  } else {
    bool start_node_was_descendant_of_end_node =
        upstream_start_.AnchorNode()->IsDescendantOf(
            downstream_end_.AnchorNode());

    bool end_node_is_selected_from_first_position = false;
    if (RuntimeEnabledFeatures::
            RemoveNodeHavingChildrenIfFullySelectedEnabled()) {
      end_node_is_selected_from_first_position =
          ComparePositions(upstream_start_,
                           Position::FirstPositionInNode(
                               *downstream_end_.AnchorNode())) <= 0;
    }

    // The selection to delete spans more than one node.
    Node* node(start_node);
    auto* start_text_node = DynamicTo<Text>(start_node);
    if (start_offset > 0) {
      if (start_text_node) {
        // in a text node that needs to be trimmed
        DeleteTextFromNode(start_text_node, start_offset,
                           start_text_node->length() - start_offset);
        node = NodeTraversal::Next(*node);
      } else {
        node = NodeTraversal::ChildAt(*start_node, start_offset);
      }
    } else if (start_node == upstream_end_.AnchorNode() && start_text_node) {
      DeleteTextFromNode(start_text_node, 0,
                         upstream_end_.ComputeOffsetInContainerNode());
    }

    // Delete all nodes that are completely selected
    RemoveCompletelySelectedNodes(node, editing_state);
    if (editing_state->IsAborted())
      return;

    // TODO(editing-dev): Hoist UpdateStyleAndLayout
    // to caller. See http://crbug.com/590369 for more details.
    GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kEditing);

    if (downstream_end_.AnchorNode() != start_node &&
        !upstream_start_.AnchorNode()->IsDescendantOf(
            downstream_end_.AnchorNode()) &&
        downstream_end_.IsConnected() &&
        downstream_end_.ComputeEditingOffset() >=
            CaretMinOffset(downstream_end_.AnchorNode())) {
      bool is_node_fully_selected =
          downstream_end_.AtLastEditingPositionForNode() &&
          !CanHaveChildrenForEditing(downstream_end_.AnchorNode());
      if (RuntimeEnabledFeatures::
              RemoveNodeHavingChildrenIfFullySelectedEnabled()) {
        // Even though `downstream_end_` has children, it can be fully selected.
        // Update `is_node_fully_selected` if the selection includes the first
        // position of the node.
        if (!is_node_fully_selected &&
            downstream_end_.AtLastEditingPositionForNode()) {
          is_node_fully_selected = end_node_is_selected_from_first_position;
        }
      }
      if (is_node_fully_selected) {
        // The node itself is fully selected, not just its contents.  Delete it.
        RemoveNode(downstream_end_.AnchorNode(), editing_state);
      } else {
        if (auto* text_node_to_trim =
                DynamicTo<Text>(downstream_end_.AnchorNode())) {
          // in a text node that needs to be trimmed
          if (downstream_end_.ComputeEditingOffset() > 0) {
            DeleteTextFromNode(text_node_to_trim, 0,
                               downstream_end_.ComputeEditingOffset());
          }
          // Remove children of downstream_end_.AnchorNode() that come after
          // upstream_start_. Don't try to remove children if upstream_start_
          // was inside downstream_end_.AnchorNode() and upstream_start_ has
          // been removed from the document, because then we don't know how many
          // children to remove.
          // FIXME: Make upstream_start_ a position we update as we remove
          // content, then we can always know which children to remove.
        } else if (!(start_node_was_descendant_of_end_node &&
                     !upstream_start_.IsConnected())) {
          int offset = 0;
          if (upstream_start_.AnchorNode()->IsDescendantOf(
                  downstream_end_.AnchorNode())) {
            Node* n = upstream_start_.AnchorNode();
            while (n && n->parentNode() != downstream_end_.AnchorNode())
              n = n->parentNode();
            if (n)
              offset = n->NodeIndex() + 1;
          }
          RemoveChildrenInRange(downstream_end_.AnchorNode(), offset,
                                downstream_end_.ComputeEditingOffset(),
                                editing_state);
          if (editing_state->IsAborted())
            return;
          downstream_end_ =
              Position::EditingPositionOf(downstream_end_.AnchorNode(), offset);
        }
      }
    }
  }
}

void DeleteSelectionCommand::FixupWhitespace(const Position& position) {
  auto* const text_node = DynamicTo<Text>(position.AnchorNode());
  if (!text_node)
    return;
  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kEditing);
  if (IsRenderedCharacter(position))
    return;
  DCHECK(!text_node->GetLayoutObject() ||
         text_node->GetLayoutObject()->Style()->ShouldCollapseWhiteSpaces())
      << text_node;
  ReplaceTextInNode(text_node, position.ComputeOffsetInContainerNode(), 1,
                    NonBreakingSpaceString());
}

// If a selection starts in one block and ends in another, we have to merge to
// bring content before the start together with content after the end.
void DeleteSelectionCommand::MergeParagraphs(EditingState* editing_state) {
  if (!merge_blocks_after_delete_) {
    if (prune_start_block_if_necessary_) {
      // We aren't going to merge into the start block, so remove it if it's
      // empty.
      Prune(start_block_, editing_state);
      if (editing_state->IsAborted())
        return;
      // Removing the start block during a deletion is usually an indication
      // that we need a placeholder, but not in this case.
      need_placeholder_ = false;
    }
    return;
  }

  // It shouldn't have been asked to both try and merge content into the start
  // block and prune it.
  DCHECK(!prune_start_block_if_necessary_);

  // FIXME: Deletion should adjust selection endpoints as it removes nodes so
  // that we never get into this state (4099839).
  if (!downstream_end_.IsConnected() || !upstream_start_.IsConnected())
    return;

  // FIXME: The deletion algorithm shouldn't let this happen.
  if (ComparePositions(upstream_start_, downstream_end_) > 0)
    return;

  // There's nothing to merge.
  if (upstream_start_ == downstream_end_)
    return;

  if (RuntimeEnabledFeatures::
          RemoveNodeHavingChildrenIfFullySelectedEnabled()) {
    // It can be the same position even though `upstream_start_` and
    // `downstream_end_` are not identical.
    // Compare them using ParentAnchoredEquivalent().
    if (upstream_start_.ParentAnchoredEquivalent() ==
        downstream_end_.ParentAnchoredEquivalent()) {
      return;
    }
  }

  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kEditing);

  VisiblePosition merge_origin = CreateVisiblePosition(downstream_end_);
  VisiblePosition merge_destination = CreateVisiblePosition(upstream_start_);

  // downstream_end_'s block has been emptied out by deletion.  There is no
  // content inside of it to move, so just remove it.
  Element* end_block = EnclosingBlock(downstream_end_.AnchorNode());
  if (!end_block ||
      !end_block->contains(merge_origin.DeepEquivalent().AnchorNode()) ||
      !merge_origin.DeepEquivalent().AnchorNode()) {
    RemoveNode(EnclosingBlock(downstream_end_.AnchorNode()), editing_state);
    return;
  }

  RelocatablePosition* relocatable_start =
      MakeGarbageCollected<RelocatablePosition>(merge_origin.DeepEquivalent());

  // We need to merge into upstream_start_'s block, but it's been emptied out
  // and collapsed by deletion.
  if (!merge_destination.DeepEquivalent().AnchorNode() ||
      (!merge_destination.DeepEquivalent().AnchorNode()->IsDescendantOf(
           EnclosingBlock(upstream_start_.ComputeContainerNode())) &&
       (!merge_destination.DeepEquivalent().AnchorNode()->hasChildren() ||
        !upstream_start_.ComputeContainerNode()->hasChildren())) ||
      (starts_at_empty_line_ &&
       merge_destination.DeepEquivalent() != merge_origin.DeepEquivalent())) {
    InsertNodeAt(MakeGarbageCollected<HTMLBRElement>(GetDocument()),
                 upstream_start_, editing_state);
    if (editing_state->IsAborted())
      return;
    GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kEditing);
    merge_destination = CreateVisiblePosition(upstream_start_);
    merge_origin = CreateVisiblePosition(relocatable_start->GetPosition());
  }

  if (merge_destination.DeepEquivalent() == merge_origin.DeepEquivalent())
    return;

  VisiblePosition start_of_paragraph_to_move = StartOfParagraph(merge_origin);
  VisiblePosition end_of_paragraph_to_move =
      EndOfParagraph(merge_origin, kCanSkipOverEditingBoundary);

  if (merge_destination.DeepEquivalent() ==
      end_of_paragraph_to_move.DeepEquivalent())
    return;

  // If the merge destination and source to be moved are both list items of
  // different lists, merge them into single list.
  Node* list_item_in_first_paragraph =
      EnclosingNodeOfType(upstream_start_, IsListItem);
  Node* list_item_in_second_paragraph =
      EnclosingNodeOfType(downstream_end_, IsListItem);
  if (list_item_in_first_paragraph && list_item_in_second_paragraph &&
      CanMergeListElements(list_item_in_first_paragraph->parentElement(),
                           list_item_in_second_paragraph->parentElement())) {
    MergeIdenticalElements(list_item_in_first_paragraph->parentElement(),
                           list_item_in_second_paragraph->parentElement(),
                           editing_state);
    if (editing_state->IsAborted())
      return;
    ending_position_ = merge_destination.DeepEquivalent();
    return;
  }

  // The rule for merging into an empty block is: only do so if its farther to
  // the right.
  // FIXME: Consider RTL.
  if (!starts_at_empty_line_ && IsStartOfParagraph(merge_destination) &&
      AbsoluteCaretBoundsOf(merge_origin.ToPositionWithAffinity()).x() >
          AbsoluteCaretBoundsOf(merge_destination.ToPositionWithAffinity())
              .x()) {
    if (IsA<HTMLBRElement>(
            *MostForwardCaretPosition(merge_destination.DeepEquivalent())
                 .AnchorNode())) {
      RemoveNodeAndPruneAncestors(
          MostForwardCaretPosition(merge_destination.DeepEquivalent())
              .AnchorNode(),
          editing_state);
      if (editing_state->IsAborted())
        return;
      ending_position_ = relocatable_start->GetPosition();
      return;
    }
  }

  // Block images, tables and horizontal rules cannot be made inline with
  // content at mergeDestination.  If there is any
  // (!isStartOfParagraph(mergeDestination)), don't merge, just move
  // the caret to just before the selection we deleted. See
  // https://bugs.webkit.org/show_bug.cgi?id=25439
  if (IsRenderedAsNonInlineTableImageOrHR(
          merge_origin.DeepEquivalent().AnchorNode()) &&
      !IsStartOfParagraph(merge_destination)) {
    ending_position_ = upstream_start_;
    return;
  }

  // moveParagraphs will insert placeholders if it removes blocks that would
  // require their use, don't let block removals that it does cause the
  // insertion of *another* placeholder.
  bool need_placeholder = need_placeholder_;
  bool paragraph_to_merge_is_empty =
      start_of_paragraph_to_move.DeepEquivalent() ==
      end_of_paragraph_to_move.DeepEquivalent();
  MoveParagraph(
      start_of_paragraph_to_move, end_of_paragraph_to_move, merge_destination,
      editing_state, kDoNotPreserveSelection,
      paragraph_to_merge_is_empty ? kDoNotPreserveStyle : kPreserveStyle);
  if (editing_state->IsAborted())
    return;
  need_placeholder_ = need_placeholder;
  // The endingPosition was likely clobbered by the move, so recompute it
  // (moveParagraph selects the moved paragraph).
  ending_position_ = EndingVisibleSelection().Start();
}

void DeleteSelectionCommand::RemovePreviouslySelectedEmptyTableRows(
    EditingState* editing_state) {
  if (end_table_row_ && end_table_row_->isConnected() &&
      end_table_row_ != start_table_row_) {
    Node* row = end_table_row_->previousSibling();
    while (row && row != start_table_row_) {
      Node* previous_row = row->previousSibling();
      if (IsTableRowEmpty(row)) {
        // Use a raw removeNode, instead of DeleteSelectionCommand's,
        // because that won't remove rows, it only empties them in
        // preparation for this function.
        CompositeEditCommand::RemoveNode(row, editing_state);
        if (editing_state->IsAborted())
          return;
      }
      row = previous_row;
    }
  }

  // Remove empty rows after the start row.
  if (start_table_row_ && start_table_row_->isConnected() &&
      start_table_row_ != end_table_row_) {
    Node* row = start_table_row_->nextSibling();
    while (row && row != end_table_row_) {
      Node* next_row = row->nextSibling();
      if (IsTableRowEmpty(row)) {
        CompositeEditCommand::RemoveNode(row, editing_state);
        if (editing_state->IsAborted())
          return;
      }
      row = next_row;
    }
  }

  if (end_table_row_ && end_table_row_->isConnected() &&
      end_table_row_ != start_table_row_) {
    if (IsTableRowEmpty(end_table_row_.Get())) {
      // Don't remove end_table_row_ if it's where we're putting the ending
      // selection.
      if (ending_position_.IsNull() ||
          !ending_position_.AnchorNode()->IsDescendantOf(
              end_table_row_.Get())) {
        // FIXME: We probably shouldn't remove end_table_row_ unless it's
        // fully selected, even if it is empty. We'll need to start
        // adjusting the selection endpoints during deletion to know
        // whether or not end_table_row_ was fully selected here.
        CompositeEditCommand::RemoveNode(end_table_row_.Get(), editing_state);
        if (editing_state->IsAborted())
          return;
      }
    }
  }
}

void DeleteSelectionCommand::CalculateTypingStyleAfterDelete() {
  // Clearing any previously set typing style and doing an early return.
  if (!typing_style_) {
    GetDocument().GetFrame()->GetEditor().ClearTypingStyle();
    return;
  }

  // Compute the difference between the style before the delete and the style
  // now after the delete has been done. Set this style on the frame, so other
  // editing commands being composed with this one will work, and also cache it
  // on the command, so the LocalFrame::appliedEditing can set it after the
  // whole composite command has completed.

  // If we deleted into a blockquote, but are now no longer in a blockquote, use
  // the alternate typing style
  if (delete_into_blockquote_style_ &&
      !EnclosingNodeOfType(ending_position_, IsMailHTMLBlockquoteElement,
                           kCanCrossEditingBoundary))
    typing_style_ = delete_into_blockquote_style_;
  delete_into_blockquote_style_ = nullptr;

  // |editing_position_| can be null. See http://crbug.com/1299189
  if (ending_position_.IsNotNull())
    typing_style_->PrepareToApplyAt(ending_position_);
  if (typing_style_->IsEmpty())
    typing_style_ = nullptr;
  // This is where we've deleted all traces of a style but not a whole paragraph
  // (that's handled above). In this case if we start typing, the new characters
  // should have the same style as the just deleted ones, but, if we change the
  // selection, come back and start typing that style should be lost.  Also see
  // preserveTypingStyle() below.
  GetDocument().GetFrame()->GetEditor().SetTypingStyle(typing_style_);
}

void DeleteSelectionCommand::ClearTransientState() {
  selection_to_delete_ = SelectionForUndoStep();
  upstream_start_ = Position();
  downstream_start_ = Position();
  upstream_end_ = Position();
  downstream_end_ = Position();
  ending_position_ = Position();
  leading_whitespace_ = Position();
  trailing_whitespace_ = Position();
  reference_move_position_ = Position();
}

// This method removes div elements with no attributes that have only one child
// or no children at all.
void DeleteSelectionCommand::RemoveRedundantBlocks(
    EditingState* editing_state) {
  Node* node = ending_position_.ComputeContainerNode();
  if (!node)
    return;
  Element* root_element = RootEditableElement(*node);

  while (node != root_element) {
    ABORT_EDITING_COMMAND_IF(!node);
    if (IsRemovableBlock(node)) {
      if (node == ending_position_.AnchorNode())
        UpdatePositionForNodeRemovalPreservingChildren(ending_position_, *node);

      CompositeEditCommand::RemoveNodePreservingChildren(node, editing_state);
      if (editing_state->IsAborted())
        return;
      node = ending_position_.AnchorNode();
    } else {
      node = node->parentNode();
    }
  }
}

void DeleteSelectionCommand::DoApply(EditingState* editing_state) {
  // If selection has not been set to a custom selection when the command was
  // created, use the current ending selection.
  if (!has_selection_to_delete_) {
    if (RuntimeEnabledFeatures::
            HandleDeletionWithNonEditableContentAtBlockBoundaryEnabled()) {
      selection_to_delete_ =
          SelectionForUndoStep::From(EndingSelection().AsSelection());
    } else {
      selection_to_delete_ =
          SelectionForUndoStep::From(EndingVisibleSelection().AsSelection());
    }
  }

  if (!selection_to_delete_.IsValidFor(GetDocument()) ||
      !selection_to_delete_.IsRange() ||
      !IsEditablePosition(selection_to_delete_.Anchor())) {
    // editing/execCommand/delete-non-editable-range-crash.html reaches here.
    return;
  }

  RelocatablePosition* relocatable_reference_position =
      MakeGarbageCollected<RelocatablePosition>(reference_move_position_);

  // save this to later make the selection with
  TextAffinity affinity = selection_to_delete_.Affinity();

  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kEditing);

  Position downstream_end =
      MostForwardCaretPosition(selection_to_delete_.End());
  const Node* downstream_container_node = downstream_end.ComputeContainerNode();
  const Element* downstream_container_root_element =
      RootEditableElement(*downstream_container_node);
  bool root_will_stay_open_without_placeholder =
      downstream_container_node == downstream_container_root_element;

  // Check to determine if the root will stay open without a placeholder.
  // This is done by checking if the downstream end is within a root editable
  // element that has an inline layout object, or if the downstream end's
  // container node is within a shadow host that is a text control.
  if (RuntimeEnabledFeatures::
          RootElementWithPlaceHolderAfterDeletingSelectionEnabled()) {
    root_will_stay_open_without_placeholder |=
        (downstream_container_root_element &&
         downstream_container_root_element->GetLayoutObject() &&
         downstream_container_root_element->GetLayoutObject()->IsInline()) ||
        (downstream_container_node->OwnerShadowHost() &&
         downstream_container_node->OwnerShadowHost()->IsTextControl());
  } else {
    root_will_stay_open_without_placeholder |=
        (downstream_container_node->IsTextNode() &&
         downstream_container_node->parentNode() ==
             downstream_container_root_element);
  }
  VisiblePosition visible_start = CreateVisiblePosition(
      selection_to_delete_.Start(),
      selection_to_delete_.IsRange() ? TextAffinity::kDownstream : affinity);
  VisiblePosition visible_end = CreateVisiblePosition(
      selection_to_delete_.End(),
      selection_to_delete_.IsRange() ? TextAffinity::kUpstream : affinity);

  bool line_break_at_end_of_selection_to_delete =
      LineBreakExistsAtVisiblePosition(visible_end);

  need_placeholder_ =
      !root_will_stay_open_without_placeholder &&
      IsStartOfParagraph(visible_start, kCanCrossEditingBoundary) &&
      IsEndOfParagraph(visible_end, kCanCrossEditingBoundary) &&
      !line_break_at_end_of_selection_to_delete;
  if (need_placeholder_) {
    // Don't need a placeholder when deleting a selection that starts just
    // before a table and ends inside it (we do need placeholders to hold
    // open empty cells, but that's handled elsewhere).
    if (Element* table = TableElementJustAfter(visible_start)) {
      if (selection_to_delete_.End().AnchorNode()->IsDescendantOf(table))
        need_placeholder_ = false;
    }
  }

  // set up our state
  InitializePositionData(editing_state);
  if (editing_state->IsAborted())
    return;

  bool line_break_before_start = LineBreakExistsAtVisiblePosition(
      PreviousPositionOf(CreateVisiblePosition(upstream_start_)));

  // Delete any text that may hinder our ability to fixup whitespace after the
  // delete
  DeleteInsignificantTextDownstream(trailing_whitespace_);

  SaveTypingStyleState();

  // deleting just a BR is handled specially, at least because we do not
  // want to replace it with a placeholder BR!
  bool br_result = HandleSpecialCaseBRDelete(editing_state);
  if (editing_state->IsAborted())
    return;
  if (br_result) {
    CalculateTypingStyleAfterDelete();
    GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kEditing);
    SelectionInDOMTree::Builder builder;
    builder.SetAffinity(affinity);
    if (ending_position_.IsNotNull())
      builder.Collapse(ending_position_);
    const VisibleSelection& visible_selection =
        CreateVisibleSelection(builder.Build());
    SetEndingSelection(
        SelectionForUndoStep::From(visible_selection.AsSelection()));
    ClearTransientState();
    RebalanceWhitespace();
    return;
  }

  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kEditing);

  HandleGeneralDelete(editing_state);
  if (editing_state->IsAborted())
    return;

  FixupWhitespace(leading_whitespace_);
  FixupWhitespace(trailing_whitespace_);

  MergeParagraphs(editing_state);
  if (editing_state->IsAborted())
    return;

  RemovePreviouslySelectedEmptyTableRows(editing_state);
  if (editing_state->IsAborted())
    return;

  if (!need_placeholder_ && root_will_stay_open_without_placeholder) {
    GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kEditing);
    VisiblePosition visual_ending = CreateVisiblePosition(ending_position_);
    bool has_placeholder =
        LineBreakExistsAtVisiblePosition(visual_ending) &&
        NextPositionOf(visual_ending, kCannotCrossEditingBoundary).IsNull();
    need_placeholder_ = has_placeholder && line_break_before_start &&
                        !line_break_at_end_of_selection_to_delete;
  }

  auto* placeholder = need_placeholder_
                          ? MakeGarbageCollected<HTMLBRElement>(GetDocument())
                          : nullptr;

  if (placeholder) {
    if (options_.IsSanitizeMarkup()) {
      RemoveRedundantBlocks(editing_state);
      if (editing_state->IsAborted())
        return;
    }
    // HandleGeneralDelete cause DOM mutation events so |ending_position_|
    // can be out of document.
    if (ending_position_.IsValidFor(GetDocument())) {
      InsertNodeAt(placeholder, ending_position_, editing_state);
      if (editing_state->IsAborted())
        return;
    }
  }

  RebalanceWhitespaceAt(ending_position_);

  CalculateTypingStyleAfterDelete();

  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kEditing);

  SelectionInDOMTree::Builder builder;
  builder.SetAffinity(affinity);
  if (ending_position_.IsNotNull())
    builder.Collapse(ending_position_);
  const VisibleSelection& visible_selection =
      CreateVisibleSelection(builder.Build());
  SetEndingSelection(
      SelectionForUndoStep::From(visible_selection.AsSelection()));

  if (relocatable_reference_position->GetPosition().IsNull()) {
    ClearTransientState();
    return;
  }

  // This deletion command is part of a move operation, we need to cleanup after
  // deletion.
  reference_move_position_ = relocatable_reference_position->GetPosition();
  // If the node for the destination has been removed as a result of the
  // deletion, set the destination to the ending point after the deletion.
  // Fixes: <rdar://problem/3910425> REGRESSION (Mail): Crash in
  // ReplaceSelectionCommand; selection is empty, leading to null deref
  if (!reference_move_position_.IsConnected())
    reference_move_position_ = EndingVisibleSelection().Start();

  // Move selection shouldn't left empty <li> block.
  CleanupAfterDeletion(editing_state,
                       CreateVisiblePosition(reference_move_position_));
  if (editing_state->IsAborted())
    return;

  ClearTransientState();
}

InputEvent::InputType DeleteSelectionCommand::GetInputType() const {
  // |DeleteSelectionCommand| could be used with Cut, Menu Bar deletion and
  // |TypingCommand|.
  // 1. Cut and Menu Bar deletion should rely on correct |input_type_|.
  // 2. |TypingCommand| will supply the |GetInputType()|, so |input_type_| could
  // default to |InputType::kNone|.
  return input_type_;
}

// Normally deletion doesn't preserve the typing style that was present before
// it.  For example, type a character, Bold, then delete the character and start
// typing.  The Bold typing style shouldn't stick around.  Deletion should
// preserve a typing style that *it* sets, however.
bool DeleteSelectionCommand::PreservesTypingStyle() const {
  return typing_style_ != nullptr;
}

void DeleteSelectionCommand::Trace(Visitor* visitor) const {
  visitor->Trace(selection_to_delete_);
  visitor->Trace(upstream_start_);
  visitor->Trace(downstream_start_);
  visitor->Trace(upstream_end_);
  visitor->Trace(downstream_end_);
  visitor->Trace(ending_position_);
  visitor->Trace(leading_whitespace_);
  visitor->Trace(trailing_whitespace_);
  visitor->Trace(reference_move_position_);
  visitor->Trace(start_block_);
  visitor->Trace(end_block_);
  visitor->Trace(typing_style_);
  visitor->Trace(delete_into_blockquote_style_);
  visitor->Trace(start_root_);
  visitor->Trace(end_root_);
  visitor->Trace(start_table_row_);
  visitor->Trace(end_table_row_);
  visitor->Trace(temporary_placeholder_);
  CompositeEditCommand::Trace(visitor);
}

}  // namespace blink
