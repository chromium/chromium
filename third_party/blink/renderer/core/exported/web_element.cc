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

#include <optional>

#include "base/functional/callback_helpers.h"
#include "third_party/blink/public/common/metrics/document_update_reason.h"
#include "third_party/blink/public/mojom/input/focus_type.mojom-blink.h"
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
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/events/simulated_click_options.h"
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
#include "third_party/blink/renderer/core/frame/local_frame_ukm_aggregator.h"
#include "third_party/blink/renderer/core/frame/visual_viewport.h"
#include "third_party/blink/renderer/core/geometry/dom_rect_list.h"
#include "third_party/blink/renderer/core/html/canvas/html_canvas_element.h"
#include "third_party/blink/renderer/core/html/custom/custom_element.h"
#include "third_party/blink/renderer/core/html/forms/html_form_control_element.h"
#include "third_party/blink/renderer/core/html/forms/html_label_element.h"
#include "third_party/blink/renderer/core/html/forms/text_control_element.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/intersection_observer/intersection_observer.h"
#include "third_party/blink/renderer/core/intersection_observer/intersection_observer_entry.h"
#include "third_party/blink/renderer/core/layout/layout_box.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/scroll/scroll_into_view_util.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/core/svg/graphics/svg_image.h"
#include "third_party/blink/renderer/core/svg/graphics/svg_image_for_container.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/graphics/image.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/timer.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/vector2d_f.h"

namespace blink {
namespace {

String AriaAttr(const Element& element, const QualifiedName& attribute) {
  // Authors sometimes add stray whitespace around ARIA-like attribute values.
  // Normalize that here while keeping role matching intentionally exact.
  return String(element.FastGetAttribute(attribute)).StripWhiteSpace();
}

bool AriaBoolAttr(const Element& element,
                  const QualifiedName& attribute,
                  bool default_value = false) {
  const String value = AriaAttr(element, attribute);
  if (EqualIgnoringAsciiCase(value, keywords::kTrue)) {
    return true;
  }
  if (EqualIgnoringAsciiCase(value, keywords::kFalse)) {
    return false;
  }
  return default_value;
}

bool HasPresentationalRole(const Element& element) {
  const String role = AriaAttr(element, html_names::kRoleAttr);

  // Trim whitespace, but intentionally do not split multiple ARIA roles.
  // Fallback roles are not used in practice or supported industry-wide. Exact
  // matching keeps this actor-facing helper simple.
  return EqualIgnoringAsciiCase(role, keywords::kNone) ||
         EqualIgnoringAsciiCase(role, keywords::kPresentation);
}

std::optional<WebElementInteractionDisallowedReason>
GetAriaInteractionDisallowedReason(const Element& element) {
  for (const Node* node = &element; node;
       node = node->ParentOrShadowHostNode()) {
    const Element* ancestor = DynamicTo<Element>(node);
    if (!ancestor) {
      continue;
    }
    if (AriaBoolAttr(*ancestor, html_names::kAriaDisabledAttr)) {
      return WebElementInteractionDisallowedReason::kAriaDisabled;
    }
    if (AriaBoolAttr(*ancestor, html_names::kAriaHiddenAttr)) {
      return WebElementInteractionDisallowedReason::kAriaHidden;
    }
  }

  return std::nullopt;
}

}  // namespace

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

  return EqualIgnoringAsciiCase(
      element->FastGetAttribute(html_names::kRoleAttr), "textbox");
}

WebString WebElement::TagName() const {
  return ConstUnwrap<Element>()->tagName();
}

WebString WebElement::GetIdAttribute() const {
  return ConstUnwrap<Element>()->GetIdAttribute();
}
WebString WebElement::Nonce() const {
  return ConstUnwrap<Element>()->nonce();
}

bool WebElement::HasHTMLTagName(const WebString& tag_name) const {
  const auto* html_element =
      blink::DynamicTo<HTMLElement>(ConstUnwrap<Element>());
  return html_element &&
         html_element->localName() == String(tag_name).ToAsciiLower();
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
         !EqualIgnoringAsciiCase(html_element->writingSuggestions(),
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
        SelectionInDomTree::Builder().SetBaseAndExtent(base, extent).Build(),
        SetSelectionOptions());
  }
}

void WebElement::Click() {
  auto* element = Unwrap<Element>();
  element->DispatchSimulatedClick(nullptr);
}

std::optional<WebElementInteractionDisallowedReason>
WebElement::InteractionDisallowedReason(bool check_aria) const {
  // Querying can update lifecycle state, but it never retargets this wrapper.
  Element* element = const_cast<Element*>(ConstUnwrap<Element>());
  if (const auto* form_control =
          blink::DynamicTo<HTMLFormControlElement>(element)) {
    if (form_control->IsDisabledFormControl()) {
      return WebElementInteractionDisallowedReason::kDisabled;
    }
  }

  // Interaction-disallowed state depends on computed style, including
  // inherited inert state from native modal dialogs and pointer event handling,
  // so refresh the style tree before reading it.
  element->GetDocument().UpdateStyleAndLayoutTree();
  if (!element->GetLayoutObject()) {
    return WebElementInteractionDisallowedReason::kNoLayoutObject;
  }

  if (const ComputedStyle* style = element->GetComputedStyle()) {
    if (style->IsInert()) {
      return WebElementInteractionDisallowedReason::kInert;
    }
    if (style->UsedPointerEvents() == EPointerEvents::kNone) {
      return WebElementInteractionDisallowedReason::kPointerEventsNone;
    }
  }

  if (!check_aria) {
    return std::nullopt;
  }

  // Note: this helper does not currently scan for ARIA modal dialogs.
  // Enforcing that here would require scanning the whole document for all
  // elements with role=dialog or role=alertdialog and aria-modal=true, then
  // rejecting targets outside any visible dialog's flat-tree subtree.
  // Clicking outside native modal dialogs is already handled by checking for
  // inertness. See https://crrev.com/c/8007486 for a prototype implementation.
  if (std::optional<WebElementInteractionDisallowedReason> aria_reason =
          GetAriaInteractionDisallowedReason(*element)) {
    return aria_reason;
  }

  if (HasPresentationalRole(*element)) {
    return WebElementInteractionDisallowedReason::kRolePresentationOrNone;
  }

  return std::nullopt;
}

bool WebElement::SimulateAccessibilityClick() {
  auto* element = Unwrap<Element>();
  Document& document = element->GetDocument();

  LocalFrame* frame = document.GetFrame();
  if (!frame) {
    return false;
  }

  if (!element->isConnected()) {
    return false;
  }

  if (InteractionDisallowedReason(/*check_aria=*/true).has_value()) {
    return false;
  }

  // This is a target-scoped lifecycle update. The interaction-disallowed
  // preflight above only needs current style-tree state, but content-visibility
  // and display locks can still leave this element without current layout data.
  // Accessibility-style activation needs current target layout before it sends
  // trusted simulated input events.
  document.UpdateStyleAndLayoutTreeForElement(element,
                                              DocumentUpdateReason::kInput);

  if (!element->isConnected() || element->GetDocument().GetFrame() != frame) {
    // Lifecycle updates can detach or move the element. Accessibility-style
    // activation is only supported while the original frame still owns the
    // target.
    return false;
  }

  // Do not call LocalFrame::NotifyUserActivation() here. Blink uses the
  // "transient user activation" term for the short-lived state that means "a
  // real user gesture happened recently", which unlocks privileged page APIs
  // such as popup, media, clipboard, and file-picker flows. This helper
  // dispatches a trusted click sequence, but that click should not also unlock
  // gesture-gated APIs. If a future caller needs that behavior, consider an
  // explicit option or a separately named helper whose call site says that it
  // grants a user gesture.

  // Match Blink's accessibility activation behavior for sequential focus
  // navigation. This changes where the next Tab search starts, not
  // activeElement.
  document.SetSequentialFocusNavigationStartingPoint(element);

  // This sends pointerdown, mousedown, pointerup, mouseup, and click. It also
  // avoids AccessKeyAction(), which focuses some controls as part of access-key
  // activation. Simulated accessibility clicks must not change activeElement.
  element->DispatchSimulatedClick(
      nullptr, SimulatedClickCreationScope::kFromAccessibility);
  return true;
}

void WebElement::PasteText(const WebString& text,
                           bool replace_all,
                           bool smart_replace) {
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
      *frame, frame->Selection().ComputeVisibleSelectionInDomTree());
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
  target->DispatchEvent(*TextEvent::CreateForPlainTextPaste(
      frame->DomWindow(), text,
      /*should_smart_replace=*/smart_replace));
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
  Element* element = Unwrap<Element>();
  if (!element) {
    return {};
  }

  if (auto* canvas = blink::DynamicTo<HTMLCanvasElement>(element)) {
    scoped_refptr<StaticBitmapImage> image = canvas->Snapshot(kBackBuffer);
    if (!image) {
      return {};
    }
    return image->AsSkBitmapForCurrentFrame(kRespectImageOrientation);
  }

  Image* image = GetImage();
  if (!image)
    return {};
  scoped_refptr<SVGImageForContainer> svg_image_for_container;
  if (RuntimeEnabledFeatures::SvgFallBackToContainerSizeEnabled()) {
    if (auto* svg_image = blink::DynamicTo<SVGImage>(*image)) {
      // Adapted from ImageElementBase::GetSourceImageFromCanvas.
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
  return element->ScrollTo(scroll_to_options);
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

namespace {

class VisibilityObserver final : public GarbageCollected<VisibilityObserver> {
 public:
  VisibilityObserver(Element* element,
                     base::TimeDelta minimum_visible_duration,
                     base::OnceClosure callback)
      : element_(element),
        minimum_visible_duration_(minimum_visible_duration),
        callback_(std::move(callback)),
        visibility_timer_(
            element->GetDocument().GetTaskRunner(TaskType::kInternalDefault),
            this,
            &VisibilityObserver::VisibilityTimerFired) {
    IntersectionObserver::Params params = {
        .root = nullptr,
        // Require 100% of the element to be intersecting the viewport.
        .thresholds = {1.0f},
        // Add a delay of 100ms between observer notifications.
        .delay = base::Milliseconds(100),
        // Enable visibility tracking; otherwise `isVisible()` in entries will
        // always be false.
        .track_visibility = true,
    };
    observer_ = IntersectionObserver::Create(
        element_->GetDocument(),
        BindRepeating(&VisibilityObserver::Deliver, WrapWeakPersistent(this)),
        LocalFrameUkmAggregator::kIntersectionObservationInternalCount,
        std::move(params));
  }

  void Deliver(const HeapVector<Member<IntersectionObserverEntry>>& entries) {
    CHECK_EQ(entries.size(), 1u);
    if (entries[0]->isVisible()) {
      if (!visibility_timer_.IsActive()) {
        visibility_timer_.StartOneShot(minimum_visible_duration_, FROM_HERE);
      }
    } else {
      visibility_timer_.Stop();
    }
  }

  void Trace(Visitor* visitor) const {
    visitor->Trace(element_);
    visitor->Trace(observer_);
    visitor->Trace(visibility_timer_);
  }

  void Start() { observer_->observe(element_); }

  void Disconnect() {
    visibility_timer_.Stop();
    callback_.Reset();
    if (observer_) {
      observer_->disconnect();
      observer_ = nullptr;
    }
  }

 private:
  // Fired when the visibility timer expires (i.e., the element has been
  // visible for `minimum_visible_duration_`). It invokes `callback_`.
  void VisibilityTimerFired(TimerBase*) {
    DeliverResult();
    if (observer_) {
      observer_->disconnect();
      observer_ = nullptr;
    }
  }

  void DeliverResult() {
    if (callback_) {
      std::move(callback_).Run();
    }
    visibility_timer_.Stop();
  }

  Member<Element> element_;
  // The minimum continuous duration a element must remain visible in the
  // viewport before `callback_` is triggered.
  base::TimeDelta minimum_visible_duration_;
  // Callback to be invoked when the monitored element meets the visibility
  // criteria.
  base::OnceClosure callback_;
  // Timer used to track how long an element has been continuously visible.
  HeapTaskRunnerTimer<VisibilityObserver> visibility_timer_;
  // `IntersectionObserver` used to monitor the visibility of a form control.
  Member<IntersectionObserver> observer_;
};

}  // namespace

base::ScopedClosureRunner WebElement::MonitorVisibility(
    base::TimeDelta minimum_visible_duration,
    base::OnceClosure callback) {
  CHECK(callback);
  CHECK(!IsNull());

  auto* observer = MakeGarbageCollected<VisibilityObserver>(
      const_cast<Element*>(ConstUnwrap<Element>()), minimum_visible_duration,
      std::move(callback));
  observer->Start();

  return base::ScopedClosureRunner(
      BindOnce(&VisibilityObserver::Disconnect, WrapPersistent(observer)));
}

}  // namespace blink
