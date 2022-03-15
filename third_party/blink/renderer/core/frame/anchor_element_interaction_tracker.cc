// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/anchor_element_interaction_tracker.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/event_type_names.h"
#include "third_party/blink/renderer/core/frame/anchor_element_listener.h"

namespace blink {

AnchorElementInteractionTracker::AnchorElementInteractionTracker(
    Document& document) {
  anchor_element_listener_ = MakeGarbageCollected<AnchorElementListener>();
  document.addEventListener(event_type_names::kPointerdown,
                            anchor_element_listener_, true);
}

void AnchorElementInteractionTracker::Trace(Visitor* visitor) const {
  visitor->Trace(anchor_element_listener_);
}

// static
bool AnchorElementInteractionTracker::IsFeatureEnabled() {
  return base::FeatureList::IsEnabled(features::kAnchorElementInteraction);
}

}  // namespace blink
