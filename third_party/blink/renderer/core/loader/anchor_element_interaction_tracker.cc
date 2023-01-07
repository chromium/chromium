// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/loader/anchor_element_interaction_tracker.h"
#include "third_party/blink/public/common/browser_interface_broker_proxy.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/input/web_pointer_properties.h"
#include "third_party/blink/public/mojom/loader/anchor_element_interaction_host.mojom-blink.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/event_type_names.h"
#include "third_party/blink/renderer/core/events/pointer_event.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"

namespace blink {

AnchorElementInteractionTracker::AnchorElementInteractionTracker(
    Document& document)
    : interaction_host_(document.GetExecutionContext()) {
  document.GetFrame()->GetBrowserInterfaceBroker().GetInterface(
      interaction_host_.BindNewPipeAndPassReceiver(
          document.GetExecutionContext()->GetTaskRunner(
              TaskType::kInternalDefault)));
}

void AnchorElementInteractionTracker::Trace(Visitor* visitor) const {
  visitor->Trace(interaction_host_);
}

// static
bool AnchorElementInteractionTracker::IsFeatureEnabled() {
  return base::FeatureList::IsEnabled(features::kAnchorElementInteraction);
}

void AnchorElementInteractionTracker::OnPointerDown(
    EventTarget& target,
    const PointerEvent& pointer_event) {
  if (!target.ToNode()) {
    return;
  }
  if (!pointer_event.isPrimary()) {
    return;
  }
  // TODO(crbug.com/1297312): Check if user changed the default mouse settings
  if (pointer_event.button() !=
          static_cast<int>(WebPointerProperties::Button::kLeft) &&
      pointer_event.button() !=
          static_cast<int>(WebPointerProperties::Button::kMiddle)) {
    return;
  }
  HTMLAnchorElement* anchor = FirstAnchorElementIncludingSelf(target.ToNode());
  if (!anchor) {
    return;
  }
  KURL url = GetHrefEligibleForPreloading(*anchor);
  if (url.IsEmpty()) {
    return;
  }

  // interaction_host_ might become unbound: Android's low memory detector
  // sometimes call NotifyContextDestroyed to save memory. This unbinds mojo
  // pipes using that ExecutionContext even if those pages can still navigate.
  if (!interaction_host_.is_bound()) {
    return;
  }
  interaction_host_->OnPointerDown(url);
}

HTMLAnchorElement*
AnchorElementInteractionTracker::FirstAnchorElementIncludingSelf(Node* node) {
  HTMLAnchorElement* anchor = nullptr;
  while (node && !anchor) {
    anchor = DynamicTo<HTMLAnchorElement>(node);
    node = node->parentNode();
  }
  return anchor;
}

KURL AnchorElementInteractionTracker::GetHrefEligibleForPreloading(
    const HTMLAnchorElement& anchor) {
  KURL url = anchor.Href();
  if (url.ProtocolIsInHTTPFamily()) {
    return url;
  }
  return KURL();
}

}  // namespace blink
