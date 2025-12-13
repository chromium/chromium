// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/navigation_api/navigate_event.h"

#include "third_party/blink/public/mojom/devtools/console_message.mojom-shared.h"
#include "third_party/blink/public/mojom/frame/frame.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/promise_all.h"
#include "third_party/blink/renderer/bindings/core/v8/script_function.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_navigate_event_init.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_navigation_intercept_handler.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_navigation_intercept_options.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_navigation_intercept_precommit_handler.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_navigation_navigate_options.h"
#include "third_party/blink/renderer/core/accessibility/ax_object_cache.h"
#include "third_party/blink/renderer/core/dom/abort_controller.h"
#include "third_party/blink/renderer/core/dom/abort_signal.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/focus_params.h"
#include "third_party/blink/renderer/core/event_interface_names.h"
#include "third_party/blink/renderer/core/event_type_names.h"
#include "third_party/blink/renderer/core/frame/deprecation/deprecation.h"
#include "third_party/blink/renderer/core/frame/history_util.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_client.h"
#include "third_party/blink/renderer/core/html/forms/form_data.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/loader/document_loader.h"
#include "third_party/blink/renderer/core/loader/progress_tracker.h"
#include "third_party/blink/renderer/core/navigation_api/navigation_api_method_tracker.h"
#include "third_party/blink/renderer/core/navigation_api/navigation_destination.h"
#include "third_party/blink/renderer/core/navigation_api/navigation_precommit_controller.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cancellable_task.h"

namespace blink {

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

enum class HandlerPhase { kPrecommit, kPostcommit };

class NavigateEvent::FulfillReaction final
    : public ThenCallable<IDLUndefined, FulfillReaction> {
 public:
  FulfillReaction(NavigateEvent* navigate_event, HandlerPhase type)
      : navigate_event_(navigate_event), type_(type) {}
  void Trace(Visitor* visitor) const final {
    ThenCallable<IDLUndefined, FulfillReaction>::Trace(visitor);
    visitor->Trace(navigate_event_);
  }
  void React(ScriptState* script_state) {
    if (type_ == HandlerPhase::kPrecommit) {
      navigate_event_->CommitNow(script_state);
    } else {
      navigate_event_->ReactDone(script_state, ScriptValue(),
                                 /*did_fulfill=*/true);
    }
  }

 private:
  Member<NavigateEvent> navigate_event_;
  HandlerPhase type_;
};

class NavigateEvent::RejectReaction final
    : public ThenCallable<IDLAny, RejectReaction> {
 public:
  explicit RejectReaction(NavigateEvent* navigate_event)
      : navigate_event_(navigate_event) {}
  void Trace(Visitor* visitor) const final {
    ThenCallable<IDLAny, RejectReaction>::Trace(visitor);
    visitor->Trace(navigate_event_);
  }
  void React(ScriptState* script_state, ScriptValue value) {
    navigate_event_->ReactDone(script_state, value, /*did_fulfill=*/false);
  }

 private:
  Member<NavigateEvent> navigate_event_;
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
        StrCat({function_name, "() may not be called in a detached window."}));
    return false;
  }
  if (!isTrusted()) {
    exception_state.ThrowSecurityError(
        StrCat({function_name, "() may only be called on a trusted event."}));
    return false;
  }
  if (defaultPrevented()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        StrCat({function_name,
                "() may not be called if the event has been canceled."}));
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
        StrCat({"A navigation with URL '", dispatch_params_->url.ElidedString(),
                "' cannot be intercepted by in a window with origin '",
                DomWindow()->GetSecurityOrigin()->ToString(), "' and URL '",
                DomWindow()->Url().ElidedString(), "'."}));
    return;
  }

  if (!IsBeingDispatched()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "intercept() may only be called while the navigate event is being "
        "dispatched.");
    return;
  }

  if (options->hasPrecommitHandler()) {
    CHECK(RuntimeEnabledFeatures::NavigateEventCommitBehaviorEnabled());
    if (!cancelable()) {
      exception_state.ThrowDOMException(
          DOMExceptionCode::kInvalidStateError,
          "intercept() may only be called with a precommitHandler when the"
          "navigate event is cancelable.");
      return;
    }
    navigation_action_precommit_handlers_list_.push_back(
        options->precommitHandler());
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
              StrCat({"The \"", options->focusReset().AsStringView(),
                      "\" value for intercept()'s focusReset option will "
                      "override the previously-passed value of \"",
                      focus_reset_behavior_->AsStringView(), "\"."})));
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
              StrCat({"The \"", options->scroll().AsStringView(),
                      "\" value for intercept()'s scroll option will override "
                      "the previously-passed value of \"",
                      scroll_behavior_->AsStringView(), "\"."})));
    }
    scroll_behavior_ = options->scroll();
  }

  CHECK(intercept_state_ == InterceptState::kNone ||
        intercept_state_ == InterceptState::kIntercepted);
  intercept_state_ = InterceptState::kIntercepted;
  if (options->hasHandler()) {
    navigation_action_handlers_list_.push_back(options->handler());
  }
}

void NavigateEvent::Redirect(const String& url_string,
                             NavigationNavigateOptions* options,
                             ExceptionState& exception_state) {
  CHECK_NE(intercept_state_, InterceptState::kNone);
  if (!PerformSharedChecks("redirect", exception_state)) {
    return;
  }

  if (intercept_state_ > InterceptState::kIntercepted) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "navigation has already committed.");
    return;
  }

  if (navigation_type_ != V8NavigationType::Enum::kPush &&
      navigation_type_ != V8NavigationType::Enum::kReplace) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "redirect() may only be used on push and replace navigations.");
    return;
  }

  KURL url = KURL(DomWindow()->BaseURL(), url_string);
  if (!url.IsValid()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kSyntaxError,
        StrCat({"Invalid URL '", url.GetString(), "'."}));
    return;
  }
  if (!CanChangeToUrlForHistoryApi(url, DomWindow()->GetSecurityOrigin(),
                                   DomWindow()->Url())) {
    exception_state.ThrowSecurityError(
        StrCat({"Cannot redirect to '", url.ElidedString(),
                "' in a document with origin '",
                DomWindow()->GetSecurityOrigin()->ToString(), "' and URL '",
                DomWindow()->Url().ElidedString(), "'."}));
    return;
  }

  if (options->history() == V8NavigationHistoryBehavior::Enum::kPush &&
      DomWindow()->GetFrame()->ShouldMaintainTrivialSessionHistory()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kNotSupportedError,
        "redirect() may not override the history behavior when navigating in a "
        "trivial session history context");
    return;
  }

  if (options->hasState()) {
    scoped_refptr<SerializedScriptValue> serialized_state =
        SerializedScriptValue::Serialize(
            DomWindow()->GetIsolate(), options->state().V8Value(),
            SerializedScriptValue::SerializeOptions(
                SerializedScriptValue::kForStorage),
            exception_state);
    if (exception_state.HadException()) {
      return;
    }
    destination_->SetSerializedState(serialized_state);
    if (auto* api_method_tracker =
            DomWindow()->navigation()->ongoing_api_method_tracker_.Get()) {
      api_method_tracker->SetSerializedState(serialized_state);
    }
  }

  dispatch_params_->url = url;

  if (options->history() == V8NavigationHistoryBehavior::Enum::kPush) {
    navigation_type_ = V8NavigationType::Enum::kPush;
  } else if (options->history() ==
             V8NavigationHistoryBehavior::Enum::kReplace) {
    navigation_type_ = V8NavigationType::Enum::kReplace;
  }

  if (options->hasInfo()) {
    info_ = options->info();
  }
}

void NavigateEvent::AddHandlerDuringPrecommit(
    V8NavigationInterceptHandler* handler,
    ExceptionState& exception_state) {
  if (!PerformSharedChecks("addHandler", exception_state)) {
    return;
  }

  if (intercept_state_ > InterceptState::kIntercepted) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "navigation has already committed.");
    return;
  }

  navigation_action_handlers_list_.push_back(handler);
}

void NavigateEvent::MaybeCommitImmediately(ScriptState* script_state) {
  delayed_load_start_task_handle_ = PostDelayedCancellableTask(
      *DomWindow()->GetTaskRunner(TaskType::kInternalLoading), FROM_HERE,
      BindOnce(&NavigateEvent::DelayedLoadStartTimerFired,
               WrapWeakPersistent(this)),
      kDelayLoadStart);

  if (navigation_action_precommit_handlers_list_.empty()) {
    CommitNow(script_state);
    return;
  }

  DomWindow()->GetFrame()->Loader().Progress().ProgressStarted();

  HeapVector<Member<V8NavigationInterceptPrecommitHandler>> handlers_list;
  handlers_list.swap(navigation_action_precommit_handlers_list_);

  auto* controller = MakeGarbageCollected<NavigationPrecommitController>(this);

  HeapVector<MemberScriptPromise<IDLUndefined>> precommit_promises_list;
  for (auto& function : handlers_list) {
    ScriptPromise<IDLUndefined> result;
    if (function->Invoke(this, controller).To(&result)) {
      precommit_promises_list.push_back(result);
    }
  }

  PromiseAll<IDLUndefined>::Create(script_state, precommit_promises_list)
      .Then(
          script_state,
          MakeGarbageCollected<FulfillReaction>(this, HandlerPhase::kPrecommit),
          MakeGarbageCollected<RejectReaction>(this));
}

void NavigateEvent::CommitNow(ScriptState* script_state) {
  CHECK_EQ(intercept_state_, InterceptState::kIntercepted);
  CHECK(!dispatch_params_->destination_item || !dispatch_params_->state_object);
  if (signal_->aborted()) {
    return;
  }

  intercept_state_ = InterceptState::kCommitted;

  auto* state_object = dispatch_params_->destination_item
                           ? dispatch_params_->destination_item->StateObject()
                           : dispatch_params_->state_object.get();
  auto fire_popstate =
      dispatch_params_->event_type == NavigateEventType::kFragment &&
              (!DomWindow()->navigation()->ongoing_api_method_tracker_ ||
               navigation_type_ == V8NavigationType::Enum::kTraverse)
          ? FirePopstate::kYes
          : FirePopstate::kNo;
  if (!RuntimeEnabledFeatures::NavigateEventPopstateLimitationsEnabled() &&
      fire_popstate == FirePopstate::kNo &&
      dispatch_params_->event_type != NavigateEventType::kHistoryApi) {
    fire_popstate = FirePopstate::kYes;
  }

  // In the spec, the URL and history update steps are not called for reloads.
  // In our implementation, we call the corresponding function anyway, but
  // |type| being a reload type makes it do none of the spec-relevant
  // steps. Instead it does stuff like the loading spinner and use counters.
  DomWindow()->document()->Loader()->RunURLAndHistoryUpdateSteps(
      dispatch_params_->url, dispatch_params_->destination_item,
      mojom::blink::SameDocumentNavigationType::kNavigationApiIntercept,
      state_object, LoadTypeFromNavigation(navigation_type_), fire_popstate,
      dispatch_params_->should_skip_screenshot,
      dispatch_params_->is_browser_initiated,
      dispatch_params_->is_synchronously_committed_same_document,
      dispatch_params_->soft_navigation_heuristics_task_id);

  React(script_state);
}

void NavigateEvent::React(ScriptState* script_state) {
  CHECK(navigation_action_handlers_list_.empty());

  if (navigation_action_promises_list_.empty()) {
    // There is a subtle timing difference between the fast-path for zero
    // promises and the path for 1+ promises, in both spec and implementation.
    // In most uses of Promise.all() / the Web IDL spec's "wait for
    // all", this does not matter. However for us there are so many events and
    // promise handlers firing around the same time (navigatesuccess, committed
    // promise, finished promise, ...) that the difference is pretty easily
    // observable by web developers and web platform tests. So, let's make sure
    // we always go down the 1+ promises path.
    navigation_action_promises_list_.push_back(
        ToResolvedUndefinedPromise(script_state));
  }

  auto promise = PromiseAll<IDLUndefined>::Create(
      script_state, navigation_action_promises_list_);
  promise.Then(
      script_state,
      MakeGarbageCollected<FulfillReaction>(this, HandlerPhase::kPostcommit),
      MakeGarbageCollected<RejectReaction>(this));

  if (HasNavigationActions() && DomWindow()) {
    if (AXObjectCache* cache =
            DomWindow()->document()->ExistingAXObjectCache()) {
      cache->HandleLoadStart(DomWindow()->document());
    }
  }
}

void NavigateEvent::ReactDone(ScriptState* script_state,
                              ScriptValue value,
                              bool did_fulfill) {
  CHECK_NE(intercept_state_, InterceptState::kFinished);

  LocalDOMWindow* window = DomWindow();
  if (signal_->aborted() || !window) {
    return;
  }

  delayed_load_start_task_handle_.Cancel();

  CHECK_EQ(this, window->navigation()->ongoing_navigate_event_);
  window->navigation()->ongoing_navigate_event_ = nullptr;

  if (intercept_state_ == InterceptState::kIntercepted) {
    CHECK(!did_fulfill);
    window->GetFrame()->Client()->DidFailAsyncSameDocumentCommit();
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
    Abort(script_state, value);
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

  NavigationApi* navigation = DomWindow()->navigation();
  CHECK(controller_);
  controller_->abort(script_state, error);
  navigation->ongoing_navigate_event_ = nullptr;
  delayed_load_start_task_handle_.Cancel();
  if (!defaultPrevented() && intercept_state_ == InterceptState::kIntercepted) {
    DomWindow()->GetFrame()->Client()->DidFailAsyncSameDocumentCommit();
  }
  navigation->DidAbort(error);
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
  visitor->Trace(navigation_action_precommit_handlers_list_);
  visitor->Trace(navigation_action_promises_list_);
  visitor->Trace(navigation_action_handlers_list_);
}

}  // namespace blink
