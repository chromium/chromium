/*
 * Copyright (C) 2005, 2006, 2007, 2008 Apple Inc.  All rights reserved.
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

#include "third_party/blink/renderer/core/editing/commands/typing_command.h"

#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/element_traversal.h"
#include "third_party/blink/renderer/core/dom/events/scoped_event_queue.h"
#include "third_party/blink/renderer/core/editing/commands/break_blockquote_command.h"
#include "third_party/blink/renderer/core/editing/commands/delete_selection_command.h"
#include "third_party/blink/renderer/core/editing/commands/delete_selection_options.h"
#include "third_party/blink/renderer/core/editing/commands/editing_commands_utilities.h"
#include "third_party/blink/renderer/core/editing/commands/insert_incremental_text_command.h"
#include "third_party/blink/renderer/core/editing/commands/insert_line_break_command.h"
#include "third_party/blink/renderer/core/editing/commands/insert_paragraph_separator_command.h"
#include "third_party/blink/renderer/core/editing/commands/insert_text_command.h"
#include "third_party/blink/renderer/core/editing/editing_behavior.h"
#include "third_party/blink/renderer/core/editing/editing_utilities.h"
#include "third_party/blink/renderer/core/editing/editor.h"
#include "third_party/blink/renderer/core/editing/ephemeral_range.h"
#include "third_party/blink/renderer/core/editing/frame_selection.h"
#include "third_party/blink/renderer/core/editing/ime/input_method_controller.h"
#include "third_party/blink/renderer/core/editing/plain_text_range.h"
#include "third_party/blink/renderer/core/editing/selection_modifier.h"
#include "third_party/blink/renderer/core/editing/selection_template.h"
#include "third_party/blink/renderer/core/editing/visible_position.h"
#include "third_party/blink/renderer/core/editing/visible_selection.h"
#include "third_party/blink/renderer/core/editing/visible_units.h"
#include "third_party/blink/renderer/core/events/before_text_inserted_event.h"
#include "third_party/blink/renderer/core/events/text_event.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/html/html_br_element.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

namespace {

bool IsValidDocument(const Document& document) {
  return document.GetFrame() && document.GetFrame()->GetDocument() == &document;
}

String DispatchBeforeTextInsertedEvent(const String& text,
                                       const SelectionInDOMTree& selection,
                                       EditingState* editing_state) {
  // We use SelectionForUndoStep because it is resilient to DOM
  // mutation.
  const SelectionForUndoStep& selection_as_undo_step =
      SelectionForUndoStep::From(selection);
  Node* start_node = selection_as_undo_step.Start().ComputeContainerNode();
  if (!start_node || !RootEditableElement(*start_node))
    return text;

  // Send BeforeTextInsertedEvent. The event handler will update text if
  // necessary.
  const Document& document = start_node->GetDocument();
  auto* evt = MakeGarbageCollected<BeforeTextInsertedEvent>(text);
  RootEditableElement(*start_node)->DefaultEventHandler(*evt);
  if (IsValidDocument(document) && selection_as_undo_step.IsValidFor(document))
    return evt->GetText();
  // editing/inserting/webkitBeforeTextInserted-removes-frame.html
  // and
  // editing/inserting/webkitBeforeTextInserted-disconnects-selection.html
  // reaches here.
  editing_state->Abort();
  return String();
}

DispatchEventResult DispatchTextInputEvent(LocalFrame* frame,
                                           const String& text,
                                           EditingState* editing_state) {
  const Document& document = *frame->GetDocument();
  Element* target = document.FocusedElement();
  if (!target)
    return DispatchEventResult::kCanceledBeforeDispatch;

  // Send TextInputEvent. Unlike BeforeTextInsertedEvent, there is no need to
  // update text for TextInputEvent as it doesn't have the API to modify text.
  TextEvent* event = TextEvent::Create(frame->DomWindow(), text,
                                       kTextEventInputIncrementalInsertion);
  event->SetUnderlyingEvent(nullptr);
  DispatchEventResult result = target->DispatchEvent(*event);
  if (IsValidDocument(document))
    return result;
  // editing/inserting/insert-text-remove-iframe-on-textInput-event.html
  // reaches here.
  editing_state->Abort();
  return result;
}

PlainTextRange GetSelectionOffsets(const SelectionInDOMTree& selection) {
  const EphemeralRange range = selection.ComputeRange();
  if (range.IsNull())
    return PlainTextRange();
  ContainerNode* const editable =
      RootEditableElementOrTreeScopeRootNodeOf(selection.Anchor());
  DCHECK(editable);
  return PlainTextRange::Create(*editable, range);
}

SelectionInDOMTree CreateSelection(const wtf_size_t start,
                                   const wtf_size_t end,
                                   Element* element) {
  const EphemeralRange& start_range =
      PlainTextRange(0, static_cast<int>(start)).CreateRange(*element);
  DCHECK(start_range.IsNotNull());
  const Position& start_position = start_range.EndPosition();

  const EphemeralRange& end_range =
      PlainTextRange(0, static_cast<int>(end)).CreateRange(*element);
  DCHECK(end_range.IsNotNull());
  const Position& end_position = end_range.EndPosition();

  const SelectionInDOMTree& selection =
      SelectionInDOMTree::Builder()
          .SetBaseAndExtent(start_position, end_position)
          .Build();
  return selection;
}

bool CanAppendNewLineFeedToSelection(const SelectionInDOMTree& selection,
                                     EditingState* editing_state) {
  // We use SelectionForUndoStep because it is resilient to DOM
  // mutation.
  const SelectionForUndoStep& selection_as_undo_step =
      SelectionForUndoStep::From(selection);
  Element* element = selection_as_undo_step.RootEditableElement();
  if (!element)
    return false;

  const Document& document = element->GetDocument();
  auto* event = MakeGarbageCollected<BeforeTextInsertedEvent>(String("\n"));
  element->DefaultEventHandler(*event);
  // event may invalidate frame or selection
  if (IsValidDocument(document) && selection_as_undo_step.IsValidFor(document))
    return event->GetText().length();
  // editing/inserting/webkitBeforeTextInserted-removes-frame.html
  // and
  // editing/inserting/webkitBeforeTextInserted-disconnects-selection.html
  // reaches here.
  editing_state->Abort();
  return false;
}

// Example: <div><img style="display:block">|<br></p>
// See "editing/deleting/delete_after_block_image.html"
Position AfterBlockIfBeforeAnonymousPlaceholder(const Position& position) {
  if (!position.IsBeforeAnchor())
    return Position();
  const LayoutObject* const layout_object =
      position.AnchorNode()->GetLayoutObject();
  if (!layout_object || !layout_object->IsBR() ||
      layout_object->NextSibling() || layout_object->PreviousSibling())
    return Position();
  const LayoutObject* const parent = layout_object->Parent();
  if (!parent || !parent->IsAnonymous())
    return Position();
  const LayoutObject* const previous = parent->PreviousSibling();
  if (!previous || !previous->NonPseudoNode())
    return Position();
  return Position::AfterNode(*previous->NonPseudoNode());
}

}  // anonymous namespace

TypingCommand::TypingCommand(Document& document,
                             CommandType command_type,
                             const String& text_to_insert,
                             Options options,
                             TextGranularity granularity,
                             TextCompositionType composition_type)
    : CompositeEditCommand(document),
      command_type_(command_type),
      text_to_insert_(text_to_insert),
      open_for_more_typing_(true),
      select_inserted_text_(options & kSelectInsertedText),
      smart_delete_(options & kSmartDelete),
      granularity_(granularity),
      composition_type_(composition_type),
      kill_ring_(options & kKillRing),
      opened_by_backward_delete_(false) {
  UpdatePreservesTypingStyle(command_type_);
}

void TypingCommand::DeleteSelection(Document& document, Options options) {
  LocalFrame* frame = document.GetFrame();
  DCHECK(frame);

  if (!frame->Selection().ComputeVisibleSelectionInDOMTree().IsRange()) {
    return;
  }

  if (TypingCommand* last_typing_command =
          LastTypingCommandIfStillOpenForTyping(frame)) {
    UpdateSelectionIfDifferentFromCurrentSelection(last_typing_command, frame);

    if (RuntimeEnabledFeatures::
            ResetInputTypeToNoneBeforeCharacterInputEnabled()) {
      last_typing_command->input_type_ = InputEvent::InputType::kNone;
    }
    // InputMethodController uses this function to delete composition
    // selection.  It won't be aborted.
    last_typing_command->DeleteSelection(options & kSmartDelete,
                                         ASSERT_NO_EDITING_ABORT);
    return;
  }

  MakeGarbageCollected<TypingCommand>(document, kDeleteSelection, "", options)
      ->Apply();
}

void TypingCommand::DeleteSelectionIfRange(
    const SelectionForUndoStep& selection,
    EditingState* editing_state) {
  if (!selection.IsRange())
    return;
  // Although the 'selection' to delete is indeed a Range, it may have been
  // built from a Caret selection; in that case we don't want to expand so that
  // the table structure is deleted as well.
  bool expand_for_special = EndingSelection().IsRange();
  ApplyCommandToComposite(
      MakeGarbageCollected<DeleteSelectionCommand>(
          selection, DeleteSelectionOptions::Builder()
                         .SetSmartDelete(smart_delete_)
                         .SetMergeBlocksAfterDelete(true)
                         .SetExpandForSpecialElements(expand_for_special)
                         .SetSanitizeMarkup(true)
                         .Build()),
      editing_state);
}

void TypingCommand::DeleteKeyPressed(Document& document,
                                     Options options,
                                     TextGranularity granularity) {
  if (granularity == TextGranularity::kCharacter) {
    LocalFrame* frame = document.GetFrame();
    if (TypingCommand* last_typing_command =
            LastTypingCommandIfStillOpenForTyping(frame)) {
      // If the last typing command is not Delete, open a new typing command.
      // We need to group continuous delete commands alone in a single typing
      // command.
      if (last_typing_command->CommandTypeOfOpenCommand() == kDeleteKey) {
        UpdateSelectionIfDifferentFromCurrentSelection(last_typing_command,
                                                       frame);
        EditingState editing_state;
        if (RuntimeEnabledFeatures::
                ResetInputTypeToNoneBeforeCharacterInputEnabled()) {
          last_typing_command->input_type_ = InputEvent::InputType::kNone;
        }
        last_typing_command->DeleteKeyPressed(granularity, options & kKillRing,
                                              &editing_state);
        return;
      }
    }
  }

  MakeGarbageCollected<TypingCommand>(document, kDeleteKey, "", options,
                                      granularity)
      ->Apply();
}

void TypingCommand::ForwardDeleteKeyPressed(Document& document,
                                            EditingState* editing_state,
                                            Options options,
                                            TextGranularity granularity) {
  // FIXME: Forward delete in TextEdit appears to open and close a new typing
  // command.
  if (granularity == TextGranularity::kCharacter) {
    LocalFrame* frame = document.GetFrame();
    if (TypingCommand* last_typing_command =
            LastTypingCommandIfStillOpenForTyping(frame)) {
      UpdateSelectionIfDifferentFromCurrentSelection(last_typing_command,
                                                     frame);
      // Reset the 'input_type_' to default value. The actual 'input_type_' will
      // be determined later in TypingCommand::GetInputType() based on the
      // 'command_type_'
      last_typing_command->input_type_ = InputEvent::InputType::kNone;
      last_typing_command->ForwardDeleteKeyPressed(
          granularity, options & kKillRing, editing_state);
      return;
    }
  }

  MakeGarbageCollected<TypingCommand>(document, kForwardDeleteKey, "", options,
                                      granularity)
      ->Apply();
}

String TypingCommand::TextDataForInputEvent() const {
  if (commands_.empty() || IsIncrementalInsertion())
    return text_to_insert_;
  return commands_.back()->TextDataForInputEvent();
}

void TypingCommand::UpdateSelectionIfDifferentFromCurrentSelection(
    TypingCommand* typing_command,
    LocalFrame* frame) {
  DCHECK(frame);
  const SelectionInDOMTree& current_selection =
      frame->Selection().GetSelectionInDOMTree();
  if (current_selection == typing_command->EndingSelection().AsSelection())
    return;

  typing_command->SetStartingSelection(
      SelectionForUndoStep::From(current_selection));
  typing_command->SetEndingSelection(
      SelectionForUndoStep::From(current_selection));
}

void TypingCommand::InsertText(Document& document,
                               const String& text,
                               Options options,
                               TextCompositionType composition,
                               const bool is_incremental_insertion) {
  LocalFrame* frame = document.GetFrame();
  DCHECK(frame);
  EditingState editing_state;
  InsertText(document, text, frame->Selection().GetSelectionInDOMTree(),
             options, &editing_state, composition, is_incremental_insertion);
}

void TypingCommand::AdjustSelectionAfterIncrementalInsertion(
    LocalFrame* frame,
    const wtf_size_t selection_start,
    const wtf_size_t text_length,
    EditingState* editing_state) {
  if (!IsIncrementalInsertion())
    return;

  // TODO(editing-dev): The use of UpdateStyleAndLayout
  // needs to be audited. see http://crbug.com/590369 for more details.
  frame->GetDocument()->UpdateStyleAndLayout(DocumentUpdateReason::kEditing);

  Element* element = frame->Selection()
                         .ComputeVisibleSelectionInDOMTree()
                         .RootEditableElement();

  // TODO(editing-dev): The text insertion should probably always leave the
  // selection in an editable region, but we know of at least one case where it
  // doesn't (see test case in crbug.com/767599). Return early in this case to
  // avoid a crash.
  if (!element) {
    editing_state->Abort();
    return;
  }

  const wtf_size_t new_end = selection_start + text_length;
  const SelectionInDOMTree& selection =
      CreateSelection(new_end, new_end, element);
  SetEndingSelection(SelectionForUndoStep::From(selection));
}

// FIXME: We shouldn't need to take selectionForInsertion. It should be
// identical to FrameSelection's current selection.
void TypingCommand::InsertText(
    Document& document,
    const String& text,
    const SelectionInDOMTree& passed_selection_for_insertion,
    Options options,
    EditingState* editing_state,
    TextCompositionType composition_type,
    const bool is_incremental_insertion,
    InputEvent::InputType input_type) {
  DCHECK(!document.NeedsLayoutTreeUpdate());
  LocalFrame* frame = document.GetFrame();
  DCHECK(frame);

  // We use SelectionForUndoStep because it is resilient to DOM
  // mutation.
  const SelectionForUndoStep& passed_selection_for_insertion_as_undo_step =
      SelectionForUndoStep::From(passed_selection_for_insertion);

  String new_text = text;
  if (composition_type != kTextCompositionUpdate) {
    new_text = DispatchBeforeTextInsertedEvent(
        text, passed_selection_for_insertion, editing_state);
    if (editing_state->IsAborted())
      return;
    ABORT_EDITING_COMMAND_IF(
        !passed_selection_for_insertion_as_undo_step.IsValidFor(document));
  }

  if (composition_type == kTextCompositionConfirm) {
    if (DispatchTextInputEvent(frame, new_text, editing_state) !=
        DispatchEventResult::kNotCanceled)
      return;
    // event handler might destroy document.
    if (editing_state->IsAborted())
      return;
    // editing/inserting/insert-text-nodes-disconnect-on-textinput-event.html
    // hits true for ABORT_EDITING_COMMAND_IF macro.
    ABORT_EDITING_COMMAND_IF(
        !passed_selection_for_insertion_as_undo_step.IsValidFor(document));
  }

  // Do nothing if no need to delete and insert.
  if (passed_selection_for_insertion_as_undo_step.IsCaret() && new_text.empty())
    return;

  // TODO(editing-dev): The use of UpdateStyleAndLayout
  // needs to be audited. see http://crbug.com/590369 for more details.
  document.UpdateStyleAndLayout(DocumentUpdateReason::kEditing);

  const PlainTextRange selection_offsets = GetSelectionOffsets(
      passed_selection_for_insertion_as_undo_step.AsSelection());
  if (selection_offsets.IsNull())
    return;
  const wtf_size_t selection_start = selection_offsets.Start();

  frame->GetEditor().NotifyAccessibilityOfDeletionOrInsertionInTextField(
      passed_selection_for_insertion_as_undo_step, /* is_deletion*/ false);

  // Set the starting and ending selection appropriately if we are using a
  // selection that is different from the current selection.  In the future, we
  // should change EditCommand to deal with custom selections in a general way
  // that can be used by all of the commands.
  if (TypingCommand* last_typing_command =
          LastTypingCommandIfStillOpenForTyping(frame)) {
    if (last_typing_command->EndingSelection() !=
        passed_selection_for_insertion_as_undo_step) {
      last_typing_command->SetStartingSelection(
          passed_selection_for_insertion_as_undo_step);
      last_typing_command->SetEndingSelection(
          passed_selection_for_insertion_as_undo_step);
    }

    last_typing_command->SetCompositionType(composition_type);
    last_typing_command->is_incremental_insertion_ = is_incremental_insertion;
    last_typing_command->selection_start_ = selection_start;
    last_typing_command->input_type_ = input_type;

    EventQueueScope event_queue_scope;
    last_typing_command->InsertTextInternal(
        new_text, options & kSelectInsertedText, editing_state);
    return;
  }

  TypingCommand* command = MakeGarbageCollected<TypingCommand>(
      document, kInsertText, new_text, options, TextGranularity::kCharacter,
      composition_type);
  const SelectionInDOMTree& current_selection =
      frame->Selection().GetSelectionInDOMTree();
  bool change_selection =
      current_selection !=
      passed_selection_for_insertion_as_undo_step.AsSelection();
  if (change_selection) {
    command->SetStartingSelection(passed_selection_for_insertion_as_undo_step);
    command->SetEndingSelection(passed_selection_for_insertion_as_undo_step);
  }
  command->is_incremental_insertion_ = is_incremental_insertion;
  command->selection_start_ = selection_start;
  command->input_type_ = input_type;
  ABORT_EDITING_COMMAND_IF(!command->Apply());

  if (change_selection) {
    const SelectionInDOMTree& current_selection_as_dom =
        frame->Selection().GetSelectionInDOMTree();
    command->SetEndingSelection(
        SelectionForUndoStep::From(current_selection_as_dom));
    frame->Selection().SetSelection(
        current_selection_as_dom,
        SetSelectionOptions::Builder()
            .SetIsDirectional(frame->Selection().IsDirectional())
            .Build());
  }
}

bool TypingCommand::InsertLineBreak(Document& document) {
  if (TypingCommand* last_typing_command =
          LastTypingCommandIfStillOpenForTyping(document.GetFrame())) {
    EditingState editing_state;
    EventQueueScope event_queue_scope;
    if (RuntimeEnabledFeatures::
            ResetInputTypeToNoneBeforeCharacterInputEnabled()) {
      last_typing_command->input_type_ = InputEvent::InputType::kNone;
    }
    last_typing_command->InsertLineBreak(&editing_state);
    return !editing_state.IsAborted();
  }

  return MakeGarbageCollected<TypingCommand>(document, kInsertLineBreak, "", 0)
      ->Apply();
}

bool TypingCommand::InsertParagraphSeparatorInQuotedContent(
    Document& document) {
  if (TypingCommand* last_typing_command =
          LastTypingCommandIfStillOpenForTyping(document.GetFrame())) {
    EditingState editing_state;
    EventQueueScope event_queue_scope;
    if (RuntimeEnabledFeatures::
            ResetInputTypeToNoneBeforeCharacterInputEnabled()) {
      last_typing_command->input_type_ = InputEvent::InputType::kNone;
    }
    last_typing_command->InsertParagraphSeparatorInQuotedContent(
        &editing_state);
    return !editing_state.IsAborted();
  }

  return MakeGarbageCollected<TypingCommand>(
             document, kInsertParagraphSeparatorInQuotedContent)
      ->Apply();
}

bool TypingCommand::InsertParagraphSeparator(Document& document) {
  if (TypingCommand* last_typing_command =
          LastTypingCommandIfStillOpenForTyping(document.GetFrame())) {
    EditingState editing_state;
    EventQueueScope event_queue_scope;
    if (RuntimeEnabledFeatures::
            ResetInputTypeToNoneBeforeCharacterInputEnabled()) {
      last_typing_command->input_type_ = InputEvent::InputType::kNone;
    }
    last_typing_command->InsertParagraphSeparator(&editing_state);
    return !editing_state.IsAborted();
  }

  return MakeGarbageCollected<TypingCommand>(document,
                                             kInsertParagraphSeparator, "", 0)
      ->Apply();
}

TypingCommand* TypingCommand::LastTypingCommandIfStillOpenForTyping(
    LocalFrame* frame) {
  DCHECK(frame);

  CompositeEditCommand* last_edit_command =
      frame->GetEditor().LastEditCommand();
  if (!last_edit_command || !last_edit_command->IsTypingCommand() ||
      !static_cast<TypingCommand*>(last_edit_command)->IsOpenForMoreTyping())
    return nullptr;

  return static_cast<TypingCommand*>(last_edit_command);
}

void TypingCommand::CloseTyping(LocalFrame* frame) {
  if (TypingCommand* last_typing_command =
          LastTypingCommandIfStillOpenForTyping(frame))
    last_typing_command->CloseTyping();
}

void TypingCommand::CloseTypingIfNeeded(LocalFrame* frame) {
  if (frame->GetDocument()->IsRunningExecCommand() ||
      frame->GetInputMethodController().HasComposition())
    return;
  if (TypingCommand* last_typing_command =
          LastTypingCommandIfStillOpenForTyping(frame))
    last_typing_command->CloseTyping();
}

void TypingCommand::DoApply(EditingState* editing_state) {
  if (EndingSelection().IsNone() ||
      !EndingSelection().IsValidFor(GetDocument()))
    return;

  if (command_type_ == kDeleteKey) {
    if (commands_.empty())
      opened_by_backward_delete_ = true;
  }

  switch (command_type_) {
    case kDeleteSelection:
      DeleteSelection(smart_delete_, editing_state);
      return;
    case kDeleteKey:
      DeleteKeyPressed(granularity_, kill_ring_, editing_state);
      return;
    case kForwardDeleteKey:
      ForwardDeleteKeyPressed(granularity_, kill_ring_, editing_state);
      return;
    case kInsertLineBreak:
      InsertLineBreak(editing_state);
      return;
    case kInsertParagraphSeparator:
      InsertParagraphSeparator(editing_state);
      return;
    case kInsertParagraphSeparatorInQuotedContent:
      InsertParagraphSeparatorInQuotedContent(editing_state);
      return;
    case kInsertText:
      InsertTextInternal(text_to_insert_, select_inserted_text_, editing_state);
      return;
  }

  NOTREACHED_IN_MIGRATION();
}

InputEvent::InputType TypingCommand::GetInputType() const {
  using InputType = InputEvent::InputType;

  if (composition_type_ != kTextCompositionNone)
    return InputType::kInsertCompositionText;

  if (input_type_ != InputType::kNone)
    return input_type_;

  switch (command_type_) {
    // TODO(editing-dev): |DeleteSelection| is used by IME but we don't have
    // direction info.
    case kDeleteSelection:
      return InputType::kDeleteContentBackward;
    case kDeleteKey:
      return DeletionInputTypeFromTextGranularity(DeleteDirection::kBackward,
                                                  granularity_);
    case kForwardDeleteKey:
      return DeletionInputTypeFromTextGranularity(DeleteDirection::kForward,
                                                  granularity_);
    case kInsertText:
      return InputType::kInsertText;
    case kInsertLineBreak:
      return InputType::kInsertLineBreak;
    case kInsertParagraphSeparator:
    case kInsertParagraphSeparatorInQuotedContent:
      return InputType::kInsertParagraph;
    default:
      return InputType::kNone;
  }
}

void TypingCommand::TypingAddedToOpenCommand(
    CommandType command_type_for_added_typing) {
  LocalFrame* frame = GetDocument().GetFrame();
  if (!frame)
    return;

  UpdatePreservesTypingStyle(command_type_for_added_typing);
  UpdateCommandTypeOfOpenCommand(command_type_for_added_typing);

  AppliedEditing();
}

void TypingCommand::InsertTextInternal(const String& text,
                                       bool select_inserted_text,
                                       EditingState* editing_state) {
  text_to_insert_ = text;

  if (text.empty()) {
    InsertTextRunWithoutNewlines(text, editing_state);
    return;
  }
  wtf_size_t selection_start = selection_start_;
  unsigned offset = 0;
  wtf_size_t newline;
  while ((newline = text.find('\n', offset)) != kNotFound) {
    if (newline > offset) {
      const wtf_size_t insertion_length = newline - offset;
      InsertTextRunWithoutNewlines(text.Substring(offset, insertion_length),
                                   editing_state);
      if (editing_state->IsAborted())
        return;

      AdjustSelectionAfterIncrementalInsertion(GetDocument().GetFrame(),
                                               selection_start,
                                               insertion_length, editing_state);
      selection_start += insertion_length;
    }

    InsertParagraphSeparator(editing_state);
    if (editing_state->IsAborted())
      return;

    offset = newline + 1;
    ++selection_start;
  }

  if (text.length() > offset) {
    const wtf_size_t insertion_length = text.length() - offset;
    InsertTextRunWithoutNewlines(text.Substring(offset, insertion_length),
                                 editing_state);
    if (editing_state->IsAborted())
      return;

    AdjustSelectionAfterIncrementalInsertion(GetDocument().GetFrame(),
                                             selection_start, insertion_length,
                                             editing_state);
  }

  if (!select_inserted_text)
    return;

  // If the caller wants the newly-inserted text to be selected, we select from
  // the plain text offset corresponding to the beginning of the range (possibly
  // collapsed) being replaced by the text insert, to wherever the selection was
  // left after the final run of text was inserted.
  ContainerNode* const editable =
      RootEditableElementOrTreeScopeRootNodeOf(EndingSelection().Anchor());

  const EphemeralRange new_selection_start_collapsed_range =
      PlainTextRange(selection_start_, selection_start_).CreateRange(*editable);
  const Position current_selection_end = EndingSelection().End();

  const SelectionInDOMTree& new_selection =
      SelectionInDOMTree::Builder()
          .SetBaseAndExtent(new_selection_start_collapsed_range.StartPosition(),
                            current_selection_end)
          .Build();

  SetEndingSelection(SelectionForUndoStep::From(new_selection));
}

void TypingCommand::InsertTextRunWithoutNewlines(const String& text,
                                                 EditingState* editing_state) {
  CompositeEditCommand* command;
  if (IsIncrementalInsertion()) {
    command = MakeGarbageCollected<InsertIncrementalTextCommand>(
        GetDocument(), text,
        composition_type_ == kTextCompositionNone
            ? InsertIncrementalTextCommand::
                  kRebalanceLeadingAndTrailingWhitespaces
            : InsertIncrementalTextCommand::kRebalanceAllWhitespaces);
  } else {
    command = MakeGarbageCollected<InsertTextCommand>(
        GetDocument(), text,
        composition_type_ == kTextCompositionNone
            ? InsertTextCommand::kRebalanceLeadingAndTrailingWhitespaces
            : InsertTextCommand::kRebalanceAllWhitespaces);
  }

  command->SetStartingSelection(EndingSelection());
  command->SetEndingSelection(EndingSelection());
  ApplyCommandToComposite(command, editing_state);
  if (editing_state->IsAborted())
    return;

  TypingAddedToOpenCommand(kInsertText);
}

void TypingCommand::InsertLineBreak(EditingState* editing_state) {
  if (!CanAppendNewLineFeedToSelection(EndingSelection().AsSelection(),
                                       editing_state))
    return;

  ApplyCommandToComposite(
      MakeGarbageCollected<InsertLineBreakCommand>(GetDocument()),
      editing_state);
  if (editing_state->IsAborted())
    return;
  TypingAddedToOpenCommand(kInsertLineBreak);
}

void TypingCommand::InsertParagraphSeparator(EditingState* editing_state) {
  if (!CanAppendNewLineFeedToSelection(EndingSelection().AsSelection(),
                                       editing_state))
    return;

  ApplyCommandToComposite(
      MakeGarbageCollected<InsertParagraphSeparatorCommand>(GetDocument()),
      editing_state);
  if (editing_state->IsAborted())
    return;
  TypingAddedToOpenCommand(kInsertParagraphSeparator);
}

void TypingCommand::InsertParagraphSeparatorInQuotedContent(
    EditingState* editing_state) {
  // If the selection starts inside a table, just insert the paragraph separator
  // normally Breaking the blockquote would also break apart the table, which is
  // unecessary when inserting a newline
  if (EnclosingNodeOfType(EndingSelection().Start(), &IsTableStructureNode)) {
    InsertParagraphSeparator(editing_state);
    return;
  }

  ApplyCommandToComposite(
      MakeGarbageCollected<BreakBlockquoteCommand>(GetDocument()),
      editing_state);
  if (editing_state->IsAborted())
    return;
  TypingAddedToOpenCommand(kInsertParagraphSeparatorInQuotedContent);
}

bool TypingCommand::MakeEditableRootEmpty(EditingState* editing_state) {
  DCHECK(!GetDocument().NeedsLayoutTreeUpdate());
  Element* root = RootEditableElementOf(EndingSelection().Anchor());
  if (!root || !root->HasChildren())
    return false;

  if (root->firstChild() == root->lastChild()) {
    if (IsA<HTMLBRElement>(root->firstChild())) {
      // If there is a single child and it could be a placeholder, leave it
      // alone.
      if (root->GetLayoutObject() &&
          root->GetLayoutObject()->IsLayoutBlockFlow())
        return false;
    }
  }

  // The selection is updated prior to the removal of the element
  // that makes the node empty. (see crbug.com/40876506)
  if (RuntimeEnabledFeatures::
          HandleSelectionChangeOnDeletingEmptyElementEnabled()) {
    LocalFrame* const frame = GetDocument().GetFrame();
    const SelectionInDOMTree& new_selection =
        SelectionInDOMTree::Builder()
            .Collapse(Position::FirstPositionInNode(*root))
            .Build();
    frame->Selection().SetSelection(
        new_selection, SetSelectionOptions::Builder()
                           .SetIsDirectional(SelectionIsDirectional())
                           .Build());
    SetEndingSelection(SelectionForUndoStep::From(new_selection));
  }

  RemoveAllChildrenIfPossible(root, editing_state);
  if (editing_state->IsAborted() || root->firstChild())
    return false;

  AddBlockPlaceholderIfNeeded(root, editing_state);
  if (editing_state->IsAborted())
    return false;

  // If the feature to handle selection change on deleting an empty element is
  // not enabled, manually set the ending selection. Otherwise, the selection is
  // already handled by the feature.
  if (!(RuntimeEnabledFeatures::
            HandleSelectionChangeOnDeletingEmptyElementEnabled())) {
    const SelectionInDOMTree& selection =
        SelectionInDOMTree::Builder()
            .Collapse(Position::FirstPositionInNode(*root))
            .Build();
    SetEndingSelection(SelectionForUndoStep::From(selection));
  }

  return true;
}

// If there are multiple Unicode code points to be deleted, adjust the
// range to match platform conventions.
static SelectionForUndoStep AdjustSelectionForBackwardDelete(
    const SelectionInDOMTree& selection) {
  const Position& anchor = selection.Anchor();
  if (selection.IsCaret()) {
    // TODO(yosin): We should make |DeleteSelectionCommand| to work with
    // anonymous placeholder.
    if (Position after_block = AfterBlockIfBeforeAnonymousPlaceholder(anchor)) {
      // We remove a anonymous placeholder <br> in <div> like <div><br></div>:
      //   <div><img style="display:block"><br></div>
      //   |selection_to_delete| is Before:<br>
      // as
      //   <div><img style="display:block"><div><br></div></div>.
      //   |selection_to_delete| is <div>@0, After:<img>
      // See "editing/deleting/delete_after_block_image.html"
      return SelectionForUndoStep::Builder()
          .SetAnchorAndFocusAsBackwardSelection(anchor, after_block)
          .Build();
    }
    return SelectionForUndoStep::From(selection);
  }
  if (anchor.ComputeContainerNode() !=
      selection.Focus().ComputeContainerNode()) {
    return SelectionForUndoStep::From(selection);
  }
  if (anchor.ComputeOffsetInContainerNode() -
          selection.Focus().ComputeOffsetInContainerNode() <=
      1) {
    return SelectionForUndoStep::From(selection);
  }
  const Position& end = selection.ComputeEndPosition();
  return SelectionForUndoStep::Builder()
      .SetAnchorAndFocusAsBackwardSelection(
          end, PreviousPositionOf(end, PositionMoveType::kBackwardDeletion))
      .Build();
}

void TypingCommand::DeleteKeyPressed(TextGranularity granularity,
                                     bool kill_ring,
                                     EditingState* editing_state) {
  LocalFrame* frame = GetDocument().GetFrame();
  if (!frame)
    return;

  if (EndingSelection().IsRange()) {
    DeleteKeyPressedInternal(EndingSelection(), EndingSelection(), kill_ring,
                             editing_state);
    return;
  }

  if (!EndingSelection().IsCaret()) {
    NOTREACHED_IN_MIGRATION();
    return;
  }

  // After breaking out of an empty mail blockquote, we still want continue
  // with the deletion so actual content will get deleted, and not just the
  // quote style.
  const bool break_out_result =
      BreakOutOfEmptyMailBlockquotedParagraph(editing_state);
  if (editing_state->IsAborted())
    return;
  if (break_out_result)
    TypingAddedToOpenCommand(kDeleteKey);

  smart_delete_ = false;
  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kEditing);

  SelectionModifier selection_modifier(*frame, EndingSelection().AsSelection());
  selection_modifier.SetSelectionIsDirectional(SelectionIsDirectional());
  selection_modifier.Modify(SelectionModifyAlteration::kExtend,
                            SelectionModifyDirection::kBackward, granularity);
  if (kill_ring && selection_modifier.Selection().IsCaret() &&
      granularity != TextGranularity::kCharacter) {
    selection_modifier.Modify(SelectionModifyAlteration::kExtend,
                              SelectionModifyDirection::kBackward,
                              TextGranularity::kCharacter);
  }

  const VisiblePosition& visible_start(EndingVisibleSelection().VisibleStart());
  const VisiblePosition& previous_position =
      PreviousPositionOf(visible_start, kCannotCrossEditingBoundary);
  const Node* enclosing_table_cell =
      EnclosingNodeOfType(visible_start.DeepEquivalent(), &IsTableCell);
  const Node* enclosing_table_cell_for_previous_position =
      EnclosingNodeOfType(previous_position.DeepEquivalent(), &IsTableCell);
  if (previous_position.IsNull() ||
      enclosing_table_cell != enclosing_table_cell_for_previous_position) {
    // When the caret is at the start of the editable area, or cell, in an
    // empty list item, break out of the list item.
    const bool break_out_of_empty_list_item_result =
        BreakOutOfEmptyListItem(editing_state);
    if (editing_state->IsAborted())
      return;
    if (break_out_of_empty_list_item_result) {
      TypingAddedToOpenCommand(kDeleteKey);
      return;
    }
  }
  if (previous_position.IsNull()) {
    // When there are no visible positions in the editing root, delete its
    // entire contents.
    if (NextPositionOf(visible_start, kCannotCrossEditingBoundary).IsNull() &&
        MakeEditableRootEmpty(editing_state)) {
      TypingAddedToOpenCommand(kDeleteKey);
      return;
    }
    if (editing_state->IsAborted())
      return;
  }

  // If we have a caret selection at the beginning of a cell, we have
  // nothing to do.
  if (enclosing_table_cell && visible_start.DeepEquivalent() ==
                                  VisiblePosition::FirstPositionInNode(
                                      *const_cast<Node*>(enclosing_table_cell))
                                      .DeepEquivalent())
    return;

  // If the caret is at the start of a paragraph after a table, move content
  // into the last table cell (this is done to follows macOS' behavior).
  if (frame->GetEditor().Behavior().ShouldMergeContentWithTablesOnBackspace() &&
      IsStartOfParagraph(visible_start) &&
      TableElementJustBefore(
          PreviousPositionOf(visible_start, kCannotCrossEditingBoundary))) {
    // Unless the caret is just before a table.  We don't want to move a
    // table into the last table cell.
    if (TableElementJustAfter(visible_start))
      return;
    // Extend the selection backward into the last cell, then deletion will
    // handle the move.
    selection_modifier.Modify(SelectionModifyAlteration::kExtend,
                              SelectionModifyDirection::kBackward, granularity);
    // If the caret is just after a table, select the table and don't delete
    // anything.
  } else if (Element* table = TableElementJustBefore(visible_start)) {
    const SelectionInDOMTree& selection =
        SelectionInDOMTree::Builder()
            .Collapse(Position::BeforeNode(*table))
            .Extend(EndingSelection().Start())
            .Build();
    SetEndingSelection(SelectionForUndoStep::From(selection));
    TypingAddedToOpenCommand(kDeleteKey);
    return;
  }

  const SelectionForUndoStep& selection_to_delete =
      granularity == TextGranularity::kCharacter
          ? AdjustSelectionForBackwardDelete(
                selection_modifier.Selection().AsSelection())
          : SelectionForUndoStep::From(
                selection_modifier.Selection().AsSelection());

  if (!StartingSelection().IsRange() ||
      selection_to_delete.Anchor() != StartingSelection().Start()) {
    DeleteKeyPressedInternal(selection_to_delete, selection_to_delete,
                             kill_ring, editing_state);
    return;
  }
  // Note: |StartingSelection().End()| can be disconnected.
  // See editing/deleting/delete_list_item.html on MacOS.
  const SelectionForUndoStep selection_after_undo =
      SelectionForUndoStep::Builder()
          .SetAnchorAndFocusAsBackwardSelection(
              StartingSelection().End(),
              CreateVisiblePosition(selection_to_delete.Focus())
                  .DeepEquivalent())
          .Build();
  DeleteKeyPressedInternal(selection_to_delete, selection_after_undo, kill_ring,
                           editing_state);
}

void TypingCommand::DeleteKeyPressedInternal(
    const SelectionForUndoStep& selection_to_delete,
    const SelectionForUndoStep& selection_after_undo,
    bool kill_ring,
    EditingState* editing_state) {
  DCHECK(!selection_to_delete.IsNone());
  if (selection_to_delete.IsNone())
    return;

  if (selection_to_delete.IsCaret())
    return;

  LocalFrame* frame = GetDocument().GetFrame();
  DCHECK(frame);

  if (kill_ring) {
    frame->GetEditor().AddToKillRing(CreateVisibleSelection(selection_to_delete)
                                         .ToNormalizedEphemeralRange());
  }
  // On Mac, make undo select everything that has been deleted, unless an undo
  // will undo more than just this deletion.
  // FIXME: This behaves like TextEdit except for the case where you open with
  // text insertion and then delete more text than you insert.  In that case all
  // of the text that was around originally should be selected.
  if (frame->GetEditor().Behavior().ShouldUndoOfDeleteSelectText() &&
      opened_by_backward_delete_)
    SetStartingSelection(selection_after_undo);
  frame->GetEditor().NotifyAccessibilityOfDeletionOrInsertionInTextField(
      selection_to_delete, /* is_deletion */ true);
  DeleteSelectionIfRange(selection_to_delete, editing_state);
  if (editing_state->IsAborted())
    return;
  SetSmartDelete(false);
  TypingAddedToOpenCommand(kDeleteKey);
}

static Position ComputeExtentForForwardDeleteUndo(
    const VisibleSelection& selection,
    const Position& extent) {
  if (extent.ComputeContainerNode() != selection.End().ComputeContainerNode())
    return selection.Focus();
  const int extra_characters =
      selection.Start().ComputeContainerNode() ==
              selection.End().ComputeContainerNode()
          ? selection.End().ComputeOffsetInContainerNode() -
                selection.Start().ComputeOffsetInContainerNode()
          : selection.End().ComputeOffsetInContainerNode();
  return Position::CreateWithoutValidation(
      *extent.ComputeContainerNode(),
      extent.ComputeOffsetInContainerNode() + extra_characters);
}

void TypingCommand::ForwardDeleteKeyPressed(TextGranularity granularity,
                                            bool kill_ring,
                                            EditingState* editing_state) {
  LocalFrame* frame = GetDocument().GetFrame();
  if (!frame)
    return;

  if (EndingSelection().IsRange()) {
    ForwardDeleteKeyPressedInternal(EndingSelection(), EndingSelection(),
                                    kill_ring, editing_state);
    return;
  }

  if (!EndingSelection().IsCaret()) {
    NOTREACHED_IN_MIGRATION();
    return;
  }

  smart_delete_ = false;
  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kEditing);

  // Handle delete at beginning-of-block case.
  // Do nothing in the case that the caret is at the start of a
  // root editable element or at the start of a document.
  SelectionModifier selection_modifier(*frame, EndingSelection().AsSelection());
  selection_modifier.SetSelectionIsDirectional(SelectionIsDirectional());
  selection_modifier.Modify(SelectionModifyAlteration::kExtend,
                            SelectionModifyDirection::kForward, granularity);
  if (kill_ring && selection_modifier.Selection().IsCaret() &&
      granularity != TextGranularity::kCharacter) {
    selection_modifier.Modify(SelectionModifyAlteration::kExtend,
                              SelectionModifyDirection::kForward,
                              TextGranularity::kCharacter);
  }

  Position downstream_end = MostForwardCaretPosition(EndingSelection().End());
  VisiblePosition visible_end = EndingVisibleSelection().VisibleEnd();
  Node* enclosing_table_cell =
      EnclosingNodeOfType(visible_end.DeepEquivalent(), &IsTableCell);
  if (enclosing_table_cell &&
      visible_end.DeepEquivalent() ==
          VisiblePosition::LastPositionInNode(*enclosing_table_cell)
              .DeepEquivalent())
    return;
  if (visible_end.DeepEquivalent() ==
      EndOfParagraph(visible_end).DeepEquivalent()) {
    downstream_end = MostForwardCaretPosition(
        NextPositionOf(visible_end, kCannotCrossEditingBoundary)
            .DeepEquivalent());
  }
  // When deleting tables: Select the table first, then perform the deletion
  if (IsDisplayInsideTable(downstream_end.ComputeContainerNode()) &&
      downstream_end.ComputeOffsetInContainerNode() <=
          CaretMinOffset(downstream_end.ComputeContainerNode())) {
    const SelectionInDOMTree& selection =
        SelectionInDOMTree::Builder()
            .SetBaseAndExtentDeprecated(
                EndingSelection().End(),
                Position::AfterNode(*downstream_end.ComputeContainerNode()))
            .Build();
    SetEndingSelection(SelectionForUndoStep::From(selection));
    TypingAddedToOpenCommand(kForwardDeleteKey);
    return;
  }

  // deleting to end of paragraph when at end of paragraph needs to merge
  // the next paragraph (if any)
  if (granularity == TextGranularity::kParagraphBoundary &&
      selection_modifier.Selection().IsCaret() &&
      IsEndOfParagraph(selection_modifier.Selection().VisibleEnd())) {
    selection_modifier.Modify(SelectionModifyAlteration::kExtend,
                              SelectionModifyDirection::kForward,
                              TextGranularity::kCharacter);
  }

  const VisibleSelection& selection_to_delete = selection_modifier.Selection();
  if (!StartingSelection().IsRange() ||
      MostBackwardCaretPosition(selection_to_delete.Anchor()) !=
          StartingSelection().Start()) {
    ForwardDeleteKeyPressedInternal(
        SelectionForUndoStep::From(selection_to_delete.AsSelection()),
        SelectionForUndoStep::From(selection_to_delete.AsSelection()),
        kill_ring, editing_state);
    return;
  }
  // Note: |StartingSelection().Start()| can be disconnected.
  const SelectionForUndoStep selection_after_undo =
      SelectionForUndoStep::Builder()
          .SetAnchorAndFocusAsForwardSelection(
              StartingSelection().Start(),
              ComputeExtentForForwardDeleteUndo(selection_to_delete,
                                                StartingSelection().End()))
          .Build();
  ForwardDeleteKeyPressedInternal(
      SelectionForUndoStep::From(selection_to_delete.AsSelection()),
      selection_after_undo, kill_ring, editing_state);
}

void TypingCommand::ForwardDeleteKeyPressedInternal(
    const SelectionForUndoStep& selection_to_delete,
    const SelectionForUndoStep& selection_after_undo,
    bool kill_ring,
    EditingState* editing_state) {
  DCHECK(!selection_to_delete.IsNone());
  if (selection_to_delete.IsNone())
    return;

  if (selection_to_delete.IsCaret())
    return;

  LocalFrame* frame = GetDocument().GetFrame();
  DCHECK(frame);

  if (kill_ring) {
    frame->GetEditor().AddToKillRing(CreateVisibleSelection(selection_to_delete)
                                         .ToNormalizedEphemeralRange());
  }
  // Make undo select what was deleted on Mac alone
  if (frame->GetEditor().Behavior().ShouldUndoOfDeleteSelectText())
    SetStartingSelection(selection_after_undo);
  DeleteSelectionIfRange(selection_to_delete, editing_state);
  if (editing_state->IsAborted())
    return;
  SetSmartDelete(false);
  TypingAddedToOpenCommand(kForwardDeleteKey);
}

void TypingCommand::DeleteSelection(bool smart_delete,
                                    EditingState* editing_state) {
  if (!CompositeEditCommand::DeleteSelection(
          editing_state, smart_delete ? DeleteSelectionOptions::SmartDelete()
                                      : DeleteSelectionOptions::NormalDelete()))
    return;
  TypingAddedToOpenCommand(kDeleteSelection);
}

void TypingCommand::UpdatePreservesTypingStyle(CommandType command_type) {
  switch (command_type) {
    case kDeleteSelection:
    case kDeleteKey:
    case kForwardDeleteKey:
    case kInsertParagraphSeparator:
    case kInsertLineBreak:
      preserves_typing_style_ = true;
      return;
    case kInsertParagraphSeparatorInQuotedContent:
    case kInsertText:
      preserves_typing_style_ = false;
      return;
  }
  NOTREACHED_IN_MIGRATION();
  preserves_typing_style_ = false;
}

bool TypingCommand::IsTypingCommand() const {
  return true;
}

}  // namespace blink
