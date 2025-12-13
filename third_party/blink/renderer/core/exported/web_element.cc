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

#include "third_party/blink/public/mojom/scroll/scroll_into_view_params.mojom-blink.h"
#include "third_party/blink/public/web/web_label_element.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_element.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_scroll_behavior.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_scroll_to_options.h"
#include "third_party/blink/renderer/core/clipboard/data_object.h"
#include "third_party/blink/renderer/core/clipboard/data_transfer.h"
#include "third_party/blink/renderer/core/clipboard/data_transfer_access_policy.h"
#include "third_party/blink/renderer/core/css/css_computed_style_declaration.h"
#include "third_party/blink/renderer/core/css/css_property_names.h"
#include "third_party/blink/renderer/core/css/style_engine.h"
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
#include "third_party/blink/renderer/core/frame/visual_viewport.h"
#include "third_party/blink/renderer/core/geometry/dom_rect_list.h"
#include "third_party/blink/renderer/core/html/custom/custom_element.h"
#include "third_party/blink/renderer/core/html/forms/html_label_element.h"
#include "third_party/blink/renderer/core/html/forms/text_control_element.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/layout/layout_box.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/scroll/scroll_into_view_util.h"
#include "third_party/blink/renderer/core/svg/graphics/svg_image.h"
#include "third_party/blink/renderer/core/svg/graphics/svg_image_for_container.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/graphics/image.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/vector2d_f.h"

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
  return ConstUnwrap<Element>()->GetInnerHTMLString();
}

void WebElement::Focus() {
  return Unwrap<Element>()->Focus();
}

void WebElement::Blur() {
  return Unwrap<Element>()->blur();
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

std::vector<WebLabelElement> WebElement::Labels() const {
  auto* html_element = blink::DynamicTo<HTMLElement>(ConstUnwrap<Element>());
  if (!html_element)
    return {};
  LabelsNodeList* html_labels =
      const_cast<HTMLElement*>(html_element)->labels();
  if (!html_labels)
    return {};
  std::vector<WebLabelElement> labels;
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

gfx::Rect WebElement::VisibleBoundsInWidget() const {
  const Element* element = ConstUnwrap<Element>();
  LocalFrame* frame = element->GetDocument().GetFrame();
  if (!frame || !frame->View()) {
    return gfx::Rect();
  }

  gfx::Rect bounds_in_local_root =
      element->VisibleBoundsRespectingClipsInLocalRoot();

  if (!frame->IsOutermostMainFrame()) {
    return bounds_in_local_root;
  }

  // In the outermost main frame the widget includes the viewport transform
  // (i.e. pinch-zoom). VisibleBoundsRespectingClipsInLocalRoot should already
  // have clipped to the visual viewport (but then transforms back into local
  // root space).
  VisualViewport& visual_viewport =
      element->GetDocument().GetPage()->GetVisualViewport();
  gfx::Rect bounds_in_viewport =
      visual_viewport.RootFrameToViewport(bounds_in_local_root);
  bounds_in_viewport.Intersect(gfx::Rect(visual_viewport.Size()));
  return bounds_in_viewport;
}

std::vector<gfx::Rect> WebElement::ClientRectsInWidget() {
  Element* element = Unwrap<Element>();
  LocalFrameView* view = element->GetDocument().View();
  if (!view) {
    return {};
  }

  std::vector<gfx::Rect> result;
  Vector<gfx::RectF> rects = element->GetClientRectsNoAdjustment();
  for (const gfx::RectF& rect : rects) {
    result.emplace_back(view->FrameToViewport(gfx::ToEnclosingRect(rect)));
  }
  return result;
}

SkBitmap WebElement::ImageContents() {
  Image* image = GetImage();
  if (!image)
    return {};
  scoped_refptr<SVGImageForContainer> svg_image_for_container;
  if (RuntimeEnabledFeatures::SvgFallBackToContainerSizeEnabled()) {
    if (auto* svg_image = blink::DynamicTo<SVGImage>(*image)) {
      // Adapted from ImageElementBase::GetSourceImageFromCanvas.
      Element* element = Unwrap<Element>();
      const ComputedStyle* style = element->GetComputedStyle();
      auto preferred_color_scheme = element->GetDocument()
                                        .GetStyleEngine()
                                        .ResolveColorSchemeForEmbedding(style);
      const SVGImageViewInfo* view_info =
          SVGImageForContainer::CreateViewInfo(*svg_image, *element);
      const gfx::SizeF image_size = SVGImageForContainer::ConcreteObjectSize(
          *svg_image, view_info, gfx::SizeF(GetClientSize()));
      if (!image_size.IsEmpty()) {
        svg_image_for_container = SVGImageForContainer::Create(
            *svg_image, image_size, 1, view_info, preferred_color_scheme);
      }
    }
  }
  if (svg_image_for_container) {
    image = svg_image_for_container.get();
  }
  return image->AsSkBitmapForCurrentFrame(kRespectImageOrientation);
}

std::vector<uint8_t> WebElement::CopyOfImageData() {
  Image* image = GetImage();
  if (!image || !image->HasData())
    return std::vector<uint8_t>();
  return image->Data()->CopyAs<std::vector<uint8_t>>();
}

WebString WebElement::ImageMimeType() {
  Image* image = GetImage();
  if (!image) {
    return WebString();
  }
  return image->MimeType();
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

gfx::Vector2dF WebElement::GetScrollOffset() const {
  Element* element = const_cast<Element*>(ConstUnwrap<Element>());
  return gfx::Vector2dF(element->scrollLeft(), element->scrollTop());
}

bool WebElement::SetScrollOffset(const gfx::Vector2dF& offset) {
  Element* element = Unwrap<Element>();
  ScrollToOptions* scroll_to_options = ScrollToOptions::Create();
  scroll_to_options->setLeft(offset.x());
  scroll_to_options->setTop(offset.y());
  scroll_to_options->setBehavior(V8ScrollBehavior::Enum::kInstant);
  return element->SetScrollOffset(scroll_to_options);
}

void WebElement::ScrollIntoViewIfNeeded() {
  Element* element = Unwrap<Element>();
  LayoutObject* layout_object = element->GetLayoutObject();
  if (!layout_object) {
    return;
  }

  mojom::blink::ScrollIntoViewParamsPtr params =
      mojom::blink::ScrollIntoViewParams::New();
  // Match ScrollAlignment::CenterIfNeeded().
  params->align_x = mojom::blink::ScrollAlignment::New();
  params->align_x->rect_visible =
      mojom::blink::ScrollAlignment::Behavior::kNoScroll;
  params->align_x->rect_hidden =
      mojom::blink::ScrollAlignment::Behavior::kCenter;
  params->align_x->rect_partial =
      mojom::blink::ScrollAlignment::Behavior::kClosestEdge;
  params->align_y = mojom::blink::ScrollAlignment::New();
  params->align_y->rect_visible =
      mojom::blink::ScrollAlignment::Behavior::kNoScroll;
  params->align_y->rect_hidden =
      mojom::blink::ScrollAlignment::Behavior::kCenter;
  params->align_y->rect_partial =
      mojom::blink::ScrollAlignment::Behavior::kClosestEdge;
  params->behavior = blink::mojom::ScrollBehavior::kInstant;
  // User scrolling to ensure only user scrollable scrollers are affected.
  params->type = mojom::blink::ScrollType::kUser;
  scroll_into_view_util::ScrollRectToVisible(
      *layout_object, layout_object->AbsoluteBoundingBoxRectForScrollIntoView(),
      std::move(params));
}

bool WebElement::HasScrollBehaviorSmooth() const {
  return GetScrollingBox()->StyleRef().GetScrollBehavior() ==
         mojom::blink::ScrollBehavior::kSmooth;
}

bool WebElement::IsUserScrollableX() const {
  LayoutBox* box = GetScrollingBox();
  if (!box) {
    return false;
  }

  return box->HasScrollableOverflowX();
}

bool WebElement::IsUserScrollableY() const {
  LayoutBox* box = GetScrollingBox();
  if (!box) {
    return false;
  }

  return box->HasScrollableOverflowY();
}

float WebElement::GetEffectiveZoom() const {
  const Element* element = ConstUnwrap<Element>();
  if (const auto* layout_object = element->GetLayoutObject()) {
    return layout_object->StyleRef().EffectiveZoom();
  }
  return 1.0f;
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

LayoutBox* WebElement::GetScrollingBox() const {
  Element* element = const_cast<Element*>(ConstUnwrap<Element>());

  // The viewport is a special case as it is scrolled by the layout view, rather
  // than body or html elements.
  if (element == element->GetDocument().scrollingElement()) {
    return element->GetDocument().GetLayoutView();
  }

  return blink::DynamicTo<LayoutBox>(element->GetLayoutObject());
}

}  // namespace blink
