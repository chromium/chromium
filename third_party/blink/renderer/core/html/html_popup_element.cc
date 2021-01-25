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

void HTMLPopupElement::hide() {
  if (!FastHasAttribute(html_names::kOpenAttr))
    return;
  SetBooleanAttribute(html_names::kOpenAttr, false);
  GetDocument().RemoveFromTopLayer(this);
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
  SetBooleanAttribute(html_names::kOpenAttr, true);
  GetDocument().AddToTopLayer(this);
}

void HTMLPopupElement::RemovedFrom(ContainerNode& insertion_point) {
  HTMLElement::RemovedFrom(insertion_point);
}

}  // namespace blink
