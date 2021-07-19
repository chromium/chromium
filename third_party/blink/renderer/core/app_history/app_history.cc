// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/app_history/app_history.h"

#include <memory>

#include "third_party/blink/renderer/bindings/core/v8/script_function.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_app_history_navigate_event_init.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_app_history_navigate_options.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/core/app_history/app_history_destination.h"
#include "third_party/blink/renderer/core/app_history/app_history_entry.h"
#include "third_party/blink/renderer/core/app_history/app_history_navigate_event.h"
#include "third_party/blink/renderer/core/dom/abort_signal.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/events/error_event.h"
#include "third_party/blink/renderer/core/frame/history_util.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/html/forms/form_data.h"
#include "third_party/blink/renderer/core/loader/document_loader.h"
#include "third_party/blink/renderer/core/loader/frame_load_request.h"

namespace blink {

namespace {

class NavigateReaction final : public ScriptFunction {
 public:
  enum class ResolveType { kFulfill, kReject, kDetach };
  static void React(ScriptState* script_state,
                    ScriptPromise promise,
                    ScriptPromiseResolver* resolver) {
    promise.Then(CreateFunction(script_state, resolver, ResolveType::kFulfill),
                 CreateFunction(script_state, resolver, ResolveType::kReject));
  }

  static void CleanupWithoutResolving(ScriptState* script_state,
                                      ScriptPromiseResolver* resolver) {
    ScriptPromise::CastUndefined(script_state)
        .Then(CreateFunction(script_state, resolver, ResolveType::kDetach));
  }

  NavigateReaction(ScriptState* script_state,
                   ScriptPromiseResolver* resolver,
                   ResolveType type)
      : ScriptFunction(script_state),
        window_(LocalDOMWindow::From(script_state)),
        resolver_(resolver),
        type_(type) {}

  void Trace(Visitor* visitor) const final {
    ScriptFunction::Trace(visitor);
    visitor->Trace(window_);
    visitor->Trace(resolver_);
  }

 private:
  static v8::Local<v8::Function> CreateFunction(ScriptState* script_state,
                                                ScriptPromiseResolver* resolver,
                                                ResolveType type) {
    return MakeGarbageCollected<NavigateReaction>(script_state, resolver, type)
        ->BindToV8Function();
  }

  Event* InitEvent(ScriptValue value) const {
    if (type_ == ResolveType::kFulfill)
      return Event::Create(event_type_names::kNavigatesuccess);

    auto* isolate = window_->GetIsolate();
    v8::Local<v8::Message> message =
        v8::Exception::CreateMessage(isolate, value.V8Value());
    std::unique_ptr<SourceLocation> location =
        SourceLocation::FromMessage(isolate, message, window_);
    ErrorEvent* event = ErrorEvent::Create(
        ToCoreStringWithNullCheck(message->Get()), std::move(location), value,
        &DOMWrapperWorld::MainWorld());
    event->SetType(event_type_names::kNavigateerror);
    return event;
  }

  ScriptValue Call(ScriptValue value) final {
    DCHECK(window_);
    if (type_ == ResolveType::kDetach) {
      resolver_->Detach();
      window_ = nullptr;
      return ScriptValue();
    }
    AppHistory::appHistory(*window_)->DispatchEvent(*InitEvent(value));
    if (resolver_) {
      if (type_ == ResolveType::kFulfill)
        resolver_->Resolve(value);
      else
        resolver_->Reject(value);
    }
    window_ = nullptr;
    return ScriptValue();
  }

  Member<LocalDOMWindow> window_;
  Member<ScriptPromiseResolver> resolver_;
  ResolveType type_;
};

}  // namespace

const char AppHistory::kSupplementName[] = "AppHistory";

AppHistory* AppHistory::appHistory(LocalDOMWindow& window) {
  if (!RuntimeEnabledFeatures::AppHistoryEnabled())
    return nullptr;
  auto* app_history = Supplement<LocalDOMWindow>::From<AppHistory>(window);
  if (!app_history) {
    app_history = MakeGarbageCollected<AppHistory>(window);
    Supplement<LocalDOMWindow>::ProvideTo(window, app_history);
  }
  return app_history;
}

AppHistory::AppHistory(LocalDOMWindow& window)
    : Supplement<LocalDOMWindow>(window) {}

void AppHistory::PopulateKeySet() {
  DCHECK(keys_to_indices_.IsEmpty());
  for (size_t i = 0; i < entries_.size(); i++)
    keys_to_indices_.insert(entries_[i]->key(), i);
}

void AppHistory::InitializeForNavigation(
    HistoryItem& current,
    const WebVector<WebHistoryItem>& back_entries,
    const WebVector<WebHistoryItem>& forward_entries) {
  DCHECK(entries_.IsEmpty());

  // Construct |entries_|. Any back entries are inserted, then the current
  // entry, then any forward entries.
  entries_.ReserveCapacity(back_entries.size() + forward_entries.size() + 1);
  for (const auto& entry : back_entries) {
    entries_.emplace_back(
        MakeGarbageCollected<AppHistoryEntry>(GetSupplementable(), entry));
  }

  current_index_ = back_entries.size();
  entries_.emplace_back(
      MakeGarbageCollected<AppHistoryEntry>(GetSupplementable(), &current));

  for (const auto& entry : forward_entries) {
    entries_.emplace_back(
        MakeGarbageCollected<AppHistoryEntry>(GetSupplementable(), entry));
  }
  PopulateKeySet();
}

void AppHistory::CloneFromPrevious(AppHistory& previous) {
  DCHECK(entries_.IsEmpty());
  entries_.ReserveCapacity(previous.entries_.size());
  for (size_t i = 0; i < previous.entries_.size(); i++) {
    // It's possible that |old_item| is indirectly holding a reference to
    // the old Document. Also, it has a bunch of state we don't need for a
    // non-current entry. Clone a subset of its state to a |new_item|.
    HistoryItem* old_item = previous.entries_[i]->GetItem();
    HistoryItem* new_item = MakeGarbageCollected<HistoryItem>();
    new_item->SetItemSequenceNumber(old_item->ItemSequenceNumber());
    new_item->SetDocumentSequenceNumber(old_item->DocumentSequenceNumber());
    new_item->SetURL(old_item->Url());
    new_item->SetAppHistoryKey(old_item->GetAppHistoryKey());
    new_item->SetAppHistoryId(old_item->GetAppHistoryId());
    new_item->SetAppHistoryState(old_item->GetAppHistoryState());
    entries_.emplace_back(
        MakeGarbageCollected<AppHistoryEntry>(GetSupplementable(), new_item));
  }
  current_index_ = previous.current_index_;
  PopulateKeySet();
}

void AppHistory::UpdateForNavigation(HistoryItem& item, WebFrameLoadType type) {
  // A same-document navigation (e.g., a document.open()) in a newly created
  // iframe will try to operate on an empty |entries_|. appHistory considers
  // this a no-op.
  post_navigate_event_ongoing_navigation_signal_ = nullptr;
  if (entries_.IsEmpty())
    return;

  if (type == WebFrameLoadType::kBackForward) {
    // If this is a same-document back/forward navigation, the new current
    // entry should already be present in entries_ and its key in
    // keys_to_indices_.
    DCHECK(keys_to_indices_.Contains(item.GetAppHistoryKey()));
    current_index_ = keys_to_indices_.at(item.GetAppHistoryKey());
    if (goto_promise_resolver_) {
      goto_promise_resolver_->Resolve();
      goto_promise_resolver_ = nullptr;
    }
    return;
  }

  if (type == WebFrameLoadType::kStandard) {
    // For a new back/forward entry, truncate any forward entries and prepare to
    // append.
    current_index_++;
    for (size_t i = current_index_; i < entries_.size(); i++)
      keys_to_indices_.erase(entries_[i]->key());
    entries_.resize(current_index_ + 1);
  }

  // current_index_ is now correctly set (for type of
  // WebFrameLoadType::kReplaceCurrentItem/kReload/kReloadBypassingCache, it
  // didn't change). Create the new current entry.
  entries_[current_index_] =
      MakeGarbageCollected<AppHistoryEntry>(GetSupplementable(), &item);
  keys_to_indices_.insert(entries_[current_index_]->key(), current_index_);
}

AppHistoryEntry* AppHistory::current() const {
  // current_index_ is initialized to -1 and set >= 0 when entries_ is
  // populated. It will still be negative if the appHistory of an initial empty
  // document is accessed.
  return current_index_ >= 0 && GetSupplementable()->GetFrame()
             ? entries_[current_index_]
             : nullptr;
}

HeapVector<Member<AppHistoryEntry>> AppHistory::entries() {
  return GetSupplementable()->GetFrame()
             ? entries_
             : HeapVector<Member<AppHistoryEntry>>();
}

ScriptPromise AppHistory::navigate(ScriptState* script_state,
                                   const String& url,
                                   AppHistoryNavigateOptions* options,
                                   ExceptionState& exception_state) {
  if (!GetSupplementable()->GetFrame()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "navigate() may not be called in a "
                                      "detached window");
    return ScriptPromise();
  }

  KURL completed_url(GetSupplementable()->Url(), url);
  if (!completed_url.IsValid()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kSyntaxError,
                                      "Invalid url");
    return ScriptPromise();
  }

  navigate_serialized_state_ = nullptr;
  if (options->hasState()) {
    navigate_serialized_state_ = SerializedScriptValue::Serialize(
        GetSupplementable()->GetIsolate(), options->state().V8Value(),
        SerializedScriptValue::SerializeOptions(
            SerializedScriptValue::kForStorage),
        exception_state);
    if (exception_state.HadException())
      return ScriptPromise();
  }

  base::AutoReset<Member<ScriptPromiseResolver>> promise(
      &navigate_method_call_promise_resolver_,
      MakeGarbageCollected<ScriptPromiseResolver>(script_state));
  base::AutoReset<bool> did_react(&did_react_to_promise_, false);
  base::AutoReset<ScriptValue> event_info(
      &navigate_event_info_,
      options->getNavigateInfoOr(
          ScriptValue(script_state->GetIsolate(),
                      v8::Undefined(script_state->GetIsolate())))
  );
  WebFrameLoadType frame_load_type = options->replace()
                                         ? WebFrameLoadType::kReplaceCurrentItem
                                         : WebFrameLoadType::kStandard;
  GetSupplementable()->GetFrame()->MaybeLogAdClickNavigation();

  FrameLoadRequest request(GetSupplementable(), ResourceRequest(completed_url));
  request.SetClientRedirectReason(ClientNavigationReason::kFrameNavigation);
  GetSupplementable()->GetFrame()->Navigate(request, frame_load_type);

  // The spec says to handle the window-detach case in DispatchNavigateEvent()
  // using navigate_method_call_promise_resolver_, but ScriptPromiseResolver
  // clears its state on window detach, so we can't use it to return a rejected
  // promise in the detach case (it returns undefined instead). Rather than
  // bypassing ScriptPromiseResolver and managing our own v8::Promise::Resolver,
  // special case detach here.
  if (!GetSupplementable()->GetFrame()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kAbortError,
                                      "Navigation was aborted");
    return ScriptPromise();
  }

  // The spec assumes it's ok to leave a promise permanently unresolved, but
  // ScriptPromiseResolver requires either resolution or explicit detach.
  // Do the detach on a microtask so that we can still return the promise.
  if (!did_react_to_promise_) {
    NavigateReaction::CleanupWithoutResolving(
        script_state, navigate_method_call_promise_resolver_);
  }
  if (navigate_serialized_state_)
    current()->GetItem()->SetAppHistoryState(navigate_serialized_state_);
  return navigate_method_call_promise_resolver_->Promise();
}

ScriptPromise AppHistory::navigate(ScriptState* script_state,
                                   AppHistoryNavigateOptions* options,
                                   ExceptionState& exception_state) {
  if (!options->hasState() && !options->hasNavigateInfo()) {
    exception_state.ThrowTypeError(
        "Must pass at least one of url, state, or navigateInfo to navigate()");
    return ScriptPromise();
  }
  return navigate(script_state, GetSupplementable()->Url(), options,
                  exception_state);
}

ScriptPromise AppHistory::goTo(ScriptState* script_state,
                               const String& key,
                               AppHistoryNavigationOptions* options,
                               ExceptionState& exception_state) {
  if (!GetSupplementable()->GetFrame()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "Window is detached");
    return ScriptPromise();
  }
  if (!keys_to_indices_.Contains(key)) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "Invalid key");
    return ScriptPromise();
  }
  if (key == current()->key())
    return ScriptPromise::CastUndefined(script_state);

  if (goto_promise_resolver_) {
    goto_promise_resolver_->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kAbortError, "Navigation was aborted"));
    goto_promise_resolver_ = nullptr;
  }

  AppHistoryEntry* destination = entries_[keys_to_indices_.at(key)];

  goto_promise_resolver_ =
      MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  // TODO(https://crbug.com/1220750): This can get "leaked" for the lifetime of
  // the window in certain cases.
  goto_navigate_event_info_ = options->getNavigateInfoOr(ScriptValue(
      script_state->GetIsolate(), v8::Undefined(script_state->GetIsolate())));
  goto_item_sequence_number_ = destination->GetItem()->ItemSequenceNumber();

  // TODO(japhet): We will fire the navigate event for same-document navigations
  // at commit time, but not cross-document. This should probably move to a more
  // central location if we want to fire the navigate event for cross-document
  // back-forward navigations in general.
  if (!destination->sameDocument()) {
    if (DispatchNavigateEvent(
            destination->url(), nullptr, NavigateEventType::kCrossDocument,
            WebFrameLoadType::kBackForward, UserNavigationInvolvement::kNone,
            nullptr,
            destination->GetItem()) != AppHistory::DispatchResult::kContinue) {
      exception_state.ThrowDOMException(DOMExceptionCode::kAbortError,
                                        "Navigation was aborted");
      return ScriptPromise();
    }
  }

  GetSupplementable()
      ->GetFrame()
      ->GetLocalFrameHostRemote()
      .NavigateToAppHistoryKey(key, LocalFrame::HasTransientUserActivation(
                                        GetSupplementable()->GetFrame()));
  return goto_promise_resolver_->Promise();
}

bool AppHistory::canGoBack() const {
  return current_index_ > 0;
}

bool AppHistory::canGoForward() const {
  return current_index_ != -1 &&
         static_cast<size_t>(current_index_) < entries_.size() - 1;
}

ScriptPromise AppHistory::back(ScriptState* script_state,
                               AppHistoryNavigationOptions* options,
                               ExceptionState& exception_state) {
  if (!canGoBack()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "Cannot go back");
    return ScriptPromise();
  }
  return goTo(script_state, entries_[current_index_ - 1]->key(), options,
              exception_state);
}

ScriptPromise AppHistory::forward(ScriptState* script_state,
                                  AppHistoryNavigationOptions* options,
                                  ExceptionState& exception_state) {
  if (!canGoForward()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "Cannot go forward");
    return ScriptPromise();
  }
  return goTo(script_state, entries_[current_index_ + 1]->key(), options,
              exception_state);
}

AppHistory::DispatchResult AppHistory::DispatchNavigateEvent(
    const KURL& url,
    HTMLFormElement* form,
    NavigateEventType event_type,
    WebFrameLoadType type,
    UserNavigationInvolvement involvement,
    SerializedScriptValue* state_object,
    HistoryItem* destination_item) {
  if (GetSupplementable()->document()->IsInitialEmptyDocument())
    return DispatchResult::kContinue;

  // TODO(japhet): The draft spec says to cancel any ongoing navigate event
  // before invoked DispatchNavigateEvent(), because not all navigations will
  // fire a navigate event, but all should abort an ongoing navigate event.
  // The main case were that would be a problem (browser-initiated back/forward)
  // is not implemented yet. Move this once it is implemented.
  CancelOngoingNavigateEvent();

  auto* script_state =
      ToScriptStateForMainWorld(GetSupplementable()->GetFrame());
  ScriptState::Scope scope(script_state);

  const KURL& current_url = GetSupplementable()->Url();

  auto* init = AppHistoryNavigateEventInit::Create();
  init->setCancelable(involvement != UserNavigationInvolvement::kBrowserUI ||
                      type != WebFrameLoadType::kBackForward);
  init->setCanRespond(
      CanChangeToUrlForHistoryApi(url, GetSupplementable()->GetSecurityOrigin(),
                                  current_url) &&
      (event_type != NavigateEventType::kCrossDocument ||
       type != WebFrameLoadType::kBackForward));
  init->setHashChange(event_type == NavigateEventType::kFragment &&
                      url != current_url &&
                      EqualIgnoringFragmentIdentifier(url, current_url));

  SerializedScriptValue* destination_state = nullptr;
  if (destination_item)
    destination_state = destination_item->GetAppHistoryState();
  else if (navigate_serialized_state_)
    destination_state = navigate_serialized_state_.get();
  init->setDestination(MakeGarbageCollected<AppHistoryDestination>(
      url, event_type != NavigateEventType::kCrossDocument, destination_state));

  init->setUserInitiated(involvement != UserNavigationInvolvement::kNone);
  init->setFormData(form ? FormData::Create(form, ASSERT_NO_EXCEPTION)
                         : nullptr);
  if (!(navigate_event_info_.IsEmpty() || navigate_event_info_.IsUndefined())) {
    init->setInfo(navigate_event_info_);
  } else if (!(goto_navigate_event_info_.IsEmpty() ||
               goto_navigate_event_info_.IsUndefined()) &&
             destination_item) {
    if (goto_item_sequence_number_ == destination_item->ItemSequenceNumber())
      init->setInfo(goto_navigate_event_info_);
    goto_navigate_event_info_ = ScriptValue();
  }
  auto* navigate_event = AppHistoryNavigateEvent::Create(
      GetSupplementable(), event_type_names::kNavigate, init);
  navigate_event->SetUrl(url);

  DCHECK(!ongoing_navigate_event_);
  ongoing_navigate_event_ = navigate_event;
  DispatchEvent(*navigate_event);
  ongoing_navigate_event_ = nullptr;

  if (!GetSupplementable()->GetFrame()) {
    FinalizeWithAbortedNavigationError(script_state, navigate_event->signal());
    return DispatchResult::kAbort;
  }

  ScriptPromise promise = navigate_event->GetNavigationActionPromise();
  bool respondWithCalled = false;
  if (!promise.IsEmpty()) {
    respondWithCalled = true;
    // The spec says that at this point we should either run the URL and history
    // update steps (for non-traverse cases) or we should do a same-document
    // history traversal. In our implementation it's easier for the caller to do
    // a history traversal since it has access to all the info it needs.
    // TODO(japhet): Figure out how cross-document back-forward should work.
    if (type != WebFrameLoadType::kBackForward) {
      GetSupplementable()->document()->Loader()->RunURLAndHistoryUpdateSteps(
          url, kSameDocumentNavigationAppHistoryRespondWith, state_object,
          type);
    }
  }

  if (!promise.IsEmpty() || (!navigate_event->defaultPrevented() &&
                             event_type != NavigateEventType::kCrossDocument)) {
    if (promise.IsEmpty())
      promise = ScriptPromise::CastUndefined(script_state);
    if (navigate_method_call_promise_resolver_)
      did_react_to_promise_ = true;
    NavigateReaction::React(script_state, promise,
                            navigate_method_call_promise_resolver_);
  } else {
    navigate_serialized_state_.reset();
  }

  if (navigate_event->defaultPrevented() && promise.IsEmpty())
    FinalizeWithAbortedNavigationError(script_state, navigate_event->signal());

  if (!navigate_event->defaultPrevented()) {
    post_navigate_event_ongoing_navigation_signal_ = navigate_event->signal();
    return DispatchResult::kContinue;
  }
  return respondWithCalled ? DispatchResult::kRespondWith
                           : DispatchResult::kAbort;
}

void AppHistory::CancelOngoingNavigateEvent() {
  DCHECK(!ongoing_navigate_event_ ||
         !post_navigate_event_ongoing_navigation_signal_);
  AbortSignal* signal = nullptr;
  if (ongoing_navigate_event_) {
    ongoing_navigate_event_->preventDefault();
    ongoing_navigate_event_->ClearNavigationActionPromise();
    signal = ongoing_navigate_event_->signal();
    ongoing_navigate_event_ = nullptr;
  } else if (post_navigate_event_ongoing_navigation_signal_) {
    signal = post_navigate_event_ongoing_navigation_signal_;
    post_navigate_event_ongoing_navigation_signal_ = nullptr;
  }

  if (!signal)
    return;
  auto* script_state =
      ToScriptStateForMainWorld(GetSupplementable()->GetFrame());
  ScriptState::Scope scope(script_state);
  FinalizeWithAbortedNavigationError(script_state, signal);
}

void AppHistory::FinalizeWithAbortedNavigationError(ScriptState* script_state,
                                                    AbortSignal* signal) {
  if (did_react_to_promise_)
    return;
  navigate_serialized_state_ = nullptr;
  signal->SignalAbort();
  ScriptPromise promise = ScriptPromise::RejectWithDOMException(
      script_state,
      MakeGarbageCollected<DOMException>(DOMExceptionCode::kAbortError,
                                         "Navigation was aborted"));
  if (navigate_method_call_promise_resolver_)
    did_react_to_promise_ = true;
  NavigateReaction::React(script_state, promise,
                          navigate_method_call_promise_resolver_);
}

int AppHistory::GetIndexFor(AppHistoryEntry* entry) {
  const auto& it = keys_to_indices_.find(entry->key());
  if (it == keys_to_indices_.end() || entry != entries_[it->value])
    return -1;
  return it->value;
}

const AtomicString& AppHistory::InterfaceName() const {
  return event_target_names::kAppHistory;
}

void AppHistory::Trace(Visitor* visitor) const {
  EventTargetWithInlineData::Trace(visitor);
  Supplement<LocalDOMWindow>::Trace(visitor);
  visitor->Trace(entries_);
  visitor->Trace(ongoing_navigate_event_);
  visitor->Trace(navigate_method_call_promise_resolver_);
  visitor->Trace(goto_promise_resolver_);
  visitor->Trace(navigate_event_info_);
  visitor->Trace(goto_navigate_event_info_);
  visitor->Trace(post_navigate_event_ongoing_navigation_signal_);
}

}  // namespace blink
