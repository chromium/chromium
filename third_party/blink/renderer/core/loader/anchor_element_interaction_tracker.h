// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LOADER_ANCHOR_ELEMENT_INTERACTION_TRACKER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LOADER_ANCHOR_ELEMENT_INTERACTION_TRACKER_H_

#include "third_party/blink/public/mojom/loader/anchor_element_interaction_host.mojom-blink.h"
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
class AnchorElementInteractionTracker
    : public GarbageCollected<AnchorElementInteractionTracker> {
 public:
  explicit AnchorElementInteractionTracker(Document& document);

  static bool IsFeatureEnabled();

  void OnPointerDown(EventTarget& target, const PointerEvent& pointer_event);

  void Trace(Visitor* visitor) const;

 private:
  HTMLAnchorElement* FirstAnchorElementIncludingSelf(Node* node);

  // Gets the `anchor's` href attribute if it is part
  // of the HTTP family
  KURL GetHrefEligibleForPreloading(const HTMLAnchorElement& anchor);

  HeapMojoRemote<mojom::blink::AnchorElementInteractionHost> interaction_host_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LOADER_ANCHOR_ELEMENT_INTERACTION_TRACKER_H_
