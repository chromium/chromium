// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/html_popup_element.h"

#include "third_party/blink/renderer/core/css/style_change_reason.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/events/keyboard_event.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"

namespace blink {

HTMLPopupElement::HTMLPopupElement(Document& document)
    : HTMLElement(html_names::kPopupTag, document), open_(false) {
  DCHECK(RuntimeEnabledFeatures::HTMLPopupElementEnabled());
  UseCounter::Count(document, WebFeature::kPopupElement);
}


void HTMLPopupElement::MarkStyleDirty() {
  SetNeedsStyleRecalc(kLocalStyleChange,
                      StyleChangeReasonForTracing::Create(
                          style_change_reason::kPopupVisibilityChange));
}

bool HTMLPopupElement::open() const {
  return open_;
}

void HTMLPopupElement::hide() {
  if (!open_)
    return;
  GetDocument().HideAllPopupsUntil(this);
  PopPopupElement(this);
  open_ = false;
  PseudoStateChanged(CSSSelector::kPseudoPopupOpen);
  MarkStyleDirty();
  ScheduleHideEvent();
}

void HTMLPopupElement::ScheduleHideEvent() {
  Event* event = Event::Create(event_type_names::kHide);
  event->SetTarget(this);
  GetDocument().EnqueueAnimationFrameEvent(event);
}

void HTMLPopupElement::show() {
  if (open_ || !isConnected())
    return;

  // Only hide popups up to this popup's ancestral popup.
  GetDocument().HideAllPopupsUntil(NearestOpenAncestralPopup(this));
  open_ = true;
  PseudoStateChanged(CSSSelector::kPseudoPopupOpen);
  PushNewPopupElement(this);
  MarkStyleDirty();
}

void HTMLPopupElement::PushNewPopupElement(HTMLPopupElement* popup) {
  auto& stack = GetDocument().PopupElementStack();
  DCHECK(!stack.Contains(popup));
  stack.push_back(popup);
  GetDocument().AddToTopLayer(popup);
}

void HTMLPopupElement::PopPopupElement(HTMLPopupElement* popup) {
  auto& stack = GetDocument().PopupElementStack();
  DCHECK(stack.back() == popup);
  stack.pop_back();
  GetDocument().RemoveFromTopLayer(popup);
}

HTMLPopupElement* HTMLPopupElement::TopmostPopupElement() {
  auto& stack = GetDocument().PopupElementStack();
  return stack.IsEmpty() ? nullptr : stack.back();
}

Element* HTMLPopupElement::AnchorElement() const {
  const AtomicString& anchor_id = FastGetAttribute(html_names::kAnchorAttr);
  if (anchor_id.IsNull())
    return nullptr;
  if (!IsInTreeScope())
    return nullptr;
  if (Element* anchor = GetTreeScope().getElementById(anchor_id))
    return anchor;
  return nullptr;
}

typedef HeapHashMap<Member<const Element>, Member<const HTMLPopupElement>>
    ElementToPopupMap;
const HTMLPopupElement* HTMLPopupElement::NearestOpenAncestralPopup(
    Node* start_node) {
  DCHECK(start_node);
  // We need to walk up from the start node to see if there
  // is a parent popup, or the anchor for a popup, or an invoking
  // element (which has the "popup" attribute). There can be
  // multiple popups for a single anchor element, and there can
  // be multiple invoking elements for each popup, but we will
  // stop on any of them.

  // Since there can be multiple popups for a single anchor element, just store
  // the popup that is highest (last) on the popup stack for each anchor.
  ElementToPopupMap anchors;
  Document& document = start_node->GetDocument();
  for (auto popup : document.PopupElementStack()) {
    if (const auto* anchor = popup->AnchorElement())
      anchors.Set(anchor, popup);
  }
  // Each popup can be invoked by multiple elements through the "popup"
  // attribute. We need to build a list of all such elements for all open
  // popups.
  ElementToPopupMap invokers;
  for (auto popup : document.PopupElementStack()) {
    for (auto invoker : popup->invokers_)
      invokers.Set(invoker, popup);
  }
  // TODO(masonfreed) Should this be a flat tree parent traversal?
  for (Node* current_node = start_node; current_node;
       current_node = current_node->ParentOrShadowHostNode()) {
    // Parent popup element (or the start_node itself, if <popup>).
    if (auto* popup = DynamicTo<HTMLPopupElement>(current_node)) {
      if (popup->open())
        return popup;
    }
    if (Element* current_element = DynamicTo<Element>(current_node)) {
      // Anchor element for an open popup.
      if (anchors.Contains(current_element))
        return anchors.at(current_element);
      // Invoking element
      if (invokers.Contains(current_element))
        return invokers.at(current_element);
    }
  }

  // If the starting element is a popup, we need to check for ancestors
  // of its anchor and invoking elements also.
  if (const auto* start_popup = DynamicTo<HTMLPopupElement>(start_node)) {
    auto* anchor_ancestor =
        start_popup->AnchorElement()
            ? NearestOpenAncestralPopup(start_popup->AnchorElement())
            : nullptr;
    if (anchor_ancestor)
      return anchor_ancestor;
    for (auto invoker : start_popup->invokers_) {
      if (auto* invoker_ancestor = NearestOpenAncestralPopup(invoker)) {
        return invoker_ancestor;
      }
    }
  }
  return nullptr;
}

void HTMLPopupElement::HandleLightDismiss(const Event& event) {
  auto* target_node = event.target()->ToNode();
  if (!target_node)
    return;
  auto& document = target_node->GetDocument();
  DCHECK(document.PopupShowing());
  const AtomicString& event_type = event.type();
  if (event_type == event_type_names::kClick) {
    document.HideAllPopupsUntil(NearestOpenAncestralPopup(target_node));
  } else if (event_type == event_type_names::kKeydown) {
    const KeyboardEvent* key_event = DynamicTo<KeyboardEvent>(event);
    if (key_event && key_event->key() == "Escape") {
      // Escape key just pops the topmost <popup> off the stack.
      document.HideTopmostPopupElement();
    }
  } else if (event_type == event_type_names::kScroll) {
    // Close all popups upon scroll.
    document.HideAllPopupsUntil(nullptr);
  }
}

void HTMLPopupElement::AddInvoker(Element* invoker) {
  // TODO(masonfreed) This needs to be called anywhere
  // invokers might change, e.g. when attributes are added
  // or removed from invoking elements, or when DOM tree
  // mutations occur that change the resolved idrefs.
  DCHECK(invoker);
  invokers_.insert(invoker);
}

void HTMLPopupElement::RemoveInvoker(Element* invoker) {
  DCHECK(invoker);
  invokers_.erase(invoker);
}

void HTMLPopupElement::Trace(Visitor* visitor) const {
  visitor->Trace(invokers_);
  HTMLElement::Trace(visitor);
}

}  // namespace blink
