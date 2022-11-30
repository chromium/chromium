// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_HTML_ANCHOR_ELEMENT_METRICS_SENDER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_HTML_ANCHOR_ELEMENT_METRICS_SENDER_H_

#include "third_party/blink/public/mojom/loader/navigation_predictor.mojom-blink.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_set.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_remote.h"
#include "third_party/blink/renderer/platform/supplementable.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

class Document;
class HTMLAnchorElement;
class IntersectionObserver;
class IntersectionObserverEntry;
class AnchorElementMetrics;

// AnchorElementMetricsSender is responsible to send anchor element metrics to
// the browser process for a given document.
//
// The high level approach is:
// 1) When HTMLAnchorElements are inserted into the DOM,
//    AnchorElementMetricsSender::AddAnchorElement is called and a reference to
//    the element is stored. We also add the element to the intersection
//    observer that watches for elements entering the viewport. The first time
//    this happens, the sender is created, which registers itself for lifecycle
//    callbacks.
// 2) If any elements enter the viewport, the intersection observer will call
//    AnchorElementMetricsSender::UpdateVisibleAnchors. Elements are collected
//    in entered_viewport_messages_ and will be reported after the next layout.
// 3) On the next layout, AnchorElementMetricsSender::DidFinishLifecycleUpdate
//    is called, and it goes over the collected anchor elements. Elements that
//    are visible are reported to the browser via ReportNewAnchorElements. The
//    anchor elements collected in AnchorElementMetricsSender are all dropped.
//    In particular, this drops elements that are not visible. They will never
//    be reported even if they become visible later, unless the are reinserted
//    into the DOM. This is not ideal, but simpler, keeps resource usage low,
//    and seems to work well enough on the sites I've looked at. Also, elements
//    that entered the viewport will be reported using
//    ReportAnchorElementsEnteredViewport.
class CORE_EXPORT AnchorElementMetricsSender final
    : public GarbageCollected<AnchorElementMetricsSender>,
      public LocalFrameView::LifecycleNotificationObserver,
      public Supplement<Document> {
 public:
  static const char kSupplementName[];

  explicit AnchorElementMetricsSender(Document&);
  AnchorElementMetricsSender(const AnchorElementMetricsSender&) = delete;
  AnchorElementMetricsSender& operator=(const AnchorElementMetricsSender&) =
      delete;
  virtual ~AnchorElementMetricsSender();

  // LocalFrameView::LifecycleNotificationObserver
  void WillStartLifecycleUpdate(const LocalFrameView&) override {}
  void DidFinishLifecycleUpdate(
      const LocalFrameView& local_frame_view) override;

  // Returns the anchor element metrics sender of the root document of
  // |Document|. Constructs new one if it does not exist.
  static AnchorElementMetricsSender* From(Document&);

  // Returns true if |document| should have associated
  // AnchorElementMetricsSender.
  static bool HasAnchorElementMetricsSender(Document& document);

  // Report the link click to the browser process, so long as the anchor
  // is an HTTP(S) link.
  void MaybeReportClickedMetricsOnClick(
      const HTMLAnchorElement& anchor_element);

  // Adds an anchor element to |anchor_elements_|.
  void AddAnchorElement(HTMLAnchorElement& element);

  HeapMojoRemote<mojom::blink::AnchorElementMetricsHost>* MetricsHost();

  // Creates AnchorElementMetrics from anchor element if possible. Then records
  // the metrics, and sends them to the browser process.
  void UpdateVisibleAnchors(
      const HeapVector<Member<IntersectionObserverEntry>>& entries);

  void Trace(Visitor*) const override;

 private:
  // Associates |metrics_host_| with the IPC interface if not already, so it can
  // be used to send messages. Returns true if associated, false otherwise.
  bool AssociateInterface();

  // Creates an AnchorElementEnteredViewportPtr for the given element and
  // enqueue it so that it gets reported after the next layout.
  void EnqueueEnteredViewport(const HTMLAnchorElement& element);

  // Use WeakMember to make sure we don't leak memory on long-lived pages.
  HeapHashSet<WeakMember<HTMLAnchorElement>> anchor_elements_to_report_;

  HeapMojoRemote<mojom::blink::AnchorElementMetricsHost> metrics_host_;

  Member<IntersectionObserver> intersection_observer_;

  WTF::Vector<mojom::blink::AnchorElementEnteredViewportPtr>
      entered_viewport_messages_;
  WTF::Vector<mojom::blink::AnchorElementClickPtr> clicked_messages_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_HTML_ANCHOR_ELEMENT_METRICS_SENDER_H_
