// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/editing/commands/undo_step.h"

#include "third_party/blink/renderer/core/dom/events/scoped_event_queue.h"
#include "third_party/blink/renderer/core/editing/commands/edit_command.h"
#include "third_party/blink/renderer/core/editing/commands/editing_commands_utilities.h"
#include "third_party/blink/renderer/core/editing/commands/undo_stack.h"
#include "third_party/blink/renderer/core/editing/editing_utilities.h"
#include "third_party/blink/renderer/core/editing/editor.h"
#include "third_party/blink/renderer/core/editing/frame_selection.h"
#include "third_party/blink/renderer/core/editing/selection_template.h"
#include "third_party/blink/renderer/core/editing/set_selection_options.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"

namespace blink {

namespace {
uint64_t g_current_sequence_number = 0;
}

UndoStep::UndoStep(Document* document,
                   const SelectionForUndoStep& starting_selection,
                   const SelectionForUndoStep& ending_selection)
    : document_(document),
      starting_selection_(starting_selection),
      ending_selection_(ending_selection),
      sequence_number_(++g_current_sequence_number) {
  // Note: Both |starting_selection| and |ending_selection| can be null,
  // Note: |starting_selection_| can be disconnected when forward-delete.
  // See |TypingCommand::ForwardDeleteKeyPressed()|
}

bool UndoStep::IsOwnedBy(const Element& element) const {
  return EndingRootEditableElement() == &element;
}

void UndoStep::Unapply() {
  DCHECK(document_);
  LocalFrame* frame = document_->GetFrame();
  DCHECK(frame);

  // Changes to the document may have been made since the last editing operation
  // that require a layout, as in <rdar://problem/5658603>. Low level
  // operations, like RemoveNodeCommand, don't require a layout because the high
  // level operations that use them perform one if one is necessary (like for
  // the creation of VisiblePositions).
  document_->UpdateStyleAndLayout(DocumentUpdateReason::kEditing);

  {
    wtf_size_t size = commands_.size();
    for (wtf_size_t i = size; i; --i)
      commands_[i - 1]->DoUnapply();
  }

  EventQueueScope scope;

  DispatchEditableContentChangedEvents(StartingRootEditableElement(),
                                       EndingRootEditableElement());
  DispatchInputEventEditableContentChanged(
      StartingRootEditableElement(), EndingRootEditableElement(),
      InputEvent::InputType::kHistoryUndo, g_null_atom,
      InputEvent::EventIsComposing::kNotComposing);

  const SelectionInDOMTree& new_selection =
      CorrectedSelectionAfterCommand(StartingSelection(), document_);
  ChangeSelectionAfterCommand(frame, new_selection,
                              SetSelectionOptions::Builder()
                                  .SetShouldCloseTyping(true)
                                  .SetShouldClearTypingStyle(true)
                                  .SetIsDirectional(SelectionIsDirectional())
                                  .Build());
  // `new_selection` may not be valid here, e.g. "focus" event handler modifies
  // DOM tree. See http://crbug.com/1378068
  Editor& editor = frame->GetEditor();
  editor.SetLastEditCommand(nullptr);
  editor.GetUndoStack().RegisterRedoStep(this);

  // Take selection `FrameSelection` which `ChangeSelectionAfterCommand()` set.
  editor.RespondToChangedContents(
      frame->Selection().GetSelectionInDOMTree().Anchor());
}

void UndoStep::Reapply() {
  DCHECK(document_);
  LocalFrame* frame = document_->GetFrame();
  DCHECK(frame);

  // Changes to the document may have been made since the last editing operation
  // that require a layout, as in <rdar://problem/5658603>. Low level
  // operations, like RemoveNodeCommand, don't require a layout because the high
  // level operations that use them perform one if one is necessary (like for
  // the creation of VisiblePositions).
  document_->UpdateStyleAndLayout(DocumentUpdateReason::kEditing);

  {
    for (const auto& command : commands_)
      command->DoReapply();
  }

  EventQueueScope scope;

  DispatchEditableContentChangedEvents(StartingRootEditableElement(),
                                       EndingRootEditableElement());
  DispatchInputEventEditableContentChanged(
      StartingRootEditableElement(), EndingRootEditableElement(),
      InputEvent::InputType::kHistoryRedo, g_null_atom,
      InputEvent::EventIsComposing::kNotComposing);

  const SelectionInDOMTree& new_selection =
      CorrectedSelectionAfterCommand(EndingSelection(), document_);
  ChangeSelectionAfterCommand(frame, new_selection,
                              SetSelectionOptions::Builder()
                                  .SetShouldCloseTyping(true)
                                  .SetShouldClearTypingStyle(true)
                                  .SetIsDirectional(SelectionIsDirectional())
                                  .Build());
  // `new_selection` may not be valid here, e.g. "focus" event handler modifies
  // DOM tree. See http://crbug.com/1378068
  Editor& editor = frame->GetEditor();
  editor.SetLastEditCommand(nullptr);
  editor.GetUndoStack().RegisterUndoStep(this);

  // Take selection `FrameSelection` which `ChangeSelectionAfterCommand()` set.
  editor.RespondToChangedContents(
      frame->Selection().GetSelectionInDOMTree().Anchor());
}

void UndoStep::Append(SimpleEditCommand* command) {
  commands_.push_back(command);
}

void UndoStep::Append(UndoStep* undo_step) {
  commands_.AppendVector(undo_step->commands_);
}

void UndoStep::SetStartingSelection(const SelectionForUndoStep& selection) {
  starting_selection_ = selection;
}

void UndoStep::SetEndingSelection(const SelectionForUndoStep& selection) {
  ending_selection_ = selection;
}

void UndoStep::Trace(Visitor* visitor) const {
  visitor->Trace(document_);
  visitor->Trace(starting_selection_);
  visitor->Trace(ending_selection_);
  visitor->Trace(commands_);
}

}  // namespace blink
