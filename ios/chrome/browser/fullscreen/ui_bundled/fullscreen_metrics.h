// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_FULLSCREEN_UI_BUNDLED_FULLSCREEN_METRICS_H_
#define IOS_CHROME_BROWSER_FULLSCREEN_UI_BUNDLED_FULLSCREEN_METRICS_H_

extern const char kEnterFullscreenModeTransitionReasonHistogram[];
extern const char kExitFullscreenModeTransitionReasonHistogram[];

// These values are persisted to IOS.Fullscreen.TransitionReason.{Enter,Exit}
// histograms. Entries should not be renumbered and numeric values should never
// be reused.
// LINT.IfChange(FullscreenModeTransitionReason)
enum class FullscreenModeTransitionReason {
  // Reported when fullscreen mode is triggered by a sustained downward scroll
  // gesture, with the animation of entering or exiting fullscreen mode being
  // fully controlled by the scroll action.
  kUserControlled = 0,
  // Reported when entering to fullscreen mode is forced by the code, without
  // any user action.
  kForcedByCode = 1,
  // Reported when fullscreen mode is triggered by a minimal downward scroll
  // gesture. The system automatically completes the fullscreen animation after
  // the user lifts their finger during the animation.
  kUserInitiatedFinishedByCode = 2,
  // Reported when exiting fullscreen mode by tapping on the toolbar.
  kUserTapped = 3,
  // Reported when exiting fullscreen mode by reaching the bottom of the page.
  kBottomReached = 4,
  kMaxValue = kBottomReached,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/ios/enums.xml:FullscreenModeTransitionReason)

#endif  // IOS_CHROME_BROWSER_FULLSCREEN_UI_BUNDLED_FULLSCREEN_METRICS_H_
