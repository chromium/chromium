// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_NAVIGATION_API_NAVIGATION_API_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_NAVIGATION_API_NAVIGATION_API_H_

#include "base/memory/scoped_refptr.h"
#include "third_party/blink/public/mojom/navigation/navigation_api_history_entry_arrays.mojom-blink.h"
#include "third_party/blink/public/web/web_frame_load_type.h"
#include "third_party/blink/public/web/web_history_item.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/dom/events/event_target.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_map.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_vector.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/supplementable.h"

namespace blink {

class AbortSignal;
class NavigationApiNavigation;
class NavigationUpdateCurrentEntryOptions;
class NavigationHistoryEntry;
class NavigateEvent;
class NavigationNavigateOptions;
class NavigationReloadOptions;
class NavigationResult;
class NavigationOptions;
class NavigationTransition;
class DOMException;
class HTMLFormElement;
class HistoryItem;
class KURL;
class SerializedScriptValue;

// TODO(japhet): This should probably move to frame_loader_types.h and possibly
// be used more broadly once it is in the HTML spec.
enum class UserNavigationInvolvement { kBrowserUI, kActivation, kNone };
enum class NavigateEventType { kFragment, kHistoryApi, kCrossDocument };

class CORE_EXPORT NavigationApi final : public EventTargetWithInlineData,
                                        public Supplement<LocalDOMWindow> {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static const char kSupplementName[];
  static NavigationApi* navigation(LocalDOMWindow&);
  // Unconditionally creates NavigationApi, even if the RuntimeEnabledFeatures
  // is disabled.
  static NavigationApi* From(LocalDOMWindow&);
  explicit NavigationApi(LocalDOMWindow&);
  ~NavigationApi() final = default;

  void InitializeForNewWindow(HistoryItem& current,
                              WebFrameLoadType,
                              CommitReason,
                              NavigationApi* previous,
                              const WebVector<WebHistoryItem>& back_entries,
                              const WebVector<WebHistoryItem>& forward_entries);
  void UpdateForNavigation(HistoryItem&, WebFrameLoadType);
  void SetEntriesForRestore(
      const mojom::blink::NavigationApiHistoryEntryArraysPtr&);

  bool HasOngoingNavigation() const { return ongoing_navigation_signal_; }

  // Web-exposed:
  NavigationHistoryEntry* currentEntry() const;
  HeapVector<Member<NavigationHistoryEntry>> entries();
  void updateCurrentEntry(NavigationUpdateCurrentEntryOptions*,
                          ExceptionState&);
  NavigationTransition* transition() const { return transition_; }

  bool canGoBack() const;
  bool canGoForward() const;

  NavigationResult* navigate(ScriptState*,
                             const String& url,
                             NavigationNavigateOptions*);
  NavigationResult* reload(ScriptState*, NavigationReloadOptions*);

  NavigationResult* traverseTo(ScriptState*,
                               const String& key,
                               NavigationOptions*);
  NavigationResult* back(ScriptState*, NavigationOptions*);
  NavigationResult* forward(ScriptState*, NavigationOptions*);

  // onnavigate is defined manually so that a UseCounter can be applied to just
  // the setter
  void setOnnavigate(EventListener* listener);
  EventListener* onnavigate() {
    return GetAttributeEventListener(event_type_names::kNavigate);
  }

  DEFINE_ATTRIBUTE_EVENT_LISTENER(navigatesuccess, kNavigatesuccess)
  DEFINE_ATTRIBUTE_EVENT_LISTENER(navigateerror, kNavigateerror)
  DEFINE_ATTRIBUTE_EVENT_LISTENER(currententrychange, kCurrententrychange)

  enum class DispatchResult { kContinue, kAbort, kTransitionWhile };
  DispatchResult DispatchNavigateEvent(const KURL& url,
                                       HTMLFormElement* form,
                                       NavigateEventType,
                                       WebFrameLoadType,
                                       UserNavigationInvolvement,
                                       SerializedScriptValue*,
                                       HistoryItem* destination_item,
                                       bool is_browser_initiated = false,
                                       bool is_synchronously_committed = true);
  void InformAboutCanceledNavigation();

  int GetIndexFor(NavigationHistoryEntry*);

  // EventTargetWithInlineData overrides:
  const AtomicString& InterfaceName() const final;
  ExecutionContext* GetExecutionContext() const final {
    return GetSupplementable();
  }

  void Trace(Visitor*) const final;

 private:
  friend class NavigateReaction;
  friend class NavigationApiNavigation;
  void CloneFromPrevious(NavigationApi&);
  NavigationHistoryEntry* GetEntryForRestore(
      const mojom::blink::NavigationApiHistoryEntryPtr&);
  void PopulateKeySet();
  void FinalizeWithAbortedNavigationError(ScriptState*,
                                          NavigationApiNavigation*);
  void ResolvePromisesAndFireNavigateSuccessEvent(NavigationApiNavigation*);
  void RejectPromisesAndFireNavigateErrorEvent(NavigationApiNavigation*,
                                               ScriptValue);

  NavigationResult* PerformNonTraverseNavigation(
      ScriptState*,
      const KURL&,
      scoped_refptr<SerializedScriptValue>,
      NavigationOptions*,
      WebFrameLoadType);

  DOMException* PerformSharedNavigationChecks(
      const String& method_name_for_error_message);

  void PromoteUpcomingNavigationToOngoing(const String& key);
  void CleanupApiNavigation(NavigationApiNavigation&);

  scoped_refptr<SerializedScriptValue> SerializeState(const ScriptValue&,
                                                      ExceptionState&);

  bool HasEntriesAndEventsDisabled() const;

  HeapVector<Member<NavigationHistoryEntry>> entries_;
  HashMap<String, int> keys_to_indices_;
  int current_entry_index_ = -1;

  Member<NavigationTransition> transition_;

  Member<NavigationApiNavigation> ongoing_navigation_;
  HeapHashMap<String, Member<NavigationApiNavigation>> upcoming_traversals_;
  Member<NavigationApiNavigation> upcoming_non_traversal_navigation_;

  Member<NavigateEvent> ongoing_navigate_event_;
  Member<AbortSignal> ongoing_navigation_signal_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_NAVIGATION_API_NAVIGATION_API_H_
