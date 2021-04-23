// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_HTML_ANCHOR_ELEMENT_METRICS_SENDER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_HTML_ANCHOR_ELEMENT_METRICS_SENDER_H_

#include "base/macros.h"
#include "third_party/blink/public/mojom/loader/navigation_predictor.mojom-blink.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/platform/geometry/int_size.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_remote.h"
#include "third_party/blink/renderer/platform/supplementable.h"
#include "third_party/blink/renderer/platform/wtf/hash_set.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

class Document;
class HTMLAnchorElement;

// AnchorElementMetricsSender is responsible to send anchor element metrics to
// the browser process for a given document.
class CORE_EXPORT AnchorElementMetricsSender final
    : public GarbageCollected<AnchorElementMetricsSender>,
      public LocalFrameView::LifecycleNotificationObserver,
      public Supplement<Document> {
 public:
  static const char kSupplementName[];

  explicit AnchorElementMetricsSender(Document&);
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

  // Sends metrics of anchor element clicked by the user to the browser.
  void SendClickedAnchorMetricsToBrowser(
      mojom::blink::AnchorElementMetricsPtr metric);

  // Sends metrics of visible anchor elements to the browser.
  void SendAnchorMetricsVectorToBrowser(
      Vector<mojom::blink::AnchorElementMetricsPtr> metrics,
      const IntSize& viewport_size);

  // Adds an anchor element to |anchor_elements_|.
  void AddAnchorElement(HTMLAnchorElement& element);

  // Returns the stored |anchor_elements_|.
  const HeapHashSet<Member<HTMLAnchorElement>>& GetAnchorElements() const;

  void Trace(Visitor*) const override;

 private:
  // Associates |metrics_host_| with the IPC interface if not already, so it can
  // be used to send messages. Returns true if associated, false otherwise.
  bool AssociateInterface();

  // Browser host to which the anchor element metrics are sent.
  HeapMojoRemote<mojom::blink::AnchorElementMetricsHost> metrics_host_;

  // Collection of anchor elements in the document. Use a HashSet to ensure that
  // an element is inserted at most once.
  HeapHashSet<Member<HTMLAnchorElement>> anchor_elements_;

  // If |has_onload_report_sent_| is true, |anchor_elements_| will not accept
  // new anchor elements.
  bool has_onload_report_sent_ = false;

  DISALLOW_COPY_AND_ASSIGN(AnchorElementMetricsSender);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_HTML_ANCHOR_ELEMENT_METRICS_SENDER_H_
