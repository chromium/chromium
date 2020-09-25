// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/editing/ime/edit_context.h"

#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/platform/web_vector.h"
#include "third_party/blink/public/web/web_range.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_edit_context_init.h"
#include "third_party/blink/renderer/core/css/css_color_value.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/editing/ime/input_method_controller.h"
#include "third_party/blink/renderer/core/editing/ime/text_format_update_event.h"
#include "third_party/blink/renderer/core/editing/ime/text_update_event.h"
#include "third_party/blink/renderer/core/events/composition_event.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/geometry/dom_rect.h"
#include "third_party/blink/renderer/platform/geometry/double_rect.h"
#include "third_party/blink/renderer/platform/wtf/decimal.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"
#include "ui/base/ime/ime_text_span.h"

namespace blink {

EditContext::EditContext(ScriptState* script_state, const EditContextInit* dict)
    : ExecutionContextClient(ExecutionContext::From(script_state)) {
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
  return ExecutionContextClient::GetFrame()->GetInputMethodController();
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
      event_type_names::kCompositionend,
      ExecutionContextClient::GetFrame()->DomWindow(), text);
  DispatchEvent(*event);
}

bool EditContext::DispatchCompositionStartEvent(const String& text) {
  auto* event = MakeGarbageCollected<CompositionEvent>(
      event_type_names::kCompositionstart,
      ExecutionContextClient::GetFrame()->DomWindow(), text);
  DispatchEvent(*event);
  if (!ExecutionContextClient::GetFrame())
    return false;
  return true;
}

void EditContext::DispatchTextUpdateEvent(const String& text,
                                          uint32_t update_range_start,
                                          uint32_t update_range_end,
                                          uint32_t new_selection_start,
                                          uint32_t new_selection_end) {
  TextUpdateEvent* event = MakeGarbageCollected<TextUpdateEvent>(
      text, update_range_start, update_range_end, new_selection_start,
      new_selection_end);
  DispatchEvent(*event);
}

void EditContext::DispatchTextFormatEvent(
    const WebVector<ui::ImeTextSpan>& ime_text_spans) {
  // Loop through the vector and fire textformatupdate event for individual text
  // spans as there could be multiple formats in the spans.
  // TODO(snianu): Try to accumulate the ranges with similar formats and fire
  // one event.
  DCHECK(has_composition_);
  String underline_thickness;
  String underline_style;
  for (const auto& ime_text_span : ime_text_spans) {
    const int format_range_start =
        ime_text_span.start_offset + composition_range_start_;
    const int format_range_end =
        ime_text_span.end_offset + composition_range_start_;

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
    TextFormatUpdateEvent* event = MakeGarbageCollected<TextFormatUpdateEvent>(
        format_range_start, format_range_end,
        cssvalue::CSSColorValue::SerializeAsCSSComponentValue(
            ime_text_span.underline_color),
        cssvalue::CSSColorValue::SerializeAsCSSComponentValue(
            ime_text_span.background_color),
        cssvalue::CSSColorValue::SerializeAsCSSComponentValue(
            ime_text_span.suggestion_highlight_color),
        cssvalue::CSSColorValue::SerializeAsCSSComponentValue(
            ime_text_span.text_color),
        underline_thickness, underline_style);
    DispatchEvent(*event);
  }
}

void EditContext::focus() {
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

void EditContext::blur() {
  if (GetInputMethodController().GetActiveEditContext() != this)
    return;
  // Clean up the state of the |this| EditContext.
  FinishComposingText(ConfirmCompositionBehavior::kKeepSelection);
  GetInputMethodController().SetActiveEditContext(this);
}

void EditContext::updateSelection(uint32_t start,
                                  uint32_t end,
                                  ExceptionState& exception_state) {
  // Following this spec:
  // https://html.spec.whatwg.org/#dom-textarea/input-setselectionrange
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

void EditContext::updateLayout(DOMRect* control_bounds,
                               DOMRect* selection_bounds) {
  // Return the IntRect containing the given DOMRect.
  const DoubleRect control_bounds_double_rect(
      control_bounds->x(), control_bounds->y(), control_bounds->width(),
      control_bounds->height());
  control_bounds_ = EnclosingIntRect(control_bounds_double_rect);
  const DoubleRect selection_bounds_double_rect(
      selection_bounds->x(), selection_bounds->y(), selection_bounds->width(),
      selection_bounds->height());
  selection_bounds_ = EnclosingIntRect(selection_bounds_double_rect);
}

void EditContext::updateText(uint32_t start,
                             uint32_t end,
                             const String& new_text,
                             ExceptionState& exception_state) {
  // Following this spec:
  // https://html.spec.whatwg.org/#dom-textarea/input-setrangetext
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
  text_ = text;
}

uint32_t EditContext::selectionStart() const {
  return selection_start_;
}

void EditContext::setSelectionStart(uint32_t selection_start,
                                    ExceptionState& exception_state) {
  // Following this spec:
  // https://html.spec.whatwg.org/#dom-textarea/input-setselectionrange
  selection_start_ = std::min(selection_end_, selection_start);
}

uint32_t EditContext::selectionEnd() const {
  return selection_end_;
}

void EditContext::setSelectionEnd(uint32_t selection_end,
                                  ExceptionState& exception_state) {
  // Following this spec:
  // https://html.spec.whatwg.org/#dom-textarea/input-setselectionrange
  selection_end_ = std::min(selection_end, text_.length());
}

String EditContext::inputPanelPolicy() const {
  if (input_panel_policy_ == EditContextInputPanelPolicy::kAuto)
    return "auto";
  return "manual";
}

void EditContext::setInputPanelPolicy(const String& input_policy) {
  if (input_policy == "auto")
    input_panel_policy_ = EditContextInputPanelPolicy::kAuto;
  else if (input_policy == "manual")
    input_panel_policy_ = EditContextInputPanelPolicy::kManual;
}

void EditContext::setInputMode(const String& input_mode) {
  // inputMode password is not supported by browsers yet:
  // https://github.com/whatwg/html/issues/4875

  if (input_mode == "text")
    input_mode_ = WebTextInputMode::kWebTextInputModeText;
  else if (input_mode == "tel")
    input_mode_ = WebTextInputMode::kWebTextInputModeTel;
  else if (input_mode == "email")
    input_mode_ = WebTextInputMode::kWebTextInputModeEmail;
  else if (input_mode == "search")
    input_mode_ = WebTextInputMode::kWebTextInputModeSearch;
  else if (input_mode == "decimal")
    input_mode_ = WebTextInputMode::kWebTextInputModeDecimal;
  else if (input_mode == "numeric")
    input_mode_ = WebTextInputMode::kWebTextInputModeNumeric;
  else if (input_mode == "url")
    input_mode_ = WebTextInputMode::kWebTextInputModeUrl;
  else
    input_mode_ = WebTextInputMode::kWebTextInputModeNone;
}

String EditContext::inputMode() const {
  switch (input_mode_) {
    case WebTextInputMode::kWebTextInputModeText:
      return "text";
    case WebTextInputMode::kWebTextInputModeSearch:
      return "search";
    case WebTextInputMode::kWebTextInputModeEmail:
      return "email";
    case WebTextInputMode::kWebTextInputModeDecimal:
      return "decimal";
    case WebTextInputMode::kWebTextInputModeNumeric:
      return "numeric";
    case WebTextInputMode::kWebTextInputModeTel:
      return "tel";
    case WebTextInputMode::kWebTextInputModeUrl:
      return "url";
    default:
      return "none";  // Defaulting to none.
  }
}

void EditContext::setEnterKeyHint(const String& enter_key_hint) {
  if (enter_key_hint == "enter")
    enter_key_hint_ = ui::TextInputAction::kEnter;
  else if (enter_key_hint == "done")
    enter_key_hint_ = ui::TextInputAction::kDone;
  else if (enter_key_hint == "go")
    enter_key_hint_ = ui::TextInputAction::kGo;
  else if (enter_key_hint == "next")
    enter_key_hint_ = ui::TextInputAction::kNext;
  else if (enter_key_hint == "previous")
    enter_key_hint_ = ui::TextInputAction::kPrevious;
  else if (enter_key_hint == "search")
    enter_key_hint_ = ui::TextInputAction::kSearch;
  else if (enter_key_hint == "send")
    enter_key_hint_ = ui::TextInputAction::kSend;
  else
    enter_key_hint_ = ui::TextInputAction::kEnter;
}

String EditContext::enterKeyHint() const {
  switch (enter_key_hint_) {
    case ui::TextInputAction::kEnter:
      return "enter";
    case ui::TextInputAction::kDone:
      return "done";
    case ui::TextInputAction::kGo:
      return "go";
    case ui::TextInputAction::kNext:
      return "next";
    case ui::TextInputAction::kPrevious:
      return "previous";
    case ui::TextInputAction::kSearch:
      return "search";
    case ui::TextInputAction::kSend:
      return "send";
    default:
      // Defaulting to enter.
      return "enter";
  }
}

void EditContext::GetLayoutBounds(WebRect* web_control_bounds,
                                  WebRect* web_selection_bounds) {
  *web_control_bounds = control_bounds_;
  *web_selection_bounds = selection_bounds_;
}

bool EditContext::SetComposition(
    const WebString& text,
    const WebVector<ui::ImeTextSpan>& ime_text_spans,
    const WebRange& replacement_range,
    int selection_start,
    int selection_end) {
  if (!text.IsEmpty() && !has_composition_) {
    if (!DispatchCompositionStartEvent(text))
      return false;
    has_composition_ = true;
  }
  if (text.IsEmpty() && !has_composition_)
    return true;

  if (composition_range_start_ == 0 && composition_range_end_ == 0) {
    composition_range_start_ = selection_start_;
    composition_range_end_ = selection_end_;
  }
  // Update the selection and buffer if the composition range has changed.
  String update_text(text);
  text_ = text_.Substring(0, composition_range_start_) + update_text +
          text_.Substring(composition_range_end_);

  // Fire textupdate and textformatupdate events to JS.
  const uint32_t update_range_start = composition_range_start_;
  const uint32_t update_range_end = composition_range_end_;
  // Update the new selection.
  selection_start_ = composition_range_start_ + selection_end;
  selection_end_ = composition_range_start_ + selection_end;
  DispatchTextUpdateEvent(update_text, update_range_start, update_range_end,
                          selection_start_, selection_end_);
  composition_range_end_ = composition_range_start_ + selection_end;
  DispatchTextFormatEvent(ime_text_spans);
  return true;
}

bool EditContext::SetCompositionFromExistingText(
    int composition_start,
    int composition_end,
    const WebVector<ui::ImeTextSpan>& ime_text_spans) {
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
  // Update the selection range.
  selection_start_ = composition_start;
  selection_end_ = composition_start;
  return true;
}

bool EditContext::CommitText(const WebString& text,
                             const WebVector<ui::ImeTextSpan>& ime_text_spans,
                             const WebRange& replacement_range,
                             int relative_caret_position) {
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
    selection_start_ = composition_range_start_ + text.length();
    selection_end_ = composition_range_start_ + text.length();
    update_range_start = composition_range_start_;
    update_range_end = composition_range_end_;
  } else {
    text_ = text_.Substring(0, selection_start_) + update_text +
            text_.Substring(selection_end_);
    update_range_start = selection_start_;
    update_range_end = selection_end_;
    selection_start_ = selection_start_ + text.length();
    selection_end_ = selection_end_ + text.length();
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
  WebString text;
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

WebRange EditContext::CompositionRange() {
  return WebRange(composition_range_start_, composition_range_end_);
}

bool EditContext::GetCompositionCharacterBounds(WebVector<WebRect>& bounds) {
  bounds[0] = selection_bounds_;
  return true;
}

WebRange EditContext::GetSelectionOffsets() const {
  return WebRange(selection_start_, selection_end_);
}

void EditContext::Trace(Visitor* visitor) const {
  ActiveScriptWrappable::Trace(visitor);
  ExecutionContextClient::Trace(visitor);
  EventTargetWithInlineData::Trace(visitor);
}

}  // namespace blink
