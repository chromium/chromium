// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/navigation_api/navigate_event.h"

#include "third_party/blink/public/mojom/devtools/console_message.mojom-shared.h"
#include "third_party/blink/public/mojom/frame/frame.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_function.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_navigate_event_init.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_navigation_intercept_handler.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_navigation_intercept_options.h"
#include "third_party/blink/renderer/core/accessibility/ax_object_cache.h"
#include "third_party/blink/renderer/core/dom/abort_controller.h"
#include "third_party/blink/renderer/core/dom/abort_signal.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/focus_params.h"
#include "third_party/blink/renderer/core/event_interface_names.h"
#include "third_party/blink/renderer/core/event_type_names.h"
#include "third_party/blink/renderer/core/frame/deprecation/deprecation.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_client.h"
#include "third_party/blink/renderer/core/html/forms/form_data.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/loader/document_loader.h"
#include "third_party/blink/renderer/core/loader/progress_tracker.h"
#include "third_party/blink/renderer/core/navigation_api/navigation_destination.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cancellable_task.h"

namespace blink {

enum class ResolveType { kFulfill, kReject };
class NavigateEvent::Reaction final : public ScriptFunction::Callable {
 public:
  Reaction(NavigateEvent* navigate_event, ResolveType resolve_type)
      : navigate_event_(navigate_event), resolve_type_(resolve_type) {}
  void Trace(Visitor* visitor) const final {
    ScriptFunction::Callable::Trace(visitor);
    visitor->Trace(navigate_event_);
  }
  ScriptValue Call(ScriptState*, ScriptValue value) final {
    navigate_event_->ReactDone(value, resolve_type_ == ResolveType::kFulfill);
    return ScriptValue();
  }

 private:
  Member<NavigateEvent> navigate_event_;
  ResolveType resolve_type_;
};

NavigateEvent::NavigateEvent(ExecutionContext* context,
                             const AtomicString& type,
                             NavigateEventInit* init,
                             AbortController* controller)
    : Event(type, init),
      ExecutionContextClient(context),
      navigation_type_(init->navigationType().AsEnum()),
      destination_(init->destination()),
      can_intercept_(init->canIntercept()),
      user_initiated_(init->userInitiated()),
      hash_change_(init->hashChange()),
      controller_(controller),
      signal_(init->signal()),
      form_data_(init->formData()),
      download_request_(init->downloadRequest()),
      info_(init->hasInfo()
                ? init->info()
                : ScriptValue(context->GetIsolate(),
                              v8::Undefined(context->GetIsolate()))),
      has_ua_visual_transition_(init->hasUAVisualTransition()),
      source_element_(init->sourceElement()) {
  CHECK(IsA<LocalDOMWindow>(context));
  CHECK(!controller_ || controller_->signal() == signal_);
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

  if (RuntimeEnabledFeatures::NavigateEventCommitBehaviorEnabled() &&
      !cancelable() && options->hasCommit() &&
      options->commit().AsEnum() ==
          V8NavigationCommitBehavior::Enum::kAfterTransition) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "intercept() may only be called with a commit option of "
        "\"after-transition\" when the navigate event is cancelable.");
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

  if (RuntimeEnabledFeatures::NavigateEventCommitBehaviorEnabled()) {
    if (options->hasCommit()) {
      if (commit_behavior_ &&
          commit_behavior_->AsEnum() != options->commit().AsEnum()) {
        GetExecutionContext()->AddConsoleMessage(
            MakeGarbageCollected<ConsoleMessage>(
                mojom::blink::ConsoleMessageSource::kJavaScript,
                mojom::blink::ConsoleMessageLevel::kWarning,
                "The \"" + options->commit().AsString() + "\" value for " +
                    "intercept()'s commit option "
                    "will override the previously-passed value of \"" +
                    commit_behavior_->AsString() + "\"."));
      }
      commit_behavior_ = options->commit();
    }
  }

  CHECK(intercept_state_ == InterceptState::kNone ||
        intercept_state_ == InterceptState::kIntercepted);
  intercept_state_ = InterceptState::kIntercepted;
  if (options->hasHandler())
    navigation_action_handlers_list_.push_back(options->handler());
}

void NavigateEvent::commit(ExceptionState& exception_state) {
  if (!PerformSharedChecks("commit", exception_state)) {
    return;
  }

  if (intercept_state_ == InterceptState::kNone) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "intercept() must be called before commit().");
    return;
  }
  if (ShouldCommitImmediately()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "commit() may only be used if { commit: "
                                      "'after-transition' } was specified.");
    return;
  }
  if (IsBeingDispatched()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "commit() may not be called during event dispatch");
    return;
  }
  if (intercept_state_ == InterceptState::kFinished) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "commit() may not be called after transition completes.");
    return;
  }
  if (intercept_state_ == InterceptState::kCommitted ||
      intercept_state_ == InterceptState::kScrolled) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "commit() already called.");
    return;
  }
  CommitNow();
}

void NavigateEvent::MaybeCommitImmediately(ScriptState* script_state) {
  delayed_load_start_task_handle_ = PostDelayedCancellableTask(
      *DomWindow()->GetTaskRunner(TaskType::kInternalLoading), FROM_HERE,
      WTF::BindOnce(&NavigateEvent::DelayedLoadStartTimerFired,
                    WrapWeakPersistent(this)),
      kDelayLoadStart);

  if (ShouldCommitImmediately()) {
    CommitNow();
    return;
  }

  DomWindow()->GetFrame()->Loader().Progress().ProgressStarted();
  FinalizeNavigationActionPromisesList();
}

bool NavigateEvent::ShouldCommitImmediately() {
  return !commit_behavior_ || commit_behavior_->AsEnum() ==
                                  V8NavigationCommitBehavior::Enum::kImmediate;
}

void NavigateEvent::CommitNow() {
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
      dispatch_params_->event_type == NavigateEventType::kHistoryApi
          ? FirePopstate::kNo
          : FirePopstate::kYes,
      dispatch_params_->is_browser_initiated,
      dispatch_params_->is_synchronously_committed_same_document,
      dispatch_params_->soft_navigation_heuristics_task_id);
}

void NavigateEvent::React(ScriptState* script_state) {
  CHECK(navigation_action_handlers_list_.empty());

  ScriptPromiseUntyped promise;
  if (!navigation_action_promises_list_.empty()) {
    promise = ScriptPromiseUntyped::All(script_state,
                                        navigation_action_promises_list_);
  } else {
    // There is a subtle timing difference between the fast-path for zero
    // promises and the path for 1+ promises, in both spec and implementation.
    // In most uses of ScriptPromiseUntyped::All / the Web IDL spec's "wait for
    // all", this does not matter. However for us there are so many events and
    // promise handlers firing around the same time (navigatesuccess, committed
    // promise, finished promise, ...) that the difference is pretty easily
    // observable by web developers and web platform tests. So, let's make sure
    // we always go down the 1+ promises path.
    promise = ScriptPromiseUntyped::All(
        script_state, HeapVector<ScriptPromiseUntyped>(
                          {ToResolvedUndefinedPromise(script_state)}));
  }

  promise.Then(MakeGarbageCollected<ScriptFunction>(
                   script_state,
                   MakeGarbageCollected<Reaction>(this, ResolveType::kFulfill)),
               MakeGarbageCollected<ScriptFunction>(
                   script_state,
                   MakeGarbageCollected<Reaction>(this, ResolveType::kReject)));

  if (HasNavigationActions() && DomWindow()) {
    if (AXObjectCache* cache =
            DomWindow()->document()->ExistingAXObjectCache()) {
      cache->HandleLoadStart(DomWindow()->document());
    }
  }
}

void NavigateEvent::ReactDone(ScriptValue value, bool did_fulfill) {
  CHECK_NE(intercept_state_, InterceptState::kFinished);

  LocalDOMWindow* window = DomWindow();
  if (signal_->aborted() || !window) {
    return;
  }

  delayed_load_start_task_handle_.Cancel();

  CHECK_EQ(this, window->navigation()->ongoing_navigate_event_);
  window->navigation()->ongoing_navigate_event_ = nullptr;

  if (intercept_state_ == InterceptState::kIntercepted) {
    if (did_fulfill) {
      CommitNow();
    } else {
      DomWindow()->GetFrame()->Client()->DidFailAsyncSameDocumentCommit();
    }
  }

  if (intercept_state_ >= InterceptState::kCommitted) {
    PotentiallyResetTheFocus();
    if (did_fulfill) {
      PotentiallyProcessScrollBehavior();
    }
    intercept_state_ = InterceptState::kFinished;
  }

  if (did_fulfill) {
    window->navigation()->DidFinishOngoingNavigation();
  } else {
    window->navigation()->DidFailOngoingNavigation(value);
  }

  if (HasNavigationActions()) {
    if (LocalFrame* frame = window->GetFrame()) {
      frame->Loader().DidFinishNavigation(
          did_fulfill ? FrameLoader::NavigationFinishState::kSuccess
                      : FrameLoader::NavigationFinishState::kFailure);
    }
    if (AXObjectCache* cache = window->document()->ExistingAXObjectCache()) {
      cache->HandleLoadComplete(window->document());
    }
  }
}

void NavigateEvent::Abort(ScriptState* script_state, ScriptValue error) {
  if (IsBeingDispatched()) {
    preventDefault();
  }
  CHECK(controller_);
  controller_->abort(script_state, error);
  delayed_load_start_task_handle_.Cancel();
}

void NavigateEvent::DelayedLoadStartTimerFired() {
  if (!DomWindow()) {
    return;
  }

  auto& frame_host = DomWindow()->GetFrame()->GetLocalFrameHostRemote();
  frame_host.StartLoadingForAsyncNavigationApiCommit();
}

void NavigateEvent::FinalizeNavigationActionPromisesList() {
  HeapVector<Member<V8NavigationInterceptHandler>> handlers_list;
  handlers_list.swap(navigation_action_handlers_list_);

  for (auto& function : handlers_list) {
    ScriptPromise<IDLUndefined> result;
    if (function->Invoke(this).To(&result))
      navigation_action_promises_list_.push_back(result);
  }
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
    focus_delegate->Focus(FocusParams(FocusTrigger::kUserGesture));
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

WebFrameLoadType LoadTypeFromNavigation(
    V8NavigationType::Enum navigation_type) {
  switch (navigation_type) {
    case V8NavigationType::Enum::kPush:
      return WebFrameLoadType::kStandard;
    case V8NavigationType::Enum::kReplace:
      return WebFrameLoadType::kReplaceCurrentItem;
    case V8NavigationType::Enum::kTraverse:
      return WebFrameLoadType::kBackForward;
    case V8NavigationType::Enum::kReload:
      return WebFrameLoadType::kReload;
  }
  NOTREACHED();
}

void NavigateEvent::ProcessScrollBehavior() {
  CHECK_EQ(intercept_state_, InterceptState::kCommitted);
  intercept_state_ = InterceptState::kScrolled;

  std::optional<HistoryItem::ViewState> view_state =
      dispatch_params_->destination_item
          ? dispatch_params_->destination_item->GetViewState()
          : std::nullopt;
  auto scroll_behavior = has_ua_visual_transition_
                             ? mojom::blink::ScrollBehavior::kInstant
                             : mojom::blink::ScrollBehavior::kAuto;
  // Use mojom::blink::ScrollRestorationType::kAuto unconditionally here
  // because we are certain that we want to actually scroll if we reach this
  // point. Using mojom::blink::ScrollRestorationType::kManual would block the
  // scroll.
  DomWindow()->GetFrame()->Loader().ProcessScrollForSameDocumentNavigation(
      dispatch_params_->url, LoadTypeFromNavigation(navigation_type_),
      view_state, mojom::blink::ScrollRestorationType::kAuto, scroll_behavior);
}

const AtomicString& NavigateEvent::InterfaceName() const {
  return event_interface_names::kNavigateEvent;
}

void NavigateEvent::Trace(Visitor* visitor) const {
  Event::Trace(visitor);
  ExecutionContextClient::Trace(visitor);
  visitor->Trace(dispatch_params_);
  visitor->Trace(destination_);
  visitor->Trace(controller_);
  visitor->Trace(signal_);
  visitor->Trace(form_data_);
  visitor->Trace(info_);
  visitor->Trace(source_element_);
  visitor->Trace(navigation_action_promises_list_);
  visitor->Trace(navigation_action_handlers_list_);
}

}  // namespace blink
