// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_APP_HISTORY_APP_HISTORY_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_APP_HISTORY_APP_HISTORY_H_

#include "base/memory/scoped_refptr.h"
#include "third_party/blink/public/web/web_frame_load_type.h"
#include "third_party/blink/public/web/web_history_item.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/dom/events/event_target.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_vector.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/supplementable.h"

namespace blink {

class AbortSignal;
class AppHistoryApiNavigation;
class AppHistoryUpdateCurrentOptions;
class AppHistoryEntry;
class AppHistoryNavigateEvent;
class AppHistoryNavigateOptions;
class AppHistoryReloadOptions;
class AppHistoryResult;
class AppHistoryNavigationOptions;
class AppHistoryTransition;
class DOMException;
class HTMLFormElement;
class HistoryItem;
class KURL;
class SerializedScriptValue;

// TODO(japhet): This should probably move to frame_loader_types.h and possibly
// be used more broadly once it is in the HTML spec.
enum class UserNavigationInvolvement { kBrowserUI, kActivation, kNone };
enum class NavigateEventType { kFragment, kHistoryApi, kCrossDocument };

class CORE_EXPORT AppHistory final : public EventTargetWithInlineData,
                                     public Supplement<LocalDOMWindow> {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static const char kSupplementName[];
  static AppHistory* appHistory(LocalDOMWindow&);
  // Unconditionally creates AppHistory, even if the RuntimeEnabledFeatures is
  // disabled.
  static AppHistory* From(LocalDOMWindow&);
  explicit AppHistory(LocalDOMWindow&);
  ~AppHistory() final = default;

  void InitializeForNewWindow(HistoryItem& current,
                              WebFrameLoadType,
                              CommitReason,
                              AppHistory* previous,
                              const WebVector<WebHistoryItem>& back_entries,
                              const WebVector<WebHistoryItem>& forward_entries);
  void UpdateForNavigation(HistoryItem&, WebFrameLoadType);

  bool HasOngoingNavigation() const { return ongoing_navigation_signal_; }

  // Web-exposed:
  AppHistoryEntry* current() const;
  HeapVector<Member<AppHistoryEntry>> entries();
  void updateCurrent(AppHistoryUpdateCurrentOptions*, ExceptionState&);
  AppHistoryTransition* transition() const { return transition_; }

  bool canGoBack() const;
  bool canGoForward() const;

  AppHistoryResult* navigate(ScriptState*,
                             const String& url,
                             AppHistoryNavigateOptions*);
  AppHistoryResult* reload(ScriptState*, AppHistoryReloadOptions*);

  AppHistoryResult* goTo(ScriptState*,
                         const String& key,
                         AppHistoryNavigationOptions*);
  AppHistoryResult* back(ScriptState*, AppHistoryNavigationOptions*);
  AppHistoryResult* forward(ScriptState*, AppHistoryNavigationOptions*);

  // onnavigate is defined manually so that a UseCounter can be applied to just
  // the setter
  void setOnnavigate(EventListener* listener);
  EventListener* onnavigate() {
    return GetAttributeEventListener(event_type_names::kNavigate);
  }

  DEFINE_ATTRIBUTE_EVENT_LISTENER(navigatesuccess, kNavigatesuccess)
  DEFINE_ATTRIBUTE_EVENT_LISTENER(navigateerror, kNavigateerror)
  DEFINE_ATTRIBUTE_EVENT_LISTENER(currentchange, kCurrentchange)

  enum class DispatchResult { kContinue, kAbort, kTransitionWhile };
  DispatchResult DispatchNavigateEvent(const KURL& url,
                                       HTMLFormElement* form,
                                       NavigateEventType,
                                       WebFrameLoadType,
                                       UserNavigationInvolvement,
                                       SerializedScriptValue* = nullptr,
                                       HistoryItem* destination_item = nullptr);
  void InformAboutCanceledNavigation();

  int GetIndexFor(AppHistoryEntry*);

  // EventTargetWithInlineData overrides:
  const AtomicString& InterfaceName() const final;
  ExecutionContext* GetExecutionContext() const final {
    return GetSupplementable();
  }

  void Trace(Visitor*) const final;

 private:
  friend class NavigateReaction;
  friend class AppHistoryApiNavigation;
  void CloneFromPrevious(AppHistory&);
  void PopulateKeySet();
  void FinalizeWithAbortedNavigationError(ScriptState*,
                                          AppHistoryApiNavigation*);
  void RejectPromiseAndFireNavigateErrorEvent(AppHistoryApiNavigation*,
                                              ScriptValue);

  AppHistoryResult* PerformNonTraverseNavigation(
      ScriptState*,
      const KURL&,
      scoped_refptr<SerializedScriptValue>,
      AppHistoryNavigationOptions*,
      WebFrameLoadType);

  DOMException* PerformSharedNavigationChecks(
      const String& method_name_for_error_message);

  void PromoteUpcomingNavigationToOngoing(const String& key);
  void CleanupApiNavigation(AppHistoryApiNavigation&);

  scoped_refptr<SerializedScriptValue> SerializeState(const ScriptValue&,
                                                      ExceptionState&);

  bool HasEntriesAndEventsDisabled() const;

  HeapVector<Member<AppHistoryEntry>> entries_;
  HashMap<String, int> keys_to_indices_;
  int current_index_ = -1;

  Member<AppHistoryTransition> transition_;

  Member<AppHistoryApiNavigation> ongoing_navigation_;
  HeapHashMap<String, Member<AppHistoryApiNavigation>> upcoming_traversals_;
  Member<AppHistoryApiNavigation> upcoming_non_traversal_navigation_;

  Member<AppHistoryNavigateEvent> ongoing_navigate_event_;
  Member<AbortSignal> ongoing_navigation_signal_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_APP_HISTORY_APP_HISTORY_H_
