// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_APP_HISTORY_APP_HISTORY_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_APP_HISTORY_APP_HISTORY_H_

#include "third_party/blink/public/web/web_frame_load_type.h"
#include "third_party/blink/public/web/web_history_item.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/dom/events/event_target.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/platform/heap/heap_allocator.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/supplementable.h"

namespace blink {

class AppHistoryEntry;
class AppHistoryNavigateEvent;
class AppHistoryNavigateOptions;
class HTMLFormElement;
class HistoryItem;
class KURL;
class ScriptPromise;
class ScriptPromiseResolver;
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
  explicit AppHistory(LocalDOMWindow&);
  ~AppHistory() final = default;

  void InitializeForNavigation(
      HistoryItem& current,
      const WebVector<WebHistoryItem>& back_entries,
      const WebVector<WebHistoryItem>& forward_entries);
  void UpdateForNavigation(HistoryItem&, WebFrameLoadType);
  void CloneFromPrevious(AppHistory&);

  // Web-exposed:
  AppHistoryEntry* current() const;
  HeapVector<Member<AppHistoryEntry>> entries();

  ScriptPromise navigate(ScriptState*,
                         const String& url,
                         AppHistoryNavigateOptions*,
                         ExceptionState&);
  ScriptPromise navigate(ScriptState*,
                         AppHistoryNavigateOptions*,
                         ExceptionState&);

  DEFINE_ATTRIBUTE_EVENT_LISTENER(navigate, kNavigate)
  DEFINE_ATTRIBUTE_EVENT_LISTENER(navigatesuccess, kNavigatesuccess)
  DEFINE_ATTRIBUTE_EVENT_LISTENER(navigateerror, kNavigateerror)

  // Returns true if the navigation should continue.
  bool DispatchNavigateEvent(const KURL& url,
                             HTMLFormElement* form,
                             NavigateEventType,
                             WebFrameLoadType,
                             UserNavigationInvolvement,
                             SerializedScriptValue* = nullptr);
  void CancelOngoingNavigateEvent();

  int GetIndexFor(AppHistoryEntry*);

  // EventTargetWithInlineData overrides:
  const AtomicString& InterfaceName() const final;
  ExecutionContext* GetExecutionContext() const final {
    return GetSupplementable();
  }

  void Trace(Visitor*) const final;

 private:
  void PopulateKeySet();

  HeapVector<Member<AppHistoryEntry>> entries_;
  HashMap<String, int> keys_to_indices_;
  int current_index_ = -1;

  Member<AppHistoryNavigateEvent> ongoing_navigate_event_;
  Member<ScriptPromiseResolver> navigate_method_call_promise_resolver_;
  scoped_refptr<SerializedScriptValue> navigate_serialized_state_;

  ScriptValue navigate_event_info_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_APP_HISTORY_APP_HISTORY_H_
