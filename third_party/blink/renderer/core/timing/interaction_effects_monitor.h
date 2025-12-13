// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_TIMING_INTERACTION_EFFECTS_MONITOR_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_TIMING_INTERACTION_EFFECTS_MONITOR_H_

#include <cstdint>

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/member.h"

namespace blink {
class SoftNavigationContext;
class SoftNavigationHeuristics;
class WebInteractionEffectsMonitorObserver;

// This class monitors the effects of ongoing user interactions, e.g. contentful
// paints, and reports them to a single `WebInteractionEffectsMonitorObserver`.
// This is intended to be used in conjunction with other heuristics to help
// determine when a page becomes stable after user interaction.
//
// Note: `InteractionEffectsMonitor`s are 1:1 with the observer passed in
// `StartMonitoring()`, and instances are not intended to be shared. Each object
// that needs to observe interactions should create a separate instance of this
// class.
class CORE_EXPORT InteractionEffectsMonitor
    : public GarbageCollected<InteractionEffectsMonitor> {
 public:
  explicit InteractionEffectsMonitor(SoftNavigationHeuristics*);
  ~InteractionEffectsMonitor();

  // Start monitoring user interactions, resetting any previous state. Only
  // interactions that occur from this point forward will be monitored. The
  // observer is not owned by this class, and `StopMonitoring()` must be called
  // before the observer is destroyed. Must not already be monitoring
  // interactions when this is called.
  void StartMonitoring(WebInteractionEffectsMonitorObserver*);

  // Stop monitoring interactions.
  void StopMonitoring();

  // Called by `SoftNavigationHeuristics` during frame detach.
  void Shutdown();

  uint64_t TotalPaintedArea() const { return total_painted_area_; }
  uint32_t InteractionCount() const { return interaction_count_; }

  void OnSoftNavigationContextCreated() { ++interaction_count_; }

  void OnContentfulPaint(SoftNavigationContext*, uint64_t new_painted_area);

  void Trace(Visitor*) const;

 private:
  WebInteractionEffectsMonitorObserver* observer_ = nullptr;
  Member<SoftNavigationHeuristics> soft_navigation_heuristics_;
  uint64_t total_painted_area_ = 0;
  uint64_t min_context_id_ = 0;
  uint32_t interaction_count_ = 0;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_TIMING_INTERACTION_EFFECTS_MONITOR_H_
