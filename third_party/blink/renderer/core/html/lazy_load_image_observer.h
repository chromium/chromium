// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_HTML_LAZY_LOAD_IMAGE_OBSERVER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_HTML_LAZY_LOAD_IMAGE_OBSERVER_H_

#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/heap_allocator.h"
#include "third_party/blink/renderer/platform/heap/member.h"

namespace blink {

class Document;
class Element;
class HTMLImageElement;
class IntersectionObserver;
class IntersectionObserverEntry;
class Visitor;

class LazyLoadImageObserver final
    : public GarbageCollected<LazyLoadImageObserver> {
 public:
  enum class DeferralMessage {
    kNone,
    kLoadEventsDeferred,
    kMissingDimensionForLazy
  };

  struct VisibleLoadTimeMetrics {
    // Keeps track of whether the image was initially intersecting the viewport.
    bool is_initially_intersecting = false;
    bool has_initial_intersection_been_set = false;

    // Set when the image first becomes visible (i.e. appears in the viewport).
    base::TimeTicks time_when_first_visible;

    // Set when the first load event is dispatched for the image.
    base::TimeTicks time_when_first_load_finished;
  };

  LazyLoadImageObserver(const Document&);

  void StartMonitoringNearViewport(Document*, Element*, DeferralMessage);
  void StopMonitoring(Element*);

  void StartMonitoringVisibility(Document*, HTMLImageElement*);
  void OnLoadFinished(HTMLImageElement*);

  bool IsFullyLoadableFirstKImageAndDecrementCount();

  void Trace(Visitor*);

 private:
  void LoadIfNearViewport(const HeapVector<Member<IntersectionObserverEntry>>&);

  void OnVisibilityChanged(
      const HeapVector<Member<IntersectionObserverEntry>>&);

  // The intersection observer responsible for loading the image once it's near
  // the viewport.
  Member<IntersectionObserver> lazy_load_intersection_observer_;

  // The intersection observer used to track when the image becomes visible.
  Member<IntersectionObserver> visibility_metrics_observer_;

  // Count of remaining images that can be fully loaded.
  int count_remaining_images_fully_loaded_ = 0;

  // Used to show the intervention console message one time only.
  bool is_load_event_deferred_intervention_shown_ = false;
  bool is_missing_dimension_intervention_shown_ = false;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_HTML_LAZY_LOAD_IMAGE_OBSERVER_H_
