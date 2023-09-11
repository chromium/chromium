// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/overscroll_configuration.h"

#include "base/command_line.h"
#include "base/strings/string_number_conversions.h"
#include "content/common/features.h"
#include "content/public/common/content_switches.h"

namespace {

bool g_is_ptr_mode_initialized = false;
content::OverscrollConfig::PullToRefreshMode g_ptr_mode =
    content::OverscrollConfig::PullToRefreshMode::kDisabled;

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
OverscrollConfig::PullToRefreshMode OverscrollConfig::GetPullToRefreshMode() {
  if (g_is_ptr_mode_initialized)
    return g_ptr_mode;

  const std::string mode =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kPullToRefresh);
  if (mode == "1")
    g_ptr_mode = PullToRefreshMode::kEnabled;
  else if (mode == "2")
    g_ptr_mode = PullToRefreshMode::kEnabledTouchschreen;
  g_is_ptr_mode_initialized = true;
  return g_ptr_mode;
}

// static
void OverscrollConfig::SetPullToRefreshMode(PullToRefreshMode mode) {
  g_ptr_mode = mode;
  g_is_ptr_mode_initialized = true;
}

// static
void OverscrollConfig::ResetPullToRefreshMode() {
  g_is_ptr_mode_initialized = false;
  g_ptr_mode = OverscrollConfig::PullToRefreshMode::kDisabled;
}

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
