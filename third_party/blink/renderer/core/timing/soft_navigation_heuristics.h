// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_TIMING_SOFT_NAVIGATION_HEURISTICS_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_TIMING_SOFT_NAVIGATION_HEURISTICS_H_

#include <optional>

#include "base/gtest_prod_util.h"
#include "base/memory/stack_allocated.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_set.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/scheduler/public/task_attribution_tracker.h"
#include "third_party/blink/renderer/platform/wtf/hash_set.h"

namespace blink {
namespace scheduler {
class TaskAttributionInfo;
}  // namespace scheduler

class ScriptState;
class SoftNavigationContext;

// This class contains the logic for calculating Single-Page-App soft navigation
// heuristics. See https://github.com/WICG/soft-navigations
class CORE_EXPORT SoftNavigationHeuristics
    : public GarbageCollected<SoftNavigationHeuristics>,
      public scheduler::TaskAttributionTracker::Observer {
 public:
  FRIEND_TEST_ALL_PREFIXES(SoftNavigationHeuristicsTest,
                           EarlyReturnOnInvalidPendingInteractionTimestamp);

  // This class defines a scope that would cover click or navigation related
  // events, in order for the SoftNavigationHeuristics class to be able to keep
  // track of them and their descendant tasks.
  class CORE_EXPORT EventScope {
    STACK_ALLOCATED();

   public:
    enum class Type {
      kKeydown,
      kKeypress,
      kKeyup,
      kClick,
      kNavigate,
      kLast = kNavigate
    };

    ~EventScope();

    EventScope(EventScope&&);
    EventScope& operator=(EventScope&&);

   private:
    using ObserverScope = scheduler::TaskAttributionTracker::ObserverScope;
    using TaskScope = scheduler::TaskAttributionTracker::TaskScope;

    friend class SoftNavigationHeuristics;

    EventScope(SoftNavigationHeuristics*,
               std::optional<ObserverScope>,
               std::optional<TaskScope>,
               Type,
               bool is_nested);

    SoftNavigationHeuristics* heuristics_;
    std::optional<ObserverScope> observer_scope_;
    std::optional<TaskScope> task_scope_;
    Type type_;
    bool is_nested_;
  };

  explicit SoftNavigationHeuristics(LocalDOMWindow* window);
  virtual ~SoftNavigationHeuristics() = default;

  static SoftNavigationHeuristics* CreateIfNeeded(LocalDOMWindow*);

  // GarbageCollected boilerplate.
  void Trace(Visitor*) const override;

  void Shutdown();

  // The class's API.

  // Returns an id to be used for retrieving the associated task state during
  // commit, or nullopt if no `SoftNavigationContext` is associated with the
  // navigation.
  std::optional<scheduler::TaskAttributionId>
  AsyncSameDocumentNavigationStarted();

  void SameDocumentNavigationCommitted(const String& url,
                                       SoftNavigationContext*);
  bool ModifiedDOM(Node* node);
  uint32_t SoftNavigationCount() { return soft_navigation_count_; }

  // TaskAttributionTracker::Observer's implementation.
  void OnCreateTaskScope(scheduler::TaskAttributionInfo&) override;

  void RecordPaint(LocalFrame*, const gfx::RectF& rect, Node* node);

  void OnPaintFinished();

  // Returns an `EventScope` suitable for navigation. Used for navigations not
  // yet associated with an event.
  EventScope CreateNavigationEventScope(ScriptState* script_state) {
    return CreateEventScope(EventScope::Type::kNavigate, script_state);
  }

  // Returns an `EventScope` for the given `Event` if the event is relevant to
  // soft navigation tracking, otherwise it returns nullopt.
  std::optional<EventScope> MaybeCreateEventScopeForEvent(const Event&);

  // This method is called during the weakness processing stage of garbage
  // collection to remove items from `potential_soft_navigations_`.
  void ProcessCustomWeakness(const LivenessBroker& info);

 private:
  void ReportSoftNavigationToMetrics(LocalFrame*, SoftNavigationContext*) const;
  void SetIsTrackingSoftNavigationHeuristicsOnDocument(bool value) const;

  // We can grab a context from the "running task", or sometimes from other
  // scheduling sources-- but these can leak across windows.
  // Any time we retrieve a context, we should check to ensure that these were
  // created for this window (i.e. by this SNH instance).
  SoftNavigationContext* EnsureContextForCurrentWindow(
      SoftNavigationContext*) const;
  SoftNavigationContext* GetSoftNavigationContextForCurrentTask() const;

  void ResetPaintTimingsIfNeeded();
  void CommitPreviousPaintTimings(LocalFrame*);
  bool EmitSoftNavigationEntryIfAllConditionsMet(SoftNavigationContext*);
  LocalFrame* GetLocalFrameIfOutermostAndNotDetached() const;
  void OnSoftNavigationEventScopeDestroyed(const EventScope&);
  EventScope CreateEventScope(EventScope::Type type, ScriptState*);
  uint64_t CalculateRequiredPaintArea() const;

  Member<LocalDOMWindow> window_;

  // The set of ongoing potential soft navigations. `SoftNavigationContext`
  // objects are added when they are the active context during an event handler
  // running in an `EventScope`. Entries are stored as untraced members to do
  // custom weak processing (see `ProcessCustomWeakness()`).
  Vector<UntracedMember<SoftNavigationContext>> potential_soft_navigations_;

  // The `SoftNavigationContext` of the "active interaction", if any.
  //
  // This is set to a new `SoftNavigationContext` when
  //   1. an `EventScope` is created for a new interaction (click, navigation,
  //      and keydown) and there isn't already an active `EventScope` on the
  //      stack for this `SoftNavigationHeuristics`. Note that the latter
  //      restriction causes the same context to be reused for nested
  //      `EventScope`s, which occur when the navigate event occurs within the
  //      scope of the input event.
  //
  //   2. an `EventScope` is created for a non-new interaction (keypress, keyup)
  //      and `active_interaction_context_` isn't set. These events typically
  //      follow a keydown, in which case the context created for that will be
  //      reused, but the context can be cleared if, for example, a click
  //      happens while a key is held.
  //
  // This is cleared when the outermost `EventScope` is destroyed if the scope
  // type is click or navigate. For keyboard events, which have multiple related
  // events, this remains alive until the next interaction.
  Member<SoftNavigationContext> active_interaction_context_;

  // Save a strong reference to the most recent soft navigation detected.  This
  // context could still be pending (not emitted) as we wait to observe more
  // paints, or it might have already been emitted, but we still want to
  // continue measuring paints for a while.
  Member<SoftNavigationContext> most_recent_context_to_meet_non_paint_criteria_;

  uint32_t soft_navigation_count_ = 0;
  bool has_active_event_scope_ = false;
  // `task_attribution_tracker_` is cleared during `Shutdown()` (frame detach),
  // which should happen before the tracker is destroyed, since its lifetime is
  // tied to the lifetime of the isolate/main thread.
  scheduler::TaskAttributionTracker* task_attribution_tracker_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_TIMING_SOFT_NAVIGATION_HEURISTICS_H_
