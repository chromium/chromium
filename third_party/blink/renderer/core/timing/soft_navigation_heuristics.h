// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_TIMING_SOFT_NAVIGATION_HEURISTICS_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_TIMING_SOFT_NAVIGATION_HEURISTICS_H_

#include <optional>

#include "base/functional/function_ref.h"
#include "base/gtest_prod_util.h"
#include "base/memory/stack_allocated.h"
#include "base/unguessable_token.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/web/web_frame_load_type.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/paint/timing/lcp_objects.h"
#include "third_party/blink/renderer/core/paint/timing/paint_timing.h"
#include "third_party/blink/renderer/core/timing/performance_timeline_entry_id_generator.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_set.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/scheduler/public/task_attribution_tracker.h"

namespace blink {
class Element;
class InteractionEffectsMonitor;
class HTMLVideoElement;
class PerformanceEventTiming;
class QualifiedName;
class SoftNavigationContext;
class SoftNavigationPaintAttributionTracker;

// This class contains the logic for calculating Single-Page-App soft navigation
// heuristics. See https://github.com/WICG/soft-navigations
class CORE_EXPORT SoftNavigationHeuristics
    : public GarbageCollected<SoftNavigationHeuristics> {
 public:
  FRIEND_TEST_ALL_PREFIXES(SoftNavigationHeuristicsTest,
                           EarlyReturnOnInvalidPendingInteractionTimestamp);

  // Returns a TaskScope for the given EventTiming entry if it has an
  // interactionId.
  std::optional<scheduler::TaskAttributionTracker::TaskScope>
  MaybeCreateTaskScopeForEvent(PerformanceEventTiming* entry);

  explicit SoftNavigationHeuristics(LocalDOMWindow* window);
  virtual ~SoftNavigationHeuristics() = default;

  static SoftNavigationHeuristics* CreateIfNeeded(LocalDOMWindow*);

  // Inform `SoftNavigationHeuristics` that `inserted_node` was inserted into
  // `container_node`. Sets up paint tracking if the modification is
  // attributable to a `SoftNavigationContext` and connected to the DOM.
  static void InsertedNode(Node* inserted_node, Node* container_node);

  // Inform `SoftNavigationHeuristics` that `node` was modified in some way.
  // Sets up paint tracking if the modification is attributable to a
  // `SoftNavigationContext` and connected to the DOM, in which case this
  // returns true.
  static bool ModifiedNode(Node* node);

  // Inform `SoftNavigationHeuristics` that the `attribute` for the given
  // `Element` changed. Sets up paint tracking if the modification is
  // attributable to a `SoftNavigationContext`, the node is connected to the
  // DOM, and the attribute is part of the heuristic.
  static void ModifiedAttribute(Element*, const QualifiedName& attribute);

  // Inform `SoftNavigationHeuristics` that the "src" attribute for the video
  // element changed. Sets up paint tracking if the modification is attributable
  // to a `SoftNavigationContext` and connected to the DOM.
  static void OnVideoSrcChanged(HTMLVideoElement*);

  // GarbageCollected boilerplate.
  void Trace(Visitor*) const;

  void Shutdown();

  // Called by the navigation stack when a same-document navigation has been
  // committed and the URL has changed. Can be called from a synchronous
  // navigation, e.g. pushState(), or an async navigation, e.g. a history.back()
  // continuation. In either case, the appropriate TaskAttribution task state
  // will be set before this is called.
  void SameDocumentNavigationCommitted(
      const KURL& old_url,
      const KURL& new_url,
      WebFrameLoadType,
      base::UnguessableToken same_document_metrics_token,
      PerformanceTimelineEntryIdInfo interaction_id);

  bool ModifiedDOM(Node* node);
  uint64_t SoftNavigationCount() { return soft_navigation_count_; }

  SoftNavigationContext* MaybeGetSoftNavigationContextForTiming(Node* node);
  void OnPaintFinished();
  void OnInputOrScroll();
  void UpdateSoftLcpCandidate();

  SoftNavigationPaintAttributionTracker* GetPaintAttributionTracker() {
    return paint_attribution_tracker_.Get();
  }

  bool IsTrackingSoftNavigationsForTest() const {
    return !interaction_id_to_context_.empty();
  }

  void RegisterInteractionEffectsMonitor(InteractionEffectsMonitor*);
  void UnregisterInteractionEffectsMonitor(InteractionEffectsMonitor*);
  void ForEachInteractionEffectsMonitor(
      base::FunctionRef<void(InteractionEffectsMonitor&)>);

  void OnContextDisposed(SoftNavigationContext*);
  void UpdateSoftLcpMetricsForContext(SoftNavigationContext*);

 private:
  // For new Interactions, we unconditionally use the Interaction id to map to
  // the right Context for this interaction id.  If a Context has not yet been
  // created (or has already been gc-ed) this will create a new context.
  SoftNavigationContext* GetSoftNavigationContextForInteractionId(
      PerformanceTimelineEntryIdInfo interaction_id) const;

  // For continuations, we unconditionally use the Context for the current task.
  SoftNavigationContext* GetSoftNavigationContextForCurrentTask() const;

  // For SameDocumentNavigationCommit, we may be observing a new Interaction or
  // running a continuation.  Sometimes we may be running a continuation but
  // still have access to a navigate's Interaction id and so have both.  This
  // uses either available method to get the right Context-- and checks that
  // its the same Context if both methods are available to use.
  SoftNavigationContext* GetRelevantContextForNavigation(
      std::optional<PerformanceTimelineEntryIdInfo> interaction_id =
          std::nullopt) const;

  void ReportSoftNavigationToMetrics(SoftNavigationContext*) const;
  void SetIsTrackingSoftNavigationHeuristicsOnDocument(bool value) const;

  // Commits the navigation, assigning the context a new navigation ID, if the
  // context has met all of the criteria for a soft navigation and it has not
  // already committed. Emits a SoftNavigationEntry if the navigation was
  // committed and the context's first contentful paint has its presentation
  // time.
  void MaybeCommitNavigationOrEmitSoftNavigationEntry(SoftNavigationContext*);

  // Emits the SoftNavigationEntry for the context. The context must have an
  // associated committed navigation and first contentful paint timestamp when
  // this is called, and it must not have already been emitted.
  void EmitSoftNavigationEntry(SoftNavigationContext*);

  uint64_t CalculateRequiredPaintArea() const;
  uint64_t CalculateViewportArea() const;

  Member<LocalDOMWindow> window_;

  // Map from interaction ID to the associated SoftNavigationContext.
  HeapHashMap<uint64_t, WeakMember<SoftNavigationContext>>
      interaction_id_to_context_;

  // Save a strong reference to the most recent context that changed URL.  This
  // context could still be pending (not emitted) as we wait to observe more
  // paints, or it might have already been emitted, but we still want to
  // continue measuring paints for a while.
  Member<SoftNavigationContext> context_for_current_url_;

  // `SoftNavigationContext`s that have met all of the soft nav criteria but
  // haven't emitted the performance entry because they're waiting for
  // presentation feedback for FCP. Tracking these ensures we always emit an
  // entry when we update the navigation ID, which might not be the case if the
  // URL changes and presentation feedback is delayed.
  HeapHashSet<Member<SoftNavigationContext>>
      contexts_waiting_for_paint_timestamp_;

  // Used to map DOM modifications to `SoftNavigationContext`s for paint
  // attribution. Only set when `IsPrePaintBasedAttributionEnabled()` is true.
  Member<SoftNavigationPaintAttributionTracker> paint_attribution_tracker_;

  HeapHashSet<Member<InteractionEffectsMonitor>> interaction_effects_monitors_;

  // This count is incremented when a soft navigation is sent to the
  // frame client for reporting, so that it will be monotonically increasing
  // as it arrives in the browser process.
  uint64_t soft_navigation_count_ = 0;

  // `task_attribution_tracker_` is cleared during `Shutdown()` (frame detach),
  // which should happen before the tracker is destroyed, since its lifetime is
  // tied to the lifetime of the isolate/main thread.
  scheduler::TaskAttributionTracker* task_attribution_tracker_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_TIMING_SOFT_NAVIGATION_HEURISTICS_H_
