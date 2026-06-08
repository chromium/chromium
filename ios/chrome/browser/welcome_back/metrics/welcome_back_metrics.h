// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_WELCOME_BACK_METRICS_WELCOME_BACK_METRICS_H_
#define IOS_CHROME_BROWSER_WELCOME_BACK_METRICS_WELCOME_BACK_METRICS_H_

// Whether the Welcome Back promo was registered. If not, the reason why it was
// not. These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// LINT.IfChange(WelcomeBackPromoRegistrationResult)
enum class WelcomeBackPromoRegistrationResult {
  // The Welcome Back promo was successfully registered.
  kSuccess = 0,
  // The time since the last active session was not met.
  kFailureTimeSinceActiveLimitNotMet = 1,
  // The minimum number of eligible features was not met.
  kFailureMinEligibleFeaturesNotMet = 2,
  // The last session end time was nil. This typically happens on a clean
  // install (first run) where no previous session exists, or if the previous
  // session info was cleared or failed to load on startup.
  kFailureSessionEndTimeNil = 3,
  // The first run has not completed.
  kFailureFirstRun = 4,
  // The Feature Engagement Tracker failed to retrieve the active days count.
  kFailureTrackerInitialization = 5,
  // The user is not a resurrected user (has not had the app installed for 28+
  // days).
  kFailureNotResurrectedUser = 6,
  kMaxValue = kFailureNotResurrectedUser,
};
// LINT.ThenChange(/tools/metrics/histograms/metadata/ios/enums.xml)

#endif  // IOS_CHROME_BROWSER_WELCOME_BACK_METRICS_WELCOME_BACK_METRICS_H_
