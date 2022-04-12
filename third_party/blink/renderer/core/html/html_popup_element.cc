// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/html_popup_element.h"

#include "third_party/blink/renderer/core/css/style_change_reason.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/dom/flat_tree_traversal.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/html/forms/html_select_menu_element.h"
#include "third_party/blink/renderer/core/layout/adjust_for_absolute_zoom.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/platform/geometry/length.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_f.h"

namespace blink {

HTMLPopupElement::HTMLPopupElement(Document& document)
    : HTMLElement(html_names::kPopupTag, document),
      open_(false),
      had_initiallyopen_when_parsed_(false) {
  DCHECK(RuntimeEnabledFeatures::HTMLPopupElementEnabled());
  UseCounter::Count(document, WebFeature::kPopupElement);
  SetHasCustomStyleCallbacks();
}

void HTMLPopupElement::MarkStyleDirty() {
  // TODO(masonf): kPopupVisibilityChange can be deleted when this method
  // is deleted - this is the only use.
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
  open_ = false;
  DCHECK(isConnected());
  GetDocument().HideAllPopupsUntil(this);
  PopPopupElement(this);
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
  SetPopupFocusOnShow();
}

bool HTMLPopupElement::IsKeyboardFocusable() const {
  // Popup is not keyboard focusable.
  return false;
}
bool HTMLPopupElement::IsMouseFocusable() const {
  // Popup *is* mouse focusable.
  return true;
}

namespace {
void ShowInitiallyOpenPopup(HTMLPopupElement* popup) {
  // If a <popup> has the initiallyopen attribute upon page
  // load, and it is the first such popup, show it.
  if (popup && popup->isConnected() && !popup->GetDocument().PopupShowing()) {
    popup->show();
  }
}
}  // namespace

Node::InsertionNotificationRequest HTMLPopupElement::InsertedInto(
    ContainerNode& insertion_point) {
  HTMLElement::InsertedInto(insertion_point);

  if (had_initiallyopen_when_parsed_) {
    DCHECK(isConnected());
    had_initiallyopen_when_parsed_ = false;
    GetDocument()
        .GetTaskRunner(TaskType::kDOMManipulation)
        ->PostTask(FROM_HERE, WTF::Bind(&ShowInitiallyOpenPopup,
                                        WrapWeakPersistent(this)));
  }
  return kInsertionDone;
}

void HTMLPopupElement::RemovedFrom(ContainerNode& insertion_point) {
  // If a popup is removed from the document, make sure it gets
  // removed from the popup element stack and the top layer.
  if (insertion_point.isConnected()) {
    insertion_point.GetDocument().HidePopupIfShowing(this);
  }
  HTMLElement::RemovedFrom(insertion_point);
}

void HTMLPopupElement::ParserDidSetAttributes() {
  HTMLElement::ParserDidSetAttributes();

  if (FastHasAttribute(html_names::kInitiallyopenAttr)) {
    DCHECK(!isConnected());
    had_initiallyopen_when_parsed_ = true;
  }
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

}  // namespace blink
