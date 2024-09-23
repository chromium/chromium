/*
 * Copyright (C) 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.
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

#include "third_party/blink/renderer/core/editing/commands/composite_edit_command.h"

#include <algorithm>

#include "third_party/blink/renderer/core/accessibility/blink_ax_event_intent.h"
#include "third_party/blink/renderer/core/accessibility/scoped_blink_ax_event_intent.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/document_fragment.h"
#include "third_party/blink/renderer/core/dom/element_traversal.h"
#include "third_party/blink/renderer/core/dom/events/scoped_event_queue.h"
#include "third_party/blink/renderer/core/dom/node_traversal.h"
#include "third_party/blink/renderer/core/dom/range.h"
#include "third_party/blink/renderer/core/dom/text.h"
#include "third_party/blink/renderer/core/editing/commands/append_node_command.h"
#include "third_party/blink/renderer/core/editing/commands/apply_style_command.h"
#include "third_party/blink/renderer/core/editing/commands/delete_from_text_node_command.h"
#include "third_party/blink/renderer/core/editing/commands/delete_selection_command.h"
#include "third_party/blink/renderer/core/editing/commands/editing_commands_utilities.h"
#include "third_party/blink/renderer/core/editing/commands/insert_into_text_node_command.h"
#include "third_party/blink/renderer/core/editing/commands/insert_line_break_command.h"
#include "third_party/blink/renderer/core/editing/commands/insert_node_before_command.h"
#include "third_party/blink/renderer/core/editing/commands/insert_paragraph_separator_command.h"
#include "third_party/blink/renderer/core/editing/commands/merge_identical_elements_command.h"
#include "third_party/blink/renderer/core/editing/commands/remove_css_property_command.h"
#include "third_party/blink/renderer/core/editing/commands/remove_node_command.h"
#include "third_party/blink/renderer/core/editing/commands/remove_node_preserving_children_command.h"
#include "third_party/blink/renderer/core/editing/commands/replace_node_with_span_command.h"
#include "third_party/blink/renderer/core/editing/commands/replace_selection_command.h"
#include "third_party/blink/renderer/core/editing/commands/set_character_data_command.h"
#include "third_party/blink/renderer/core/editing/commands/set_node_attribute_command.h"
#include "third_party/blink/renderer/core/editing/commands/split_element_command.h"
#include "third_party/blink/renderer/core/editing/commands/split_text_node_command.h"
#include "third_party/blink/renderer/core/editing/commands/split_text_node_containing_element_command.h"
#include "third_party/blink/renderer/core/editing/commands/undo_stack.h"
#include "third_party/blink/renderer/core/editing/commands/wrap_contents_in_dummy_span_command.h"
#include "third_party/blink/renderer/core/editing/editing_utilities.h"
#include "third_party/blink/renderer/core/editing/editor.h"
#include "third_party/blink/renderer/core/editing/ephemeral_range.h"
#include "third_party/blink/renderer/core/editing/frame_selection.h"
#include "third_party/blink/renderer/core/editing/iterators/text_iterator.h"
#include "third_party/blink/renderer/core/editing/markers/document_marker_controller.h"
#include "third_party/blink/renderer/core/editing/plain_text_range.h"
#include "third_party/blink/renderer/core/editing/relocatable_position.h"
#include "third_party/blink/renderer/core/editing/selection_template.h"
#include "third_party/blink/renderer/core/editing/serializers/serialization.h"
#include "third_party/blink/renderer/core/editing/visible_position.h"
#include "third_party/blink/renderer/core/editing/visible_selection.h"
#include "third_party/blink/renderer/core/editing/visible_units.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/html/forms/html_input_element.h"
#include "third_party/blink/renderer/core/html/html_br_element.h"
#include "third_party/blink/renderer/core/html/html_div_element.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/html/html_li_element.h"
#include "third_party/blink/renderer/core/html/html_object_element.h"
#include "third_party/blink/renderer/core/html/html_olist_element.h"
#include "third_party/blink/renderer/core/html/html_quote_element.h"
#include "third_party/blink/renderer/core/html/html_span_element.h"
#include "third_party/blink/renderer/core/html/html_ulist_element.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/layout/layout_block_flow.h"
#include "third_party/blink/renderer/core/layout/layout_text.h"
#include "third_party/blink/renderer/core/page/chrome_client.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/heap/collection_support/clear_collection_scope.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/instrumentation/resource_coordinator/document_resource_coordinator.h"

namespace blink {

namespace {

bool IsWhitespaceForRebalance(const Text& text_node, UChar character) {
  if (IsWhitespace(character)) {
    if (character == kNewlineCharacter &&
        RuntimeEnabledFeatures::InsertLineBreakIfPhrasingContentEnabled()) {
      return !text_node.GetLayoutObject() ||
             text_node.GetLayoutObject()->StyleRef().ShouldCollapseBreaks();
    }
    return true;
  }
  return false;
}

}  // namespace

CompositeEditCommand::CompositeEditCommand(Document& document)
    : EditCommand(document) {
  const VisibleSelection& visible_selection =
      document.GetFrame()
          ->Selection()
          .ComputeVisibleSelectionInDOMTreeDeprecated();
  SetStartingSelection(
      SelectionForUndoStep::From(visible_selection.AsSelection()));
  SetEndingSelection(starting_selection_);
}

CompositeEditCommand::~CompositeEditCommand() {
  DCHECK(IsTopLevelCommand() || !undo_step_);
}

VisibleSelection CompositeEditCommand::EndingVisibleSelection() const {
  // TODO(editing-dev): The use of
  // |Document::UpdateStyleAndLayout()|
  // needs to be audited.  See http://crbug.com/590369 for more details.
  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kEditing);
  return CreateVisibleSelection(ending_selection_);
}

bool CompositeEditCommand::Apply() {
  DCHECK(!IsCommandGroupWrapper());
  if (!IsRichlyEditablePosition(EndingVisibleSelection().Anchor())) {
    switch (GetInputType()) {
      case InputEvent::InputType::kInsertText:
      case InputEvent::InputType::kInsertLineBreak:
      case InputEvent::InputType::kInsertParagraph:
      case InputEvent::InputType::kInsertFromPaste:
      case InputEvent::InputType::kInsertFromDrop:
      case InputEvent::InputType::kInsertFromYank:
      case InputEvent::InputType::kInsertTranspose:
      case InputEvent::InputType::kInsertReplacementText:
      case InputEvent::InputType::kInsertCompositionText:
      case InputEvent::InputType::kInsertLink:
      case InputEvent::InputType::kDeleteWordBackward:
      case InputEvent::InputType::kDeleteWordForward:
      case InputEvent::InputType::kDeleteSoftLineBackward:
      case InputEvent::InputType::kDeleteSoftLineForward:
      case InputEvent::InputType::kDeleteHardLineBackward:
      case InputEvent::InputType::kDeleteHardLineForward:
      case InputEvent::InputType::kDeleteContentBackward:
      case InputEvent::InputType::kDeleteContentForward:
      case InputEvent::InputType::kDeleteByCut:
      case InputEvent::InputType::kDeleteByDrag:
      case InputEvent::InputType::kNone:
        break;
      default:
        return false;
    }
  }
  EnsureUndoStep();

  // Changes to the document may have been made since the last editing operation
  // that require a layout, as in <rdar://problem/5658603>. Low level
  // operations, like RemoveNodeCommand, don't require a layout because the high
  // level operations that use them perform one if one is necessary (like for
  // the creation of VisiblePositions).
  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kEditing);

  LocalFrame* frame = GetDocument().GetFrame();
  DCHECK(frame);
  // directional is stored at the top level command, so that before and after
  // executing command same directional will be there.
  SetSelectionIsDirectional(frame->Selection().IsDirectional());
  GetUndoStep()->SetSelectionIsDirectional(SelectionIsDirectional());

  // Provides details to accessibility about any text change caused by applying
  // this command, throughout the current call stack.
  ScopedBlinkAXEventIntent scoped_blink_ax_event_intent(
      BlinkAXEventIntent::FromEditCommand(*this), &GetDocument());

  EditingState editing_state;
  EventQueueScope event_queue_scope;
  DoApply(&editing_state);

  // Only need to call appliedEditing for top-level commands, and TypingCommands
  // do it on their own (see TypingCommand::typingAddedToOpenCommand).
  if (!IsTypingCommand())
    AppliedEditing();
  return !editing_state.IsAborted();
}

UndoStep* CompositeEditCommand::EnsureUndoStep() {
  CompositeEditCommand* command = this;
  while (command && command->Parent())
    command = command->Parent();
  if (!command->undo_step_) {
    command->undo_step_ = MakeGarbageCollected<UndoStep>(
        &GetDocument(), StartingSelection(), EndingSelection());
  }
  return command->undo_step_.Get();
}

bool CompositeEditCommand::PreservesTypingStyle() const {
  return false;
}

bool CompositeEditCommand::IsTypingCommand() const {
  return false;
}

bool CompositeEditCommand::IsCommandGroupWrapper() const {
  return false;
}

bool CompositeEditCommand::IsDragAndDropCommand() const {
  return false;
}

bool CompositeEditCommand::IsReplaceSelectionCommand() const {
  return false;
}

//
// sugary-sweet convenience functions to help create and apply edit commands in
// composite commands
//
void CompositeEditCommand::ApplyCommandToComposite(
    EditCommand* command,
    EditingState* editing_state) {
  command->SetParent(this);
  command->SetSelectionIsDirectional(SelectionIsDirectional());
  command->DoApply(editing_state);
  if (editing_state->IsAborted()) {
    command->SetParent(nullptr);
    return;
  }
  if (auto* simple_edit_command = DynamicTo<SimpleEditCommand>(command)) {
    command->SetParent(nullptr);
    EnsureUndoStep()->Append(simple_edit_command);
  }
  commands_.push_back(command);
}

void CompositeEditCommand::AppendCommandToUndoStep(
    CompositeEditCommand* command) {
  EnsureUndoStep()->Append(command->EnsureUndoStep());
  command->undo_step_ = nullptr;
  command->SetParent(this);
  commands_.push_back(command);
}

void CompositeEditCommand::ApplyStyle(const EditingStyle* style,
                                      EditingState* editing_state) {
  ApplyCommandToComposite(
      MakeGarbageCollected<ApplyStyleCommand>(GetDocument(), style,
                                              InputEvent::InputType::kNone),
      editing_state);
}

void CompositeEditCommand::ApplyStyle(const EditingStyle* style,
                                      const Position& start,
                                      const Position& end,
                                      EditingState* editing_state) {
  ApplyCommandToComposite(
      MakeGarbageCollected<ApplyStyleCommand>(GetDocument(), style, start, end),
      editing_state);
}

void CompositeEditCommand::ApplyStyledElement(Element* element,
                                              EditingState* editing_state) {
  ApplyCommandToComposite(
      MakeGarbageCollected<ApplyStyleCommand>(element, false), editing_state);
}

void CompositeEditCommand::RemoveStyledElement(Element* element,
                                               EditingState* editing_state) {
  ApplyCommandToComposite(
      MakeGarbageCollected<ApplyStyleCommand>(element, true), editing_state);
}

void CompositeEditCommand::InsertParagraphSeparator(
    EditingState* editing_state,
    bool use_default_paragraph_element,
    bool paste_blockqutoe_into_unquoted_area) {
  ApplyCommandToComposite(MakeGarbageCollected<InsertParagraphSeparatorCommand>(
                              GetDocument(), use_default_paragraph_element,
                              paste_blockqutoe_into_unquoted_area),
                          editing_state);
}

bool CompositeEditCommand::IsRemovableBlock(const Node* node) {
  DCHECK(node);
  const auto* element = DynamicTo<HTMLDivElement>(node);
  if (!element)
    return false;

  ContainerNode* parent_node = element->parentNode();
  if (parent_node && parent_node->firstChild() != parent_node->lastChild())
    return false;

  if (!element->hasAttributes())
    return true;

  return false;
}

void CompositeEditCommand::InsertNodeBefore(
    Node* insert_child,
    Node* ref_child,
    EditingState* editing_state,
    ShouldAssumeContentIsAlwaysEditable
        should_assume_content_is_always_editable) {
  ABORT_EDITING_COMMAND_IF(GetDocument().body() == ref_child);
  ABORT_EDITING_COMMAND_IF(!ref_child->parentNode());
  // TODO(editing-dev): Use of UpdateStyleAndLayout
  // needs to be audited.  See http://crbug.com/590369 for more details.
  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kEditing);
  ABORT_EDITING_COMMAND_IF(!IsEditable(*ref_child->parentNode()) &&
                           ref_child->parentNode()->InActiveDocument());
  ApplyCommandToComposite(
      MakeGarbageCollected<InsertNodeBeforeCommand>(
          insert_child, ref_child, should_assume_content_is_always_editable),
      editing_state);
}

void CompositeEditCommand::InsertNodeAfter(Node* insert_child,
                                           Node* ref_child,
                                           EditingState* editing_state) {
  ABORT_EDITING_COMMAND_IF(!ref_child->parentNode());
  DCHECK(insert_child);
  DCHECK(ref_child);
  ABORT_EDITING_COMMAND_IF(GetDocument().body() == ref_child);
  ContainerNode* parent = ref_child->parentNode();
  DCHECK(parent);
  DCHECK(!parent->IsShadowRoot()) << parent;
  if (parent->lastChild() == ref_child) {
    AppendNode(insert_child, parent, editing_state);
  } else {
    DCHECK(ref_child->nextSibling()) << ref_child;
    InsertNodeBefore(insert_child, ref_child->nextSibling(), editing_state);
  }
}

void CompositeEditCommand::InsertNodeAt(Node* insert_child,
                                        const Position& editing_position,
                                        EditingState* editing_state) {
  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kEditing);
  ABORT_EDITING_COMMAND_IF(!IsEditablePosition(editing_position));
  // For editing positions like [table, 0], insert before the table,
  // likewise for replaced elements, brs, etc.
  Position p = editing_position.ParentAnchoredEquivalent();
  Node* ref_child = p.AnchorNode();
  int offset = p.OffsetInContainerNode();

  auto* ref_child_text_node = DynamicTo<Text>(ref_child);
  if (CanHaveChildrenForEditing(ref_child)) {
    Node* child = ref_child->firstChild();
    for (int i = 0; child && i < offset; i++)
      child = child->nextSibling();
    if (child)
      InsertNodeBefore(insert_child, child, editing_state);
    else
      AppendNode(insert_child, To<ContainerNode>(ref_child), editing_state);
  } else if (CaretMinOffset(ref_child) >= offset) {
    InsertNodeBefore(insert_child, ref_child, editing_state);
  } else if (ref_child_text_node && CaretMaxOffset(ref_child) > offset) {
    SplitTextNode(ref_child_text_node, offset);

    // Mutation events (bug 22634) from the text node insertion may have
    // removed the refChild
    if (!ref_child->isConnected())
      return;
    InsertNodeBefore(insert_child, ref_child, editing_state);
  } else {
    InsertNodeAfter(insert_child, ref_child, editing_state);
  }
}

void CompositeEditCommand::AppendNode(Node* node,
                                      ContainerNode* parent,
                                      EditingState* editing_state) {
  // When cloneParagraphUnderNewElement() clones the fallback content
  // of an OBJECT element, the ASSERT below may fire since the return
  // value of canHaveChildrenForEditing is not reliable until the layout
  // object of the OBJECT is created. Hence we ignore this check for OBJECTs.
  // TODO(yosin): We should move following |ABORT_EDITING_COMMAND_IF|s to
  // |AppendNodeCommand|.
  // TODO(yosin): We should get rid of |canHaveChildrenForEditing()|, since
  // |cloneParagraphUnderNewElement()| attempt to clone non-well-formed HTML,
  // produced by JavaScript.
  auto* parent_element = DynamicTo<Element>(parent);
  ABORT_EDITING_COMMAND_IF(!CanHaveChildrenForEditing(parent) &&
                           !(parent_element && parent_element->TagQName() ==
                                                   html_names::kObjectTag));
  ABORT_EDITING_COMMAND_IF(!IsEditable(*parent) && parent->InActiveDocument());
  ApplyCommandToComposite(MakeGarbageCollected<AppendNodeCommand>(parent, node),
                          editing_state);
}

void CompositeEditCommand::RemoveAllChildrenIfPossible(
    ContainerNode* container,
    EditingState* editing_state,
    ShouldAssumeContentIsAlwaysEditable
        should_assume_content_is_always_editable) {
  Node* child = container->firstChild();
  while (child) {
    Node* const next = child->nextSibling();
    RemoveNode(child, editing_state, should_assume_content_is_always_editable);
    if (editing_state->IsAborted())
      return;
    if (next && next->parentNode() != container) {
      // |RemoveNode()| moves |next| outside |node|.
      return;
    }
    child = next;
  }
}

void CompositeEditCommand::RemoveChildrenInRange(Node* node,
                                                 unsigned from,
                                                 unsigned to,
                                                 EditingState* editing_state) {
  HeapVector<Member<Node>> children;
  Node* child = NodeTraversal::ChildAt(*node, from);
  for (unsigned i = from; child && i < to; i++, child = child->nextSibling())
    children.push_back(child);

  size_t size = children.size();
  for (wtf_size_t i = 0; i < size; ++i) {
    RemoveNode(children[i].Release(), editing_state);
    if (editing_state->IsAborted())
      return;
  }
}

void CompositeEditCommand::RemoveNode(
    Node* node,
    EditingState* editing_state,
    ShouldAssumeContentIsAlwaysEditable
        should_assume_content_is_always_editable) {
  if (!node || !node->NonShadowBoundaryParentNode())
    return;
  ABORT_EDITING_COMMAND_IF(!node->GetDocument().GetFrame());
  ApplyCommandToComposite(MakeGarbageCollected<RemoveNodeCommand>(
                              node, should_assume_content_is_always_editable),
                          editing_state);
}

void CompositeEditCommand::RemoveNodePreservingChildren(
    Node* node,
    EditingState* editing_state,
    ShouldAssumeContentIsAlwaysEditable
        should_assume_content_is_always_editable) {
  ABORT_EDITING_COMMAND_IF(!node->GetDocument().GetFrame());
  ApplyCommandToComposite(
      MakeGarbageCollected<RemoveNodePreservingChildrenCommand>(
          node, should_assume_content_is_always_editable),
      editing_state);
}

void CompositeEditCommand::RemoveNodeAndPruneAncestors(
    Node* node,
    EditingState* editing_state,
    Node* exclude_node) {
  DCHECK_NE(node, exclude_node);
  ContainerNode* parent = node->parentNode();
  RemoveNode(node, editing_state);
  if (editing_state->IsAborted())
    return;
  Prune(parent, editing_state, exclude_node);
}

void CompositeEditCommand::MoveRemainingSiblingsToNewParent(
    Node* node,
    Node* past_last_node_to_move,
    Element* new_parent,
    EditingState* editing_state) {
  NodeVector nodes_to_remove;

  for (; node && node != past_last_node_to_move; node = node->nextSibling())
    nodes_to_remove.push_back(node);

  for (unsigned i = 0; i < nodes_to_remove.size(); i++) {
    RemoveNode(nodes_to_remove[i], editing_state);
    if (editing_state->IsAborted())
      return;
    AppendNode(nodes_to_remove[i], new_parent, editing_state);
    if (editing_state->IsAborted())
      return;
  }
}

void CompositeEditCommand::UpdatePositionForNodeRemovalPreservingChildren(
    Position& position,
    Node& node) {
  int offset =
      position.IsOffsetInAnchor() ? position.OffsetInContainerNode() : 0;
  position = ComputePositionForNodeRemoval(position, node);
  if (offset == 0)
    return;
  position = Position::CreateWithoutValidationDeprecated(
      *position.ComputeContainerNode(), offset);
}

HTMLSpanElement*
CompositeEditCommand::ReplaceElementWithSpanPreservingChildrenAndAttributes(
    HTMLElement* node) {
  // It would also be possible to implement all of ReplaceNodeWithSpanCommand
  // as a series of existing smaller edit commands.  Someone who wanted to
  // reduce the number of edit commands could do so here.
  auto* command = MakeGarbageCollected<ReplaceNodeWithSpanCommand>(node);
  // ReplaceNodeWithSpanCommand is never aborted.
  ApplyCommandToComposite(command, ASSERT_NO_EDITING_ABORT);
  // Returning a raw pointer here is OK because the command is retained by
  // applyCommandToComposite (thus retaining the span), and the span is also
  // in the DOM tree, and thus alive whie it has a parent.
  DCHECK(command->SpanElement()->isConnected()) << command->SpanElement();
  return command->SpanElement();
}

void CompositeEditCommand::Prune(Node* node,
                                 EditingState* editing_state,
                                 Node* exclude_node) {
  if (Node* highest_node_to_remove =
          HighestNodeToRemoveInPruning(node, exclude_node))
    RemoveNode(highest_node_to_remove, editing_state);
}

void CompositeEditCommand::SplitTextNode(Text* node, unsigned offset) {
  // SplitTextNodeCommand is never aborted.
  ApplyCommandToComposite(
      MakeGarbageCollected<SplitTextNodeCommand>(node, offset),
      ASSERT_NO_EDITING_ABORT);
}

void CompositeEditCommand::SplitElement(Element* element, Node* at_child) {
  // SplitElementCommand is never aborted.
  ApplyCommandToComposite(
      MakeGarbageCollected<SplitElementCommand>(element, at_child),
      ASSERT_NO_EDITING_ABORT);
}

void CompositeEditCommand::MergeIdenticalElements(Element* first,
                                                  Element* second,
                                                  EditingState* editing_state) {
  DCHECK(!first->IsDescendantOf(second)) << first << " " << second;
  DCHECK_NE(second, first);
  if (first->nextSibling() != second) {
    RemoveNode(second, editing_state);
    if (editing_state->IsAborted())
      return;
    InsertNodeAfter(second, first, editing_state);
    if (editing_state->IsAborted())
      return;
  }
  ApplyCommandToComposite(
      MakeGarbageCollected<MergeIdenticalElementsCommand>(first, second),
      editing_state);
}

void CompositeEditCommand::WrapContentsInDummySpan(Element* element) {
  // WrapContentsInDummySpanCommand is never aborted.
  ApplyCommandToComposite(
      MakeGarbageCollected<WrapContentsInDummySpanCommand>(element),
      ASSERT_NO_EDITING_ABORT);
}

void CompositeEditCommand::SplitTextNodeContainingElement(Text* text,
                                                          unsigned offset) {
  // SplitTextNodeContainingElementCommand is never aborted.
  ApplyCommandToComposite(
      MakeGarbageCollected<SplitTextNodeContainingElementCommand>(text, offset),
      ASSERT_NO_EDITING_ABORT);
}

void CompositeEditCommand::InsertTextIntoNode(Text* node,
                                              unsigned offset,
                                              const String& text) {
  // InsertIntoTextNodeCommand is never aborted.
  if (!text.empty())
    ApplyCommandToComposite(
        MakeGarbageCollected<InsertIntoTextNodeCommand>(node, offset, text),
        ASSERT_NO_EDITING_ABORT);
}

void CompositeEditCommand::DeleteTextFromNode(Text* node,
                                              unsigned offset,
                                              unsigned count) {
  // DeleteFromTextNodeCommand is never aborted.
  ApplyCommandToComposite(
      MakeGarbageCollected<DeleteFromTextNodeCommand>(node, offset, count),
      ASSERT_NO_EDITING_ABORT);
}

void CompositeEditCommand::ReplaceTextInNode(Text* node,
                                             unsigned offset,
                                             unsigned count,
                                             const String& replacement_text) {
  // SetCharacterDataCommand is never aborted.
  ApplyCommandToComposite(MakeGarbageCollected<SetCharacterDataCommand>(
                              node, offset, count, replacement_text),
                          ASSERT_NO_EDITING_ABORT);
}

Position CompositeEditCommand::ReplaceSelectedTextInNode(const String& text) {
  const Position& start = EndingSelection().Start();
  const Position& end = EndingSelection().End();
  auto* text_node = DynamicTo<Text>(start.ComputeContainerNode());
  if (!text_node || text_node != end.ComputeContainerNode() ||
      IsTabHTMLSpanElementTextNode(text_node))
    return Position();

  ReplaceTextInNode(text_node, start.OffsetInContainerNode(),
                    end.OffsetInContainerNode() - start.OffsetInContainerNode(),
                    text);

  return Position(text_node, start.OffsetInContainerNode() + text.length());
}

Position CompositeEditCommand::PositionOutsideTabSpan(const Position& pos) {
  Node* anchor_node = pos.AnchorNode();
  if (!IsTabHTMLSpanElementTextNode(anchor_node)) {
    return pos;
  }

  switch (pos.AnchorType()) {
    case PositionAnchorType::kAfterChildren:
      NOTREACHED_IN_MIGRATION();
      return pos;
    case PositionAnchorType::kOffsetInAnchor:
      break;
    case PositionAnchorType::kBeforeAnchor:
      return Position::InParentBeforeNode(*anchor_node);
    case PositionAnchorType::kAfterAnchor:
      return Position::InParentAfterNode(*anchor_node);
  }

  HTMLSpanElement* tab_span = TabSpanElement(pos.ComputeContainerNode());
  DCHECK(tab_span);

  // TODO(editing-dev): Hoist this UpdateStyleAndLayout
  // to the callers. See crbug.com/590369 for details.
  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kEditing);

  if (pos.OffsetInContainerNode() <= CaretMinOffset(pos.ComputeContainerNode()))
    return Position::InParentBeforeNode(*tab_span);

  if (pos.OffsetInContainerNode() >=
      CaretMaxOffset(pos.ComputeContainerNode())) {
    return anchor_node->HasNextSibling() &&
                   RuntimeEnabledFeatures::
                       PositionOutsideTabSpanCheckSiblingNodeEnabled()
               ? Position::InParentAfterNode(*anchor_node)
               : Position::InParentAfterNode(*tab_span);
  }

  SplitTextNodeContainingElement(To<Text>(pos.ComputeContainerNode()),
                                 pos.OffsetInContainerNode());
  return Position::InParentBeforeNode(*tab_span);
}

void CompositeEditCommand::InsertNodeAtTabSpanPosition(
    Node* node,
    const Position& pos,
    EditingState* editing_state) {
  // insert node before, after, or at split of tab span
  InsertNodeAt(node, PositionOutsideTabSpan(pos), editing_state);
}

bool CompositeEditCommand::DeleteSelection(
    EditingState* editing_state,
    const DeleteSelectionOptions& options) {
  if (!EndingSelection().IsRange())
    return true;

  ApplyCommandToComposite(
      MakeGarbageCollected<DeleteSelectionCommand>(GetDocument(), options),
      editing_state);
  if (editing_state->IsAborted())
    return false;

  if (!EndingSelection().IsValidFor(GetDocument())) {
    editing_state->Abort();
    return false;
  }
  return true;
}

void CompositeEditCommand::RemoveCSSProperty(Element* element,
                                             CSSPropertyID property) {
  // RemoveCSSPropertyCommand is never aborted.
  ApplyCommandToComposite(MakeGarbageCollected<RemoveCSSPropertyCommand>(
                              GetDocument(), element, property),
                          ASSERT_NO_EDITING_ABORT);
}

void CompositeEditCommand::RemoveElementAttribute(
    Element* element,
    const QualifiedName& attribute) {
  SetNodeAttribute(element, attribute, AtomicString());
}

void CompositeEditCommand::SetNodeAttribute(Element* element,
                                            const QualifiedName& attribute,
                                            const AtomicString& value) {
  // SetNodeAttributeCommand is never aborted.
  ApplyCommandToComposite(
      MakeGarbageCollected<SetNodeAttributeCommand>(element, attribute, value),
      ASSERT_NO_EDITING_ABORT);
}

bool CompositeEditCommand::CanRebalance(const Position& position) const {
  // TODO(editing-dev): Use of UpdateStyleAndLayout()
  // needs to be audited.  See http://crbug.com/590369 for more details.
  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kEditing);

  auto* text_node = DynamicTo<Text>(position.ComputeContainerNode());
  if (!position.IsOffsetInAnchor() || !text_node ||
      !IsRichlyEditable(*text_node)) {
    return false;
  }

  if (text_node->length() == 0)
    return false;

  LayoutText* layout_text = text_node->GetLayoutObject();
  if (layout_text && layout_text->Style()->ShouldPreserveWhiteSpaces()) {
    return false;
  }

  return true;
}

// FIXME: Doesn't go into text nodes that contribute adjacent text (siblings,
// cousins, etc).
void CompositeEditCommand::RebalanceWhitespaceAt(const Position& position) {
  Node* node = position.ComputeContainerNode();
  if (!CanRebalance(position))
    return;

  // If the rebalance is for the single offset, and neither text[offset] nor
  // text[offset - 1] are some form of whitespace, do nothing.
  int offset = position.ComputeOffsetInContainerNode();
  String text = To<Text>(node)->data();
  if (!IsWhitespace(text[offset])) {
    offset--;
    if (offset < 0 || !IsWhitespace(text[offset]))
      return;
  }

  RebalanceWhitespaceOnTextSubstring(To<Text>(node),
                                     position.OffsetInContainerNode(),
                                     position.OffsetInContainerNode());
}

void CompositeEditCommand::RebalanceWhitespaceOnTextSubstring(Text* text_node,
                                                              int start_offset,
                                                              int end_offset) {
  String text = text_node->data();
  DCHECK(!text.empty());

  // Set upstream and downstream to define the extent of the whitespace
  // surrounding text[offset].
  int upstream = start_offset;
  while (upstream > 0 &&
         IsWhitespaceForRebalance(*text_node, text[upstream - 1])) {
    upstream--;
  }

  int downstream = end_offset;
  while ((unsigned)downstream < text.length() &&
         IsWhitespaceForRebalance(*text_node, text[downstream])) {
    downstream++;
  }

  int length = downstream - upstream;
  if (!length)
    return;

  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kEditing);
  VisiblePosition visible_upstream_pos =
      CreateVisiblePosition(Position(text_node, upstream));
  VisiblePosition visible_downstream_pos =
      CreateVisiblePosition(Position(text_node, downstream));

  String string = text.Substring(upstream, length);
  // FIXME: Because of the problem mentioned at the top of this function, we
  // must also use nbsps at the start/end of the string because this function
  // doesn't get all surrounding whitespace, just the whitespace in the
  // current text node. However, if the next sibling node is a text node
  // (not empty, see http://crbug.com/632300), we should use a plain space.
  // See http://crbug.com/310149
  auto* next_text_node = DynamicTo<Text>(text_node->nextSibling());
  const bool next_sibling_is_text_node =
      next_text_node && next_text_node->data().length() &&
      !IsWhitespace(next_text_node->data()[0]);
  const bool should_emit_nbs_pbefore_end =
      (IsEndOfParagraph(visible_downstream_pos) ||
       (unsigned)downstream == text.length()) &&
      !next_sibling_is_text_node;
  String rebalanced_string = StringWithRebalancedWhitespace(
      string, IsStartOfParagraph(visible_upstream_pos) || !upstream,
      should_emit_nbs_pbefore_end);

  if (string != rebalanced_string)
    ReplaceTextInNode(text_node, upstream, length, rebalanced_string);
}

void CompositeEditCommand::PrepareWhitespaceAtPositionForSplit(
    Position& position) {
  if (!IsRichlyEditablePosition(position))
    return;

  auto* text_node = DynamicTo<Text>(position.AnchorNode());
  if (!text_node)
    return;

  if (text_node->length() == 0)
    return;
  LayoutText* layout_text = text_node->GetLayoutObject();
  if (layout_text && layout_text->Style()->ShouldPreserveWhiteSpaces()) {
    return;
  }

  // Delete collapsed whitespace so that inserting nbsps doesn't uncollapse it.
  Position upstream_pos = MostBackwardCaretPosition(position);
  RelocatablePosition* relocatable_upstream_pos =
      MakeGarbageCollected<RelocatablePosition>(upstream_pos);
  DeleteInsignificantText(upstream_pos, MostForwardCaretPosition(position));

  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kEditing);
  position = MostForwardCaretPosition(relocatable_upstream_pos->GetPosition());
  VisiblePosition visible_pos = CreateVisiblePosition(position);
  VisiblePosition previous_visible_pos = PreviousPositionOf(visible_pos);
  ReplaceCollapsibleWhitespaceWithNonBreakingSpaceIfNeeded(
      previous_visible_pos);

  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kEditing);
  ReplaceCollapsibleWhitespaceWithNonBreakingSpaceIfNeeded(
      CreateVisiblePosition(position));
}

void CompositeEditCommand::
    ReplaceCollapsibleWhitespaceWithNonBreakingSpaceIfNeeded(
        const VisiblePosition& visible_position) {
  if (!IsCollapsibleWhitespace(CharacterAfter(visible_position)))
    return;
  Position pos = MostForwardCaretPosition(visible_position.DeepEquivalent());
  auto* container_text_node = DynamicTo<Text>(pos.ComputeContainerNode());
  if (!container_text_node)
    return;
  ReplaceTextInNode(container_text_node, pos.OffsetInContainerNode(), 1,
                    NonBreakingSpaceString());
}

void CompositeEditCommand::RebalanceWhitespace() {
  VisibleSelection selection = EndingVisibleSelection();
  if (selection.IsNone())
    return;

  RebalanceWhitespaceAt(selection.Start());
  if (selection.IsRange())
    RebalanceWhitespaceAt(selection.End());
}

static bool IsInsignificantText(const LayoutText& layout_text) {
  if (layout_text.HasInlineFragments())
    return false;
  // Spaces causing line break don't have `FragmentItem` but it has
  // non-zero length. See http://crbug.com/1322746
  return !layout_text.ResolvedTextLength();
}

void CompositeEditCommand::DeleteInsignificantText(Text* text_node,
                                                   unsigned start,
                                                   unsigned end) {
  if (!text_node || start >= end)
    return;

  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kEditing);

  LayoutText* text_layout_object = text_node->GetLayoutObject();
  if (!text_layout_object)
    return;

  if (IsInsignificantText(*text_layout_object)) {
    // whole text node is empty
    // Removing a Text node won't dispatch synchronous events.
    RemoveNode(text_node, ASSERT_NO_EDITING_ABORT);
    return;
  }
  unsigned length = text_node->length();
  if (start >= length || end > length)
    return;

  CHECK(text_layout_object->IsInLayoutNGInlineFormattingContext());
  const String string = PlainText(
      EphemeralRange(Position(*text_node, start), Position(*text_node, end)),
      TextIteratorBehavior::Builder().SetEmitsOriginalText(true).Build());
  if (string.empty()) {
    return DeleteTextFromNode(text_node, start, end - start);
  }
  // Replace the text between start and end with collapsed version.
  return ReplaceTextInNode(text_node, start, end - start, string);
}

void CompositeEditCommand::DeleteInsignificantText(const Position& start,
                                                   const Position& end) {
  if (start.IsNull() || end.IsNull())
    return;

  if (ComparePositions(start, end) >= 0)
    return;

  HeapVector<Member<Text>> nodes;
  for (Node& node : NodeTraversal::StartsAt(*start.AnchorNode())) {
    if (auto* text_node = DynamicTo<Text>(&node))
      nodes.push_back(text_node);
    if (&node == end.AnchorNode())
      break;
  }

  for (const auto& node : nodes) {
    Text* text_node = node;
    int start_offset = text_node == start.AnchorNode()
                           ? start.ComputeOffsetInContainerNode()
                           : 0;
    int end_offset = text_node == end.AnchorNode()
                         ? end.ComputeOffsetInContainerNode()
                         : static_cast<int>(text_node->length());
    DeleteInsignificantText(text_node, start_offset, end_offset);
  }
}

void CompositeEditCommand::DeleteInsignificantTextDownstream(
    const Position& pos) {
  DCHECK(!GetDocument().NeedsLayoutTreeUpdate());
  Position end = MostForwardCaretPosition(
      NextPositionOf(CreateVisiblePosition(pos)).DeepEquivalent());
  DeleteInsignificantText(pos, end);
}

HTMLBRElement* CompositeEditCommand::AppendBlockPlaceholder(
    Element* container,
    EditingState* editing_state) {
  if (!container)
    return nullptr;

  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kEditing);

  // Should assert isLayoutBlockFlow || isInlineFlow when deletion improves. See
  // 4244964.
  // Note: When `container` is newly created <object> as fallback content, it
  // isn't associated to layout object. See http://crbug.com/1357082
  DCHECK(container->GetLayoutObject() ||
         Traversal<HTMLObjectElement>::FirstAncestor(*container))
      << container;

  auto* placeholder = MakeGarbageCollected<HTMLBRElement>(GetDocument());
  AppendNode(placeholder, container, editing_state);
  if (editing_state->IsAborted())
    return nullptr;
  return placeholder;
}

HTMLBRElement* CompositeEditCommand::InsertBlockPlaceholder(
    const Position& pos,
    EditingState* editing_state) {
  if (pos.IsNull())
    return nullptr;

  // Should assert isLayoutBlockFlow || isInlineFlow when deletion improves. See
  // 4244964.
  DCHECK(pos.AnchorNode()->GetLayoutObject()) << pos;

  auto* placeholder = MakeGarbageCollected<HTMLBRElement>(GetDocument());
  InsertNodeAt(placeholder, pos, editing_state);
  if (editing_state->IsAborted())
    return nullptr;
  return placeholder;
}

static bool IsEmptyListItem(const LayoutBlockFlow& block_flow) {
  if (block_flow.IsLayoutListItem()) {
    return !block_flow.FirstChild();
  }
  return false;
}

HTMLBRElement* CompositeEditCommand::AddBlockPlaceholderIfNeeded(
    Element* container,
    EditingState* editing_state) {
  if (!container)
    return nullptr;

  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kEditing);

  auto* block = DynamicTo<LayoutBlockFlow>(container->GetLayoutObject());
  if (!block)
    return nullptr;

  // append the placeholder to make sure it follows
  // any unrendered blocks
  if (block->Size().height == 0 || IsEmptyListItem(*block)) {
    return AppendBlockPlaceholder(container, editing_state);
  }

  return nullptr;
}

// Assumes that the position is at a placeholder and does the removal without
// much checking.
void CompositeEditCommand::RemovePlaceholderAt(const Position& p) {
  DCHECK(LineBreakExistsAtPosition(p)) << p;

  // We are certain that the position is at a line break, but it may be a br or
  // a preserved newline.
  if (IsA<HTMLBRElement>(*p.AnchorNode())) {
    // Removing a BR element won't dispatch synchronous events.
    RemoveNode(p.AnchorNode(), ASSERT_NO_EDITING_ABORT);
    return;
  }

  DeleteTextFromNode(To<Text>(p.AnchorNode()), p.OffsetInContainerNode(), 1);
}

HTMLElement* CompositeEditCommand::InsertNewDefaultParagraphElementAt(
    const Position& position,
    EditingState* editing_state) {
  HTMLElement* paragraph_element = CreateDefaultParagraphElement(GetDocument());
  paragraph_element->AppendChild(
      MakeGarbageCollected<HTMLBRElement>(GetDocument()));
  InsertNodeAt(paragraph_element, position, editing_state);
  if (editing_state->IsAborted())
    return nullptr;
  return paragraph_element;
}

// If the paragraph is not entirely within it's own block, create one and move
// the paragraph into it, and return that block.  Otherwise return 0.
HTMLElement* CompositeEditCommand::MoveParagraphContentsToNewBlockIfNecessary(
    const Position& pos,
    EditingState* editing_state) {
  DCHECK(!GetDocument().NeedsLayoutTreeUpdate());
  DCHECK(IsEditablePosition(pos)) << pos;

  // It's strange that this function is responsible for verifying that pos has
  // not been invalidated by an earlier call to this function.  The caller,
  // applyBlockStyle, should do this.
  VisiblePosition visible_pos = CreateVisiblePosition(pos);
  if (visible_pos.IsNull()) {
    editing_state->Abort();
    return nullptr;
  }
  VisiblePosition visible_paragraph_start = StartOfParagraph(visible_pos);
  VisiblePosition visible_paragraph_end = EndOfParagraph(visible_pos);
  VisiblePosition next = NextPositionOf(visible_paragraph_end);
  VisiblePosition visible_end = next.IsNotNull() ? next : visible_paragraph_end;

  Position upstream_start =
      MostBackwardCaretPosition(visible_paragraph_start.DeepEquivalent());
  Position upstream_end =
      MostBackwardCaretPosition(visible_end.DeepEquivalent());

  // If there are no VisiblePositions in the same block as pos then
  // upstreamStart will be outside the paragraph
  if (ComparePositions(pos, upstream_start) < 0)
    return nullptr;

  // Perform some checks to see if we need to perform work in this function.
  if (IsEnclosingBlock(upstream_start.AnchorNode())) {
    // If the block is the root editable element, always move content to a new
    // block, since it is illegal to modify attributes on the root editable
    // element for editing.
    if (upstream_start.AnchorNode() == RootEditableElementOf(upstream_start)) {
      // If the block is the root editable element and it contains no visible
      // content, create a new block but don't try and move content into it,
      // since there's nothing for moveParagraphs to move.
      if (!HasRenderedNonAnonymousDescendantsWithHeight(
              upstream_start.AnchorNode()->GetLayoutObject()))
        return InsertNewDefaultParagraphElementAt(upstream_start,
                                                  editing_state);
    } else if (IsEnclosingBlock(upstream_end.AnchorNode())) {
      if (!upstream_end.AnchorNode()->IsDescendantOf(
              upstream_start.AnchorNode())) {
        // If the paragraph end is a descendant of paragraph start, then we need
        // to run the rest of this function. If not, we can bail here.
        return nullptr;
      }
    } else if (EnclosingBlock(upstream_end.AnchorNode()) !=
               upstream_start.AnchorNode()) {
      // It should be an ancestor of the paragraph start.
      // We can bail as we have a full block to work with.
      return nullptr;
    } else if (IsEndOfEditableOrNonEditableContent(visible_end)) {
      // At the end of the editable region. We can bail here as well.
      return nullptr;
    }
  }

  if (visible_paragraph_end.IsNull())
    return nullptr;

  HTMLElement* const new_block =
      InsertNewDefaultParagraphElementAt(upstream_start, editing_state);
  if (editing_state->IsAborted())
    return nullptr;
  DCHECK(new_block);

  bool end_was_br =
      IsA<HTMLBRElement>(*visible_paragraph_end.DeepEquivalent().AnchorNode());

  // Inserting default paragraph element can change visible position. We
  // should update visible positions before use them.
  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kEditing);
  const VisiblePosition& destination =
      VisiblePosition::FirstPositionInNode(*new_block);
  if (destination.IsNull()) {
    // Reached by CompositeEditingCommandTest
    //    .MoveParagraphContentsToNewBlockWithNonEditableStyle.
    editing_state->Abort();
    return nullptr;
  }

  visible_pos = CreateVisiblePosition(pos);
  if (visible_pos.IsNull()) {
    editing_state->Abort();
    return nullptr;
  }
  visible_paragraph_start = StartOfParagraph(visible_pos);
  visible_paragraph_end = EndOfParagraph(visible_pos);
  DCHECK_LE(visible_paragraph_start.DeepEquivalent(),
            visible_paragraph_end.DeepEquivalent());
  MoveParagraphs(visible_paragraph_start, visible_paragraph_end, destination,
                 editing_state);
  if (editing_state->IsAborted())
    return nullptr;

  if (new_block->lastChild() && IsA<HTMLBRElement>(*new_block->lastChild()) &&
      !end_was_br) {
    RemoveNode(new_block->lastChild(), editing_state);
    if (editing_state->IsAborted())
      return nullptr;
  }

  return new_block;
}

void CompositeEditCommand::PushAnchorElementDown(Element* anchor_node,
                                                 EditingState* editing_state) {
  if (!anchor_node)
    return;

  DCHECK(anchor_node->IsLink()) << anchor_node;

  const VisibleSelection& visible_selection = CreateVisibleSelection(
      SelectionInDOMTree::Builder().SelectAllChildren(*anchor_node).Build());
  SetEndingSelection(
      SelectionForUndoStep::From(visible_selection.AsSelection()));
  ApplyStyledElement(anchor_node, editing_state);
  if (editing_state->IsAborted())
    return;
  // Clones of anchorNode have been pushed down, now remove it.
  if (anchor_node->isConnected())
    RemoveNodePreservingChildren(anchor_node, editing_state);
}

// Clone the paragraph between start and end under blockElement,
// preserving the hierarchy up to outerNode.

void CompositeEditCommand::CloneParagraphUnderNewElement(
    const Position& start,
    const Position& end,
    Node* passed_outer_node,
    Element* block_element,
    EditingState* editing_state) {
  DCHECK_LE(start, end);
  DCHECK(passed_outer_node);
  DCHECK(block_element);

  // First we clone the outerNode
  Node* last_node = nullptr;
  Node* outer_node = passed_outer_node;

  if (IsRootEditableElement(*outer_node)) {
    last_node = block_element;
  } else {
    last_node = outer_node->cloneNode(IsDisplayInsideTable(outer_node));
    AppendNode(last_node, block_element, editing_state);
    if (editing_state->IsAborted())
      return;
  }

  if (start.AnchorNode() != outer_node && last_node->IsElementNode() &&
      start.AnchorNode()->IsDescendantOf(outer_node)) {
    HeapVector<Member<Node>> ancestors;

    // Insert each node from innerNode to outerNode (excluded) in a list.
    for (Node& runner :
         NodeTraversal::InclusiveAncestorsOf(*start.AnchorNode())) {
      if (runner == outer_node)
        break;
      ancestors.push_back(runner);
    }

    // Clone every node between start.anchorNode() and outerBlock.

    for (wtf_size_t i = ancestors.size(); i != 0; --i) {
      Node* item = ancestors[i - 1].Get();
      Node* child = item->cloneNode(IsDisplayInsideTable(item));
      AppendNode(child, To<Element>(last_node), editing_state);
      if (editing_state->IsAborted())
        return;
      last_node = child;
    }
  }

  // Scripts specified in javascript protocol may remove |outerNode|
  // during insertion, e.g. <iframe src="javascript:...">
  if (!outer_node->isConnected())
    return;

  // Handle the case of paragraphs with more than one node,
  // cloning all the siblings until end.anchorNode() is reached.

  if (start.AnchorNode() != end.AnchorNode() &&
      !start.AnchorNode()->IsDescendantOf(end.AnchorNode())) {
    // If end is not a descendant of outerNode we need to
    // find the first common ancestor to increase the scope
    // of our nextSibling traversal.
    while (outer_node && !end.AnchorNode()->IsDescendantOf(outer_node)) {
      outer_node = outer_node->parentNode();
    }

    if (!outer_node)
      return;

    Node* start_node = start.AnchorNode();
    for (Node* node =
             NodeTraversal::NextSkippingChildren(*start_node, outer_node);
         node; node = NodeTraversal::NextSkippingChildren(*node, outer_node)) {
      // Move lastNode up in the tree as much as node was moved up in the tree
      // by NodeTraversal::nextSkippingChildren, so that the relative depth
      // between node and the original start node is maintained in the clone.
      while (start_node && last_node &&
             start_node->parentNode() != node->parentNode()) {
        start_node = start_node->parentNode();
        last_node = last_node->parentNode();
      }

      if (!last_node || !last_node->parentNode())
        return;

      Node* cloned_node = node->cloneNode(true);
      InsertNodeAfter(cloned_node, last_node, editing_state);
      if (editing_state->IsAborted())
        return;
      last_node = cloned_node;
      if (node == end.AnchorNode() || end.AnchorNode()->IsDescendantOf(node))
        break;
    }
  }
}

// There are bugs in deletion when it removes a fully selected table/list.
// It expands and removes the entire table/list, but will let content
// before and after the table/list collapse onto one line.
// Deleting a paragraph will leave a placeholder. Remove it (and prune
// empty or unrendered parents).

void CompositeEditCommand::CleanupAfterDeletion(EditingState* editing_state) {
  CleanupAfterDeletion(editing_state, VisiblePosition());
}

void CompositeEditCommand::CleanupAfterDeletion(EditingState* editing_state,
                                                VisiblePosition destination) {
  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kEditing);

  VisiblePosition caret_after_delete = EndingVisibleSelection().VisibleStart();
  Node* destination_node = destination.DeepEquivalent().AnchorNode();
  if (caret_after_delete.DeepEquivalent() != destination.DeepEquivalent() &&
      IsStartOfParagraph(caret_after_delete) &&
      IsEndOfParagraph(caret_after_delete)) {
    // Note: We want the rightmost candidate.
    Position position =
        MostForwardCaretPosition(caret_after_delete.DeepEquivalent());
    Node* node = position.AnchorNode();

    // InsertListCommandTest.CleanupNodeSameAsDestinationNode reaches here.
    ABORT_EDITING_COMMAND_IF(destination_node == node);
    // Bail if we'd remove an ancestor of our destination.
    if (destination_node && destination_node->IsDescendantOf(node))
      return;

    // Normally deletion will leave a br as a placeholder.
    if (IsA<HTMLBRElement>(*node)) {
      RemoveNodeAndPruneAncestors(node, editing_state, destination_node);

      // If the selection to move was empty and in an empty block that
      // doesn't require a placeholder to prop itself open (like a bordered
      // div or an li), remove it during the move (the list removal code
      // expects this behavior).
    } else if (IsEnclosingBlock(node)) {
      // If caret position after deletion and destination position coincides,
      // node should not be removed.
      if (!RendersInDifferentPosition(position, destination.DeepEquivalent())) {
        Prune(node, editing_state, destination_node);
        return;
      }
      RemoveNodeAndPruneAncestors(node, editing_state, destination_node);
    } else if (LineBreakExistsAtPosition(position)) {
      // There is a preserved '\n' at caretAfterDelete.
      // We can safely assume this is a text node.
      auto* text_node = To<Text>(node);
      if (text_node->length() == 1)
        RemoveNodeAndPruneAncestors(node, editing_state, destination_node);
      else
        DeleteTextFromNode(text_node, position.ComputeOffsetInContainerNode(),
                           1);
    }
  }
}

// This is a version of moveParagraph that preserves style by keeping the
// original markup. It is currently used only by IndentOutdentCommand but it is
// meant to be used in the future by several other commands such as InsertList
// and the align commands.
// The blockElement parameter is the element to move the paragraph to, outerNode
// is the top element of the paragraph hierarchy.

void CompositeEditCommand::MoveParagraphWithClones(
    const VisiblePosition& start_of_paragraph_to_move,
    const VisiblePosition& end_of_paragraph_to_move,
    HTMLElement* block_element,
    Node* outer_node,
    EditingState* editing_state) {
  // InsertListCommandTest.InsertListWithCollapsedVisibility reaches here.
  ABORT_EDITING_COMMAND_IF(start_of_paragraph_to_move.IsNull());
  ABORT_EDITING_COMMAND_IF(end_of_paragraph_to_move.IsNull());
  DCHECK(outer_node);
  DCHECK(block_element);

  RelocatablePosition* relocatable_before_paragraph =
      MakeGarbageCollected<RelocatablePosition>(
          PreviousPositionOf(start_of_paragraph_to_move).DeepEquivalent());
  RelocatablePosition* relocatable_after_paragraph =
      MakeGarbageCollected<RelocatablePosition>(
          NextPositionOf(end_of_paragraph_to_move).DeepEquivalent());

  // We upstream() the end and downstream() the start so that we don't include
  // collapsed whitespace in the move. When we paste a fragment, spaces after
  // the end and before the start are treated as though they were rendered.
  Position start =
      MostForwardCaretPosition(start_of_paragraph_to_move.DeepEquivalent());
  Position end = start_of_paragraph_to_move.DeepEquivalent() ==
                         end_of_paragraph_to_move.DeepEquivalent()
                     ? start
                     : MostBackwardCaretPosition(
                           end_of_paragraph_to_move.DeepEquivalent());
  if (ComparePositions(start, end) > 0)
    end = start;

  CloneParagraphUnderNewElement(start, end, outer_node, block_element,
                                editing_state);

  SetEndingSelection(SelectionForUndoStep::From(
      SelectionInDOMTree::Builder().Collapse(start).Extend(end).Build()));
  if (!DeleteSelection(
          editing_state,
          DeleteSelectionOptions::Builder().SetSanitizeMarkup(true).Build()))
    return;

  // There are bugs in deletion when it removes a fully selected table/list.
  // It expands and removes the entire table/list, but will let content
  // before and after the table/list collapse onto one line.

  CleanupAfterDeletion(editing_state);
  if (editing_state->IsAborted())
    return;

  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kEditing);

  // Add a br if pruning an empty block level element caused a collapse.  For
  // example:
  // foo^
  // <div>bar</div>
  // baz
  // Imagine moving 'bar' to ^.  'bar' will be deleted and its div pruned.  That
  // would cause 'baz' to collapse onto the line with 'foobar' unless we insert
  // a br. Must recononicalize these two VisiblePositions after the pruning
  // above.
  const VisiblePosition& before_paragraph =
      CreateVisiblePosition(relocatable_before_paragraph->GetPosition());
  const VisiblePosition& after_paragraph =
      CreateVisiblePosition(relocatable_after_paragraph->GetPosition());

  if (before_paragraph.IsNotNull() &&
      !IsDisplayInsideTable(before_paragraph.DeepEquivalent().AnchorNode()) &&
      ((!IsEndOfParagraph(before_paragraph) &&
        !IsStartOfParagraph(before_paragraph)) ||
       before_paragraph.DeepEquivalent() == after_paragraph.DeepEquivalent())) {
    // FIXME: Trim text between beforeParagraph and afterParagraph if they
    // aren't equal.
    InsertNodeAt(MakeGarbageCollected<HTMLBRElement>(GetDocument()),
                 before_paragraph.DeepEquivalent(), editing_state);
  }
}

void CompositeEditCommand::MoveParagraph(
    const VisiblePosition& start_of_paragraph_to_move,
    const VisiblePosition& end_of_paragraph_to_move,
    const VisiblePosition& destination,
    EditingState* editing_state,
    ShouldPreserveSelection should_preserve_selection,
    ShouldPreserveStyle should_preserve_style,
    Node* constraining_ancestor) {
  DCHECK(!GetDocument().NeedsLayoutTreeUpdate());
  DCHECK(IsStartOfParagraph(start_of_paragraph_to_move))
      << start_of_paragraph_to_move;
  DCHECK(IsEndOfParagraph(end_of_paragraph_to_move))
      << end_of_paragraph_to_move;
  MoveParagraphs(start_of_paragraph_to_move, end_of_paragraph_to_move,
                 destination, editing_state, should_preserve_selection,
                 should_preserve_style, constraining_ancestor);
}

void CompositeEditCommand::MoveParagraphs(
    const VisiblePosition& start_of_paragraph_to_move,
    const VisiblePosition& end_of_paragraph_to_move,
    const VisiblePosition& destination,
    EditingState* editing_state,
    ShouldPreserveSelection should_preserve_selection,
    ShouldPreserveStyle should_preserve_style,
    Node* constraining_ancestor) {
  DCHECK(!GetDocument().NeedsLayoutTreeUpdate());
  DCHECK(start_of_paragraph_to_move.IsNotNull());
  DCHECK(end_of_paragraph_to_move.IsNotNull());
  DCHECK(destination.IsNotNull());

  if (start_of_paragraph_to_move.DeepEquivalent() ==
          destination.DeepEquivalent() ||
      start_of_paragraph_to_move.IsNull())
    return;

  // Can't move the range to a destination inside itself.
  if (destination.DeepEquivalent() >=
          start_of_paragraph_to_move.DeepEquivalent() &&
      destination.DeepEquivalent() <=
          end_of_paragraph_to_move.DeepEquivalent()) {
    // Reached by unit test TypingCommandTest.insertLineBreakWithIllFormedHTML
    // and ApplyStyleCommandTest.JustifyRightDetachesDestination
    editing_state->Abort();
    return;
  }

  int start_index = -1;
  int end_index = -1;
  int destination_index = -1;
  if (should_preserve_selection == kPreserveSelection &&
      !EndingSelection().IsNone()) {
    VisiblePosition visible_start = EndingVisibleSelection().VisibleStart();
    VisiblePosition visible_end = EndingVisibleSelection().VisibleEnd();

    bool start_after_paragraph =
        ComparePositions(visible_start, end_of_paragraph_to_move) > 0;
    bool end_before_paragraph =
        ComparePositions(visible_end, start_of_paragraph_to_move) < 0;

    if (!start_after_paragraph && !end_before_paragraph) {
      bool start_in_paragraph =
          ComparePositions(visible_start, start_of_paragraph_to_move) >= 0;
      bool end_in_paragraph =
          ComparePositions(visible_end, end_of_paragraph_to_move) <= 0;

      const TextIteratorBehavior behavior =
          TextIteratorBehavior::AllVisiblePositionsRangeLengthBehavior();

      start_index = 0;
      if (start_in_paragraph) {
        start_index = TextIterator::RangeLength(
            start_of_paragraph_to_move.ToParentAnchoredPosition(),
            visible_start.ToParentAnchoredPosition(), behavior);
      }

      end_index = 0;
      if (end_in_paragraph) {
        end_index = TextIterator::RangeLength(
            start_of_paragraph_to_move.ToParentAnchoredPosition(),
            visible_end.ToParentAnchoredPosition(), behavior);
      }
    }
  }

  RelocatablePosition* before_paragraph_position =
      MakeGarbageCollected<RelocatablePosition>(
          PreviousPositionOf(start_of_paragraph_to_move,
                             kCannotCrossEditingBoundary)
              .DeepEquivalent());
  RelocatablePosition* after_paragraph_position =
      MakeGarbageCollected<RelocatablePosition>(
          NextPositionOf(end_of_paragraph_to_move, kCannotCrossEditingBoundary)
              .DeepEquivalent());

  const Position& start_candidate = start_of_paragraph_to_move.DeepEquivalent();
  const Position& end_candidate = end_of_paragraph_to_move.DeepEquivalent();
  DCHECK_LE(start_candidate, end_candidate);

  // We upstream() the end and downstream() the start so that we don't include
  // collapsed whitespace in the move. When we paste a fragment, spaces after
  // the end and before the start are treated as though they were rendered.
  Position start = MostForwardCaretPosition(start_candidate);
  Position end = MostBackwardCaretPosition(end_candidate);
  if (end < start)
    end = start;

  // FIXME: This is an inefficient way to preserve style on nodes in the
  // paragraph to move. It shouldn't matter though, since moved paragraphs will
  // usually be quite small.
  DocumentFragment* fragment = nullptr;
  if (start_of_paragraph_to_move.DeepEquivalent() !=
      end_of_paragraph_to_move.DeepEquivalent()) {
    const String paragraphs_markup = CreateMarkup(
        start.ParentAnchoredEquivalent(), end.ParentAnchoredEquivalent(),
        CreateMarkupOptions::Builder()
            .SetShouldConvertBlocksToInlines(true)
            .SetConstrainingAncestor(constraining_ancestor)
            .Build());
    fragment = CreateStrictlyProcessedFragmentFromMarkupWithContext(
        GetDocument(), paragraphs_markup, 0, paragraphs_markup.length(), "");
  }

  // A non-empty paragraph's style is moved when we copy and move it.  We don't
  // move anything if we're given an empty paragraph, but an empty paragraph can
  // have style too, <div><b><br></b></div> for example.  Save it so that we can
  // preserve it later.
  EditingStyle* style_in_empty_paragraph = nullptr;
  if (start_of_paragraph_to_move.DeepEquivalent() ==
          end_of_paragraph_to_move.DeepEquivalent() &&
      should_preserve_style == kPreserveStyle) {
    style_in_empty_paragraph = MakeGarbageCollected<EditingStyle>(
        start_of_paragraph_to_move.DeepEquivalent());
    style_in_empty_paragraph->MergeTypingStyle(&GetDocument());
    // The moved paragraph should assume the block style of the destination.
    style_in_empty_paragraph->RemoveBlockProperties(
        GetDocument().GetExecutionContext());
  }

  // FIXME (5098931): We should add a new insert action
  // "WebViewInsertActionMoved" and call shouldInsertFragment here.

  DCHECK(!GetDocument().NeedsLayoutTreeUpdate());

  const VisibleSelection& selection_to_delete = CreateVisibleSelection(
      SelectionInDOMTree::Builder().Collapse(start).Extend(end).Build());
  SetEndingSelection(
      SelectionForUndoStep::From(selection_to_delete.AsSelection()));
  if (!DeleteSelection(
          editing_state,
          DeleteSelectionOptions::Builder().SetSanitizeMarkup(true).Build()))
    return;

  DCHECK(destination.DeepEquivalent().IsConnected()) << destination;
  CleanupAfterDeletion(editing_state, destination);
  if (editing_state->IsAborted())
    return;
  DCHECK(destination.DeepEquivalent().IsConnected()) << destination;

  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kEditing);

  // Add a br if pruning an empty block level element caused a collapse. For
  // example:
  // foo^
  // <div>bar</div>
  // baz
  // Imagine moving 'bar' to ^. 'bar' will be deleted and its div pruned. That
  // would cause 'baz' to collapse onto the line with 'foobar' unless we insert
  // a br. Must recononicalize these two VisiblePositions after the pruning
  // above.
  VisiblePosition before_paragraph =
      CreateVisiblePosition(before_paragraph_position->GetPosition());
  VisiblePosition after_paragraph =
      CreateVisiblePosition(after_paragraph_position->GetPosition());
  if (before_paragraph.IsNotNull() &&
      ((!IsStartOfParagraph(before_paragraph) &&
        !IsEndOfParagraph(before_paragraph)) ||
       before_paragraph.DeepEquivalent() == after_paragraph.DeepEquivalent())) {
    // FIXME: Trim text between beforeParagraph and afterParagraph if they
    // aren't equal.
    InsertNodeAt(MakeGarbageCollected<HTMLBRElement>(GetDocument()),
                 before_paragraph.DeepEquivalent(), editing_state);
    if (editing_state->IsAborted())
      return;
  }

  // TextIterator::rangeLength requires clean layout.
  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kEditing);

  destination_index = TextIterator::RangeLength(
      Position::FirstPositionInNode(*GetDocument().documentElement()),
      destination.ToParentAnchoredPosition(),
      TextIteratorBehavior::AllVisiblePositionsRangeLengthBehavior());

  const VisibleSelection& destination_selection =
      CreateVisibleSelection(SelectionInDOMTree::Builder()
                                 .Collapse(destination.ToPositionWithAffinity())
                                 .Build());
  if (EndingSelection().IsNone()) {
    // We abort executing command since |destination| becomes invisible.
    editing_state->Abort();
    return;
  }
  SetEndingSelection(
      SelectionForUndoStep::From(destination_selection.AsSelection()));
  ReplaceSelectionCommand::CommandOptions options =
      ReplaceSelectionCommand::kSelectReplacement |
      ReplaceSelectionCommand::kMovingParagraph;
  if (should_preserve_style == kDoNotPreserveStyle)
    options |= ReplaceSelectionCommand::kMatchStyle;
  ApplyCommandToComposite(MakeGarbageCollected<ReplaceSelectionCommand>(
                              GetDocument(), fragment, options),
                          editing_state);
  if (editing_state->IsAborted())
    return;
  ABORT_EDITING_COMMAND_IF(!EndingSelection().IsValidFor(GetDocument()));

  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kEditing);

  // If the selection is in an empty paragraph, restore styles from the old
  // empty paragraph to the new empty paragraph.
  bool selection_is_empty_paragraph =
      EndingSelection().IsCaret() &&
      IsStartOfParagraph(EndingVisibleSelection().VisibleStart()) &&
      IsEndOfParagraph(EndingVisibleSelection().VisibleStart());
  if (style_in_empty_paragraph && selection_is_empty_paragraph) {
    ApplyStyle(style_in_empty_paragraph, editing_state);
    if (editing_state->IsAborted())
      return;
  }

  if (should_preserve_selection == kDoNotPreserveSelection || start_index == -1)
    return;
  Element* document_element = GetDocument().documentElement();
  if (!document_element)
    return;

  // We need clean layout in order to compute plain-text ranges below.
  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kEditing);

  // Fragment creation (using createMarkup) incorrectly uses regular spaces
  // instead of nbsps for some spaces that were rendered (11475), which causes
  // spaces to be collapsed during the move operation. This results in a call
  // to rangeFromLocationAndLength with a location past the end of the
  // document (which will return null).
  EphemeralRange start_range = PlainTextRange(destination_index + start_index)
                                   .CreateRangeForSelection(*document_element);
  if (start_range.IsNull())
    return;
  EphemeralRange end_range = PlainTextRange(destination_index + end_index)
                                 .CreateRangeForSelection(*document_element);
  if (end_range.IsNull())
    return;
  const VisibleSelection& visible_selection =
      CreateVisibleSelection(SelectionInDOMTree::Builder()
                                 .Collapse(start_range.StartPosition())
                                 .Extend(end_range.StartPosition())
                                 .Build());
  SetEndingSelection(
      SelectionForUndoStep::From(visible_selection.AsSelection()));
}

// FIXME: Send an appropriate shouldDeleteRange call.
bool CompositeEditCommand::BreakOutOfEmptyListItem(
    EditingState* editing_state) {
  DCHECK(!GetDocument().NeedsLayoutTreeUpdate());
  Node* empty_list_item =
      EnclosingEmptyListItem(EndingVisibleSelection().VisibleStart());
  if (!empty_list_item)
    return false;

  EditingStyle* style =
      MakeGarbageCollected<EditingStyle>(EndingSelection().Start());
  style->MergeTypingStyle(&GetDocument());

  ContainerNode* list_node = empty_list_item->parentNode();
  // FIXME: Can't we do something better when the immediate parent wasn't a list
  // node?
  if (!list_node ||
      (!IsA<HTMLUListElement>(*list_node) &&
       !IsA<HTMLOListElement>(*list_node)) ||
      !IsEditable(*list_node) ||
      list_node == RootEditableElement(*empty_list_item))
    return false;

  HTMLElement* new_block = nullptr;
  if (ContainerNode* block_enclosing_list = list_node->parentNode()) {
    if (block_enclosing_list->HasTagName(
            html_names::kLiTag)) {  // listNode is inside another list item
      if (CreateVisiblePosition(PositionAfterNode(*block_enclosing_list))
              .DeepEquivalent() ==
          CreateVisiblePosition(PositionAfterNode(*list_node))
              .DeepEquivalent()) {
        // If listNode appears at the end of the outer list item, then move
        // listNode outside of this list item, e.g.
        //   <ul><li>hello <ul><li><br></li></ul> </li></ul>
        // should become
        //   <ul><li>hello</li> <ul><li><br></li></ul> </ul>
        // after this section.
        //
        // If listNode does NOT appear at the end, then we should consider it as
        // a regular paragraph, e.g.
        //   <ul><li> <ul><li><br></li></ul> hello</li></ul>
        // should become
        //   <ul><li> <div><br></div> hello</li></ul>
        // at the end
        SplitElement(To<Element>(block_enclosing_list), list_node);
        RemoveNodePreservingChildren(list_node->parentNode(), editing_state);
        if (editing_state->IsAborted())
          return false;
        new_block = MakeGarbageCollected<HTMLLIElement>(GetDocument());
      }
      // If listNode does NOT appear at the end of the outer list item, then
      // behave as if in a regular paragraph.
    } else if (block_enclosing_list->HasTagName(html_names::kOlTag) ||
               block_enclosing_list->HasTagName(html_names::kUlTag)) {
      new_block = MakeGarbageCollected<HTMLLIElement>(GetDocument());
    }
  }
  if (!new_block)
    new_block = CreateDefaultParagraphElement(GetDocument());

  Node* previous_list_node =
      empty_list_item->IsElementNode()
          ? ElementTraversal::PreviousSibling(*empty_list_item)
          : empty_list_item->previousSibling();
  Node* next_list_node = empty_list_item->IsElementNode()
                             ? ElementTraversal::NextSibling(*empty_list_item)
                             : empty_list_item->nextSibling();
  if (next_list_node && IsListElementTag(list_node)) {
    // If emptyListItem follows another list item or nested list, split the list
    // node.
    if (IsListItemTag(previous_list_node) ||
        IsHTMLListElement(previous_list_node)) {
      SplitElement(To<Element>(list_node), empty_list_item);
    }

    // If emptyListItem is followed by other list item or nested list, then
    // insert newBlock before the list node. Because we have split the
    // element, emptyListItem is the first element in the list node.
    // i.e. insert newBlock before ul or ol whose first element is emptyListItem
    InsertNodeBefore(new_block, list_node, editing_state);
    if (editing_state->IsAborted())
      return false;
    RemoveNode(empty_list_item, editing_state);
    if (editing_state->IsAborted())
      return false;
  } else {
    // When emptyListItem does not follow any list item or nested list, insert
    // newBlock after the enclosing list node. Remove the enclosing node if
    // emptyListItem is the only child; otherwise just remove emptyListItem.
    //   <ul>                             <ul>
    //     <li>                             <li>
    //       abc                              abc
    //       <ul>                             <ul>
    //         <li>def</li>                     <li>def</li>
    //         <li>{}<br></li>    ->          </ul>
    //       </ul>                            <div>{}<br></div>
    //       ghi                              ghi
    //     </li>                            </li>
    //   </ul>                            </ul>
    InsertNodeAfter(new_block, list_node, editing_state);
    if (editing_state->IsAborted())
      return false;
    RemoveNode(previous_list_node ? empty_list_item : list_node, editing_state);
    if (editing_state->IsAborted())
      return false;
  }

  AppendBlockPlaceholder(new_block, editing_state);
  if (editing_state->IsAborted())
    return false;

  SetEndingSelection(SelectionForUndoStep::From(
      SelectionInDOMTree::Builder()
          .Collapse(Position::FirstPositionInNode(*new_block))
          .Build()));

  style->PrepareToApplyAt(EndingSelection().Start());
  if (!style->IsEmpty()) {
    ApplyStyle(style, editing_state);
    if (editing_state->IsAborted())
      return false;
  }

  return true;
}

// If the caret is in an empty quoted paragraph, and either there is nothing
// before that paragraph, or what is before is unquoted, and the user presses
// delete, unquote that paragraph.
bool CompositeEditCommand::BreakOutOfEmptyMailBlockquotedParagraph(
    EditingState* editing_state) {
  if (!EndingSelection().IsCaret())
    return false;

  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kEditing);

  VisiblePosition caret = EndingVisibleSelection().VisibleStart();
  auto* highest_blockquote = To<HTMLQuoteElement>(HighestEnclosingNodeOfType(
      caret.DeepEquivalent(), &IsMailHTMLBlockquoteElement));
  if (!highest_blockquote)
    return false;

  if (!IsStartOfParagraph(caret) || !IsEndOfParagraph(caret))
    return false;

  VisiblePosition previous =
      PreviousPositionOf(caret, kCannotCrossEditingBoundary);
  // Only move forward if there's nothing before the caret, or if there's
  // unquoted content before it.
  if (EnclosingNodeOfType(previous.DeepEquivalent(),
                          &IsMailHTMLBlockquoteElement))
    return false;

  auto* br = MakeGarbageCollected<HTMLBRElement>(GetDocument());
  // We want to replace this quoted paragraph with an unquoted one, so insert a
  // br to hold the caret before the highest blockquote.
  InsertNodeBefore(br, highest_blockquote, editing_state);
  if (editing_state->IsAborted())
    return false;

  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kEditing);

  VisiblePosition at_br = VisiblePosition::BeforeNode(*br);
  // If the br we inserted collapsed, for example:
  //   foo<br><blockquote>...</blockquote>
  // insert a second one.
  if (!IsStartOfParagraph(at_br)) {
    InsertNodeBefore(MakeGarbageCollected<HTMLBRElement>(GetDocument()), br,
                     editing_state);
    if (editing_state->IsAborted())
      return false;
    GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kEditing);
  }
  SetEndingSelection(SelectionForUndoStep::From(
      SelectionInDOMTree::Builder()
          .Collapse(at_br.ToPositionWithAffinity())
          .Build()));

  // If this is an empty paragraph there must be a line break here.
  if (!LineBreakExistsAtVisiblePosition(caret))
    return false;

  Position caret_pos(MostForwardCaretPosition(caret.DeepEquivalent()));
  // A line break is either a br or a preserved newline.
  DCHECK(IsA<HTMLBRElement>(caret_pos.AnchorNode()) ||
         (caret_pos.AnchorNode()->IsTextNode() && caret_pos.AnchorNode()
                                                      ->GetLayoutObject()
                                                      ->Style()
                                                      ->ShouldPreserveBreaks()))
      << caret_pos;

  if (IsA<HTMLBRElement>(*caret_pos.AnchorNode())) {
    RemoveNodeAndPruneAncestors(caret_pos.AnchorNode(), editing_state);
    if (editing_state->IsAborted())
      return false;
  } else if (auto* text_node = DynamicTo<Text>(caret_pos.AnchorNode())) {
    DCHECK_EQ(caret_pos.ComputeOffsetInContainerNode(), 0);
    ContainerNode* parent_node = text_node->parentNode();
    // The preserved newline must be the first thing in the node, since
    // otherwise the previous paragraph would be quoted, and we verified that it
    // wasn't above.
    DeleteTextFromNode(text_node, 0, 1);
    Prune(parent_node, editing_state);
    if (editing_state->IsAborted())
      return false;
  }

  return true;
}

// Operations use this function to avoid inserting content into an anchor when
// at the start or the end of that anchor, as in NSTextView.
// FIXME: This is only an approximation of NSTextViews insertion behavior, which
// varies depending on how the caret was made.
Position CompositeEditCommand::PositionAvoidingSpecialElementBoundary(
    const Position& original,
    EditingState* editing_state) {
  if (original.IsNull())
    return original;

  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kEditing);
  VisiblePosition visible_pos = CreateVisiblePosition(original);
  Element* enclosing_anchor = EnclosingAnchorElement(original);
  Position result = original;

  if (!enclosing_anchor)
    return result;

  // Don't avoid block level anchors, because that would insert content into the
  // wrong paragraph.
  if (enclosing_anchor && !IsEnclosingBlock(enclosing_anchor)) {
    VisiblePosition first_in_anchor =
        VisiblePosition::FirstPositionInNode(*enclosing_anchor);
    VisiblePosition last_in_anchor =
        VisiblePosition::LastPositionInNode(*enclosing_anchor);
    // If visually just after the anchor, insert *inside* the anchor unless it's
    // the last VisiblePosition in the document, to match NSTextView.
    if (visible_pos.DeepEquivalent() == last_in_anchor.DeepEquivalent()) {
      // Make sure anchors are pushed down before avoiding them so that we don't
      // also avoid structural elements like lists and blocks (5142012).
      Element* enclosing_block = EnclosingBlock(original.AnchorNode());
      if (enclosing_block &&
          enclosing_block->IsDescendantOf(enclosing_anchor)) {
        // Only push down anchor element if there are block elements inside it.
        PushAnchorElementDown(enclosing_anchor, editing_state);
        if (editing_state->IsAborted())
          return original;
        enclosing_anchor = EnclosingAnchorElement(original);
        if (!enclosing_anchor)
          return original;
      }

      GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kEditing);

      // Don't insert outside an anchor if doing so would skip over a line
      // break.  It would probably be safe to move the line break so that we
      // could still avoid the anchor here.
      Position downstream(
          MostForwardCaretPosition(visible_pos.DeepEquivalent()));
      if (LineBreakExistsAtVisiblePosition(visible_pos) &&
          downstream.AnchorNode()->IsDescendantOf(enclosing_anchor))
        return original;

      result = Position::InParentAfterNode(*enclosing_anchor);
    }

    // If visually just before an anchor, insert *outside* the anchor unless
    // it's the first VisiblePosition in a paragraph, to match NSTextView.
    if (visible_pos.DeepEquivalent() == first_in_anchor.DeepEquivalent()) {
      // Make sure anchors are pushed down before avoiding them so that we don't
      // also avoid structural elements like lists and blocks (5142012).
      Element* enclosing_block = EnclosingBlock(original.AnchorNode());
      if (enclosing_block &&
          enclosing_block->IsDescendantOf(enclosing_anchor)) {
        // Only push down anchor element if there are block elements inside it.
        PushAnchorElementDown(enclosing_anchor, editing_state);
        if (editing_state->IsAborted())
          return original;
        enclosing_anchor = EnclosingAnchorElement(original);
      }
      if (!enclosing_anchor)
        return original;

      result = Position::InParentBeforeNode(*enclosing_anchor);
    }
  }

  if (result.IsNull() || !RootEditableElementOf(result))
    result = original;

  return result;
}

// Splits the tree parent by parent until we reach the specified ancestor. We
// use VisiblePositions to determine if the split is necessary. Returns the last
// split node.
Node* CompositeEditCommand::SplitTreeToNode(Node* start,
                                            Node* end,
                                            bool should_split_ancestor) {
  DCHECK(start);
  DCHECK(end);
  DCHECK_NE(start, end);

  if (should_split_ancestor && end->parentNode())
    end = end->parentNode();
  if (!start->IsDescendantOf(end))
    return end;

  Node* end_node = end;
  Node* node = nullptr;
  for (node = start; node->parentNode() != end_node;
       node = node->parentNode()) {
    Element* parent_element = node->parentElement();
    if (!parent_element)
      break;

    GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kEditing);

    // Do not split a node when doing so introduces an empty node.
    if (node->previousSibling()) {
      const Position& first_in_parent =
          Position::FirstPositionInNode(*parent_element);
      const Position& before_node =
          Position::BeforeNode(*node).ToOffsetInAnchor();
      if (MostBackwardCaretPosition(first_in_parent) !=
          MostBackwardCaretPosition(before_node))
        SplitElement(parent_element, node);
    }
  }

  return node;
}

void CompositeEditCommand::SetStartingSelection(
    const SelectionForUndoStep& selection) {
  for (CompositeEditCommand* command = this;; command = command->Parent()) {
    if (UndoStep* undo_step = command->GetUndoStep()) {
      DCHECK(command->IsTopLevelCommand());
      undo_step->SetStartingSelection(selection);
    }
    command->starting_selection_ = selection;
    if (!command->Parent() || command->Parent()->IsFirstCommand(command))
      break;
  }
}

void CompositeEditCommand::SetEndingSelection(
    const SelectionForUndoStep& selection) {
  for (CompositeEditCommand* command = this; command;
       command = command->Parent()) {
    if (UndoStep* undo_step = command->GetUndoStep()) {
      DCHECK(command->IsTopLevelCommand());
      undo_step->SetEndingSelection(selection);
    }
    command->ending_selection_ = selection;
  }
}

void CompositeEditCommand::SetParent(CompositeEditCommand* parent) {
  EditCommand::SetParent(parent);
  if (!parent)
    return;
  starting_selection_ = parent->ending_selection_;
  ending_selection_ = parent->ending_selection_;
}

// Determines whether a node is inside a range or visibly starts and ends at the
// boundaries of the range. Call this function to determine whether a node is
// visibly fit inside selectedRange
bool CompositeEditCommand::IsNodeVisiblyContainedWithin(
    Node& node,
    const EphemeralRange& selected_range) {
  DCHECK(!NeedsLayoutTreeUpdate(node));
  DocumentLifecycle::DisallowTransitionScope disallow_transition(
      node.GetDocument().Lifecycle());

  if (IsNodeFullyContained(selected_range, node))
    return true;

  bool start_is_visually_same =
      CreateVisiblePosition(PositionBeforeNode(node)).DeepEquivalent() ==
      CreateVisiblePosition(selected_range.StartPosition()).DeepEquivalent();
  if (start_is_visually_same &&
      ComparePositions(Position::InParentAfterNode(node),
                       selected_range.EndPosition()) < 0)
    return true;

  bool end_is_visually_same =
      CreateVisiblePosition(PositionAfterNode(node)).DeepEquivalent() ==
      CreateVisiblePosition(selected_range.EndPosition()).DeepEquivalent();
  if (end_is_visually_same &&
      ComparePositions(selected_range.StartPosition(),
                       Position::InParentBeforeNode(node)) < 0)
    return true;

  return start_is_visually_same && end_is_visually_same;
}

void CompositeEditCommand::Trace(Visitor* visitor) const {
  visitor->Trace(commands_);
  visitor->Trace(starting_selection_);
  visitor->Trace(ending_selection_);
  visitor->Trace(undo_step_);
  EditCommand::Trace(visitor);
}

void CompositeEditCommand::AppliedEditing() {
  DCHECK(!IsCommandGroupWrapper());
  EventQueueScope scope;

  const UndoStep& undo_step = *GetUndoStep();
  DispatchEditableContentChangedEvents(undo_step.StartingRootEditableElement(),
                                       undo_step.EndingRootEditableElement());
  LocalFrame* const frame = GetDocument().GetFrame();
  Editor& editor = frame->GetEditor();
  // TODO(editing-dev): Filter empty InputType after spec is finalized.
  DispatchInputEventEditableContentChanged(
      undo_step.StartingRootEditableElement(),
      undo_step.EndingRootEditableElement(), GetInputType(),
      TextDataForInputEvent(), IsComposingFromCommand(this));

  const SelectionInDOMTree& new_selection =
      CorrectedSelectionAfterCommand(EndingSelection(), &GetDocument());

  // Don't clear the typing style with this selection change. We do those things
  // elsewhere if necessary.
  ChangeSelectionAfterCommand(frame, new_selection,
                              SetSelectionOptions::Builder()
                                  .SetIsDirectional(SelectionIsDirectional())
                                  .Build());

  if (!PreservesTypingStyle())
    editor.ClearTypingStyle();

  CompositeEditCommand* const last_edit_command = editor.LastEditCommand();
  // Command will be equal to last edit command only in the case of typing
  if (last_edit_command == this) {
    DCHECK(IsTypingCommand());
  } else if (last_edit_command && last_edit_command->IsDragAndDropCommand() &&
             (GetInputType() == InputEvent::InputType::kDeleteByDrag ||
              GetInputType() == InputEvent::InputType::kInsertFromDrop)) {
    // Only register undo entry when combined with other commands.
    if (!last_edit_command->GetUndoStep()) {
      editor.GetUndoStack().RegisterUndoStep(
          last_edit_command->EnsureUndoStep());
    }
    last_edit_command->EnsureUndoStep()->SetEndingSelection(
        EnsureUndoStep()->EndingSelection());
    last_edit_command->GetUndoStep()->SetSelectionIsDirectional(
        GetUndoStep()->SelectionIsDirectional());
    editor.GetUndoStack().DidSetEndingSelection(
        last_edit_command->GetUndoStep());
    last_edit_command->AppendCommandToUndoStep(this);
  } else {
    // Only register a new undo command if the command passed in is
    // different from the last command
    editor.SetLastEditCommand(this);
    editor.GetUndoStack().RegisterUndoStep(EnsureUndoStep());
  }

  if (Element* element = undo_step.StartingRootEditableElement()) {
    if (element->GetDocument().IsPageVisible()) {
      element->GetDocument()
          .GetPage()
          ->GetChromeClient()
          .DidUserChangeContentEditableContent(*element);
    }
  }
  editor.RespondToChangedContents(new_selection.Anchor());

  if (auto* rc = GetDocument().GetResourceCoordinator()) {
    rc->SetHadUserEdits();
  }
}

}  // namespace blink
