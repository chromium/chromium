// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/media_controls/elements/media_control_popup_menu_element.h"

#include "third_party/blink/renderer/bindings/core/v8/v8_focus_options.h"
#include "third_party/blink/renderer/core/css/css_property_value_set.h"
#include "third_party/blink/renderer/core/css/css_style_declaration.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/dom/events/native_event_listener.h"
#include "third_party/blink/renderer/core/event_type_names.h"
#include "third_party/blink/renderer/core/events/keyboard_event.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/geometry/dom_rect.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/html/media/html_media_element.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/keywords.h"
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
    popup_menu_->addEventListener(event_type_names::kBeforetoggle, this, false);

    LocalDOMWindow* window = popup_menu_->GetDocument().domWindow();
    if (!window)
      return;

    window->addEventListener(event_type_names::kScroll, this, true);
    if (DOMWindow* outer_window = window->top()) {
      if (outer_window != window)
        outer_window->addEventListener(event_type_names::kScroll, this, true);
      outer_window->addEventListener(event_type_names::kResize, this, true);
    }
  }

  void StopListening() {
    popup_menu_->removeEventListener(event_type_names::kKeydown, this, false);
    popup_menu_->removeEventListener(event_type_names::kBeforetoggle, this,
                                     false);

    LocalDOMWindow* window = popup_menu_->GetDocument().domWindow();
    if (!window)
      return;

    window->removeEventListener(event_type_names::kScroll, this, true);
    if (DOMWindow* outer_window = window->top()) {
      if (outer_window != window) {
        outer_window->removeEventListener(event_type_names::kScroll, this,
                                          true);
      }
      outer_window->removeEventListener(event_type_names::kResize, this, true);
    }
  }

  void Trace(Visitor* visitor) const final {
    NativeEventListener::Trace(visitor);
    visitor->Trace(popup_menu_);
  }

 private:
  void Invoke(ExecutionContext*, Event* event) final {
    if (event->type() == event_type_names::kKeydown) {
      auto* keyboard_event = To<KeyboardEvent>(event);
      bool handled = true;

      switch (keyboard_event->keyCode()) {
        case VKEY_TAB:
          keyboard_event->shiftKey() ? popup_menu_->SelectPreviousItem()
                                     : popup_menu_->SelectNextItem();
          break;
        case VKEY_UP:
          popup_menu_->SelectPreviousItem();
          break;
        case VKEY_DOWN:
          popup_menu_->SelectNextItem();
          break;
        case VKEY_ESCAPE:
          popup_menu_->CloseFromKeyboard();
          break;
        case VKEY_RETURN:
        case VKEY_SPACE:
          To<Element>(event->target()->ToNode())->DispatchSimulatedClick(event);
          popup_menu_->FocusPopupAnchorIfOverflowClosed();
          break;
        default:
          handled = false;
      }

      if (handled) {
        event->stopPropagation();
        event->SetDefaultHandled();
      }
    } else if (event->type() == event_type_names::kResize ||
               event->type() == event_type_names::kScroll ||
               event->type() == event_type_names::kBeforetoggle) {
      popup_menu_->SetIsWanted(false);
    }
  }

  Member<MediaControlPopupMenuElement> popup_menu_;
};

MediaControlPopupMenuElement::~MediaControlPopupMenuElement() = default;

void MediaControlPopupMenuElement::SetIsWanted(bool wanted) {
  MediaControlDivElement::SetIsWanted(wanted);

  // TODO(crbug.com/1466192) On systems with HTMLPopoverAttribute disabled
  // manually, popovers will not work. This feature will be removed on
  // August 9, 2023. Until then, at least don't crash the renderer.
  bool popover_enabled = RuntimeEnabledFeatures::HTMLPopoverAttributeEnabled(
      GetDocument().GetExecutionContext());

  if (wanted) {
    if (popover_enabled) {
      ShowPopoverInternal(/*invoker*/ nullptr, /*exception_state*/ nullptr);
    }
    if (!RuntimeEnabledFeatures::CSSAnchorPositioningEnabled()) {
      SetPosition();
    }

    SelectFirstItem();

    if (!event_listener_)
      event_listener_ = MakeGarbageCollected<EventListener>(this);
    event_listener_->StartListening();
  } else {
    if (event_listener_)
      event_listener_->StopListening();
    if (popover_enabled && popoverOpen()) {
      HidePopoverInternal(HidePopoverFocusBehavior::kNone,
                          HidePopoverTransitionBehavior::kNoEventsNoWaiting,
                          nullptr);
    }
  }
}

void MediaControlPopupMenuElement::OnItemSelected() {
  SetIsWanted(false);
}

void MediaControlPopupMenuElement::DefaultEventHandler(Event& event) {
  if (event.type() == event_type_names::kPointermove &&
      event.target() != this) {
    To<Element>(event.target()->ToNode())->Focus();
    last_focused_element_ = To<Element>(event.target()->ToNode());
  } else if (event.type() == event_type_names::kFocusout) {
    GetDocument()
        .GetTaskRunner(TaskType::kMediaElementEvent)
        ->PostTask(
            FROM_HERE,
            WTF::BindOnce(&MediaControlPopupMenuElement::HideIfNotFocused,
                          WrapWeakPersistent(this)));
  } else if (event.type() == event_type_names::kClick &&
             event.target() != this) {
    // Since event.target() != this, we know that one of our children was
    // clicked.
    OnItemSelected();

    event.stopPropagation();
    event.SetDefaultHandled();
  } else if (event.type() == event_type_names::kFocus &&
             event.target() == this) {
    // When the popup menu gains focus from scrolling, switch focus
    // back to the last focused item in the menu.
    if (last_focused_element_) {
      FocusOptions* focus_options = FocusOptions::Create();
      focus_options->setPreventScroll(true);
      last_focused_element_->Focus(focus_options);
    }
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

void MediaControlPopupMenuElement::Trace(Visitor* visitor) const {
  MediaControlDivElement::Trace(visitor);
  visitor->Trace(event_listener_);
  visitor->Trace(last_focused_element_);
}

MediaControlPopupMenuElement::MediaControlPopupMenuElement(
    MediaControlsImpl& media_controls)
    : MediaControlDivElement(media_controls) {
  setAttribute(html_names::kPopoverAttr, keywords::kAuto);
  SetElementAttribute(html_names::kAnchorAttr, PopupAnchor());
  SetIsWanted(false);
}

// TODO(crbug.com/1309178): This entire function and the one callsite can be
// removed once anchor positioning is enabled by default.
void MediaControlPopupMenuElement::SetPosition() {
  DCHECK(!RuntimeEnabledFeatures::CSSAnchorPositioningEnabled());
  // The popup is positioned slightly on the inside of the bottom right
  // corner.
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

  style()->setProperty(dom_window, "bottom", bottom_str_value, kImportant,
                       ASSERT_NO_EXCEPTION);
  style()->setProperty(dom_window, "right", right_str_value, kImportant,
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
    element->Focus();
    last_focused_element_ = element;
    return true;
  }

  return false;
}

void MediaControlPopupMenuElement::SelectFirstItem() {
  for (Node* target = firstChild(); target; target = target->nextSibling()) {
    if (FocusListItemIfDisplayed(target))
      break;
  }
}

void MediaControlPopupMenuElement::SelectNextItem() {
  Element* focused_element = GetDocument().FocusedElement();
  if (!focused_element || focused_element->parentElement() != this)
    return;

  for (Node* target = focused_element->nextSibling(); target;
       target = target->nextSibling()) {
    if (FocusListItemIfDisplayed(target))
      break;
  }
}

void MediaControlPopupMenuElement::SelectPreviousItem() {
  Element* focused_element = GetDocument().FocusedElement();
  if (!focused_element || focused_element->parentElement() != this)
    return;

  for (Node* target = focused_element->previousSibling(); target;
       target = target->previousSibling()) {
    if (FocusListItemIfDisplayed(target))
      break;
  }
}

void MediaControlPopupMenuElement::CloseFromKeyboard() {
  SetIsWanted(false);
  PopupAnchor()->Focus();
}

void MediaControlPopupMenuElement::FocusPopupAnchorIfOverflowClosed() {
  if (!GetMediaControls().OverflowMenuIsWanted() &&
      !GetMediaControls().PlaybackSpeedListIsWanted() &&
      !GetMediaControls().TextTrackListIsWanted()) {
    PopupAnchor()->Focus();
  }
}

}  // namespace blink
