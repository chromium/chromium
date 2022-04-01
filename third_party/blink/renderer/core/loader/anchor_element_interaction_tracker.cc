// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/loader/anchor_element_interaction_tracker.h"
#include "third_party/blink/public/common/browser_interface_broker_proxy.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/loader/anchor_element_interaction_host.mojom-blink.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/event_type_names.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/loader/anchor_element_listener.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"

namespace blink {

AnchorElementInteractionTracker::AnchorElementInteractionTracker(
    Document& document)
    : interaction_host_(document.GetExecutionContext()) {
  base::RepeatingCallback<void(const KURL&)> callback =
      WTF::BindRepeating(&AnchorElementInteractionTracker::OnPointerDown,
                         WrapWeakPersistent(this));

  anchor_element_listener_ =
      MakeGarbageCollected<AnchorElementListener>(callback);

  document.addEventListener(event_type_names::kPointerdown,
                            anchor_element_listener_, true);

  document.GetFrame()->GetBrowserInterfaceBroker().GetInterface(
      interaction_host_.BindNewPipeAndPassReceiver(
          document.GetExecutionContext()->GetTaskRunner(
              TaskType::kInternalDefault)));
}

void AnchorElementInteractionTracker::Trace(Visitor* visitor) const {
  visitor->Trace(anchor_element_listener_);
  visitor->Trace(interaction_host_);
}

// static
bool AnchorElementInteractionTracker::IsFeatureEnabled() {
  return base::FeatureList::IsEnabled(features::kAnchorElementInteraction);
}

void AnchorElementInteractionTracker::OnPointerDown(const KURL& url) {
  interaction_host_->OnPointerDown(url);
}

}  // namespace blink
