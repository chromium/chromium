/*
 * Copyright (C) 2006, 2008 Apple Inc. All rights reserved.
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

#include "third_party/blink/renderer/core/editing/commands/indent_outdent_command.h"

#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element_traversal.h"
#include "third_party/blink/renderer/core/editing/commands/editing_commands_utilities.h"
#include "third_party/blink/renderer/core/editing/commands/insert_list_command.h"
#include "third_party/blink/renderer/core/editing/editing_utilities.h"
#include "third_party/blink/renderer/core/editing/relocatable_position.h"
#include "third_party/blink/renderer/core/editing/selection_template.h"
#include "third_party/blink/renderer/core/editing/visible_position.h"
#include "third_party/blink/renderer/core/editing/visible_selection.h"
#include "third_party/blink/renderer/core/editing/visible_units.h"
#include "third_party/blink/renderer/core/html/html_br_element.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/html/html_li_element.h"
#include "third_party/blink/renderer/core/html/html_olist_element.h"
#include "third_party/blink/renderer/core/html/html_ulist_element.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

namespace blink {

// Returns true if |node| is UL, OL, or BLOCKQUOTE with "display:block".
// "Outdent" command considers <BLOCKQUOTE style="display:inline"> makes
// indentation.
static bool IsHTMLListOrBlockquoteElement(const Node* node) {
  const auto* element = DynamicTo<HTMLElement>(node);
  if (!element)
    return false;
  if (!node->GetLayoutObject() || !node->GetLayoutObject()->IsLayoutBlock())
    return false;
  // TODO(yosin): We should check OL/UL element has "list-style-type" CSS
  // property to make sure they layout contents as list.
  return IsA<HTMLUListElement>(*element) || IsA<HTMLOListElement>(*element) ||
         element->HasTagName(html_names::kBlockquoteTag);
}

IndentOutdentCommand::IndentOutdentCommand(Document& document,
                                           IndentType type_of_action)
    : ApplyBlockElementCommand(
          document,
          html_names::kBlockquoteTag,
          AtomicString("margin: 0 0 0 40px; border: none; padding: 0px;")),
      type_of_action_(type_of_action) {}

bool IndentOutdentCommand::TryIndentingAsListItem(
    const Position& start,
    const Position& end,
    VisiblePosition& out_end_of_next_of_paragraph_to_move,
    EditingState* editing_state) {
  // If our selection is not inside a list, bail out.
  Node* last_node_in_selected_paragraph = start.AnchorNode();
  HTMLElement* list_element = EnclosingList(last_node_in_selected_paragraph);
  if (!list_element)
    return false;

  // Find the block that we want to indent.  If it's not a list item (e.g., a
  // div inside a list item), we bail out.
  Element* selected_list_item = EnclosingBlock(last_node_in_selected_paragraph);

  // FIXME: we need to deal with the case where there is no li (malformed HTML)
  if (!IsA<HTMLLIElement>(selected_list_item))
    return false;

  // FIXME: previousElementSibling does not ignore non-rendered content like
  // <span></span>.  Should we?
  Element* previous_list =
      ElementTraversal::PreviousSibling(*selected_list_item);
  Element* next_list = ElementTraversal::NextSibling(*selected_list_item);

  // We should calculate visible range in list item because inserting new
  // list element will change visibility of list item, e.g. :first-child
  // CSS selector.
  auto* new_list = To<HTMLElement>(GetDocument().CreateElement(
      list_element->TagQName(), CreateElementFlags::ByCloneNode(),
      g_null_atom));
  InsertNodeBefore(new_list, selected_list_item, editing_state);
  if (editing_state->IsAborted())
    return false;

  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kEditing);

  // We should clone all the children of the list item for indenting purposes.
  // However, in case the current selection does not encompass all its children,
  // we need to explicitally handle the same. The original list item too would
  // require proper deletion in that case.
  const bool should_keep_selected_list =
      end.AnchorNode() == selected_list_item ||
      end.AnchorNode()->IsDescendantOf(selected_list_item->lastChild());

  const VisiblePosition& start_of_paragraph_to_move =
      CreateVisiblePosition(start);
  const VisiblePosition& end_of_paragraph_to_move =
      should_keep_selected_list
          ? CreateVisiblePosition(end)
          : VisiblePosition::AfterNode(*selected_list_item->lastChild());

  // The insertion of |newList| may change the computed style of other
  // elements, resulting in failure in visible canonicalization.
  if (start_of_paragraph_to_move.IsNull() ||
      end_of_paragraph_to_move.IsNull()) {
    editing_state->Abort();
    return false;
  }

  if (RuntimeEnabledFeatures::
          AdjustEndOfNextParagraphIfMovedParagraphIsUpdatedEnabled()) {
    // If `end_of_paragraph_to_move` is adjusted above since
    // `should_keep_selected_list` is false, before move the paragraphs below,
    // update the end of the next of the paragraph to move.
    if (!should_keep_selected_list) {
      out_end_of_next_of_paragraph_to_move =
          EndOfParagraph(NextPositionOf(end_of_paragraph_to_move));
    }
  }

  MoveParagraphWithClones(start_of_paragraph_to_move, end_of_paragraph_to_move,
                          new_list, selected_list_item, editing_state);
  if (editing_state->IsAborted())
    return false;

  if (!should_keep_selected_list) {
    RemoveNode(selected_list_item, editing_state);
    if (editing_state->IsAborted())
      return false;
  }

  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kEditing);
  DCHECK(new_list);
  if (previous_list && CanMergeLists(*previous_list, *new_list)) {
    MergeIdenticalElements(previous_list, new_list, editing_state);
    if (editing_state->IsAborted())
      return false;
  }

  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kEditing);
  if (next_list && CanMergeLists(*new_list, *next_list)) {
    MergeIdenticalElements(new_list, next_list, editing_state);
    if (editing_state->IsAborted())
      return false;
  }

  return true;
}

void IndentOutdentCommand::IndentIntoBlockquote(const Position& start,
                                                const Position& end,
                                                HTMLElement*& target_blockquote,
                                                EditingState* editing_state) {
  auto* enclosing_cell = To<Element>(EnclosingNodeOfType(start, &IsTableCell));
  Element* element_to_split_to;
  if (enclosing_cell)
    element_to_split_to = enclosing_cell;
  else if (EnclosingList(start.ComputeContainerNode()))
    element_to_split_to = EnclosingBlock(start.ComputeContainerNode());
  else
    element_to_split_to = RootEditableElementOf(start);

  if (!element_to_split_to)
    return;

  Node* outer_block =
      (start.ComputeContainerNode() == element_to_split_to)
          ? start.ComputeContainerNode()
          : SplitTreeToNode(start.ComputeContainerNode(), element_to_split_to);

  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kEditing);
  // Before moving the paragraph under the new blockquote, make sure that there
  // aren't any nested paragraphs or line breaks under the outer_block. If there
  // are then split it into its own block so it doesn't copy multiple
  // paragraphs.
  Node* highest_inline_node = HighestEnclosingNodeOfType(
      end, IsInlineElement, kCannotCrossEditingBoundary, outer_block);
  if (highest_inline_node) {
    Position next_position = MostForwardCaretPosition(
        NextPositionOf(CreateVisiblePosition(end)).DeepEquivalent());
    if (IsStartOfParagraph(CreateVisiblePosition(next_position)) &&
        next_position.AnchorNode()->IsDescendantOf(highest_inline_node)) {
      // <div>Line                                 <blockquote>
      //                                             <div>
      //   <span> 1<div>Line 2</div></span>    ->      Line<span> 1</span>
      //                                             </div>
      // </div>                                    </blockquote>
      //                                           <div><span><div>Line
      //                                           2</div></span></div>
      //
      // <div>Line                                 <blockquote>
      //   <span> 1<br>Line 2</span>    ->           Line<span> 1</span>
      // </div>                                    </blockquote>
      //                                           <div><span>Line
      //                                           2</span></div>
      // The below steps are essentially trying to figure out where the split
      // needs to happen:
      // 1. If the next paragraph is enclosed with nested block level elements.
      // 2. If the next paragraph is enclosed with nested inline elements.
      // 3. If the next paragraph doesn't have any inline or block level
      // elements, but has elements like textarea/input/img etc.
      Node* split_point = HighestEnclosingNodeOfType(
          next_position, IsEnclosingBlock, kCannotCrossEditingBoundary,
          highest_inline_node);
      split_point = split_point
                        ? split_point
                        : HighestEnclosingNodeOfType(
                              next_position, IsInlineElement,
                              kCannotCrossEditingBoundary, highest_inline_node);
      split_point = split_point ? split_point : next_position.AnchorNode();
      // Split the element to separate the paragraphs.
      SplitElement(DynamicTo<Element>(highest_inline_node), split_point);
    }
  }
  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kEditing);
  VisiblePosition start_of_contents = CreateVisiblePosition(start);
  if (!target_blockquote) {
    // Create a new blockquote and insert it as a child of the root editable
    // element. We accomplish this by splitting all parents of the current
    // paragraph up to that point.
    target_blockquote = CreateBlockElement();
    if (outer_block == start.ComputeContainerNode()) {
      if (outer_block->HasTagName(html_names::kBlockquoteTag)) {
        if (RuntimeEnabledFeatures::InsertBlockquoteBeforeOuterBlockEnabled()) {
          // Insert `target_blockquote` before `outer_block` so that
          // `start_of_contents` includes the start of deletion. See
          // https://crbug.com/327665597 for more details.
          InsertNodeBefore(target_blockquote, outer_block, editing_state);
        } else {
          // When we apply indent to an empty <blockquote>, we should call
          // InsertNodeAfter(). See http://crbug.com/625802 for more details.
          InsertNodeAfter(target_blockquote, outer_block, editing_state);
        }
      } else {
        InsertNodeAt(target_blockquote, start, editing_state);
      }
    } else
      InsertNodeBefore(target_blockquote, outer_block, editing_state);
    if (editing_state->IsAborted())
      return;
    GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kEditing);
    start_of_contents = VisiblePosition::InParentAfterNode(*target_blockquote);
  }

  VisiblePosition end_of_contents = CreateVisiblePosition(end);
  if (start_of_contents.IsNull() || end_of_contents.IsNull())
    return;
  MoveParagraphWithClones(start_of_contents, end_of_contents, target_blockquote,
                          outer_block, editing_state);
}

void IndentOutdentCommand::OutdentParagraph(EditingState* editing_state) {
  VisiblePosition visible_start_of_paragraph =
      StartOfParagraph(EndingVisibleSelection().VisibleStart());
  VisiblePosition visible_end_of_paragraph =
      EndOfParagraph(visible_start_of_paragraph);

  auto* enclosing_element = To<HTMLElement>(
      EnclosingNodeOfType(visible_start_of_paragraph.DeepEquivalent(),
                          &IsHTMLListOrBlockquoteElement));
  // We can't outdent if there is no place to go!
  if (!enclosing_element || !IsEditable(*enclosing_element->parentNode()))
    return;

  // Use InsertListCommand to remove the selection from the list
  if (IsA<HTMLOListElement>(*enclosing_element)) {
    ApplyCommandToComposite(MakeGarbageCollected<InsertListCommand>(
                                GetDocument(), InsertListCommand::kOrderedList),
                            editing_state);
    return;
  }
  if (IsA<HTMLUListElement>(*enclosing_element)) {
    ApplyCommandToComposite(
        MakeGarbageCollected<InsertListCommand>(
            GetDocument(), InsertListCommand::kUnorderedList),
        editing_state);
    return;
  }

  // The selection is inside a blockquote i.e. enclosingNode is a blockquote
  VisiblePosition position_in_enclosing_block =
      VisiblePosition::FirstPositionInNode(*enclosing_element);
  // If the blockquote is inline, the start of the enclosing block coincides
  // with positionInEnclosingBlock.
  VisiblePosition start_of_enclosing_block =
      (enclosing_element->GetLayoutObject() &&
       enclosing_element->GetLayoutObject()->IsInline())
          ? position_in_enclosing_block
          : StartOfBlock(position_in_enclosing_block);
  VisiblePosition last_position_in_enclosing_block =
      VisiblePosition::LastPositionInNode(*enclosing_element);
  VisiblePosition end_of_enclosing_block =
      EndOfBlock(last_position_in_enclosing_block);
  RelocatablePosition* start_of_paragraph =
      MakeGarbageCollected<RelocatablePosition>(
          visible_start_of_paragraph.DeepEquivalent());
  RelocatablePosition* end_of_paragraph =
      MakeGarbageCollected<RelocatablePosition>(
          visible_end_of_paragraph.DeepEquivalent());
  if (visible_start_of_paragraph.DeepEquivalent() ==
          start_of_enclosing_block.DeepEquivalent() &&
      visible_end_of_paragraph.DeepEquivalent() ==
          end_of_enclosing_block.DeepEquivalent()) {
    // The blockquote doesn't contain anything outside the paragraph, so it can
    // be totally removed.
    // This procedure will make {start,end}_of_paragraph out of sync if the
    // blockquote has children, so store the first and last children.
    Node* first_child = enclosing_element->firstChild();
    Node* last_child = enclosing_element->lastChild();
    Node* split_point = enclosing_element->nextSibling();
    RemoveNodePreservingChildren(enclosing_element, editing_state);
    if (editing_state->IsAborted())
      return;
    // outdentRegion() assumes it is operating on the first paragraph of an
    // enclosing blockquote, but if there are multiply nested blockquotes and
    // we've just removed one, then this assumption isn't true. By splitting the
    // next containing blockquote after this node, we keep this assumption true
    if (split_point) {
      if (Element* split_point_parent = split_point->parentElement()) {
        // We can't outdent if there is no place to go!
        if (split_point_parent->HasTagName(html_names::kBlockquoteTag) &&
            !split_point->HasTagName(html_names::kBlockquoteTag) &&
            IsEditable(*split_point_parent->parentNode()))
          SplitElement(split_point_parent, split_point);
      }
    }

    // Re-canonicalize visible_start_of_paragraph, make it valid again after DOM
    // change. If enclosing_element had children, start_of_paragraph will be out
    // of sync, so use first_child instead.
    GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kEditing);
    DCHECK(!first_child || first_child->isConnected());
    visible_start_of_paragraph =
        CreateVisiblePosition(first_child ? Position::BeforeNode(*first_child)
                                          : start_of_paragraph->GetPosition());
    if (visible_start_of_paragraph.IsNotNull() &&
        !IsStartOfParagraph(visible_start_of_paragraph)) {
      InsertNodeAt(MakeGarbageCollected<HTMLBRElement>(GetDocument()),
                   visible_start_of_paragraph.DeepEquivalent(), editing_state);
      if (editing_state->IsAborted())
        return;
    }

    // Re-canonicalize visible_end_of_paragraph, make it valid again after DOM
    // change. If enclosing_element had children, end_of_paragraph will be out
    // of sync, so use last_child instead.
    GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kEditing);
    DCHECK(!last_child || last_child->isConnected());
    visible_end_of_paragraph =
        CreateVisiblePosition(last_child ? Position::AfterNode(*last_child)
                                         : end_of_paragraph->GetPosition());
    // Insert BR after the old paragraph end if it got merged into the next
    // paragraph. This happens if the original paragraph end is no longer a
    // paragraph end, or if it is followed by a BR.
    // TODO(editing-dev): This doesn't work if there is other unrendered nodes
    // (e.g., comments) between the old paragraph end and the BR.
    const bool should_insert_br =
        (visible_end_of_paragraph.IsNotNull() &&
         !IsEndOfParagraph(visible_end_of_paragraph)) ||
        IsA<HTMLBRElement>(split_point);
    if (should_insert_br) {
      InsertNodeAt(MakeGarbageCollected<HTMLBRElement>(GetDocument()),
                   visible_end_of_paragraph.DeepEquivalent(), editing_state);
    }
    return;
  }

  Node* split_blockquote_node = enclosing_element;
  if (Element* enclosing_block_flow = EnclosingBlock(
          visible_start_of_paragraph.DeepEquivalent().AnchorNode())) {
    if (enclosing_block_flow != enclosing_element) {
      // We should check if the blockquotes are nested, as nested blockquotes
      // may be at different indentations.
      const Position& previous_element =
          PreviousCandidate(visible_start_of_paragraph.DeepEquivalent());
      auto* const previous_element_is_blockquote =
          To<HTMLElement>(EnclosingNodeOfType(previous_element,
                                              &IsHTMLListOrBlockquoteElement));
      const bool is_previous_blockquote_same =
          !previous_element_is_blockquote ||
          (enclosing_element == previous_element_is_blockquote);
      const bool split_ancestor = true;
      if (is_previous_blockquote_same) {
        split_blockquote_node = SplitTreeToNode(
            enclosing_block_flow, enclosing_element, split_ancestor);
      } else {
        SplitTreeToNode(
            visible_start_of_paragraph.DeepEquivalent().AnchorNode(),
            enclosing_element, split_ancestor);
      }
    } else {
      if (RuntimeEnabledFeatures::NonEmptyBlockquotesOnOutdentingEnabled()) {
        // Insert BR after the previous sibling of `enclosing_element` if the
        // LayoutObject of sibling is 'inline-level' and it gets merged into the
        // splitted element below.
        if (enclosing_element->HasPreviousSibling()) {
          Node* previous_sibling = enclosing_element->previousSibling();
          if (IsInlineNode(previous_sibling) &&
              !IsA<HTMLBRElement>(previous_sibling)) {
            InsertNodeAt(MakeGarbageCollected<HTMLBRElement>(GetDocument()),
                         Position::AfterNode(*previous_sibling), editing_state);
          }
        }
      }
      // We split the blockquote at where we start outdenting.
      Node* highest_inline_node = HighestEnclosingNodeOfType(
          visible_start_of_paragraph.DeepEquivalent(), IsInlineElement,
          kCannotCrossEditingBoundary, enclosing_block_flow);
      SplitElement(
          enclosing_element,
          highest_inline_node
              ? highest_inline_node
              : visible_start_of_paragraph.DeepEquivalent().AnchorNode());
    }

    GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kEditing);

    // Re-canonicalize visible_{start,end}_of_paragraph, make them valid again
    // after DOM change.
    visible_start_of_paragraph =
        CreateVisiblePosition(start_of_paragraph->GetPosition());
    visible_end_of_paragraph =
        CreateVisiblePosition(end_of_paragraph->GetPosition());
  }

  VisiblePosition visible_start_of_paragraph_to_move =
      StartOfParagraph(visible_start_of_paragraph);
  VisiblePosition visible_end_of_paragraph_to_move =
      EndOfParagraph(visible_end_of_paragraph);
  if (visible_start_of_paragraph_to_move.IsNull() ||
      visible_end_of_paragraph_to_move.IsNull())
    return;
  RelocatablePosition* start_of_paragraph_to_move =
      MakeGarbageCollected<RelocatablePosition>(
          visible_start_of_paragraph_to_move.DeepEquivalent());
  RelocatablePosition* end_of_paragraph_to_move =
      MakeGarbageCollected<RelocatablePosition>(
          visible_end_of_paragraph_to_move.DeepEquivalent());
  auto* placeholder = MakeGarbageCollected<HTMLBRElement>(GetDocument());
  InsertNodeBefore(placeholder, split_blockquote_node, editing_state);
  if (editing_state->IsAborted())
    return;

  // Re-canonicalize visible_{start,end}_of_paragraph_to_move, make them valid
  // again after DOM change.
  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kEditing);
  visible_start_of_paragraph_to_move =
      CreateVisiblePosition(start_of_paragraph_to_move->GetPosition());
  visible_end_of_paragraph_to_move =
      CreateVisiblePosition(end_of_paragraph_to_move->GetPosition());
  MoveParagraph(visible_start_of_paragraph_to_move,
                visible_end_of_paragraph_to_move,
                VisiblePosition::BeforeNode(*placeholder), editing_state,
                kPreserveSelection);
}

// FIXME: We should merge this function with
// ApplyBlockElementCommand::formatSelection
void IndentOutdentCommand::OutdentRegion(
    const VisiblePosition& start_of_selection,
    const VisiblePosition& end_of_selection,
    EditingState* editing_state) {
  VisiblePosition end_of_current_paragraph = EndOfParagraph(start_of_selection);
  VisiblePosition end_of_last_paragraph = EndOfParagraph(end_of_selection);

  if (end_of_current_paragraph.DeepEquivalent() ==
      end_of_last_paragraph.DeepEquivalent()) {
    OutdentParagraph(editing_state);
    return;
  }

  Position original_selection_end = EndingVisibleSelection().End();
  Position end_after_selection =
      EndOfParagraph(NextPositionOf(end_of_last_paragraph)).DeepEquivalent();

  while (!end_of_current_paragraph.IsNull() &&
         end_of_current_paragraph.DeepEquivalent() != end_after_selection) {
    PositionWithAffinity end_of_next_paragraph =
        EndOfParagraph(NextPositionOf(end_of_current_paragraph))
            .ToPositionWithAffinity();
    if (end_of_current_paragraph.DeepEquivalent() ==
        end_of_last_paragraph.DeepEquivalent()) {
      SelectionInDOMTree::Builder builder;
      if (original_selection_end.IsNotNull())
        builder.Collapse(original_selection_end);
      SetEndingSelection(SelectionForUndoStep::From(builder.Build()));
    } else {
      SetEndingSelection(SelectionForUndoStep::From(
          SelectionInDOMTree::Builder()
              .Collapse(end_of_current_paragraph.DeepEquivalent())
              .Build()));
    }

    OutdentParagraph(editing_state);
    if (editing_state->IsAborted())
      return;

    // outdentParagraph could move more than one paragraph if the paragraph
    // is in a list item. As a result, endAfterSelection and endOfNextParagraph
    // could refer to positions no longer in the document.
    if (end_after_selection.IsNotNull() && !end_after_selection.IsConnected())
      break;

    GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kEditing);
    if (end_of_next_paragraph.IsNotNull() &&
        !end_of_next_paragraph.IsConnected()) {
      if (RuntimeEnabledFeatures::MoveEndingSelectionToListChildEnabled()) {
        // If the end of the current selection is in a list item, set the
        // selection to the last position in the list item since
        // OutdentParagraph() moves all children in a list item at once using
        // InsertListCommand.
        SetEndingSelectionToListChildIfListItem();
      }
      end_of_current_paragraph =
          CreateVisiblePosition(EndingVisibleSelection().End());
      end_of_next_paragraph =
          EndOfParagraph(NextPositionOf(end_of_current_paragraph))
              .ToPositionWithAffinity();
    }
    end_of_current_paragraph = CreateVisiblePosition(end_of_next_paragraph);
  }
}

void IndentOutdentCommand::SetEndingSelectionToListChildIfListItem() {
  Node* selection_node = EndingVisibleSelection().Start().AnchorNode();
  Node* list_child_node = EnclosingListChild(selection_node);
  if (list_child_node && IsA<HTMLLIElement>(*list_child_node)) {
    SetEndingSelection(SelectionForUndoStep::From(
        SelectionInDOMTree::Builder()
            .Collapse(Position::LastPositionInNode(*list_child_node))
            .Build()));
  }
}

void IndentOutdentCommand::FormatSelection(
    const VisiblePosition& start_of_selection,
    const VisiblePosition& end_of_selection,
    EditingState* editing_state) {
  if (type_of_action_ == kIndent)
    ApplyBlockElementCommand::FormatSelection(start_of_selection,
                                              end_of_selection, editing_state);
  else
    OutdentRegion(start_of_selection, end_of_selection, editing_state);
}

void IndentOutdentCommand::FormatRange(
    const Position& start,
    const Position& end,
    const Position&,
    HTMLElement*& blockquote_for_next_indent,
    VisiblePosition& out_end_of_next_of_paragraph_to_move,
    EditingState* editing_state) {
  bool indenting_as_list_item_result = TryIndentingAsListItem(
      start, end, out_end_of_next_of_paragraph_to_move, editing_state);
  if (editing_state->IsAborted())
    return;
  if (indenting_as_list_item_result)
    blockquote_for_next_indent = nullptr;
  else
    IndentIntoBlockquote(start, end, blockquote_for_next_indent, editing_state);
}

InputEvent::InputType IndentOutdentCommand::GetInputType() const {
  return type_of_action_ == kIndent ? InputEvent::InputType::kFormatIndent
                                    : InputEvent::InputType::kFormatOutdent;
}

}  // namespace blink
