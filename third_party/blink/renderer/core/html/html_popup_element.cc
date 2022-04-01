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
      had_initiallyopen_when_parsed_(false),
      needs_repositioning_for_select_menu_(false),
      owner_select_menu_element_(nullptr) {
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
  needs_repositioning_for_select_menu_ = false;
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

// TODO(crbug.com/1197720): The popup position should be provided by the new
// anchored positioning scheme.
void HTMLPopupElement::SetNeedsRepositioningForSelectMenu(bool flag) {
  if (needs_repositioning_for_select_menu_ == flag)
    return;

  needs_repositioning_for_select_menu_ = flag;
  if (needs_repositioning_for_select_menu_)
    MarkStyleDirty();
}

// TODO(crbug.com/1197720): The popup position should be provided by the new
// anchored positioning scheme.
bool HTMLPopupElement::NeedsRepositioningForSelectMenu() const {
  return needs_repositioning_for_select_menu_;
}

// TODO(crbug.com/1197720): The popup position should be provided by the new
// anchored positioning scheme.
void HTMLPopupElement::SetOwnerSelectMenuElement(
    HTMLSelectMenuElement* owner_select_menu_element) {
  DCHECK(RuntimeEnabledFeatures::HTMLSelectMenuElementEnabled());
  owner_select_menu_element_ = owner_select_menu_element;
}

// TODO(crbug.com/1197720): The popup position should be provided by the new
// anchored positioning scheme.
scoped_refptr<ComputedStyle> HTMLPopupElement::CustomStyleForLayoutObject(
    const StyleRecalcContext& style_recalc_context) {
  scoped_refptr<ComputedStyle> style =
      OriginalStyleForLayoutObject(style_recalc_context);
  if (NeedsRepositioningForSelectMenu())
    AdjustPopupPositionForSelectMenu(*style);
  return style;
}

// TODO(crbug.com/1197720): The popup position should be provided by the new
// anchored positioning scheme.
void HTMLPopupElement::AdjustPopupPositionForSelectMenu(ComputedStyle& style) {
  if (!needs_repositioning_for_select_menu_ || !owner_select_menu_element_)
    return;

  LocalDOMWindow* window = GetDocument().domWindow();
  if (!window)
    return;

  gfx::RectF anchor_rect_in_screen =
      owner_select_menu_element_->GetBoundingClientRectNoLifecycleUpdate();
  const float anchor_zoom = owner_select_menu_element_->GetLayoutObject()
                                ? owner_select_menu_element_->GetLayoutObject()
                                      ->StyleRef()
                                      .EffectiveZoom()
                                : 1;
  anchor_rect_in_screen.Scale(anchor_zoom);
  // Don't use the LocalDOMWindow innerHeight/innerWidth getters, as those can
  // trigger a re-entrant style and layout update.
  int avail_width = GetDocument().View()->Size().width();
  int avail_height = GetDocument().View()->Size().height();
  gfx::Rect avail_rect = gfx::Rect(0, 0, avail_width, avail_height);

  // Position the listbox part where is more space available.
  const float available_space_above =
      anchor_rect_in_screen.y() - avail_rect.y();
  const float available_space_below =
      avail_rect.bottom() - anchor_rect_in_screen.bottom();
  if (available_space_below < available_space_above) {
    style.SetMaxHeight(Length::Fixed(available_space_above));
    style.SetBottom(
        Length::Fixed(avail_rect.bottom() - anchor_rect_in_screen.y()));
    style.SetTop(Length::Auto());
  } else {
    style.SetMaxHeight(Length::Fixed(available_space_below));
    style.SetTop(Length::Fixed(anchor_rect_in_screen.bottom()));
  }

  const float available_space_if_left_anchored =
      avail_rect.right() - anchor_rect_in_screen.x();
  const float available_space_if_right_anchored =
      anchor_rect_in_screen.right() - avail_rect.x();
  style.SetMinWidth(Length::Fixed(anchor_rect_in_screen.width()));
  if (available_space_if_left_anchored > anchor_rect_in_screen.width() ||
      available_space_if_left_anchored > available_space_if_right_anchored) {
    style.SetLeft(Length::Fixed(anchor_rect_in_screen.x()));
    style.SetMaxWidth(Length::Fixed(available_space_if_left_anchored));
  } else {
    style.SetRight(
        Length::Fixed(avail_rect.right() - anchor_rect_in_screen.right()));
    style.SetLeft(Length::Auto());
    style.SetMaxWidth(Length::Fixed(available_space_if_right_anchored));
  }
}

void HTMLPopupElement::Trace(Visitor* visitor) const {
  visitor->Trace(owner_select_menu_element_);
  HTMLElement::Trace(visitor);
}

}  // namespace blink
