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
class AppHistoryNavigateOptions;
class HTMLFormElement;
class HistoryItem;
class KURL;
class ScriptPromise;
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

  // EventTargetWithInlineData overrides:
  const AtomicString& InterfaceName() const final;
  ExecutionContext* GetExecutionContext() const final {
    return GetSupplementable();
  }

  void Trace(Visitor*) const final;

 private:
  HeapVector<Member<AppHistoryEntry>> entries_;
  int current_index_ = -1;

  ScriptValue navigate_event_info_;
  ScriptPromise navigate_method_call_promise_;

  // When navigate() is called and a cross-document navigation is pending, we
  // want to return an unresolved promise. But the promise will never resolve,
  // because the navigation won't be "done" until the navigaton commits and this
  // context is detached. Therefore, use this resolver to create a promise that
  // never resolves. The resolver needs to be per-AppHistory so that it isn't
  // GCed until window detach, because ScriptPromiseResolver DCHECKs if it is
  // GCed without resolving unless the window is detached.
  Member<ScriptPromiseResolver> hung_promise_resolver_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_APP_HISTORY_APP_HISTORY_H_
