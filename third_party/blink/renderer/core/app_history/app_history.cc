// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/app_history/app_history.h"

#include <memory>

#include "third_party/blink/public/web/web_frame_load_type.h"
#include "third_party/blink/renderer/bindings/core/v8/script_function.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_app_history_navigate_event_init.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_app_history_navigate_options.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_app_history_reload_options.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_app_history_update_current_options.h"
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
#include "third_party/blink/renderer/core/html/forms/html_form_element.h"
#include "third_party/blink/renderer/core/loader/document_loader.h"
#include "third_party/blink/renderer/core/loader/frame_load_request.h"
#include "third_party/blink/renderer/platform/wtf/uuid.h"

namespace blink {

class AppHistoryApiNavigation final
    : public GarbageCollected<AppHistoryApiNavigation> {
 public:
  AppHistoryApiNavigation(ScriptState* script_state,
                          AppHistoryNavigationOptions* options,
                          const String& key = String())
      : info(options->getInfoOr(
            ScriptValue(script_state->GetIsolate(),
                        v8::Undefined(script_state->GetIsolate())))),
        returned_promise(
            MakeGarbageCollected<ScriptPromiseResolver>(script_state)),
        key(key) {}

  ScriptValue info;
  Member<ScriptPromiseResolver> returned_promise;
  String key;
  bool did_react_to_promise = false;

  void Trace(Visitor* visitor) const {
    visitor->Trace(info);
    visitor->Trace(returned_promise);
  }
};

class NavigateReaction final : public ScriptFunction {
 public:
  enum class ResolveType { kFulfill, kReject, kDetach };
  static void React(ScriptState* script_state,
                    ScriptPromise promise,
                    AppHistoryApiNavigation* navigation,
                    AbortSignal* signal) {
    if (navigation)
      navigation->did_react_to_promise = true;
    promise.Then(
        CreateFunction(script_state, navigation, signal, ResolveType::kFulfill),
        CreateFunction(script_state, navigation, signal, ResolveType::kReject));
  }

  static void CleanupWithoutResolving(ScriptState* script_state,
                                      AppHistoryApiNavigation* navigation) {
    ScriptPromise::CastUndefined(script_state)
        .Then(CreateFunction(script_state, navigation, nullptr,
                             ResolveType::kDetach));
  }

  NavigateReaction(ScriptState* script_state,
                   AppHistoryApiNavigation* navigation,
                   AbortSignal* signal,
                   ResolveType type)
      : ScriptFunction(script_state),
        window_(LocalDOMWindow::From(script_state)),
        navigation_(navigation),
        signal_(signal),
        type_(type) {}

  void Trace(Visitor* visitor) const final {
    ScriptFunction::Trace(visitor);
    visitor->Trace(window_);
    visitor->Trace(navigation_);
    visitor->Trace(signal_);
  }

 private:
  static v8::Local<v8::Function> CreateFunction(
      ScriptState* script_state,
      AppHistoryApiNavigation* navigation,
      AbortSignal* signal,
      ResolveType type) {
    return MakeGarbageCollected<NavigateReaction>(script_state, navigation,
                                                  signal, type)
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
    if (signal_ && signal_->aborted()) {
      window_ = nullptr;
      return ScriptValue();
    }

    AppHistory* app_history = AppHistory::appHistory(*window_);
    if (navigation_) {
      if (navigation_->key.IsNull()) {
        if (navigation_ == app_history->ongoing_non_traversal_navigation_)
          app_history->ongoing_non_traversal_navigation_ = nullptr;
      } else {
        DCHECK(app_history->ongoing_traversals_.Contains(navigation_->key));
        app_history->ongoing_traversals_.erase(navigation_->key);
      }
    }

    if (type_ == ResolveType::kDetach) {
      navigation_->returned_promise->Detach();
      window_ = nullptr;
      return ScriptValue();
    }
    app_history->DispatchEvent(*InitEvent(value));
    if (navigation_) {
      if (type_ == ResolveType::kFulfill)
        navigation_->returned_promise->Resolve();
      else
        navigation_->returned_promise->Reject(value);
    }
    window_ = nullptr;
    return ScriptValue();
  }

  Member<LocalDOMWindow> window_;
  Member<AppHistoryApiNavigation> navigation_;
  Member<AbortSignal> signal_;
  ResolveType type_;
};

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
  for (wtf_size_t i = 0; i < entries_.size(); i++)
    keys_to_indices_.insert(entries_[i]->key(), i);
}

void AppHistory::InitializeForNavigation(
    HistoryItem& current,
    const WebVector<WebHistoryItem>& back_entries,
    const WebVector<WebHistoryItem>& forward_entries) {
  DCHECK(entries_.IsEmpty());

  // Construct |entries_|. Any back entries are inserted, then the current
  // entry, then any forward entries.
  entries_.ReserveCapacity(base::checked_cast<wtf_size_t>(
      back_entries.size() + forward_entries.size() + 1));
  for (const auto& entry : back_entries) {
    entries_.emplace_back(
        MakeGarbageCollected<AppHistoryEntry>(GetSupplementable(), entry));
  }

  current_index_ = base::checked_cast<wtf_size_t>(back_entries.size());
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
  for (wtf_size_t i = 0; i < previous.entries_.size(); i++) {
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
    return;
  }

  if (type == WebFrameLoadType::kStandard) {
    // For a new back/forward entry, truncate any forward entries and prepare to
    // append.
    current_index_++;
    for (wtf_size_t i = current_index_; i < entries_.size(); i++)
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

void AppHistory::updateCurrent(AppHistoryUpdateCurrentOptions* options,
                               ExceptionState& exception_state) {
  AppHistoryEntry* current_entry = current();

  if (!current_entry) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "updateCurrent() cannot be called when on the initial about:blank "
        "Document, or when the Window is detached.");
    return;
  }

  scoped_refptr<SerializedScriptValue> serialized_state =
      SerializeState(options->state(), exception_state);
  if (exception_state.HadException())
    return;

  current_entry->GetItem()->SetAppHistoryState(std::move(serialized_state));
}

ScriptPromise AppHistory::navigate(ScriptState* script_state,
                                   const String& url,
                                   AppHistoryNavigateOptions* options,
                                   ExceptionState& exception_state) {
  KURL completed_url(GetSupplementable()->Url(), url);
  if (!completed_url.IsValid()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kSyntaxError,
        "Invalid URL '" + completed_url.GetString() + "'.");
    return ScriptPromise();
  }

  PerformSharedNavigationChecks(exception_state, "navigate()");
  if (exception_state.HadException())
    return ScriptPromise();

  scoped_refptr<SerializedScriptValue> serialized_state = nullptr;
  if (options->hasState()) {
    serialized_state = SerializeState(options->state(), exception_state);
    if (exception_state.HadException())
      return ScriptPromise();
  }

  WebFrameLoadType frame_load_type = options->replace()
                                         ? WebFrameLoadType::kReplaceCurrentItem
                                         : WebFrameLoadType::kStandard;

  return PerformNonTraverseNavigation(script_state, completed_url,
                                      std::move(serialized_state), options,
                                      frame_load_type, exception_state);
}

ScriptPromise AppHistory::reload(ScriptState* script_state,
                                 AppHistoryReloadOptions* options,
                                 ExceptionState& exception_state) {
  PerformSharedNavigationChecks(exception_state, "reload()");
  if (exception_state.HadException())
    return ScriptPromise();

  scoped_refptr<SerializedScriptValue> serialized_state = nullptr;
  if (options->hasState()) {
    serialized_state = SerializeState(options->state(), exception_state);
    if (exception_state.HadException())
      return ScriptPromise();
  } else if (AppHistoryEntry* current_entry = current()) {
    serialized_state = current_entry->GetItem()->GetAppHistoryState();
  }

  return PerformNonTraverseNavigation(
      script_state, GetSupplementable()->Url(), std::move(serialized_state),
      options, WebFrameLoadType::kReload, exception_state);
}

ScriptPromise AppHistory::PerformNonTraverseNavigation(
    ScriptState* script_state,
    const KURL& url,
    scoped_refptr<SerializedScriptValue> serialized_state,
    AppHistoryNavigationOptions* options,
    WebFrameLoadType frame_load_type,
    ExceptionState& exception_state) {
  DCHECK(frame_load_type == WebFrameLoadType::kReplaceCurrentItem ||
         frame_load_type == WebFrameLoadType::kReload ||
         frame_load_type == WebFrameLoadType::kStandard);

  AppHistoryApiNavigation* navigation =
      MakeGarbageCollected<AppHistoryApiNavigation>(script_state, options);
  upcoming_non_traversal_navigation_ = navigation;

  to_be_set_serialized_state_ = serialized_state;

  GetSupplementable()->GetFrame()->MaybeLogAdClickNavigation();

  FrameLoadRequest request(GetSupplementable(), ResourceRequest(url));
  request.SetClientRedirectReason(ClientNavigationReason::kFrameNavigation);
  GetSupplementable()->GetFrame()->Navigate(request, frame_load_type);

  // The spec says to handle the window-detach case in DispatchNavigateEvent()
  // using navigation->returned_promise, but ScriptPromiseResolver
  // clears its state on window detach, so we can't use it to return a rejected
  // promise in the detach case (it returns undefined instead). Rather than
  // bypassing ScriptPromiseResolver and managing our own v8::Promise::Resolver,
  // special case detach here.
  if (!GetSupplementable()->GetFrame()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kAbortError,
                                      "Navigation was aborted");
    return ScriptPromise();
  }

  // DispatchNavigateEvent() will clear upcoming_non_traversal_navigation_ if we
  // get that far. If the navigation is blocked before DispatchNavigateEvent()
  // is called, reject the promise and cleanup here.
  if (upcoming_non_traversal_navigation_ == navigation) {
    upcoming_non_traversal_navigation_ = nullptr;
    exception_state.ThrowDOMException(DOMExceptionCode::kAbortError,
                                      "Navigation was aborted");
    return ScriptPromise();
  }

  // The spec assumes it's ok to leave a promise permanently unresolved, but
  // ScriptPromiseResolver requires either resolution or explicit detach.
  // Do the detach on a microtask so that we can still return the promise.
  if (!navigation->did_react_to_promise)
    NavigateReaction::CleanupWithoutResolving(script_state, navigation);
  if (to_be_set_serialized_state_) {
    current()->GetItem()->SetAppHistoryState(
        std::move(to_be_set_serialized_state_));
  }
  return navigation->returned_promise->Promise();
}

ScriptPromise AppHistory::goTo(ScriptState* script_state,
                               const String& key,
                               AppHistoryNavigationOptions* options,
                               ExceptionState& exception_state) {
  PerformSharedNavigationChecks(exception_state, "goTo()/back()/forward()");
  if (exception_state.HadException())
    return ScriptPromise();

  if (!keys_to_indices_.Contains(key)) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "Invalid key");
    return ScriptPromise();
  }
  if (key == current()->key())
    return ScriptPromise::CastUndefined(script_state);

  auto previous_navigation = ongoing_traversals_.find(key);
  if (previous_navigation != ongoing_traversals_.end()) {
    return previous_navigation->value->returned_promise->Promise();
  }

  AppHistoryApiNavigation* ongoing_navigation =
      MakeGarbageCollected<AppHistoryApiNavigation>(script_state, options, key);
  ongoing_traversals_.insert(key, ongoing_navigation);

  AppHistoryEntry* destination = entries_[keys_to_indices_.at(key)];

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
  return ongoing_navigation->returned_promise->Promise();
}

bool AppHistory::canGoBack() const {
  return GetSupplementable()->GetFrame() && current_index_ > 0;
}

bool AppHistory::canGoForward() const {
  return GetSupplementable()->GetFrame() && current_index_ != -1 &&
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

void AppHistory::PerformSharedNavigationChecks(
    ExceptionState& exception_state,
    const String& method_name_for_error_message) {
  if (!GetSupplementable()->GetFrame()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        method_name_for_error_message +
            " cannot be called when the Window is detached.");
  }
  if (GetSupplementable()->document()->PageDismissalEventBeingDispatched()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        method_name_for_error_message +
            " cannot be called during unload or beforeunload.");
  }
}

scoped_refptr<SerializedScriptValue> AppHistory::SerializeState(
    const ScriptValue& value,
    ExceptionState& exception_state) {
  return SerializedScriptValue::Serialize(
      GetSupplementable()->GetIsolate(), value.V8Value(),
      SerializedScriptValue::SerializeOptions(
          SerializedScriptValue::kForStorage),
      exception_state);
}

String DetermineNavigationType(WebFrameLoadType type) {
  switch (type) {
    case WebFrameLoadType::kStandard:
      return "push";
    case WebFrameLoadType::kBackForward:
      return "traverse";
    case WebFrameLoadType::kReload:
    case WebFrameLoadType::kReloadBypassingCache:
      return "reload";
    case WebFrameLoadType::kReplaceCurrentItem:
      return "replace";
  }
  NOTREACHED();
  return String();
}

AppHistory::DispatchResult AppHistory::DispatchNavigateEvent(
    const KURL& url,
    HTMLFormElement* form,
    NavigateEventType event_type,
    WebFrameLoadType type,
    UserNavigationInvolvement involvement,
    SerializedScriptValue* state_object,
    HistoryItem* destination_item) {
  // TODO(japhet): The draft spec says to cancel any ongoing navigate event
  // before invoking DispatchNavigateEvent(), because not all navigations will
  // fire a navigate event, but all should abort an ongoing navigate event.
  // The main case were that would be a problem (browser-initiated back/forward)
  // is not implemented yet. Move this once it is implemented.
  InformAboutCanceledNavigation();
  if (upcoming_non_traversal_navigation_) {
    ongoing_non_traversal_navigation_ =
        upcoming_non_traversal_navigation_.Release();
  }

  if (!GetSupplementable()->GetFrame()->Loader().HasLoadedNonEmptyDocument()) {
    if (ongoing_non_traversal_navigation_ &&
        event_type != NavigateEventType::kCrossDocument) {
      ongoing_non_traversal_navigation_->returned_promise->Resolve();
    }
    ongoing_non_traversal_navigation_ = nullptr;
    return DispatchResult::kContinue;
  }

  auto* script_state =
      ToScriptStateForMainWorld(GetSupplementable()->GetFrame());
  ScriptState::Scope scope(script_state);

  const KURL& current_url = GetSupplementable()->Url();

  AppHistoryApiNavigation* navigation = nullptr;
  if (destination_item && !destination_item->GetAppHistoryKey().IsNull()) {
    auto iter = ongoing_traversals_.find(destination_item->GetAppHistoryKey());
    navigation = iter == ongoing_traversals_.end() ? nullptr : iter->value;
  } else {
    navigation = ongoing_non_traversal_navigation_;
  }

  auto* init = AppHistoryNavigateEventInit::Create();
  init->setNavigationType(DetermineNavigationType(type));

  SerializedScriptValue* destination_state = nullptr;
  if (destination_item)
    destination_state = destination_item->GetAppHistoryState();
  else if (to_be_set_serialized_state_)
    destination_state = to_be_set_serialized_state_.get();
  AppHistoryDestination* destination =
      MakeGarbageCollected<AppHistoryDestination>(
          url, event_type != NavigateEventType::kCrossDocument,
          destination_state);
  if (type == WebFrameLoadType::kBackForward) {
    const String& key = destination_item->GetAppHistoryKey();
    auto iter = keys_to_indices_.find(key);
    int index = iter == keys_to_indices_.end() ? 0 : iter->value;
    destination->SetTraverseProperties(key, destination_item->GetAppHistoryId(),
                                       index);
  }
  init->setDestination(destination);

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

  init->setUserInitiated(involvement != UserNavigationInvolvement::kNone);
  if (form && form->Method() == FormSubmission::kPostMethod) {
    init->setFormData(FormData::Create(form, ASSERT_NO_EXCEPTION));
  }
  if (navigation)
    init->setInfo(navigation->info);
  init->setSignal(MakeGarbageCollected<AbortSignal>(GetSupplementable()));
  auto* navigate_event = AppHistoryNavigateEvent::Create(
      GetSupplementable(), event_type_names::kNavigate, init);
  navigate_event->SetUrl(url);

  DCHECK(!ongoing_navigate_event_);
  ongoing_navigate_event_ = navigate_event;
  DispatchEvent(*navigate_event);
  ongoing_navigate_event_ = nullptr;

  if (!GetSupplementable()->GetFrame()) {
    FinalizeWithAbortedNavigationError(script_state, navigation);
    return DispatchResult::kAbort;
  }

  auto promise_list = navigate_event->GetNavigationActionPromisesList();
  if (!promise_list.IsEmpty()) {
    // The spec says that at this point we should either run the URL and history
    // update steps (for non-traverse cases) or we should do a same-document
    // history traversal. In our implementation it's easier for the caller to do
    // a history traversal since it has access to all the info it needs.
    // TODO(japhet): Figure out how cross-document back-forward should work.
    if (type != WebFrameLoadType::kBackForward) {
      GetSupplementable()->document()->Loader()->RunURLAndHistoryUpdateSteps(
          url, mojom::blink::SameDocumentNavigationType::kAppHistoryRespondWith,
          state_object, type);
    }
  }
  post_navigate_event_ongoing_navigation_signal_ = navigate_event->signal();

  if (!promise_list.IsEmpty() ||
      (!navigate_event->defaultPrevented() &&
       event_type != NavigateEventType::kCrossDocument)) {
    ScriptPromise promise;
    if (promise_list.IsEmpty())
      promise = ScriptPromise::CastUndefined(script_state);
    else
      promise = ScriptPromise::All(script_state, promise_list);
    NavigateReaction::React(
        script_state, promise, navigation,
        promise_list.IsEmpty() ? nullptr : navigate_event->signal());
  } else {
    to_be_set_serialized_state_.reset();
  }

  if (navigate_event->defaultPrevented()) {
    FinalizeWithAbortedNavigationError(script_state, navigation);
    return DispatchResult::kAbort;
  }

  return promise_list.IsEmpty() ? DispatchResult::kContinue
                                : DispatchResult::kRespondWith;
}

void AppHistory::InformAboutCanceledNavigation() {
  if (ongoing_non_traversal_navigation_ || ongoing_navigate_event_ ||
      post_navigate_event_ongoing_navigation_signal_) {
    auto* script_state =
        ToScriptStateForMainWorld(GetSupplementable()->GetFrame());
    ScriptState::Scope scope(script_state);
    FinalizeWithAbortedNavigationError(script_state,
                                       ongoing_non_traversal_navigation_);
  }
}

void AppHistory::FinalizeWithAbortedNavigationError(
    ScriptState* script_state,
    AppHistoryApiNavigation* navigation) {
  DCHECK(!ongoing_navigate_event_ ||
         !post_navigate_event_ongoing_navigation_signal_);
  if (ongoing_navigate_event_) {
    ongoing_navigate_event_->preventDefault();
    ongoing_navigate_event_->signal()->SignalAbort();
    ongoing_navigate_event_ = nullptr;
  } else if (post_navigate_event_ongoing_navigation_signal_) {
    post_navigate_event_ongoing_navigation_signal_->SignalAbort();
    post_navigate_event_ongoing_navigation_signal_ = nullptr;
  }

  to_be_set_serialized_state_ = nullptr;
  if (navigation) {
    ScriptPromise promise = ScriptPromise::RejectWithDOMException(
        script_state,
        MakeGarbageCollected<DOMException>(DOMExceptionCode::kAbortError,
                                           "Navigation was aborted"));
    NavigateReaction::React(script_state, promise, navigation, nullptr);
  }
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
  visitor->Trace(ongoing_non_traversal_navigation_);
  visitor->Trace(ongoing_traversals_);
  visitor->Trace(upcoming_non_traversal_navigation_);
  visitor->Trace(ongoing_navigate_event_);
  visitor->Trace(post_navigate_event_ongoing_navigation_signal_);
}

}  // namespace blink
