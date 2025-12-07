// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/timing/interaction_effects_monitor.h"

#include "base/check.h"
#include "third_party/blink/public/web/web_interaction_effects_monitor_observer.h"
#include "third_party/blink/renderer/core/timing/soft_navigation_context.h"
#include "third_party/blink/renderer/core/timing/soft_navigation_heuristics.h"

namespace blink {

InteractionEffectsMonitor::InteractionEffectsMonitor(
    SoftNavigationHeuristics* heuristics)
    : soft_navigation_heuristics_(heuristics) {}

InteractionEffectsMonitor::~InteractionEffectsMonitor() {
  CHECK(!observer_);
}

void InteractionEffectsMonitor::OnContentfulPaint(
    SoftNavigationContext* context,
    uint64_t new_painted_area) {
  // Only consider interactions that occurred since the monitor was started.
  if (context->ContextId() < min_context_id_) {
    return;
  }
  CHECK(observer_);
  total_painted_area_ += new_painted_area;
  observer_->OnContentfulPaint(new_painted_area);
}

void InteractionEffectsMonitor::StartMonitoring(
    WebInteractionEffectsMonitorObserver* observer) {
  if (!soft_navigation_heuristics_) {
    return;
  }
  CHECK(!observer_);
  total_painted_area_ = 0;
  observer_ = observer;
  min_context_id_ = SoftNavigationContext::NextContextId();
  soft_navigation_heuristics_->RegisterInteractionEffectsMonitor(this);
}

void InteractionEffectsMonitor::StopMonitoring() {
  if (!soft_navigation_heuristics_) {
    return;
  }
  observer_ = nullptr;
  soft_navigation_heuristics_->UnregisterInteractionEffectsMonitor(this);
}

void InteractionEffectsMonitor::Shutdown() {
  soft_navigation_heuristics_ = nullptr;
  observer_ = nullptr;
}

void InteractionEffectsMonitor::Trace(Visitor* visitor) const {
  visitor->Trace(soft_navigation_heuristics_);
}

}  // namespace blink
