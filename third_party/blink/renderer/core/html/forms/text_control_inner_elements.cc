/*
 * Copyright (C) 2006, 2008, 2010 Apple Inc. All rights reserved.
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

#include "third_party/blink/renderer/core/html/forms/text_control_inner_elements.h"

#include "third_party/blink/public/common/input/web_pointer_properties.h"
#include "third_party/blink/renderer/core/css/resolver/style_adjuster.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver.h"
#include "third_party/blink/renderer/core/css/style_change_reason.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/dom_token_list.h"
#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/events/mouse_event.h"
#include "third_party/blink/renderer/core/html/forms/html_input_element.h"
#include "third_party/blink/renderer/core/html/forms/html_text_area_element.h"
#include "third_party/blink/renderer/core/html/shadow/shadow_element_names.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/layout/forms/layout_text_control_inner_editor.h"

namespace blink {

EditingViewPortElement::EditingViewPortElement(Document& document)
    : HTMLDivElement(document) {
  SetHasCustomStyleCallbacks();
  setAttribute(html_names::kIdAttr, shadow_element_names::kIdEditingViewPort);
}

const ComputedStyle* EditingViewPortElement::CustomStyleForLayoutObject(
    const StyleRecalcContext&) {
  // FXIME: Move these styles to html.css.

  ComputedStyleBuilder style_builder =
      GetDocument().GetStyleResolver().CreateComputedStyleBuilderInheritingFrom(
          OwnerShadowHost()->ComputedStyleRef());

  style_builder.SetFlexGrow(1);
  style_builder.SetMinWidth(Length::Fixed(0));
  style_builder.SetMinHeight(Length::Fixed(0));
  style_builder.SetDisplay(EDisplay::kBlock);
  style_builder.SetDirection(TextDirection::kLtr);

  // We don't want the shadow dom to be editable, so we set this block to
  // read-only in case the input itself is editable.
  style_builder.SetUserModify(EUserModify::kReadOnly);

  return style_builder.TakeStyle();
}

// ---------------------------

TextControlInnerEditorElement::TextControlInnerEditorElement(Document& document)
    : HTMLDivElement(document) {
  SetHasCustomStyleCallbacks();
}

void TextControlInnerEditorElement::DefaultEventHandler(Event& event) {
  // FIXME: In the future, we should add a way to have default event listeners.
  // Then we would add one to the text field's inner div, and we wouldn't need
  // this subclass.
  // Or possibly we could just use a normal event listener.
  if (event.IsBeforeTextInsertedEvent() ||
      event.type() == event_type_names::kWebkitEditableContentChanged) {
    Element* shadow_ancestor = OwnerShadowHost();
    // A TextControlInnerTextElement can have no host if its been detached,
    // but kept alive by an EditCommand. In this case, an undo/redo can
    // cause events to be sent to the TextControlInnerTextElement. To
    // prevent an infinite loop, we must check for this case before sending
    // the event up the chain.
    if (shadow_ancestor)
      shadow_ancestor->DefaultEventHandler(event);
  }

  if (event.type() == event_type_names::kScroll ||
      event.type() == event_type_names::kScrollend) {
    // The scroller for a text control is inside of a shadow tree but the
    // scroll event won't bubble past the shadow root and authors cannot add
    // an event listener to it. Fire the scroll event at the shadow host so
    // that the page can hear about the scroll.
    Element* shadow_ancestor = OwnerShadowHost();
    if (shadow_ancestor)
      shadow_ancestor->DispatchEvent(event);
  }

  if (!event.DefaultHandled())
    HTMLDivElement::DefaultEventHandler(event);
}

void TextControlInnerEditorElement::SetVisibility(bool is_visible) {
  if (is_visible_ != is_visible) {
    is_visible_ = is_visible;
    SetNeedsStyleRecalc(kLocalStyleChange,
                        StyleChangeReasonForTracing::Create(
                            style_change_reason::kControlValue));
  }
}

void TextControlInnerEditorElement::FocusChanged() {
  // When the focus changes for the host element, we may need to recalc style
  // for text-overflow. See TextControlElement::ValueForTextOverflow().
  SetNeedsStyleRecalc(kLocalStyleChange, StyleChangeReasonForTracing::Create(
                                             style_change_reason::kControl));
}

LayoutObject* TextControlInnerEditorElement::CreateLayoutObject(
    const ComputedStyle&) {
  return MakeGarbageCollected<LayoutTextControlInnerEditor>(this);
}

const ComputedStyle* TextControlInnerEditorElement::CustomStyleForLayoutObject(
    const StyleRecalcContext&) {
  Element* host = OwnerShadowHost();
  DCHECK(host);
  const ComputedStyle& start_style = host->ComputedStyleRef();
  ComputedStyleBuilder style_builder =
      GetDocument().GetStyleResolver().CreateComputedStyleBuilderInheritingFrom(
          start_style);
  // The inner block, if present, always has its direction set to LTR,
  // so we need to inherit the direction and unicode-bidi style from the
  // element.
  // TODO(https://crbug.com/1101564): The custom inheritance done here means we
  // need to mark for style recalc inside style recalc. See the workaround in
  // LayoutTextControl::StyleDidChange.
  style_builder.SetDirection(start_style.Direction());
  style_builder.SetUnicodeBidi(start_style.GetUnicodeBidi());
  style_builder.SetUserSelect(EUserSelect::kText);
  style_builder.SetUserModify(
      To<HTMLFormControlElement>(host)->IsDisabledOrReadOnly()
          ? EUserModify::kReadOnly
          : EUserModify::kReadWritePlaintextOnly);
  style_builder.SetDisplay(EDisplay::kBlock);
  style_builder.SetHasLineIfEmpty(true);
  if (!start_style.ApplyControlFixedSize(host)) {
    Length caret_width(GetDocument().View()->CaretWidth(), Length::kFixed);
    if (IsHorizontalWritingMode(style_builder.GetWritingMode())) {
      style_builder.SetMinWidth(caret_width);
    } else {
      style_builder.SetMinHeight(caret_width);
    }
  }
  style_builder.SetShouldIgnoreOverflowPropertyForInlineBlockBaseline();

  if (!IsA<HTMLTextAreaElement>(host)) {
    style_builder.SetScrollbarColor(nullptr);
    style_builder.SetWhiteSpace(EWhiteSpace::kPre);
    style_builder.SetOverflowWrap(EOverflowWrap::kNormal);
    style_builder.SetTextOverflow(ToTextControl(host)->ValueForTextOverflow());
    int computed_line_height = start_style.ComputedLineHeight();
    // Do not allow line-height to be smaller than our default.
    if (style_builder.FontSize() >= computed_line_height) {
      style_builder.SetLineHeight(
          ComputedStyleInitialValues::InitialLineHeight());
    }

    // We'd like to remove line-height if it's unnecessary because
    // overflow:scroll clips editing text by line-height.
    const Length& logical_height = start_style.LogicalHeight();
    // Here, we remove line-height if the INPUT fixed height is taller than the
    // line-height.  It's not the precise condition because logicalHeight
    // includes border and padding if box-sizing:border-box, and there are cases
    // in which we don't want to remove line-height with percent or calculated
    // length.
    // TODO(tkent): This should be done during layout.
    if (logical_height.HasPercent() ||
        (logical_height.IsFixed() &&
         logical_height.GetFloatValue() > computed_line_height)) {
      style_builder.SetLineHeight(
          ComputedStyleInitialValues::InitialLineHeight());
    }

    if (To<HTMLInputElement>(host)->ShouldRevealPassword())
      style_builder.SetTextSecurity(ETextSecurity::kNone);

    style_builder.SetOverflowX(EOverflow::kScroll);
    // overflow-y:visible doesn't work because overflow-x:scroll makes a layer.
    style_builder.SetOverflowY(EOverflow::kScroll);
    style_builder.SetScrollbarWidth(EScrollbarWidth::kNone);
    style_builder.SetDisplay(EDisplay::kFlowRoot);
  }

  // Using StyleAdjuster::adjustComputedStyle updates unwanted style. We'd like
  // to apply only editing-related and alignment-related.
  StyleAdjuster::AdjustStyleForEditing(style_builder, this);
  if (!is_visible_)
    style_builder.SetOpacity(0);

  return style_builder.TakeStyle();
}

// ----------------------------

SearchFieldCancelButtonElement::SearchFieldCancelButtonElement(
    Document& document)
    : HTMLDivElement(document) {
  SetShadowPseudoId(AtomicString("-webkit-search-cancel-button"));
  setAttribute(html_names::kIdAttr, shadow_element_names::kIdSearchClearButton);
}

void SearchFieldCancelButtonElement::DefaultEventHandler(Event& event) {
  // If the element is visible, on mouseup, clear the value, and set selection
  auto* mouse_event = DynamicTo<MouseEvent>(event);
  auto* input = To<HTMLInputElement>(OwnerShadowHost());
  if (!input || input->IsDisabledOrReadOnly()) {
    if (!event.DefaultHandled())
      HTMLDivElement::DefaultEventHandler(event);
    return;
  }

  if (event.type() == event_type_names::kClick && mouse_event &&
      mouse_event->button() ==
          static_cast<int16_t>(WebPointerProperties::Button::kLeft)) {
    input->SetValueForUser("");
    input->SetAutofillState(WebAutofillState::kNotFilled);
    input->OnSearch();
    event.SetDefaultHandled();
  }

  if (!event.DefaultHandled())
    HTMLDivElement::DefaultEventHandler(event);
}

bool SearchFieldCancelButtonElement::WillRespondToMouseClickEvents() {
  auto* input = To<HTMLInputElement>(OwnerShadowHost());
  if (input && !input->IsDisabledOrReadOnly())
    return true;

  return HTMLDivElement::WillRespondToMouseClickEvents();
}

// ----------------------------

PasswordRevealButtonElement::PasswordRevealButtonElement(Document& document)
    : HTMLDivElement(document) {
  SetShadowPseudoId(AtomicString("-internal-reveal"));
  setAttribute(html_names::kIdAttr,
               shadow_element_names::kIdPasswordRevealButton);
}

void PasswordRevealButtonElement::DefaultEventHandler(Event& event) {
  auto* input = To<HTMLInputElement>(OwnerShadowHost());
  if (!input || input->IsDisabledOrReadOnly()) {
    if (!event.DefaultHandled())
      HTMLDivElement::DefaultEventHandler(event);
    return;
  }

  // Toggle the should-reveal-password state when clicked.
  if (event.type() == event_type_names::kClick && IsA<MouseEvent>(event)) {
    bool shouldRevealPassword = !input->ShouldRevealPassword();

    input->SetShouldRevealPassword(shouldRevealPassword);
    input->UpdateView();

    event.SetDefaultHandled();
  }

  if (!event.DefaultHandled())
    HTMLDivElement::DefaultEventHandler(event);
}

bool PasswordRevealButtonElement::WillRespondToMouseClickEvents() {
  auto* input = To<HTMLInputElement>(OwnerShadowHost());
  if (input && !input->IsDisabledOrReadOnly())
    return true;

  return HTMLDivElement::WillRespondToMouseClickEvents();
}

}  // namespace blink
