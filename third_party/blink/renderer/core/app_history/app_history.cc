// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/app_history/app_history.h"

#include <memory>

#include "base/check_op.h"
#include "third_party/blink/public/web/web_frame_load_type.h"
#include "third_party/blink/renderer/bindings/core/v8/script_function.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_app_history_current_change_event_init.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_app_history_navigate_event_init.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_app_history_navigate_options.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_app_history_reload_options.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_app_history_result.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_app_history_transition.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_app_history_update_current_options.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/core/app_history/app_history_api_navigation.h"
#include "third_party/blink/renderer/core/app_history/app_history_current_change_event.h"
#include "third_party/blink/renderer/core/app_history/app_history_destination.h"
#include "third_party/blink/renderer/core/app_history/app_history_entry.h"
#include "third_party/blink/renderer/core/app_history/app_history_navigate_event.h"
#include "third_party/blink/renderer/core/app_history/app_history_transition.h"
#include "third_party/blink/renderer/core/dom/abort_signal.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/events/error_event.h"
#include "third_party/blink/renderer/core/frame/history_util.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/html/forms/form_data.h"
#include "third_party/blink/renderer/core/html/forms/html_form_element.h"
#include "third_party/blink/renderer/core/loader/document_loader.h"
#include "third_party/blink/renderer/core/loader/frame_load_request.h"
#include "third_party/blink/renderer/platform/bindings/exception_context.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/to_v8.h"
#include "third_party/blink/renderer/platform/wtf/uuid.h"

namespace blink {

class NavigateReaction final : public NewScriptFunction::Callable {
 public:
  enum class ResolveType { kFulfill, kReject };
  static void React(ScriptState* script_state,
                    ScriptPromise promise,
                    AppHistoryApiNavigation* navigation,
                    AppHistoryTransition* transition,
                    AbortSignal* signal) {
    promise.Then(MakeGarbageCollected<NewScriptFunction>(
                     script_state, MakeGarbageCollected<NavigateReaction>(
                                       navigation, transition, signal,
                                       ResolveType::kFulfill)),
                 MakeGarbageCollected<NewScriptFunction>(
                     script_state, MakeGarbageCollected<NavigateReaction>(
                                       navigation, transition, signal,
                                       ResolveType::kReject)));
  }

  NavigateReaction(AppHistoryApiNavigation* navigation,
                   AppHistoryTransition* transition,
                   AbortSignal* signal,
                   ResolveType type)
      : navigation_(navigation),
        transition_(transition),
        signal_(signal),
        type_(type) {}

  void Trace(Visitor* visitor) const final {
    NewScriptFunction::Callable::Trace(visitor);
    visitor->Trace(navigation_);
    visitor->Trace(transition_);
    visitor->Trace(signal_);
  }

  ScriptValue Call(ScriptState* script_state, ScriptValue value) final {
    auto* window = LocalDOMWindow::From(script_state);
    DCHECK(window);
    if (signal_->aborted()) {
      return ScriptValue();
    }

    AppHistory* app_history = AppHistory::appHistory(*window);
    app_history->ongoing_navigation_signal_ = nullptr;
    if (type_ == ResolveType::kFulfill) {
      if (navigation_) {
        navigation_->ResolveFinishedPromise();
      }
      app_history->DispatchEvent(
          *Event::Create(event_type_names::kNavigatesuccess));
    } else {
      app_history->RejectPromiseAndFireNavigateErrorEvent(navigation_, value);
    }

    if (app_history->transition() == transition_) {
      app_history->transition_ = nullptr;
    }

    return ScriptValue();
  }

 private:
  Member<AppHistoryApiNavigation> navigation_;
  Member<AppHistoryTransition> transition_;
  Member<AbortSignal> signal_;
  ResolveType type_;
};

template <typename... DOMExceptionArgs>
AppHistoryResult* EarlyErrorResult(ScriptState* script_state,
                                   DOMExceptionArgs&&... args) {
  auto* ex = MakeGarbageCollected<DOMException>(
      std::forward<DOMExceptionArgs>(args)...);
  return EarlyErrorResult(script_state, ex);
}

AppHistoryResult* EarlyErrorResult(ScriptState* script_state,
                                   DOMException* ex) {
  auto* result = AppHistoryResult::Create();
  result->setCommitted(ScriptPromise::RejectWithDOMException(script_state, ex));
  result->setFinished(ScriptPromise::RejectWithDOMException(script_state, ex));
  return result;
}

AppHistoryResult* EarlyErrorResult(ScriptState* script_state,
                                   v8::Local<v8::Value> ex) {
  auto* result = AppHistoryResult::Create();
  result->setCommitted(ScriptPromise::Reject(script_state, ex));
  result->setFinished(ScriptPromise::Reject(script_state, ex));
  return result;
}

AppHistoryResult* EarlySuccessResult(ScriptState* script_state,
                                     AppHistoryEntry* entry) {
  auto* result = AppHistoryResult::Create();
  result->setCommitted(
      ScriptPromise::Cast(script_state, ToV8(entry, script_state)));
  result->setFinished(
      ScriptPromise::Cast(script_state, ToV8(entry, script_state)));
  return result;
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

const char AppHistory::kSupplementName[] = "AppHistory";

AppHistory* AppHistory::appHistory(LocalDOMWindow& window) {
  return RuntimeEnabledFeatures::AppHistoryEnabled(&window) ? From(window)
                                                            : nullptr;
}

AppHistory* AppHistory::From(LocalDOMWindow& window) {
  auto* app_history = Supplement<LocalDOMWindow>::From<AppHistory>(window);
  if (!app_history) {
    app_history = MakeGarbageCollected<AppHistory>(window);
    Supplement<LocalDOMWindow>::ProvideTo(window, app_history);
  }
  return app_history;
}

AppHistory::AppHistory(LocalDOMWindow& window)
    : Supplement<LocalDOMWindow>(window) {}

void AppHistory::setOnnavigate(EventListener* listener) {
  UseCounter::Count(GetSupplementable(), WebFeature::kAppHistory);
  SetAttributeEventListener(event_type_names::kNavigate, listener);
}

void AppHistory::PopulateKeySet() {
  DCHECK(keys_to_indices_.IsEmpty());
  for (wtf_size_t i = 0; i < entries_.size(); i++)
    keys_to_indices_.insert(entries_[i]->key(), i);
}

void AppHistory::InitializeForNewWindow(
    HistoryItem& current,
    WebFrameLoadType load_type,
    CommitReason commit_reason,
    AppHistory* previous,
    const WebVector<WebHistoryItem>& back_entries,
    const WebVector<WebHistoryItem>& forward_entries) {
  DCHECK(entries_.IsEmpty());

  // This can happen even when commit_reason is not kInitialization, e.g. when
  // navigating from about:blank#1 to about:blank#2 where both are initial
  // about:blanks.
  if (HasEntriesAndEventsDisabled())
    return;

  // Under most circumstances, the browser process provides the information
  // need to initialize appHistory's entries array from
  // |app_history_back_entries_| and |app_history_forward_entries_|.
  // However, these are not available when the renderer handles the
  // navigation entirely, so in those cases (javascript: urls, XSLT commits,
  // and non-back/forward about:blank), copy the array from the previous
  // window and use the same update algorithm as same-document navigations.
  if (commit_reason != CommitReason::kRegular ||
      (current.Url() == BlankURL() && !IsBackForwardLoadType(load_type)) ||
      (current.Url().IsAboutSrcdocURL() && !IsBackForwardLoadType(load_type))) {
    if (previous && !previous->entries_.IsEmpty()) {
      CloneFromPrevious(*previous);
      UpdateForNavigation(current, load_type);
      return;
    }
  }

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
  // A same-document navigation (e.g., a document.open()) in a
  // |HasEntriesAndEventsDisabled()| situation will try to operate on an empty
  // |entries_|. appHistory considers this a no-op.
  if (entries_.IsEmpty())
    return;

  AppHistoryEntry* old_current = current();

  HeapVector<Member<AppHistoryEntry>> disposed_entries;
  if (type == WebFrameLoadType::kBackForward) {
    // If this is a same-document back/forward navigation, the new current
    // entry should already be present in entries_ and its key in
    // keys_to_indices_.
    DCHECK(keys_to_indices_.Contains(item.GetAppHistoryKey()));
    current_index_ = keys_to_indices_.at(item.GetAppHistoryKey());
  } else {
    if (type == WebFrameLoadType::kStandard) {
      // For a new back/forward entry, truncate any forward entries and prepare
      // to append.
      current_index_++;
      for (wtf_size_t i = current_index_; i < entries_.size(); i++) {
        keys_to_indices_.erase(entries_[i]->key());
        disposed_entries.push_back(entries_[i]);
      }
      entries_.resize(current_index_ + 1);
    } else if (type == WebFrameLoadType::kReplaceCurrentItem) {
      DCHECK_NE(current_index_, -1);
      disposed_entries.push_back(entries_[current_index_]);
    }

    // current_index_ is now correctly set (for type of
    // WebFrameLoadType::kReplaceCurrentItem/kReload/kReloadBypassingCache, it
    // didn't change). Create the new current entry.
    entries_[current_index_] =
        MakeGarbageCollected<AppHistoryEntry>(GetSupplementable(), &item);
    keys_to_indices_.insert(entries_[current_index_]->key(), current_index_);
  }

  auto* init = AppHistoryCurrentChangeEventInit::Create();
  init->setNavigationType(DetermineNavigationType(type));
  init->setFrom(old_current);
  DispatchEvent(*AppHistoryCurrentChangeEvent::Create(
      event_type_names::kCurrentchange, init));

  // It's important to do this before firing dispose events, since dispose
  // events could start another navigation or otherwise mess with
  // ongoing_navigation_.
  if (ongoing_navigation_) {
    ongoing_navigation_->NotifyAboutTheCommittedToEntry(
        entries_[current_index_]);
  }

  for (const auto& disposed_entry : disposed_entries) {
    disposed_entry->DispatchEvent(*Event::Create(event_type_names::kDispose));
  }
}

AppHistoryEntry* AppHistory::current() const {
  // current_index_ is initialized to -1 and set >= 0 when entries_ is
  // populated. It will still be negative if the appHistory of an initial empty
  // document or opaque-origin document is accessed.
  return !HasEntriesAndEventsDisabled() && current_index_ >= 0
             ? entries_[current_index_]
             : nullptr;
}

HeapVector<Member<AppHistoryEntry>> AppHistory::entries() {
  return HasEntriesAndEventsDisabled() ? HeapVector<Member<AppHistoryEntry>>()
                                       : entries_;
}

void AppHistory::updateCurrent(AppHistoryUpdateCurrentOptions* options,
                               ExceptionState& exception_state) {
  AppHistoryEntry* current_entry = current();

  if (!current_entry) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "updateCurrent() cannot be called when appHistory.current is null.");
    return;
  }

  scoped_refptr<SerializedScriptValue> serialized_state =
      SerializeState(options->state(), exception_state);
  if (exception_state.HadException())
    return;

  current_entry->GetItem()->SetAppHistoryState(std::move(serialized_state));

  auto* init = AppHistoryCurrentChangeEventInit::Create();
  init->setFrom(current_entry);
  DispatchEvent(*AppHistoryCurrentChangeEvent::Create(
      event_type_names::kCurrentchange, init));
}

AppHistoryResult* AppHistory::navigate(ScriptState* script_state,
                                       const String& url,
                                       AppHistoryNavigateOptions* options) {
  KURL completed_url(GetSupplementable()->Url(), url);
  if (!completed_url.IsValid()) {
    return EarlyErrorResult(script_state, DOMExceptionCode::kSyntaxError,
                            "Invalid URL '" + completed_url.GetString() + "'.");
  }

  if (DOMException* maybe_ex = PerformSharedNavigationChecks("navigate()"))
    return EarlyErrorResult(script_state, maybe_ex);

  scoped_refptr<SerializedScriptValue> serialized_state = nullptr;
  {
    if (options->hasState()) {
      ExceptionState exception_state(
          script_state->GetIsolate(),
          ExceptionContext::Context::kOperationInvoke, "AppHistory",
          "navigate");
      serialized_state = SerializeState(options->state(), exception_state);
      if (exception_state.HadException()) {
        AppHistoryResult* result =
            EarlyErrorResult(script_state, exception_state.GetException());
        exception_state.ClearException();
        return result;
      }
    }
  }

  WebFrameLoadType frame_load_type = options->replace()
                                         ? WebFrameLoadType::kReplaceCurrentItem
                                         : WebFrameLoadType::kStandard;

  return PerformNonTraverseNavigation(script_state, completed_url,
                                      std::move(serialized_state), options,
                                      frame_load_type);
}

AppHistoryResult* AppHistory::reload(ScriptState* script_state,
                                     AppHistoryReloadOptions* options) {
  if (DOMException* maybe_ex = PerformSharedNavigationChecks("reload()"))
    return EarlyErrorResult(script_state, maybe_ex);

  scoped_refptr<SerializedScriptValue> serialized_state = nullptr;
  {
    if (options->hasState()) {
      ExceptionState exception_state(
          script_state->GetIsolate(),
          ExceptionContext::Context::kOperationInvoke, "AppHistory", "reload");
      serialized_state = SerializeState(options->state(), exception_state);
      if (exception_state.HadException()) {
        AppHistoryResult* result =
            EarlyErrorResult(script_state, exception_state.GetException());
        exception_state.ClearException();
        return result;
      }
    } else if (AppHistoryEntry* current_entry = current()) {
      serialized_state = current_entry->GetItem()->GetAppHistoryState();
    }
  }

  return PerformNonTraverseNavigation(script_state, GetSupplementable()->Url(),
                                      std::move(serialized_state), options,
                                      WebFrameLoadType::kReload);
}

AppHistoryResult* AppHistory::PerformNonTraverseNavigation(
    ScriptState* script_state,
    const KURL& url,
    scoped_refptr<SerializedScriptValue> serialized_state,
    AppHistoryNavigationOptions* options,
    WebFrameLoadType frame_load_type) {
  DCHECK(frame_load_type == WebFrameLoadType::kReplaceCurrentItem ||
         frame_load_type == WebFrameLoadType::kReload ||
         frame_load_type == WebFrameLoadType::kStandard);

  AppHistoryApiNavigation* navigation =
      MakeGarbageCollected<AppHistoryApiNavigation>(
          script_state, this, options, String(), std::move(serialized_state));
  upcoming_non_traversal_navigation_ = navigation;

  GetSupplementable()->GetFrame()->MaybeLogAdClickNavigation();

  FrameLoadRequest request(GetSupplementable(), ResourceRequest(url));
  request.SetClientRedirectReason(ClientNavigationReason::kFrameNavigation);
  GetSupplementable()->GetFrame()->Navigate(request, frame_load_type);

  // DispatchNavigateEvent() will clear upcoming_non_traversal_navigation_ if we
  // get that far. If the navigation is blocked before DispatchNavigateEvent()
  // is called, reject the promise and cleanup here.
  if (upcoming_non_traversal_navigation_ == navigation) {
    upcoming_non_traversal_navigation_ = nullptr;
    return EarlyErrorResult(script_state, DOMExceptionCode::kAbortError,
                            "Navigation was aborted");
  }

  if (SerializedScriptValue* state = navigation->TakeSerializedState()) {
    current()->GetItem()->SetAppHistoryState(state);
  }
  return navigation->GetAppHistoryResult();
}

AppHistoryResult* AppHistory::goTo(ScriptState* script_state,
                                   const String& key,
                                   AppHistoryNavigationOptions* options) {
  if (DOMException* maybe_ex =
          PerformSharedNavigationChecks("goTo()/back()/forward()")) {
    return EarlyErrorResult(script_state, maybe_ex);
  }

  if (!keys_to_indices_.Contains(key)) {
    return EarlyErrorResult(script_state, DOMExceptionCode::kInvalidStateError,
                            "Invalid key");
  }
  if (key == current()->key()) {
    return EarlySuccessResult(script_state, current());
  }

  auto previous_navigation = upcoming_traversals_.find(key);
  if (previous_navigation != upcoming_traversals_.end())
    return previous_navigation->value->GetAppHistoryResult();

  AppHistoryApiNavigation* ongoing_navigation =
      MakeGarbageCollected<AppHistoryApiNavigation>(script_state, this, options,
                                                    key);
  upcoming_traversals_.insert(key, ongoing_navigation);
  GetSupplementable()
      ->GetFrame()
      ->GetLocalFrameHostRemote()
      .NavigateToAppHistoryKey(key, LocalFrame::HasTransientUserActivation(
                                        GetSupplementable()->GetFrame()));
  return ongoing_navigation->GetAppHistoryResult();
}

bool AppHistory::canGoBack() const {
  return !HasEntriesAndEventsDisabled() && current_index_ > 0;
}

bool AppHistory::canGoForward() const {
  return !HasEntriesAndEventsDisabled() && current_index_ != -1 &&
         static_cast<size_t>(current_index_) < entries_.size() - 1;
}

AppHistoryResult* AppHistory::back(ScriptState* script_state,
                                   AppHistoryNavigationOptions* options) {
  if (!canGoBack()) {
    return EarlyErrorResult(script_state, DOMExceptionCode::kInvalidStateError,
                            "Cannot go back");
  }
  return goTo(script_state, entries_[current_index_ - 1]->key(), options);
}

AppHistoryResult* AppHistory::forward(ScriptState* script_state,
                                      AppHistoryNavigationOptions* options) {
  if (!canGoForward()) {
    return EarlyErrorResult(script_state, DOMExceptionCode::kInvalidStateError,
                            "Cannot go forward");
  }
  return goTo(script_state, entries_[current_index_ + 1]->key(), options);
}

DOMException* AppHistory::PerformSharedNavigationChecks(
    const String& method_name_for_error_message) {
  if (!GetSupplementable()->GetFrame()) {
    return MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kInvalidStateError,
        method_name_for_error_message +
            " cannot be called when the Window is detached.");
  }
  if (GetSupplementable()->document()->PageDismissalEventBeingDispatched()) {
    return MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kInvalidStateError,
        method_name_for_error_message +
            " cannot be called during unload or beforeunload.");
  }
  return nullptr;
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

void AppHistory::PromoteUpcomingNavigationToOngoing(const String& key) {
  DCHECK(!ongoing_navigation_);
  if (!key.IsNull()) {
    DCHECK(!upcoming_non_traversal_navigation_);
    auto iter = upcoming_traversals_.find(key);
    if (iter != upcoming_traversals_.end()) {
      ongoing_navigation_ = iter->value;
      upcoming_traversals_.erase(iter);
    }
  } else {
    ongoing_navigation_ = upcoming_non_traversal_navigation_.Release();
  }
}

bool AppHistory::HasEntriesAndEventsDisabled() const {
  auto* frame = GetSupplementable()->GetFrame();
  return !frame ||
         !GetSupplementable()
              ->GetFrame()
              ->Loader()
              .HasLoadedNonInitialEmptyDocument() ||
         GetSupplementable()->GetSecurityOrigin()->IsOpaque();
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

  const KURL& current_url = GetSupplementable()->Url();
  const String& key =
      destination_item ? destination_item->GetAppHistoryKey() : String();
  PromoteUpcomingNavigationToOngoing(key);

  if (HasEntriesAndEventsDisabled()) {
    if (ongoing_navigation_) {
      CleanupApiNavigation(*ongoing_navigation_);
    }
    return DispatchResult::kContinue;
  }

  auto* script_state =
      ToScriptStateForMainWorld(GetSupplementable()->GetFrame());
  ScriptState::Scope scope(script_state);

  if (type == WebFrameLoadType::kBackForward &&
      event_type == NavigateEventType::kFragment &&
      !keys_to_indices_.Contains(key)) {
    // This same document history traversal was preempted by another navigation
    // that removed this entry from the back/forward list. Proceeding will leave
    // entries_ out of sync with the browser process.
    FinalizeWithAbortedNavigationError(script_state, ongoing_navigation_);
    return DispatchResult::kAbort;
  }

  auto* init = AppHistoryNavigateEventInit::Create();
  const String& navigation_type = DetermineNavigationType(type);
  init->setNavigationType(navigation_type);

  SerializedScriptValue* destination_state = nullptr;
  if (destination_item)
    destination_state = destination_item->GetAppHistoryState();
  else if (ongoing_navigation_)
    destination_state = ongoing_navigation_->GetSerializedState();
  AppHistoryDestination* destination =
      MakeGarbageCollected<AppHistoryDestination>(
          url, event_type != NavigateEventType::kCrossDocument,
          destination_state);
  if (type == WebFrameLoadType::kBackForward) {
    auto iter = keys_to_indices_.find(key);
    int index = iter == keys_to_indices_.end() ? 0 : iter->value;
    destination->SetTraverseProperties(key, destination_item->GetAppHistoryId(),
                                       index);
  }
  init->setDestination(destination);

  init->setCancelable(type != WebFrameLoadType::kBackForward);
  init->setCanTransition(
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
  if (ongoing_navigation_)
    init->setInfo(ongoing_navigation_->GetInfo());
  init->setSignal(MakeGarbageCollected<AbortSignal>(GetSupplementable()));
  auto* navigate_event = AppHistoryNavigateEvent::Create(
      GetSupplementable(), event_type_names::kNavigate, init);
  navigate_event->SetUrl(url);

  DCHECK(!ongoing_navigate_event_);
  DCHECK(!ongoing_navigation_signal_);
  ongoing_navigate_event_ = navigate_event;
  ongoing_navigation_signal_ = navigate_event->signal();
  DispatchEvent(*navigate_event);
  ongoing_navigate_event_ = nullptr;

  if (navigate_event->defaultPrevented()) {
    if (!navigate_event->signal()->aborted())
      FinalizeWithAbortedNavigationError(script_state, ongoing_navigation_);
    return DispatchResult::kAbort;
  }

  auto promise_list = navigate_event->GetNavigationActionPromisesList();
  if (!promise_list.IsEmpty()) {
    transition_ =
        MakeGarbageCollected<AppHistoryTransition>(navigation_type, current());

    // The spec says that at this point we should either run the URL and history
    // update steps (for non-traverse cases) or we should do a same-document
    // history traversal. In our implementation it's easier for the caller to do
    // a history traversal since it has access to all the info it needs.
    // TODO(japhet): Figure out how cross-document back-forward should work.
    if (type != WebFrameLoadType::kBackForward) {
      GetSupplementable()->document()->Loader()->RunURLAndHistoryUpdateSteps(
          url,
          mojom::blink::SameDocumentNavigationType::kAppHistoryTransitionWhile,
          state_object, type);
    }
  }

  if (!promise_list.IsEmpty() ||
      event_type != NavigateEventType::kCrossDocument) {
    NavigateReaction::React(
        script_state, ScriptPromise::All(script_state, promise_list),
        ongoing_navigation_, transition_, navigate_event->signal());
  } else if (ongoing_navigation_) {
    // The spec assumes it's ok to leave a promise permanently unresolved, but
    // ScriptPromiseResolver requires either resolution or explicit detach.
    ongoing_navigation_->CleanupForCrossDocument();
  }

  return promise_list.IsEmpty() ? DispatchResult::kContinue
                                : DispatchResult::kTransitionWhile;
}

void AppHistory::InformAboutCanceledNavigation() {
  if (ongoing_navigation_signal_) {
    auto* script_state =
        ToScriptStateForMainWorld(GetSupplementable()->GetFrame());
    ScriptState::Scope scope(script_state);
    FinalizeWithAbortedNavigationError(script_state, ongoing_navigation_);
  }

  // If this function is being called as part of frame detach, also cleanup any
  // upcoming_traversals_.
  //
  // This function may be called when a v8 context hasn't been initialized.
  // upcoming_traversals_ being non-empty requires a v8 context, so check that
  // so that we don't unnecessarily try to initialize one below.
  if (!upcoming_traversals_.IsEmpty() && GetSupplementable()->GetFrame() &&
      !GetSupplementable()->GetFrame()->IsAttached()) {
    auto* script_state =
        ToScriptStateForMainWorld(GetSupplementable()->GetFrame());
    ScriptState::Scope scope(script_state);

    HeapVector<Member<AppHistoryApiNavigation>> traversals;
    CopyValuesToVector(upcoming_traversals_, traversals);
    for (auto& traversal : traversals)
      FinalizeWithAbortedNavigationError(script_state, traversal);
    DCHECK(upcoming_traversals_.IsEmpty());
  }
}

void AppHistory::RejectPromiseAndFireNavigateErrorEvent(
    AppHistoryApiNavigation* navigation,
    ScriptValue value) {
  if (navigation)
    navigation->RejectFinishedPromise(value);

  auto* isolate = GetSupplementable()->GetIsolate();
  v8::Local<v8::Message> message =
      v8::Exception::CreateMessage(isolate, value.V8Value());
  std::unique_ptr<SourceLocation> location =
      SourceLocation::FromMessage(isolate, message, GetSupplementable());
  ErrorEvent* event = ErrorEvent::Create(
      ToCoreStringWithNullCheck(message->Get()), std::move(location), value,
      &DOMWrapperWorld::MainWorld());
  event->SetType(event_type_names::kNavigateerror);
  DispatchEvent(*event);
}

void AppHistory::CleanupApiNavigation(AppHistoryApiNavigation& navigation) {
  if (&navigation == ongoing_navigation_) {
    ongoing_navigation_ = nullptr;
  } else {
    DCHECK(!navigation.GetKey().IsNull());
    DCHECK(upcoming_traversals_.Contains(navigation.GetKey()));
    upcoming_traversals_.erase(navigation.GetKey());
  }
}

void AppHistory::FinalizeWithAbortedNavigationError(
    ScriptState* script_state,
    AppHistoryApiNavigation* navigation) {
  if (ongoing_navigate_event_) {
    ongoing_navigate_event_->preventDefault();
    ongoing_navigate_event_ = nullptr;
  }
  if (ongoing_navigation_signal_) {
    ongoing_navigation_signal_->SignalAbort(script_state);
    ongoing_navigation_signal_ = nullptr;
  }

  RejectPromiseAndFireNavigateErrorEvent(
      navigation,
      ScriptValue::From(script_state, MakeGarbageCollected<DOMException>(
                                          DOMExceptionCode::kAbortError,
                                          "Navigation was aborted")));
  transition_ = nullptr;
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
  visitor->Trace(transition_);
  visitor->Trace(ongoing_navigation_);
  visitor->Trace(upcoming_traversals_);
  visitor->Trace(upcoming_non_traversal_navigation_);
  visitor->Trace(ongoing_navigate_event_);
  visitor->Trace(ongoing_navigation_signal_);
}

}  // namespace blink
