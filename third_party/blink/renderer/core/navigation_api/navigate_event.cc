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
#include "third_party/blink/renderer/core/loader/document_loader.h"
#include "third_party/blink/renderer/core/navigation_api/navigation_destination.h"
#include "third_party/blink/renderer/core/timing/soft_navigation_heuristics.h"
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
  CHECK(IsA<LocalDOMWindow>(context));
}

bool NavigateEvent::PerformSharedChecks(const String& function_name,
                                        ExceptionState& exception_state) {
  if (!DomWindow()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        function_name + "() may not be called in a detached window.");
    return false;
  }
  if (!isTrusted()) {
    exception_state.ThrowSecurityError(
        function_name + "() may only be called on a trusted event.");
    return false;
  }
  if (defaultPrevented()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        function_name + "() may not be called if the event has been canceled.");
    return false;
  }
  return true;
}

void NavigateEvent::intercept(NavigationInterceptOptions* options,
                              ExceptionState& exception_state) {
  if (!PerformSharedChecks("intercept", exception_state)) {
    return;
  }

  if (!can_intercept_) {
    exception_state.ThrowSecurityError(
        "A navigation with URL '" + dispatch_params_->url.ElidedString() +
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

  CHECK(intercept_state_ == InterceptState::kNone ||
        intercept_state_ == InterceptState::kIntercepted);
  intercept_state_ = InterceptState::kIntercepted;
  if (options->hasHandler())
    navigation_action_handlers_list_.push_back(options->handler());
}

void NavigateEvent::DoCommit() {
  CHECK_EQ(intercept_state_, InterceptState::kIntercepted);
  CHECK(!dispatch_params_->destination_item || !dispatch_params_->state_object);

  intercept_state_ = InterceptState::kCommitted;

  auto* state_object = dispatch_params_->destination_item
                           ? dispatch_params_->destination_item->StateObject()
                           : dispatch_params_->state_object.get();

  // In the spec, the URL and history update steps are not called for reloads.
  // In our implementation, we call the corresponding function anyway, but
  // |type| being a reload type makes it do none of the spec-relevant
  // steps. Instead it does stuff like the loading spinner and use counters.
  DomWindow()->document()->Loader()->RunURLAndHistoryUpdateSteps(
      dispatch_params_->url, dispatch_params_->destination_item,
      mojom::blink::SameDocumentNavigationType::kNavigationApiIntercept,
      state_object, dispatch_params_->frame_load_type,
      dispatch_params_->is_browser_initiated,
      dispatch_params_->is_synchronously_committed_same_document);

  // This is considered a soft navigation URL change at this point, when the
  // user visible URL change happens. Skip the descendant check because the URL
  // change doesn't happen in a JS task.
  auto* soft_navigation_heuristics =
      DomWindow() ? SoftNavigationHeuristics::From(*DomWindow()) : nullptr;
  if (soft_navigation_heuristics && user_initiated_ && !download_request_) {
    auto* script_state = ToScriptStateForMainWorld(DomWindow()->GetFrame());
    ScriptState::Scope scope(script_state);
    soft_navigation_heuristics->SawURLChange(script_state,
                                             dispatch_params_->url,
                                             /*skip_descendant_check=*/true);
  }
}

ScriptPromise NavigateEvent::GetReactionPromiseAll(ScriptState* script_state) {
  CHECK(navigation_action_handlers_list_.empty());
  if (!navigation_action_promises_list_.empty()) {
    return ScriptPromise::All(script_state, navigation_action_promises_list_);
  }
  // There is a subtle timing difference between the fast-path for zero
  // promises and the path for 1+ promises, in both spec and implementation.
  // In most uses of ScriptPromise::All / the Web IDL spec's "wait for all",
  // this does not matter. However for us there are so many events and promise
  // handlers firing around the same time (navigatesuccess, committed promise,
  // finished promise, ...) that the difference is pretty easily observable by
  // web developers and web platform tests. So, let's make sure we always go
  // down the 1+ promises path.
  return ScriptPromise::All(
      script_state,
      HeapVector<ScriptPromise>({ScriptPromise::CastUndefined(script_state)}));
}

void NavigateEvent::FinalizeNavigationActionPromisesList() {
  for (auto& function : navigation_action_handlers_list_) {
    ScriptPromise result;
    if (function->Invoke(this).To(&result))
      navigation_action_promises_list_.push_back(result);
  }
  navigation_action_handlers_list_.clear();
}

void NavigateEvent::PotentiallyResetTheFocus() {
  CHECK(intercept_state_ == InterceptState::kCommitted ||
        intercept_state_ == InterceptState::kScrolled);
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
  CHECK(HasNavigationActions());
  did_change_focus_during_intercept_ = true;
}

void NavigateEvent::scroll(ExceptionState& exception_state) {
  if (!PerformSharedChecks("scroll", exception_state)) {
    return;
  }

  if (intercept_state_ == InterceptState::kFinished) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "scroll() may not be called after transition completes");
    return;
  }
  if (intercept_state_ == InterceptState::kScrolled) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "scroll() already called");
    return;
  }
  if (intercept_state_ == InterceptState::kNone) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "intercept() must be called before scroll()");
    return;
  }
  if (intercept_state_ == InterceptState::kIntercepted) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "scroll() may not be called before commit.");
    return;
  }

  ProcessScrollBehavior();
}

void NavigateEvent::Finish(bool did_fulfill) {
  CHECK_NE(intercept_state_, InterceptState::kIntercepted);
  CHECK_NE(intercept_state_, InterceptState::kFinished);
  if (intercept_state_ == InterceptState::kNone) {
    return;
  }
  PotentiallyResetTheFocus();
  if (did_fulfill) {
    PotentiallyProcessScrollBehavior();
  }
  intercept_state_ = InterceptState::kFinished;
}

void NavigateEvent::PotentiallyProcessScrollBehavior() {
  CHECK(intercept_state_ == InterceptState::kCommitted ||
        intercept_state_ == InterceptState::kScrolled);
  if (intercept_state_ == InterceptState::kScrolled) {
    return;
  }
  if (scroll_behavior_ &&
      scroll_behavior_->AsEnum() == V8NavigationScrollBehavior::Enum::kManual) {
    return;
  }
  ProcessScrollBehavior();
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
  NOTREACHED_NORETURN();
}

void NavigateEvent::ProcessScrollBehavior() {
  CHECK_EQ(intercept_state_, InterceptState::kCommitted);
  intercept_state_ = InterceptState::kScrolled;

  absl::optional<HistoryItem::ViewState> view_state =
      dispatch_params_->destination_item
          ? dispatch_params_->destination_item->GetViewState()
          : absl::nullopt;
  // Use mojom::blink::ScrollRestorationType::kAuto unconditionally here
  // because we are certain that we want to actually scroll if we reach this
  // point. Using mojom::blink::ScrollRestorationType::kManual would block the
  // scroll.
  DomWindow()->GetFrame()->Loader().ProcessScrollForSameDocumentNavigation(
      dispatch_params_->url, LoadTypeFromNavigation(navigation_type_),
      view_state, mojom::blink::ScrollRestorationType::kAuto);
}

const AtomicString& NavigateEvent::InterfaceName() const {
  return event_interface_names::kNavigateEvent;
}

void NavigateEvent::Trace(Visitor* visitor) const {
  Event::Trace(visitor);
  ExecutionContextClient::Trace(visitor);
  visitor->Trace(dispatch_params_);
  visitor->Trace(destination_);
  visitor->Trace(signal_);
  visitor->Trace(form_data_);
  visitor->Trace(info_);
  visitor->Trace(navigation_action_promises_list_);
  visitor->Trace(navigation_action_handlers_list_);
}

}  // namespace blink
