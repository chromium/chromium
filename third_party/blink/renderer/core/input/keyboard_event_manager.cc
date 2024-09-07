// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/input/keyboard_event_manager.h"

#include <memory>

#include "base/auto_reset.h"
#include "base/notreached.h"
#include "build/build_config.h"
#include "third_party/blink/public/common/input/web_input_event.h"
#include "third_party/blink/public/mojom/frame/user_activation_notification_type.mojom-blink.h"
#include "third_party/blink/public/mojom/input/focus_type.mojom-blink.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/web/web_link_preview_triggerer.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/events/simulated_click_options.h"
#include "third_party/blink/renderer/core/dom/focus_params.h"
#include "third_party/blink/renderer/core/editing/editing_utilities.h"
#include "third_party/blink/renderer/core/editing/editor.h"
#include "third_party/blink/renderer/core/events/keyboard_event.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/html/forms/html_input_element.h"
#include "third_party/blink/renderer/core/html/html_dialog_element.h"
#include "third_party/blink/renderer/core/input/event_handling_util.h"
#include "third_party/blink/renderer/core/input/input_device_capabilities.h"
#include "third_party/blink/renderer/core/input/keyboard_shortcut_recorder.h"
#include "third_party/blink/renderer/core/input/scroll_manager.h"
#include "third_party/blink/renderer/core/keywords.h"
#include "third_party/blink/renderer/core/page/focus_controller.h"
#include "third_party/blink/renderer/core/page/focusgroup_controller.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/page/spatial_navigation.h"
#include "third_party/blink/renderer/core/page/spatial_navigation_controller.h"
#include "third_party/blink/renderer/platform/keyboard_codes.h"
#include "third_party/blink/renderer/platform/widget/frame_widget.h"
#include "third_party/blink/renderer/platform/windows_keyboard_codes.h"
#include "ui/events/keycodes/dom/keycode_converter.h"

#if BUILDFLAG(IS_WIN)
#include <windows.h>
#elif BUILDFLAG(IS_MAC)
#import <Carbon/Carbon.h>
#endif

namespace blink {

namespace {

const int kVKeyProcessKey = 229;

bool IsPageUpOrDownKeyEvent(int key_code, WebInputEvent::Modifiers modifiers) {
  if (modifiers & WebInputEvent::kAltKey) {
    // Alt-Up/Down should behave like PageUp/Down on Mac. (Note that Alt-keys
    // on other platforms are suppressed due to isSystemKey being set.)
    return key_code == VKEY_UP || key_code == VKEY_DOWN;
  } else if (key_code == VKEY_PRIOR || key_code == VKEY_NEXT) {
    return modifiers == WebInputEvent::kNoModifiers;
  }

  return false;
}

bool MapKeyCodeForScroll(int key_code,
                         WebInputEvent::Modifiers modifiers,
                         mojom::blink::ScrollDirection* scroll_direction,
                         ui::ScrollGranularity* scroll_granularity,
                         WebFeature* scroll_use_uma) {
  if (modifiers & WebInputEvent::kShiftKey ||
      modifiers & WebInputEvent::kMetaKey)
    return false;

  if (modifiers & WebInputEvent::kAltKey) {
    // Alt-Up/Down should behave like PageUp/Down on Mac.  (Note that Alt-keys
    // on other platforms are suppressed due to isSystemKey being set.)
    if (key_code == VKEY_UP)
      key_code = VKEY_PRIOR;
    else if (key_code == VKEY_DOWN)
      key_code = VKEY_NEXT;
    else
      return false;
  }

  if (modifiers & WebInputEvent::kControlKey) {
    // Match FF behavior in the sense that Ctrl+home/end are the only Ctrl
    // key combinations which affect scrolling.
    if (key_code != VKEY_HOME && key_code != VKEY_END)
      return false;
  }

#if BUILDFLAG(IS_ANDROID)
  switch (key_code) {
    case VKEY_PRIOR:
      RecordKeyboardShortcutForAndroid(KeyboardShortcut::kPageUp);
      break;
    case VKEY_NEXT:
      RecordKeyboardShortcutForAndroid(KeyboardShortcut::kPageDown);
      break;
  }
#endif

  switch (key_code) {
    case VKEY_LEFT:
      *scroll_direction =
          mojom::blink::ScrollDirection::kScrollLeftIgnoringWritingMode;
      *scroll_granularity =
          RuntimeEnabledFeatures::PercentBasedScrollingEnabled()
              ? ui::ScrollGranularity::kScrollByPercentage
              : ui::ScrollGranularity::kScrollByLine;
      *scroll_use_uma = WebFeature::kScrollByKeyboardArrowKeys;
      break;
    case VKEY_RIGHT:
      *scroll_direction =
          mojom::blink::ScrollDirection::kScrollRightIgnoringWritingMode;
      *scroll_granularity =
          RuntimeEnabledFeatures::PercentBasedScrollingEnabled()
              ? ui::ScrollGranularity::kScrollByPercentage
              : ui::ScrollGranularity::kScrollByLine;
      *scroll_use_uma = WebFeature::kScrollByKeyboardArrowKeys;
      break;
    case VKEY_UP:
      *scroll_direction =
          mojom::blink::ScrollDirection::kScrollUpIgnoringWritingMode;
      *scroll_granularity =
          RuntimeEnabledFeatures::PercentBasedScrollingEnabled()
              ? ui::ScrollGranularity::kScrollByPercentage
              : ui::ScrollGranularity::kScrollByLine;
      *scroll_use_uma = WebFeature::kScrollByKeyboardArrowKeys;
      break;
    case VKEY_DOWN:
      *scroll_direction =
          mojom::blink::ScrollDirection::kScrollDownIgnoringWritingMode;
      *scroll_granularity =
          RuntimeEnabledFeatures::PercentBasedScrollingEnabled()
              ? ui::ScrollGranularity::kScrollByPercentage
              : ui::ScrollGranularity::kScrollByLine;
      *scroll_use_uma = WebFeature::kScrollByKeyboardArrowKeys;
      break;
    case VKEY_HOME:
      *scroll_direction =
          mojom::blink::ScrollDirection::kScrollUpIgnoringWritingMode;
      *scroll_granularity = ui::ScrollGranularity::kScrollByDocument;
      *scroll_use_uma = WebFeature::kScrollByKeyboardHomeEndKeys;
      break;
    case VKEY_END:
      *scroll_direction =
          mojom::blink::ScrollDirection::kScrollDownIgnoringWritingMode;
      *scroll_granularity = ui::ScrollGranularity::kScrollByDocument;
      *scroll_use_uma = WebFeature::kScrollByKeyboardHomeEndKeys;
      break;
    case VKEY_PRIOR:  // page up
      *scroll_direction =
          mojom::blink::ScrollDirection::kScrollUpIgnoringWritingMode;
      *scroll_granularity = ui::ScrollGranularity::kScrollByPage;
      *scroll_use_uma = WebFeature::kScrollByKeyboardPageUpDownKeys;
      break;
    case VKEY_NEXT:  // page down
      *scroll_direction =
          mojom::blink::ScrollDirection::kScrollDownIgnoringWritingMode;
      *scroll_granularity = ui::ScrollGranularity::kScrollByPage;
      *scroll_use_uma = WebFeature::kScrollByKeyboardPageUpDownKeys;
      break;
    default:
      return false;
  }

  return true;
}

}  // namespace

KeyboardEventManager::KeyboardEventManager(LocalFrame& frame,
                                           ScrollManager& scroll_manager)
    : frame_(frame), scroll_manager_(scroll_manager) {}

void KeyboardEventManager::Trace(Visitor* visitor) const {
  visitor->Trace(frame_);
  visitor->Trace(scroll_manager_);
  visitor->Trace(scrollend_event_target_);
}

bool KeyboardEventManager::HandleAccessKey(const WebKeyboardEvent& evt) {
  base::AutoReset<bool> is_handling_key_event(&is_handling_key_event_, true);
  // TODO: Ignoring the state of Shift key is what neither IE nor Firefox do.
  // IE matches lower and upper case access keys regardless of Shift key state -
  // but if both upper and lower case variants are present in a document, the
  // correct element is matched based on Shift key state.  Firefox only matches
  // an access key if Shift is not pressed, and does that case-insensitively.
  DCHECK(!(kAccessKeyModifiers & WebInputEvent::kShiftKey));
  if ((evt.GetModifiers() & (WebKeyboardEvent::kKeyModifiers &
                             ~WebInputEvent::kShiftKey)) != kAccessKeyModifiers)
    return false;
  String key = String(evt.unmodified_text.data());
  Element* elem =
      frame_->GetDocument()->GetElementByAccessKey(key.DeprecatedLower());
  if (!elem)
    return false;
  elem->Focus(FocusParams(SelectionBehaviorOnFocus::kReset,
                          mojom::blink::FocusType::kAccessKey, nullptr,
                          FocusOptions::Create(), FocusTrigger::kUserGesture));
  elem->AccessKeyAction(SimulatedClickCreationScope::kFromUserAgent);
  return true;
}

WebInputEventResult KeyboardEventManager::KeyEvent(
    const WebKeyboardEvent& initial_key_event) {
  base::AutoReset<bool> is_handling_key_event(&is_handling_key_event_, true);
  if (initial_key_event.windows_key_code == VK_CAPITAL)
    CapsLockStateMayHaveChanged();

  KeyEventModifierMayHaveChanged(initial_key_event.GetModifiers());

  if (scroll_manager_->MiddleClickAutoscrollInProgress()) {
    DCHECK(RuntimeEnabledFeatures::MiddleClickAutoscrollEnabled());
    // If a key is pressed while the middleClickAutoscroll is in progress then
    // we want to stop.
    if (initial_key_event.GetType() == WebInputEvent::Type::kKeyDown ||
        initial_key_event.GetType() == WebInputEvent::Type::kRawKeyDown)
      scroll_manager_->StopMiddleClickAutoscroll();

    // If we were in panscroll mode, we swallow the key event
    return WebInputEventResult::kHandledSuppressed;
  }

  // Check for cases where we are too early for events -- possible unmatched key
  // up from pressing return in the location bar.
  Node* node = EventTargetNodeForDocument(frame_->GetDocument());
  if (!node)
    return WebInputEventResult::kNotHandled;

  // To be meaningful enough to indicate user intention, a keyboard event needs
  // - not to be a modifier event
  // https://crbug.com/709765
  bool is_modifier = ui::KeycodeConverter::IsDomKeyForModifier(
      static_cast<ui::DomKey>(initial_key_event.dom_key));

  if (!is_modifier && initial_key_event.dom_key != ui::DomKey::ESCAPE &&
      (initial_key_event.GetType() == WebInputEvent::Type::kKeyDown ||
       initial_key_event.GetType() == WebInputEvent::Type::kRawKeyDown)) {
    LocalFrame::NotifyUserActivation(
        frame_, mojom::blink::UserActivationNotificationType::kInteraction,
        RuntimeEnabledFeatures::BrowserVerifiedUserActivationKeyboardEnabled());
  }

  // Don't expose key events to pages while browsing on the drive-by web. This
  // is to prevent pages from accidentally interfering with the built-in
  // behavior eg. spatial-navigation. Installed PWAs are a signal from the user
  // that they trust the app more than a random page on the drive-by web so we
  // allow PWAs to receive and override key events. The only exception is the
  // browser display mode since it must always behave like the the drive-by web.
  bool should_send_key_events_to_js =
      !frame_->GetSettings()->GetDontSendKeyEventsToJavascript();

  if (!should_send_key_events_to_js &&
      frame_->GetDocument()->IsInWebAppScope()) {
    mojom::blink::DisplayMode display_mode =
        frame_->GetWidgetForLocalRoot()->DisplayMode();
    should_send_key_events_to_js =
        display_mode == blink::mojom::DisplayMode::kMinimalUi ||
        display_mode == blink::mojom::DisplayMode::kStandalone ||
        display_mode == blink::mojom::DisplayMode::kFullscreen ||
        display_mode == blink::mojom::DisplayMode::kBorderless ||
        display_mode == blink::mojom::DisplayMode::kWindowControlsOverlay;
  }

  // We have 2 level of not exposing key event to js, not send and send but not
  // cancellable.
  bool send_key_event = true;
  bool event_cancellable = true;

  if (!should_send_key_events_to_js) {
    // TODO(bokan) Should cleanup these magic number. https://crbug.com/949766.
    const int kDomKeysDontSend[] = {0x00200309, 0x00200310};
    const int kDomKeysNotCancellabelUnlessInEditor[] = {0x00400031, 0x00400032,
                                                        0x00400033};
    for (uint32_t dom_key : kDomKeysDontSend) {
      if (initial_key_event.dom_key == dom_key)
        send_key_event = false;
    }

    for (uint32_t dom_key : kDomKeysNotCancellabelUnlessInEditor) {
      auto* text_control = ToTextControlOrNull(node);
      auto* element = DynamicTo<Element>(node);
      bool is_editable =
          IsEditable(*node) ||
          (text_control && !text_control->IsDisabledOrReadOnly()) ||
          (element &&
           EqualIgnoringASCIICase(
               element->FastGetAttribute(html_names::kRoleAttr), "textbox"));
      if (initial_key_event.dom_key == dom_key && !is_editable)
        event_cancellable = false;
    }
  } else {
    // TODO(bokan) Should cleanup these magic numbers. https://crbug.com/949766.
    const int kDomKeyNeverSend = 0x00200309;
    send_key_event = initial_key_event.dom_key != kDomKeyNeverSend;
  }

  DispatchEventResult dispatch_result = DispatchEventResult::kNotCanceled;
  switch (initial_key_event.GetType()) {
    // TODO: it would be fair to let an input method handle KeyUp events
    // before DOM dispatch.
    case WebInputEvent::Type::kKeyUp: {
      KeyboardEvent* event = KeyboardEvent::Create(
          initial_key_event, frame_->GetDocument()->domWindow(),
          event_cancellable);
      event->SetTarget(node);
      event->SetStopPropagation(!send_key_event);

      dispatch_result = node->DispatchEvent(*event);
      break;
    }
    case WebInputEvent::Type::kRawKeyDown:
    case WebInputEvent::Type::kKeyDown: {
      WebKeyboardEvent web_event = initial_key_event;
      web_event.SetType(WebInputEvent::Type::kRawKeyDown);

      KeyboardEvent* event = KeyboardEvent::Create(
          web_event, frame_->GetDocument()->domWindow(), event_cancellable);
      event->SetTarget(node);
      event->SetStopPropagation(!send_key_event);

      // In IE, access keys are special, they are handled after default keydown
      // processing, but cannot be canceled - this is hard to match.  On Mac OS
      // X, we process them before dispatching keydown, as the default keydown
      // handler implements Emacs key bindings, which may conflict with access
      // keys. Then we dispatch keydown, but suppress its default handling. On
      // Windows, WebKit explicitly calls handleAccessKey() instead of
      // dispatching a keypress event for WM_SYSCHAR messages.  Other platforms
      // currently match either Mac or Windows behavior, depending on whether
      // they send combined KeyDown events.
      if (initial_key_event.GetType() == WebInputEvent::Type::kKeyDown &&
          HandleAccessKey(initial_key_event)) {
        event->preventDefault();
      }

      // If this keydown did not involve a meta-key press, update the keyboard
      // event state and trigger :focus-visible matching if necessary.
      if (!event->ctrlKey() && !event->altKey() && !event->metaKey()) {
        node->UpdateHadKeyboardEvent(*event);
      }

      if (dispatch_result = node->DispatchEvent(*event);
          dispatch_result != DispatchEventResult::kNotCanceled) {
        break;
      }

      // If frame changed as a result of keydown dispatch, then return early to
      // avoid sending a subsequent keypress message to the new frame.
      if (frame_->GetPage() &&
          frame_ !=
              frame_->GetPage()->GetFocusController().FocusedOrMainFrame()) {
        return WebInputEventResult::kHandledSystem;
      }

      // kRawKeyDown doesn't trigger `keypress`es, so we end the logic here.
      if (initial_key_event.GetType() != WebInputEvent::Type::kKeyDown) {
        return WebInputEventResult::kNotHandled;
      }

      // Focus may have changed during keydown handling, so refetch node.
      // But if we are dispatching a fake backward compatibility keypress, then
      // we pretend that the keypress happened on the original node.
      node = EventTargetNodeForDocument(frame_->GetDocument());
      if (!node) {
        return WebInputEventResult::kNotHandled;
      }

#if BUILDFLAG(IS_MAC)
      // According to NSEvents.h, OpenStep reserves the range 0xF700-0xF8FF for
      // function keys. However, some actual private use characters happen to be
      // in this range, e.g. the Apple logo (Option+Shift+K). 0xF7FF is an
      // arbitrary cut-off.
      if (initial_key_event.text[0U] >= 0xF700 &&
          initial_key_event.text[0U] <= 0xF7FF) {
        return WebInputEventResult::kNotHandled;
      }
#endif
      if (initial_key_event.text[0] == 0) {
        return WebInputEventResult::kNotHandled;
      }
      [[fallthrough]];
    }
    case WebInputEvent::Type::kChar: {
      WebKeyboardEvent char_event = initial_key_event;
      char_event.SetType(WebInputEvent::Type::kChar);

      KeyboardEvent* event = KeyboardEvent::Create(
          char_event, frame_->GetDocument()->domWindow(), event_cancellable);
      event->SetTarget(node);
      event->SetStopPropagation(!send_key_event);

      dispatch_result = node->DispatchEvent(*event);
      break;
    }
    default:
      NOTREACHED();
  }
  return event_handling_util::ToWebInputEventResult(dispatch_result);
}

void KeyboardEventManager::CapsLockStateMayHaveChanged() {
  if (Element* element = frame_->GetDocument()->FocusedElement()) {
    if (auto* text_control = DynamicTo<HTMLInputElement>(element))
      text_control->CapsLockStateMayHaveChanged();
  }
}

void KeyboardEventManager::KeyEventModifierMayHaveChanged(int modifiers) {
  WebLinkPreviewTriggerer* triggerer =
      frame_->GetOrCreateLinkPreviewTriggerer();
  if (!triggerer) {
    return;
  }

  triggerer->MaybeChangedKeyEventModifier(modifiers);
}

void KeyboardEventManager::DefaultKeyboardEventHandler(
    KeyboardEvent* event,
    Node* possible_focused_node) {
  if (event->type() == event_type_names::kKeydown) {
    frame_->GetEditor().HandleKeyboardEvent(event);
    if (event->DefaultHandled())
      return;

    // Do not perform the default action when inside a IME composition context.
    // TODO(dtapuska): Replace this with isComposing support. crbug.com/625686
    if (event->keyCode() == kVKeyProcessKey)
      return;

    const AtomicString key(event->key());
    if (key == keywords::kTab) {
      DefaultTabEventHandler(event);
    } else if (key == keywords::kEscape) {
      DefaultEscapeEventHandler(event);
    } else if (key == keywords::kCapitalEnter) {
      DefaultEnterEventHandler(event);
    } else if (event->KeyEvent() &&
               static_cast<int>(event->KeyEvent()->dom_key) == 0x00200310) {
      // TODO(bokan): Cleanup magic numbers once https://crbug.com/949766 lands.
      DefaultImeSubmitHandler(event);
    } else {
      // TODO(bokan): Seems odd to call the default _arrow_ event handler on
      // events that aren't necessarily arrow keys.
      DefaultArrowEventHandler(event, possible_focused_node);
    }
  } else if (event->type() == event_type_names::kKeypress) {
    frame_->GetEditor().HandleKeyboardEvent(event);
    if (event->DefaultHandled())
      return;
    if (event->key() == keywords::kCapitalEnter) {
      DefaultEnterEventHandler(event);
    } else if (event->charCode() == ' ') {
      DefaultSpaceEventHandler(event, possible_focused_node);
    }
  } else if (event->type() == event_type_names::kKeyup) {
    if (event->DefaultHandled())
      return;
    if (event->key() == keywords::kCapitalEnter) {
      DefaultEnterEventHandler(event);
    }
    if (event->keyCode() == last_scrolling_keycode_) {
      if (scrollend_event_target_ && has_pending_scrollend_on_key_up_) {
        scrollend_event_target_->OnScrollFinished(true);
      }
      scrollend_event_target_.Clear();
      last_scrolling_keycode_ = VKEY_UNKNOWN;
      has_pending_scrollend_on_key_up_ = false;
    }
  }
}

void KeyboardEventManager::DefaultSpaceEventHandler(
    KeyboardEvent* event,
    Node* possible_focused_node) {
  DCHECK_EQ(event->type(), event_type_names::kKeypress);

  if (event->ctrlKey() || event->metaKey() || event->altKey())
    return;

  mojom::blink::ScrollDirection direction =
      event->shiftKey()
          ? mojom::blink::ScrollDirection::kScrollBlockDirectionBackward
          : mojom::blink::ScrollDirection::kScrollBlockDirectionForward;

  // We must clear |scrollend_event_target_| at the beginning of each scroll
  // so that we don't fire scrollend based on a prior scroll if a newer scroll
  // begins before the keyup event associated with the prior scroll/keydown.
  // If a newer scroll begins before the keyup event and ends after it,
  // we should fire scrollend at the end of that newer scroll rather than at
  // the keyup event.
  scrollend_event_target_.Clear();
  // TODO(bokan): enable scroll customization in this case. See
  // crbug.com/410974.
  if (scroll_manager_->LogicalScroll(direction,
                                     ui::ScrollGranularity::kScrollByPage,
                                     nullptr, possible_focused_node, true)) {
    UseCounter::Count(frame_->GetDocument(),
                      WebFeature::kScrollByKeyboardSpacebarKey);
    last_scrolling_keycode_ = event->keyCode();
    has_pending_scrollend_on_key_up_ = true;
    event->SetDefaultHandled();
    return;
  }
}

void KeyboardEventManager::DefaultArrowEventHandler(
    KeyboardEvent* event,
    Node* possible_focused_node) {
  DCHECK_EQ(event->type(), event_type_names::kKeydown);

  Page* page = frame_->GetPage();
  if (!page)
    return;

  ExecutionContext* context = frame_->GetDocument()->GetExecutionContext();
  if (RuntimeEnabledFeatures::FocusgroupEnabled(context) &&
      FocusgroupController::HandleArrowKeyboardEvent(event, frame_)) {
    event->SetDefaultHandled();
    return;
  }

  if (IsSpatialNavigationEnabled(frame_) &&
      !frame_->GetDocument()->InDesignMode() &&
      !IsPageUpOrDownKeyEvent(event->keyCode(), event->GetModifiers())) {
    if (page->GetSpatialNavigationController().HandleArrowKeyboardEvent(
            event)) {
      event->SetDefaultHandled();
      return;
    }
  }

  if (event->KeyEvent() && event->KeyEvent()->is_system_key)
    return;

  mojom::blink::ScrollDirection scroll_direction;
  ui::ScrollGranularity scroll_granularity;
  WebFeature scroll_use_uma;
  if (!MapKeyCodeForScroll(event->keyCode(), event->GetModifiers(),
                           &scroll_direction, &scroll_granularity,
                           &scroll_use_uma))
    return;

  // See KeyboardEventManager::DefaultSpaceEventHandler for the reason for
  // this Clear.
  scrollend_event_target_.Clear();
  if (scroll_manager_->BubblingScroll(scroll_direction, scroll_granularity,
                                      nullptr, possible_focused_node, true)) {
    UseCounter::Count(frame_->GetDocument(), scroll_use_uma);
    last_scrolling_keycode_ = event->keyCode();
    has_pending_scrollend_on_key_up_ = true;
    event->SetDefaultHandled();
    return;
  }
}

void KeyboardEventManager::DefaultTabEventHandler(KeyboardEvent* event) {
  // TODO (liviutinta) remove TRACE after fixing crbug.com/1063548
  TRACE_EVENT0("input", "KeyboardEventManager::DefaultTabEventHandler");
  DCHECK_EQ(event->type(), event_type_names::kKeydown);
  // We should only advance focus on tabs if no special modifier keys are held
  // down.
  if (event->ctrlKey() || event->metaKey()) {
    // TODO (liviutinta) remove TRACE after fixing crbug.com/1063548
    TRACE_EVENT_INSTANT1(
        "input", "KeyboardEventManager::DefaultTabEventHandler",
        TRACE_EVENT_SCOPE_THREAD, "reason_tab_does_not_advance_focus",
        (event->ctrlKey() ? (event->metaKey() ? "Ctrl+MetaKey+Tab" : "Ctrl+Tab")
                          : "MetaKey+Tab"));
    return;
  }

#if !BUILDFLAG(IS_MAC)
  // Option-Tab is a shortcut based on a system-wide preference on Mac but
  // should be ignored on all other platforms.
  if (event->altKey()) {
    // TODO (liviutinta) remove TRACE after fixing crbug.com/1063548
    TRACE_EVENT_INSTANT1("input",
                         "KeyboardEventManager::DefaultTabEventHandler",
                         TRACE_EVENT_SCOPE_THREAD,
                         "reason_tab_does_not_advance_focus", "Alt+Tab");
    return;
  }
#endif

  Page* page = frame_->GetPage();
  if (!page) {
    // TODO (liviutinta) remove TRACE after fixing crbug.com/1063548
    TRACE_EVENT_INSTANT1("input",
                         "KeyboardEventManager::DefaultTabEventHandler",
                         TRACE_EVENT_SCOPE_THREAD,
                         "reason_tab_does_not_advance_focus", "Page is null");
    return;
  }
  if (!page->TabKeyCyclesThroughElements()) {
    // TODO (liviutinta) remove TRACE after fixing crbug.com/1063548
    TRACE_EVENT_INSTANT1(
        "input", "KeyboardEventManager::DefaultTabEventHandler",
        TRACE_EVENT_SCOPE_THREAD, "reason_tab_does_not_advance_focus",
        "TabKeyCyclesThroughElements is false");
    return;
  }

  mojom::blink::FocusType focus_type = event->shiftKey()
                                           ? mojom::blink::FocusType::kBackward
                                           : mojom::blink::FocusType::kForward;

  // Tabs can be used in design mode editing.
  if (frame_->GetDocument()->InDesignMode()) {
    // TODO (liviutinta) remove TRACE after fixing crbug.com/1063548
    TRACE_EVENT_INSTANT1(
        "input", "KeyboardEventManager::DefaultTabEventHandler",
        TRACE_EVENT_SCOPE_THREAD, "reason_tab_does_not_advance_focus",
        "DesignMode is true");
    return;
  }

  if (page->GetFocusController().AdvanceFocus(focus_type,
                                              frame_->GetDocument()
                                                  ->domWindow()
                                                  ->GetInputDeviceCapabilities()
                                                  ->FiresTouchEvents(false))) {
    event->SetDefaultHandled();
  } else {
    // TODO (liviutinta) remove TRACE after fixing crbug.com/1063548
    TRACE_EVENT_INSTANT1(
        "input", "KeyboardEventManager::DefaultTabEventHandler",
        TRACE_EVENT_SCOPE_THREAD, "reason_tab_does_not_advance_focus",
        "AdvanceFocus returned false");
    return;
  }
}

void KeyboardEventManager::DefaultEscapeEventHandler(KeyboardEvent* event) {
  Page* page = frame_->GetPage();
  if (!page)
    return;

  if (IsSpatialNavigationEnabled(frame_) &&
      !frame_->GetDocument()->InDesignMode()) {
    page->GetSpatialNavigationController().HandleEscapeKeyboardEvent(event);
  }

  frame_->DomWindow()->closewatcher_stack()->EscapeKeyHandler(event);
}

void KeyboardEventManager::DefaultEnterEventHandler(KeyboardEvent* event) {
  Page* page = frame_->GetPage();
  if (!page)
    return;

  if (IsSpatialNavigationEnabled(frame_) &&
      !frame_->GetDocument()->InDesignMode()) {
    page->GetSpatialNavigationController().HandleEnterKeyboardEvent(event);
  }
}

void KeyboardEventManager::DefaultImeSubmitHandler(KeyboardEvent* event) {
  Page* page = frame_->GetPage();
  if (!page)
    return;

  if (IsSpatialNavigationEnabled(frame_) &&
      !frame_->GetDocument()->InDesignMode()) {
    page->GetSpatialNavigationController().HandleImeSubmitKeyboardEvent(event);
  }
}

static OverrideCapsLockState g_override_caps_lock_state;

void KeyboardEventManager::SetCurrentCapsLockState(
    OverrideCapsLockState state) {
  g_override_caps_lock_state = state;
}

bool KeyboardEventManager::CurrentCapsLockState() {
  switch (g_override_caps_lock_state) {
    case OverrideCapsLockState::kDefault:
#if BUILDFLAG(IS_MAC)
      return GetCurrentKeyModifiers() & alphaLock;
#else
      // Caps lock state use is limited to Mac password input
      // fields, so just return false. See http://crbug.com/618739.
      return false;
#endif
    case OverrideCapsLockState::kOn:
      return true;
    case OverrideCapsLockState::kOff:
    default:
      return false;
  }
}

WebInputEvent::Modifiers KeyboardEventManager::GetCurrentModifierState() {
#if BUILDFLAG(IS_MAC)
  unsigned modifiers = 0;
  UInt32 current_modifiers = GetCurrentKeyModifiers();
  if (current_modifiers & ::shiftKey)
    modifiers |= WebInputEvent::kShiftKey;
  if (current_modifiers & ::controlKey)
    modifiers |= WebInputEvent::kControlKey;
  if (current_modifiers & ::optionKey)
    modifiers |= WebInputEvent::kAltKey;
  if (current_modifiers & ::cmdKey)
    modifiers |= WebInputEvent::kMetaKey;
  return static_cast<WebInputEvent::Modifiers>(modifiers);
#else
  // TODO(crbug.com/538289): Implement on other platforms.
  return static_cast<WebInputEvent::Modifiers>(0);
#endif
}

}  // namespace blink
