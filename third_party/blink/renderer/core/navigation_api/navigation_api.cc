// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/navigation_api/navigation_api.h"

#include <memory>

#include "base/check_op.h"
#include "third_party/blink/public/mojom/frame/frame.mojom-blink.h"
#include "third_party/blink/public/web/web_frame_load_type.h"
#include "third_party/blink/renderer/bindings/core/v8/capture_source_location.h"
#include "third_party/blink/renderer/bindings/core/v8/script_function.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_navigate_event_init.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_navigation_current_entry_change_event_init.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_navigation_history_behavior.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_navigation_navigate_options.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_navigation_reload_options.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_navigation_result.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_navigation_transition.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_navigation_update_current_entry_options.h"
#include "third_party/blink/renderer/core/accessibility/ax_object_cache.h"
#include "third_party/blink/renderer/core/dom/abort_signal.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/event_target_names.h"
#include "third_party/blink/renderer/core/events/error_event.h"
#include "third_party/blink/renderer/core/frame/history_util.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/html/forms/form_data.h"
#include "third_party/blink/renderer/core/html/forms/html_form_element.h"
#include "third_party/blink/renderer/core/loader/document_loader.h"
#include "third_party/blink/renderer/core/loader/frame_load_request.h"
#include "third_party/blink/renderer/core/navigation_api/navigate_event.h"
#include "third_party/blink/renderer/core/navigation_api/navigation_api_navigation.h"
#include "third_party/blink/renderer/core/navigation_api/navigation_current_entry_change_event.h"
#include "third_party/blink/renderer/core/navigation_api/navigation_destination.h"
#include "third_party/blink/renderer/core/navigation_api/navigation_history_entry.h"
#include "third_party/blink/renderer/core/navigation_api/navigation_transition.h"
#include "third_party/blink/renderer/core/timing/soft_navigation_heuristics.h"
#include "third_party/blink/renderer/platform/bindings/exception_context.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/to_v8.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread_scheduler.h"
#include "third_party/blink/renderer/platform/wtf/uuid.h"

namespace blink {

class NavigateReaction final : public ScriptFunction::Callable {
 public:
  enum class ResolveType { kFulfill, kReject };
  enum class ReactType { kImmediate, kIntercept };
  static void React(ScriptState* script_state,
                    ScriptPromise promise,
                    NavigationApiNavigation* navigation,
                    NavigateEvent* navigate_event,
                    ReactType react_type) {
    promise.Then(MakeGarbageCollected<ScriptFunction>(
                     script_state, MakeGarbageCollected<NavigateReaction>(
                                       navigation, navigate_event,
                                       ResolveType::kFulfill, react_type)),
                 MakeGarbageCollected<ScriptFunction>(
                     script_state, MakeGarbageCollected<NavigateReaction>(
                                       navigation, navigate_event,
                                       ResolveType::kReject, react_type)));
    if (navigate_event->ShouldSendAxEvents()) {
      auto* window = LocalDOMWindow::From(script_state);
      DCHECK(window);
      if (AXObjectCache* cache = window->document()->ExistingAXObjectCache())
        cache->HandleLoadStart(window->document());
    }
  }

  NavigateReaction(NavigationApiNavigation* navigation,
                   NavigateEvent* navigate_event,
                   ResolveType resolve_type,
                   ReactType react_type)
      : navigation_(navigation),
        navigate_event_(navigate_event),
        resolve_type_(resolve_type),
        react_type_(react_type) {}

  void Trace(Visitor* visitor) const final {
    ScriptFunction::Callable::Trace(visitor);
    visitor->Trace(navigation_);
    visitor->Trace(navigate_event_);
  }

  ScriptValue Call(ScriptState* script_state, ScriptValue value) final {
    auto* window = LocalDOMWindow::From(script_state);
    DCHECK(window);
    if (navigate_event_->signal()->aborted()) {
      return ScriptValue();
    }

    NavigationApi* navigation_api = window->navigation();
    navigation_api->ongoing_navigate_event_ = nullptr;

    if (resolve_type_ == ResolveType::kFulfill) {
      if (react_type_ == ReactType::kIntercept)
        navigate_event_->PotentiallyProcessScrollBehavior();
      navigation_api->ResolvePromisesAndFireNavigateSuccessEvent(navigation_);
    } else {
      navigation_api->RejectPromisesAndFireNavigateErrorEvent(navigation_,
                                                              value);
    }

    navigate_event_->ResetFocusIfNeeded();

    if (react_type_ == ReactType::kIntercept && window->GetFrame()) {
      window->GetFrame()->Loader().DidFinishNavigation(
          resolve_type_ == ResolveType::kFulfill
              ? FrameLoader::NavigationFinishState::kSuccess
              : FrameLoader::NavigationFinishState::kFailure);
    }

    if (navigate_event_->ShouldSendAxEvents()) {
      window = LocalDOMWindow::From(script_state);
      DCHECK(window);
      if (AXObjectCache* cache = window->document()->ExistingAXObjectCache())
        cache->HandleLoadComplete(window->document());
    }

    return ScriptValue();
  }

 private:
  Member<NavigationApiNavigation> navigation_;
  Member<NavigateEvent> navigate_event_;
  ResolveType resolve_type_;
  ReactType react_type_;
};

template <typename... DOMExceptionArgs>
NavigationResult* EarlyErrorResult(ScriptState* script_state,
                                   DOMExceptionArgs&&... args) {
  auto* ex = MakeGarbageCollected<DOMException>(
      std::forward<DOMExceptionArgs>(args)...);
  return EarlyErrorResult(script_state, ex);
}

NavigationResult* EarlyErrorResult(ScriptState* script_state,
                                   DOMException* ex) {
  auto* result = NavigationResult::Create();
  result->setCommitted(ScriptPromise::RejectWithDOMException(script_state, ex));
  result->setFinished(ScriptPromise::RejectWithDOMException(script_state, ex));
  return result;
}

NavigationResult* EarlyErrorResult(ScriptState* script_state,
                                   v8::Local<v8::Value> ex) {
  auto* result = NavigationResult::Create();
  result->setCommitted(ScriptPromise::Reject(script_state, ex));
  result->setFinished(ScriptPromise::Reject(script_state, ex));
  return result;
}

NavigationResult* EarlySuccessResult(ScriptState* script_state,
                                     NavigationHistoryEntry* entry) {
  auto* result = NavigationResult::Create();
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

NavigationApi::NavigationApi(LocalDOMWindow* window)
    : ExecutionContextLifecycleObserver(window), window_(window) {}

void NavigationApi::setOnnavigate(EventListener* listener) {
  UseCounter::Count(window_, WebFeature::kAppHistory);
  SetAttributeEventListener(event_type_names::kNavigate, listener);
}

void NavigationApi::PopulateKeySet() {
  DCHECK(keys_to_indices_.empty());
  for (wtf_size_t i = 0; i < entries_.size(); i++)
    keys_to_indices_.insert(entries_[i]->key(), i);
}

void NavigationApi::InitializeForNewWindow(
    HistoryItem& current,
    WebFrameLoadType load_type,
    CommitReason commit_reason,
    NavigationApi* previous,
    const WebVector<WebHistoryItem>& back_entries,
    const WebVector<WebHistoryItem>& forward_entries) {
  DCHECK(entries_.empty());

  // This can happen even when commit_reason is not kInitialization, e.g. when
  // navigating from about:blank#1 to about:blank#2 where both are initial
  // about:blanks.
  if (HasEntriesAndEventsDisabled())
    return;

  // Under most circumstances, the browser process provides the information
  // need to initialize the navigation API's entries array from |back_entries|
  // and |forward_entries|. However, these are not available when the renderer
  // handles the navigation entirely, so in those cases (javascript: urls, XSLT
  // commits, and non-back/forward about:blank), copy the array from the
  // previous window and use the same update algorithm as same-document
  // navigations.
  if (commit_reason != CommitReason::kRegular ||
      (current.Url() == BlankURL() && !IsBackForwardLoadType(load_type)) ||
      (current.Url().IsAboutSrcdocURL() && !IsBackForwardLoadType(load_type))) {
    if (previous && !previous->entries_.empty() &&
        window_->GetSecurityOrigin()->IsSameOriginWith(
            previous->window_->GetSecurityOrigin())) {
      DCHECK(entries_.empty());
      entries_.reserve(previous->entries_.size());
      for (wtf_size_t i = 0; i < previous->entries_.size(); i++)
        entries_.emplace_back(previous->entries_[i]->Clone(window_));
      current_entry_index_ = previous->current_entry_index_;
      PopulateKeySet();
      UpdateForNavigation(current, load_type);
      return;
    }
  }

  // Construct |entries_|. Any back entries are inserted, then the current
  // entry, then any forward entries.
  entries_.reserve(base::checked_cast<wtf_size_t>(back_entries.size() +
                                                  forward_entries.size() + 1));
  for (const auto& entry : back_entries)
    entries_.emplace_back(MakeEntryFromItem(*entry));

  current_entry_index_ = base::checked_cast<wtf_size_t>(back_entries.size());
  entries_.emplace_back(MakeEntryFromItem(current));

  for (const auto& entry : forward_entries)
    entries_.emplace_back(MakeEntryFromItem(*entry));
  PopulateKeySet();
}

void NavigationApi::UpdateForNavigation(HistoryItem& item,
                                        WebFrameLoadType type) {
  // A same-document navigation (e.g., a document.open()) in a
  // |HasEntriesAndEventsDisabled()| situation will try to operate on an empty
  // |entries_|. The navigation API considers this a no-op.
  if (HasEntriesAndEventsDisabled())
    return;

  NavigationHistoryEntry* old_current = currentEntry();

  HeapVector<Member<NavigationHistoryEntry>> disposed_entries;
  if (type == WebFrameLoadType::kBackForward) {
    // If this is a same-document back/forward navigation, the new current
    // entry should already be present in entries_ and its key in
    // keys_to_indices_.
    DCHECK(keys_to_indices_.Contains(item.GetNavigationApiKey()));
    current_entry_index_ = keys_to_indices_.at(item.GetNavigationApiKey());
  } else if (type == WebFrameLoadType::kStandard) {
    // For a new back/forward entry, truncate any forward entries and prepare
    // to append.
    current_entry_index_++;
    for (wtf_size_t i = current_entry_index_; i < entries_.size(); i++) {
      keys_to_indices_.erase(entries_[i]->key());
      disposed_entries.push_back(entries_[i]);
    }
    entries_.resize(current_entry_index_ + 1);
  } else if (type == WebFrameLoadType::kReplaceCurrentItem) {
    DCHECK_NE(current_entry_index_, -1);
    disposed_entries.push_back(entries_[current_entry_index_]);
  }

  if (type == WebFrameLoadType::kStandard ||
      type == WebFrameLoadType::kReplaceCurrentItem) {
    // current_index_ is now correctly set (for type of
    // WebFrameLoadType::kReplaceCurrentItem, it didn't change). Create the new
    // current entry.
    entries_[current_entry_index_] = MakeEntryFromItem(item);
    keys_to_indices_.insert(entries_[current_entry_index_]->key(),
                            current_entry_index_);
  }

  // Note how reload types don't update the current entry or dispose any
  // entries.

  // It's important to do this before firing dispose events, since
  // currententrychange or dispose events below could start another navigation
  // or otherwise mess with ongoing_navigation_. In that case, waiting to call
  // NotifyAboutTheCommittedToEntry() leads to the committed promise rejecting,
  // even though we have already committed and the promise should definitely
  // fulfill.
  if (ongoing_navigation_) {
    ongoing_navigation_->NotifyAboutTheCommittedToEntry(
        entries_[current_entry_index_], type);
  }

  NavigateEvent* ongoing_navigate_event = ongoing_navigate_event_;

  // Entering a MicrotasksScope here allows us to defer microtasks from running
  // immediately after the currententrychange and dispose events if there is an
  // event listener for any of those events. This ensures a stable
  // relative ordering of the navigateResult.committed promise (fulfilled in
  // NotifyAboutTheCommittedToEntry() above) and any intercept() handlers (run
  // in FinalizeNavigationActionPromisesList() below). intercept() handlers must
  // execute first.
  // Without the microtasks scope deferring promise continuations, the order
  // inverts when committing a browser-initiated same-document navigation and
  // an event listener is present for either currententrychange or dispose.
  v8::MicrotasksScope scope(window_->GetIsolate(), ToMicrotaskQueue(window_),
                            v8::MicrotasksScope::kRunMicrotasks);

  auto* init = NavigationCurrentEntryChangeEventInit::Create();
  init->setNavigationType(DetermineNavigationType(type));
  init->setFrom(old_current);
  DispatchEvent(*NavigationCurrentEntryChangeEvent::Create(
      event_type_names::kCurrententrychange, init));

  if (ongoing_navigate_event)
    ongoing_navigate_event->FinalizeNavigationActionPromisesList();

  for (const auto& disposed_entry : disposed_entries) {
    disposed_entry->DispatchEvent(*Event::Create(event_type_names::kDispose));
  }
}

NavigationHistoryEntry* NavigationApi::GetEntryForRestore(
    const mojom::blink::NavigationApiHistoryEntryPtr& entry) {
  const auto& it = keys_to_indices_.find(entry->key);
  if (it != keys_to_indices_.end()) {
    NavigationHistoryEntry* existing_entry = entries_[it->value];
    if (existing_entry->id() == entry->id)
      return existing_entry;
  }
  return MakeGarbageCollected<NavigationHistoryEntry>(
      window_, entry->key, entry->id, KURL(entry->url),
      entry->document_sequence_number,
      entry->state ? SerializedScriptValue::Create(entry->state) : nullptr);
}

// static
void FireDisposeEventsAsync(
    HeapVector<Member<NavigationHistoryEntry>>* disposed_entries) {
  for (const auto& entry : *disposed_entries) {
    entry->DispatchEvent(*Event::Create(event_type_names::kDispose));
  }
}

void NavigationApi::SetEntriesForRestore(
    const mojom::blink::NavigationApiHistoryEntryArraysPtr& entry_arrays) {
  // If this window HasEntriesAndEventsDisabled(), we shouldn't attempt to
  // restore anything.
  if (HasEntriesAndEventsDisabled())
    return;

  HeapVector<Member<NavigationHistoryEntry>> new_entries;
  new_entries.reserve(
      base::checked_cast<wtf_size_t>(entry_arrays->back_entries.size() +
                                     entry_arrays->forward_entries.size() + 1));
  for (const auto& item : entry_arrays->back_entries)
    new_entries.emplace_back(GetEntryForRestore(item));
  new_entries.emplace_back(currentEntry());
  for (const auto& item : entry_arrays->forward_entries)
    new_entries.emplace_back(GetEntryForRestore(item));

  new_entries.swap(entries_);
  current_entry_index_ =
      base::checked_cast<wtf_size_t>(entry_arrays->back_entries.size());
  keys_to_indices_.clear();
  PopulateKeySet();

  // |new_entries| now contains the previous entries_. Find the ones that are no
  // longer in entries_ so they can be disposed.
  HeapVector<Member<NavigationHistoryEntry>>* disposed_entries =
      MakeGarbageCollected<HeapVector<Member<NavigationHistoryEntry>>>();
  for (const auto& entry : new_entries) {
    const auto& it = keys_to_indices_.find(entry->key());
    if (it == keys_to_indices_.end() || entries_[it->value] != entry)
      disposed_entries->push_back(entry);
  }
  window_->GetTaskRunner(TaskType::kInternalDefault)
      ->PostTask(FROM_HERE, WTF::BindOnce(&FireDisposeEventsAsync,
                                          WrapPersistent(disposed_entries)));
}

void NavigationApi::DisposeEntriesForSessionHistoryRemoval(
    const Vector<String>& keys) {
  if (HasEntriesAndEventsDisabled())
    return;

  HeapHashSet<Member<NavigationHistoryEntry>> disposed_entries;
  for (const String& key : keys) {
    auto it = keys_to_indices_.find(key);
    // |key| may have already been disposed in UpdateForNavigation() if the
    // entry was removed due to a navigation in this frame.
    // The browser process may give us the key for currentEntry() in certain
    // situations (e.g., if this is an iframe that was added after a push, and
    // we navigate back past the creation of the iframe, currentEntry()'s key
    // will no longer be present in the session history). Don't ever dispose the
    // currentEntry().
    if (it != keys_to_indices_.end() && entries_[it->value] != currentEntry())
      disposed_entries.insert(entries_[it->value]);
  }

  HeapVector<Member<NavigationHistoryEntry>> entries_after_dispose;
  for (auto& entry : entries_) {
    if (!disposed_entries.Contains(entry))
      entries_after_dispose.push_back(entry);
  }

  String current_entry_key = currentEntry()->key();
  entries_.swap(entries_after_dispose);
  keys_to_indices_.clear();
  PopulateKeySet();
  current_entry_index_ = keys_to_indices_.at(current_entry_key);

  for (const auto& disposed_entry : disposed_entries)
    disposed_entry->DispatchEvent(*Event::Create(event_type_names::kDispose));
}

NavigationHistoryEntry* NavigationApi::currentEntry() const {
  // current_index_ is initialized to -1 and set >= 0 when entries_ is
  // populated. It will still be negative if the navigation object of an initial
  // empty document or opaque-origin document is accessed.
  return !HasEntriesAndEventsDisabled() && current_entry_index_ >= 0
             ? entries_[current_entry_index_]
             : nullptr;
}

HeapVector<Member<NavigationHistoryEntry>> NavigationApi::entries() {
  return HasEntriesAndEventsDisabled()
             ? HeapVector<Member<NavigationHistoryEntry>>()
             : entries_;
}

void NavigationApi::updateCurrentEntry(
    NavigationUpdateCurrentEntryOptions* options,
    ExceptionState& exception_state) {
  NavigationHistoryEntry* current_entry = currentEntry();

  if (!current_entry) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "updateCurrent() cannot be called when navigation.current is null.");
    return;
  }

  scoped_refptr<SerializedScriptValue> serialized_state =
      SerializeState(options->state(), exception_state);
  if (exception_state.HadException())
    return;

  current_entry->SetAndSaveState(std::move(serialized_state));

  auto* init = NavigationCurrentEntryChangeEventInit::Create();
  init->setFrom(current_entry);
  DispatchEvent(*NavigationCurrentEntryChangeEvent::Create(
      event_type_names::kCurrententrychange, init));
}

NavigationResult* NavigationApi::navigate(ScriptState* script_state,
                                          const String& url,
                                          NavigationNavigateOptions* options) {
  KURL completed_url = window_->CompleteURL(url);
  if (!completed_url.IsValid()) {
    return EarlyErrorResult(script_state, DOMExceptionCode::kSyntaxError,
                            "Invalid URL '" + completed_url.GetString() + "'.");
  }

  scoped_refptr<SerializedScriptValue> serialized_state = nullptr;
  {
    if (options->hasState()) {
      ExceptionState exception_state(
          script_state->GetIsolate(),
          ExceptionContext::Context::kOperationInvoke, "Navigation",
          "navigate");
      serialized_state = SerializeState(options->state(), exception_state);
      if (exception_state.HadException()) {
        NavigationResult* result =
            EarlyErrorResult(script_state, exception_state.GetException());
        exception_state.ClearException();
        return result;
      }
    }
  }

  FrameLoadRequest request(window_, ResourceRequest(completed_url));
  request.SetClientRedirectReason(ClientNavigationReason::kFrameNavigation);

  if (options->history() == V8NavigationHistoryBehavior::Enum::kPush) {
    LocalFrame* frame = window_->GetFrame();

    if (frame->Loader().IsOnInitialEmptyDocument()) {
      return EarlyErrorResult(
          script_state, DOMExceptionCode::kNotSupportedError,
          "A \"push\" navigation was explicitly requested, but only a "
          "\"replace\" navigation is possible while on the initial about:blank "
          "document.");
    }

    if (completed_url.ProtocolIsJavaScript()) {
      return EarlyErrorResult(
          script_state, DOMExceptionCode::kNotSupportedError,
          "A \"push\" navigation was explicitly requested, but only a "
          "\"replace\" navigation is possible when navigating to a javascript: "
          "URL.");
    }

    if (frame->ShouldMaintainTrivialSessionHistory()) {
      return EarlyErrorResult(
          script_state, DOMExceptionCode::kNotSupportedError,
          "A \"push\" navigation was explicitly requested, but only a "
          "\"replace\" navigation is possible when navigating in a trivial "
          "session history context, which maintains only one session history "
          "entry.");
    }

    request.SetForceHistoryPush();
  }

  // The spec also converts "auto" to "replace" here if the document is not
  // completely loaded. We let that happen later in the navigation pipeline.
  WebFrameLoadType frame_load_type =
      options->history() == V8NavigationHistoryBehavior::Enum::kReplace
          ? WebFrameLoadType::kReplaceCurrentItem
          : WebFrameLoadType::kStandard;

  return PerformNonTraverseNavigation(script_state, request,
                                      std::move(serialized_state), options,
                                      frame_load_type);
}

NavigationResult* NavigationApi::reload(ScriptState* script_state,
                                        NavigationReloadOptions* options) {
  scoped_refptr<SerializedScriptValue> serialized_state = nullptr;
  {
    if (options->hasState()) {
      ExceptionState exception_state(
          script_state->GetIsolate(),
          ExceptionContext::Context::kOperationInvoke, "Navigation", "reload");
      serialized_state = SerializeState(options->state(), exception_state);
      if (exception_state.HadException()) {
        NavigationResult* result =
            EarlyErrorResult(script_state, exception_state.GetException());
        exception_state.ClearException();
        return result;
      }
    } else if (NavigationHistoryEntry* current_entry = currentEntry()) {
      serialized_state = current_entry->GetSerializedState();
    }
  }

  FrameLoadRequest request(window_, ResourceRequest(window_->Url()));
  request.SetClientRedirectReason(ClientNavigationReason::kFrameNavigation);

  return PerformNonTraverseNavigation(script_state, request,
                                      std::move(serialized_state), options,
                                      WebFrameLoadType::kReload);
}

NavigationResult* NavigationApi::PerformNonTraverseNavigation(
    ScriptState* script_state,
    FrameLoadRequest& request,
    scoped_refptr<SerializedScriptValue> serialized_state,
    NavigationOptions* options,
    WebFrameLoadType frame_load_type) {
  DCHECK(frame_load_type == WebFrameLoadType::kReplaceCurrentItem ||
         frame_load_type == WebFrameLoadType::kReload ||
         frame_load_type == WebFrameLoadType::kStandard);

  String method_name_for_error_message(
      frame_load_type == WebFrameLoadType::kReload ? "reload()" : "navigate()");
  if (DOMException* maybe_ex =
          PerformSharedNavigationChecks(method_name_for_error_message))
    return EarlyErrorResult(script_state, maybe_ex);

  NavigationApiNavigation* navigation =
      MakeGarbageCollected<NavigationApiNavigation>(
          script_state, this, options, String(), std::move(serialized_state));
  upcoming_non_traversal_navigation_ = navigation;

  window_->GetFrame()->MaybeLogAdClickNavigation();
  window_->GetFrame()->Navigate(request, frame_load_type);

  // DispatchNavigateEvent() will clear upcoming_non_traversal_navigation_ if we
  // get that far. If the navigation is blocked before DispatchNavigateEvent()
  // is called, reject the promise and cleanup here.
  if (upcoming_non_traversal_navigation_ == navigation) {
    upcoming_non_traversal_navigation_ = nullptr;
    return EarlyErrorResult(script_state, DOMExceptionCode::kAbortError,
                            "Navigation was aborted");
  }
  return navigation->GetNavigationResult();
}

NavigationResult* NavigationApi::traverseTo(ScriptState* script_state,
                                            const String& key,
                                            NavigationOptions* options) {
  if (DOMException* maybe_ex =
          PerformSharedNavigationChecks("traverseTo()/back()/forward()")) {
    return EarlyErrorResult(script_state, maybe_ex);
  }

  if (!keys_to_indices_.Contains(key)) {
    return EarlyErrorResult(script_state, DOMExceptionCode::kInvalidStateError,
                            "Invalid key");
  }
  if (key == currentEntry()->key()) {
    return EarlySuccessResult(script_state, currentEntry());
  }

  auto previous_navigation = upcoming_traversals_.find(key);
  if (previous_navigation != upcoming_traversals_.end())
    return previous_navigation->value->GetNavigationResult();

  NavigationApiNavigation* ongoing_navigation =
      MakeGarbageCollected<NavigationApiNavigation>(script_state, this, options,
                                                    key);
  upcoming_traversals_.insert(key, ongoing_navigation);
  if (window_->GetFrame()->IsMainFrame()) {
    SoftNavigationHeuristics* heuristics =
        SoftNavigationHeuristics::From(*window_);
    heuristics->SawURLChange(script_state, /*url=*/String(""));
  }
  auto* tracker = ThreadScheduler::Current()->GetTaskAttributionTracker();
  absl::optional<scheduler::TaskAttributionId> task_id;
  if (tracker && script_state->World().IsMainWorld()) {
    task_id = tracker->RunningTaskAttributionId(script_state);
  }
  window_->GetFrame()->GetLocalFrameHostRemote().NavigateToNavigationApiKey(
      key, LocalFrame::HasTransientUserActivation(window_->GetFrame()),
      task_id);
  return ongoing_navigation->GetNavigationResult();
}

bool NavigationApi::canGoBack() const {
  return !HasEntriesAndEventsDisabled() && current_entry_index_ > 0;
}

bool NavigationApi::canGoForward() const {
  return !HasEntriesAndEventsDisabled() && current_entry_index_ != -1 &&
         static_cast<size_t>(current_entry_index_) < entries_.size() - 1;
}

NavigationResult* NavigationApi::back(ScriptState* script_state,
                                      NavigationOptions* options) {
  if (!canGoBack()) {
    return EarlyErrorResult(script_state, DOMExceptionCode::kInvalidStateError,
                            "Cannot go back");
  }
  return traverseTo(script_state, entries_[current_entry_index_ - 1]->key(),
                    options);
}

NavigationResult* NavigationApi::forward(ScriptState* script_state,
                                         NavigationOptions* options) {
  if (!canGoForward()) {
    return EarlyErrorResult(script_state, DOMExceptionCode::kInvalidStateError,
                            "Cannot go forward");
  }
  return traverseTo(script_state, entries_[current_entry_index_ + 1]->key(),
                    options);
}

DOMException* NavigationApi::PerformSharedNavigationChecks(
    const String& method_name_for_error_message) {
  if (!window_->GetFrame()) {
    return MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kInvalidStateError,
        method_name_for_error_message +
            " cannot be called when the Window is detached.");
  }
  if (window_->document()->PageDismissalEventBeingDispatched()) {
    return MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kInvalidStateError,
        method_name_for_error_message +
            " cannot be called during unload or beforeunload.");
  }
  return nullptr;
}

scoped_refptr<SerializedScriptValue> NavigationApi::SerializeState(
    const ScriptValue& value,
    ExceptionState& exception_state) {
  return SerializedScriptValue::Serialize(
      window_->GetIsolate(), value.V8Value(),
      SerializedScriptValue::SerializeOptions(
          SerializedScriptValue::kForStorage),
      exception_state);
}

void NavigationApi::PromoteUpcomingNavigationToOngoing(const String& key) {
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

bool NavigationApi::HasEntriesAndEventsDisabled() const {
  // Disable for initial empty documents, opaque origins, or in detached
  // windows. Also, in destroyed-but-not-detached windows due to memory purging
  // (see https://crbug.com/1319341).
  return !window_->GetFrame() || window_->IsContextDestroyed() ||
         !window_->GetFrame()->Loader().HasLoadedNonInitialEmptyDocument() ||
         window_->GetSecurityOrigin()->IsOpaque();
}

NavigationHistoryEntry* NavigationApi::MakeEntryFromItem(HistoryItem& item) {
  return MakeGarbageCollected<NavigationHistoryEntry>(
      window_, item.GetNavigationApiKey(), item.GetNavigationApiId(),
      item.Url(), item.DocumentSequenceNumber(), item.GetNavigationApiState());
}

NavigationApi::DispatchResult NavigationApi::DispatchNavigateEvent(
    NavigateEventDispatchParams* params) {
  // TODO(japhet): The draft spec says to cancel any ongoing navigate event
  // before invoking DispatchNavigateEvent(), because not all navigations will
  // fire a navigate event, but all should abort an ongoing navigate event.
  // The main case were that would be a problem (browser-initiated back/forward)
  // is not implemented yet. Move this once it is implemented.
  InformAboutCanceledNavigation();
  DCHECK(window_);

  const KURL& current_url = window_->Url();

  const String& key = params->destination_item
                          ? params->destination_item->GetNavigationApiKey()
                          : String();
  PromoteUpcomingNavigationToOngoing(key);

  if (HasEntriesAndEventsDisabled()) {
    if (ongoing_navigation_) {
      // The spec only does the equivalent of CleanupApiNavigation() + resetting
      // the state, but we need to detach promise resolvers for this case since
      // we will never resolve the finished/committed promises.
      ongoing_navigation_->CleanupForWillNeverSettle();
    }
    return DispatchResult::kContinue;
  }

  LocalFrame* frame = window_->GetFrame();
  auto* script_state = ToScriptStateForMainWorld(frame);
  ScriptState::Scope scope(script_state);

  if (params->frame_load_type == WebFrameLoadType::kBackForward &&
      params->event_type == NavigateEventType::kFragment &&
      !keys_to_indices_.Contains(key)) {
    // This same document history traversal was preempted by another navigation
    // that removed this entry from the back/forward list. Proceeding will leave
    // entries_ out of sync with the browser process.
    FinalizeWithAbortedNavigationError(script_state, ongoing_navigation_);
    return DispatchResult::kAbort;
  }

  auto* init = NavigateEventInit::Create();
  const String& navigation_type =
      DetermineNavigationType(params->frame_load_type);
  init->setNavigationType(navigation_type);

  SerializedScriptValue* destination_state = nullptr;
  if (params->destination_item) {
    destination_state = params->destination_item->GetNavigationApiState();
  } else if (ongoing_navigation_) {
    destination_state = ongoing_navigation_->GetSerializedState();
  } else if (navigation_type == "reload") {
    HistoryItem* current_item = window_->document()->Loader()->GetHistoryItem();
    destination_state = current_item->GetNavigationApiState();
  }
  NavigationDestination* destination =
      MakeGarbageCollected<NavigationDestination>(
          params->url, params->event_type != NavigateEventType::kCrossDocument,
          destination_state);
  if (params->frame_load_type == WebFrameLoadType::kBackForward) {
    auto iter = keys_to_indices_.find(key);
    int index = iter == keys_to_indices_.end() ? 0 : iter->value;
    destination->SetTraverseProperties(
        key, params->destination_item->GetNavigationApiId(), index);
  }
  init->setDestination(destination);

  bool should_allow_traversal_cancellation =
      RuntimeEnabledFeatures::NavigateEventCancelableTraversalsEnabled() &&
      params->frame_load_type == WebFrameLoadType::kBackForward &&
      params->event_type != NavigateEventType::kCrossDocument &&
      frame->IsMainFrame() &&
      (!params->is_browser_initiated || frame->IsHistoryUserActivationActive());
  init->setCancelable(params->frame_load_type !=
                          WebFrameLoadType::kBackForward ||
                      should_allow_traversal_cancellation);
  init->setCanIntercept(
      CanChangeToUrlForHistoryApi(params->url, window_->GetSecurityOrigin(),
                                  current_url) &&
      (params->event_type != NavigateEventType::kCrossDocument ||
       params->frame_load_type != WebFrameLoadType::kBackForward));
  init->setHashChange(
      params->event_type == NavigateEventType::kFragment &&
      params->url != current_url &&
      EqualIgnoringFragmentIdentifier(params->url, current_url));

  init->setUserInitiated(params->involvement !=
                         UserNavigationInvolvement::kNone);
  if (params->form && params->form->Method() == FormSubmission::kPostMethod) {
    init->setFormData(FormData::Create(params->form, ASSERT_NO_EXCEPTION));
  }
  if (ongoing_navigation_)
    init->setInfo(ongoing_navigation_->GetInfo());
  init->setSignal(MakeGarbageCollected<AbortSignal>(window_));
  init->setDownloadRequest(params->download_filename);
  // This unique_ptr needs to be in the function's scope, to maintain the
  // SoftNavigationEventScope until the event handler runs.
  std::unique_ptr<SoftNavigationEventScope> soft_navigation_scope;
  auto* soft_navigation_heuristics = SoftNavigationHeuristics::From(*window_);
  if (soft_navigation_heuristics && init->userInitiated() &&
      !init->downloadRequest() && init->canIntercept()) {
    // If these conditions are met, create a SoftNavigationEventScope to
    // consider this a "user initiated click", and the dispatched event handlers
    // as potential soft navigation tasks.
    soft_navigation_scope = std::make_unique<SoftNavigationEventScope>(
        soft_navigation_heuristics, script_state);
  }
  auto* navigate_event =
      NavigateEvent::Create(window_, event_type_names::kNavigate, init);
  navigate_event->SetUrl(params->url);
  navigate_event->SaveStateFromDestinationItem(params->destination_item);

  DCHECK(!ongoing_navigate_event_);
  ongoing_navigate_event_ = navigate_event;
  has_dropped_navigation_ = false;
  DispatchEvent(*navigate_event);

  if (navigate_event->defaultPrevented()) {
    if (params->frame_load_type == WebFrameLoadType::kBackForward &&
        window_->GetFrame()) {
      window_->GetFrame()->ConsumeHistoryUserActivation();
    }
    if (!navigate_event->signal()->aborted()) {
      FinalizeWithAbortedNavigationError(script_state, ongoing_navigation_);
    }
    return DispatchResult::kAbort;
  }

  if (navigate_event->HasNavigationActions()) {
    transition_ = MakeGarbageCollected<NavigationTransition>(
        script_state, navigation_type, currentEntry());

    DCHECK(!params->destination_item || !params->state_object);
    auto* state_object = params->destination_item
                             ? params->destination_item->StateObject()
                             : params->state_object.get();

    // In the spec, the URL and history update steps are not called for reloads.
    // In our implementation, we call the corresponding function anyway, but
    // |type| being a reload type makes it do none of the spec-relevant
    // steps. Instead it does stuff like the loading spinner and use counters.
    window_->document()->Loader()->RunURLAndHistoryUpdateSteps(
        params->url, params->destination_item,
        mojom::blink::SameDocumentNavigationType::kNavigationApiIntercept,
        state_object, params->frame_load_type, params->is_browser_initiated,
        params->is_synchronously_committed_same_document);

    // This is considered a soft navigation URL change at this point, when the
    // user visible URL change happens, and before the interception handler
    // runs. We're skipping the descendant check because the the URL change
    // doesn't happen in a JS task, and we know this URL change is related to
    // the user initiated click event from the fact that
    // `soft_navigation_scope` is not nullptr.
    if (soft_navigation_scope) {
      soft_navigation_heuristics->SawURLChange(script_state,
                                               /*url=*/params->url,
                                               /*skip_descendant_check=*/true);
    }
  }

  if (navigate_event->HasNavigationActions() ||
      params->event_type != NavigateEventType::kCrossDocument) {
    NavigateReaction::ReactType react_type =
        navigate_event->HasNavigationActions()
            ? NavigateReaction::ReactType::kIntercept
            : NavigateReaction::ReactType::kImmediate;

    // There is a subtle timing difference between the fast-path for zero
    // promises and the path for 1+ promises, in both spec and implementation.
    // In most uses of ScriptPromise::All / the Web IDL spec's "wait for all",
    // this does not matter. However for us there are so many events and promise
    // handlers firing around the same time (navigatesuccess, committed promise,
    // finished promise, ...) that the difference is pretty easily observable by
    // web developers and web platform tests. So, let's make sure we always go
    // down the 1+ promises path.
    auto promise_list = navigate_event->GetNavigationActionPromisesList();
    const HeapVector<ScriptPromise>& tweaked_promise_list =
        promise_list.empty() ? HeapVector<ScriptPromise>(
                                   {ScriptPromise::CastUndefined(script_state)})
                             : promise_list;

    NavigateReaction::React(
        script_state, ScriptPromise::All(script_state, tweaked_promise_list),
        ongoing_navigation_, navigate_event, react_type);
  }

  // Note: we cannot clean up ongoing_navigation_ for cross-document
  // navigations, because they might later get interrupted by another
  // navigation, in which case we need to reject the promises and so on.

  return navigate_event->HasNavigationActions() ? DispatchResult::kIntercept
                                                : DispatchResult::kContinue;
}

void NavigationApi::InformAboutCanceledNavigation(
    CancelNavigationReason reason) {
  if (reason == CancelNavigationReason::kDropped) {
    has_dropped_navigation_ = true;
    return;
  }
  if (HasEntriesAndEventsDisabled())
    return;

  if (ongoing_navigate_event_) {
    auto* script_state = ToScriptStateForMainWorld(window_->GetFrame());
    ScriptState::Scope scope(script_state);
    FinalizeWithAbortedNavigationError(script_state, ongoing_navigation_);
  }

  // If this function is being called as part of frame detach, also cleanup any
  // upcoming_traversals_.
  //
  // This function may be called when a v8 context hasn't been initialized.
  // upcoming_traversals_ being non-empty requires a v8 context, so check that
  // so that we don't unnecessarily try to initialize one below.
  if (!upcoming_traversals_.empty() && window_->GetFrame() &&
      !window_->GetFrame()->IsAttached()) {
    auto* script_state = ToScriptStateForMainWorld(window_->GetFrame());
    ScriptState::Scope scope(script_state);

    HeapVector<Member<NavigationApiNavigation>> traversals;
    CopyValuesToVector(upcoming_traversals_, traversals);
    for (auto& traversal : traversals)
      FinalizeWithAbortedNavigationError(script_state, traversal);
    DCHECK(upcoming_traversals_.empty());
  }
}

void NavigationApi::TraverseCancelled(
    const String& key,
    mojom::blink::TraverseCancelledReason reason) {
  auto traversal = upcoming_traversals_.find(key);
  if (traversal == upcoming_traversals_.end())
    return;

  auto* script_state = ToScriptStateForMainWorld(window_->GetFrame());
  ScriptState::Scope scope(script_state);
  DOMException* exception = nullptr;
  if (reason == mojom::blink::TraverseCancelledReason::kNotFound) {
    exception = MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kInvalidStateError, "Invalid key");
  } else if (reason ==
             mojom::blink::TraverseCancelledReason::kSandboxViolation) {
    exception = MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kSecurityError,
        "Navigating to key " + key +
            " would require a navigation that "
            "violates this frame's sandbox policy");
  } else if (reason ==
             mojom::blink::TraverseCancelledReason::kAbortedBeforeCommit) {
    exception = MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kAbortError, "Navigation was aborted");
  }
  DCHECK(exception);

  RejectPromisesAndFireNavigateErrorEvent(
      traversal->value, ScriptValue::From(script_state, exception));
}

void NavigationApi::ContextDestroyed() {
  if (ongoing_navigation_)
    ongoing_navigation_->CleanupForWillNeverSettle();
}

bool NavigationApi::HasNonDroppedOngoingNavigation() const {
  bool has_ongoing_intercept = ongoing_navigate_event_ &&
                               ongoing_navigate_event_->HasNavigationActions();
  return has_ongoing_intercept && !has_dropped_navigation_;
}

void NavigationApi::RejectPromisesAndFireNavigateErrorEvent(
    NavigationApiNavigation* navigation,
    ScriptValue value) {
  auto* isolate = window_->GetIsolate();
  v8::Local<v8::Message> message =
      v8::Exception::CreateMessage(isolate, value.V8Value());
  std::unique_ptr<SourceLocation> location =
      blink::CaptureSourceLocation(isolate, message, window_);
  ErrorEvent* event = ErrorEvent::Create(
      ToCoreStringWithNullCheck(message->Get()), std::move(location), value,
      &DOMWrapperWorld::MainWorld());
  event->SetType(event_type_names::kNavigateerror);
  DispatchEvent(*event);

  if (navigation)
    navigation->RejectFinishedPromise(value);

  if (transition_) {
    transition_->RejectFinishedPromise(value);
    transition_ = nullptr;
  }
}

void NavigationApi::ResolvePromisesAndFireNavigateSuccessEvent(
    NavigationApiNavigation* navigation) {
  DispatchEvent(*Event::Create(event_type_names::kNavigatesuccess));

  if (navigation)
    navigation->ResolveFinishedPromise();

  if (transition_) {
    transition_->ResolveFinishedPromise();
    transition_ = nullptr;
  }
}

void NavigationApi::CleanupApiNavigation(NavigationApiNavigation& navigation) {
  if (&navigation == ongoing_navigation_) {
    ongoing_navigation_ = nullptr;
  } else {
    DCHECK(!navigation.GetKey().IsNull());
    DCHECK(upcoming_traversals_.Contains(navigation.GetKey()));
    upcoming_traversals_.erase(navigation.GetKey());
  }
}

void NavigationApi::FinalizeWithAbortedNavigationError(
    ScriptState* script_state,
    NavigationApiNavigation* navigation) {
  ScriptValue error = ScriptValue::From(
      script_state,
      MakeGarbageCollected<DOMException>(DOMExceptionCode::kAbortError,
                                         "Navigation was aborted"));

  if (ongoing_navigate_event_) {
    if (ongoing_navigate_event_->IsBeingDispatched())
      ongoing_navigate_event_->preventDefault();
    ongoing_navigate_event_->signal()->SignalAbort(script_state, error);
    ongoing_navigate_event_ = nullptr;
  }

  RejectPromisesAndFireNavigateErrorEvent(navigation, error);
}

int NavigationApi::GetIndexFor(NavigationHistoryEntry* entry) {
  const auto& it = keys_to_indices_.find(entry->key());
  if (it == keys_to_indices_.end() || entry != entries_[it->value])
    return -1;
  return it->value;
}

const AtomicString& NavigationApi::InterfaceName() const {
  return event_target_names::kNavigation;
}

void NavigationApi::AddedEventListener(
    const AtomicString& event_type,
    RegisteredEventListener& registered_listener) {
  EventTargetWithInlineData::AddedEventListener(event_type,
                                                registered_listener);
  LocalFrame* frame = window_->GetFrame();
  if (event_type != event_type_names::kNavigate || !frame) {
    return;
  }
  navigate_event_handler_count_++;
  if (navigate_event_handler_count_ == 1) {
    frame->GetLocalFrameHostRemote().NavigateEventHandlerPresenceChanged(true);
  }
}

void NavigationApi::RemovedEventListener(
    const AtomicString& event_type,
    const RegisteredEventListener& registered_listener) {
  EventTargetWithInlineData::RemovedEventListener(event_type,
                                                  registered_listener);
  LocalFrame* frame = window_->GetFrame();
  if (event_type != event_type_names::kNavigate || !frame) {
    return;
  }
  navigate_event_handler_count_--;
  if (navigate_event_handler_count_ == 0) {
    frame->GetLocalFrameHostRemote().NavigateEventHandlerPresenceChanged(false);
  }
}

void NavigationApi::Trace(Visitor* visitor) const {
  EventTargetWithInlineData::Trace(visitor);
  ExecutionContextLifecycleObserver::Trace(visitor);
  visitor->Trace(window_);
  visitor->Trace(entries_);
  visitor->Trace(transition_);
  visitor->Trace(ongoing_navigation_);
  visitor->Trace(upcoming_traversals_);
  visitor->Trace(upcoming_non_traversal_navigation_);
  visitor->Trace(ongoing_navigate_event_);
}

}  // namespace blink
