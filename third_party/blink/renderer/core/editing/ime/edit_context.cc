// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/editing/ime/edit_context.h"

#include "base/containers/contains.h"
#include "base/ranges/algorithm.h"
#include "base/trace_event/trace_event.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/platform/web_vector.h"
#include "third_party/blink/public/web/web_range.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_edit_context_enter_key_hint.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_edit_context_init.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_edit_context_input_mode.h"
#include "third_party/blink/renderer/core/css/css_color.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/editing/ime/character_bounds_update_event.h"
#include "third_party/blink/renderer/core/editing/ime/input_method_controller.h"
#include "third_party/blink/renderer/core/editing/ime/text_format.h"
#include "third_party/blink/renderer/core/editing/ime/text_format_update_event.h"
#include "third_party/blink/renderer/core/editing/ime/text_update_event.h"
#include "third_party/blink/renderer/core/editing/state_machines/backward_grapheme_boundary_state_machine.h"
#include "third_party/blink/renderer/core/editing/state_machines/forward_grapheme_boundary_state_machine.h"
#include "third_party/blink/renderer/core/event_target_names.h"
#include "third_party/blink/renderer/core/events/composition_event.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/geometry/dom_rect.h"
#include "third_party/blink/renderer/platform/text/text_boundaries.h"
#include "third_party/blink/renderer/platform/wtf/decimal.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"
#include "ui/base/ime/ime_text_span.h"

namespace blink {

EditContext::EditContext(ScriptState* script_state, const EditContextInit* dict)
    : ActiveScriptWrappable<EditContext>({}),
      ExecutionContextClient(ExecutionContext::From(script_state)) {
  DCHECK(IsMainThread());

  if (dict->hasText())
    text_ = dict->text();

  if (dict->hasSelectionStart())
    selection_start_ = std::min(dict->selectionStart(), text_.length());

  if (dict->hasSelectionEnd())
    selection_end_ = std::min(dict->selectionEnd(), text_.length());

  if (dict->hasInputMode())
    setInputMode(dict->inputMode());

  if (dict->hasEnterKeyHint())
    setEnterKeyHint(dict->enterKeyHint());

  if (dict->hasInputPanelPolicy())
    setInputPanelPolicy(dict->inputPanelPolicy());
}

EditContext* EditContext::Create(ScriptState* script_state,
                                 const EditContextInit* dict) {
  return MakeGarbageCollected<EditContext>(script_state, dict);
}

EditContext::~EditContext() = default;

const AtomicString& EditContext::InterfaceName() const {
  return event_target_names::kEditContext;
}

ExecutionContext* EditContext::GetExecutionContext() const {
  return ExecutionContextClient::GetExecutionContext();
}

bool EditContext::HasPendingActivity() const {
  return GetExecutionContext() && HasEventListeners();
}

InputMethodController& EditContext::GetInputMethodController() const {
  return DomWindow()->GetFrame()->GetInputMethodController();
}

bool EditContext::IsEditContextActive() const {
  return true;
}

ui::mojom::VirtualKeyboardVisibilityRequest
EditContext::GetLastVirtualKeyboardVisibilityRequest() const {
  return GetInputMethodController().GetLastVirtualKeyboardVisibilityRequest();
}

void EditContext::SetVirtualKeyboardVisibilityRequest(
    ui::mojom::VirtualKeyboardVisibilityRequest vk_visibility_request) {
  GetInputMethodController().SetVirtualKeyboardVisibilityRequest(
      vk_visibility_request);
}

bool EditContext::IsVirtualKeyboardPolicyManual() const {
  return GetInputMethodController()
             .GetActiveEditContext()
             ->inputPanelPolicy() == "manual";
}

void EditContext::DispatchCompositionEndEvent(const String& text) {
  auto* event = MakeGarbageCollected<CompositionEvent>(
      event_type_names::kCompositionend, DomWindow(), text);
  DispatchEvent(*event);
}

bool EditContext::DispatchCompositionStartEvent(const String& text) {
  auto* event = MakeGarbageCollected<CompositionEvent>(
      event_type_names::kCompositionstart, DomWindow(), text);
  DispatchEvent(*event);
  return DomWindow();
}

void EditContext::DispatchCharacterBoundsUpdateEvent(uint32_t range_start,
                                                     uint32_t range_end) {
  auto* event = MakeGarbageCollected<CharacterBoundsUpdateEvent>(
      event_type_names::kCharacterboundsupdate, range_start, range_end);
  DispatchEvent(*event);
}

void EditContext::DispatchTextUpdateEvent(const String& text,
                                          uint32_t update_range_start,
                                          uint32_t update_range_end,
                                          uint32_t new_selection_start,
                                          uint32_t new_selection_end) {
  TextUpdateEvent* event = MakeGarbageCollected<TextUpdateEvent>(
      event_type_names::kTextupdate, text, update_range_start, update_range_end,
      new_selection_start, new_selection_end);
  DispatchEvent(*event);
}

void EditContext::DispatchTextFormatEvent(
    const WebVector<ui::ImeTextSpan>& ime_text_spans) {
  // Loop through IME text spans to prepare an array of TextFormat and
  // fire textformateupdate event.
  DCHECK(has_composition_);
  HeapVector<Member<TextFormat>> text_formats;
  text_formats.reserve(base::checked_cast<wtf_size_t>(ime_text_spans.size()));

  for (const auto& ime_text_span : ime_text_spans) {
    const auto range_start = base::checked_cast<wtf_size_t>(
        ime_text_span.start_offset + composition_range_start_);
    const auto range_end = base::checked_cast<wtf_size_t>(
        ime_text_span.end_offset + composition_range_start_);

    String underline_thickness;
    String underline_style;
    switch (ime_text_span.thickness) {
      case ui::ImeTextSpan::Thickness::kNone:
        underline_thickness = "None";
        break;
      case ui::ImeTextSpan::Thickness::kThin:
        underline_thickness = "Thin";
        break;
      case ui::ImeTextSpan::Thickness::kThick:
        underline_thickness = "Thick";
        break;
    }
    switch (ime_text_span.underline_style) {
      case ui::ImeTextSpan::UnderlineStyle::kNone:
        underline_style = "None";
        break;
      case ui::ImeTextSpan::UnderlineStyle::kSolid:
        underline_style = "Solid";
        break;
      case ui::ImeTextSpan::UnderlineStyle::kDot:
        underline_style = "Dotted";
        break;
      case ui::ImeTextSpan::UnderlineStyle::kDash:
        underline_style = "Dashed";
        break;
      case ui::ImeTextSpan::UnderlineStyle::kSquiggle:
        underline_style = "Squiggle";
        break;
    }

    text_formats.push_back(TextFormat::Create(
        range_start, range_end,
        cssvalue::CSSColor::SerializeAsCSSComponentValue(
            Color::FromSkColor(ime_text_span.text_color)),
        cssvalue::CSSColor::SerializeAsCSSComponentValue(
            Color::FromSkColor(ime_text_span.background_color)),
        cssvalue::CSSColor::SerializeAsCSSComponentValue(
            Color::FromSkColor(ime_text_span.underline_color)),
        underline_style, underline_thickness));
  }

  TextFormatUpdateEvent* event = MakeGarbageCollected<TextFormatUpdateEvent>(
      event_type_names::kTextformatupdate, text_formats);
  DispatchEvent(*event);
}

void EditContext::Focus() {
  TRACE_EVENT0("ime", "EditContext::Focus");

  EditContext* current_active_edit_context =
      GetInputMethodController().GetActiveEditContext();
  if (current_active_edit_context && current_active_edit_context != this) {
    // Reset the state of the EditContext if there is
    // an active composition in progress.
    current_active_edit_context->FinishComposingText(
        ConfirmCompositionBehavior::kKeepSelection);
  }
  GetInputMethodController().SetActiveEditContext(this);
}

void EditContext::Blur() {
  TRACE_EVENT0("ime", "EditContext::Blur");

  if (GetInputMethodController().GetActiveEditContext() != this)
    return;
  // Clean up the state of the |this| EditContext.
  FinishComposingText(ConfirmCompositionBehavior::kKeepSelection);
  GetInputMethodController().SetActiveEditContext(nullptr);
}

void EditContext::updateSelection(uint32_t start,
                                  uint32_t end,
                                  ExceptionState& exception_state) {
  TRACE_EVENT2("ime", "EditContext::updateSelection", "start",
               std::to_string(start), "end", std::to_string(end));

  // Following this spec:
  // https://html.spec.whatwg.org/C/#dom-textarea/input-setselectionrange
  if (start > end) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kIndexSizeError,
        "The provided start value (" + String::Number(start) +
            ") is larger than the provided end value (" + String::Number(end) +
            ").");
    return;
  }
  end = std::min(end, text_.length());
  start = std::min(start, end);
  selection_start_ = start;
  selection_end_ = end;
  if (!has_composition_)
    return;

  // There is an active composition so need to set the range of the
  // composition too so that we can commit the string properly.
  if (composition_range_start_ == 0 && composition_range_end_ == 0) {
    composition_range_start_ = selection_start_;
    composition_range_end_ = selection_end_;
  }
}

void EditContext::updateCharacterBounds(
    unsigned long range_start,
    HeapVector<Member<DOMRect>>& character_bounds) {
  character_bounds_range_start_ = static_cast<uint32_t>(range_start);

  TRACE_EVENT1("ime", "EditContext::updateCharacterBounds", "range_start, size",
               std::to_string(range_start) + ", " +
                   std::to_string(character_bounds.size()));

  character_bounds_.Clear();
  base::ranges::for_each(character_bounds, [this](const auto& bounds) {
    auto result_bounds = bounds->ToEnclosingRect();
    TRACE_EVENT1("ime", "EditContext::updateCharacterBounds", "charBounds",
                 result_bounds.ToString());
    character_bounds_.push_back(result_bounds);
  });
}

void EditContext::updateControlBounds(DOMRect* control_bounds) {
  TRACE_EVENT1("ime", "EditContext::updateControlBounds", "control_bounds",
               control_bounds->ToEnclosingRect().ToString());
  control_bounds_ = control_bounds->ToEnclosingRect();
}

void EditContext::updateSelectionBounds(DOMRect* selection_bounds) {
  TRACE_EVENT1("ime", "EditContext::updateSelectionBounds", "selection_bounds",
               selection_bounds->ToEnclosingRect().ToString());
  selection_bounds_ = selection_bounds->ToEnclosingRect();
}

void EditContext::updateText(uint32_t start,
                             uint32_t end,
                             const String& new_text,
                             ExceptionState& exception_state) {
  TRACE_EVENT2("ime", "EditContext::updateText", "start, end",
               std::to_string(start) + ", " + std::to_string(end), "new_text",
               new_text);

  // Following this spec:
  // https://html.spec.whatwg.org/C/#dom-textarea/input-setrangetext
  if (start > end) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kIndexSizeError,
        "The provided start value (" + String::Number(start) +
            ") is larger than the provided end value (" + String::Number(end) +
            ").");
    return;
  }
  end = std::min(end, text_.length());
  start = std::min(start, end);
  text_ = text_.Substring(0, start) + new_text + text_.Substring(end);
}

String EditContext::text() const {
  return text_;
}

void EditContext::setText(const String& text) {
  TRACE_EVENT1("ime", "EditContext::setText", "text", text);
  text_ = text;
}

uint32_t EditContext::selectionStart() const {
  return selection_start_;
}

void EditContext::setSelectionStart(uint32_t selection_start,
                                    ExceptionState& exception_state) {
  TRACE_EVENT1("ime", "EditContext::setSelectionStart", "start",
               std::to_string(selection_start));
  // Following this spec:
  // https://html.spec.whatwg.org/C/#dom-textarea/input-setselectionrange
  selection_start_ = std::min(selection_end_, selection_start);
}

uint32_t EditContext::selectionEnd() const {
  return selection_end_;
}

uint32_t EditContext::characterBoundsRangeStart() const {
  return character_bounds_range_start_;
}

void EditContext::setSelectionEnd(uint32_t selection_end,
                                  ExceptionState& exception_state) {
  TRACE_EVENT1("ime", "EditContext::setSelectionEnd", "end",
               std::to_string(selection_end));

  // Following this spec:
  // https://html.spec.whatwg.org/C/#dom-textarea/input-setselectionrange
  selection_end_ = std::min(selection_end, text_.length());
}

V8EditContextInputPanelPolicy EditContext::inputPanelPolicy() const {
  return V8EditContextInputPanelPolicy(input_panel_policy_);
}

const HeapVector<Member<Element>>& EditContext::attachedElements() {
  return attached_elements_;
}

const HeapVector<Member<DOMRect>> EditContext::characterBounds() {
  HeapVector<Member<DOMRect>> dom_rects;
  base::ranges::transform(
      character_bounds_, std::back_inserter(dom_rects), [](const auto& bound) {
        return DOMRect::Create(bound.x(), bound.y(), bound.width(),
                               bound.height());
      });
  return dom_rects;
}

void EditContext::setInputPanelPolicy(
    const V8EditContextInputPanelPolicy& input_policy) {
  input_panel_policy_ = input_policy.AsEnum();
}

void EditContext::setInputMode(const V8EditContextInputMode& input_mode) {
  // inputMode password is not supported by browsers yet:
  // https://github.com/whatwg/html/issues/4875

  switch (input_mode.AsEnum()) {
    case V8EditContextInputMode::Enum::kText:
      input_mode_ = WebTextInputMode::kWebTextInputModeText;
      break;
    case V8EditContextInputMode::Enum::kTel:
      input_mode_ = WebTextInputMode::kWebTextInputModeTel;
      break;
    case V8EditContextInputMode::Enum::kEmail:
      input_mode_ = WebTextInputMode::kWebTextInputModeEmail;
      break;
    case V8EditContextInputMode::Enum::kSearch:
      input_mode_ = WebTextInputMode::kWebTextInputModeSearch;
      break;
    case V8EditContextInputMode::Enum::kDecimal:
      input_mode_ = WebTextInputMode::kWebTextInputModeDecimal;
      break;
    case V8EditContextInputMode::Enum::kNumeric:
      input_mode_ = WebTextInputMode::kWebTextInputModeNumeric;
      break;
    case V8EditContextInputMode::Enum::kUrl:
      input_mode_ = WebTextInputMode::kWebTextInputModeUrl;
      break;
    case V8EditContextInputMode::Enum::kPassword:
    case V8EditContextInputMode::Enum::kNone:
      input_mode_ = WebTextInputMode::kWebTextInputModeNone;
  }
}

V8EditContextInputMode EditContext::inputMode() const {
  switch (input_mode_) {
    case WebTextInputMode::kWebTextInputModeText:
      return V8EditContextInputMode(V8EditContextInputMode::Enum::kText);
    case WebTextInputMode::kWebTextInputModeSearch:
      return V8EditContextInputMode(V8EditContextInputMode::Enum::kSearch);
    case WebTextInputMode::kWebTextInputModeEmail:
      return V8EditContextInputMode(V8EditContextInputMode::Enum::kEmail);
    case WebTextInputMode::kWebTextInputModeDecimal:
      return V8EditContextInputMode(V8EditContextInputMode::Enum::kDecimal);
    case WebTextInputMode::kWebTextInputModeNumeric:
      return V8EditContextInputMode(V8EditContextInputMode::Enum::kNumeric);
    case WebTextInputMode::kWebTextInputModeTel:
      return V8EditContextInputMode(V8EditContextInputMode::Enum::kTel);
    case WebTextInputMode::kWebTextInputModeUrl:
      return V8EditContextInputMode(V8EditContextInputMode::Enum::kUrl);
    default:
      return V8EditContextInputMode(V8EditContextInputMode::Enum::kNone);
  }
}

void EditContext::setEnterKeyHint(
    const V8EditContextEnterKeyHint& enter_key_hint) {
  switch (enter_key_hint.AsEnum()) {
    case V8EditContextEnterKeyHint::Enum::kEnter:
      enter_key_hint_ = ui::TextInputAction::kEnter;
      break;
    case V8EditContextEnterKeyHint::Enum::kDone:
      enter_key_hint_ = ui::TextInputAction::kDone;
      break;
    case V8EditContextEnterKeyHint::Enum::kGo:
      enter_key_hint_ = ui::TextInputAction::kGo;
      break;
    case V8EditContextEnterKeyHint::Enum::kNext:
      enter_key_hint_ = ui::TextInputAction::kNext;
      break;
    case V8EditContextEnterKeyHint::Enum::kPrevious:
      enter_key_hint_ = ui::TextInputAction::kPrevious;
      break;
    case V8EditContextEnterKeyHint::Enum::kSearch:
      enter_key_hint_ = ui::TextInputAction::kSearch;
      break;
    case V8EditContextEnterKeyHint::Enum::kSend:
      enter_key_hint_ = ui::TextInputAction::kSend;
      break;
  }
}

V8EditContextEnterKeyHint EditContext::enterKeyHint() const {
  switch (enter_key_hint_) {
    case ui::TextInputAction::kEnter:
      return V8EditContextEnterKeyHint(V8EditContextEnterKeyHint::Enum::kEnter);
    case ui::TextInputAction::kDone:
      return V8EditContextEnterKeyHint(V8EditContextEnterKeyHint::Enum::kDone);
    case ui::TextInputAction::kGo:
      return V8EditContextEnterKeyHint(V8EditContextEnterKeyHint::Enum::kGo);
    case ui::TextInputAction::kNext:
      return V8EditContextEnterKeyHint(V8EditContextEnterKeyHint::Enum::kNext);
    case ui::TextInputAction::kPrevious:
      return V8EditContextEnterKeyHint(
          V8EditContextEnterKeyHint::Enum::kPrevious);
    case ui::TextInputAction::kSearch:
      return V8EditContextEnterKeyHint(
          V8EditContextEnterKeyHint::Enum::kSearch);
    case ui::TextInputAction::kSend:
      return V8EditContextEnterKeyHint(V8EditContextEnterKeyHint::Enum::kSend);
    default:
      // Defaulting to enter.
      return V8EditContextEnterKeyHint(V8EditContextEnterKeyHint::Enum::kEnter);
  }
}

void EditContext::GetLayoutBounds(gfx::Rect* control_bounds,
                                  gfx::Rect* selection_bounds) {
  // EditContext's coordinates are in CSS pixels, which need to be converted to
  // physical pixels before return.
  *control_bounds = gfx::ScaleToEnclosingRect(
      control_bounds_, DomWindow()->GetFrame()->DevicePixelRatio());
  *selection_bounds = gfx::ScaleToEnclosingRect(
      selection_bounds_, DomWindow()->GetFrame()->DevicePixelRatio());

  TRACE_EVENT2("ime", "EditContext::GetLayoutBounds", "control",
               control_bounds->ToString(), "selection",
               selection_bounds->ToString());
}

bool EditContext::SetComposition(
    const WebString& text,
    const WebVector<ui::ImeTextSpan>& ime_text_spans,
    const WebRange& replacement_range,
    int selection_start,
    int selection_end) {
  TRACE_EVENT2(
      "ime", "EditContext::SetComposition", "start, end",
      std::to_string(selection_start) + ", " + std::to_string(selection_end),
      "text", text.Utf8());

  if (!text.IsEmpty() && !has_composition_) {
    if (!DispatchCompositionStartEvent(text))
      return false;
    has_composition_ = true;
  }
  if (text.IsEmpty() && !has_composition_)
    return true;

  WebRange actual_replacement_range = replacement_range;
  if (actual_replacement_range.IsEmpty()) {
    // If no composition range, the current selection will be replaced.
    if (composition_range_start_ == 0 && composition_range_end_ == 0) {
      actual_replacement_range =
          WebRange(selection_start_, selection_end_ - selection_start_);
    }
    // Otherwise, the current composition range will be replaced.
    else {
      actual_replacement_range =
          WebRange(composition_range_start_,
                   composition_range_end_ - composition_range_start_);
    }
  }

  // Update the selection and buffer if the composition range has changed.
  String update_text(text);
  text_ = text_.Substring(0, actual_replacement_range.StartOffset()) +
          update_text + text_.Substring(actual_replacement_range.EndOffset());

  // Fire textupdate and textformatupdate events to JS.
  // Note selection_start_ is a global offset while selection_start is a local
  // offset computed from the begninning of the inserted string.
  selection_start_ = actual_replacement_range.StartOffset() + selection_start;
  selection_end_ = actual_replacement_range.StartOffset() + selection_end;
  DispatchTextUpdateEvent(update_text, actual_replacement_range.StartOffset(),
                          actual_replacement_range.EndOffset(),
                          selection_start_, selection_end_);

  composition_range_start_ = actual_replacement_range.StartOffset();
  composition_range_end_ =
      actual_replacement_range.StartOffset() + update_text.length();
  DispatchTextFormatEvent(ime_text_spans);
  DispatchCharacterBoundsUpdateEvent(composition_range_start_,
                                     composition_range_end_);
  return true;
}

bool EditContext::SetCompositionFromExistingText(
    int composition_start,
    int composition_end,
    const WebVector<ui::ImeTextSpan>& ime_text_spans) {
  TRACE_EVENT1("ime", "EditContext::SetCompositionFromExistingText",
               "start, end",
               std::to_string(composition_start) + ", " +
                   std::to_string(composition_end));

  if (composition_start < 0 || composition_end < 0)
    return false;

  if (!has_composition_) {
    if (!DispatchCompositionStartEvent(""))
      return false;
    has_composition_ = true;
  }
  // composition_start and composition_end offsets are relative to the current
  // composition unit which should be smaller than the text's length.
  composition_start =
      std::min(composition_start, static_cast<int>(text_.length()));
  composition_end = std::min(composition_end, static_cast<int>(text_.length()));
  String update_text(text_.Substring(composition_start, composition_end));
  text_ =
      text_.Substring(0, composition_start) + text_.Substring(composition_end);
  if (composition_range_start_ == 0 && composition_range_end_ == 0) {
    composition_range_start_ = composition_start;
    composition_range_end_ = composition_end;
  }

  DispatchTextUpdateEvent(update_text, composition_range_start_,
                          composition_range_end_, composition_start,
                          composition_start);
  DispatchTextFormatEvent(ime_text_spans);
  DispatchCharacterBoundsUpdateEvent(composition_range_start_,
                                     composition_range_end_);
  // Update the selection range.
  selection_start_ = composition_start;
  selection_end_ = composition_start;
  return true;
}

bool EditContext::InsertText(const WebString& text) {
  TRACE_EVENT1("ime", "EditContext::InsertText", "text", text.Utf8());

  String update_text(text);
  text_ = text_.Substring(0, selection_start_) + update_text +
          text_.Substring(selection_end_);
  uint32_t update_range_start = selection_start_;
  uint32_t update_range_end = selection_end_;
  selection_start_ = selection_start_ + update_text.length();
  selection_end_ = selection_start_;

  DispatchTextUpdateEvent(update_text, update_range_start, update_range_end,
                          selection_start_, selection_end_);
  return true;
}

void EditContext::DeleteCurrentSelection() {
  if (selection_start_ == selection_end_)
    return;

  StringBuilder stringBuilder;
  stringBuilder.Append(StringView(text_, 0, selection_start_));
  stringBuilder.Append(StringView(text_, selection_end_));
  text_ = stringBuilder.ToString();

  DispatchTextUpdateEvent(String(), selection_start_, selection_end_,
                          selection_start_, selection_start_);

  selection_end_ = selection_start_;
}

template <typename StateMachine>
int FindNextBoundaryOffset(const String& str, int current);

void EditContext::DeleteBackward() {
  // If the current selection is collapsed, delete one grapheme, otherwise,
  // delete whole selection.
  if (selection_start_ == selection_end_) {
    selection_start_ =
        FindNextBoundaryOffset<BackwardGraphemeBoundaryStateMachine>(
            text_, selection_start_);
  }

  DeleteCurrentSelection();
}

void EditContext::DeleteForward() {
  if (selection_start_ == selection_end_) {
    selection_end_ =
        FindNextBoundaryOffset<ForwardGraphemeBoundaryStateMachine>(
            text_, selection_start_);
  }

  DeleteCurrentSelection();
}

void EditContext::DeleteWordBackward() {
  if (selection_start_ == selection_end_) {
    String text16bit(text_);
    text16bit.Ensure16Bit();
    // TODO(shihken): implement platform behaviors when the spec is finalized.
    selection_start_ = FindNextWordBackward(text16bit.Characters16(),
                                            text16bit.length(), selection_end_);
  }

  DeleteCurrentSelection();
}

void EditContext::DeleteWordForward() {
  if (selection_start_ == selection_end_) {
    String text16bit(text_);
    text16bit.Ensure16Bit();
    // TODO(shihken): implement platform behaviors when the spec is finalized.
    selection_end_ = FindNextWordForward(text16bit.Characters16(),
                                         text16bit.length(), selection_start_);
  }

  DeleteCurrentSelection();
}

bool EditContext::CommitText(const WebString& text,
                             const WebVector<ui::ImeTextSpan>& ime_text_spans,
                             const WebRange& replacement_range,
                             int relative_caret_position) {
  TRACE_EVENT2("ime", "EditContext::CommitText", "range, ralative_caret",
               "(" + std::to_string(replacement_range.StartOffset()) + "," +
                   std::to_string(replacement_range.EndOffset()) + ")" + ", " +
                   std::to_string(relative_caret_position),
               "text", text.Utf8());

  // Fire textupdate and textformatupdate events to JS.
  // ime_text_spans can have multiple format updates so loop through and fire
  // events accordingly.
  // Update the cached selection too.
  String update_text(text);
  uint32_t update_range_start;
  uint32_t update_range_end;
  uint32_t new_selection_start;
  uint32_t new_selection_end;
  if (has_composition_) {
    text_ = text_.Substring(0, composition_range_start_) + update_text +
            text_.Substring(composition_range_end_);
    selection_start_ = composition_range_start_ + update_text.length();
    selection_end_ = composition_range_start_ + update_text.length();
    update_range_start = composition_range_start_;
    update_range_end = composition_range_end_;
  } else {
    text_ = text_.Substring(0, selection_start_) + update_text +
            text_.Substring(selection_end_);
    update_range_start = selection_start_;
    update_range_end = selection_end_;
    selection_start_ = selection_start_ + update_text.length();
    selection_end_ = selection_end_ + update_text.length();
  }
  new_selection_start = selection_start_;
  new_selection_end = selection_end_;
  composition_range_start_ = 0;
  composition_range_end_ = 0;
  DispatchTextUpdateEvent(update_text, update_range_start, update_range_end,
                          new_selection_start, new_selection_end);
  // Fire composition end event.
  if (!text.IsEmpty() && has_composition_)
    DispatchCompositionEndEvent(text);

  has_composition_ = false;
  return true;
}

bool EditContext::FinishComposingText(
    ConfirmCompositionBehavior selection_behavior) {
  TRACE_EVENT0("ime", "EditContext::FinishComposingText");

  String text;
  if (has_composition_) {
    text = text_.Substring(composition_range_start_, composition_range_end_);
    // Fire composition end event.
    DispatchCompositionEndEvent(text);
  } else {
    text = text_.Substring(selection_start_, selection_end_);
  }

  // TODO(snianu): also need to fire formatupdate here to remove formats from
  // the previous compositions?
  selection_start_ = selection_start_ + text.length();
  selection_end_ = selection_end_ + text.length();
  composition_range_start_ = 0;
  composition_range_end_ = 0;
  has_composition_ = false;
  return true;
}

void EditContext::ExtendSelectionAndDelete(int before, int after) {
  TRACE_EVENT1("ime", "EditContext::ExtendSelectionAndDelete", "before, afters",
               std::to_string(before) + ", " + std::to_string(after));
  String text;
  before = std::min(before, static_cast<int>(selection_start_));
  after = std::min(after, static_cast<int>(text_.length()));
  text_ = text_.Substring(0, selection_start_ - before) +
          text_.Substring(selection_end_ + after);
  const uint32_t update_range_start = selection_start_ - before;
  const uint32_t update_range_end = selection_end_ + after;
  selection_start_ = selection_start_ - before;
  selection_end_ = selection_start_;
  DispatchTextUpdateEvent(text, update_range_start, update_range_end,
                          selection_start_, selection_end_);
}

void EditContext::AttachElement(Element* element_to_attach) {
  if (base::Contains(attached_elements_, element_to_attach,
                     &Member<Element>::Get))
    return;

  attached_elements_.push_back(element_to_attach);
}

void EditContext::DetachElement(Element* element_to_detach) {
  auto* it = base::ranges::find(attached_elements_, element_to_detach,
                                &Member<Element>::Get);

  if (it != attached_elements_.end())
    attached_elements_.erase(it);
}

WebTextInputType EditContext::TextInputType() {
  switch (input_mode_) {
    case WebTextInputMode::kWebTextInputModeText:
      return WebTextInputType::kWebTextInputTypeText;
    case WebTextInputMode::kWebTextInputModeTel:
      return WebTextInputType::kWebTextInputTypeTelephone;
    case WebTextInputMode::kWebTextInputModeEmail:
      return WebTextInputType::kWebTextInputTypeEmail;
    case WebTextInputMode::kWebTextInputModeSearch:
      return WebTextInputType::kWebTextInputTypeSearch;
    case WebTextInputMode::kWebTextInputModeNumeric:
      return WebTextInputType::kWebTextInputTypeNumber;
    case WebTextInputMode::kWebTextInputModeDecimal:
      return WebTextInputType::kWebTextInputTypeNumber;
    case WebTextInputMode::kWebTextInputModeUrl:
      return WebTextInputType::kWebTextInputTypeURL;
    default:
      return WebTextInputType::kWebTextInputTypeText;
  }
}

ui::TextInputAction EditContext::GetEditContextEnterKeyHint() const {
  return enter_key_hint_;
}

WebTextInputMode EditContext::GetInputModeOfEditContext() const {
  return input_mode_;
}

WebTextInputInfo EditContext::TextInputInfo() {
  WebTextInputInfo info;
  // Fetch all the text input info from edit context.
  // TODO(crbug.com/1197325): Change this to refer to the "view" part of the
  // EditContext once the EditContext spec adds this feature.
  info.node_id = GetInputMethodController().NodeIdOfFocusedElement();
  info.action = GetEditContextEnterKeyHint();
  info.input_mode = GetInputModeOfEditContext();
  info.type = TextInputType();
  info.virtual_keyboard_policy = IsVirtualKeyboardPolicyManual()
                                     ? ui::mojom::VirtualKeyboardPolicy::MANUAL
                                     : ui::mojom::VirtualKeyboardPolicy::AUTO;
  info.value = text();
  info.flags = TextInputFlags();
  info.selection_start = selection_start_;
  info.selection_end = selection_end_;
  info.composition_start = composition_range_start_;
  info.composition_end = composition_range_end_;
  return info;
}

int EditContext::TextInputFlags() const {
  int flags = 0;
  // Disable spellcheck & autocorrect for EditContext.
  flags |= kWebTextInputFlagAutocorrectOff;
  flags |= kWebTextInputFlagSpellcheckOff;

  // TODO:(snianu) Enable this once the password type
  // is supported by inputMode attribute.
  // if (input_mode_ == WebTextInputMode::kPassword)
  //   flags |= kWebTextInputFlagHasBeenPasswordField;

  return flags;
}

WebRange EditContext::CompositionRange() const {
  return WebRange(composition_range_start_,
                  composition_range_end_ - composition_range_start_);
}

bool EditContext::GetCompositionCharacterBounds(WebVector<gfx::Rect>& bounds) {
  if (!HasValidCompositionBounds()) {
    return false;
  }

  TRACE_EVENT1("ime", "EditContext::GetCompositionCharacterBounds", "size",
               std::to_string(character_bounds_.size()));

  bounds.Clear();
  base::ranges::for_each(
      character_bounds_, [&bounds, this](auto& bound_in_css_pixels) {
        // EditContext's coordinates are in CSS pixels, which need to be
        // converted to physical pixels before return.
        auto result_bounds = gfx::ScaleToEnclosingRect(
            bound_in_css_pixels, DomWindow()->GetFrame()->DevicePixelRatio());
        bounds.push_back(result_bounds);
        TRACE_EVENT1("ime", "EditContext::GetCompositionCharacterBounds",
                     "charBounds", result_bounds.ToString());
      });

  return true;
}

bool EditContext::FirstRectForCharacterRange(uint32_t location,
                                             uint32_t length,
                                             gfx::Rect& rect_in_viewport) {
  if (HasValidCompositionBounds()) {
    WebRange range = this->CompositionRange();

    // If the requested range is within the current composition range,
    // we'll use that to provide the result.
    if (base::saturated_cast<int>(location) >= range.StartOffset() &&
        base::saturated_cast<int>(location + length) <= range.EndOffset()) {
      if (length == 0) {
        if (location == character_bounds_.size()) {
          // Zero-width rect after the last character in the composition range
          rect_in_viewport =
              gfx::Rect(character_bounds_[location - 1].right(),
                        character_bounds_[location - 1].y(), 0,
                        character_bounds_[location - 1].height());
        } else {
          // Zero-width rect before the next character in the composition range
          rect_in_viewport = gfx::Rect(character_bounds_[location].x(),
                                       character_bounds_[location].y(), 0,
                                       character_bounds_[location].height());
        }
      } else {
        const int start_in_composition = location - range.StartOffset();
        const int end_in_composition = location + length - range.StartOffset();
        gfx::Rect rect = character_bounds_[start_in_composition];
        for (int i = start_in_composition + 1; i < end_in_composition; ++i) {
          rect.Union(character_bounds_[i]);
        }

        rect_in_viewport = rect;
      }
      return true;
    }
  }

  // If we couldn't get a result from the composition bounds then we'll fall
  // back to using the selection bounds, since these will generally be close to
  // where the composition is happening.
  if (!selection_bounds_.IsEmpty()) {
    rect_in_viewport = selection_bounds_;
    return true;
  }

  // If we have neither composition bounds nor selection bounds, we'll fall back
  // to using the control bounds. In this case the IME might not be drawn
  // exactly in the right spot, but will at least be adjacent to the editable
  // region rather than in the corner of the screen.
  if (!control_bounds_.IsEmpty()) {
    rect_in_viewport = control_bounds_;
    return true;
  }

  return false;
}

bool EditContext::HasValidCompositionBounds() const {
  WebRange composition_range = CompositionRange();
  if (composition_range.IsEmpty()) {
    return false;
  }

  // The number of character bounds provided by the authors has to be the same
  // as the length of the composition (as we request in
  // CompositionCharacterBoundsUpdate event).
  return (base::saturated_cast<int>(character_bounds_.size()) ==
          composition_range.length());
}

WebRange EditContext::GetSelectionOffsets() const {
  return WebRange(selection_start_, selection_end_ - selection_start_);
}

void EditContext::Trace(Visitor* visitor) const {
  ActiveScriptWrappable::Trace(visitor);
  ExecutionContextClient::Trace(visitor);
  EventTargetWithInlineData::Trace(visitor);
  ElementRareDataField::Trace(visitor);
  visitor->Trace(attached_elements_);
}

}  // namespace blink
