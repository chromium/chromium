// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_HTML_MEDIA_LAZY_LOAD_MEDIA_OBSERVER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_HTML_MEDIA_LAZY_LOAD_MEDIA_OBSERVER_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_vector.h"
#include "third_party/blink/renderer/platform/heap/forward.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/member.h"

namespace blink {

class Document;
class Element;
class IntersectionObserver;
class IntersectionObserverEntry;

// Shared observer for lazy loading of images, video, and audio elements.
// Uses a single IntersectionObserver to detect when any lazy-loaded element
// approaches the viewport and triggers loading.
class CORE_EXPORT LazyLoadMediaObserver final
    : public GarbageCollected<LazyLoadMediaObserver> {
 public:
  LazyLoadMediaObserver() = default;

  // Start monitoring an element for viewport intersection.
  void StartMonitoringNearViewport(Document* root_document, Element* element);

  // Stop monitoring an element.
  void StopMonitoring(Element* element);

  void Trace(Visitor*) const;

  // Loads all currently known lazy-loaded images and blocks the load event.
  // Returns whether any resources started loading as a result.
  bool LoadAllImagesAndBlockLoadEvent(Document& for_document);

 private:
  // Callback invoked when observed elements intersect or stop intersecting
  // with the viewport margin.
  void LoadIfNearViewport(const HeapVector<Member<IntersectionObserverEntry>>&);

  // Get the lazy loading margin based on network connection type.
  int GetLazyLoadingMarginPx(const Document& document);

  // The intersection observer responsible for loading elements once they're
  // near the viewport.
  Member<IntersectionObserver> lazy_load_intersection_observer_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_HTML_MEDIA_LAZY_LOAD_MEDIA_OBSERVER_H_
