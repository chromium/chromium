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

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/core/editing/ime/input_method_controller.h"

#include <tuple>

#include "third_party/blink/public/mojom/input/focus_type.mojom-blink.h"
#include "third_party/blink/public/web/web_frame_widget.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/dom_node_ids.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/events/event_dispatcher.h"
#include "third_party/blink/renderer/core/dom/events/scoped_event_queue.h"
#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/dom/range.h"
#include "third_party/blink/renderer/core/dom/text.h"
#include "third_party/blink/renderer/core/editing/commands/delete_selection_command.h"
#include "third_party/blink/renderer/core/editing/commands/undo_stack.h"
#include "third_party/blink/renderer/core/editing/editing_utilities.h"
#include "third_party/blink/renderer/core/editing/editor.h"
#include "third_party/blink/renderer/core/editing/ephemeral_range.h"
#include "third_party/blink/renderer/core/editing/frame_selection.h"
#include "third_party/blink/renderer/core/editing/ime/edit_context.h"
#include "third_party/blink/renderer/core/editing/markers/document_marker_controller.h"
#include "third_party/blink/renderer/core/editing/markers/suggestion_marker_properties.h"
#include "third_party/blink/renderer/core/editing/reveal_selection_scope.h"
#include "third_party/blink/renderer/core/editing/selection_template.h"
#include "third_party/blink/renderer/core/editing/set_selection_options.h"
#include "third_party/blink/renderer/core/editing/spellcheck/spell_checker.h"
#include "third_party/blink/renderer/core/editing/state_machines/backward_code_point_state_machine.h"
#include "third_party/blink/renderer/core/editing/state_machines/forward_code_point_state_machine.h"
#include "third_party/blink/renderer/core/events/composition_event.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_client.h"
#include "third_party/blink/renderer/core/html/canvas/html_canvas_element.h"
#include "third_party/blink/renderer/core/html/forms/html_input_element.h"
#include "third_party/blink/renderer/core/html/forms/html_text_area_element.h"
#include "third_party/blink/renderer/core/input/context_menu_allowed_scope.h"
#include "third_party/blink/renderer/core/input/event_handler.h"
#include "third_party/blink/renderer/core/input_type_names.h"
#include "third_party/blink/renderer/core/keywords.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/layout/layout_theme.h"
#include "third_party/blink/renderer/core/page/focus_controller.h"
#include "third_party/blink/renderer/core/page/page.h"

namespace blink {

using mojom::blink::FormControlType;

namespace {

bool NeedsIncrementalInsertion(const LocalFrame& frame,
                               const String& new_text) {
  // No need to apply incremental insertion if it doesn't support formated text.
  if (!frame.GetEditor().CanEditRichly())
    return false;

  // No need to apply incremental insertion if the old text (text to be
  // replaced) or the new text (text to be inserted) is empty.
  if (frame.SelectedText().empty() || new_text.empty())
    return false;

  return true;
}

AtomicString GetInputModeAttribute(Element* element) {
  if (!element)
    return AtomicString();

  bool query_attribute = false;
  if (auto* input = DynamicTo<HTMLInputElement>(*element)) {
    query_attribute = input->SupportsInputModeAttribute();
  } else if (IsA<HTMLTextAreaElement>(*element)) {
    query_attribute = true;
  } else {
    element->GetDocument().UpdateStyleAndLayoutTree();
    if (IsEditable(*element))
      query_attribute = true;
  }

  if (!query_attribute)
    return AtomicString();

  // TODO(dtapuska): We may wish to restrict this to a yet to be proposed
  // <contenteditable> or <richtext> element Mozilla discussed at TPAC 2016.
  return element->FastGetAttribute(html_names::kInputmodeAttr).LowerASCII();
}

AtomicString GetEnterKeyHintAttribute(Element* element) {
  if (!element)
    return AtomicString();

  bool query_attribute = false;
  if (auto* input = DynamicTo<HTMLInputElement>(*element)) {
    query_attribute = input->SupportsInputModeAttribute();
  } else if (IsA<HTMLTextAreaElement>(*element)) {
    query_attribute = true;
  } else {
    element->GetDocument().UpdateStyleAndLayoutTree();
    if (IsEditable(*element))
      query_attribute = true;
  }

  if (!query_attribute)
    return AtomicString();

  return element->FastGetAttribute(html_names::kEnterkeyhintAttr).LowerASCII();
}

AtomicString GetVirtualKeyboardPolicyAttribute(Element* element) {
  if (!element)
    return AtomicString();

  if (!element->MayTriggerVirtualKeyboard())
    return g_null_atom;

  const AtomicString& virtual_keyboard_policy_value =
      element->FastGetAttribute(html_names::kVirtualkeyboardpolicyAttr);
  if (virtual_keyboard_policy_value.IsNull())
    return AtomicString();

  return virtual_keyboard_policy_value.LowerASCII();
}

constexpr int kInvalidDeletionLength = -1;
constexpr bool IsInvalidDeletionLength(const int length) {
  return length == kInvalidDeletionLength;
}

int CalculateBeforeDeletionLengthsInCodePoints(
    const String& text,
    const int before_length_in_code_points,
    const int selection_start) {
  DCHECK_GE(before_length_in_code_points, 0);
  DCHECK_GE(selection_start, 0);
  DCHECK_LE(selection_start, static_cast<int>(text.length()));

  const UChar* u_text = text.Characters16();
  BackwardCodePointStateMachine backward_machine;
  int counter = before_length_in_code_points;
  int deletion_start = selection_start;
  while (counter > 0 && deletion_start > 0) {
    const TextSegmentationMachineState state =
        backward_machine.FeedPrecedingCodeUnit(u_text[deletion_start - 1]);
    // According to Android's InputConnection spec, we should do nothing if
    // |text| has invalid surrogate pair in the deletion range.
    if (state == TextSegmentationMachineState::kInvalid)
      return kInvalidDeletionLength;

    if (backward_machine.AtCodePointBoundary())
      --counter;
    --deletion_start;
  }
  if (!backward_machine.AtCodePointBoundary())
    return kInvalidDeletionLength;

  const int offset = backward_machine.GetBoundaryOffset();
  DCHECK_EQ(-offset, selection_start - deletion_start);
  return -offset;
}

int CalculateAfterDeletionLengthsInCodePoints(
    const String& text,
    const int after_length_in_code_points,
    const int selection_end) {
  DCHECK_GE(after_length_in_code_points, 0);
  DCHECK_GE(selection_end, 0);
  const int length = text.length();
  DCHECK_LE(selection_end, length);

  const UChar* u_text = text.Characters16();
  ForwardCodePointStateMachine forward_machine;
  int counter = after_length_in_code_points;
  int deletion_end = selection_end;
  while (counter > 0 && deletion_end < length) {
    const TextSegmentationMachineState state =
        forward_machine.FeedFollowingCodeUnit(u_text[deletion_end]);
    // According to Android's InputConnection spec, we should do nothing if
    // |text| has invalid surrogate pair in the deletion range.
    if (state == TextSegmentationMachineState::kInvalid)
      return kInvalidDeletionLength;

    if (forward_machine.AtCodePointBoundary())
      --counter;
    ++deletion_end;
  }
  if (!forward_machine.AtCodePointBoundary())
    return kInvalidDeletionLength;

  const int offset = forward_machine.GetBoundaryOffset();
  DCHECK_EQ(offset, deletion_end - selection_end);
  return offset;
}

Element* RootEditableElementOfSelection(const FrameSelection& frameSelection) {
  const SelectionInDOMTree& selection = frameSelection.GetSelectionInDOMTree();
  if (selection.IsNone())
    return nullptr;
  // To avoid update layout, we attempt to get root editable element from
  // a position where script/user specified.
  if (Element* editable = RootEditableElementOf(selection.Anchor())) {
    return editable;
  }

  // This is work around for applications assumes a position before editable
  // element as editable[1]
  // [1] http://crbug.com/712761

  // TODO(editing-dev): Use of UpdateStyleAndLayout
  // needs to be audited. see http://crbug.com/590369 for more details.
  frameSelection.GetDocument().UpdateStyleAndLayout(
      DocumentUpdateReason::kEditing);
  const VisibleSelection& visibleSeleciton =
      frameSelection.ComputeVisibleSelectionInDOMTree();
  return RootEditableElementOf(visibleSeleciton.Start());
}

std::pair<ContainerNode*, PlainTextRange> PlainTextRangeForEphemeralRange(
    const EphemeralRange& range) {
  if (range.IsNull())
    return {};
  ContainerNode* const editable =
      RootEditableElementOrTreeScopeRootNodeOf(range.StartPosition());
  DCHECK(editable);
  return std::make_pair(editable, PlainTextRange::Create(*editable, range));
}

int ComputeAutocapitalizeFlags(const Element* element) {
  const auto* const html_element = DynamicTo<HTMLElement>(element);
  if (!html_element)
    return 0;

  // We set the autocapitalization flag corresponding to the "used
  // autocapitalization hint" for the focused element:
  // https://html.spec.whatwg.org/C/#used-autocapitalization-hint
  if (auto* input = DynamicTo<HTMLInputElement>(*html_element)) {
    FormControlType input_type = input->FormControlType();
    if (input_type == FormControlType::kInputEmail ||
        input_type == FormControlType::kInputUrl ||
        input_type == FormControlType::kInputPassword) {
      // The autocapitalize IDL attribute value is ignored for these input
      // types, so we set the None flag.
      return kWebTextInputFlagAutocapitalizeNone;
    }
  }

  int flags = 0;

  DEFINE_STATIC_LOCAL(const AtomicString, none, ("none"));
  DEFINE_STATIC_LOCAL(const AtomicString, characters, ("characters"));
  DEFINE_STATIC_LOCAL(const AtomicString, words, ("words"));
  DEFINE_STATIC_LOCAL(const AtomicString, sentences, ("sentences"));

  const AtomicString& autocapitalize = html_element->autocapitalize();
  if (autocapitalize == none) {
    flags |= kWebTextInputFlagAutocapitalizeNone;
  } else if (autocapitalize == characters) {
    flags |= kWebTextInputFlagAutocapitalizeCharacters;
  } else if (autocapitalize == words) {
    flags |= kWebTextInputFlagAutocapitalizeWords;
  } else if (autocapitalize == sentences) {
    flags |= kWebTextInputFlagAutocapitalizeSentences;
  } else if (autocapitalize == g_empty_atom) {
    // https://html.spec.whatwg.org/multipage/interaction.html#autocapitalization
    // If autocapitalize is empty, the UA can decide on an appropriate behavior
    // depending on context. We use the presence of the autocomplete attribute
    // with an email/url/password type as a hint to disable autocapitalization.
    if (auto* form_control = DynamicTo<HTMLFormControlElement>(html_element);
        form_control && form_control->IsAutocompleteEmailUrlOrPassword()) {
      flags |= kWebTextInputFlagAutocapitalizeNone;
    } else {
      flags |= kWebTextInputFlagAutocapitalizeSentences;
    }
  } else {
    NOTREACHED_IN_MIGRATION();
  }

  return flags;
}

SuggestionMarker::SuggestionType ConvertImeTextSpanType(
    ImeTextSpan::Type type) {
  switch (type) {
    case ImeTextSpan::Type::kAutocorrect:
      return SuggestionMarker::SuggestionType::kAutocorrect;
    case ImeTextSpan::Type::kGrammarSuggestion:
      return SuggestionMarker::SuggestionType::kGrammar;
    case ImeTextSpan::Type::kMisspellingSuggestion:
      return SuggestionMarker::SuggestionType::kMisspelling;
    case ImeTextSpan::Type::kComposition:
    case ImeTextSpan::Type::kSuggestion:
      return SuggestionMarker::SuggestionType::kNotMisspelling;
  }
}

ImeTextSpan::Type ConvertSuggestionMarkerType(
    SuggestionMarker::SuggestionType type) {
  switch (type) {
    case SuggestionMarker::SuggestionType::kAutocorrect:
      return ImeTextSpan::Type::kAutocorrect;
    case SuggestionMarker::SuggestionType::kGrammar:
      return ImeTextSpan::Type::kGrammarSuggestion;
    case SuggestionMarker::SuggestionType::kMisspelling:
      return ImeTextSpan::Type::kMisspellingSuggestion;
    case SuggestionMarker::SuggestionType::kNotMisspelling:
      return ImeTextSpan::Type::kSuggestion;
  }
}

// ImeTextSpans types that need to be provided to TextInputInfo can be added
// here.
bool ShouldGetImeTextSpans(ImeTextSpan::Type type) {
  return type == ImeTextSpan::Type::kAutocorrect ||
         type == ImeTextSpan::Type::kGrammarSuggestion;
}

}  // anonymous namespace

enum class InputMethodController::TypingContinuation { kContinue, kEnd };

InputMethodController::InputMethodController(LocalDOMWindow& window,
                                             LocalFrame& frame)
    : ExecutionContextLifecycleObserver(&window),
      frame_(frame),
      has_composition_(false),
      last_vk_visibility_request_(
          ui::mojom::VirtualKeyboardVisibilityRequest::NONE) {}

InputMethodController::~InputMethodController() = default;

bool InputMethodController::IsAvailable() const {
  return GetExecutionContext();
}

Document& InputMethodController::GetDocument() const {
  DCHECK(IsAvailable());
  return *To<LocalDOMWindow>(GetExecutionContext())->document();
}

bool InputMethodController::HasComposition() const {
  return has_composition_ && !composition_range_->collapsed() &&
         composition_range_->IsConnected();
}

inline Editor& InputMethodController::GetEditor() const {
  return GetFrame().GetEditor();
}

LocalFrame& InputMethodController::GetFrame() const {
  return *frame_;
}

void InputMethodController::DispatchCompositionUpdateEvent(LocalFrame& frame,
                                                           const String& text) {
  Element* target = frame.GetDocument()->FocusedElement();
  if (!target)
    return;

  auto* event = MakeGarbageCollected<CompositionEvent>(
      event_type_names::kCompositionupdate, frame.DomWindow(), text);
  target->DispatchEvent(*event);
}

void InputMethodController::DispatchCompositionEndEvent(LocalFrame& frame,
                                                        const String& text) {
  // Verify that the caller is using an EventQueueScope to suppress the input
  // event from being fired until the proper time (e.g. after applying an IME
  // selection update, if necessary).
  DCHECK(ScopedEventQueue::Instance()->ShouldQueueEvents());

  Element* target = frame.GetDocument()->FocusedElement();
  if (!target)
    return;

  auto* event = MakeGarbageCollected<CompositionEvent>(
      event_type_names::kCompositionend, frame.DomWindow(), text);
  EventDispatcher::DispatchScopedEvent(*target, *event);
}

void InputMethodController::DispatchBeforeInputFromComposition(
    EventTarget* target,
    InputEvent::InputType input_type,
    const String& data) {
  if (!target)
    return;
  // TODO(editing-dev): Pass appropriate |ranges| after it's defined on spec.
  // http://w3c.github.io/editing/input-events.html#dom-inputevent-inputtype
  const StaticRangeVector* ranges = nullptr;
  if (auto* node = target->ToNode())
    ranges = TargetRangesForInputEvent(*node);
  InputEvent* before_input_event = InputEvent::CreateBeforeInput(
      input_type, data, InputEvent::EventIsComposing::kIsComposing, ranges);
  target->DispatchEvent(*before_input_event);
}

// Used to insert/replace text during composition update and confirm
// composition.
// Procedure:
//   1. Fire 'beforeinput' event for (TODO(editing-dev): deleted composed text)
//      and inserted text
//   2. Fire 'compositionupdate' event
//   3. Fire TextEvent and modify DOM
//   4. Fire 'input' event; dispatched by Editor::AppliedEditing()
void InputMethodController::InsertTextDuringCompositionWithEvents(
    LocalFrame& frame,
    const String& text,
    TypingCommand::Options options,
    TypingCommand::TextCompositionType composition_type) {
  // Verify that the caller is using an EventQueueScope to suppress the input
  // event from being fired until the proper time (e.g. after applying an IME
  // selection update, if necessary).
  DCHECK(ScopedEventQueue::Instance()->ShouldQueueEvents());
  DCHECK(composition_type ==
             TypingCommand::TextCompositionType::kTextCompositionUpdate ||
         composition_type ==
             TypingCommand::TextCompositionType::kTextCompositionConfirm ||
         composition_type ==
             TypingCommand::TextCompositionType::kTextCompositionCancel)
      << "compositionType should be TextCompositionUpdate or "
         "TextCompositionConfirm  or TextCompositionCancel, but got "
      << static_cast<int>(composition_type);
  if (!frame.GetDocument())
    return;

  Element* target = frame.GetDocument()->FocusedElement();
  if (!target)
    return;

  DispatchCompositionUpdateEvent(frame, text);
  // 'compositionupdate' event handler may destroy document.
  if (!IsAvailable()) {
    return;
  }

  DispatchBeforeInputFromComposition(
      target, InputEvent::InputType::kInsertCompositionText, text);

  // 'beforeinput' event handler may destroy document.
  if (!IsAvailable())
    return;

  // TODO(editing-dev): The use of UpdateStyleAndLayout
  // needs to be audited. see http://crbug.com/590369 for more details.
  frame.GetDocument()->UpdateStyleAndLayout(DocumentUpdateReason::kInput);

  const bool is_incremental_insertion = NeedsIncrementalInsertion(frame, text);

  switch (composition_type) {
    case TypingCommand::TextCompositionType::kTextCompositionUpdate:
    case TypingCommand::TextCompositionType::kTextCompositionConfirm:
      // Calling |TypingCommand::insertText()| with empty text will result in an
      // incorrect ending selection. We need to delete selection first.
      // https://crbug.com/693481
      if (text.empty())
        TypingCommand::DeleteSelection(*frame.GetDocument(), 0);
      frame.GetDocument()->UpdateStyleAndLayout(DocumentUpdateReason::kEditing);
      TypingCommand::InsertText(*frame.GetDocument(), text, options,
                                composition_type, is_incremental_insertion);
      break;
    case TypingCommand::TextCompositionType::kTextCompositionCancel:
      // TODO(editing-dev): Use TypingCommand::insertText after TextEvent was
      // removed. (Removed from spec since 2012)
      // See text_event.idl.
      frame.GetEventHandler().HandleTextInputEvent(text, nullptr,
                                                   kTextEventInputComposition);
      break;
    default:
      NOTREACHED_IN_MIGRATION();
  }
}

void InputMethodController::Clear() {
  RemoveSuggestionMarkerInCompositionRange();

  has_composition_ = false;
  if (composition_range_) {
    composition_range_->setStart(&GetDocument(), 0);
    composition_range_->collapse(true);
  }
  GetDocument().Markers().RemoveMarkersOfTypes(
      DocumentMarker::MarkerTypes::Composition());
}

void InputMethodController::ClearImeTextSpansByType(ImeTextSpan::Type type,
                                                    unsigned text_start,
                                                    unsigned text_end) {
  Element* target = GetDocument().FocusedElement();
  if (!target)
    return;

  Element* editable = GetFrame()
                          .Selection()
                          .ComputeVisibleSelectionInDOMTree()
                          .RootEditableElement();
  if (!editable)
    return;

  const EphemeralRange range =
      PlainTextRange(text_start, text_end).CreateRange(*editable);
  if (range.IsNull() ||
      RootEditableElementOf(range.StartPosition()) != editable ||
      RootEditableElementOf(range.EndPosition()) != editable) {
    return;
  }

  switch (type) {
    case ImeTextSpan::Type::kAutocorrect:
    case ImeTextSpan::Type::kGrammarSuggestion:
    case ImeTextSpan::Type::kMisspellingSuggestion:
    case ImeTextSpan::Type::kSuggestion:
      GetDocument().Markers().RemoveSuggestionMarkerByType(
          ToEphemeralRangeInFlatTree(range), ConvertImeTextSpanType(type));
      break;
    case ImeTextSpan::Type::kComposition:
      GetDocument().Markers().RemoveMarkersInRange(
          range, DocumentMarker::MarkerTypes::Composition());
      break;
  }
}

void InputMethodController::ContextDestroyed() {
  Clear();
  composition_range_ = nullptr;
  active_edit_context_ = nullptr;
}

void InputMethodController::SelectComposition() const {
  const EphemeralRange range = CompositionEphemeralRange();
  if (range.IsNull())
    return;

  // When we select the composition (to be able to replace it), we must not
  // claim that the selection is the result of an input event, even though
  // the act of committing the composition _is_ an input event in itself.
  // Otherwise, X11 clients would interpret the selection as a command to
  // replace the primary selection (on the clipboard) with the contents
  // of the composition.
  bool old_handling_input_event = false;
  WebFrameWidget* widget = nullptr;
  if (GetFrame().Client() && GetFrame().Client()->GetWebFrame()) {
    widget = GetFrame().Client()->GetWebFrame()->FrameWidget();
  }
  if (widget) {
    old_handling_input_event = widget->HandlingInputEvent();
    widget->SetHandlingInputEvent(false);
  }

  // The composition can start inside a composed character sequence, so we have
  // to override checks. See <http://bugs.webkit.org/show_bug.cgi?id=15781>

  // The SetSelectionOptions() parameter is necessary because without it,
  // FrameSelection::SetSelection() will actually call
  // SetShouldClearTypingStyle(true), which will cause problems applying
  // formatting during composition. See https://crbug.com/803278.
  GetFrame().Selection().SetSelection(
      SelectionInDOMTree::Builder().SetBaseAndExtent(range).Build(),
      SetSelectionOptions());

  if (widget) {
    widget->SetHandlingInputEvent(old_handling_input_event);
  }
}

bool IsTextTooLongAt(const Position& position) {
  const Element* element = EnclosingTextControl(position);
  if (!element)
    return false;
  if (auto* input = DynamicTo<HTMLInputElement>(element))
    return input->TooLong();
  if (auto* textarea = DynamicTo<HTMLTextAreaElement>(element))
    return textarea->TooLong();
  return false;
}

bool InputMethodController::FinishComposingText(
    ConfirmCompositionBehavior confirm_behavior) {
  if (!HasComposition())
    return false;

  // If text is longer than maxlength, give input/textarea's handler a chance to
  // clamp the text by replacing the composition with the same value.
  const bool is_too_long = IsTextTooLongAt(composition_range_->StartPosition());

  // TODO(editing-dev): Use of UpdateStyleAndLayout
  // needs to be audited. see http://crbug.com/590369 for more details.
  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kEditing);
  const String& composing = ComposingText();

  // Suppress input event (if we hit the is_too_long case) and compositionend
  // event until after we restore the original selection (to avoid clobbering a
  // selection update applied by an event handler).
  EventQueueScope scope;

  if (confirm_behavior == kKeepSelection) {
    // Do not dismiss handles even if we are moving selection, because we will
    // eventually move back to the old selection offsets.
    const bool is_handle_visible = GetFrame().Selection().IsHandleVisible();

    // Maintain to direction of the original selection as it affects how the
    // selection can be extended.
    const PlainTextRange& old_offsets = GetSelectionOffsets();
    const bool is_forward_selection = GetFrame()
                                          .Selection()
                                          .ComputeVisibleSelectionInDOMTree()
                                          .IsAnchorFirst();
    RevealSelectionScope reveal_selection_scope(GetFrame());

    if (is_too_long) {
      std::ignore = ReplaceComposition(ComposingText());
    } else {
      Clear();
      DispatchCompositionEndEvent(GetFrame(), composing);
    }

    // TODO(editing-dev): Use of updateStyleAndLayout
    // needs to be audited. see http://crbug.com/590369 for more details.
    GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kEditing);

    const EphemeralRange& old_selection_range =
        EphemeralRangeForOffsets(old_offsets);
    if (old_selection_range.IsNull())
      return false;
    const SelectionInDOMTree& selection =
        is_forward_selection ? SelectionInDOMTree::Builder()
                                   .SetAsForwardSelection(old_selection_range)
                                   .Build()
                             : SelectionInDOMTree::Builder()
                                   .SetAsBackwardSelection(old_selection_range)
                                   .Build();
    GetFrame().Selection().SetSelection(
        selection, SetSelectionOptions::Builder()
                       .SetShouldCloseTyping(true)
                       .SetShouldShowHandle(is_handle_visible)
                       .Build());
    return true;
  }

  PlainTextRange composition_range =
      PlainTextRangeForEphemeralRange(CompositionEphemeralRange()).second;
  if (composition_range.IsNull())
    return false;

  if (is_too_long) {
    // Don't move caret or dispatch compositionend event if
    // ReplaceComposition() fails.
    if (!ReplaceComposition(ComposingText()))
      return false;
  } else {
    Clear();
    DispatchCompositionEndEvent(GetFrame(), composing);
  }

  // Note: MoveCaret() occurs *before* the input and compositionend events are
  // dispatched, due to the use of ScopedEventQueue. This allows input and
  // compositionend event handlers to change the current selection without
  // it getting overwritten again.
  return MoveCaret(composition_range.End());
}

bool InputMethodController::CommitText(
    const String& text,
    const Vector<ImeTextSpan>& ime_text_spans,
    int relative_caret_position) {
  if (HasComposition()) {
    return ReplaceCompositionAndMoveCaret(text, relative_caret_position,
                                          ime_text_spans);
  }

  return InsertTextAndMoveCaret(text, relative_caret_position, ime_text_spans);
}

bool InputMethodController::ReplaceTextAndMoveCaret(
    const String& text,
    PlainTextRange range,
    MoveCaretBehavior move_caret_behavior) {
  EventQueueScope scope;
  const PlainTextRange old_selection(GetSelectionOffsets());
  if (!SetSelectionOffsets(range))
    return false;
  if (!InsertText(text))
    return false;

  // TODO(editing-dev): The use of UpdateStyleAndLayout
  // needs to be audited.  see http://crbug.com/590369 for more details.
  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kEditing);
  switch (move_caret_behavior) {
    case MoveCaretBehavior::kMoveCaretAfterText: {
      wtf_size_t absolute_caret_position = range.Start() + text.length();
      return SetSelectionOffsets(
          {absolute_caret_position, absolute_caret_position});
    }
    case MoveCaretBehavior::kDoNotMove: {
      wtf_size_t selection_delta = text.length() - range.length();
      wtf_size_t start = old_selection.Start();
      wtf_size_t end = old_selection.End();
      return SetSelectionOffsets(
          {start >= range.End() ? start + selection_delta : start,
           end >= range.End() ? end + selection_delta : end});
    }
  }
}

bool InputMethodController::ReplaceComposition(const String& text) {
  // Verify that the caller is using an EventQueueScope to suppress the input
  // event from being fired until the proper time (e.g. after applying an IME
  // selection update, if necessary).
  DCHECK(ScopedEventQueue::Instance()->ShouldQueueEvents());

  if (!HasComposition())
    return false;

  // Select the text that will be deleted or replaced.
  SelectComposition();

  if (GetFrame().Selection().ComputeVisibleSelectionInDOMTree().IsNone()) {
    return false;
  }

  if (!IsAvailable())
    return false;

  Clear();

  InsertTextDuringCompositionWithEvents(
      GetFrame(), text, 0,
      TypingCommand::TextCompositionType::kTextCompositionConfirm);

  // textInput event handler might destroy document (input event is queued
  // until later).
  if (!IsAvailable())
    return false;

  // No DOM update after 'compositionend'.
  DispatchCompositionEndEvent(GetFrame(), text);

  return true;
}

// relativeCaretPosition is relative to the end of the text.
static int ComputeAbsoluteCaretPosition(int text_start,
                                        int text_length,
                                        int relative_caret_position) {
  return text_start + text_length + relative_caret_position;
}

void InputMethodController::AddImeTextSpans(
    const Vector<ImeTextSpan>& ime_text_spans,
    ContainerNode* base_element,
    unsigned offset_in_plain_chars) {
  for (const auto& ime_text_span : ime_text_spans) {
    wtf_size_t ime_text_span_start =
        offset_in_plain_chars + ime_text_span.StartOffset();
    wtf_size_t ime_text_span_end =
        offset_in_plain_chars + ime_text_span.EndOffset();

    EphemeralRange ephemeral_line_range =
        PlainTextRange(ime_text_span_start, ime_text_span_end)
            .CreateRange(*base_element);
    if (ephemeral_line_range.IsNull())
      continue;

    switch (ime_text_span.GetType()) {
      case ImeTextSpan::Type::kComposition: {
        ImeTextSpanUnderlineStyle underline_style =
            ime_text_span.InterimCharSelection()
                ? ImeTextSpanUnderlineStyle::kSolid
                : ime_text_span.UnderlineStyle();
        GetDocument().Markers().AddCompositionMarker(
            ephemeral_line_range, ime_text_span.UnderlineColor(),
            ime_text_span.Thickness(), underline_style,
            ime_text_span.TextColor(), ime_text_span.BackgroundColor());
        break;
      }
      case ImeTextSpan::Type::kAutocorrect:
      case ImeTextSpan::Type::kGrammarSuggestion:
      case ImeTextSpan::Type::kSuggestion:
      case ImeTextSpan::Type::kMisspellingSuggestion:
        const SuggestionMarker::SuggestionType suggestion_type =
            ConvertImeTextSpanType(ime_text_span.GetType());

        // If spell-checking is disabled for an element, we ignore suggestion
        // markers used to mark misspelled words, but allow other ones (e.g.,
        // markers added by an IME to allow picking between multiple possible
        // words, none of which is necessarily misspelled).
        if (suggestion_type == SuggestionMarker::SuggestionType::kMisspelling &&
            !SpellChecker::IsSpellCheckingEnabledAt(
                ephemeral_line_range.StartPosition()))
          continue;

        // Do not add the grammar marker if it overlaps with existing spellcheck
        // markers.
        if (suggestion_type == SuggestionMarker::SuggestionType::kGrammar &&
            !GetDocument()
                 .Markers()
                 .MarkersIntersectingRange(
                     ToEphemeralRangeInFlatTree(ephemeral_line_range),
                     DocumentMarker::MarkerTypes::Spelling())
                 .empty()) {
          continue;
        }

        GetDocument().Markers().AddSuggestionMarker(
            ephemeral_line_range,
            SuggestionMarkerProperties::Builder()
                .SetType(suggestion_type)
                .SetSuggestions(ime_text_span.Suggestions())
                .SetHighlightColor(ime_text_span.SuggestionHighlightColor())
                .SetUnderlineColor(ime_text_span.UnderlineColor())
                .SetThickness(ime_text_span.Thickness())
                .SetUnderlineStyle(ime_text_span.UnderlineStyle())
                .SetTextColor(ime_text_span.TextColor())
                .SetBackgroundColor(ime_text_span.BackgroundColor())
                .SetRemoveOnFinishComposing(
                    ime_text_span.NeedsRemovalOnFinishComposing())
                .Build());
        break;
    }
  }
}

bool InputMethodController::ReplaceCompositionAndMoveCaret(
    const String& text,
    int relative_caret_position,
    const Vector<ImeTextSpan>& ime_text_spans) {
  Element* root_editable_element = GetFrame()
                                       .Selection()
                                       .ComputeVisibleSelectionInDOMTree()
                                       .RootEditableElement();
  if (!root_editable_element)
    return false;
  DCHECK(HasComposition());
  PlainTextRange composition_range =
      PlainTextRange::Create(*root_editable_element, *composition_range_);
  if (composition_range.IsNull())
    return false;
  int text_start = composition_range.Start();

  // Suppress input and compositionend events until after we move the caret to
  // the new position.
  EventQueueScope scope;
  if (!ReplaceComposition(text))
    return false;

  // TODO(editing-dev): The use of UpdateStyleAndLayout
  // needs to be audited. see http://crbug.com/590369 for more details.
  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kEditing);

  AddImeTextSpans(ime_text_spans, root_editable_element, text_start);

  int absolute_caret_position = ComputeAbsoluteCaretPosition(
      text_start, text.length(), relative_caret_position);
  return MoveCaret(absolute_caret_position);
}

bool InputMethodController::InsertText(const String& text) {
  if (DispatchBeforeInputInsertText(GetDocument().FocusedElement(), text) !=
      DispatchEventResult::kNotCanceled)
    return false;
  if (!IsAvailable())
    return false;
  GetEditor().InsertText(text, nullptr);
  return true;
}

bool InputMethodController::InsertTextAndMoveCaret(
    const String& text,
    int relative_caret_position,
    const Vector<ImeTextSpan>& ime_text_spans) {
  PlainTextRange selection_range = GetSelectionOffsets();
  if (selection_range.IsNull())
    return false;
  int text_start = selection_range.Start();

  // Suppress input event until after we move the caret to the new position.
  EventQueueScope scope;

  // Don't fire events for a no-op operation.
  if (!text.empty() || selection_range.length() > 0) {
    if (!InsertText(text))
      return false;
  }

  // TODO(editing-dev): The use of UpdateStyleAndLayout
  // needs to be audited. see http://crbug.com/590369 for more details.
  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kEditing);
  Element* root_editable_element = GetFrame()
                                       .Selection()
                                       .ComputeVisibleSelectionInDOMTree()
                                       .RootEditableElement();
  if (root_editable_element) {
    AddImeTextSpans(ime_text_spans, root_editable_element, text_start);
  }

  int absolute_caret_position = ComputeAbsoluteCaretPosition(
      text_start, text.length(), relative_caret_position);
  return MoveCaret(absolute_caret_position);
}

void InputMethodController::CancelComposition() {
  if (!HasComposition())
    return;

  // TODO(editing-dev): Use of UpdateStyleAndLayout
  // needs to be audited. see http://crbug.com/590369 for more details.
  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kEditing);

  RevealSelectionScope reveal_selection_scope(GetFrame());

  if (GetFrame().Selection().ComputeVisibleSelectionInDOMTree().IsNone()) {
    return;
  }

  Clear();

  InsertTextDuringCompositionWithEvents(
      GetFrame(), g_empty_string, 0,
      TypingCommand::TextCompositionType::kTextCompositionCancel);
  // Event handler might destroy document.
  if (!IsAvailable())
    return;

  // An open typing command that disagrees about current selection would cause
  // issues with typing later on.
  TypingCommand::CloseTyping(&GetFrame());

  // No DOM update after 'compositionend'.
  DispatchCompositionEndEvent(GetFrame(), g_empty_string);
}

bool InputMethodController::DispatchCompositionStartEvent(const String& text) {
  Element* target = GetDocument().FocusedElement();
  if (!target)
    return IsAvailable();

  auto* event = MakeGarbageCollected<CompositionEvent>(
      event_type_names::kCompositionstart, GetFrame().DomWindow(), text);
  target->DispatchEvent(*event);

  return IsAvailable();
}

void InputMethodController::SetComposition(
    const String& text,
    const Vector<ImeTextSpan>& ime_text_spans,
    int selection_start,
    int selection_end) {
  RevealSelectionScope reveal_selection_scope(GetFrame());

  // Updates styles before setting selection for composition to prevent
  // inserting the previous composition text into text nodes oddly.
  // See https://bugs.webkit.org/show_bug.cgi?id=46868
  GetDocument().UpdateStyleAndLayoutTree();

  SelectComposition();

  // TODO(editing-dev): The use of UpdateStyleAndLayout
  // needs to be audited. see http://crbug.com/590369 for more details.
  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kEditing);

  if (GetFrame().Selection().ComputeVisibleSelectionInDOMTree().IsNone()) {
    return;
  }

  Element* target = GetDocument().FocusedElement();
  if (!target)
    return;

  PlainTextRange selected_range = CreateSelectionRangeForSetComposition(
      selection_start, selection_end, text.length());

  // Dispatch an appropriate composition event to the focused node.
  // We check the composition status and choose an appropriate composition event
  // since this function is used for three purposes:
  // 1. Starting a new composition.
  //    Send a compositionstart and a compositionupdate event when this function
  //    creates a new composition node, i.e. !hasComposition() &&
  //    !text.isEmpty().
  //    Sending a compositionupdate event at this time ensures that at least one
  //    compositionupdate event is dispatched.
  // 2. Updating the existing composition node.
  //    Send a compositionupdate event when this function updates the existing
  //    composition node, i.e. hasComposition() && !text.isEmpty().
  // 3. Canceling the ongoing composition.
  //    Send a compositionend event when function deletes the existing
  //    composition node, i.e. !hasComposition() && test.isEmpty().
  if (text.empty()) {
    // Suppress input and compositionend events until after we move the caret
    // to the new position.
    EventQueueScope scope;
    if (HasComposition()) {
      RevealSelectionScope inner_reveal_selection_scope(GetFrame());
      // Do not attempt to apply IME selection offsets if ReplaceComposition()
      // fails (we compute the new range assuming the replacement will succeed).
      if (!ReplaceComposition(g_empty_string))
        return;
    } else {
      // It's weird to call |setComposition()| with empty text outside
      // composition, however some IME (e.g. Japanese IBus-Anthy) did this, so
      // we simply delete selection without sending extra events.
      if (!DeleteSelection())
        return;
    }

    // TODO(editing-dev): Use of UpdateStyleAndLayout
    // needs to be audited. see http://crbug.com/590369 for more details.
    GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kEditing);

    SetEditableSelectionOffsets(selected_range);
    return;
  }

  // We should send a 'compositionstart' event only when the given text is not
  // empty because this function doesn't create a composition node when the text
  // is empty.
  if (!HasComposition() &&
      !DispatchCompositionStartEvent(GetFrame().SelectedText())) {
    return;
  }

  DCHECK(!text.empty());

  Clear();

  // Suppress input event until after we move the caret to the new position.
  EventQueueScope scope;
  InsertTextDuringCompositionWithEvents(GetFrame(), text,
                                        TypingCommand::kSelectInsertedText,
                                        TypingCommand::kTextCompositionUpdate);
  // Event handlers might destroy document.
  if (!IsAvailable())
    return;

  // TODO(editing-dev): The use of UpdateStyleAndLayout
  // needs to be audited. see http://crbug.com/590369 for more details.
  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kEditing);

  // The undo stack could become empty if a JavaScript event handler calls
  // execCommand('undo') to pop elements off the stack. Or, the top element of
  // the stack could end up not corresponding to the TypingCommand. Make sure we
  // don't crash in these cases (it's unclear what the composition range should
  // be set to in these cases, so we don't worry too much about that).
  SelectionInDOMTree selection;
  if (GetEditor().GetUndoStack().CanUndo()) {
    const UndoStep* undo_step = *GetEditor().GetUndoStack().UndoSteps().begin();
    const SelectionForUndoStep& undo_selection = undo_step->EndingSelection();
    if (undo_selection.IsValidFor(GetDocument()))
      selection = undo_selection.AsSelection();
  }

  // Find out what node has the composition now.
  const Position anchor =
      MostForwardCaretPosition(selection.Anchor(), kCanSkipOverEditingBoundary);
  Node* anchor_node = anchor.AnchorNode();
  if (!anchor_node || !anchor_node->IsTextNode()) {
    return;
  }

  const Position focus = selection.Focus();
  Node* focus_node = focus.AnchorNode();

  unsigned focus_offset = focus.ComputeOffsetInContainerNode();
  unsigned anchor_offset = anchor.ComputeOffsetInContainerNode();

  has_composition_ = true;
  if (!composition_range_)
    composition_range_ = Range::Create(GetDocument());
  composition_range_->setStart(anchor_node, anchor_offset);
  composition_range_->setEnd(focus_node, focus_offset);

  if (anchor_node->GetLayoutObject()) {
    anchor_node->GetLayoutObject()->SetShouldDoFullPaintInvalidation();
  }

  // TODO(editing-dev): The use of UpdateStyleAndLayout
  // needs to be audited. see http://crbug.com/590369 for more details.
  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kEditing);

  // We shouldn't close typing in the middle of setComposition.
  SetEditableSelectionOffsets(selected_range, TypingContinuation::kContinue);

  if (TypingCommand* const last_typing_command =
          TypingCommand::LastTypingCommandIfStillOpenForTyping(&GetFrame())) {
    // When we called InsertTextDuringCompositionWithEvents() with the
    // kSelectInsertedText flag, it set what is now the composition range as the
    // ending selection on the open TypingCommand. We now update it to the
    // current selection to fix two problems:
    //
    // 1. Certain operations, e.g. pressing enter on a physical keyboard on
    // Android, would otherwise incorrectly replace the composition range.
    //
    // 2. Using undo would cause text to be selected, even though we never
    // actually showed the selection to the user.
    TypingCommand::UpdateSelectionIfDifferentFromCurrentSelection(
        last_typing_command, &GetFrame());
  }

  // Even though we would've returned already if SetComposition() were called
  // with an empty string, the composition range could still be empty right now
  // due to Unicode grapheme cluster position normalization (e.g. if
  // SetComposition() were passed an extending character which doesn't allow a
  // grapheme cluster break immediately before.
  if (!HasComposition())
    return;

  if (ime_text_spans.empty()) {
    GetDocument().Markers().AddCompositionMarker(
        CompositionEphemeralRange(), Color::kTransparent,
        ui::mojom::ImeTextSpanThickness::kThin,
        ui::mojom::ImeTextSpanUnderlineStyle::kSolid, Color::kTransparent,
        LayoutTheme::GetTheme().PlatformDefaultCompositionBackgroundColor());
    return;
  }

  const std::pair<ContainerNode*, PlainTextRange>&
      root_element_and_plain_text_range =
          PlainTextRangeForEphemeralRange(CompositionEphemeralRange());
  AddImeTextSpans(ime_text_spans, root_element_and_plain_text_range.first,
                  root_element_and_plain_text_range.second.Start());
}

PlainTextRange InputMethodController::CreateSelectionRangeForSetComposition(
    int selection_start,
    int selection_end,
    size_t text_length) const {
  const int selection_offsets_start =
      static_cast<int>(GetSelectionOffsets().Start());
  const int start = selection_offsets_start + selection_start;
  const int end = selection_offsets_start + selection_end;
  return CreateRangeForSelection(start, end, text_length);
}

void InputMethodController::SetCompositionFromExistingText(
    const Vector<ImeTextSpan>& ime_text_spans,
    unsigned composition_start,
    unsigned composition_end) {
  Element* target = GetDocument().FocusedElement();
  if (!target)
    return;

  if (!HasComposition() && !DispatchCompositionStartEvent(""))
    return;

  // TODO(editing-dev): The use of UpdateStyleAndLayout
  // needs to be audited.  see http://crbug.com/590369 for more details.
  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kEditing);
  Element* editable = GetFrame()
                          .Selection()
                          .ComputeVisibleSelectionInDOMTree()
                          .RootEditableElement();
  if (!editable)
    return;

  const EphemeralRange range =
      PlainTextRange(composition_start, composition_end).CreateRange(*editable);
  if (range.IsNull())
    return;

  const Position start = range.StartPosition();
  if (RootEditableElementOf(start) != editable)
    return;

  const Position end = range.EndPosition();
  if (RootEditableElementOf(end) != editable)
    return;

  Clear();

  AddImeTextSpans(ime_text_spans, editable, composition_start);

  has_composition_ = true;
  if (!composition_range_)
    composition_range_ = Range::Create(GetDocument());
  composition_range_->setStart(range.StartPosition());
  composition_range_->setEnd(range.EndPosition());

  DispatchCompositionUpdateEvent(GetFrame(), ComposingText());
}

void InputMethodController::AddImeTextSpansToExistingText(
    const Vector<ImeTextSpan>& ime_text_spans,
    unsigned text_start,
    unsigned text_end) {
  Element* target = GetDocument().FocusedElement();
  if (!target)
    return;

  Element* editable = GetFrame()
                          .Selection()
                          .ComputeVisibleSelectionInDOMTree()
                          .RootEditableElement();
  if (!editable)
    return;

  const EphemeralRange range =
      PlainTextRange(text_start, text_end).CreateRange(*editable);
  if (range.IsNull() ||
      RootEditableElementOf(range.StartPosition()) != editable ||
      RootEditableElementOf(range.EndPosition()) != editable) {
    return;
  }

  AddImeTextSpans(ime_text_spans, editable, text_start);
}

EphemeralRange InputMethodController::CompositionEphemeralRange() const {
  if (!HasComposition())
    return EphemeralRange();
  return EphemeralRange(composition_range_.Get());
}

String InputMethodController::ComposingText() const {
  DocumentLifecycle::DisallowTransitionScope disallow_transition(
      GetDocument().Lifecycle());
  return PlainText(
      CompositionEphemeralRange(),
      TextIteratorBehavior::Builder().SetEmitsOriginalText(true).Build());
}

PlainTextRange InputMethodController::GetSelectionOffsets() const {
  const EphemeralRange range = FirstEphemeralRangeOf(
      GetFrame().Selection().ComputeVisibleSelectionInDOMTree());
  if (range.IsNull())
    return PlainTextRange();
  const ContainerNode& element =
      *RootEditableElementOrTreeScopeRootNodeOf(range.StartPosition());
  cached_text_input_info_.EnsureCached(element);
  return cached_text_input_info_.GetSelection(range);
}

EphemeralRange InputMethodController::EphemeralRangeForOffsets(
    const PlainTextRange& offsets) const {
  if (offsets.IsNull())
    return EphemeralRange();
  Element* root_editable_element = GetFrame()
                                       .Selection()
                                       .ComputeVisibleSelectionInDOMTree()
                                       .RootEditableElement();
  if (!root_editable_element)
    return EphemeralRange();

  return offsets.CreateRange(*root_editable_element);
}

bool InputMethodController::SetSelectionOffsets(
    const PlainTextRange& selection_offsets) {
  return SetSelectionOffsets(selection_offsets, TypingContinuation::kEnd,
                             /*show_handle=*/false,
                             /*show_context_menu=*/false);
}

bool InputMethodController::SetSelectionOffsets(
    const PlainTextRange& selection_offsets,
    TypingContinuation typing_continuation,
    bool show_handle,
    bool show_context_menu) {
  const EphemeralRange range = EphemeralRangeForOffsets(selection_offsets);
  if (range.IsNull())
    return false;

  GetFrame().Selection().SetSelection(
      SelectionInDOMTree::Builder().SetBaseAndExtent(range).Build(),
      SetSelectionOptions::Builder()
          .SetShouldCloseTyping(typing_continuation == TypingContinuation::kEnd)
          .SetShouldShowHandle(show_handle)
          .Build());

  if (show_context_menu) {
    ContextMenuAllowedScope scope;
    GetFrame().GetEventHandler().ShowNonLocatedContextMenu(
        /*override_target_element=*/nullptr, kMenuSourceTouch);
  }
  return true;
}

bool InputMethodController::SetEditableSelectionOffsets(
    const PlainTextRange& selection_offsets,
    bool show_handle,
    bool show_context_menu) {
  return SetEditableSelectionOffsets(selection_offsets,
                                     TypingContinuation::kEnd, show_handle,
                                     show_context_menu);
}

bool InputMethodController::SetEditableSelectionOffsets(
    const PlainTextRange& selection_offsets,
    TypingContinuation typing_continuation,
    bool show_handle,
    bool show_context_menu) {
  if (!GetEditor().CanEdit())
    return false;

  return SetSelectionOffsets(selection_offsets, typing_continuation,
                             show_handle, show_context_menu);
}

void InputMethodController::RemoveSuggestionMarkerInCompositionRange() {
  if (HasComposition()) {
    GetDocument().Markers().RemoveSuggestionMarkerInRangeOnFinish(
        EphemeralRangeInFlatTree(composition_range_.Get()));
  }
}

PlainTextRange InputMethodController::CreateRangeForSelection(
    int start,
    int end,
    size_t text_length) const {
  // In case of exceeding the left boundary.
  start = std::max(start, 0);
  end = std::max(end, start);

  Element* root_editable_element = GetFrame()
                                       .Selection()
                                       .ComputeVisibleSelectionInDOMTree()
                                       .RootEditableElement();
  if (!root_editable_element)
    return PlainTextRange();
  const EphemeralRange& range =
      EphemeralRange::RangeOfContents(*root_editable_element);
  if (range.IsNull())
    return PlainTextRange();

  const TextIteratorBehavior& behavior =
      TextIteratorBehavior::Builder()
          .SetEmitsObjectReplacementCharacter(true)
          .SetEmitsCharactersBetweenAllVisiblePositions(true)
          .Build();
  TextIterator it(range.StartPosition(), range.EndPosition(), behavior);

  int right_boundary = 0;
  for (; !it.AtEnd(); it.Advance())
    right_boundary += it.length();

  if (HasComposition())
    right_boundary -= composition_range_->GetText().length();

  right_boundary += text_length;

  // In case of exceeding the right boundary.
  start = std::min(start, right_boundary);
  end = std::min(end, right_boundary);

  return PlainTextRange(start, end);
}

bool InputMethodController::DeleteSelection() {
  if (!GetFrame().Selection().ComputeVisibleSelectionInDOMTree().IsRange())
    return true;

  Node* target = GetFrame().GetDocument()->FocusedElement();
  if (target) {
    DispatchBeforeInputEditorCommand(
        target, InputEvent::InputType::kDeleteContentBackward,
        TargetRangesForInputEvent(*target));

    // Frame could have been destroyed by the beforeinput event.
    if (!IsAvailable())
      return false;
  }

  // TODO(editing-dev): The use of UpdateStyleAndLayout
  // needs to be audited.  see http://crbug.com/590369 for more details.
  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kEditing);
  TypingCommand::DeleteSelection(GetDocument());

  // Frame could have been destroyed by the input event.
  return IsAvailable();
}

bool InputMethodController::DeleteSelectionWithoutAdjustment() {
  const SelectionInDOMTree& selection_in_dom_tree =
      GetFrame().Selection().GetSelectionInDOMTree();
  if (selection_in_dom_tree.IsCaret())
    return true;

  const SelectionForUndoStep& selection =
      SelectionForUndoStep::From(selection_in_dom_tree);

  Node* target = GetFrame().GetDocument()->FocusedElement();
  if (target) {
    DispatchBeforeInputEditorCommand(
        target, InputEvent::InputType::kDeleteContentBackward,
        TargetRangesForInputEvent(*target));
    // Frame could have been destroyed by the beforeinput event.
    if (!IsAvailable())
      return false;
  }

  if (TypingCommand* last_typing_command =
          TypingCommand::LastTypingCommandIfStillOpenForTyping(&GetFrame())) {
    TypingCommand::UpdateSelectionIfDifferentFromCurrentSelection(
        last_typing_command, &GetFrame());

    last_typing_command->DeleteSelection(true, ASSERT_NO_EDITING_ABORT);
    return true;
  }

  MakeGarbageCollected<DeleteSelectionCommand>(
      selection,
      DeleteSelectionOptions::Builder()
          .SetMergeBlocksAfterDelete(true)
          .SetSanitizeMarkup(true)
          .Build(),
      InputEvent::InputType::kDeleteContentBackward)
      ->Apply();

  // Frame could have been destroyed by the input event.
  return IsAvailable();
}

bool InputMethodController::MoveCaret(int new_caret_position) {
  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kEditing);
  PlainTextRange selected_range =
      CreateRangeForSelection(new_caret_position, new_caret_position, 0);
  if (selected_range.IsNull())
    return false;
  return SetEditableSelectionOffsets(selected_range);
}

void InputMethodController::ExtendSelectionAndDelete(int before, int after) {
  if (!GetEditor().CanEdit())
    return;
  PlainTextRange selection_offsets(GetSelectionOffsets());
  if (selection_offsets.IsNull())
    return;

  // A common call of before=1 and after=0 will fail if the last character
  // is multi-code-word UTF-16, including both multi-16bit code-points and
  // Unicode combining character sequences of multiple single-16bit code-
  // points (officially called "compositions"). Try more until success.
  // http://crbug.com/355995
  //
  // FIXME: Note that this is not an ideal solution when this function is
  // called to implement "backspace". In that case, there should be some call
  // that will not delete a full multi-code-point composition but rather
  // only the last code-point so that it's possible for a user to correct
  // a composition without starting it from the beginning.
  // http://crbug.com/37993
  do {
    if (!SetSelectionOffsets(PlainTextRange(
            std::max(static_cast<int>(selection_offsets.Start()) - before, 0),
            selection_offsets.End() + after)))
      return;
    if (before == 0)
      break;
    ++before;
  } while (
      GetFrame().Selection().ComputeVisibleSelectionInDOMTree().Start() ==
          GetFrame().Selection().ComputeVisibleSelectionInDOMTree().End() &&
      before <= static_cast<int>(selection_offsets.Start()));
  // TODO(editing-dev): Find a way to distinguish Forward and Backward.
  std::ignore = DeleteSelection();
}

// TODO(ctzsm): We should reduce the number of selectionchange events.
// Ideally, we want to do the deletion without selection, however, there is no
// such editing command exists currently.
void InputMethodController::DeleteSurroundingText(int before, int after) {
  if (!GetEditor().CanEdit())
    return;
  const PlainTextRange selection_offsets(GetSelectionOffsets());
  if (selection_offsets.IsNull())
    return;
  Element* const root_editable_element = GetFrame()
                                             .Selection()
                                             .ComputeVisibleSelectionInDOMTree()
                                             .RootEditableElement();
  if (!root_editable_element)
    return;
  int selection_start = static_cast<int>(selection_offsets.Start());
  int selection_end = static_cast<int>(selection_offsets.End());

  // Select the text to be deleted before SelectionState::kStart.
  if (before > 0 && selection_start > 0) {
    // In case of exceeding the left boundary.
    const int start = std::max(selection_start - before, 0);
    const EphemeralRange& range =
        PlainTextRange(0, start).CreateRange(*root_editable_element);
    if (range.IsNull())
      return;
    if (!SetSelectionOffsets(PlainTextRange(start, selection_start)))
      return;
    if (!DeleteSelectionWithoutAdjustment())
      return;

    selection_end = selection_end - (selection_start - start);
    selection_start = start;
  }

  // Select the text to be deleted after SelectionState::kEnd.
  if (after > 0) {
    // Adjust the deleted range in case of exceeding the right boundary.
    const PlainTextRange range(0, selection_end + after);
    if (range.IsNull())
      return;
    const EphemeralRange& valid_range =
        range.CreateRange(*root_editable_element);
    if (valid_range.IsNull())
      return;
    const int end =
        PlainTextRange::Create(*root_editable_element, valid_range).End();

    // TODO(editing-dev): The use of UpdateStyleAndLayout
    // needs to be audited.  see http://crbug.com/590369 for more details.
    GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kEditing);
    if (!SetSelectionOffsets(PlainTextRange(selection_end, end)))
      return;
    if (!DeleteSelectionWithoutAdjustment())
      return;
  }

  // TODO(editing-dev): The use of UpdateStyleAndLayout
  // needs to be audited.  see http://crbug.com/590369 for more details.
  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kEditing);
  SetSelectionOffsets(PlainTextRange(selection_start, selection_end));
}

void InputMethodController::DeleteSurroundingTextInCodePoints(int before,
                                                              int after) {
  DCHECK_GE(before, 0);
  DCHECK_GE(after, 0);
  if (!GetEditor().CanEdit())
    return;
  const PlainTextRange selection_offsets(GetSelectionOffsets());
  if (selection_offsets.IsNull())
    return;
  Element* const root_editable_element =
      GetFrame().Selection().RootEditableElementOrDocumentElement();
  if (!root_editable_element)
    return;

  const TextIteratorBehavior& behavior =
      TextIteratorBehavior::Builder()
          .SetEmitsObjectReplacementCharacter(true)
          .Build();
  const String& text = PlainText(
      EphemeralRange::RangeOfContents(*root_editable_element), behavior);

  // 8-bit characters are Latin-1 characters, so the deletion lengths are
  // trivial.
  if (text.Is8Bit())
    return DeleteSurroundingText(before, after);

  const int selection_start = static_cast<int>(selection_offsets.Start());
  const int selection_end = static_cast<int>(selection_offsets.End());

  const int before_length =
      CalculateBeforeDeletionLengthsInCodePoints(text, before, selection_start);
  if (IsInvalidDeletionLength(before_length))
    return;
  const int after_length =
      CalculateAfterDeletionLengthsInCodePoints(text, after, selection_end);
  if (IsInvalidDeletionLength(after_length))
    return;

  return DeleteSurroundingText(before_length, after_length);
}

void InputMethodController::ExtendSelectionAndReplace(
    int before,
    int after,
    const String& replacement_text) {
  const PlainTextRange selection_offsets(GetSelectionOffsets());
  if (selection_offsets.IsNull() || before < 0 || after < 0) {
    return;
  }

  ReplaceTextAndMoveCaret(
      replacement_text,
      PlainTextRange(
          std::max(static_cast<int>(selection_offsets.Start()) - before, 0),
          selection_offsets.End() + after),
      MoveCaretBehavior::kMoveCaretAfterText);
}

void InputMethodController::GetLayoutBounds(gfx::Rect* control_bounds,
                                            gfx::Rect* selection_bounds) {
  if (!IsAvailable())
    return;

  if (GetActiveEditContext()) {
    return GetActiveEditContext()->GetLayoutBounds(control_bounds,
                                                   selection_bounds);
  }
  if (!GetFrame().Selection().IsAvailable())
    return;
  Element* element = RootEditableElementOfSelection(GetFrame().Selection());
  if (!element)
    return;
  // Fetch the control bounds of the active editable element.
  // Selection bounds are currently populated only for EditContext.
  // For editable elements we use GetCompositionCharacterBounds to fetch the
  // selection bounds.
  *control_bounds = element->BoundsInWidget();
}

void InputMethodController::DidChangeVisibility(
    const LayoutObject& layout_object) {
  cached_text_input_info_.DidChangeVisibility(layout_object);
}

void InputMethodController::DidLayoutSubtree(
    const LayoutObject& layout_object) {
  cached_text_input_info_.DidLayoutSubtree(layout_object);
}

void InputMethodController::DidUpdateLayout(const LayoutObject& layout_object) {
  cached_text_input_info_.DidUpdateLayout(layout_object);
}

void InputMethodController::LayoutObjectWillBeDestroyed(
    const LayoutObject& layout_object) {
  cached_text_input_info_.LayoutObjectWillBeDestroyed(layout_object);
}

WebTextInputInfo InputMethodController::TextInputInfo() const {
  WebTextInputInfo info;
  if (!IsAvailable())
    return info;

  if (!GetFrame().Selection().IsAvailable()) {
    // plugins/mouse-capture-inside-shadow.html reaches here.
    return info;
  }
  Element* element = RootEditableElementOfSelection(GetFrame().Selection());
  if (!element)
    return info;

  info.node_id = NodeIdOfFocusedElement();
  info.action = InputActionOfFocusedElement();
  info.input_mode = InputModeOfFocusedElement();
  info.virtual_keyboard_policy = VirtualKeyboardPolicyOfFocusedElement();
  info.type = TextInputType();
  info.flags = TextInputFlags();
  if (info.type == kWebTextInputTypeNone)
    return info;

  if (!GetFrame().GetEditor().CanEdit())
    return info;

  // TODO(editing-dev): The use of UpdateStyleAndLayout
  // needs to be audited.  see http://crbug.com/590369 for more details.
  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kEditing);
  const EphemeralRange& first_range = FirstEphemeralRangeOf(
      GetFrame().Selection().ComputeVisibleSelectionInDOMTree());

  DocumentLifecycle::DisallowTransitionScope disallow_transition(
      GetDocument().Lifecycle());

  if (const Node* start_node = first_range.StartPosition().AnchorNode()) {
    if (start_node->GetComputedStyle() &&
        !start_node->GetComputedStyle()->IsHorizontalWritingMode()) {
      info.flags |= kWebTextInputFlagVertical;
    }
  }

  cached_text_input_info_.EnsureCached(*element);

  // Emits an object replacement character for each replaced element so that
  // it is exposed to IME and thus could be deleted by IME on android.
  info.value = cached_text_input_info_.GetText();
  if (info.value.IsEmpty())
    return info;

  const PlainTextRange& selection_plain_text_range =
      cached_text_input_info_.GetSelection(first_range);
  if (selection_plain_text_range.IsNotNull()) {
    info.selection_start = selection_plain_text_range.Start();
    info.selection_end = selection_plain_text_range.End();
  }

  info.ime_text_spans = GetImeTextSpans();

  const EphemeralRange& range = CompositionEphemeralRange();
  const PlainTextRange& composition_plain_text_range =
      cached_text_input_info_.GetComposition(range);
  if (composition_plain_text_range.IsNotNull()) {
    info.composition_start = composition_plain_text_range.Start();
    info.composition_end = composition_plain_text_range.End();
  }

  return info;
}

int InputMethodController::TextInputFlags() const {
  Element* element = GetDocument().FocusedElement();
  if (!element)
    return kWebTextInputFlagNone;

  int flags = 0;

  const AtomicString& autocomplete =
      element->FastGetAttribute(html_names::kAutocompleteAttr);
  if (autocomplete == keywords::kOn) {
    flags |= kWebTextInputFlagAutocompleteOn;
  } else if (autocomplete == keywords::kOff) {
    flags |= kWebTextInputFlagAutocompleteOff;
  }

  const AtomicString& autocorrect =
      element->FastGetAttribute(html_names::kAutocorrectAttr);
  if (autocorrect == keywords::kOn) {
    flags |= kWebTextInputFlagAutocorrectOn;
  } else if (autocorrect == keywords::kOff) {
    flags |= kWebTextInputFlagAutocorrectOff;
  }

  SpellcheckAttributeState spellcheck = element->GetSpellcheckAttributeState();
  if (spellcheck == kSpellcheckAttributeTrue)
    flags |= kWebTextInputFlagSpellcheckOn;
  else if (spellcheck == kSpellcheckAttributeFalse)
    flags |= kWebTextInputFlagSpellcheckOff;

  flags |= ComputeAutocapitalizeFlags(element);

  if (auto* input = DynamicTo<HTMLInputElement>(element)) {
    if (input->HasBeenPasswordField())
      flags |= kWebTextInputFlagHasBeenPasswordField;
  }

  return flags;
}

int InputMethodController::ComputeWebTextInputNextPreviousFlags() const {
  if (!IsAvailable())
    return kWebTextInputFlagNone;

  Element* const element = GetDocument().FocusedElement();
  if (!element)
    return kWebTextInputFlagNone;

  Page* page = GetDocument().GetPage();
  if (!page)
    return kWebTextInputFlagNone;

  int flags = kWebTextInputFlagNone;
  if (page->GetFocusController().NextFocusableElementForImeAndAutofill(
          element, mojom::blink::FocusType::kForward)) {
    flags |= kWebTextInputFlagHaveNextFocusableElement;
  }

  if (page->GetFocusController().NextFocusableElementForImeAndAutofill(
          element, mojom::blink::FocusType::kBackward)) {
    flags |= kWebTextInputFlagHavePreviousFocusableElement;
  }

  return flags;
}

ui::TextInputAction InputMethodController::InputActionOfFocusedElement() const {
  AtomicString action =
      GetEnterKeyHintAttribute(GetDocument().FocusedElement());

  if (action.empty())
    return ui::TextInputAction::kDefault;
  if (action == keywords::kEnter)
    return ui::TextInputAction::kEnter;
  if (action == keywords::kDone)
    return ui::TextInputAction::kDone;
  if (action == keywords::kGo)
    return ui::TextInputAction::kGo;
  if (action == keywords::kNext)
    return ui::TextInputAction::kNext;
  if (action == keywords::kPrevious)
    return ui::TextInputAction::kPrevious;
  if (action == keywords::kSearch)
    return ui::TextInputAction::kSearch;
  if (action == keywords::kSend)
    return ui::TextInputAction::kSend;
  return ui::TextInputAction::kDefault;
}

WebTextInputMode InputMethodController::InputModeOfFocusedElement() const {
  AtomicString mode = GetInputModeAttribute(GetDocument().FocusedElement());

  if (mode.empty())
    return kWebTextInputModeDefault;
  if (mode == keywords::kNone)
    return kWebTextInputModeNone;
  if (mode == keywords::kText)
    return kWebTextInputModeText;
  if (mode == keywords::kTel)
    return kWebTextInputModeTel;
  if (mode == keywords::kUrl)
    return kWebTextInputModeUrl;
  if (mode == keywords::kEmail)
    return kWebTextInputModeEmail;
  if (mode == keywords::kNumeric)
    return kWebTextInputModeNumeric;
  if (mode == keywords::kDecimal)
    return kWebTextInputModeDecimal;
  if (mode == keywords::kSearch)
    return kWebTextInputModeSearch;
  return kWebTextInputModeDefault;
}

ui::mojom::VirtualKeyboardPolicy
InputMethodController::VirtualKeyboardPolicyOfFocusedElement() const {
  // Return the default value if ExecutionContext is not defined.
  if (!IsAvailable())
    return ui::mojom::VirtualKeyboardPolicy::AUTO;

  AtomicString vk_policy =
      GetVirtualKeyboardPolicyAttribute(GetDocument().FocusedElement());

  if (vk_policy == keywords::kManual)
    return ui::mojom::VirtualKeyboardPolicy::MANUAL;
  return ui::mojom::VirtualKeyboardPolicy::AUTO;
}

void InputMethodController::SetVirtualKeyboardVisibilityRequest(
    ui::mojom::VirtualKeyboardVisibilityRequest vk_visibility_request) {
  // show/hide API behavior is only applicable for elements/editcontexts that
  // have manual VK policy.
  if (VirtualKeyboardPolicyOfFocusedElement() ==
      ui::mojom::VirtualKeyboardPolicy::MANUAL) {
    last_vk_visibility_request_ = vk_visibility_request;
  }  // else we don't change the last VK visibility request.
}

DOMNodeId InputMethodController::NodeIdOfFocusedElement() const {
  Element* element = GetDocument().FocusedElement();
  return element ? element->GetDomNodeId() : kInvalidDOMNodeId;
}

WebTextInputType InputMethodController::TextInputType() const {
  if (!IsAvailable()) {
    return kWebTextInputTypeNone;
  }

  // Since selection can never go inside a <canvas> element, if the user is
  // editing inside a <canvas> with EditContext we need to handle that case
  // directly before looking at the selection position.
  if (GetActiveEditContext()) {
    Element* element = GetDocument().FocusedElement();
    if (IsA<HTMLCanvasElement>(element)) {
      return kWebTextInputTypeContentEditable;
    }
  }

  if (!GetFrame().Selection().IsAvailable()) {
    // "mouse-capture-inside-shadow.html" reaches here.
    return kWebTextInputTypeNone;
  }

  // It's important to preserve the equivalence of textInputInfo().type and
  // textInputType(), so perform the same rootEditableElement() existence check
  // here for consistency.
  if (!RootEditableElementOfSelection(GetFrame().Selection()))
    return kWebTextInputTypeNone;

  Element* element = GetDocument().FocusedElement();
  if (!element) {
    return kWebTextInputTypeNone;
  }

  if (auto* input = DynamicTo<HTMLInputElement>(*element)) {
    FormControlType type = input->FormControlType();

    if (input->IsDisabledOrReadOnly())
      return kWebTextInputTypeNone;

    switch (type) {
      case FormControlType::kInputPassword:
        return kWebTextInputTypePassword;
      case FormControlType::kInputSearch:
        return kWebTextInputTypeSearch;
      case FormControlType::kInputEmail:
        return kWebTextInputTypeEmail;
      case FormControlType::kInputNumber:
        return kWebTextInputTypeNumber;
      case FormControlType::kInputTelephone:
        return kWebTextInputTypeTelephone;
      case FormControlType::kInputUrl:
        return kWebTextInputTypeURL;
      case FormControlType::kInputText:
        return kWebTextInputTypeText;
      default:
        return kWebTextInputTypeNone;
    }
  }

  if (auto* textarea = DynamicTo<HTMLTextAreaElement>(*element)) {
    if (textarea->IsDisabledOrReadOnly())
      return kWebTextInputTypeNone;
    return kWebTextInputTypeTextArea;
  }

  if (auto* html_element = DynamicTo<HTMLElement>(element)) {
    if (html_element->IsDateTimeFieldElement())
      return kWebTextInputTypeDateTimeField;
  }

  GetDocument().UpdateStyleAndLayoutTree();
  if (IsEditable(*element))
    return kWebTextInputTypeContentEditable;

  return kWebTextInputTypeNone;
}

void InputMethodController::WillChangeFocus() {
  FinishComposingText(kKeepSelection);

  // Event handler might destroy document.
  if (!IsAvailable())
    return;

  GetDocument().Markers().RemoveSuggestionMarkerByType(
      SuggestionMarker::SuggestionType::kAutocorrect);
}

void InputMethodController::Trace(Visitor* visitor) const {
  visitor->Trace(cached_text_input_info_);
  visitor->Trace(frame_);
  visitor->Trace(composition_range_);
  visitor->Trace(active_edit_context_);
  ExecutionContextLifecycleObserver::Trace(visitor);
}

WebVector<ui::ImeTextSpan> InputMethodController::GetImeTextSpans() const {
  DCHECK(!GetDocument().NeedsLayoutTreeUpdate());
  Element* target = GetDocument().FocusedElement();
  if (!target)
    return WebVector<ui::ImeTextSpan>();

  Element* editable = GetFrame()
                          .Selection()
                          .ComputeVisibleSelectionInDOMTree()
                          .RootEditableElement();
  if (!editable)
    return WebVector<ui::ImeTextSpan>();

  WebVector<ui::ImeTextSpan> ime_text_spans;

  const EphemeralRange range = EphemeralRange::RangeOfContents(*editable);
  if (range.IsNull())
    return WebVector<ui::ImeTextSpan>();

  // MarkersIntersectingRange() might be expensive. In practice, we hope we will
  // only check one node for the range.
  const HeapVector<std::pair<Member<const Text>, Member<DocumentMarker>>>&
      node_marker_pairs = GetDocument().Markers().MarkersIntersectingRange(
          ToEphemeralRangeInFlatTree(range),
          DocumentMarker::MarkerTypes::Suggestion());

  for (const std::pair<Member<const Text>, Member<DocumentMarker>>&
           node_marker_pair : node_marker_pairs) {
    SuggestionMarker* marker =
        To<SuggestionMarker>(node_marker_pair.second.Get());
    ImeTextSpan::Type type =
        ConvertSuggestionMarkerType(marker->GetSuggestionType());
    if (ShouldGetImeTextSpans(type)) {
      const Text* node = node_marker_pair.first;
      const EphemeralRange& marker_ephemeral_range =
          EphemeralRange(Position(node, marker->StartOffset()),
                         Position(node, marker->EndOffset()));
      const PlainTextRange& marker_plain_text_range =
          cached_text_input_info_.GetPlainTextRange(marker_ephemeral_range);

      ime_text_spans.emplace_back(
          ImeTextSpan(type, marker_plain_text_range.Start(),
                      marker_plain_text_range.End(), Color::kTransparent,
                      ImeTextSpanThickness::kNone,
                      ImeTextSpanUnderlineStyle::kNone, Color::kTransparent,
                      Color::kTransparent, Color::kTransparent, false, false,
                      marker->Suggestions())
              .ToUiImeTextSpan());
    }
  }

  return ime_text_spans;
}

}  // namespace blink
