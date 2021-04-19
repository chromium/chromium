// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/html_popup_element.h"

#include "third_party/blink/renderer/core/css/style_change_reason.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/events/keyboard_event.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/html/forms/html_select_menu_element.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/platform/geometry/float_rect.h"
#include "third_party/blink/renderer/platform/geometry/int_rect.h"
#include "third_party/blink/renderer/platform/geometry/length.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"

namespace blink {

HTMLPopupElement::HTMLPopupElement(Document& document)
    : HTMLElement(html_names::kPopupTag, document),
      open_(false),
      had_initiallyopen_when_parsed_(false),
      invoker_(nullptr),
      needs_repositioning_for_select_menu_(false),
      owner_select_menu_element_(nullptr) {
  DCHECK(RuntimeEnabledFeatures::HTMLPopupElementEnabled());
  UseCounter::Count(document, WebFeature::kPopupElement);
  SetHasCustomStyleCallbacks();
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
  open_ = false;
  invoker_ = nullptr;
  if (!isConnected())
    return;
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

void HTMLPopupElement::Invoke(Element* invoker) {
  DCHECK(invoker);
  invoker_ = invoker;
  show();
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

Node::InsertionNotificationRequest HTMLPopupElement::InsertedInto(
    ContainerNode& insertion_point) {
  HTMLElement::InsertedInto(insertion_point);

  if (had_initiallyopen_when_parsed_) {
    DCHECK(isConnected()) << "This should be being inserted by the parser";
    // If a <popup> has the initiallyopen attribute upon page
    // load, and it is the first such popup, show it.
    if (!GetDocument().PopupShowing())
      show();
    had_initiallyopen_when_parsed_ = false;
  }
  return kInsertionDone;
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

const HTMLPopupElement* HTMLPopupElement::NearestOpenAncestralPopup(
    Node* start_node) {
  if (!start_node)
    return nullptr;
  // We need to walk up from the start node to see if there is a parent popup,
  // or the anchor for a popup, or an invoking element (which has the "popup"
  // attribute). There can be multiple popups for a single anchor element, and
  // an anchor for one popup can also be an invoker for a different popup, but
  // we will stop on any of them. Therefore, just store the popup that is
  // highest (last) on the popup stack for each anchor and/or invoker.
  HeapHashMap<Member<const Element>, Member<const HTMLPopupElement>>
      anchors_and_invokers;
  Document& document = start_node->GetDocument();
  for (auto popup : document.PopupElementStack()) {
    if (const auto* anchor = popup->AnchorElement())
      anchors_and_invokers.Set(anchor, popup);
    if (const auto* invoker = popup->invoker_.Get())
      anchors_and_invokers.Set(invoker, popup);
  }
  // TODO(masonf) Should this be a flat tree parent traversal?
  for (Node* current_node = start_node; current_node;
       current_node = current_node->ParentOrShadowHostNode()) {
    // Parent popup element (or the start_node itself, if <popup>).
    if (auto* popup = DynamicTo<HTMLPopupElement>(current_node)) {
      if (popup->open())
        return popup;
    }
    if (Element* current_element = DynamicTo<Element>(current_node)) {
      if (anchors_and_invokers.Contains(current_element))
        return anchors_and_invokers.at(current_element);
    }
  }

  // If the starting element is a popup, we need to check for ancestors
  // of its anchor and invoking element also.
  if (const auto* start_popup = DynamicTo<HTMLPopupElement>(start_node)) {
    if (auto* anchor_ancestor =
            NearestOpenAncestralPopup(start_popup->AnchorElement())) {
      return anchor_ancestor;
    }
    if (auto* invoker_ancestor =
            NearestOpenAncestralPopup(start_popup->invoker_)) {
      return invoker_ancestor;
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
ComputedStyle* HTMLPopupElement::CustomStyleForLayoutObject(
    const StyleRecalcContext& style_recalc_context) {
  ComputedStyle* style = OriginalStyleForLayoutObject(style_recalc_context);
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

  IntRect anchor_rect_in_screen = RoundedIntRect(
      owner_select_menu_element_->GetBoundingClientRectNoLifecycleUpdate());
  IntRect avail_rect =
      IntRect(0, 0, window->innerWidth(), window->innerHeight());

  // Position the listbox part where is more space available.
  const int available_space_above = anchor_rect_in_screen.Y() - avail_rect.Y();
  const int available_space_below =
      avail_rect.MaxY() - anchor_rect_in_screen.MaxY();
  if (available_space_below < available_space_above) {
    style.SetMaxHeight(Length::Fixed(available_space_above));
    style.SetBottom(
        Length::Fixed(avail_rect.MaxY() - anchor_rect_in_screen.Y()));
    style.SetTop(Length::Auto());
  } else {
    style.SetMaxHeight(Length::Fixed(available_space_below));
    style.SetTop(Length::Fixed(anchor_rect_in_screen.MaxY()));
  }

  const int available_space_if_left_anchored =
      avail_rect.MaxX() - anchor_rect_in_screen.X();
  const int available_space_if_right_anchored =
      anchor_rect_in_screen.MaxX() - avail_rect.X();
  style.SetMinWidth(Length::Fixed(anchor_rect_in_screen.Width()));
  if (available_space_if_left_anchored > anchor_rect_in_screen.Width() ||
      available_space_if_left_anchored > available_space_if_right_anchored) {
    style.SetLeft(Length::Fixed(anchor_rect_in_screen.X()));
    style.SetMaxWidth(Length::Fixed(available_space_if_left_anchored));
  } else {
    style.SetRight(
        Length::Fixed(avail_rect.MaxX() - anchor_rect_in_screen.MaxX()));
    style.SetLeft(Length::Auto());
    style.SetMaxWidth(Length::Fixed(available_space_if_right_anchored));
  }

  needs_repositioning_for_select_menu_ = false;
}

void HTMLPopupElement::Trace(Visitor* visitor) const {
  visitor->Trace(invoker_);
  visitor->Trace(owner_select_menu_element_);
  HTMLElement::Trace(visitor);
}

}  // namespace blink
