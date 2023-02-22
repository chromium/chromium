// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/navigation_api/navigate_event.h"

#include "third_party/blink/public/mojom/devtools/console_message.mojom-shared.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_navigate_event_init.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_navigation_intercept_handler.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_navigation_intercept_options.h"
#include "third_party/blink/renderer/core/dom/abort_signal.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/event_interface_names.h"
#include "third_party/blink/renderer/core/event_type_names.h"
#include "third_party/blink/renderer/core/frame/deprecation/deprecation.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/html/forms/form_data.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/navigation_api/navigation_destination.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

NavigateEvent::NavigateEvent(ExecutionContext* context,
                             const AtomicString& type,
                             NavigateEventInit* init)
    : Event(type, init),
      ExecutionContextClient(context),
      navigation_type_(init->navigationType()),
      destination_(init->destination()),
      can_intercept_(init->canIntercept()),
      user_initiated_(init->userInitiated()),
      hash_change_(init->hashChange()),
      signal_(init->signal()),
      form_data_(init->formData()),
      download_request_(init->downloadRequest()),
      info_(init->hasInfo()
                ? init->info()
                : ScriptValue(context->GetIsolate(),
                              v8::Undefined(context->GetIsolate()))) {
  DCHECK(IsA<LocalDOMWindow>(context));
}

void NavigateEvent::intercept(NavigationInterceptOptions* options,
                              ExceptionState& exception_state) {
  if (!DomWindow()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "intercept() may not be called in a detached window.");
    return;
  }

  if (!isTrusted()) {
    exception_state.ThrowSecurityError(
        "intercept() may only be called on a trusted event.");
    return;
  }

  if (!can_intercept_) {
    exception_state.ThrowSecurityError(
        "A navigation with URL '" + url_.ElidedString() +
        "' cannot be intercepted by in a window with origin '" +
        DomWindow()->GetSecurityOrigin()->ToString() + "' and URL '" +
        DomWindow()->Url().ElidedString() + "'.");
    return;
  }

  if (!IsBeingDispatched()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "intercept() may only be called while the navigate event is being "
        "dispatched.");
    return;
  }

  if (defaultPrevented()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "intercept() may not be called if the event has been canceled.");
    return;
  }

  if (!HasNavigationActions()) {
    DomWindow()->document()->AddFocusedElementChangeObserver(this);
  }

  if (options->hasFocusReset()) {
    if (focus_reset_behavior_ &&
        focus_reset_behavior_->AsEnum() != options->focusReset().AsEnum()) {
      GetExecutionContext()->AddConsoleMessage(
          MakeGarbageCollected<ConsoleMessage>(
              mojom::blink::ConsoleMessageSource::kJavaScript,
              mojom::blink::ConsoleMessageLevel::kWarning,
              "The \"" + options->focusReset().AsString() + "\" value for " +
                  "intercept()'s focusReset option "
                  "will override the previously-passed value of \"" +
                  focus_reset_behavior_->AsString() + "\"."));
    }
    focus_reset_behavior_ = options->focusReset();
  }

  if (options->hasScroll()) {
    if (scroll_behavior_ &&
        scroll_behavior_->AsEnum() != options->scroll().AsEnum()) {
      GetExecutionContext()->AddConsoleMessage(
          MakeGarbageCollected<ConsoleMessage>(
              mojom::blink::ConsoleMessageSource::kJavaScript,
              mojom::blink::ConsoleMessageLevel::kWarning,
              "The \"" + options->scroll().AsString() + "\" value for " +
                  "intercept()'s scroll option "
                  "will override the previously-passed value of \"" +
                  scroll_behavior_->AsString() + "\"."));
    }
    scroll_behavior_ = options->scroll();
  }

  has_navigation_actions_ = true;
  if (options->hasHandler())
    navigation_action_handlers_list_.push_back(options->handler());
}

void NavigateEvent::FinalizeNavigationActionPromisesList() {
  for (auto& function : navigation_action_handlers_list_) {
    ScriptPromise result;
    if (function->Invoke(this).To(&result))
      navigation_action_promises_list_.push_back(result);
  }
  navigation_action_handlers_list_.clear();
}

void NavigateEvent::ResetFocusIfNeeded() {
  // We only do focus reset if intercept() was called, opting us into the
  // new default behavior which the navigation API provides.
  if (!HasNavigationActions())
    return;
  auto* document = DomWindow()->document();
  document->RemoveFocusedElementChangeObserver(this);

  // If focus has changed since intercept() was invoked, don't reset
  // focus.
  if (did_change_focus_during_intercept_)
    return;

  // If we're in "navigation API mode" per the above, then either leaving focus
  // reset behavior as the default, or setting it to "after-transition"
  // explicitly, should reset the focus.
  if (focus_reset_behavior_ &&
      focus_reset_behavior_->AsEnum() !=
          V8NavigationFocusReset::Enum::kAfterTransition) {
    return;
  }

  if (Element* focus_delegate = document->GetAutofocusDelegate()) {
    focus_delegate->Focus();
  } else {
    document->ClearFocusedElement();
    document->SetSequentialFocusNavigationStartingPoint(nullptr);
  }
}

void NavigateEvent::DidChangeFocus() {
  DCHECK(HasNavigationActions());
  did_change_focus_during_intercept_ = true;
}

bool NavigateEvent::ShouldSendAxEvents() const {
  return HasNavigationActions();
}

void NavigateEvent::scroll(ExceptionState& exception_state) {
  if (did_finish_) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "scroll() may not be called after transition completes");
    return;
  }
  if (did_process_scroll_behavior_) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "scroll() already called");
    return;
  }
  if (!DomWindow()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "scroll() may not be called in a detached window.");
  }
  if (!has_navigation_actions_) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "intercept() must be called before scroll()");
  }
  DefinitelyProcessScrollBehavior();
}

void NavigateEvent::PotentiallyProcessScrollBehavior() {
  DCHECK(!did_finish_);
  did_finish_ = true;
  if (!has_navigation_actions_ || did_process_scroll_behavior_)
    return;
  if (scroll_behavior_ &&
      scroll_behavior_->AsEnum() == V8NavigationScrollBehavior::Enum::kManual) {
    return;
  }
  DefinitelyProcessScrollBehavior();
}

void NavigateEvent::SaveStateFromDestinationItem(HistoryItem* item) {
  if (item)
    history_item_view_state_ = item->GetViewState();
}

WebFrameLoadType LoadTypeFromNavigation(const String& navigation_type) {
  if (navigation_type == "push")
    return WebFrameLoadType::kStandard;
  if (navigation_type == "replace")
    return WebFrameLoadType::kReplaceCurrentItem;
  if (navigation_type == "traverse")
    return WebFrameLoadType::kBackForward;
  if (navigation_type == "reload")
    return WebFrameLoadType::kReload;
  NOTREACHED();
  return WebFrameLoadType::kStandard;
}

void NavigateEvent::DefinitelyProcessScrollBehavior() {
  DCHECK(!did_process_scroll_behavior_);
  did_process_scroll_behavior_ = true;
  // Use mojom::blink::ScrollRestorationType::kAuto unconditionally here
  // because we are certain that we want to actually scroll if we reach this
  // point. Using mojom::blink::ScrollRestorationType::kManual would block the
  // scroll.
  DomWindow()->GetFrame()->Loader().ProcessScrollForSameDocumentNavigation(
      url_, LoadTypeFromNavigation(navigation_type_), history_item_view_state_,
      mojom::blink::ScrollRestorationType::kAuto);
}

const AtomicString& NavigateEvent::InterfaceName() const {
  return event_interface_names::kNavigateEvent;
}

void NavigateEvent::Trace(Visitor* visitor) const {
  Event::Trace(visitor);
  ExecutionContextClient::Trace(visitor);
  visitor->Trace(destination_);
  visitor->Trace(signal_);
  visitor->Trace(form_data_);
  visitor->Trace(info_);
  visitor->Trace(navigation_action_promises_list_);
  visitor->Trace(navigation_action_handlers_list_);
}

}  // namespace blink
