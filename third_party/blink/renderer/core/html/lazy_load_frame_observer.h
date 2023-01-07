// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_HTML_LAZY_LOAD_FRAME_OBSERVER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_HTML_LAZY_LOAD_FRAME_OBSERVER_H_

#include <memory>

#include "base/time/time.h"
#include "third_party/blink/public/web/web_frame_load_type.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_vector.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/heap/visitor.h"

namespace blink {

class IntersectionObserver;
class IntersectionObserverEntry;
class HTMLFrameOwnerElement;
class ResourceRequestHead;

class LazyLoadFrameObserver final
    : public GarbageCollected<LazyLoadFrameObserver> {
 public:
  // This enum is logged to histograms, so values should not be reordered or
  // reused, and it must match the corresponding enum
  // "LazyLoad.FrameInitialDeferralAction" in
  // tools/metrics/histograms/enums.xml.
  enum class FrameInitialDeferralAction {
    // The frame was not loaded immediately, and the load continued to be
    // deferred.
    kDeferred = 0,
    // The frame was either visible or near enough to the viewport that it was
    // loaded immediately.
    kLoadedNearOrInViewport = 1,
    // The frame was determined to likely be a hidden frame (e.g. analytics or
    // communication iframes), so it was loaded immediately.
    kLoadedHidden = 2,

    kMaxValue = kLoadedHidden
  };

  // The loading pipeline for an iframe differs depending on whether the
  // navigation is its first, or a dynamic / subsequent one. Since the iframe
  // loading path differs, LazyLoadFrameObserver must also account for this
  // difference when loading a deferred frame. This enum helps us keep track of
  // that so we can do the right thing.
  enum class LoadType {
    kFirst,
    kSubsequent,
  };

  LazyLoadFrameObserver(HTMLFrameOwnerElement&, LoadType);
  ~LazyLoadFrameObserver();

  void DeferLoadUntilNearViewport(const ResourceRequestHead&, WebFrameLoadType);
  bool IsLazyLoadPending() const { return lazy_load_intersection_observer_; }
  void CancelPendingLazyLoad();

  void StartTrackingVisibilityMetrics();
  void RecordMetricsOnLoadFinished();

  void LoadImmediately();

  void Trace(Visitor*) const;

 private:
  struct LazyLoadRequestInfo;

  void LoadIfHiddenOrNearViewport(
      const HeapVector<Member<IntersectionObserverEntry>>&);

  void RecordMetricsOnVisibilityChanged(
      const HeapVector<Member<IntersectionObserverEntry>>&);

  void RecordVisibilityMetricsIfLoadedAndVisible();
  void RecordInitialDeferralAction(FrameInitialDeferralAction);

  const Member<HTMLFrameOwnerElement> element_;

  // The intersection observer responsible for loading the frame once it's near
  // the viewport.
  Member<IntersectionObserver> lazy_load_intersection_observer_;

  // Keeps track of the resource request and other info needed to load in the
  // deferred frame. This is only non-null if there's a lazy load pending.
  std::unique_ptr<LazyLoadRequestInfo> lazy_load_request_info_;

  // Used to record visibility-related metrics related to lazy load. This is an
  // IntersectionObserver instead of just an ElementVisibilityObserver so that
  // hidden frames can be detected in order to avoid recording metrics for them.
  Member<IntersectionObserver> visibility_metrics_observer_;

  // Keeps track of whether this frame was initially visible on the page.
  bool is_initially_above_the_fold_ = false;
  bool has_above_the_fold_been_set_ = false;

  // Set when the frame first becomes visible (i.e. appears in the viewport).
  base::TimeTicks time_when_first_visible_;
  // Set when the first load event is dispatched for the frame.
  base::TimeTicks time_when_first_load_finished_;

  // Keeps track of whether the frame was initially recorded as having been
  // deferred, so that the appropriate histograms can be recorded if the frame
  // later gets loaded in for some reason.
  bool was_recorded_as_deferred_ = false;

  LoadType load_type_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_HTML_LAZY_LOAD_FRAME_OBSERVER_H_
