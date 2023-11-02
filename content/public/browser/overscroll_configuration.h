// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_OVERSCROLL_CONFIGURATION_H_
#define CONTENT_PUBLIC_BROWSER_OVERSCROLL_CONFIGURATION_H_

#include "base/time/time.h"
#include "content/common/content_export.h"

namespace content {

class CONTENT_EXPORT OverscrollConfig {
 public:
  // Determines pull-to-refresh mode according to --pull-to-refresh flag.
  enum class PullToRefreshMode {
    // Pull-to-refresh is disabled.
    kDisabled,

    // Pull-to-refresh is enabled for both touchscreen and touchpad.
    kEnabled,

    // Pull-to-refresh is enabled only for touchscreen.
    kEnabledTouchschreen,
  };

  // These are percentages of the display size.
  static const float kCompleteTouchpadThresholdPercent;
  static const float kCompleteTouchscreenThresholdPercent;

  static const float kStartTouchpadThresholdDips;
  static const float kStartTouchscreenThresholdDips;

  OverscrollConfig() = delete;
  OverscrollConfig(const OverscrollConfig&) = delete;
  OverscrollConfig& operator=(const OverscrollConfig&) = delete;

  static PullToRefreshMode GetPullToRefreshMode();

  static bool TouchpadOverscrollHistoryNavigationEnabled();

  static base::TimeDelta MaxInertialEventsBeforeOverscrollCancellation();

 private:
  friend class ScopedPullToRefreshMode;
  friend class OverscrollControllerTest;

  // Helper functions used by |ScopedPullToRefreshMode| to set and reset mode in
  // tests.
  static void SetPullToRefreshMode(PullToRefreshMode mode);
  static void ResetPullToRefreshMode();

  // Helper functions to reset TouchpadOverscrollHistoryNavigationEnabled in
  // tests.
  static void ResetTouchpadOverscrollHistoryNavigationEnabled();
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_OVERSCROLL_CONFIGURATION_H_
