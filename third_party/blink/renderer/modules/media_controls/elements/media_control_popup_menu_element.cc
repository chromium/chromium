// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/media_controls/elements/media_control_popup_menu_element.h"

#include "third_party/blink/renderer/core/css/css_property_value_set.h"
#include "third_party/blink/renderer/core/css/css_style_declaration.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/dom/events/event_listener.h"
#include "third_party/blink/renderer/core/events/keyboard_event.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/geometry/dom_rect.h"
#include "third_party/blink/renderer/core/html/media/html_media_element.h"
#include "third_party/blink/renderer/modules/media_controls/elements/media_control_elements_helper.h"
#include "third_party/blink/renderer/modules/media_controls/elements/media_control_overflow_menu_button_element.h"
#include "third_party/blink/renderer/modules/media_controls/media_controls_impl.h"
#include "third_party/blink/renderer/platform/keyboard_codes.h"

namespace blink {

namespace {

// Focus the given item in the list if it is displayed. Returns whether it was
// focused.
bool FocusListItemIfDisplayed(Node* node) {
  Element* element = ToElement(node);

  if (!element->InlineStyle() ||
      !element->InlineStyle()->HasProperty(CSSPropertyDisplay)) {
    element->focus();
    return true;
  }

  return false;
}

}  // anonymous namespace

class MediaControlPopupMenuElement::EventListener final
    : public blink::EventListener {
 public:
  explicit EventListener(MediaControlPopupMenuElement* popup_menu)
      : blink::EventListener(kCPPEventListenerType), popup_menu_(popup_menu) {}

  ~EventListener() final = default;

  void StartListening() {
    popup_menu_->addEventListener(EventTypeNames::keydown, this, false);

    LocalDOMWindow* window = popup_menu_->GetDocument().domWindow();
    window->addEventListener(EventTypeNames::scroll, this, true);
    if (DOMWindow* outer_window = window->top()) {
      if (outer_window != window)
        outer_window->addEventListener(EventTypeNames::scroll, this, true);
      outer_window->addEventListener(EventTypeNames::resize, this, true);
    }
  }

  void StopListening() {
    popup_menu_->removeEventListener(EventTypeNames::keydown, this, false);

    LocalDOMWindow* window = popup_menu_->GetDocument().domWindow();
    window->removeEventListener(EventTypeNames::scroll, this, true);
    if (DOMWindow* outer_window = window->top()) {
      if (outer_window != window)
        outer_window->removeEventListener(EventTypeNames::scroll, this, true);
      outer_window->removeEventListener(EventTypeNames::resize, this, true);
    }
  }

  bool operator==(const blink::EventListener& other) const final {
    return &other == this;
  }

  void Trace(blink::Visitor* visitor) final {
    blink::EventListener::Trace(visitor);
    visitor->Trace(popup_menu_);
  }

 private:
  void handleEvent(ExecutionContext*, Event* event) final {
    if (event->type() == EventTypeNames::keydown && event->IsKeyboardEvent()) {
      KeyboardEvent* keyboard_event = ToKeyboardEvent(event);
      bool handled = true;

      switch (keyboard_event->keyCode()) {
        case VKEY_TAB:
          keyboard_event->shiftKey() ? popup_menu_->SelectNextItem()
                                     : popup_menu_->SelectPreviousitem();
          break;
        case VKEY_UP:
          popup_menu_->SelectNextItem();
          break;
        case VKEY_DOWN:
          popup_menu_->SelectPreviousitem();
          break;
        case VKEY_ESCAPE:
          popup_menu_->CloseFromKeyboard();
          break;
        case VKEY_RETURN:
        case VKEY_SPACE:
          ToElement(event->target()->ToNode())->DispatchSimulatedClick(event);
          break;
        default:
          handled = false;
      }

      if (handled) {
        event->stopPropagation();
        event->SetDefaultHandled();
      }
    } else if (event->type() == EventTypeNames::resize ||
               event->type() == EventTypeNames::scroll) {
      popup_menu_->SetIsWanted(false);
    }
  }

  Member<MediaControlPopupMenuElement> popup_menu_;
};

MediaControlPopupMenuElement::~MediaControlPopupMenuElement() = default;

void MediaControlPopupMenuElement::SetIsWanted(bool wanted) {
  MediaControlDivElement::SetIsWanted(wanted);

  if (wanted) {
    SetPosition();

    SelectFirstItem();

    if (!event_listener_)
      event_listener_ = new EventListener(this);
    event_listener_->StartListening();
  } else {
    if (event_listener_)
      event_listener_->StopListening();
  }
}

void MediaControlPopupMenuElement::OnItemSelected() {
  SetIsWanted(false);
}

void MediaControlPopupMenuElement::DefaultEventHandler(Event& event) {
  if (event.type() == EventTypeNames::pointermove) {
    ToElement(event.target()->ToNode())->focus();
  } else if (event.type() == EventTypeNames::focusout) {
    GetDocument()
        .GetTaskRunner(TaskType::kMediaElementEvent)
        ->PostTask(FROM_HERE,
                   WTF::Bind(&MediaControlPopupMenuElement::HideIfNotFocused,
                             WrapWeakPersistent(this)));
  } else if (event.type() == EventTypeNames::click) {
    OnItemSelected();

    event.stopPropagation();
    event.SetDefaultHandled();
  }

  MediaControlDivElement::DefaultEventHandler(event);
}

bool MediaControlPopupMenuElement::KeepEventInNode(const Event& event) const {
  return MediaControlsImpl::IsModern() &&
         MediaControlElementsHelper::IsUserInteractionEvent(event);
}

void MediaControlPopupMenuElement::RemovedFrom(ContainerNode& container) {
  if (IsWanted())
    SetIsWanted(false);
  event_listener_ = nullptr;

  MediaControlDivElement::RemovedFrom(container);
}

void MediaControlPopupMenuElement::Trace(blink::Visitor* visitor) {
  MediaControlDivElement::Trace(visitor);
  visitor->Trace(event_listener_);
}

MediaControlPopupMenuElement::MediaControlPopupMenuElement(
    MediaControlsImpl& media_controls,
    MediaControlElementType type)
    : MediaControlDivElement(media_controls, type) {
  SetIsWanted(false);
}

void MediaControlPopupMenuElement::SetPosition() {
  // The popup is positioned slightly on the inside of the bottom right corner.
  static constexpr int kPopupMenuMarginPx = 4;
  static const char kImportant[] = "important";
  static const char kPx[] = "px";

  DOMRect* bounding_client_rect =
      EffectivePopupAnchor()->getBoundingClientRect();
  LocalDOMWindow* dom_window = GetDocument().domWindow();

  DCHECK(bounding_client_rect);
  DCHECK(dom_window);

  WTF::String bottom_str_value =
      WTF::String::Number(dom_window->innerHeight() -
                          bounding_client_rect->bottom() + kPopupMenuMarginPx);
  WTF::String right_str_value =
      WTF::String::Number(dom_window->innerWidth() -
                          bounding_client_rect->right() + kPopupMenuMarginPx);

  bottom_str_value.append(kPx);
  right_str_value.append(kPx);

  style()->setProperty(&GetDocument(), "bottom", bottom_str_value, kImportant,
                       ASSERT_NO_EXCEPTION);
  style()->setProperty(&GetDocument(), "right", right_str_value, kImportant,
                       ASSERT_NO_EXCEPTION);
}

Element* MediaControlPopupMenuElement::EffectivePopupAnchor() const {
  return MediaControlsImpl::IsModern() ? &GetMediaControls().OverflowButton()
                                       : PopupAnchor();
}

void MediaControlPopupMenuElement::HideIfNotFocused() {
  if (!IsWanted())
    return;

  if (!GetDocument().FocusedElement() ||
      GetDocument().FocusedElement()->parentElement() != this) {
    SetIsWanted(false);
  }
}

void MediaControlPopupMenuElement::SelectFirstItem() {
  for (Node* target = lastChild(); target; target = target->previousSibling()) {
    if (FocusListItemIfDisplayed(target))
      break;
  }
}

void MediaControlPopupMenuElement::SelectNextItem() {
  Element* focused_element = GetDocument().FocusedElement();
  if (!focused_element || focused_element->parentElement() != this)
    return;

  for (Node* target = focused_element->previousSibling(); target;
       target = target->previousSibling()) {
    if (FocusListItemIfDisplayed(target))
      break;
  }
}

void MediaControlPopupMenuElement::SelectPreviousitem() {
  Element* focused_element = GetDocument().FocusedElement();
  if (!focused_element || focused_element->parentElement() != this)
    return;

  for (Node* target = focused_element->nextSibling(); target;
       target = target->nextSibling()) {
    if (FocusListItemIfDisplayed(target))
      break;
  }
}

void MediaControlPopupMenuElement::CloseFromKeyboard() {
  SetIsWanted(false);
  EffectivePopupAnchor()->focus();
}

}  // namespace blink
