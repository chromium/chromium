// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/navigation_api/navigate_event.h"

#include "third_party/blink/public/mojom/devtools/console_message.mojom-shared.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_navigate_event_init.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_navigation_transition_while_options.h"
#include "third_party/blink/renderer/core/dom/abort_signal.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/event_interface_names.h"
#include "third_party/blink/renderer/core/event_type_names.h"
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
      can_transition_(init->canTransition()),
      user_initiated_(init->userInitiated()),
      hash_change_(init->hashChange()),
      signal_(init->signal()),
      form_data_(init->formData()),
      info_(init->hasInfo()
                ? init->info()
                : ScriptValue(context->GetIsolate(),
                              v8::Undefined(context->GetIsolate()))) {
  DCHECK(IsA<LocalDOMWindow>(context));
}

void NavigateEvent::transitionWhile(ScriptState* script_state,
                                    ScriptPromise newNavigationAction,
                                    NavigationTransitionWhileOptions* options,
                                    ExceptionState& exception_state) {
  if (!DomWindow()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "transitionWhile() may not be called in a "
        "detached window.");
    return;
  }

  if (!isTrusted()) {
    exception_state.ThrowSecurityError(
        "transitionWhile() may only be called on a "
        "trusted event.");
    return;
  }

  if (!can_transition_) {
    exception_state.ThrowSecurityError(
        "A navigation with URL '" + url_.ElidedString() +
        "' cannot be intercepted by transitionWhile() in a window with origin "
        "'" +
        DomWindow()->GetSecurityOrigin()->ToString() + "' and URL '" +
        DomWindow()->Url().ElidedString() + "'.");
    return;
  }

  if (!IsBeingDispatched()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "transitionWhile() may only be called while the navigate event is "
        "being dispatched.");
  }

  if (defaultPrevented()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "transitionWhile() may not be called if the event has been canceled.");
    return;
  }

  navigation_action_promises_list_.push_back(newNavigationAction);

  if (options->hasFocusReset()) {
    if (focus_reset_behavior_ &&
        focus_reset_behavior_->AsEnum() != options->focusReset().AsEnum()) {
      GetExecutionContext()->AddConsoleMessage(
          MakeGarbageCollected<ConsoleMessage>(
              mojom::ConsoleMessageSource::kJavaScript,
              mojom::blink::ConsoleMessageLevel::kWarning,
              "The \"" + options->focusReset().AsString() +
                  "\" value for transitionWhile()'s focusReset option will "
                  "override the previously-passed value of \"" +
                  focus_reset_behavior_->AsString() + "\"."));
    }
    focus_reset_behavior_ = options->focusReset();
  }
}

bool NavigateEvent::ShouldResetFocus() const {
  // We only do focus reset if transitionWhile() was called, opting us into the
  // new default behavior which the navigation API provides.
  if (navigation_action_promises_list_.IsEmpty())
    return false;

  // If we're in "navigation API mode" per the above, then either leaving focus
  // reset behavior as the default, or setting it to "after-transition"
  // explicitly, should reset the focus.
  return !focus_reset_behavior_ ||
         focus_reset_behavior_->AsEnum() ==
             V8NavigationFocusReset::Enum::kAfterTransition;
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
}

}  // namespace blink
