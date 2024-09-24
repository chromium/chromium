/*
 * Copyright (C) 2006, 2007, 2008, 2011 Apple Inc. All rights reserved.
 * Copyright (C) 2008 Nokia Corporation and/or its subsidiary(-ies)
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

#include "third_party/blink/renderer/core/editing/editor.h"

#include "third_party/blink/public/web/web_local_frame_client.h"
#include "third_party/blink/renderer/core/accessibility/ax_object_cache.h"
#include "third_party/blink/renderer/core/clipboard/data_object.h"
#include "third_party/blink/renderer/core/clipboard/data_transfer.h"
#include "third_party/blink/renderer/core/clipboard/data_transfer_access_policy.h"
#include "third_party/blink/renderer/core/clipboard/system_clipboard.h"
#include "third_party/blink/renderer/core/css/css_computed_style_declaration.h"
#include "third_party/blink/renderer/core/css/css_property_names.h"
#include "third_party/blink/renderer/core/css/css_property_value_set.h"
#include "third_party/blink/renderer/core/dom/document_fragment.h"
#include "third_party/blink/renderer/core/dom/element_traversal.h"
#include "third_party/blink/renderer/core/dom/events/scoped_event_queue.h"
#include "third_party/blink/renderer/core/dom/node_traversal.h"
#include "third_party/blink/renderer/core/dom/parser_content_policy.h"
#include "third_party/blink/renderer/core/dom/text.h"
#include "third_party/blink/renderer/core/editing/commands/apply_style_command.h"
#include "third_party/blink/renderer/core/editing/commands/delete_selection_command.h"
#include "third_party/blink/renderer/core/editing/commands/indent_outdent_command.h"
#include "third_party/blink/renderer/core/editing/commands/insert_list_command.h"
#include "third_party/blink/renderer/core/editing/commands/replace_selection_command.h"
#include "third_party/blink/renderer/core/editing/commands/selection_for_undo_step.h"
#include "third_party/blink/renderer/core/editing/commands/simplify_markup_command.h"
#include "third_party/blink/renderer/core/editing/commands/typing_command.h"
#include "third_party/blink/renderer/core/editing/commands/undo_stack.h"
#include "third_party/blink/renderer/core/editing/editing_behavior.h"
#include "third_party/blink/renderer/core/editing/editing_style_utilities.h"
#include "third_party/blink/renderer/core/editing/editing_tri_state.h"
#include "third_party/blink/renderer/core/editing/editing_utilities.h"
#include "third_party/blink/renderer/core/editing/ephemeral_range.h"
#include "third_party/blink/renderer/core/editing/finder/find_buffer.h"
#include "third_party/blink/renderer/core/editing/frame_selection.h"
#include "third_party/blink/renderer/core/editing/ime/input_method_controller.h"
#include "third_party/blink/renderer/core/editing/kill_ring.h"
#include "third_party/blink/renderer/core/editing/markers/document_marker.h"
#include "third_party/blink/renderer/core/editing/markers/document_marker_controller.h"
#include "third_party/blink/renderer/core/editing/selection_template.h"
#include "third_party/blink/renderer/core/editing/serializers/serialization.h"
#include "third_party/blink/renderer/core/editing/set_selection_options.h"
#include "third_party/blink/renderer/core/editing/spellcheck/spell_checker.h"
#include "third_party/blink/renderer/core/editing/visible_position.h"
#include "third_party/blink/renderer/core/editing/visible_units.h"
#include "third_party/blink/renderer/core/events/keyboard_event.h"
#include "third_party/blink/renderer/core/events/text_event.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/html/forms/html_input_element.h"
#include "third_party/blink/renderer/core/html/forms/html_text_area_element.h"
#include "third_party/blink/renderer/core/html/html_image_element.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/input/event_handler.h"
#include "third_party/blink/renderer/core/input_type_names.h"
#include "third_party/blink/renderer/core/layout/hit_test_result.h"
#include "third_party/blink/renderer/core/loader/empty_clients.h"
#include "third_party/blink/renderer/core/loader/resource/image_resource_content.h"
#include "third_party/blink/renderer/core/page/drag_data.h"
#include "third_party/blink/renderer/core/page/focus_controller.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/scroll/scroll_alignment.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/wtf/text/character_names.h"

namespace blink {

namespace {

bool IsInPasswordFieldWithUnrevealedPassword(const Position& position) {
  if (auto* input =
          DynamicTo<HTMLInputElement>(EnclosingTextControl(position))) {
    return input->FormControlType() ==
               mojom::blink::FormControlType::kInputPassword &&
           !input->ShouldRevealPassword();
  }
  return false;
}

}  // anonymous namespace

// When an event handler has moved the selection outside of a text control
// we should use the target control's selection for this editing operation.
SelectionInDOMTree Editor::SelectionForCommand(Event* event) {
  const SelectionInDOMTree selection =
      GetFrameSelection().GetSelectionInDOMTree();
  if (!event)
    return selection;
  // If the target is a text control, and the current selection is outside of
  // its shadow tree, then use the saved selection for that text control.
  if (!IsTextControl(*event->target()->ToNode()))
    return selection;
  auto* text_control_of_selection_start =
      EnclosingTextControl(selection.Anchor());
  auto* text_control_of_target = ToTextControl(event->target()->ToNode());
  if (!selection.IsNone() &&
      text_control_of_target == text_control_of_selection_start)
    return selection;
  const SelectionInDOMTree& select = text_control_of_target->Selection();
  if (select.IsNone())
    return selection;
  return select;
}

// Function considers Mac editing behavior a fallback when Page or Settings is
// not available.
EditingBehavior Editor::Behavior() const {
  if (!GetFrame().GetSettings())
    return EditingBehavior(mojom::blink::EditingBehavior::kEditingMacBehavior);

  return EditingBehavior(GetFrame().GetSettings()->GetEditingBehaviorType());
}

static bool IsCaretAtStartOfWrappedLine(const FrameSelection& selection) {
  if (!selection.ComputeVisibleSelectionInDOMTree().IsCaret())
    return false;
  if (selection.GetSelectionInDOMTree().Affinity() != TextAffinity::kDownstream)
    return false;
  const Position& position =
      selection.ComputeVisibleSelectionInDOMTree().Start();
  if (InSameLine(PositionWithAffinity(position, TextAffinity::kUpstream),
                 PositionWithAffinity(position, TextAffinity::kDownstream)))
    return false;

  // Only when the previous character is a space to avoid undesired side
  // effects. There are cases where a new line is desired even if the previous
  // character is not a space, but typing another space will do.
  Position prev =
      PreviousPositionOf(position, PositionMoveType::kGraphemeCluster);
  const auto* prev_node = DynamicTo<Text>(prev.ComputeContainerNode());
  if (!prev_node)
    return false;
  int prev_offset = prev.ComputeOffsetInContainerNode();
  UChar prev_char = prev_node->data()[prev_offset];
  return prev_char == kSpaceCharacter;
}

bool Editor::HandleTextEvent(TextEvent* event) {
  // Default event handling for Drag and Drop will be handled by DragController
  // so we leave the event for it.
  if (event->IsDrop())
    return false;

  // Default event handling for IncrementalInsertion will be handled by
  // TypingCommand::insertText(), so we leave the event for it.
  if (event->IsIncrementalInsertion())
    return false;

  // TODO(editing-dev): The use of UpdateStyleAndLayout
  // needs to be audited.  See http://crbug.com/590369 for more details.
  frame_->GetDocument()->UpdateStyleAndLayout(DocumentUpdateReason::kEditing);

  if (event->IsPaste()) {
    if (event->PastingFragment()) {
      ReplaceSelectionWithFragment(
          event->PastingFragment(), false, event->ShouldSmartReplace(),
          event->ShouldMatchStyle(), InputEvent::InputType::kInsertFromPaste);
    } else {
      ReplaceSelectionWithText(event->data(), false,
                               event->ShouldSmartReplace(),
                               InputEvent::InputType::kInsertFromPaste);
    }
    return true;
  }

  String data = event->data();
  if (data == "\n") {
    if (event->IsLineBreak())
      return InsertLineBreak();
    return InsertParagraphSeparator();
  }

  // Typing spaces at the beginning of wrapped line is confusing, because
  // inserted spaces would appear in the previous line.
  // Insert a line break automatically so that the spaces appear at the caret.
  // TODO(kojii): rich editing has the same issue, but has more options and
  // needs coordination with JS. Enable for plaintext only for now and collect
  // feedback.
  if (data == " " && !CanEditRichly() &&
      IsCaretAtStartOfWrappedLine(GetFrameSelection())) {
    InsertLineBreak();
  }

  return InsertTextWithoutSendingTextEvent(data, false, event);
}

bool Editor::CanEdit() const {
  return GetFrame()
      .Selection()
      .ComputeVisibleSelectionInDOMTreeDeprecated()
      .RootEditableElement();
}

bool Editor::CanEditRichly() const {
  return IsRichlyEditablePosition(
      GetFrame()
          .Selection()
          .ComputeVisibleSelectionInDOMTreeDeprecated()
          .Anchor());
}

bool Editor::CanCut() const {
  return CanCopy() && CanDelete();
}

bool Editor::CanCopy() const {
  if (ImageElementFromImageDocument(GetFrame().GetDocument()))
    return true;
  FrameSelection& selection = GetFrameSelection();
  if (!selection.IsAvailable())
    return false;
  frame_->GetDocument()->UpdateStyleAndLayout(DocumentUpdateReason::kEditing);
  const VisibleSelectionInFlatTree& visible_selection =
      selection.ComputeVisibleSelectionInFlatTree();
  return visible_selection.IsRange() &&
         !IsInPasswordFieldWithUnrevealedPassword(
             ToPositionInDOMTree(visible_selection.Start()));
}

bool Editor::CanPaste() const {
  return CanEdit();
}

bool Editor::CanDelete() const {
  FrameSelection& selection = GetFrameSelection();
  return selection.ComputeVisibleSelectionInDOMTreeDeprecated().IsRange() &&
         selection.ComputeVisibleSelectionInDOMTree().RootEditableElement();
}

bool Editor::SmartInsertDeleteEnabled() const {
  if (Settings* settings = GetFrame().GetSettings())
    return settings->GetSmartInsertDeleteEnabled();
  return false;
}

bool Editor::IsSelectTrailingWhitespaceEnabled() const {
  if (Settings* settings = GetFrame().GetSettings())
    return settings->GetSelectTrailingWhitespaceEnabled();
  return false;
}

void Editor::DeleteSelectionWithSmartDelete(
    DeleteMode delete_mode,
    InputEvent::InputType input_type,
    const Position& reference_move_position) {
  if (GetFrame()
          .Selection()
          .ComputeVisibleSelectionInDOMTreeDeprecated()
          .IsNone())
    return;

  DCHECK(GetFrame().GetDocument());
  MakeGarbageCollected<DeleteSelectionCommand>(
      *GetFrame().GetDocument(),
      DeleteSelectionOptions::Builder()
          .SetSmartDelete(delete_mode == DeleteMode::kSmart)
          .SetMergeBlocksAfterDelete(true)
          .SetExpandForSpecialElements(true)
          .SetSanitizeMarkup(true)
          .Build(),
      input_type, reference_move_position)
      ->Apply();
}

void Editor::ReplaceSelectionWithFragment(DocumentFragment* fragment,
                                          bool select_replacement,
                                          bool smart_replace,
                                          bool match_style,
                                          InputEvent::InputType input_type) {
  DCHECK(!GetFrame().GetDocument()->NeedsLayoutTreeUpdate());
  const VisibleSelection& selection =
      GetFrameSelection().ComputeVisibleSelectionInDOMTree();
  if (selection.IsNone() || !selection.IsContentEditable() || !fragment)
    return;

  ReplaceSelectionCommand::CommandOptions options =
      ReplaceSelectionCommand::kPreventNesting |
      ReplaceSelectionCommand::kSanitizeFragment;
  if (select_replacement)
    options |= ReplaceSelectionCommand::kSelectReplacement;
  if (smart_replace)
    options |= ReplaceSelectionCommand::kSmartReplace;
  if (match_style)
    options |= ReplaceSelectionCommand::kMatchStyle;
  DCHECK(GetFrame().GetDocument());
  MakeGarbageCollected<ReplaceSelectionCommand>(*GetFrame().GetDocument(),
                                                fragment, options, input_type)
      ->Apply();
  RevealSelectionAfterEditingOperation();
}

void Editor::ReplaceSelectionWithText(const String& text,
                                      bool select_replacement,
                                      bool smart_replace,
                                      InputEvent::InputType input_type) {
  ReplaceSelectionWithFragment(CreateFragmentFromText(SelectedRange(), text),
                               select_replacement, smart_replace, true,
                               input_type);
}

void Editor::ReplaceSelectionAfterDragging(DocumentFragment* fragment,
                                           InsertMode insert_mode,
                                           DragSourceType drag_source_type) {
  ReplaceSelectionCommand::CommandOptions options =
      ReplaceSelectionCommand::kSelectReplacement |
      ReplaceSelectionCommand::kPreventNesting;
  if (insert_mode == InsertMode::kSmart)
    options |= ReplaceSelectionCommand::kSmartReplace;
  if (drag_source_type == DragSourceType::kPlainTextSource)
    options |= ReplaceSelectionCommand::kMatchStyle;
  DCHECK(GetFrame().GetDocument());
  MakeGarbageCollected<ReplaceSelectionCommand>(
      *GetFrame().GetDocument(), fragment, options,
      InputEvent::InputType::kInsertFromDrop)
      ->Apply();
}

bool Editor::DeleteSelectionAfterDraggingWithEvents(
    Element* drag_source,
    DeleteMode delete_mode,
    const Position& reference_move_position) {
  if (!drag_source || !drag_source->isConnected())
    return true;

  // Dispatch 'beforeinput'.
  const bool should_delete =
      DispatchBeforeInputEditorCommand(
          drag_source, InputEvent::InputType::kDeleteByDrag,
          TargetRangesForInputEvent(*drag_source)) ==
      DispatchEventResult::kNotCanceled;

  // 'beforeinput' event handler may destroy frame, return false to cancel
  // remaining actions;
  if (frame_->GetDocument()->GetFrame() != frame_)
    return false;

  // No DOM mutation if EditContext is active.
  if (frame_->GetInputMethodController().GetActiveEditContext())
    return true;

  if (should_delete && drag_source->isConnected()) {
    DeleteSelectionWithSmartDelete(delete_mode,
                                   InputEvent::InputType::kDeleteByDrag,
                                   reference_move_position);
  }

  return true;
}

bool Editor::ReplaceSelectionAfterDraggingWithEvents(
    Element* drop_target,
    DragData* drag_data,
    DocumentFragment* fragment,
    Range* drop_caret_range,
    InsertMode insert_mode,
    DragSourceType drag_source_type) {
  if (!drop_target || !drop_target->isConnected())
    return true;

  // Dispatch 'beforeinput'.
  DataTransfer* data_transfer = DataTransfer::Create(
      DataTransfer::kDragAndDrop, DataTransferAccessPolicy::kReadable,
      drag_data->PlatformData());
  data_transfer->SetSourceOperation(drag_data->DraggingSourceOperationMask());
  const bool should_insert =
      DispatchBeforeInputDataTransfer(
          drop_target, InputEvent::InputType::kInsertFromDrop, data_transfer) ==
      DispatchEventResult::kNotCanceled;

  // 'beforeinput' event handler may destroy frame, return false to cancel
  // remaining actions;
  if (frame_->GetDocument()->GetFrame() != frame_)
    return false;

  // No DOM mutation if EditContext is active.
  if (frame_->GetInputMethodController().GetActiveEditContext())
    return true;

  if (should_insert && drop_target->isConnected())
    ReplaceSelectionAfterDragging(fragment, insert_mode, drag_source_type);

  return true;
}

EphemeralRange Editor::SelectedRange() {
  return GetFrame()
      .Selection()
      .ComputeVisibleSelectionInDOMTreeDeprecated()
      .ToNormalizedEphemeralRange();
}

void Editor::RespondToChangedContents(const Position& position) {
  if (AXObjectCache* cache =
          GetFrame().GetDocument()->ExistingAXObjectCache()) {
    cache->HandleEditableTextContentChanged(position.AnchorNode());
  }

  GetSpellChecker().RespondToChangedContents();
  frame_->Client()->DidChangeContents();
}

void Editor::NotifyAccessibilityOfDeletionOrInsertionInTextField(
    const SelectionForUndoStep& changed_selection,
    bool is_deletion) {
  if (AXObjectCache* cache =
          GetFrame().GetDocument()->ExistingAXObjectCache()) {
    if (!changed_selection.Start().IsValidFor(*GetFrame().GetDocument()) ||
        !changed_selection.End().IsValidFor(*GetFrame().GetDocument())) {
      return;
    }
    cache->HandleDeletionOrInsertionInTextField(changed_selection.AsSelection(),
                                                is_deletion);
  }
}

void Editor::RegisterCommandGroup(CompositeEditCommand* command_group_wrapper) {
  DCHECK(command_group_wrapper->IsCommandGroupWrapper());
  last_edit_command_ = command_group_wrapper;
}

void Editor::ApplyParagraphStyle(CSSPropertyValueSet* style,
                                 InputEvent::InputType input_type) {
  if (GetFrame()
          .Selection()
          .ComputeVisibleSelectionInDOMTreeDeprecated()
          .IsNone() ||
      !style)
    return;
  DCHECK(GetFrame().GetDocument());
  MakeGarbageCollected<ApplyStyleCommand>(
      *GetFrame().GetDocument(), MakeGarbageCollected<EditingStyle>(style),
      input_type, ApplyStyleCommand::kForceBlockProperties)
      ->Apply();
}

void Editor::ApplyParagraphStyleToSelection(CSSPropertyValueSet* style,
                                            InputEvent::InputType input_type) {
  if (!style || style->IsEmpty() || !CanEditRichly())
    return;

  ApplyParagraphStyle(style, input_type);
}

Editor::Editor(LocalFrame& frame)
    : frame_(&frame),
      undo_stack_(MakeGarbageCollected<UndoStack>()),
      prevent_reveal_selection_(0),
      should_start_new_kill_ring_sequence_(false),
      // This is off by default, since most editors want this behavior (this
      // matches IE but not FF).
      should_style_with_css_(false),
      kill_ring_(std::make_unique<KillRing>()),
      default_paragraph_separator_(EditorParagraphSeparator::kIsDiv) {}

Editor::~Editor() = default;

void Editor::Clear() {
  should_style_with_css_ = false;
  default_paragraph_separator_ = EditorParagraphSeparator::kIsDiv;
  last_edit_command_ = nullptr;
  undo_stack_->Clear();
}

bool Editor::InsertText(const String& text, KeyboardEvent* triggering_event) {
  return GetFrame().GetEventHandler().HandleTextInputEvent(text,
                                                           triggering_event);
}

bool Editor::InsertTextWithoutSendingTextEvent(
    const String& text,
    bool select_inserted_text,
    TextEvent* triggering_event,
    InputEvent::InputType input_type) {
  const VisibleSelection& selection =
      CreateVisibleSelection(SelectionForCommand(triggering_event));
  if (!selection.IsContentEditable())
    return false;

  EditingState editing_state;
  // Insert the text
  TypingCommand::InsertText(
      *selection.Start().GetDocument(), text, selection.AsSelection(),
      select_inserted_text ? TypingCommand::kSelectInsertedText : 0,
      &editing_state,
      triggering_event && triggering_event->IsComposition()
          ? TypingCommand::kTextCompositionConfirm
          : TypingCommand::kTextCompositionNone,
      false, input_type);
  if (editing_state.IsAborted())
    return false;

  // Reveal the current selection
  if (LocalFrame* edited_frame = selection.Start().GetDocument()->GetFrame()) {
    if (Page* page = edited_frame->GetPage()) {
      LocalFrame* focused_or_main_frame =
          To<LocalFrame>(page->GetFocusController().FocusedOrMainFrame());
      focused_or_main_frame->Selection().RevealSelection(
          ScrollAlignment::ToEdgeIfNeeded());
    }
  }

  return true;
}

bool Editor::InsertLineBreak() {
  if (!CanEdit())
    return false;

  VisiblePosition caret =
      GetFrameSelection().ComputeVisibleSelectionInDOMTree().VisibleStart();
  DCHECK(GetFrame().GetDocument());
  if (!TypingCommand::InsertLineBreak(*GetFrame().GetDocument()))
    return false;
  RevealSelectionAfterEditingOperation(ScrollAlignment::ToEdgeIfNeeded());

  return true;
}

bool Editor::InsertParagraphSeparator() {
  if (!CanEdit())
    return false;

  if (!CanEditRichly())
    return InsertLineBreak();

  VisiblePosition caret =
      GetFrameSelection().ComputeVisibleSelectionInDOMTree().VisibleStart();
  DCHECK(GetFrame().GetDocument());
  EditingState editing_state;
  if (!TypingCommand::InsertParagraphSeparator(*GetFrame().GetDocument()))
    return false;
  RevealSelectionAfterEditingOperation(ScrollAlignment::ToEdgeIfNeeded());

  return true;
}

static void CountEditingEvent(ExecutionContext* execution_context,
                              const Event& event,
                              WebFeature feature_on_input,
                              WebFeature feature_on_text_area,
                              WebFeature feature_on_content_editable,
                              WebFeature feature_on_non_node) {
  EventTarget* event_target = event.target();
  Node* node = event_target->ToNode();
  if (!node) {
    UseCounter::Count(execution_context, feature_on_non_node);
    return;
  }

  if (IsA<HTMLInputElement>(node)) {
    UseCounter::Count(execution_context, feature_on_input);
    return;
  }

  if (IsA<HTMLTextAreaElement>(node)) {
    UseCounter::Count(execution_context, feature_on_text_area);
    return;
  }

  TextControlElement* control = EnclosingTextControl(node);
  if (IsA<HTMLInputElement>(control)) {
    UseCounter::Count(execution_context, feature_on_input);
    return;
  }

  if (IsA<HTMLTextAreaElement>(control)) {
    UseCounter::Count(execution_context, feature_on_text_area);
    return;
  }

  UseCounter::Count(execution_context, feature_on_content_editable);
}

void Editor::CountEvent(ExecutionContext* execution_context,
                        const Event& event) {
  if (!execution_context)
    return;

  if (event.type() == event_type_names::kTextInput) {
    CountEditingEvent(execution_context, event,
                      WebFeature::kTextInputEventOnInput,
                      WebFeature::kTextInputEventOnTextArea,
                      WebFeature::kTextInputEventOnContentEditable,
                      WebFeature::kTextInputEventOnNotNode);
    return;
  }

  if (event.type() == event_type_names::kWebkitBeforeTextInserted) {
    CountEditingEvent(execution_context, event,
                      WebFeature::kWebkitBeforeTextInsertedOnInput,
                      WebFeature::kWebkitBeforeTextInsertedOnTextArea,
                      WebFeature::kWebkitBeforeTextInsertedOnContentEditable,
                      WebFeature::kWebkitBeforeTextInsertedOnNotNode);
    return;
  }

  if (event.type() == event_type_names::kWebkitEditableContentChanged) {
    CountEditingEvent(
        execution_context, event,
        WebFeature::kWebkitEditableContentChangedOnInput,
        WebFeature::kWebkitEditableContentChangedOnTextArea,
        WebFeature::kWebkitEditableContentChangedOnContentEditable,
        WebFeature::kWebkitEditableContentChangedOnNotNode);
  }
}

void Editor::CopyImage(const HitTestResult& result) {
  WriteImageNodeToClipboard(*frame_->GetSystemClipboard(),
                            *result.InnerNodeOrImageMapImage(),
                            result.AltDisplayString());
}

void Editor::CopyImage(const HitTestResult& result,
                       const scoped_refptr<Image>& image) {
  WriteImageToClipboard(*frame_->GetSystemClipboard(), image, KURL(),
                        result.AltDisplayString());
}

bool Editor::CanUndo() {
  return undo_stack_->CanUndo();
}

void Editor::Undo() {
  undo_stack_->Undo();
}

bool Editor::CanRedo() {
  return undo_stack_->CanRedo();
}

void Editor::Redo() {
  undo_stack_->Redo();
}

void Editor::SetBaseWritingDirection(
    mojo_base::mojom::blink::TextDirection direction) {
  Element* focused_element = GetFrame().GetDocument()->FocusedElement();
  if (auto* text_control = ToTextControlOrNull(focused_element)) {
    if (direction == mojo_base::mojom::blink::TextDirection::UNKNOWN_DIRECTION)
      return;
    text_control->setAttribute(
        html_names::kDirAttr,
        AtomicString(
            direction == mojo_base::mojom::blink::TextDirection::LEFT_TO_RIGHT
                ? "ltr"
                : "rtl"));
    text_control->DispatchInputEvent();
    return;
  }

  auto* style =
      MakeGarbageCollected<MutableCSSPropertyValueSet>(kHTMLQuirksMode);
  style->ParseAndSetProperty(
      CSSPropertyID::kDirection,
      direction == mojo_base::mojom::blink::TextDirection::LEFT_TO_RIGHT ? "ltr"
      : direction == mojo_base::mojom::blink::TextDirection::RIGHT_TO_LEFT
          ? "rtl"
          : "inherit",
      /* important */ false, GetFrame().DomWindow()->GetSecureContextMode());
  ApplyParagraphStyleToSelection(
      style, InputEvent::InputType::kFormatSetBlockTextDirection);
}

void Editor::RevealSelectionAfterEditingOperation(
    const mojom::blink::ScrollAlignment& alignment) {
  if (prevent_reveal_selection_)
    return;
  if (!GetFrameSelection().IsAvailable())
    return;
  GetFrameSelection().RevealSelection(alignment, kDoNotRevealExtent);
}

void Editor::AddImageResourceObserver(ImageResourceObserver* observer) {
  image_resource_observers_.insert(observer);
}

void Editor::RemoveImageResourceObserver(ImageResourceObserver* observer) {
  image_resource_observers_.erase(observer);
}

void Editor::AddToKillRing(const EphemeralRange& range) {
  if (should_start_new_kill_ring_sequence_)
    GetKillRing().StartNewSequence();

  DCHECK(!GetFrame().GetDocument()->NeedsLayoutTreeUpdate());
  String text = PlainText(range);
  GetKillRing().Append(text);
  should_start_new_kill_ring_sequence_ = false;
}

EphemeralRange Editor::RangeForPoint(const gfx::Point& frame_point) const {
  const PositionWithAffinity position_with_affinity =
      GetFrame().PositionForPoint(PhysicalOffset(frame_point));
  if (position_with_affinity.IsNull())
    return EphemeralRange();

  const VisiblePosition position =
      CreateVisiblePosition(position_with_affinity);
  const VisiblePosition previous = PreviousPositionOf(position);
  if (previous.IsNotNull()) {
    const EphemeralRange previous_character_range =
        MakeRange(previous, position);
    const gfx::Rect rect = FirstRectForRange(previous_character_range);
    if (rect.Contains(frame_point))
      return EphemeralRange(previous_character_range);
  }

  const VisiblePosition next = NextPositionOf(position);
  const EphemeralRange next_character_range = MakeRange(position, next);
  if (next_character_range.IsNotNull()) {
    const gfx::Rect rect = FirstRectForRange(next_character_range);
    if (rect.Contains(frame_point))
      return EphemeralRange(next_character_range);
  }

  return EphemeralRange();
}

EphemeralRange Editor::RangeBetweenPoints(const gfx::Point& start_point,
                                          const gfx::Point& end_point) const {
  const PositionWithAffinity start_position =
      GetFrame().PositionForPoint(PhysicalOffset(start_point));
  if (start_position.IsNull())
    return EphemeralRange();
  const VisiblePosition start_visible_position =
      CreateVisiblePosition(start_position);
  if (start_visible_position.IsNull())
    return EphemeralRange();

  const PositionWithAffinity end_position =
      GetFrame().PositionForPoint(PhysicalOffset(end_point));
  if (end_position.IsNull())
    return EphemeralRange();
  const VisiblePosition end_visible_position =
      CreateVisiblePosition(end_position);
  if (end_visible_position.IsNull())
    return EphemeralRange();
  return start_position.GetPosition() <= end_position.GetPosition()
             ? MakeRange(start_visible_position, end_visible_position)
             : MakeRange(end_visible_position, start_visible_position);
}

void Editor::ComputeAndSetTypingStyle(CSSPropertyValueSet* style,
                                      InputEvent::InputType input_type) {
  if (!style || style->IsEmpty()) {
    ClearTypingStyle();
    return;
  }

  // Calculate the current typing style.
  if (typing_style_)
    typing_style_->OverrideWithStyle(style);
  else
    typing_style_ = MakeGarbageCollected<EditingStyle>(style);

  const Position& position = GetFrame()
                                 .Selection()
                                 .ComputeVisibleSelectionInDOMTreeDeprecated()
                                 .VisibleStart()
                                 .DeepEquivalent();
  if (position.IsNull())
    return;
  typing_style_->PrepareToApplyAt(position,
                                  EditingStyle::kPreserveWritingDirection);

  // Handle block styles, substracting these from the typing style.
  EditingStyle* block_style =
      typing_style_->ExtractAndRemoveBlockProperties(GetFrame().DomWindow());
  if (!block_style->IsEmpty()) {
    DCHECK(GetFrame().GetDocument());
    MakeGarbageCollected<ApplyStyleCommand>(*GetFrame().GetDocument(),
                                            block_style, input_type)
        ->Apply();
  }
}

bool Editor::FindString(LocalFrame& frame,
                        const String& target,
                        FindOptions options) {
  VisibleSelectionInFlatTree selection =
      frame.Selection().ComputeVisibleSelectionInFlatTree();

  // TODO(yosin) We should make |findRangeOfString()| to return
  // |EphemeralRange| rather than|Range| object.
  Range* const result_range = FindRangeOfString(
      *frame.GetDocument(), target,
      EphemeralRangeInFlatTree(selection.Start(), selection.End()),
      options.SetFindApiCall(true));

  if (!result_range)
    return false;

  frame.Selection().SetSelectionAndEndTyping(
      SelectionInDOMTree::Builder()
          .SetBaseAndExtent(EphemeralRange(result_range))
          .Build());
  frame.Selection().RevealSelection();
  return true;
}

// TODO(yosin) We should return |EphemeralRange| rather than |Range|. We use
// |Range| object for checking whether start and end position crossing shadow
// boundaries, however we can do it without |Range| object.
static Range* FindStringBetweenPositions(
    const String& target,
    const EphemeralRangeInFlatTree& reference_range,
    FindOptions options) {
  EphemeralRangeInFlatTree search_range(reference_range);

  bool forward = !options.IsBackwards();

  while (true) {
    EphemeralRangeInFlatTree result_range =
        FindBuffer::FindMatchInRange(search_range, target, options);
    if (result_range.IsCollapsed())
      return nullptr;

    auto* range_object = MakeGarbageCollected<Range>(
        result_range.GetDocument(),
        ToPositionInDOMTree(result_range.StartPosition()),
        ToPositionInDOMTree(result_range.EndPosition()));
    if (!range_object->collapsed())
      return range_object;

    // Found text spans over multiple TreeScopes. Since it's impossible to
    // return such section as a Range, we skip this match and seek for the
    // next occurrence.
    // TODO(yosin) Handle this case.
    if (forward) {
      search_range = EphemeralRangeInFlatTree(
          NextPositionOf(result_range.StartPosition(),
                         PositionMoveType::kGraphemeCluster),
          search_range.EndPosition());
    } else {
      search_range = EphemeralRangeInFlatTree(
          search_range.StartPosition(),
          PreviousPositionOf(result_range.EndPosition(),
                             PositionMoveType::kGraphemeCluster));
    }
  }

  NOTREACHED_IN_MIGRATION();
  return nullptr;
}

Range* Editor::FindRangeOfString(
    Document& document,
    const String& target,
    const EphemeralRangeInFlatTree& reference_range,
    FindOptions options,
    bool* wrapped_around) {
  if (target.empty())
    return nullptr;

  // Start from an edge of the reference range. Which edge is used depends on
  // whether we're searching forward or backward, and whether startInSelection
  // is set.
  EphemeralRangeInFlatTree document_range =
      EphemeralRangeInFlatTree::RangeOfContents(document);
  EphemeralRangeInFlatTree search_range(document_range);

  const bool forward = !options.IsBackwards();
  bool start_in_reference_range = false;
  if (reference_range.IsNotNull()) {
    start_in_reference_range = options.IsStartingInSelection();
    if (forward && start_in_reference_range) {
      search_range = EphemeralRangeInFlatTree(reference_range.StartPosition(),
                                              document_range.EndPosition());
    } else if (forward) {
      search_range = EphemeralRangeInFlatTree(reference_range.EndPosition(),
                                              document_range.EndPosition());
    } else if (start_in_reference_range) {
      search_range = EphemeralRangeInFlatTree(document_range.StartPosition(),
                                              reference_range.EndPosition());
    } else {
      search_range = EphemeralRangeInFlatTree(document_range.StartPosition(),
                                              reference_range.StartPosition());
    }
  }

  Range* result_range =
      FindStringBetweenPositions(target, search_range, options);

  // If we started in the reference range and the found range exactly matches
  // the reference range, find again. Build a selection with the found range
  // to remove collapsed whitespace. Compare ranges instead of selection
  // objects to ignore the way that the current selection was made.
  if (result_range && start_in_reference_range &&
      NormalizeRange(EphemeralRangeInFlatTree(result_range)) ==
          reference_range) {
    if (forward)
      search_range = EphemeralRangeInFlatTree(
          ToPositionInFlatTree(result_range->EndPosition()),
          search_range.EndPosition());
    else
      search_range = EphemeralRangeInFlatTree(
          search_range.StartPosition(),
          ToPositionInFlatTree(result_range->StartPosition()));
    result_range = FindStringBetweenPositions(target, search_range, options);
  }

  if (!result_range && options.IsWrappingAround()) {
    if (wrapped_around)
      *wrapped_around = true;
    return FindStringBetweenPositions(target, document_range, options);
  }

  return result_range;
}

void Editor::RespondToChangedSelection() {
  GetSpellChecker().RespondToChangedSelection();
  SyncSelection(blink::SyncCondition::kNotForced);
  SetStartNewKillRingSequence(true);
}

void Editor::SyncSelection(SyncCondition force_sync) {
  frame_->Client()->DidChangeSelection(
      !GetFrameSelection().GetSelectionInDOMTree().IsRange(), force_sync);
}

SpellChecker& Editor::GetSpellChecker() const {
  return GetFrame().GetSpellChecker();
}

FrameSelection& Editor::GetFrameSelection() const {
  return GetFrame().Selection();
}

void Editor::SetMark() {
  mark_ = GetFrameSelection().ComputeVisibleSelectionInDOMTree();
  mark_is_directional_ = GetFrameSelection().IsDirectional();
}

void Editor::ReplaceSelection(const String& text) {
  DCHECK(!GetFrame().GetDocument()->NeedsLayoutTreeUpdate());
  bool select_replacement = Behavior().ShouldSelectReplacement();
  bool smart_replace = false;
  ReplaceSelectionWithText(text, select_replacement, smart_replace,
                           InputEvent::InputType::kInsertReplacementText);
}

void Editor::ElementRemoved(Element* element) {
  if (last_edit_command_ &&
      last_edit_command_->EndingSelection().RootEditableElement() == element) {
    last_edit_command_ = nullptr;
  }
}

void Editor::Trace(Visitor* visitor) const {
  visitor->Trace(frame_);
  visitor->Trace(last_edit_command_);
  visitor->Trace(undo_stack_);
  visitor->Trace(mark_);
  visitor->Trace(typing_style_);
  visitor->Trace(image_resource_observers_);
}

}  // namespace blink
