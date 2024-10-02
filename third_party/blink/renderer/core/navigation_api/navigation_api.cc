// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/navigation_api/navigation_api.h"

#include <memory>
#include <optional>

#include "base/check_op.h"
#include "base/feature_list.h"
#include "third_party/blink/public/mojom/frame/frame.mojom-blink.h"
#include "third_party/blink/public/web/web_frame_load_type.h"
#include "third_party/blink/renderer/bindings/core/v8/capture_source_location.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/bindings/core/v8/to_v8_traits.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_navigate_event_init.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_navigation_current_entry_change_event_init.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_navigation_history_behavior.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_navigation_navigate_options.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_navigation_reload_options.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_navigation_result.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_navigation_transition.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_navigation_update_current_entry_options.h"
#include "third_party/blink/renderer/core/dom/abort_controller.h"
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
#include "third_party/blink/renderer/core/navigation_api/navigation_activation.h"
#include "third_party/blink/renderer/core/navigation_api/navigation_api_method_tracker.h"
#include "third_party/blink/renderer/core/navigation_api/navigation_current_entry_change_event.h"
#include "third_party/blink/renderer/core/navigation_api/navigation_destination.h"
#include "third_party/blink/renderer/core/navigation_api/navigation_history_entry.h"
#include "third_party/blink/renderer/core/navigation_api/navigation_transition.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/timing/soft_navigation_heuristics.h"
#include "third_party/blink/renderer/platform/bindings/exception_context.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/scheduler/public/task_attribution_info.h"
#include "third_party/blink/renderer/platform/scheduler/public/task_attribution_tracker.h"

namespace blink {

namespace {

// Kill switch feature for fixing https://crbug.com/40283021.
// TODO(domenic): clean this up after that fix has baked in stable for a few
// milestones.
BASE_FEATURE(kUtf8OnlyNavigationApiNavigations,
             "Utf8OnlyNavigationApiNavigations",
             base::FEATURE_ENABLED_BY_DEFAULT);
}  // namespace

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
  result->setCommitted(
      ScriptPromise<NavigationHistoryEntry>::RejectWithDOMException(
          script_state, ex));
  result->setFinished(
      ScriptPromise<NavigationHistoryEntry>::RejectWithDOMException(
          script_state, ex));
  return result;
}

NavigationResult* EarlyErrorResult(ScriptState* script_state,
                                   v8::Local<v8::Value> ex) {
  auto* result = NavigationResult::Create();
  result->setCommitted(
      ScriptPromise<NavigationHistoryEntry>::Reject(script_state, ex));
  result->setFinished(
      ScriptPromise<NavigationHistoryEntry>::Reject(script_state, ex));
  return result;
}

NavigationResult* EarlySuccessResult(ScriptState* script_state,
                                     NavigationHistoryEntry* entry) {
  auto* result = NavigationResult::Create();
  result->setCommitted(
      ToResolvedPromise<NavigationHistoryEntry>(script_state, entry));
  result->setFinished(
      ToResolvedPromise<NavigationHistoryEntry>(script_state, entry));
  return result;
}

V8NavigationType::Enum DetermineNavigationType(WebFrameLoadType type) {
  switch (type) {
    case WebFrameLoadType::kStandard:
      return V8NavigationType::Enum::kPush;
    case WebFrameLoadType::kBackForward:
    case WebFrameLoadType::kRestore:
      return V8NavigationType::Enum::kTraverse;
    case WebFrameLoadType::kReload:
    case WebFrameLoadType::kReloadBypassingCache:
      return V8NavigationType::Enum::kReload;
    case WebFrameLoadType::kReplaceCurrentItem:
      return V8NavigationType::Enum::kReplace;
  }
  NOTREACHED();
}

NavigationApi::NavigationApi(LocalDOMWindow* window)
    : window_(window),
      activation_(MakeGarbageCollected<NavigationActivation>()) {}

NavigationActivation* NavigationApi::activation() const {
  return HasEntriesAndEventsDisabled() ? nullptr : activation_;
}

void NavigationApi::setOnnavigate(EventListener* listener) {
  UseCounter::Count(window_, WebFeature::kNavigationAPI);
  SetAttributeEventListener(event_type_names::kNavigate, listener);
}

void NavigationApi::PopulateKeySet() {
  CHECK(keys_to_indices_.empty());
  for (wtf_size_t i = 0; i < entries_.size(); i++)
    keys_to_indices_.insert(entries_[i]->key(), i);
}

void NavigationApi::UpdateActivation(HistoryItem* previous_item,
                                     WebFrameLoadType load_type) {
  NavigationHistoryEntry* previous_history_entry = nullptr;
  if (previous_item) {
    if (auto* entry =
            GetExistingEntryFor(previous_item->GetNavigationApiKey(),
                                previous_item->GetNavigationApiId())) {
      previous_history_entry = entry;
    } else {
      previous_history_entry = MakeEntryFromItem(*previous_item);
    }
  }
  V8NavigationType::Enum navigation_type =
      window_->GetFrame()->GetPage()->IsPrerendering()
          ? V8NavigationType::Enum::kPush
          : DetermineNavigationType(load_type);
  activation_->Update(currentEntry(), previous_history_entry, navigation_type);
}

NavigationHistoryEntry* NavigationApi::GetExistingEntryFor(const String& key,
                                                           const String& id) {
  const auto& it = keys_to_indices_.find(key);
  if (it == keys_to_indices_.end()) {
    return nullptr;
  }
  NavigationHistoryEntry* existing_entry = entries_[it->value];
  return existing_entry->id() == id ? existing_entry : nullptr;
}

void NavigationApi::InitializeForNewWindow(
    HistoryItem& current,
    WebFrameLoadType load_type,
    CommitReason commit_reason,
    NavigationApi* previous,
    const WebVector<WebHistoryItem>& back_entries,
    const WebVector<WebHistoryItem>& forward_entries,
    HistoryItem* previous_entry) {
  CHECK(entries_.empty());

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
      CHECK(entries_.empty());
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
  UpdateActivation(previous_entry, load_type);
}

void NavigationApi::UpdateCurrentEntryForTesting(HistoryItem& item) {
  current_entry_index_++;
  entries_.resize(current_entry_index_ + 1);
  entries_[current_entry_index_] = MakeEntryFromItem(item);
  keys_to_indices_.insert(entries_[current_entry_index_]->key(),
                          current_entry_index_);
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
  if (IsBackForwardOrRestore(type)) {
    // If this is a same-document back/forward navigation or restore, the new
    // current entry should already be present in entries_ and its key in
    // keys_to_indices_.
    CHECK(keys_to_indices_.Contains(item.GetNavigationApiKey()));
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
    CHECK_NE(current_entry_index_, -1);
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
  if (ongoing_api_method_tracker_) {
    ongoing_api_method_tracker_->NotifyAboutTheCommittedToEntry(
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
  if (!entry) {
    return nullptr;
  }
  if (auto* existing_entry = GetExistingEntryFor(entry->key, entry->id)) {
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
    const mojom::blink::NavigationApiHistoryEntryArraysPtr& entry_arrays,
    mojom::blink::NavigationApiEntryRestoreReason restore_reason) {
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

  V8NavigationType::Enum navigation_type;
  switch (restore_reason) {
    case mojom::blink::NavigationApiEntryRestoreReason::kBFCache:
      navigation_type = V8NavigationType::Enum::kTraverse;
      break;
    case mojom::blink::NavigationApiEntryRestoreReason::
        kPrerenderActivationPush:
      navigation_type = V8NavigationType::Enum::kPush;
      break;
    case mojom::blink::NavigationApiEntryRestoreReason::
        kPrerenderActivationReplace:
      navigation_type = V8NavigationType::Enum::kReplace;
      break;
    default:
      NOTREACHED();
  }
  activation_->Update(currentEntry(),
                      GetEntryForRestore(entry_arrays->previous_entry),
                      navigation_type);

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
  KURL completed_url =
      base::FeatureList::IsEnabled(kUtf8OnlyNavigationApiNavigations)
          ? KURL(window_->BaseURL(), url)
          : window_->CompleteURL(url);
  if (!completed_url.IsValid()) {
    return EarlyErrorResult(script_state, DOMExceptionCode::kSyntaxError,
                            "Invalid URL '" + completed_url.GetString() + "'.");
  }

  scoped_refptr<SerializedScriptValue> serialized_state = nullptr;
  {
    if (options->hasState()) {
      v8::TryCatch try_catch(script_state->GetIsolate());
      serialized_state = SerializeState(
          options->state(), PassThroughException(script_state->GetIsolate()));
      if (try_catch.HasCaught()) {
        return EarlyErrorResult(script_state, try_catch.Exception());
      }
    }
  }

  FrameLoadRequest request(window_, ResourceRequest(completed_url));
  request.SetClientNavigationReason(ClientNavigationReason::kFrameNavigation);

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
      v8::TryCatch try_catch(script_state->GetIsolate());
      serialized_state = SerializeState(
          options->state(), PassThroughException(script_state->GetIsolate()));
      if (try_catch.HasCaught()) {
        return EarlyErrorResult(script_state, try_catch.Exception());
      }
    } else if (NavigationHistoryEntry* current_entry = currentEntry()) {
      serialized_state = current_entry->GetSerializedState();
    }
  }

  FrameLoadRequest request(window_, ResourceRequest(window_->Url()));
  request.SetClientNavigationReason(ClientNavigationReason::kFrameNavigation);

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
  CHECK(frame_load_type == WebFrameLoadType::kReplaceCurrentItem ||
        frame_load_type == WebFrameLoadType::kReload ||
        frame_load_type == WebFrameLoadType::kStandard);

  String method_name_for_error_message(
      frame_load_type == WebFrameLoadType::kReload ? "reload()" : "navigate()");
  if (DOMException* maybe_ex =
          PerformSharedNavigationChecks(method_name_for_error_message))
    return EarlyErrorResult(script_state, maybe_ex);

  NavigationApiMethodTracker* api_method_tracker =
      MakeGarbageCollected<NavigationApiMethodTracker>(
          script_state, options, String(), std::move(serialized_state));
  if (HasEntriesAndEventsDisabled()) {
    // If `HasEntriesAndEventsDisabled()` is true, we still allow the
    // navigation, but the navigate event won't fire and we won't do anything
    // with the promises, so we need to detach the promise resolvers.
    api_method_tracker->CleanupForWillNeverSettle();
  } else {
    upcoming_non_traverse_api_method_tracker_ = api_method_tracker;
  }

  window_->GetFrame()->Navigate(request, frame_load_type);

  // DispatchNavigateEvent() will clear
  // upcoming_non_traverse_api_method_tracker_ if we get that far. If the
  // navigation is blocked before DispatchNavigateEvent() is called, reject the
  // promise and cleanup here.
  if (upcoming_non_traverse_api_method_tracker_ == api_method_tracker) {
    upcoming_non_traverse_api_method_tracker_ = nullptr;
    return EarlyErrorResult(script_state, DOMExceptionCode::kAbortError,
                            "Navigation was aborted");
  }
  return api_method_tracker->GetNavigationResult();
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

  auto previous_api_method_tracker =
      upcoming_traverse_api_method_trackers_.find(key);
  if (previous_api_method_tracker !=
      upcoming_traverse_api_method_trackers_.end()) {
    return previous_api_method_tracker->value->GetNavigationResult();
  }

  NavigationApiMethodTracker* api_method_tracker =
      MakeGarbageCollected<NavigationApiMethodTracker>(script_state, options,
                                                       key);
  upcoming_traverse_api_method_trackers_.insert(key, api_method_tracker);
  LocalFrame* frame = window_->GetFrame();
  std::optional<scheduler::TaskAttributionId> soft_navigation_task_id;
  if (script_state->World().IsMainWorld() && frame->IsOutermostMainFrame()) {
    if (SoftNavigationHeuristics* heuristics =
            SoftNavigationHeuristics::From(*window_)) {
      soft_navigation_task_id =
          heuristics->AsyncSameDocumentNavigationStarted();
    }
  }
  frame->GetLocalFrameHostRemote().NavigateToNavigationApiKey(
      key, LocalFrame::HasTransientUserActivation(frame),
      soft_navigation_task_id);
  return api_method_tracker->GetNavigationResult();
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
  CHECK(!ongoing_api_method_tracker_);
  if (!key.IsNull()) {
    CHECK(!upcoming_non_traverse_api_method_tracker_);
    auto iter = upcoming_traverse_api_method_trackers_.find(key);
    if (iter != upcoming_traverse_api_method_trackers_.end()) {
      ongoing_api_method_tracker_ = iter->value;
      upcoming_traverse_api_method_trackers_.erase(iter);
    }
  } else {
    ongoing_api_method_tracker_ =
        upcoming_non_traverse_api_method_tracker_.Release();
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
  InformAboutCanceledNavigation(CancelNavigationReason::kNavigateEvent);
  CHECK(window_);

  if (HasEntriesAndEventsDisabled()) {
    // These assertions holds because:
    // * back()/forward()/traverseTo() immediately fail when
    //   `HasEntriesAndEventsDisabled()` is false, because current_entry_index_
    //   will be permanently -1.
    // * navigate()/reload() will not set
    //   `upcoming_non_traverse_api_method_tracker_` when
    //   `HasEntriesAndEventsDisabled()` is false, so there's nothing to promote
    //   to `ongoing_navigation_`.
    // * non-NavigationApi navigations never create an upcoming navigation.
    CHECK(!ongoing_api_method_tracker_);
    CHECK(!upcoming_non_traverse_api_method_tracker_);
    CHECK(upcoming_traverse_api_method_trackers_.empty());
    return DispatchResult::kContinue;
  }

  const String& key = params->destination_item
                          ? params->destination_item->GetNavigationApiKey()
                          : String();
  if (IsBackForwardOrRestore(params->frame_load_type) &&
      params->event_type == NavigateEventType::kFragment &&
      !keys_to_indices_.Contains(key)) {
    // This same document history traversal was preempted by another navigation
    // that removed this entry from the back/forward list. Proceeding will leave
    // entries_ out of sync with the browser process.
    TraverseCancelled(
        key, mojom::blink::TraverseCancelledReason::kAbortedBeforeCommit);
    return DispatchResult::kAbort;
  }

  PromoteUpcomingNavigationToOngoing(key);

  LocalFrame* frame = window_->GetFrame();
  auto* script_state = ToScriptStateForMainWorld(frame);
  ScriptState::Scope scope(script_state);

  auto* init = NavigateEventInit::Create();
  V8NavigationType::Enum navigation_type =
      DetermineNavigationType(params->frame_load_type);
  init->setNavigationType(navigation_type);

  SerializedScriptValue* destination_state = nullptr;
  if (params->destination_item) {
    destination_state = params->destination_item->GetNavigationApiState();
  } else if (ongoing_api_method_tracker_) {
    destination_state = ongoing_api_method_tracker_->GetSerializedState();
  } else if (navigation_type == V8NavigationType::Enum::kReload) {
    HistoryItem* current_item = window_->document()->Loader()->GetHistoryItem();
    destination_state = current_item->GetNavigationApiState();
  }
  NavigationDestination* destination =
      MakeGarbageCollected<NavigationDestination>(
          params->url, params->event_type != NavigateEventType::kCrossDocument,
          destination_state);
  if (IsBackForwardOrRestore(params->frame_load_type)) {
    auto iter = keys_to_indices_.find(key);
    if (iter != keys_to_indices_.end()) {
      destination->SetDestinationEntry(entries_[iter->value]);
    }
  }
  init->setDestination(destination);

  bool should_allow_traversal_cancellation =
      IsBackForwardOrRestore(params->frame_load_type) &&
      params->event_type != NavigateEventType::kCrossDocument &&
      frame->IsMainFrame() &&
      (!params->is_browser_initiated || frame->IsHistoryUserActivationActive());
  init->setCancelable(!IsBackForwardOrRestore(params->frame_load_type) ||
                      should_allow_traversal_cancellation);
  init->setCanIntercept(
      CanChangeToUrlForHistoryApi(params->url, window_->GetSecurityOrigin(),
                                  window_->Url()) &&
      (params->event_type != NavigateEventType::kCrossDocument ||
       !IsBackForwardOrRestore(params->frame_load_type)));
  init->setHashChange(
      params->event_type == NavigateEventType::kFragment &&
      params->url != window_->Url() &&
      EqualIgnoringFragmentIdentifier(params->url, window_->Url()));

  init->setUserInitiated(params->involvement !=
                         UserNavigationInvolvement::kNone);
  if (params->source_element) {
    HTMLFormElement* form =
        DynamicTo<HTMLFormElement>(params->source_element.Get());
    if (!form) {
      if (auto* control =
              DynamicTo<HTMLFormControlElement>(params->source_element.Get())) {
        form = control->formOwner();
      }
    }
    if (form && form->Method() == FormSubmission::kPostMethod) {
      init->setFormData(FormData::Create(form, ASSERT_NO_EXCEPTION));
    }
  }
  if (ongoing_api_method_tracker_) {
    init->setInfo(ongoing_api_method_tracker_->GetInfo());
  }
  auto* controller = AbortController::Create(script_state);
  init->setSignal(controller->signal());
  init->setDownloadRequest(params->download_filename);
  if (params->source_element &&
      params->source_element->GetExecutionContext() == window_) {
    init->setSourceElement(params->source_element);
  }
  init->setHasUAVisualTransition(params->has_ua_visual_transition);

  auto* navigate_event = NavigateEvent::Create(
      window_, event_type_names::kNavigate, init, controller);
  navigate_event->SetDispatchParams(params);

  std::optional<SoftNavigationHeuristics::EventScope> soft_navigation_scope;
  if (params->frame_load_type != WebFrameLoadType::kReplaceCurrentItem &&
      init->userInitiated() && !init->downloadRequest() &&
      init->canIntercept()) {
    if (auto* heuristics = SoftNavigationHeuristics::From(*window_)) {
      // If these conditions are met, create a SoftNavigationEventScope to
      // consider this a "user initiated click", and the dispatched event
      // handlers as potential soft navigation tasks.
      soft_navigation_scope =
          heuristics->MaybeCreateEventScopeForEvent(*navigate_event);
    }
  }

  CHECK(!ongoing_navigate_event_);
  ongoing_navigate_event_ = navigate_event;
  has_dropped_navigation_ = false;
  DispatchEvent(*navigate_event);

  if (navigate_event->defaultPrevented()) {
    if (IsBackForwardOrRestore(params->frame_load_type) &&
        window_->GetFrame()) {
      window_->GetFrame()->ConsumeHistoryUserActivation();
    }
    if (!navigate_event->signal()->aborted()) {
      AbortOngoingNavigation(script_state);
    }
    return DispatchResult::kAbort;
  }

  if (navigate_event->HasNavigationActions()) {
    transition_ = MakeGarbageCollected<NavigationTransition>(
        window_, navigation_type, currentEntry());
    navigate_event->MaybeCommitImmediately(script_state);
  }

  if (navigate_event->HasNavigationActions() ||
      params->event_type != NavigateEventType::kCrossDocument) {
    navigate_event->React(script_state);
  }

  // Note: we cannot clean up ongoing_navigation_ for cross-document
  // navigations, because they might later get interrupted by another
  // navigation, in which case we need to reject the promises and so on.

  return navigate_event->HasNavigationActions() ? DispatchResult::kIntercept
                                                : DispatchResult::kContinue;
}

void NavigationApi::InformAboutCanceledNavigation(
    CancelNavigationReason reason) {
  if (auto* tracker =
          scheduler::TaskAttributionTracker::From(window_->GetIsolate());
      tracker && reason != CancelNavigationReason::kNavigateEvent) {
    tracker->ResetSameDocumentNavigationTasks();
  }
  if (reason == CancelNavigationReason::kDropped) {
    has_dropped_navigation_ = true;
    return;
  }
  if (HasEntriesAndEventsDisabled())
    return;

  if (ongoing_navigate_event_) {
    auto* script_state = ToScriptStateForMainWorld(window_->GetFrame());
    ScriptState::Scope scope(script_state);
    AbortOngoingNavigation(script_state);
  }

  // If this function is being called as part of frame detach, also cleanup any
  // upcoming_traverse_api_method_trackers_.
  if (!upcoming_traverse_api_method_trackers_.empty() && window_->GetFrame() &&
      !window_->GetFrame()->IsAttached()) {
    HeapVector<Member<NavigationApiMethodTracker>> traversals;
    CopyValuesToVector(upcoming_traverse_api_method_trackers_, traversals);
    for (auto& traversal : traversals) {
      TraverseCancelled(
          traversal->GetKey(),
          mojom::blink::TraverseCancelledReason::kAbortedBeforeCommit);
    }
    CHECK(upcoming_traverse_api_method_trackers_.empty());
  }
}

void NavigationApi::TraverseCancelled(
    const String& key,
    mojom::blink::TraverseCancelledReason reason) {
  auto traversal = upcoming_traverse_api_method_trackers_.find(key);
  if (traversal == upcoming_traverse_api_method_trackers_.end()) {
    return;
  }

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
  CHECK(exception);
  traversal->value->RejectFinishedPromise(
      ScriptValue::From(script_state, exception));
  upcoming_traverse_api_method_trackers_.erase(traversal);
}

bool NavigationApi::HasNonDroppedOngoingNavigation() const {
  bool has_ongoing_intercept = ongoing_navigate_event_ &&
                               ongoing_navigate_event_->HasNavigationActions();
  return has_ongoing_intercept && !has_dropped_navigation_;
}

void NavigationApi::DidFailOngoingNavigation(ScriptValue value) {
  auto* isolate = window_->GetIsolate();
  v8::Local<v8::Message> message =
      v8::Exception::CreateMessage(isolate, value.V8Value());
  std::unique_ptr<SourceLocation> location =
      blink::CaptureSourceLocation(isolate, message, window_);
  ErrorEvent* event = ErrorEvent::Create(
      ToCoreStringWithNullCheck(isolate, message->Get()), std::move(location),
      value, &DOMWrapperWorld::MainWorld(isolate));
  event->SetType(event_type_names::kNavigateerror);
  DispatchEvent(*event);

  if (ongoing_api_method_tracker_) {
    ongoing_api_method_tracker_->RejectFinishedPromise(value);
    ongoing_api_method_tracker_ = nullptr;
  }

  if (transition_) {
    transition_->RejectFinishedPromise(value);
    transition_ = nullptr;
  }
}

void NavigationApi::DidFinishOngoingNavigation() {
  DispatchEvent(*Event::Create(event_type_names::kNavigatesuccess));

  if (ongoing_api_method_tracker_) {
    ongoing_api_method_tracker_->ResolveFinishedPromise();
    ongoing_api_method_tracker_ = nullptr;
  }

  if (transition_) {
    transition_->ResolveFinishedPromise();
    transition_ = nullptr;
  }
}

void NavigationApi::AbortOngoingNavigation(ScriptState* script_state) {
  CHECK(ongoing_navigate_event_);
  ScriptValue error = ScriptValue::From(
      script_state,
      MakeGarbageCollected<DOMException>(DOMExceptionCode::kAbortError,
                                         "Navigation was aborted"));
  ongoing_navigate_event_->Abort(script_state, error);
  ongoing_navigate_event_ = nullptr;
  DidFailOngoingNavigation(error);
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
  EventTarget::AddedEventListener(event_type, registered_listener);
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
  EventTarget::RemovedEventListener(event_type, registered_listener);
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
  EventTarget::Trace(visitor);
  visitor->Trace(window_);
  visitor->Trace(entries_);
  visitor->Trace(transition_);
  visitor->Trace(activation_);
  visitor->Trace(ongoing_api_method_tracker_);
  visitor->Trace(upcoming_traverse_api_method_trackers_);
  visitor->Trace(upcoming_non_traverse_api_method_tracker_);
  visitor->Trace(ongoing_navigate_event_);
}

}  // namespace blink
