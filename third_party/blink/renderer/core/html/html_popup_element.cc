// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/html_popup_element.h"

#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"

namespace blink {

HTMLPopupElement::HTMLPopupElement(Document& document)
    : HTMLElement(html_names::kPopupTag, document) {
  DCHECK(RuntimeEnabledFeatures::HTMLPopupElementEnabled());
  UseCounter::Count(document, WebFeature::kPopupElement);
}

void HTMLPopupElement::PopUntilElementReached(HTMLPopupElement* endpoint) {
  HTMLPopupElement* element;
  while ((element = GetDocument().TopmostPopupElement()) != endpoint) {
    DCHECK(element) << "endpoint not found in element stack";
    element->hide();
  }
}

void HTMLPopupElement::hide() {
  if (!FastHasAttribute(html_names::kOpenAttr))
    return;
  PopUntilElementReached(this);
  GetDocument().PopPopupElement(this);
  SetBooleanAttribute(html_names::kOpenAttr, false);
  ScheduleHideEvent();
}

void HTMLPopupElement::ScheduleHideEvent() {
  Event* event = Event::Create(event_type_names::kHide);
  event->SetTarget(this);
  GetDocument().EnqueueAnimationFrameEvent(event);
}

void HTMLPopupElement::show() {
  if (FastHasAttribute(html_names::kOpenAttr))
    return;
  if (!isConnected())
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
  PopUntilElementReached(parent_popup);
  SetBooleanAttribute(html_names::kOpenAttr, true);
  GetDocument().PushNewPopupElement(this);
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

}  // namespace blink
