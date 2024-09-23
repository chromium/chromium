/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 * Copyright (C) 2011 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
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

#include "third_party/blink/renderer/core/html/forms/text_field_input_type.h"

#include "third_party/blink/renderer/core/css_value_keywords.h"
#include "third_party/blink/renderer/core/dom/events/event_dispatch_forbidden_scope.h"
#include "third_party/blink/renderer/core/dom/focus_params.h"
#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/editing/frame_selection.h"
#include "third_party/blink/renderer/core/events/before_text_inserted_event.h"
#include "third_party/blink/renderer/core/events/drag_event.h"
#include "third_party/blink/renderer/core/events/keyboard_event.h"
#include "third_party/blink/renderer/core/events/mouse_event.h"
#include "third_party/blink/renderer/core/events/text_event.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/html/forms/html_input_element.h"
#include "third_party/blink/renderer/core/html/forms/text_control_inner_elements.h"
#include "third_party/blink/renderer/core/html/shadow/shadow_element_names.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/keywords.h"
#include "third_party/blink/renderer/core/layout/forms/layout_text_control_single_line.h"
#include "third_party/blink/renderer/core/page/chrome_client.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class DataListIndicatorElement final : public HTMLDivElement {
 private:
  inline HTMLInputElement* HostInput() const {
    return To<HTMLInputElement>(OwnerShadowHost());
  }

  EventDispatchHandlingState* PreDispatchEventHandler(Event& event) override {
    // Chromium opens autofill popup in a mousedown event listener
    // associated to the document. We don't want to open it in this case
    // because we opens a datalist chooser later.
    // FIXME: We should dispatch mousedown events even in such case.
    if (event.type() == event_type_names::kMousedown)
      event.stopPropagation();
    return nullptr;
  }

  void DefaultEventHandler(Event& event) override {
    DCHECK(GetDocument().IsActive());
    if (event.type() != event_type_names::kClick)
      return;
    HTMLInputElement* host = HostInput();
    if (host && !host->IsDisabledOrReadOnly()) {
      GetDocument().GetPage()->GetChromeClient().OpenTextDataListChooser(*host);
      event.SetDefaultHandled();
    }
  }

  bool WillRespondToMouseClickEvents() override {
    return HostInput() && !HostInput()->IsDisabledOrReadOnly() &&
           GetDocument().IsActive();
  }

 public:
  explicit DataListIndicatorElement(Document& document)
      : HTMLDivElement(document) {}

  // This function should be called after appending |this| to a UA ShadowRoot.
  void InitializeInShadowTree() {
    DCHECK(ContainingShadowRoot());
    DCHECK(ContainingShadowRoot()->IsUserAgent());
    SetShadowPseudoId(shadow_element_names::kPseudoCalendarPickerIndicator);
    setAttribute(html_names::kIdAttr, shadow_element_names::kIdPickerIndicator);
    SetInlineStyleProperty(CSSPropertyID::kDisplay, CSSValueID::kListItem);
    SetInlineStyleProperty(CSSPropertyID::kListStyle, "disclosure-open inside");
    SetInlineStyleProperty(CSSPropertyID::kCounterIncrement, "list-item 0");
    SetInlineStyleProperty(CSSPropertyID::kBlockSize, 1.0,
                           CSSPrimitiveValue::UnitType::kEms);
    // Do not expose list-item role.
    setAttribute(html_names::kAriaHiddenAttr, keywords::kTrue);
  }
};

TextFieldInputType::TextFieldInputType(Type type, HTMLInputElement& element)
    : InputType(type, element), InputTypeView(element) {}

TextFieldInputType::~TextFieldInputType() = default;

void TextFieldInputType::Trace(Visitor* visitor) const {
  InputTypeView::Trace(visitor);
  InputType::Trace(visitor);
}

InputTypeView* TextFieldInputType::CreateView() {
  return this;
}

InputType::ValueMode TextFieldInputType::GetValueMode() const {
  return ValueMode::kValue;
}

SpinButtonElement* TextFieldInputType::GetSpinButtonElement() const {
  if (!HasCreatedShadowSubtree()) {
    return nullptr;
  }
  auto* element = GetElement().UserAgentShadowRoot()->getElementById(
      shadow_element_names::kIdSpinButton);
  CHECK(!element || IsA<SpinButtonElement>(element));
  return To<SpinButtonElement>(element);
}

bool TextFieldInputType::MayTriggerVirtualKeyboard() const {
  return true;
}

bool TextFieldInputType::ValueMissing(const String& value) const {
  // For text-mode input elements, the value is missing only if it is mutable.
  // https://html.spec.whatwg.org/multipage/input.html#the-required-attribute
  return GetElement().IsRequired() && value.empty() &&
         !GetElement().IsDisabledOrReadOnly();
}

bool TextFieldInputType::CanSetSuggestedValue() {
  return true;
}

void TextFieldInputType::SetValue(const String& sanitized_value,
                                  bool value_changed,
                                  TextFieldEventBehavior event_behavior,
                                  TextControlSetValueSelection selection) {
  // We don't use InputType::setValue.  TextFieldInputType dispatches events
  // different way from InputType::setValue.
  if (event_behavior == TextFieldEventBehavior::kDispatchNoEvent)
    GetElement().SetNonAttributeValue(sanitized_value);
  else
    GetElement().SetNonAttributeValueByUserEdit(sanitized_value);

  // Visible value needs update if it differs from sanitized value,
  // if it was set with setValue().
  // event_behavior == kDispatchNoEvent usually means this call is
  // not a user edit.
  bool need_editor_update =
      value_changed ||
      (event_behavior == TextFieldEventBehavior::kDispatchNoEvent &&
       sanitized_value != GetElement().InnerEditorValue());

  if (need_editor_update)
    GetElement().UpdateView();
  // The following early-return can't be moved to the beginning of this
  // function. We need to update non-attribute value even if the value is not
  // changed.  For example, <input type=number> has a badInput string, that is
  // to say, IDL value=="", and new value is "", which should clear the badInput
  // string and update validity.
  if (!value_changed)
    return;

  if (selection == TextControlSetValueSelection::kSetSelectionToEnd) {
    unsigned max = VisibleValue().length();
    GetElement().SetSelectionRange(max, max);
  }

  switch (event_behavior) {
    case TextFieldEventBehavior::kDispatchChangeEvent:
      // If the user is still editing this field, dispatch an input event rather
      // than a change event.  The change event will be dispatched when editing
      // finishes.
      if (GetElement().IsFocused())
        GetElement().DispatchInputEvent();
      else
        GetElement().DispatchFormControlChangeEvent();
      break;

    case TextFieldEventBehavior::kDispatchInputEvent:
      GetElement().DispatchInputEvent();
      break;

    case TextFieldEventBehavior::kDispatchInputAndChangeEvent:
      GetElement().DispatchInputEvent();
      GetElement().DispatchFormControlChangeEvent();
      break;

    case TextFieldEventBehavior::kDispatchNoEvent:
      break;
  }
}

void TextFieldInputType::HandleKeydownEvent(KeyboardEvent& event) {
  if (!GetElement().IsFocused())
    return;
  if (ChromeClient* chrome_client = GetChromeClient()) {
    chrome_client->HandleKeyboardEventOnTextField(GetElement(), event);
    return;
  }
  event.SetDefaultHandled();
}

void TextFieldInputType::HandleKeydownEventForSpinButton(KeyboardEvent& event) {
  if (GetElement().IsDisabledOrReadOnly())
    return;
  const AtomicString key(event.key());
  const PhysicalToLogical<const AtomicString*> key_mapper(
      GetElement().GetComputedStyle()
          ? GetElement().GetComputedStyle()->GetWritingDirection()
          : WritingDirectionMode(WritingMode::kHorizontalTb,
                                 TextDirection::kLtr),
      &keywords::kArrowUp, &keywords::kArrowRight, &keywords::kArrowDown,
      &keywords::kArrowLeft);
  const AtomicString* key_up = key_mapper.LineOver();
  const AtomicString* key_down = key_mapper.LineUnder();

  if (key == *key_up) {
    SpinButtonStepUp();
  } else if (key == *key_down && !event.altKey()) {
    SpinButtonStepDown();
  } else {
    return;
  }
  GetElement().DispatchFormControlChangeEvent();
  event.SetDefaultHandled();
}

void TextFieldInputType::ForwardEvent(Event& event) {
  if (SpinButtonElement* spin_button = GetSpinButtonElement()) {
    spin_button->ForwardEvent(event);
    if (event.DefaultHandled())
      return;
  }

  // Style and layout may be dirty at this point. E.g. if an event handler for
  // the input element has modified its type attribute. If so, the LayoutObject
  // and the input type is out of sync. Avoid accessing the LayoutObject if we
  // have scheduled a forced re-attach (GetForceReattachLayoutTree()) for the
  // input element.
  if (GetElement().GetLayoutObject() &&
      !GetElement().GetForceReattachLayoutTree() &&
      (IsA<MouseEvent>(event) || IsA<DragEvent>(event) ||
       event.HasInterface(event_interface_names::kWheelEvent) ||
       event.type() == event_type_names::kBlur ||
       event.type() == event_type_names::kFocus)) {
    if (event.type() == event_type_names::kBlur) {
      if (LayoutBox* inner_editor_layout_object =
              GetElement().InnerEditorElement()->GetLayoutBox()) {
        // FIXME: This class has no need to know about PaintLayer!
        if (PaintLayer* inner_layer = inner_editor_layout_object->Layer()) {
          if (PaintLayerScrollableArea* inner_scrollable_area =
                  inner_layer->GetScrollableArea()) {
            inner_scrollable_area->SetScrollOffset(
                ScrollOffset(0, 0), mojom::blink::ScrollType::kProgrammatic);
          }
        }
      }
    }

    GetElement().ForwardEvent(event);
  }
}

void TextFieldInputType::HandleBlurEvent() {
  InputTypeView::HandleBlurEvent();
  GetElement().EndEditing();
  if (SpinButtonElement* spin_button = GetSpinButtonElement())
    spin_button->ReleaseCapture();
}

bool TextFieldInputType::ShouldSubmitImplicitly(const Event& event) {
  if (const TextEvent* text_event = DynamicTo<TextEvent>(event)) {
    if (!text_event->IsPaste() && !text_event->IsDrop() &&
        text_event->data() == "\n") {
      return true;
    }
  }
  return InputTypeView::ShouldSubmitImplicitly(event);
}

void TextFieldInputType::AdjustStyle(ComputedStyleBuilder& builder) {
  // The flag is necessary in order that a text field <input> with non-'visible'
  // overflow property doesn't change its baseline.
  builder.SetShouldIgnoreOverflowPropertyForInlineBlockBaseline();
}

LayoutObject* TextFieldInputType::CreateLayoutObject(
    const ComputedStyle&) const {
  return MakeGarbageCollected<LayoutTextControlSingleLine>(&GetElement());
}

ControlPart TextFieldInputType::AutoAppearance() const {
  return kTextFieldPart;
}

bool TextFieldInputType::IsInnerEditorValueEmpty() const {
  if (!HasCreatedShadowSubtree()) {
    return VisibleValue().empty();
  }
  return GetElement().InnerEditorValue().empty();
}

void TextFieldInputType::CreateShadowSubtree() {
  DCHECK(IsShadowHost(GetElement()));
  ShadowRoot* shadow_root = GetElement().UserAgentShadowRoot();
  DCHECK(!shadow_root->HasChildren());

  bool should_have_spin_button = GetElement().IsSteppable();
  bool should_have_data_list_indicator = GetElement().HasValidDataListOptions();
  bool creates_container = should_have_spin_button ||
                           should_have_data_list_indicator || NeedsContainer();

  HTMLElement* inner_editor = GetElement().CreateInnerEditorElement();
  if (!creates_container) {
    shadow_root->AppendChild(inner_editor);
    return;
  }

  Document& document = GetElement().GetDocument();
  auto* container = MakeGarbageCollected<HTMLDivElement>(document);
  container->SetInlineStyleProperty(CSSPropertyID::kUnicodeBidi,
                                    CSSValueID::kNormal);
  container->SetIdAttribute(shadow_element_names::kIdTextFieldContainer);
  container->SetShadowPseudoId(
      shadow_element_names::kPseudoTextFieldDecorationContainer);
  shadow_root->AppendChild(container);

  auto* editing_view_port =
      MakeGarbageCollected<EditingViewPortElement>(document);
  editing_view_port->AppendChild(inner_editor);
  container->AppendChild(editing_view_port);

  if (should_have_data_list_indicator) {
    auto* data_list = MakeGarbageCollected<DataListIndicatorElement>(document);
    container->AppendChild(data_list);
    data_list->InitializeInShadowTree();
  }
  // FIXME: Because of a special handling for a spin button in
  // LayoutTextControlSingleLine, we need to put it to the last position. It's
  // inconsistent with multiple-fields date/time types.
  if (should_have_spin_button) {
    container->AppendChild(
        MakeGarbageCollected<SpinButtonElement, Document&,
                             SpinButtonElement::SpinButtonOwner&>(document,
                                                                  *this));
  }

  // See listAttributeTargetChanged too.
}

Element* TextFieldInputType::ContainerElement() const {
  return GetElement().EnsureShadowSubtree()->getElementById(
      shadow_element_names::kIdTextFieldContainer);
}

void TextFieldInputType::DestroyShadowSubtree() {
  InputTypeView::DestroyShadowSubtree();
  if (SpinButtonElement* spin_button = GetSpinButtonElement())
    spin_button->RemoveSpinButtonOwner();
}

void TextFieldInputType::ListAttributeTargetChanged() {
  if (!HasCreatedShadowSubtree()) {
    return;
  }
  if (ChromeClient* chrome_client = GetChromeClient())
    chrome_client->TextFieldDataListChanged(GetElement());
  Element* picker = GetElement().UserAgentShadowRoot()->getElementById(
      shadow_element_names::kIdPickerIndicator);
  bool did_have_picker_indicator = picker;
  bool will_have_picker_indicator = GetElement().HasValidDataListOptions();
  if (did_have_picker_indicator == will_have_picker_indicator)
    return;
  EventDispatchForbiddenScope::AllowUserAgentEvents allow_events;
  if (will_have_picker_indicator) {
    Document& document = GetElement().GetDocument();
    if (Element* container = ContainerElement()) {
      auto* data_list =
          MakeGarbageCollected<DataListIndicatorElement>(document);
      container->InsertBefore(data_list, GetSpinButtonElement());
      data_list->InitializeInShadowTree();
    } else {
      // FIXME: The following code is similar to createShadowSubtree(),
      // but they are different. We should simplify the code by making
      // containerElement mandatory.
      auto* rp_container = MakeGarbageCollected<HTMLDivElement>(document);
      rp_container->SetIdAttribute(shadow_element_names::kIdTextFieldContainer);
      rp_container->SetShadowPseudoId(
          shadow_element_names::kPseudoTextFieldDecorationContainer);
      Element* inner_editor = GetElement().InnerEditorElement();
      inner_editor->parentNode()->ReplaceChild(rp_container, inner_editor);
      auto* editing_view_port =
          MakeGarbageCollected<EditingViewPortElement>(document);
      editing_view_port->AppendChild(inner_editor);
      rp_container->AppendChild(editing_view_port);
      auto* data_list =
          MakeGarbageCollected<DataListIndicatorElement>(document);
      rp_container->AppendChild(data_list);
      data_list->InitializeInShadowTree();
      Element& input = GetElement();
      if (input.GetDocument().FocusedElement() == input)
        input.UpdateSelectionOnFocus(SelectionBehaviorOnFocus::kRestore);
    }
  } else {
    picker->remove(ASSERT_NO_EXCEPTION);
  }
}

void TextFieldInputType::ValueAttributeChanged() {
  UpdateView();
}

void TextFieldInputType::DisabledOrReadonlyAttributeChanged() {
  if (SpinButtonElement* spin_button = GetSpinButtonElement())
    spin_button->ReleaseCapture();
}

void TextFieldInputType::DisabledAttributeChanged() {
  if (!HasCreatedShadowSubtree()) {
    return;
  }
  DisabledOrReadonlyAttributeChanged();
}

void TextFieldInputType::ReadonlyAttributeChanged() {
  if (!HasCreatedShadowSubtree()) {
    return;
  }
  DisabledOrReadonlyAttributeChanged();
}

bool TextFieldInputType::SupportsReadOnly() const {
  return true;
}

static bool IsASCIILineBreak(UChar c) {
  return c == '\r' || c == '\n';
}

// Returns true if `c` may contain a line break. This is an inexact comparison.
// This is used as the common case is the text does not contain a newline.
static bool MayBeASCIILineBreak(UChar c) {
  static_assert('\n' < '\r');
  return c <= '\r';
}

static String LimitLength(const String& string, unsigned max_length) {
  unsigned new_length = std::min(max_length, string.length());
  if (new_length == string.length())
    return string;
  if (new_length > 0 && U16_IS_LEAD(string[new_length - 1]))
    --new_length;
  return string.Left(new_length);
}

String TextFieldInputType::SanitizeValue(const String& proposed_value) const {
  // Typical case is the string doesn't contain a break and fits. The Find()
  // is not exact (meaning it'll match many other characters), but is a good
  // approximation for a fast path.
  if (proposed_value.Find(MayBeASCIILineBreak) == kNotFound &&
      proposed_value.length() < std::numeric_limits<int>::max()) {
    return proposed_value;
  }
  return LimitLength(proposed_value.RemoveCharacters(IsASCIILineBreak),
                     std::numeric_limits<int>::max());
}

void TextFieldInputType::HandleBeforeTextInsertedEvent(
    BeforeTextInsertedEvent& event) {
  // Make sure that the text to be inserted will not violate the maxLength.

  // We use HTMLInputElement::innerEditorValue() instead of
  // HTMLInputElement::value() because they can be mismatched by
  // sanitizeValue() in HTMLInputElement::subtreeHasChanged() in some cases.
  unsigned old_length = GetElement().InnerEditorValue().length();

  // selectionLength represents the selection length of this text field to be
  // removed by this insertion.
  // If the text field has no focus, we don't need to take account of the
  // selection length. The selection is the source of text drag-and-drop in
  // that case, and nothing in the text field will be removed.
  unsigned selection_length = 0;
  if (GetElement().IsFocused()) {
    // TODO(editing-dev): Use of UpdateStyleAndLayout
    // needs to be audited.  See http://crbug.com/590369 for more details.
    GetElement().GetDocument().UpdateStyleAndLayout(
        DocumentUpdateReason::kEditing);

    selection_length = GetElement()
                           .GetDocument()
                           .GetFrame()
                           ->Selection()
                           .SelectedText()
                           .length();
  }
  DCHECK_GE(old_length, selection_length);

  // Selected characters will be removed by the next text event.
  unsigned base_length = old_length - selection_length;
  unsigned max_length;
  if (MaxLength() < 0)
    max_length = std::numeric_limits<int>::max();
  else
    max_length = static_cast<unsigned>(MaxLength());
  unsigned appendable_length =
      max_length > base_length ? max_length - base_length : 0;

  // Truncate the inserted text to avoid violating the maxLength and other
  // constraints.
  String event_text = event.GetText();
  unsigned text_length = event_text.length();
  while (text_length > 0 && IsASCIILineBreak(event_text[text_length - 1]))
    text_length--;
  event_text.Truncate(text_length);
  event_text.Replace("\r\n", " ");
  event_text.Replace('\r', ' ');
  event_text.Replace('\n', ' ');

  event.SetText(LimitLength(event_text, appendable_length));

  if (ChromeClient* chrome_client = GetChromeClient()) {
    if (selection_length == old_length && selection_length != 0 &&
        !event_text.empty()) {
      chrome_client->DidClearValueInTextField(GetElement());
    }
  }
}

bool TextFieldInputType::ShouldRespectListAttribute() {
  return true;
}

HTMLElement* TextFieldInputType::UpdatePlaceholderText(
    bool is_suggested_value) {
  if (!HasCreatedShadowSubtree() &&
      RuntimeEnabledFeatures::CreateInputShadowTreeDuringLayoutEnabled()) {
    return nullptr;
  }
  if (!SupportsPlaceholder()) {
    return nullptr;
  }
  HTMLElement* placeholder = GetElement().PlaceholderElement();
  if (!is_suggested_value &&
      !GetElement().FastHasAttribute(html_names::kPlaceholderAttr)) {
    if (placeholder)
      placeholder->remove(ASSERT_NO_EXCEPTION);
    return nullptr;
  }
  if (!placeholder) {
    GetElement().EnsureShadowSubtree();
    auto* new_element =
        MakeGarbageCollected<HTMLDivElement>(GetElement().GetDocument());
    placeholder = new_element;
    placeholder->SetShadowPseudoId(
        shadow_element_names::kPseudoInputPlaceholder);
    placeholder->SetInlineStyleProperty(CSSPropertyID::kDisplay,
                                        GetElement().IsPlaceholderVisible()
                                            ? CSSValueID::kBlock
                                            : CSSValueID::kNone,
                                        true);
    placeholder->setAttribute(html_names::kIdAttr,
                              shadow_element_names::kIdPlaceholder);
    Element* container = ContainerElement();
    Node* previous = container ? container : GetElement().InnerEditorElement();
    previous->parentNode()->InsertBefore(placeholder, previous);
    SECURITY_DCHECK(placeholder->parentNode() == previous->parentNode());
  }
  if (is_suggested_value) {
    placeholder->SetInlineStyleProperty(CSSPropertyID::kUserSelect,
                                        CSSValueID::kNone, true);
  } else {
    placeholder->RemoveInlineStyleProperty(CSSPropertyID::kUserSelect);
  }
  placeholder->setTextContent(GetElement().GetPlaceholderValue());
  return placeholder;
}

String TextFieldInputType::ConvertFromVisibleValue(
    const String& visible_value) const {
  return visible_value;
}

void TextFieldInputType::SubtreeHasChanged() {
  GetElement().SetValueFromRenderer(SanitizeUserInputValue(
      ConvertFromVisibleValue(GetElement().InnerEditorValue())));
  GetElement().UpdatePlaceholderVisibility();
  GetElement().PseudoStateChanged(CSSSelector::kPseudoValid);
  GetElement().PseudoStateChanged(CSSSelector::kPseudoInvalid);
  GetElement().PseudoStateChanged(CSSSelector::kPseudoUserValid);
  GetElement().PseudoStateChanged(CSSSelector::kPseudoUserInvalid);
  GetElement().PseudoStateChanged(CSSSelector::kPseudoInRange);
  GetElement().PseudoStateChanged(CSSSelector::kPseudoOutOfRange);

  DidSetValueByUserEdit();
}

void TextFieldInputType::OpenPopupView() {
  if (GetElement().IsDisabledOrReadOnly())
    return;
  if (ChromeClient* chrome_client = GetChromeClient())
    chrome_client->OpenTextDataListChooser(GetElement());
}

void TextFieldInputType::DidSetValueByUserEdit() {
  if (!GetElement().IsFocused())
    return;
  if (ChromeClient* chrome_client = GetChromeClient()) {
    if (GetElement().Value().empty()) {
      chrome_client->DidClearValueInTextField(GetElement());
    }
    chrome_client->DidChangeValueInTextField(GetElement());
  }
}

void TextFieldInputType::SpinButtonStepDown() {
  StepUpFromLayoutObject(-1);
}

void TextFieldInputType::SpinButtonStepUp() {
  StepUpFromLayoutObject(1);
}

void TextFieldInputType::UpdateView() {
  if (GetElement().SuggestedValue().empty() &&
      GetElement().NeedsToUpdateViewValue()) {
    // Update the view only if needsToUpdateViewValue is true. It protects
    // an unacceptable view value from being overwritten with the DOM value.
    //
    // e.g. <input type=number> has a view value "abc", and input.max is
    // updated. In this case, updateView() is called but we should not
    // update the view value.
    GetElement().SetInnerEditorValue(VisibleValue());
    GetElement().UpdatePlaceholderVisibility();
  }
}

void TextFieldInputType::FocusAndSelectSpinButtonOwner() {
  GetElement().Focus(FocusParams(FocusTrigger::kUserGesture));
  GetElement().SetSelectionRange(0, std::numeric_limits<int>::max());
}

bool TextFieldInputType::ShouldSpinButtonRespondToMouseEvents() {
  return !GetElement().IsDisabledOrReadOnly();
}

bool TextFieldInputType::ShouldSpinButtonRespondToWheelEvents() {
  return ShouldSpinButtonRespondToMouseEvents() && GetElement().IsFocused();
}

void TextFieldInputType::SpinButtonDidReleaseMouseCapture(
    SpinButtonElement::EventDispatch event_dispatch) {
  if (event_dispatch == SpinButtonElement::kEventDispatchAllowed)
    GetElement().DispatchFormControlChangeEvent();
}

}  // namespace blink
