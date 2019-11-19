/*
 * Copyright (C) 2004, 2005, 2006, 2007 Apple Inc. All rights reserved.
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

// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/editing/commands/editing_commands_utilities.h"

#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/dom/text.h"
#include "third_party/blink/renderer/core/editing/commands/selection_for_undo_step.h"
#include "third_party/blink/renderer/core/editing/commands/typing_command.h"
#include "third_party/blink/renderer/core/editing/editing_utilities.h"
#include "third_party/blink/renderer/core/editing/frame_selection.h"
#include "third_party/blink/renderer/core/editing/selection_template.h"
#include "third_party/blink/renderer/core/editing/visible_position.h"
#include "third_party/blink/renderer/core/editing/visible_selection.h"
#include "third_party/blink/renderer/core/frame/local_frame_client.h"
#include "third_party/blink/renderer/core/frame/web_feature_forward.h"
#include "third_party/blink/renderer/core/html/html_body_element.h"
#include "third_party/blink/renderer/core/html/html_html_element.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/layout/layout_text.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"

namespace blink {

static bool HasARenderedDescendant(const Node* node,
                                   const Node* excluded_node) {
  for (const Node* n = node->firstChild(); n;) {
    if (n == excluded_node) {
      n = NodeTraversal::NextSkippingChildren(*n, node);
      continue;
    }
    if (n->GetLayoutObject())
      return true;
    n = NodeTraversal::Next(*n, node);
  }
  return false;
}

Node* HighestNodeToRemoveInPruning(Node* node, const Node* exclude_node) {
  Node* previous_node = nullptr;
  Element* element = node ? RootEditableElement(*node) : nullptr;
  for (; node; node = node->parentNode()) {
    if (LayoutObject* layout_object = node->GetLayoutObject()) {
      if (!layout_object->CanHaveChildren() ||
          HasARenderedDescendant(node, previous_node) || element == node ||
          exclude_node == node)
        return previous_node;
    }
    previous_node = node;
  }
  return nullptr;
}

Element* EnclosingTableCell(const Position& p) {
  return To<Element>(EnclosingNodeOfType(p, IsTableCell));
}

bool IsTableStructureNode(const Node* node) {
  LayoutObject* layout_object = node->GetLayoutObject();
  return (layout_object &&
          (layout_object->IsTableCell() || layout_object->IsTableRow() ||
           layout_object->IsTableSection() ||
           layout_object->IsLayoutTableCol()));
}

bool IsNodeRendered(const Node& node) {
  LayoutObject* layout_object = node.GetLayoutObject();
  if (!layout_object)
    return false;

  return layout_object->Style()->Visibility() == EVisibility::kVisible;
}

bool IsInline(const Node* node) {
  if (!node)
    return false;

  const ComputedStyle* style = node->GetComputedStyle();
  return style && style->Display() == EDisplay::kInline;
}

// FIXME: This method should not need to call
// isStartOfParagraph/isEndOfParagraph
Node* EnclosingEmptyListItem(const VisiblePosition& visible_pos) {
  DCHECK(visible_pos.IsValid());

  // Check that position is on a line by itself inside a list item
  Node* list_child_node =
      EnclosingListChild(visible_pos.DeepEquivalent().AnchorNode());
  if (!list_child_node || !IsStartOfParagraph(visible_pos) ||
      !IsEndOfParagraph(visible_pos))
    return nullptr;

  VisiblePosition first_in_list_child =
      CreateVisiblePosition(FirstPositionInOrBeforeNode(*list_child_node));
  VisiblePosition last_in_list_child =
      CreateVisiblePosition(LastPositionInOrAfterNode(*list_child_node));

  if (first_in_list_child.DeepEquivalent() != visible_pos.DeepEquivalent() ||
      last_in_list_child.DeepEquivalent() != visible_pos.DeepEquivalent())
    return nullptr;

  return list_child_node;
}

bool AreIdenticalElements(const Node& first, const Node& second) {
  const auto* first_element = DynamicTo<Element>(first);
  const auto* second_element = DynamicTo<Element>(second);
  if (!first_element || !second_element)
    return false;

  if (!first_element->HasTagName(second_element->TagQName()))
    return false;

  if (!first_element->HasEquivalentAttributes(*second_element))
    return false;

  return HasEditableStyle(*first_element) && HasEditableStyle(*second_element);
}

// FIXME: need to dump this
static bool IsSpecialHTMLElement(const Node& n) {
  if (!n.IsHTMLElement())
    return false;

  if (n.IsLink())
    return true;

  LayoutObject* layout_object = n.GetLayoutObject();
  if (!layout_object)
    return false;

  if (layout_object->Style()->Display() == EDisplay::kTable ||
      layout_object->Style()->Display() == EDisplay::kInlineTable)
    return true;

  if (layout_object->Style()->IsFloating())
    return true;

  return false;
}

static HTMLElement* FirstInSpecialElement(const Position& pos) {
  DCHECK(!NeedsLayoutTreeUpdate(pos));
  Element* element = RootEditableElement(*pos.ComputeContainerNode());
  for (Node& runner : NodeTraversal::InclusiveAncestorsOf(*pos.AnchorNode())) {
    if (RootEditableElement(runner) != element)
      break;
    if (IsSpecialHTMLElement(runner)) {
      auto* special_element = To<HTMLElement>(&runner);
      VisiblePosition v_pos = CreateVisiblePosition(pos);
      VisiblePosition first_in_element =
          CreateVisiblePosition(FirstPositionInOrBeforeNode(*special_element));
      if (IsDisplayInsideTable(special_element) &&
          !IsListItem(v_pos.DeepEquivalent().ComputeContainerNode()) &&
          v_pos.DeepEquivalent() ==
              NextPositionOf(first_in_element).DeepEquivalent())
        return special_element;
      if (v_pos.DeepEquivalent() == first_in_element.DeepEquivalent())
        return special_element;
    }
  }
  return nullptr;
}

static HTMLElement* LastInSpecialElement(const Position& pos) {
  DCHECK(!NeedsLayoutTreeUpdate(pos));
  Element* element = RootEditableElement(*pos.ComputeContainerNode());
  for (Node& runner : NodeTraversal::InclusiveAncestorsOf(*pos.AnchorNode())) {
    if (RootEditableElement(runner) != element)
      break;
    if (IsSpecialHTMLElement(runner)) {
      auto* special_element = To<HTMLElement>(&runner);
      VisiblePosition v_pos = CreateVisiblePosition(pos);
      VisiblePosition last_in_element =
          CreateVisiblePosition(LastPositionInOrAfterNode(*special_element));
      if (IsDisplayInsideTable(special_element) &&
          v_pos.DeepEquivalent() ==
              PreviousPositionOf(last_in_element).DeepEquivalent())
        return special_element;
      if (v_pos.DeepEquivalent() == last_in_element.DeepEquivalent())
        return special_element;
    }
  }
  return nullptr;
}

Position PositionBeforeContainingSpecialElement(
    const Position& pos,
    HTMLElement** containing_special_element) {
  DCHECK(!NeedsLayoutTreeUpdate(pos));
  HTMLElement* n = FirstInSpecialElement(pos);
  if (!n)
    return pos;
  Position result = Position::InParentBeforeNode(*n);
  if (result.IsNull() || RootEditableElement(*result.AnchorNode()) !=
                             RootEditableElement(*pos.AnchorNode()))
    return pos;
  if (containing_special_element)
    *containing_special_element = n;
  return result;
}

Position PositionAfterContainingSpecialElement(
    const Position& pos,
    HTMLElement** containing_special_element) {
  DCHECK(!NeedsLayoutTreeUpdate(pos));
  HTMLElement* n = LastInSpecialElement(pos);
  if (!n)
    return pos;
  Position result = Position::InParentAfterNode(*n);
  if (result.IsNull() || RootEditableElement(*result.AnchorNode()) !=
                             RootEditableElement(*pos.AnchorNode()))
    return pos;
  if (containing_special_element)
    *containing_special_element = n;
  return result;
}

bool LineBreakExistsAtPosition(const Position& position) {
  if (position.IsNull())
    return false;

  if (IsA<HTMLBRElement>(*position.AnchorNode()) &&
      position.AtFirstEditingPositionForNode())
    return true;

  if (!position.AnchorNode()->GetLayoutObject())
    return false;

  const auto* text_node = DynamicTo<Text>(position.AnchorNode());
  if (!text_node || !text_node->GetLayoutObject()->Style()->PreserveNewline())
    return false;

  unsigned offset = position.OffsetInContainerNode();
  return offset < text_node->length() && text_node->data()[offset] == '\n';
}

// return first preceding DOM position rendered at a different location, or
// "this"
static Position PreviousCharacterPosition(const Position& position,
                                          TextAffinity affinity) {
  DCHECK(!NeedsLayoutTreeUpdate(position));
  if (position.IsNull())
    return Position();

  Element* from_root_editable_element =
      RootEditableElement(*position.AnchorNode());

  bool at_start_of_line =
      IsStartOfLine(CreateVisiblePosition(position, affinity));
  bool rendered = IsVisuallyEquivalentCandidate(position);

  Position current_pos = position;
  while (!current_pos.AtStartOfTree()) {
    // TODO(yosin) When we use |previousCharacterPosition()| other than
    // finding leading whitespace, we should use |Character| instead of
    // |CodePoint|.
    current_pos = PreviousPositionOf(current_pos, PositionMoveType::kCodeUnit);

    if (RootEditableElement(*current_pos.AnchorNode()) !=
        from_root_editable_element)
      return position;

    if (at_start_of_line || !rendered) {
      if (IsVisuallyEquivalentCandidate(current_pos))
        return current_pos;
    } else if (RendersInDifferentPosition(position, current_pos)) {
      return current_pos;
    }
  }

  return position;
}

// This assumes that it starts in editable content.
Position LeadingCollapsibleWhitespacePosition(const Position& position,
                                              TextAffinity affinity,
                                              WhitespacePositionOption option) {
  DCHECK(!NeedsLayoutTreeUpdate(position));
  DCHECK(IsEditablePosition(position)) << position;
  if (position.IsNull())
    return Position();

  if (IsA<HTMLBRElement>(*MostBackwardCaretPosition(position).AnchorNode()))
    return Position();

  const Position& prev = PreviousCharacterPosition(position, affinity);
  if (prev == position)
    return Position();
  const Node* const anchor_node = prev.AnchorNode();
  auto* anchor_text_node = DynamicTo<Text>(anchor_node);
  if (!anchor_text_node)
    return Position();
  if (EnclosingBlockFlowElement(*anchor_node) !=
      EnclosingBlockFlowElement(*position.AnchorNode()))
    return Position();
  if (option == kNotConsiderNonCollapsibleWhitespace &&
      anchor_node->GetLayoutObject() &&
      !anchor_node->GetLayoutObject()->Style()->CollapseWhiteSpace())
    return Position();
  const String& string = anchor_text_node->data();
  const UChar previous_character = string[prev.ComputeOffsetInContainerNode()];
  const bool is_space = option == kConsiderNonCollapsibleWhitespace
                            ? (IsSpaceOrNewline(previous_character) ||
                               previous_character == kNoBreakSpaceCharacter)
                            : IsCollapsibleWhitespace(previous_character);
  if (!is_space || !IsEditablePosition(prev))
    return Position();
  return prev;
}

unsigned NumEnclosingMailBlockquotes(const Position& p) {
  unsigned num = 0;
  for (const Node* n = p.AnchorNode(); n; n = n->parentNode()) {
    if (IsMailHTMLBlockquoteElement(n))
      num++;
  }
  return num;
}

bool LineBreakExistsAtVisiblePosition(const VisiblePosition& visible_position) {
  return LineBreakExistsAtPosition(
      MostForwardCaretPosition(visible_position.DeepEquivalent()));
}

HTMLElement* CreateHTMLElement(Document& document, const QualifiedName& name) {
  DCHECK_EQ(name.NamespaceURI(), html_names::xhtmlNamespaceURI)
      << "Unexpected namespace: " << name;
  return To<HTMLElement>(document.CreateElement(
      name, CreateElementFlags::ByCloneNode(), g_null_atom));
}

HTMLElement* EnclosingList(const Node* node) {
  if (!node)
    return nullptr;

  ContainerNode* root = HighestEditableRoot(FirstPositionInOrBeforeNode(*node));

  for (Node& runner : NodeTraversal::AncestorsOf(*node)) {
    if (IsA<HTMLUListElement>(runner) || IsA<HTMLOListElement>(runner))
      return To<HTMLElement>(&runner);
    if (runner == root)
      return nullptr;
  }

  return nullptr;
}

Node* EnclosingListChild(const Node* node) {
  if (!node)
    return nullptr;
  // Check for a list item element, or for a node whose parent is a list
  // element. Such a node will appear visually as a list item (but without a
  // list marker)
  ContainerNode* root = HighestEditableRoot(FirstPositionInOrBeforeNode(*node));

  // FIXME: This function is inappropriately named if it starts with node
  // instead of node->parentNode()
  for (Node* n = const_cast<Node*>(node); n && n->parentNode();
       n = n->parentNode()) {
    if (IsA<HTMLLIElement>(*n) ||
        (IsHTMLListElement(n->parentNode()) && n != root))
      return n;
    if (n == root || IsTableCell(n))
      return nullptr;
  }

  return nullptr;
}

HTMLElement* OutermostEnclosingList(const Node* node,
                                    const HTMLElement* root_list) {
  HTMLElement* list = EnclosingList(node);
  if (!list)
    return nullptr;

  while (HTMLElement* next_list = EnclosingList(list)) {
    if (next_list == root_list)
      break;
    list = next_list;
  }

  return list;
}

// Determines whether two positions are visibly next to each other (first then
// second) while ignoring whitespaces and unrendered nodes
static bool IsVisiblyAdjacent(const Position& first, const Position& second) {
  return CreateVisiblePosition(first).DeepEquivalent() ==
         CreateVisiblePosition(MostBackwardCaretPosition(second))
             .DeepEquivalent();
}

bool CanMergeLists(const Element& first_list, const Element& second_list) {
  if (!first_list.IsHTMLElement() || !second_list.IsHTMLElement())
    return false;

  DCHECK(!NeedsLayoutTreeUpdate(first_list));
  DCHECK(!NeedsLayoutTreeUpdate(second_list));
  return first_list.HasTagName(
             second_list
                 .TagQName())  // make sure the list types match (ol vs. ul)
         && HasEditableStyle(first_list) &&
         HasEditableStyle(second_list)  // both lists are editable
         &&
         RootEditableElement(first_list) ==
             RootEditableElement(second_list)  // don't cross editing boundaries
         && IsVisiblyAdjacent(Position::InParentAfterNode(first_list),
                              Position::InParentBeforeNode(second_list));
  // Make sure there is no visible content between this li and the previous list
}

// Modifies selections that have an end point at the edge of a table
// that contains the other endpoint so that they don't confuse
// code that iterates over selected paragraphs.
VisibleSelection SelectionForParagraphIteration(
    const VisibleSelection& original) {
  VisibleSelection new_selection(original);
  VisiblePosition start_of_selection(new_selection.VisibleStart());
  VisiblePosition end_of_selection(new_selection.VisibleEnd());

  // If the end of the selection to modify is just after a table, and if the
  // start of the selection is inside that table, then the last paragraph that
  // we'll want modify is the last one inside the table, not the table itself (a
  // table is itself a paragraph).
  if (Element* table = TableElementJustBefore(end_of_selection)) {
    DCHECK(start_of_selection.IsNotNull()) << new_selection;
    if (start_of_selection.DeepEquivalent().AnchorNode()->IsDescendantOf(
            table)) {
      const VisiblePosition& new_end =
          PreviousPositionOf(end_of_selection, kCannotCrossEditingBoundary);
      if (new_end.IsNotNull()) {
        new_selection = CreateVisibleSelection(
            SelectionInDOMTree::Builder()
                .Collapse(start_of_selection.ToPositionWithAffinity())
                .Extend(new_end.DeepEquivalent())
                .Build());
      } else {
        new_selection = CreateVisibleSelection(
            SelectionInDOMTree::Builder()
                .Collapse(start_of_selection.ToPositionWithAffinity())
                .Build());
      }
    }
  }

  // If the start of the selection to modify is just before a table, and if the
  // end of the selection is inside that table, then the first paragraph we'll
  // want to modify is the first one inside the table, not the paragraph
  // containing the table itself.
  if (Element* table = TableElementJustAfter(start_of_selection)) {
    DCHECK(end_of_selection.IsNotNull()) << new_selection;
    if (end_of_selection.DeepEquivalent().AnchorNode()->IsDescendantOf(table)) {
      const VisiblePosition new_start =
          NextPositionOf(start_of_selection, kCannotCrossEditingBoundary);
      if (new_start.IsNotNull()) {
        new_selection = CreateVisibleSelection(
            SelectionInDOMTree::Builder()
                .Collapse(new_start.ToPositionWithAffinity())
                .Extend(end_of_selection.DeepEquivalent())
                .Build());
      } else {
        new_selection = CreateVisibleSelection(
            SelectionInDOMTree::Builder()
                .Collapse(end_of_selection.ToPositionWithAffinity())
                .Build());
      }
    }
  }

  return new_selection;
}

const String& NonBreakingSpaceString() {
  DEFINE_STATIC_LOCAL(String, non_breaking_space_string,
                      (&kNoBreakSpaceCharacter, 1));
  return non_breaking_space_string;
}

// TODO(tkent): This is a workaround of some crash bugs in the editing code,
// which assumes a document has a valid HTML structure. We should make the
// editing code more robust, and should remove this hack. crbug.com/580941.
void TidyUpHTMLStructure(Document& document) {
  // hasEditableStyle() needs up-to-date ComputedStyle.
  document.UpdateStyleAndLayoutTree();
  const bool needs_valid_structure =
      HasEditableStyle(document) ||
      (document.documentElement() &&
       HasEditableStyle(*document.documentElement()));
  if (!needs_valid_structure)
    return;

  Element* const current_root = document.documentElement();
  if (current_root && IsA<HTMLHtmlElement>(current_root))
    return;
  Element* const existing_head =
      current_root && IsA<HTMLHeadElement>(current_root) ? current_root
                                                         : nullptr;
  Element* const existing_body =
      current_root && (IsA<HTMLBodyElement>(current_root) ||
                       IsA<HTMLFrameSetElement>(current_root))
          ? current_root
          : nullptr;
  // We ensure only "the root is <html>."
  // documentElement as rootEditableElement is problematic.  So we move
  // non-<html> root elements under <body>, and the <body> works as
  // rootEditableElement.
  document.AddConsoleMessage(ConsoleMessage::Create(
      mojom::ConsoleMessageSource::kJavaScript,
      mojom::ConsoleMessageLevel::kWarning,
      "document.execCommand() doesn't work with an invalid HTML structure. It "
      "is corrected automatically."));
  UseCounter::Count(document, WebFeature::kExecCommandAltersHTMLStructure);

  auto* const root = MakeGarbageCollected<HTMLHtmlElement>(document);
  if (existing_head)
    root->AppendChild(existing_head);
  auto* const body = existing_body
                         ? existing_body
                         : MakeGarbageCollected<HTMLBodyElement>(document);
  if (document.documentElement() && body != document.documentElement())
    body->AppendChild(document.documentElement());
  root->AppendChild(body);
  DCHECK(!document.documentElement());
  document.AppendChild(root);

  // TODO(tkent): Should we check and move Text node children of <html>?
}

InputEvent::InputType DeletionInputTypeFromTextGranularity(
    DeleteDirection direction,
    TextGranularity granularity) {
  using InputType = InputEvent::InputType;
  switch (direction) {
    case DeleteDirection::kForward:
      if (granularity == TextGranularity::kWord)
        return InputType::kDeleteWordForward;
      if (granularity == TextGranularity::kLineBoundary)
        return InputType::kDeleteSoftLineForward;
      if (granularity == TextGranularity::kParagraphBoundary)
        return InputType::kDeleteHardLineForward;
      return InputType::kDeleteContentForward;
    case DeleteDirection::kBackward:
      if (granularity == TextGranularity::kWord)
        return InputType::kDeleteWordBackward;
      if (granularity == TextGranularity::kLineBoundary)
        return InputType::kDeleteSoftLineBackward;
      if (granularity == TextGranularity::kParagraphBoundary)
        return InputType::kDeleteHardLineBackward;
      return InputType::kDeleteContentBackward;
    default:
      return InputType::kNone;
  }
}

void DispatchEditableContentChangedEvents(Element* start_root,
                                          Element* end_root) {
  if (start_root) {
    start_root->DispatchEvent(
        *Event::Create(event_type_names::kWebkitEditableContentChanged));
  }
  if (end_root && end_root != start_root) {
    end_root->DispatchEvent(
        *Event::Create(event_type_names::kWebkitEditableContentChanged));
  }
}

static void DispatchInputEvent(Element* target,
                               InputEvent::InputType input_type,
                               const String& data,
                               InputEvent::EventIsComposing is_composing) {
  if (!target)
    return;
  // TODO(editing-dev): Pass appreciate |ranges| after it's defined on spec.
  // http://w3c.github.io/editing/input-events.html#dom-inputevent-inputtype
  InputEvent* const input_event =
      InputEvent::CreateInput(input_type, data, is_composing, nullptr);
  target->DispatchScopedEvent(*input_event);
}

void DispatchInputEventEditableContentChanged(
    Element* start_root,
    Element* end_root,
    InputEvent::InputType input_type,
    const String& data,
    InputEvent::EventIsComposing is_composing) {
  if (start_root)
    DispatchInputEvent(start_root, input_type, data, is_composing);
  if (end_root && end_root != start_root)
    DispatchInputEvent(end_root, input_type, data, is_composing);
}

SelectionInDOMTree CorrectedSelectionAfterCommand(
    const SelectionForUndoStep& passed_selection,
    const Document* document) {
  if (!passed_selection.Base().IsValidFor(*document) ||
      !passed_selection.Extent().IsValidFor(*document))
    return SelectionInDOMTree();
  return passed_selection.AsSelection();
}

void ChangeSelectionAfterCommand(LocalFrame* frame,
                                 const SelectionInDOMTree& new_selection,
                                 const SetSelectionOptions& options) {
  if (new_selection.IsNone())
    return;
  // See <rdar://problem/5729315> Some shouldChangeSelectedDOMRange contain
  // Ranges for selections that are no longer valid
  const bool selection_did_not_change_dom_position =
      new_selection == frame->Selection().GetSelectionInDOMTree() &&
      options.IsDirectional() == frame->Selection().IsDirectional();
  const bool handle_visible =
      frame->Selection().IsHandleVisible() && new_selection.IsRange();
  frame->Selection().SetSelection(new_selection,
                                  SetSelectionOptions::Builder(options)
                                      .SetShouldShowHandle(handle_visible)
                                      .SetIsDirectional(options.IsDirectional())
                                      .Build());

  // Some editing operations change the selection visually without affecting its
  // position within the DOM. For example when you press return in the following
  // (the caret is marked by ^):
  // <div contentEditable="true"><div>^Hello</div></div>
  // WebCore inserts <div><br></div> *before* the current block, which correctly
  // moves the paragraph down but which doesn't change the caret's DOM position
  // (["hello", 0]). In these situations the above FrameSelection::setSelection
  // call does not call LocalFrameClient::DidChangeSelection(), which, on the
  // Mac, sends selection change notifications and starts a new kill ring
  // sequence, but we want to do these things (matches AppKit).
  if (!selection_did_not_change_dom_position)
    return;
  frame->Client()->DidChangeSelection(
      frame->Selection().GetSelectionInDOMTree().Type() != kRangeSelection);
}

InputEvent::EventIsComposing IsComposingFromCommand(
    const CompositeEditCommand* command) {
  auto* typing_command = DynamicTo<TypingCommand>(command);
  if (typing_command &&
      typing_command->CompositionType() != TypingCommand::kTextCompositionNone)
    return InputEvent::EventIsComposing::kIsComposing;
  return InputEvent::EventIsComposing::kNotComposing;
}

// ---------

VisiblePosition StartOfBlock(const VisiblePosition& visible_position,
                             EditingBoundaryCrossingRule rule) {
  DCHECK(visible_position.IsValid()) << visible_position;
  Position position = visible_position.DeepEquivalent();
  Element* start_block =
      position.ComputeContainerNode()
          ? EnclosingBlock(position.ComputeContainerNode(), rule)
          : nullptr;
  return start_block ? VisiblePosition::FirstPositionInNode(*start_block)
                     : VisiblePosition();
}

VisiblePosition EndOfBlock(const VisiblePosition& visible_position,
                           EditingBoundaryCrossingRule rule) {
  DCHECK(visible_position.IsValid()) << visible_position;
  Position position = visible_position.DeepEquivalent();
  Element* end_block =
      position.ComputeContainerNode()
          ? EnclosingBlock(position.ComputeContainerNode(), rule)
          : nullptr;
  return end_block ? VisiblePosition::LastPositionInNode(*end_block)
                   : VisiblePosition();
}

bool IsStartOfBlock(const VisiblePosition& pos) {
  DCHECK(pos.IsValid()) << pos;
  return pos.IsNotNull() &&
         pos.DeepEquivalent() ==
             StartOfBlock(pos, kCanCrossEditingBoundary).DeepEquivalent();
}

bool IsEndOfBlock(const VisiblePosition& pos) {
  DCHECK(pos.IsValid()) << pos;
  return pos.IsNotNull() &&
         pos.DeepEquivalent() ==
             EndOfBlock(pos, kCanCrossEditingBoundary).DeepEquivalent();
}

}  // namespace blink
