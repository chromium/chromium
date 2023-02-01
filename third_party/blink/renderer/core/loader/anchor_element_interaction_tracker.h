// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LOADER_ANCHOR_ELEMENT_INTERACTION_TRACKER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LOADER_ANCHOR_ELEMENT_INTERACTION_TRACKER_H_

#include "base/metrics/field_trial_params.h"
#include "third_party/blink/public/mojom/preloading/anchor_element_interaction_host.mojom-blink.h"
#include "third_party/blink/renderer/core/html/html_anchor_element.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_remote.h"

namespace blink {

class Document;
class EventTarget;
class HTMLAnchorElement;
class KURL;
class Node;
class PointerEvent;

// Tracks pointerdown events anywhere on a document.  On receiving a pointerdown
// event, the tracker will retrieve the valid href from the anchor element from
// the event and will report the href value to the browser process via Mojo. The
// browser process can use this information to preload (e.g. preconnect to the
// origin) the URL in order to improve performance.
class BLINK_EXPORT AnchorElementInteractionTracker
    : public GarbageCollected<AnchorElementInteractionTracker> {
 public:
  explicit AnchorElementInteractionTracker(Document& document);
  ~AnchorElementInteractionTracker();

  static bool IsFeatureEnabled();
  static base::TimeDelta GetHoverDwellTime();

  void OnPointerEvent(EventTarget& target, const PointerEvent& pointer_event);
  void HoverTimerFired(TimerBase*);
  void Trace(Visitor* visitor) const;
  void FireHoverTimerForTesting();
  void SetTickClockForTesting(const base::TickClock* clock);
  Document* GetDocument() { return document_; }

 private:
  HTMLAnchorElement* FirstAnchorElementIncludingSelf(Node* node);

  // Gets the `anchor's` href attribute if it is part
  // of the HTTP family
  KURL GetHrefEligibleForPreloading(const HTMLAnchorElement& anchor);

  HeapMojoRemote<mojom::blink::AnchorElementInteractionHost> interaction_host_;
  // This hash map contains anchor element's url and the timetick at which a
  // hover event should be reported if not cancelled.
  HashMap<KURL, base::TimeTicks> hover_events_;
  HeapTaskRunnerTimer<AnchorElementInteractionTracker> hover_timer_;
  const base::TickClock* clock_;
  Member<Document> document_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LOADER_ANCHOR_ELEMENT_INTERACTION_TRACKER_H_
