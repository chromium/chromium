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

#include "third_party/blink/renderer/core/editing/commands/insert_text_command.h"

#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/text.h"
#include "third_party/blink/renderer/core/editing/commands/delete_selection_options.h"
#include "third_party/blink/renderer/core/editing/commands/editing_commands_utilities.h"
#include "third_party/blink/renderer/core/editing/editing_utilities.h"
#include "third_party/blink/renderer/core/editing/editor.h"
#include "third_party/blink/renderer/core/editing/relocatable_position.h"
#include "third_party/blink/renderer/core/editing/selection_template.h"
#include "third_party/blink/renderer/core/editing/visible_position.h"
#include "third_party/blink/renderer/core/editing/visible_units.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/html/html_span_element.h"

namespace blink {

InsertTextCommand::InsertTextCommand(Document& document,
                                     const String& text,
                                     RebalanceType rebalance_type)
    : CompositeEditCommand(document),
      text_(text),
      rebalance_type_(rebalance_type) {}

String InsertTextCommand::TextDataForInputEvent() const {
  return text_;
}

Position InsertTextCommand::PositionInsideTextNode(
    const Position& p,
    EditingState* editing_state) {
  Position pos = p;
  if (IsTabHTMLSpanElementTextNode(pos.AnchorNode())) {
    Text* text_node = GetDocument().CreateEditingTextNode("");
    InsertNodeAtTabSpanPosition(text_node, pos, editing_state);
    if (editing_state->IsAborted())
      return Position();
    return Position::FirstPositionInNode(*text_node);
  }

  // Prepare for text input by looking at the specified position.
  // It may be necessary to insert a text node to receive characters.
  if (!pos.ComputeContainerNode()->IsTextNode()) {
    Text* text_node = GetDocument().CreateEditingTextNode("");
    InsertNodeAt(text_node, pos, editing_state);
    if (editing_state->IsAborted())
      return Position();
    return Position::FirstPositionInNode(*text_node);
  }

  return pos;
}

void InsertTextCommand::SetEndingSelectionWithoutValidation(
    const Position& start_position,
    const Position& end_position) {
  // We could have inserted a part of composed character sequence,
  // so we are basically treating ending selection as a range to avoid
  // validation. <http://bugs.webkit.org/show_bug.cgi?id=15781>
  SetEndingSelection(SelectionForUndoStep::From(
      SelectionInDOMTree::Builder()
          .Collapse(start_position)
          .Extend(end_position)
          .Build()));
}

// This avoids the expense of a full fledged delete operation, and avoids a
// layout that typically results from text removal.
bool InsertTextCommand::PerformTrivialReplace(const String& text) {
  // We may need to manipulate neighboring whitespace if we're deleting text.
  // This case is tested in
  // InsertTextCommandTest_InsertEmptyTextAfterWhitespaceThatNeedsFixup.
  if (text.empty())
    return false;

  if (!EndingSelection().IsRange())
    return false;

  if (text.Contains('\t') || text.Contains(' ') || text.Contains('\n'))
    return false;

  // Also if the text is surrounded by a hyperlink and all the contents of the
  // link are selected, then we shouldn't be retaining the link with just one
  // character because the user wouldn't be able to edit the link if it has only
  // one character.
  Position start = EndingVisibleSelection().Start();
  Element* enclosing_anchor = EnclosingAnchorElement(start);
  if (enclosing_anchor && text.length() <= 1) {
    VisiblePosition first_in_anchor =
        VisiblePosition::FirstPositionInNode(*enclosing_anchor);
    VisiblePosition last_in_anchor =
        VisiblePosition::LastPositionInNode(*enclosing_anchor);
    Position end = EndingVisibleSelection().End();
    if (first_in_anchor.DeepEquivalent() == start &&
        last_in_anchor.DeepEquivalent() == end)
      return false;
  }

  RelocatablePosition* relocatable_start =
      MakeGarbageCollected<RelocatablePosition>(start);
  Position end_position = ReplaceSelectedTextInNode(text);
  if (end_position.IsNull())
    return false;

  SetEndingSelectionWithoutValidation(relocatable_start->GetPosition(),
                                      end_position);
  SetEndingSelection(SelectionForUndoStep::From(
      SelectionInDOMTree::Builder()
          .Collapse(EndingVisibleSelection().End())
          .Build()));
  return true;
}

void InsertTextCommand::DoApply(EditingState* editing_state) {
  DCHECK_EQ(text_.find('\n'), kNotFound);

  // TODO(editing-dev): We shouldn't construct an InsertTextCommand with none or
  // invalid selection.
  const VisibleSelection& visible_selection = EndingVisibleSelection();
  if (visible_selection.IsNone() ||
      !visible_selection.IsValidFor(GetDocument()))
    return;

  // Delete the current selection.
  // FIXME: This delete operation blows away the typing style.
  if (EndingSelection().IsRange()) {
    if (PerformTrivialReplace(text_))
      return;
    GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kEditing);
    bool end_of_selection_was_at_start_of_block =
        IsStartOfBlock(EndingVisibleSelection().VisibleEnd());
    if (!DeleteSelection(editing_state, DeleteSelectionOptions::Builder()
                                            .SetMergeBlocksAfterDelete(true)
                                            .Build()))
      return;
    // deleteSelection eventually makes a new endingSelection out of a Position.
    // If that Position doesn't have a layoutObject (e.g. it is on a <frameset>
    // in the DOM), the VisibleSelection cannot be canonicalized to anything
    // other than NoSelection. The rest of this function requires a real
    // endingSelection, so bail out.
    if (EndingSelection().IsNone())
      return;
    if (end_of_selection_was_at_start_of_block) {
      if (EditingStyle* typing_style =
              GetDocument().GetFrame()->GetEditor().TypingStyle()) {
        typing_style->RemoveBlockProperties(
            GetDocument().GetExecutionContext());
      }
    }
  }

  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kEditing);

  // Reached by InsertTextCommandTest.NoVisibleSelectionAfterDeletingSelection
  ABORT_EDITING_COMMAND_IF(EndingVisibleSelection().IsNone());

  Position start_position(EndingVisibleSelection().Start());

  Position placeholder = ComputePlaceholderToCollapseAt(start_position);

  // Insert the character at the leftmost candidate.
  start_position = MostBackwardCaretPosition(start_position);

  // It is possible for the node that contains startPosition to contain only
  // unrendered whitespace, and so deleteInsignificantText could remove it.
  // Save the position before the node in case that happens.
  DCHECK(start_position.ComputeContainerNode()) << start_position;
  Position position_before_start_node(
      Position::InParentBeforeNode(*start_position.ComputeContainerNode()));
  DeleteInsignificantText(start_position,
                          MostForwardCaretPosition(start_position));

  // TODO(editing-dev): Use of UpdateStyleAndLayout()
  // needs to be audited.  See http://crbug.com/590369 for more details.
  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kEditing);

  if (!start_position.IsConnected())
    start_position = position_before_start_node;
  if (!IsVisuallyEquivalentCandidate(start_position))
    start_position = MostForwardCaretPosition(start_position);

  start_position =
      PositionAvoidingSpecialElementBoundary(start_position, editing_state);
  if (editing_state->IsAborted())
    return;

  Position end_position;

  if (text_ == "\t" && IsRichlyEditablePosition(start_position)) {
    end_position = InsertTab(start_position, editing_state);
    if (editing_state->IsAborted())
      return;
    start_position =
        PreviousPositionOf(end_position, PositionMoveType::kGraphemeCluster);
    if (placeholder.IsNotNull())
      RemovePlaceholderAt(placeholder);
  } else {
    // Make sure the document is set up to receive text_
    start_position = PositionInsideTextNode(start_position, editing_state);
    if (editing_state->IsAborted())
      return;
    DCHECK(start_position.IsOffsetInAnchor()) << start_position;
    DCHECK(start_position.ComputeContainerNode()) << start_position;
    DCHECK(start_position.ComputeContainerNode()->IsTextNode())
        << start_position;
    if (placeholder.IsNotNull())
      RemovePlaceholderAt(placeholder);
    auto* text_node = To<Text>(start_position.ComputeContainerNode());
    const unsigned offset = start_position.OffsetInContainerNode();

    InsertTextIntoNode(text_node, offset, text_);
    end_position = Position(text_node, offset + text_.length());

    if (rebalance_type_ == kRebalanceLeadingAndTrailingWhitespaces) {
      // The insertion may require adjusting adjacent whitespace, if it is
      // present.
      RebalanceWhitespaceAt(end_position);
      // Rebalancing on both sides isn't necessary if we've inserted only
      // spaces.
      if (!text_.ContainsOnlyWhitespaceOrEmpty())
        RebalanceWhitespaceAt(start_position);
    } else {
      DCHECK_EQ(rebalance_type_, kRebalanceAllWhitespaces);
      if (CanRebalance(start_position) && CanRebalance(end_position))
        RebalanceWhitespaceOnTextSubstring(
            text_node, start_position.OffsetInContainerNode(),
            end_position.OffsetInContainerNode());
    }
  }

  SetEndingSelectionWithoutValidation(start_position, end_position);

  // Handle the case where there is a typing style.
  if (EditingStyle* typing_style =
          GetDocument().GetFrame()->GetEditor().TypingStyle()) {
    typing_style->PrepareToApplyAt(end_position,
                                   EditingStyle::kPreserveWritingDirection);
    if (!typing_style->IsEmpty() && !EndingSelection().IsNone()) {
      ApplyStyle(typing_style, editing_state);
      if (editing_state->IsAborted())
        return;
    }
  }

  SelectionInDOMTree::Builder builder;
  const VisibleSelection& selection = EndingVisibleSelection();
  builder.SetAffinity(selection.Affinity());
  if (selection.End().IsNotNull())
    builder.Collapse(selection.End());
  SetEndingSelection(SelectionForUndoStep::From(builder.Build()));
}

Position InsertTextCommand::InsertTab(const Position& pos,
                                      EditingState* editing_state) {
  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kEditing);

  Position insert_pos = CreateVisiblePosition(pos).DeepEquivalent();
  if (insert_pos.IsNull())
    return pos;

  Node* node = insert_pos.ComputeContainerNode();
  auto* text_node = DynamicTo<Text>(node);
  unsigned offset = text_node ? insert_pos.OffsetInContainerNode() : 0;

  // keep tabs coalesced in tab span
  if (IsTabHTMLSpanElementTextNode(node)) {
    InsertTextIntoNode(text_node, offset, "\t");
    return Position(text_node, offset + 1);
  }

  // create new tab span
  HTMLSpanElement* span_element = CreateTabSpanElement(GetDocument());

  // place it
  if (!text_node) {
    InsertNodeAt(span_element, insert_pos, editing_state);
  } else {
    if (offset >= text_node->length()) {
      InsertNodeAfter(span_element, text_node, editing_state);
    } else {
      // split node to make room for the span
      // NOTE: splitTextNode uses textNode for the
      // second node in the split, so we need to
      // insert the span before it.
      if (offset > 0)
        SplitTextNode(text_node, offset);
      InsertNodeBefore(span_element, text_node, editing_state);
    }
  }
  if (editing_state->IsAborted())
    return Position();

  // return the position following the new tab
  return Position::LastPositionInNode(*span_element);
}

}  // namespace blink
