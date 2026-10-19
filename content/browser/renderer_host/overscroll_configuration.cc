// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/overscroll_configuration.h"

#include "content/common/features.h"

namespace {

bool g_is_touchpad_overscroll_history_navigation_enabled_initialized = false;
bool g_touchpad_overscroll_history_navigation_enabled = false;

// On Windows, we only process 0.3 second inertial events then cancel the
// overscroll if it is not completed yet.
int g_max_inertial_events_before_overscroll_cancellation_in_ms = 300;

}  // namespace

namespace content {

// static
const float OverscrollConfig::kCompleteTouchpadThresholdPercent = 0.3f;
const float OverscrollConfig::kCompleteTouchscreenThresholdPercent = 0.25f;

// static
const float OverscrollConfig::kStartTouchpadThresholdDips = 60.f;
const float OverscrollConfig::kStartTouchscreenThresholdDips = 50.f;

// static
bool OverscrollConfig::TouchpadOverscrollHistoryNavigationEnabled() {
  if (!g_is_touchpad_overscroll_history_navigation_enabled_initialized) {
    g_is_touchpad_overscroll_history_navigation_enabled_initialized = true;
    g_touchpad_overscroll_history_navigation_enabled =
        base::FeatureList::IsEnabled(
            features::kTouchpadOverscrollHistoryNavigation);
  }

  return g_touchpad_overscroll_history_navigation_enabled;
}

// static
void OverscrollConfig::ResetTouchpadOverscrollHistoryNavigationEnabled() {
  g_is_touchpad_overscroll_history_navigation_enabled_initialized = false;
}

// static
base::TimeDelta
OverscrollConfig::MaxInertialEventsBeforeOverscrollCancellation() {
  return base::Milliseconds(
      g_max_inertial_events_before_overscroll_cancellation_in_ms);
}

}  // namespace content
