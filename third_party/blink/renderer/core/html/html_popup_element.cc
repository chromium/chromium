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

  // Only hide popups up to the anchor element's nearest shadow-including
  // ancestor popup element.
  HTMLPopupElement* parent_popup = nullptr;
  if (Element* anchor = AnchorElement()) {
    for (Node* ancestor = anchor; ancestor;
         ancestor = ancestor->ParentOrShadowHostNode()) {
      if (HTMLPopupElement* ancestor_popup =
              DynamicTo<HTMLPopupElement>(ancestor)) {
        parent_popup = ancestor_popup;
        break;
      }
    }
  }
  GetDocument().HideAllPopupsUntil(parent_popup);
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

void HTMLPopupElement::HandleLightDismiss(const Event& event) {
  auto* target_node = event.target()->ToNode();
  if (!target_node)
    return;
  auto& document = target_node->GetDocument();
  DCHECK(document.PopupShowing());
  const AtomicString& event_type = event.type();
  if (event_type == event_type_names::kClick) {
    // We need to walk up from the clicked element to see if there
    // is a parent popup, or the anchor for a popup. There can be
    // multiple popups for a single anchor element, but we will
    // stop on any of them. Therefore, just store the popup that
    // is highest (last) on the popup stack for each anchor.
    HeapHashMap<Member<const Element>, Member<const HTMLPopupElement>> anchors;
    for (auto popup : document.PopupElementStack()) {
      if (auto* anchor = popup->AnchorElement()) {
        anchors.Set(anchor, popup);
      }
    }
    const HTMLPopupElement* closest_popup_parent = nullptr;
    for (Node* current_node = target_node; current_node;
         current_node = current_node->parentNode()) {
      if (auto* popup = DynamicTo<HTMLPopupElement>(current_node)) {
        closest_popup_parent = popup;
        break;
      }
      Element* current_element = DynamicTo<Element>(current_node);
      if (current_element && anchors.Contains(current_element)) {
        closest_popup_parent = anchors.at(current_element);
        break;
      }
    }
    document.HideAllPopupsUntil(closest_popup_parent);
  } else if (event_type == event_type_names::kKeydown) {
    const KeyboardEvent* key_event = DynamicTo<KeyboardEvent>(event);
    if (key_event && key_event->key() == "Escape") {
      // Escape key just pops the topmost <popup> off the stack.
      document.HideTopmostPopupElement();
    }
  }
}

}  // namespace blink
