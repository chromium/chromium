// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/media_controls/elements/media_control_popup_menu_element.h"

#include "third_party/blink/renderer/core/css/css_property_value_set.h"
#include "third_party/blink/renderer/core/css/css_style_declaration.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/dom/events/native_event_listener.h"
#include "third_party/blink/renderer/core/events/keyboard_event.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/geometry/dom_rect.h"
#include "third_party/blink/renderer/core/html/media/html_media_element.h"
#include "third_party/blink/renderer/modules/media_controls/elements/media_control_elements_helper.h"
#include "third_party/blink/renderer/modules/media_controls/elements/media_control_overflow_menu_button_element.h"
#include "third_party/blink/renderer/modules/media_controls/media_controls_impl.h"
#include "third_party/blink/renderer/platform/keyboard_codes.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

class MediaControlPopupMenuElement::EventListener final
    : public NativeEventListener {
 public:
  explicit EventListener(MediaControlPopupMenuElement* popup_menu)
      : popup_menu_(popup_menu) {}

  ~EventListener() final = default;

  void StartListening() {
    popup_menu_->addEventListener(event_type_names::kKeydown, this, false);

    LocalDOMWindow* window = popup_menu_->GetDocument().domWindow();
    window->addEventListener(event_type_names::kScroll, this, true);
    if (DOMWindow* outer_window = window->top()) {
      if (outer_window != window)
        outer_window->addEventListener(event_type_names::kScroll, this, true);
      outer_window->addEventListener(event_type_names::kResize, this, true);
    }
  }

  void StopListening() {
    popup_menu_->removeEventListener(event_type_names::kKeydown, this, false);

    LocalDOMWindow* window = popup_menu_->GetDocument().domWindow();
    window->removeEventListener(event_type_names::kScroll, this, true);
    if (DOMWindow* outer_window = window->top()) {
      if (outer_window != window) {
        outer_window->removeEventListener(event_type_names::kScroll, this,
                                          true);
      }
      outer_window->removeEventListener(event_type_names::kResize, this, true);
    }
  }

  void Trace(blink::Visitor* visitor) final {
    NativeEventListener::Trace(visitor);
    visitor->Trace(popup_menu_);
  }

 private:
  void Invoke(ExecutionContext*, Event* event) final {
    if (event->type() == event_type_names::kKeydown &&
        event->IsKeyboardEvent()) {
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
          To<Element>(event->target()->ToNode())->DispatchSimulatedClick(event);
          break;
        default:
          handled = false;
      }

      if (handled) {
        event->stopPropagation();
        event->SetDefaultHandled();
      }
    } else if (event->type() == event_type_names::kResize ||
               event->type() == event_type_names::kScroll) {
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
      event_listener_ = MakeGarbageCollected<EventListener>(this);
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
  if (event.type() == event_type_names::kPointermove &&
      event.target() != this) {
    To<Element>(event.target()->ToNode())->focus();
    last_focused_element_ = To<Element>(event.target()->ToNode());
  } else if (event.type() == event_type_names::kFocusout) {
    GetDocument()
        .GetTaskRunner(TaskType::kMediaElementEvent)
        ->PostTask(FROM_HERE,
                   WTF::Bind(&MediaControlPopupMenuElement::HideIfNotFocused,
                             WrapWeakPersistent(this)));
  } else if (event.type() == event_type_names::kClick &&
             event.target() != this) {
    // Since event.target() != this, we know that one of our children was
    // clicked.
    OnItemSelected();

    event.stopPropagation();
    event.SetDefaultHandled();
  } else if (event.type() == event_type_names::kFocus) {
    // When popup menu gain focus from scrolling, switch focus
    // back to the last focused item in the menu
    DCHECK(last_focused_element_);
    last_focused_element_->focus();
  }

  MediaControlDivElement::DefaultEventHandler(event);
}

bool MediaControlPopupMenuElement::KeepEventInNode(const Event& event) const {
  return MediaControlElementsHelper::IsUserInteractionEvent(event);
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
  visitor->Trace(last_focused_element_);
}

MediaControlPopupMenuElement::MediaControlPopupMenuElement(
    MediaControlsImpl& media_controls)
    : MediaControlDivElement(media_controls) {
  SetIsWanted(false);
}

void MediaControlPopupMenuElement::SetPosition() {
  // The popup is positioned slightly on the inside of the bottom right corner.
  static constexpr int kPopupMenuMarginPx = 4;
  static const char kImportant[] = "important";
  static const char kPx[] = "px";

  DOMRect* bounding_client_rect = PopupAnchor()->getBoundingClientRect();
  LocalDOMWindow* dom_window = GetDocument().domWindow();

  DCHECK(bounding_client_rect);
  DCHECK(dom_window);

  WTF::String bottom_str_value =
      WTF::String::Number(dom_window->innerHeight() -
                          bounding_client_rect->bottom() + kPopupMenuMarginPx) +
      kPx;
  WTF::String right_str_value =
      WTF::String::Number(dom_window->innerWidth() -
                          bounding_client_rect->right() + kPopupMenuMarginPx) +
      kPx;

  style()->setProperty(&GetDocument(), "bottom", bottom_str_value, kImportant,
                       ASSERT_NO_EXCEPTION);
  style()->setProperty(&GetDocument(), "right", right_str_value, kImportant,
                       ASSERT_NO_EXCEPTION);
}

Element* MediaControlPopupMenuElement::PopupAnchor() const {
  return &GetMediaControls().OverflowButton();
}

void MediaControlPopupMenuElement::HideIfNotFocused() {
  if (!IsWanted())
    return;

  // Cancel hiding if the focused element is a descendent of this element
  auto* focused_element = GetDocument().FocusedElement();
  while (focused_element) {
    if (focused_element == this) {
      return;
    }

    focused_element = focused_element->parentElement();
  }

  SetIsWanted(false);
}

// Focus the given item in the list if it is displayed. Returns whether it was
// focused.
bool MediaControlPopupMenuElement::FocusListItemIfDisplayed(Node* node) {
  auto* element = To<Element>(node);

  if (!element->InlineStyle() ||
      !element->InlineStyle()->HasProperty(CSSPropertyID::kDisplay)) {
    element->focus();
    last_focused_element_ = element;
    return true;
  }

  return false;
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
  PopupAnchor()->focus();
}

}  // namespace blink
