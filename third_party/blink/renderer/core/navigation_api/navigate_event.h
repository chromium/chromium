// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_NAVIGATION_API_NAVIGATE_EVENT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_NAVIGATION_API_NAVIGATE_EVENT_H_

#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/web/web_frame_load_type.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/serialization/serialized_script_value.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_navigation_focus_reset.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_navigation_scroll_restoration.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/dom/focused_element_change_observer.h"
#include "third_party/blink/renderer/core/execution_context/execution_context_lifecycle_observer.h"
#include "third_party/blink/renderer/core/navigation_api/navigation_api.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_vector.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/supplementable.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"

namespace blink {

class AbortSignal;
class NavigationDestination;
class NavigateEventInit;
class NavigationTransitionWhileOptions;
class ExceptionState;
class FormData;
class ScriptPromise;

class NavigateEvent final : public Event,
                            public ExecutionContextClient,
                            public FocusedElementChangeObserver {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static NavigateEvent* Create(ExecutionContext* context,
                               const AtomicString& type,
                               NavigateEventInit* init) {
    return MakeGarbageCollected<NavigateEvent>(context, type, init);
  }

  NavigateEvent(ExecutionContext* context,
                const AtomicString& type,
                NavigateEventInit* init);

  void SetUrl(const KURL& url) { url_ = url; }

  String navigationType() { return navigation_type_; }
  NavigationDestination* destination() { return destination_; }
  bool canTransition() const { return can_transition_; }
  bool userInitiated() const { return user_initiated_; }
  bool hashChange() const { return hash_change_; }
  AbortSignal* signal() { return signal_; }
  FormData* formData() const { return form_data_; }
  String downloadRequest() const { return download_request_; }
  ScriptValue info() const { return info_; }

  void transitionWhile(ScriptState*,
                       ScriptPromise newNavigationAction,
                       NavigationTransitionWhileOptions*,
                       ExceptionState&);

  void restoreScroll(ExceptionState&);
  void RestoreScrollAfterTransitionIfNeeded();

  const HeapVector<ScriptPromise>& GetNavigationActionPromisesList() {
    return navigation_action_promises_list_;
  }
  void ResetFocusIfNeeded();
  bool ShouldSendAxEvents() const;

  void SaveStateFromDestinationItem(HistoryItem*);

  // FocusedElementChangeObserver implementation:
  void DidChangeFocus() final;

  const AtomicString& InterfaceName() const final;
  void Trace(Visitor*) const final;

 private:
  void RestoreScrollInternal();
  bool InManualScrollRestorationMode();

  String navigation_type_;
  Member<NavigationDestination> destination_;
  bool can_transition_;
  bool user_initiated_;
  bool hash_change_;
  Member<AbortSignal> signal_;
  Member<FormData> form_data_;
  String download_request_;
  ScriptValue info_;
  absl::optional<V8NavigationFocusReset> focus_reset_behavior_ = absl::nullopt;
  absl::optional<V8NavigationScrollRestoration> scroll_restoration_behavior_ =
      absl::nullopt;
  absl::optional<HistoryItem::ViewState> history_item_view_state_;

  KURL url_;
  HeapVector<ScriptPromise> navigation_action_promises_list_;

  enum class ManualRestoreState { kNotRestored, kRestored, kDone };
  ManualRestoreState restore_state_ = ManualRestoreState::kNotRestored;
  bool did_change_focus_during_transition_while_ = false;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_NAVIGATION_API_NAVIGATE_EVENT_H_
