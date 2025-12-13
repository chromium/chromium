// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/web/web_interaction_effects_monitor.h"

#include "base/check.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/core/timing/interaction_effects_monitor.h"
#include "third_party/blink/renderer/core/timing/soft_navigation_heuristics.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"

namespace blink {

WebInteractionEffectsMonitor::WebInteractionEffectsMonitor(
    LocalFrame* frame,
    WebInteractionEffectsMonitorObserver* observer) {
  CHECK(observer);
  CHECK(frame);
  if (SoftNavigationHeuristics* heuristics =
          frame->DomWindow()->GetSoftNavigationHeuristics()) {
    interaction_effects_monitor_ =
        MakeGarbageCollected<InteractionEffectsMonitor>(heuristics);
    interaction_effects_monitor_->StartMonitoring(observer);
  }
}

WebInteractionEffectsMonitor::WebInteractionEffectsMonitor(
    const WebLocalFrame& web_local_frame,
    WebInteractionEffectsMonitorObserver* observer)
    : WebInteractionEffectsMonitor(
          To<WebLocalFrameImpl>(web_local_frame).GetFrame(),
          observer) {}

WebInteractionEffectsMonitor::~WebInteractionEffectsMonitor() {
  if (interaction_effects_monitor_) {
    interaction_effects_monitor_->StopMonitoring();
  }
}

uint32_t WebInteractionEffectsMonitor::InteractionCount() const {
  return interaction_effects_monitor_
             ? interaction_effects_monitor_->InteractionCount()
             : 0;
}

uint64_t WebInteractionEffectsMonitor::TotalPaintedArea() const {
  return interaction_effects_monitor_
             ? interaction_effects_monitor_->TotalPaintedArea()
             : 0;
}

}  // namespace blink
