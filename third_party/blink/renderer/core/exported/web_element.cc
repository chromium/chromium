/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
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

#include "third_party/blink/public/web/web_element.h"

#include "third_party/blink/public/web/web_label_element.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_element.h"
#include "third_party/blink/renderer/core/clipboard/data_object.h"
#include "third_party/blink/renderer/core/clipboard/data_transfer.h"
#include "third_party/blink/renderer/core/clipboard/data_transfer_access_policy.h"
#include "third_party/blink/renderer/core/css/css_computed_style_declaration.h"
#include "third_party/blink/renderer/core/css/css_property_names.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/focus_params.h"
#include "third_party/blink/renderer/core/editing/editing_utilities.h"
#include "third_party/blink/renderer/core/editing/frame_selection.h"
#include "third_party/blink/renderer/core/editing/ime/input_method_controller.h"
#include "third_party/blink/renderer/core/editing/selection_template.h"
#include "third_party/blink/renderer/core/editing/visible_selection.h"
#include "third_party/blink/renderer/core/events/clipboard_event.h"
#include "third_party/blink/renderer/core/events/text_event.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/html/custom/custom_element.h"
#include "third_party/blink/renderer/core/html/forms/html_label_element.h"
#include "third_party/blink/renderer/core/html/forms/text_control_element.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/graphics/image.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "ui/gfx/geometry/size.h"

namespace blink {

WebElement WebElement::FromV8Value(v8::Isolate* isolate,
                                   v8::Local<v8::Value> value) {
  Element* element = V8Element::ToWrappable(isolate, value);
  return WebElement(element);
}

bool WebElement::IsFormControlElement() const {
  return ConstUnwrap<Element>()->IsFormControlElement();
}

// TODO(dglazkov): Remove. Consumers of this code should use
// Node:hasEditableStyle.  http://crbug.com/612560
bool WebElement::IsEditable() const {
  const Element* element = ConstUnwrap<Element>();

  element->GetDocument().UpdateStyleAndLayoutTree();
  if (blink::IsEditable(*element))
    return true;

  if (auto* text_control = ToTextControlOrNull(element)) {
    if (!text_control->IsDisabledOrReadOnly())
      return true;
  }

  return EqualIgnoringASCIICase(
      element->FastGetAttribute(html_names::kRoleAttr), "textbox");
}

WebString WebElement::TagName() const {
  return ConstUnwrap<Element>()->tagName();
}

WebString WebElement::GetIdAttribute() const {
  return ConstUnwrap<Element>()->GetIdAttribute();
}

bool WebElement::HasHTMLTagName(const WebString& tag_name) const {
  const auto* html_element =
      blink::DynamicTo<HTMLElement>(ConstUnwrap<Element>());
  return html_element &&
         html_element->localName() == String(tag_name).LowerASCII();
}

bool WebElement::HasAttribute(const WebString& attr_name) const {
  return ConstUnwrap<Element>()->hasAttribute(attr_name);
}

WebString WebElement::GetAttribute(const WebString& attr_name) const {
  return ConstUnwrap<Element>()->getAttribute(attr_name);
}

void WebElement::SetAttribute(const WebString& attr_name,
                              const WebString& attr_value) {
  Unwrap<Element>()->setAttribute(attr_name, attr_value,
                                  IGNORE_EXCEPTION_FOR_TESTING);
}

WebString WebElement::TextContent() const {
  return ConstUnwrap<Element>()->textContent();
}
WebString WebElement::TextContentAbridged(const unsigned int max_length) const {
  return ConstUnwrap<Element>()->textContent(false, nullptr, max_length);
}

WebString WebElement::InnerHTML() const {
  return ConstUnwrap<Element>()->innerHTML();
}

bool WebElement::WritingSuggestions() const {
  const auto* html_element =
      blink::DynamicTo<HTMLElement>(ConstUnwrap<Element>());
  return html_element &&
         !EqualIgnoringASCIICase(html_element->writingSuggestions(),
                                 keywords::kFalse);
}

bool WebElement::ContainsFrameSelection() const {
  auto& e = *ConstUnwrap<Element>();
  LocalFrame* frame = e.GetDocument().GetFrame();
  if (!frame) {
    return false;
  }
  Element* root = frame->Selection().RootEditableElementOrDocumentElement();
  if (!root) {
    return false;
  }
  // For form controls, the selection's root editable is a contenteditable in
  // a shadow DOM tree.
  return (e.IsFormControlElement() ? root->OwnerShadowHost() : root) == e;
}

WebString WebElement::SelectedText() const {
  if (!ContainsFrameSelection()) {
    return "";
  }
  return ConstUnwrap<Element>()
      ->GetDocument()
      .GetFrame()
      ->Selection()
      .SelectedText(TextIteratorBehavior::Builder()
                        .SetEntersOpenShadowRoots(true)
                        .SetSkipsUnselectableContent(true)
                        .SetEntersTextControls(true)
                        .Build());
}

void WebElement::SelectText(bool select_all) {
  auto* element = Unwrap<Element>();
  LocalFrame* frame = element->GetDocument().GetFrame();
  if (!frame) {
    return;
  }

  // Makes sure the selection is inside `element`: if `select_all`, selects
  // all inside `element`; otherwise, selects an empty range at the end.
  if (auto* text_control_element =
          blink::DynamicTo<TextControlElement>(element)) {
    if (select_all) {
      text_control_element->select();
    } else {
      text_control_element->Focus(FocusParams(SelectionBehaviorOnFocus::kNone,
                                              mojom::blink::FocusType::kScript,
                                              nullptr, FocusOptions::Create()));
      text_control_element->setSelectionStart(std::numeric_limits<int>::max());
    }
  } else {
    Position base = FirstPositionInOrBeforeNode(*element);
    Position extent = LastPositionInOrAfterNode(*element);
    if (!select_all) {
      base = extent;
    }
    frame->Selection().SetSelection(
        SelectionInDOMTree::Builder().SetBaseAndExtent(base, extent).Build(),
        SetSelectionOptions());
  }
}

void WebElement::PasteText(const WebString& text, bool replace_all) {
  if (!IsEditable()) {
    return;
  }
  auto* element = Unwrap<Element>();
  LocalFrame* frame = element->GetDocument().GetFrame();
  if (!frame) {
    return;
  }

  // Returns true if JavaScript handlers destroyed the `frame`.
  auto is_destroyed = [](LocalFrame& frame) {
    return frame.GetDocument()->GetFrame() != frame;
  };

  if (replace_all || !ContainsFrameSelection()) {
    SelectText(replace_all);
    // JavaScript handlers may have destroyed the frame or moved the selection.
    if (is_destroyed(*frame) || !ContainsFrameSelection()) {
      return;
    }
  }

  // Simulates a paste command, except that it does not access the system
  // clipboard but instead pastes `text`. This block is a stripped-down version
  // of ClipboardCommands::Paste() that's limited to pasting plain text.
  Element* target = FindEventTargetFrom(
      *frame, frame->Selection().ComputeVisibleSelectionInDOMTree());
  auto create_data_transfer = [](const WebString& text) {
    return DataTransfer::Create(DataTransfer::kCopyAndPaste,
                                DataTransferAccessPolicy::kReadable,
                                DataObject::CreateFromString(text));
  };
  // Fires "paste" event.
  if (target->DispatchEvent(*ClipboardEvent::Create(
          event_type_names::kPaste, create_data_transfer(text))) !=
      DispatchEventResult::kNotCanceled) {
    return;
  }
  // Fires "beforeinput" event.
  if (DispatchBeforeInputDataTransfer(
          target, InputEvent::InputType::kInsertFromPaste,
          create_data_transfer(text)) != DispatchEventResult::kNotCanceled) {
    return;
  }
  // No DOM mutation if EditContext is active.
  if (frame->GetInputMethodController().GetActiveEditContext()) {
    return;
  }
  // Fires "textInput" and "input".
  target->DispatchEvent(
      *TextEvent::CreateForPlainTextPaste(frame->DomWindow(), text,
                                          /*should_smart_replace=*/true));
}

WebVector<WebLabelElement> WebElement::Labels() const {
  auto* html_element = blink::DynamicTo<HTMLElement>(ConstUnwrap<Element>());
  if (!html_element)
    return {};
  LabelsNodeList* html_labels =
      const_cast<HTMLElement*>(html_element)->labels();
  if (!html_labels)
    return {};
  Vector<WebLabelElement> labels;
  for (unsigned i = 0; i < html_labels->length(); i++) {
    if (auto* label_element =
            blink::DynamicTo<HTMLLabelElement>(html_labels->item(i))) {
      labels.push_back(label_element);
    }
  }
  return labels;
}

bool WebElement::IsAutonomousCustomElement() const {
  auto* element = ConstUnwrap<Element>();
  if (element->GetCustomElementState() == CustomElementState::kCustom)
    return CustomElement::IsValidName(element->localName());
  return false;
}

WebNode WebElement::ShadowRoot() const {
  auto* root = ConstUnwrap<Element>()->GetShadowRoot();
  if (!root || root->IsUserAgent())
    return WebNode();
  return WebNode(root);
}

WebElement WebElement::OwnerShadowHost() const {
  if (auto* host = ConstUnwrap<Element>()->OwnerShadowHost()) {
    return WebElement(host);
  }
  return WebElement();
}

WebNode WebElement::OpenOrClosedShadowRoot() {
  if (IsNull())
    return WebNode();

  auto* root = ConstUnwrap<Element>()->AuthorShadowRoot();
  return WebNode(root);
}

gfx::Rect WebElement::BoundsInWidget() const {
  return ConstUnwrap<Element>()->BoundsInWidget();
}

SkBitmap WebElement::ImageContents() {
  Image* image = GetImage();
  if (!image)
    return {};
  return image->AsSkBitmapForCurrentFrame(kRespectImageOrientation);
}

std::vector<uint8_t> WebElement::CopyOfImageData() {
  Image* image = GetImage();
  if (!image || !image->HasData())
    return std::vector<uint8_t>();
  return image->Data()->CopyAs<std::vector<uint8_t>>();
}

std::string WebElement::ImageExtension() {
  Image* image = GetImage();
  if (!image)
    return std::string();
  return image->FilenameExtension().Utf8();
}

gfx::Size WebElement::GetImageSize() {
  Image* image = GetImage();
  if (!image)
    return gfx::Size();
  return gfx::Size(image->width(), image->height());
}

gfx::Size WebElement::GetClientSize() const {
  Element* element = const_cast<Element*>(ConstUnwrap<Element>());
  return gfx::Size(element->clientWidth(), element->clientHeight());
}

gfx::Size WebElement::GetScrollSize() const {
  Element* element = const_cast<Element*>(ConstUnwrap<Element>());
  return gfx::Size(element->scrollWidth(), element->scrollHeight());
}

WebString WebElement::GetComputedValue(const WebString& property_name) {
  if (IsNull())
    return WebString();

  Element* element = Unwrap<Element>();
  CSSPropertyID property_id = CssPropertyID(
      element->GetDocument().GetExecutionContext(), property_name);
  if (property_id == CSSPropertyID::kInvalid)
    return WebString();

  element->GetDocument().UpdateStyleAndLayoutTree();
  auto* computed_style =
      MakeGarbageCollected<CSSComputedStyleDeclaration>(element);
  return computed_style->GetPropertyCSSValue(property_id)->CssText();
}

WebElement::WebElement(Element* elem) : WebNode(elem) {}

DEFINE_WEB_NODE_TYPE_CASTS(WebElement, IsElementNode())

WebElement& WebElement::operator=(Element* elem) {
  private_ = elem;
  return *this;
}

WebElement::operator Element*() const {
  return blink::To<Element>(private_.Get());
}

Image* WebElement::GetImage() {
  if (IsNull())
    return nullptr;
  return Unwrap<Element>()->ImageContents();
}

}  // namespace blink
