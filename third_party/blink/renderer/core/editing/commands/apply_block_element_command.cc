/*
 * Copyright (C) 2006, 2008 Apple Inc. All rights reserved.
 * Copyright (C) 2010 Google Inc. All rights reserved.
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

#include "third_party/blink/renderer/core/editing/commands/apply_block_element_command.h"

#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/dom/text.h"
#include "third_party/blink/renderer/core/editing/commands/editing_commands_utilities.h"
#include "third_party/blink/renderer/core/editing/editing_utilities.h"
#include "third_party/blink/renderer/core/editing/selection_template.h"
#include "third_party/blink/renderer/core/editing/visible_position.h"
#include "third_party/blink/renderer/core/editing/visible_selection.h"
#include "third_party/blink/renderer/core/editing/visible_units.h"
#include "third_party/blink/renderer/core/html/html_br_element.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/heap/heap.h"

namespace blink {

ApplyBlockElementCommand::ApplyBlockElementCommand(
    Document& document,
    const QualifiedName& tag_name,
    const AtomicString& inline_style)
    : CompositeEditCommand(document),
      tag_name_(tag_name),
      inline_style_(inline_style) {}

ApplyBlockElementCommand::ApplyBlockElementCommand(
    Document& document,
    const QualifiedName& tag_name)
    : CompositeEditCommand(document), tag_name_(tag_name) {}

void ApplyBlockElementCommand::DoApply(EditingState* editing_state) {
  // ApplyBlockElementCommands are only created directly by editor commands'
  // execution, which updates layout before entering doApply().
  DCHECK(!GetDocument().NeedsLayoutTreeUpdate());

  if (!RootEditableElementOf(EndingSelection().Base()))
    return;

  VisiblePosition visible_end = EndingVisibleSelection().VisibleEnd();
  VisiblePosition visible_start = EndingVisibleSelection().VisibleStart();
  if (visible_start.IsNull() || visible_start.IsOrphan() ||
      visible_end.IsNull() || visible_end.IsOrphan())
    return;

  // When a selection ends at the start of a paragraph, we rarely paint
  // the selection gap before that paragraph, because there often is no gap.
  // In a case like this, it's not obvious to the user that the selection
  // ends "inside" that paragraph, so it would be confusing if Indent/Outdent
  // operated on that paragraph.
  // FIXME: We paint the gap before some paragraphs that are indented with left
  // margin/padding, but not others.  We should make the gap painting more
  // consistent and then use a left margin/padding rule here.
  if (visible_end.DeepEquivalent() != visible_start.DeepEquivalent() &&
      IsStartOfParagraph(visible_end)) {
    const Position& new_end =
        PreviousPositionOf(visible_end, kCannotCrossEditingBoundary)
            .DeepEquivalent();
    SelectionInDOMTree::Builder builder;
    builder.Collapse(visible_start.ToPositionWithAffinity());
    if (new_end.IsNotNull())
      builder.Extend(new_end);
    SetEndingSelection(SelectionForUndoStep::From(builder.Build()));
    ABORT_EDITING_COMMAND_IF(EndingVisibleSelection().VisibleStart().IsNull());
    ABORT_EDITING_COMMAND_IF(EndingVisibleSelection().VisibleEnd().IsNull());
  }

  VisibleSelection selection =
      SelectionForParagraphIteration(EndingVisibleSelection());
  VisiblePosition start_of_selection = selection.VisibleStart();
  ABORT_EDITING_COMMAND_IF(start_of_selection.IsNull());
  VisiblePosition end_of_selection = selection.VisibleEnd();
  ABORT_EDITING_COMMAND_IF(end_of_selection.IsNull());
  ContainerNode* start_scope = nullptr;
  int start_index = IndexForVisiblePosition(start_of_selection, start_scope);
  ContainerNode* end_scope = nullptr;
  int end_index = IndexForVisiblePosition(end_of_selection, end_scope);

  FormatSelection(start_of_selection, end_of_selection, editing_state);
  if (editing_state->IsAborted())
    return;

  GetDocument().UpdateStyleAndLayout();

  DCHECK_EQ(start_scope, end_scope);
  DCHECK_GE(start_index, 0);
  DCHECK_LE(start_index, end_index);
  if (start_scope == end_scope && start_index >= 0 &&
      start_index <= end_index) {
    VisiblePosition start(VisiblePositionForIndex(start_index, start_scope));
    VisiblePosition end(VisiblePositionForIndex(end_index, end_scope));
    if (start.IsNotNull() && end.IsNotNull()) {
      SetEndingSelection(SelectionForUndoStep::From(
          SelectionInDOMTree::Builder()
              .Collapse(start.ToPositionWithAffinity())
              .Extend(end.DeepEquivalent())
              .Build()));
    }
  }
}

static bool IsAtUnsplittableElement(const Position& pos) {
  Node* node = pos.AnchorNode();
  return node == RootEditableElementOf(pos) ||
         node == EnclosingNodeOfType(pos, &IsTableCell);
}

void ApplyBlockElementCommand::FormatSelection(
    const VisiblePosition& start_of_selection,
    const VisiblePosition& end_of_selection,
    EditingState* editing_state) {
  // Special case empty unsplittable elements because there's nothing to split
  // and there's nothing to move.
  const Position& caret_position =
      MostForwardCaretPosition(start_of_selection.DeepEquivalent());
  if (IsAtUnsplittableElement(caret_position)) {
    HTMLElement* blockquote = CreateBlockElement();
    InsertNodeAt(blockquote, caret_position, editing_state);
    if (editing_state->IsAborted())
      return;
    auto* placeholder = MakeGarbageCollected<HTMLBRElement>(GetDocument());
    AppendNode(placeholder, blockquote, editing_state);
    if (editing_state->IsAborted())
      return;
    SetEndingSelection(SelectionForUndoStep::From(
        SelectionInDOMTree::Builder()
            .Collapse(Position::BeforeNode(*placeholder))
            .Build()));
    return;
  }

  HTMLElement* blockquote_for_next_indent = nullptr;
  VisiblePosition end_of_current_paragraph = EndOfParagraph(start_of_selection);
  const VisiblePosition& visible_end_of_last_paragraph =
      EndOfParagraph(end_of_selection);
  const Position& end_of_next_last_paragraph =
      EndOfParagraph(NextPositionOf(visible_end_of_last_paragraph))
          .DeepEquivalent();
  Position end_of_last_paragraph =
      visible_end_of_last_paragraph.DeepEquivalent();

  bool at_end = false;
  while (end_of_current_paragraph.DeepEquivalent() !=
             end_of_next_last_paragraph &&
         !at_end) {
    if (end_of_current_paragraph.DeepEquivalent() == end_of_last_paragraph)
      at_end = true;

    Position start, end;
    RangeForParagraphSplittingTextNodesIfNeeded(
        end_of_current_paragraph, end_of_last_paragraph, start, end);
    end_of_current_paragraph = CreateVisiblePosition(end);

    Node* enclosing_cell = EnclosingNodeOfType(start, &IsTableCell);
    PositionWithAffinity end_of_next_paragraph =
        EndOfNextParagrahSplittingTextNodesIfNeeded(
            end_of_current_paragraph, end_of_last_paragraph, start, end)
            .ToPositionWithAffinity();

    FormatRange(start, end, end_of_last_paragraph, blockquote_for_next_indent,
                editing_state);
    if (editing_state->IsAborted())
      return;

    // Don't put the next paragraph in the blockquote we just created for this
    // paragraph unless the next paragraph is in the same cell.
    if (enclosing_cell &&
        enclosing_cell !=
            EnclosingNodeOfType(end_of_next_paragraph.GetPosition(),
                                &IsTableCell))
      blockquote_for_next_indent = nullptr;

    // indentIntoBlockquote could move more than one paragraph if the paragraph
    // is in a list item or a table. As a result,
    // |endOfNextLastParagraph| could refer to a position no longer in the
    // document.
    if (end_of_next_last_paragraph.IsNotNull() &&
        !end_of_next_last_paragraph.IsConnected())
      break;
    // Sanity check: Make sure our moveParagraph calls didn't remove
    // endOfNextParagraph.anchorNode() If somehow, e.g. mutation
    // event handler, we did, return to prevent crashes.
    if (end_of_next_paragraph.IsNotNull() &&
        !end_of_next_paragraph.IsConnected())
      return;

    GetDocument().UpdateStyleAndLayout();
    end_of_current_paragraph = CreateVisiblePosition(end_of_next_paragraph);
  }
}

static bool IsNewLineAtPosition(const Position& position) {
  auto* text_node = DynamicTo<Text>(position.ComputeContainerNode());
  int offset = position.OffsetInContainerNode();
  if (!text_node || offset < 0 ||
      offset >= static_cast<int>(text_node->length()))
    return false;

  DummyExceptionStateForTesting exception_state;
  String text_at_position =
      text_node->substringData(offset, 1, exception_state);
  if (exception_state.HadException())
    return false;

  return text_at_position[0] == '\n';
}

static const ComputedStyle* ComputedStyleOfEnclosingTextNode(
    const Position& position) {
  if (!position.IsOffsetInAnchor() || !position.ComputeContainerNode() ||
      !position.ComputeContainerNode()->IsTextNode())
    return nullptr;
  return position.ComputeContainerNode()->GetComputedStyle();
}

void ApplyBlockElementCommand::RangeForParagraphSplittingTextNodesIfNeeded(
    const VisiblePosition& end_of_current_paragraph,
    Position& end_of_last_paragraph,
    Position& start,
    Position& end) {
  start = StartOfParagraph(end_of_current_paragraph).DeepEquivalent();
  end = end_of_current_paragraph.DeepEquivalent();

  bool is_start_and_end_on_same_node = false;
  if (const ComputedStyle* start_style =
          ComputedStyleOfEnclosingTextNode(start)) {
    is_start_and_end_on_same_node =
        ComputedStyleOfEnclosingTextNode(end) &&
        start.ComputeContainerNode() == end.ComputeContainerNode();
    bool is_start_and_end_of_last_paragraph_on_same_node =
        ComputedStyleOfEnclosingTextNode(end_of_last_paragraph) &&
        start.ComputeContainerNode() ==
            end_of_last_paragraph.ComputeContainerNode();

    // Avoid obtanining the start of next paragraph for start
    // TODO(yosin) We should use |PositionMoveType::CodePoint| for
    // |previousPositionOf()|.
    if (start_style->PreserveNewline() && IsNewLineAtPosition(start) &&
        !IsNewLineAtPosition(
            PreviousPositionOf(start, PositionMoveType::kCodeUnit)) &&
        start.OffsetInContainerNode() > 0)
      start = StartOfParagraph(CreateVisiblePosition(PreviousPositionOf(
                                   end, PositionMoveType::kCodeUnit)))
                  .DeepEquivalent();

    // If start is in the middle of a text node, split.
    if (!start_style->CollapseWhiteSpace() &&
        start.OffsetInContainerNode() > 0) {
      int start_offset = start.OffsetInContainerNode();
      auto* start_text = To<Text>(start.ComputeContainerNode());
      SplitTextNode(start_text, start_offset);
      GetDocument().UpdateStyleAndLayoutTree();

      start = Position::FirstPositionInNode(*start_text);
      if (is_start_and_end_on_same_node) {
        DCHECK_GE(end.OffsetInContainerNode(), start_offset);
        end = Position(start_text, end.OffsetInContainerNode() - start_offset);
      }
      if (is_start_and_end_of_last_paragraph_on_same_node) {
        DCHECK_GE(end_of_last_paragraph.OffsetInContainerNode(), start_offset);
        end_of_last_paragraph =
            Position(start_text, end_of_last_paragraph.OffsetInContainerNode() -
                                     start_offset);
      }
    }
  }

  if (const ComputedStyle* end_style = ComputedStyleOfEnclosingTextNode(end)) {
    bool is_end_and_end_of_last_paragraph_on_same_node =
        ComputedStyleOfEnclosingTextNode(end_of_last_paragraph) &&
        end.AnchorNode() == end_of_last_paragraph.AnchorNode();
    // Include \n at the end of line if we're at an empty paragraph
    if (end_style->PreserveNewline() && start == end &&
        end.OffsetInContainerNode() <
            static_cast<int>(To<Text>(end.ComputeContainerNode())->length())) {
      int end_offset = end.OffsetInContainerNode();
      // TODO(yosin) We should use |PositionMoveType::CodePoint| for
      // |previousPositionOf()|.
      if (!IsNewLineAtPosition(
              PreviousPositionOf(end, PositionMoveType::kCodeUnit)) &&
          IsNewLineAtPosition(end))
        end = Position(end.ComputeContainerNode(), end_offset + 1);
      if (is_end_and_end_of_last_paragraph_on_same_node &&
          end.OffsetInContainerNode() >=
              end_of_last_paragraph.OffsetInContainerNode())
        end_of_last_paragraph = end;
    }

    // If end is in the middle of a text node, split.
    if (end_style->UserModify() != EUserModify::kReadOnly &&
        !end_style->CollapseWhiteSpace() && end.OffsetInContainerNode() &&
        end.OffsetInContainerNode() <
            static_cast<int>(To<Text>(end.ComputeContainerNode())->length())) {
      auto* end_container = To<Text>(end.ComputeContainerNode());
      SplitTextNode(end_container, end.OffsetInContainerNode());
      GetDocument().UpdateStyleAndLayoutTree();

      const Node* const previous_sibling_of_end =
          end_container->previousSibling();
      DCHECK(previous_sibling_of_end);
      if (is_start_and_end_on_same_node) {
        start = FirstPositionInOrBeforeNode(*previous_sibling_of_end);
      }
      if (is_end_and_end_of_last_paragraph_on_same_node) {
        if (end_of_last_paragraph.OffsetInContainerNode() ==
            end.OffsetInContainerNode()) {
          end_of_last_paragraph =
              LastPositionInOrAfterNode(*previous_sibling_of_end);
        } else {
          end_of_last_paragraph = Position(
              end_container, end_of_last_paragraph.OffsetInContainerNode() -
                                 end.OffsetInContainerNode());
        }
      }
      end = Position::LastPositionInNode(*previous_sibling_of_end);
    }
  }
}

VisiblePosition
ApplyBlockElementCommand::EndOfNextParagrahSplittingTextNodesIfNeeded(
    VisiblePosition& end_of_current_paragraph,
    Position& end_of_last_paragraph,
    Position& start,
    Position& end) {
  const VisiblePosition& end_of_next_paragraph =
      EndOfParagraph(NextPositionOf(end_of_current_paragraph));
  const Position& end_of_next_paragraph_position =
      end_of_next_paragraph.DeepEquivalent();
  const ComputedStyle* style =
      ComputedStyleOfEnclosingTextNode(end_of_next_paragraph_position);
  if (!style)
    return end_of_next_paragraph;

  auto* const end_of_next_paragraph_text =
      To<Text>(end_of_next_paragraph_position.ComputeContainerNode());
  if (!style->PreserveNewline() ||
      !end_of_next_paragraph_position.OffsetInContainerNode() ||
      !IsNewLineAtPosition(
          Position::FirstPositionInNode(*end_of_next_paragraph_text)))
    return end_of_next_paragraph;

  // \n at the beginning of the text node immediately following the current
  // paragraph is trimmed by moveParagraphWithClones. If endOfNextParagraph was
  // pointing at this same text node, endOfNextParagraph will be shifted by one
  // paragraph. Avoid this by splitting "\n"
  SplitTextNode(end_of_next_paragraph_text, 1);
  GetDocument().UpdateStyleAndLayout();
  Text* const previous_text =
      DynamicTo<Text>(end_of_next_paragraph_text->previousSibling());
  if (end_of_next_paragraph_text == start.ComputeContainerNode() &&
      previous_text) {
    DCHECK_LT(start.OffsetInContainerNode(),
              end_of_next_paragraph_position.OffsetInContainerNode());
    start = Position(previous_text, start.OffsetInContainerNode());
  }
  if (end_of_next_paragraph_text == end.ComputeContainerNode() &&
      previous_text) {
    DCHECK_LT(end.OffsetInContainerNode(),
              end_of_next_paragraph_position.OffsetInContainerNode());
    end = Position(previous_text, end.OffsetInContainerNode());
  }
  if (end_of_next_paragraph_text ==
      end_of_last_paragraph.ComputeContainerNode()) {
    if (end_of_last_paragraph.OffsetInContainerNode() <
        end_of_next_paragraph_position.OffsetInContainerNode()) {
      // We can only fix endOfLastParagraph if the previous node was still text
      // and hasn't been modified by script.
      if (previous_text && static_cast<unsigned>(
                               end_of_last_paragraph.OffsetInContainerNode()) <=
                               previous_text->length()) {
        end_of_last_paragraph = Position(
            previous_text, end_of_last_paragraph.OffsetInContainerNode());
      }
    } else {
      end_of_last_paragraph =
          Position(end_of_next_paragraph_text,
                   end_of_last_paragraph.OffsetInContainerNode() - 1);
    }
  }

  return CreateVisiblePosition(
      Position(end_of_next_paragraph_text,
               end_of_next_paragraph_position.OffsetInContainerNode() - 1));
}

HTMLElement* ApplyBlockElementCommand::CreateBlockElement() const {
  HTMLElement* element = CreateHTMLElement(GetDocument(), tag_name_);
  if (inline_style_.length())
    element->setAttribute(html_names::kStyleAttr, inline_style_);
  return element;
}

}  // namespace blink
