// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/html_popup_element.h"

#include "third_party/blink/renderer/core/css/style_change_reason.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/dom/flat_tree_traversal.h"
#include "third_party/blink/renderer/core/events/keyboard_event.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/html/forms/html_select_menu_element.h"
#include "third_party/blink/renderer/core/layout/adjust_for_absolute_zoom.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/platform/geometry/float_rect.h"
#include "third_party/blink/renderer/platform/geometry/length.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "ui/gfx/geometry/rect.h"

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
  SetFocus();
}

bool HTMLPopupElement::IsKeyboardFocusable() const {
  // Popup is not keyboard focusable.
  return false;
}
bool HTMLPopupElement::IsMouseFocusable() const {
  // Popup *is* mouse focusable.
  return true;
}

// TODO(masonf) This should really live in either Element or FocusController.
// The spec for
// https://html.spec.whatwg.org/multipage/interaction.html#get-the-focusable-area
// does not include dialogs or popups yet.
Element* HTMLPopupElement::GetFocusableArea(bool autofocus_only) const {
  Node* next = nullptr;
  for (Node* node = FlatTreeTraversal::FirstChild(*this); node; node = next) {
    if (IsA<HTMLPopupElement>(*node) || IsA<HTMLDialogElement>(*node)) {
      next = FlatTreeTraversal::NextSkippingChildren(*node, this);
      continue;
    }
    next = FlatTreeTraversal::Next(*node, this);
    auto* element = DynamicTo<Element>(node);
    if (element && element->IsFocusable() &&
        (!autofocus_only || element->IsAutofocusable())) {
      return element;
    }
  }
  return nullptr;
}

void HTMLPopupElement::focus(const FocusParams& params) {
  if (hasAttribute(html_names::kDelegatesfocusAttr)) {
    if (auto* node_to_focus = GetFocusableArea(/*autofocus_only=*/false)) {
      node_to_focus->focus(params);
    }
  } else {
    HTMLElement::focus(params);
  }
}

void HTMLPopupElement::SetFocus() {
  // The layout must be updated here because we call Element::isFocusable,
  // which requires an up-to-date layout.
  GetDocument().UpdateStyleAndLayoutTreeForNode(this);

  Element* control = nullptr;
  if (IsAutofocusable() || hasAttribute(html_names::kDelegatesfocusAttr)) {
    // If the <popup> has the autofocus or delegatesfocus, focus it.
    control = this;
  } else {
    // Otherwise, look for a child control that has the autofocus attribute.
    control = GetFocusableArea(/*autofocus_only=*/true);
  }

  // If the popup does not use autofocus or delegatesfocus, then the focus
  // should remain on the currently active element.
  // https://open-ui.org/components/popup.research.explainer#autofocus-logic
  if (!control)
    return;

  // 3. Run the focusing steps for control.
  DCHECK(control->IsFocusable());
  control->focus();

  // 4. Let topDocument be the active document of control's node document's
  // browsing context's top-level browsing context.
  // 5. If control's node document's origin is not the same as the origin of
  // topDocument, then return.
  Document& doc = control->GetDocument();
  if (!doc.IsActive())
    return;
  if (!doc.IsInMainFrame() &&
      !doc.TopFrameOrigin()->CanAccess(
          doc.GetExecutionContext()->GetSecurityOrigin())) {
    return;
  }

  // 6. Empty topDocument's autofocus candidates.
  // 7. Set topDocument's autofocus processed flag to true.
  doc.TopDocument().FinalizeAutofocus();
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

HTMLPopupElement* HTMLPopupElement::TopmostPopupElement() {
  auto& stack = GetDocument().PopupElementStack();
  return stack.IsEmpty() ? nullptr : stack.back();
}

Element* HTMLPopupElement::anchor() const {
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
    if (const auto* anchor = popup->anchor())
      anchors_and_invokers.Set(anchor, popup);
    if (const auto* invoker = popup->invoker_.Get())
      anchors_and_invokers.Set(invoker, popup);
  }
  for (Node* current_node = start_node; current_node;
       current_node = FlatTreeTraversal::Parent(*current_node)) {
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
            NearestOpenAncestralPopup(start_popup->anchor())) {
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
  if (event_type == event_type_names::kMousedown ||
      event_type == event_type_names::kScroll) {
    // - For scroll, hide everything up to the scrolled element, to allow
    //   scrolling within a popup.
    // - For mousedown, hide everything up to the clicked element. We do
    //   this on mousedown, rather than mouseup/click, for two reasons:
    //    1. This mirrors typical platform popups, which dismiss on mousedown.
    //    2. This allows a mouse-drag that starts on a popup and finishes off
    //       the popup, without light-dismissing the popup.
    document.HideAllPopupsUntil(NearestOpenAncestralPopup(target_node));
  } else if (event_type == event_type_names::kKeydown) {
    const KeyboardEvent* key_event = DynamicTo<KeyboardEvent>(event);
    if (key_event && key_event->key() == "Escape") {
      // Escape key just pops the topmost <popup> off the stack.
      document.HideTopmostPopupElement();
    }
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

  FloatRect anchor_rect_in_screen =
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
  visitor->Trace(invoker_);
  visitor->Trace(owner_select_menu_element_);
  HTMLElement::Trace(visitor);
}

}  // namespace blink
