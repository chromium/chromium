// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_FULLSCREEN_PUBLIC_FULLSCREEN_METRICS_H_
#define IOS_CHROME_BROWSER_FULLSCREEN_PUBLIC_FULLSCREEN_METRICS_H_

extern const char kEnterFullscreenModeTransitionTriggerHistogram[];
extern const char kExitFullscreenModeTransitionTriggerHistogram[];
extern const char kTimeInFullscreenHistogram[];
extern const char kTimeNotInFullscreenHistogram[];

// These values are persisted to IOS.Fullscreen.TransitionTrigger.{Enter,Exit}
// histograms. Entries should not be renumbered and numeric values should never
// be reused.
// LINT.IfChange(FullscreenModeTransitionTrigger)
enum class FullscreenModeTransitionTrigger {
  // Reported when fullscreen mode is triggered by a sustained downward scroll
  // gesture, with the animation of entering or exiting fullscreen mode being
  // fully controlled by the scroll action.
  kUserControlled = 0,
  // Reported when entering or exiting fullscreen mode is forced by the code,
  // either without any user action or in response to a user action where the
  // user's intent wasn't explicitly to change the fullscreen state.
  kForcedByCode = 1,
  // Reported when fullscreen mode is triggered by a minimal downward scroll
  // gesture. The system automatically completes the fullscreen animation after
  // the user lifts their finger during the animation.
  kUserInitiatedFinishedByCode = 2,
  // Reported when exiting fullscreen mode by tapping on the toolbar.
  kForcedByUser = 3,
  // Reported when exiting fullscreen mode by reaching the bottom of the page.
  kBottomReached = 4,
  // Reported when the animation is called but the state is still the same, so
  // for example, when the user start in fullscreen mode and the animation is
  // triggered to go in fullscreen mode.
  kNoChange = 5,
  kMaxValue = kNoChange,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/ios/enums.xml:FullscreenModeTransitionTrigger)

#endif  // IOS_CHROME_BROWSER_FULLSCREEN_PUBLIC_FULLSCREEN_METRICS_H_
