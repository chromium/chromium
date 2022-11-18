// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/loader/anchor_element_interaction_tracker.h"
#include "third_party/blink/public/common/browser_interface_broker_proxy.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/input/web_pointer_properties.h"
#include "third_party/blink/public/mojom/preloading/anchor_element_interaction_host.mojom-blink.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/event_type_names.h"
#include "third_party/blink/renderer/core/events/pointer_event.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"

namespace blink {

AnchorElementInteractionTracker::AnchorElementInteractionTracker(
    Document& document)
    : interaction_host_(document.GetExecutionContext()),
      hover_timer_(document.GetTaskRunner(TaskType::kUserInteraction),
                   this,
                   &AnchorElementInteractionTracker::HoverTimerFired),
      clock_(base::DefaultTickClock::GetInstance()) {
  DCHECK(clock_);
  document.GetFrame()->GetBrowserInterfaceBroker().GetInterface(
      interaction_host_.BindNewPipeAndPassReceiver(
          document.GetExecutionContext()->GetTaskRunner(
              TaskType::kInternalDefault)));
}

AnchorElementInteractionTracker::~AnchorElementInteractionTracker() = default;

void AnchorElementInteractionTracker::Trace(Visitor* visitor) const {
  visitor->Trace(interaction_host_);
  visitor->Trace(hover_timer_);
}

// static
bool AnchorElementInteractionTracker::IsFeatureEnabled() {
  return base::FeatureList::IsEnabled(features::kAnchorElementInteraction);
}

// static
base::TimeDelta AnchorElementInteractionTracker::GetHoverDwellTime() {
  static base::FeatureParam<base::TimeDelta> hover_dwell_time{
      &blink::features::kSpeculationRulesPointerHoverHeuristics,
      "HoverDwellTime", base::Milliseconds(200)};
  return hover_dwell_time.Get();
}

void AnchorElementInteractionTracker::OnPointerEvent(
    EventTarget& target,
    const PointerEvent& pointer_event) {
  if (!target.ToNode()) {
    return;
  }
  if (!pointer_event.isPrimary()) {
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

  const AtomicString& event_type = pointer_event.type();
  if (event_type == event_type_names::kPointerdown) {
    // TODO(crbug.com/1297312): Check if user changed the default mouse
    // settings
    if (pointer_event.button() !=
            static_cast<int>(WebPointerProperties::Button::kLeft) &&
        pointer_event.button() !=
            static_cast<int>(WebPointerProperties::Button::kMiddle)) {
      return;
    }
    interaction_host_->OnPointerDown(url);
    return;
  }

  if (!base::FeatureList::IsEnabled(
          features::kSpeculationRulesPointerHoverHeuristics)) {
    return;
  }

  if (event_type == event_type_names::kPointerover) {
    hover_events_.insert(url, clock_->NowTicks() + GetHoverDwellTime());
    if (!hover_timer_.IsActive()) {
      hover_timer_.StartOneShot(GetHoverDwellTime(), FROM_HERE);
    }
  } else if (event_type == event_type_names::kPointerout) {
    // Since the pointer is no longer hovering on the link, there is no need to
    // check the timer. We should just remove it here.
    hover_events_.erase(url);
  }
}

void AnchorElementInteractionTracker::HoverTimerFired(TimerBase*) {
  if (!interaction_host_.is_bound()) {
    return;
  }
  const base::TimeTicks now = clock_->NowTicks();
  auto next_fire_time = base::TimeTicks::Max();
  Vector<KURL> to_be_erased;
  for (auto& hover_event : hover_events_) {
    // Check whether pointer hovered long enough on the link to send the
    // PointerHover event to interaction host.
    if (now >= hover_event.value) {
      interaction_host_->OnPointerHover(hover_event.key);
      to_be_erased.push_back(hover_event.key);
      continue;
    }
    // Update next fire time
    next_fire_time = std::min(next_fire_time, hover_event.value);
  }
  WTF::RemoveAll(hover_events_, to_be_erased);
  if (!next_fire_time.is_max()) {
    hover_timer_.StartOneShot(next_fire_time - now, FROM_HERE);
  }
}

void AnchorElementInteractionTracker::FireHoverTimerForTesting() {
  if (hover_timer_.IsActive()) {
    hover_timer_.Stop();
  }
  HoverTimerFired(&hover_timer_);
}

void AnchorElementInteractionTracker::SetTickClockForTesting(
    const base::TickClock* clock) {
  clock_ = clock;
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
