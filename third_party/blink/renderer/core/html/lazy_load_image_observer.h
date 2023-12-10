// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_HTML_LAZY_LOAD_IMAGE_OBSERVER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_HTML_LAZY_LOAD_IMAGE_OBSERVER_H_

#include "base/time/time.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_vector.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/heap/visitor.h"

namespace blink {

class Document;
class Element;
class HTMLImageElement;
class IntersectionObserver;
class IntersectionObserverEntry;

class LazyLoadImageObserver final
    : public GarbageCollected<LazyLoadImageObserver> {
 public:
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

  void StartMonitoringNearViewport(Document*, Element*);
  void StopMonitoring(Element*);

  void StartMonitoringVisibility(Document*, HTMLImageElement*);
  void OnLoadFinished(HTMLImageElement*);

  void Trace(Visitor*) const;

  // Loads all currently known lazy-loaded images. Returns whether any
  // resources started loading as a result.
  bool LoadAllImagesAndBlockLoadEvent(Document& for_document);

 private:
  void LoadIfNearViewport(const HeapVector<Member<IntersectionObserverEntry>>&);

  void OnVisibilityChanged(
      const HeapVector<Member<IntersectionObserverEntry>>&);

  int GetLazyLoadingImageMarginPx(const Document& document);

  // The intersection observer responsible for loading the image once it's near
  // the viewport.
  Member<IntersectionObserver> lazy_load_intersection_observer_;

  // The intersection observer used to track when the image becomes visible.
  Member<IntersectionObserver> visibility_metrics_observer_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_HTML_LAZY_LOAD_IMAGE_OBSERVER_H_
