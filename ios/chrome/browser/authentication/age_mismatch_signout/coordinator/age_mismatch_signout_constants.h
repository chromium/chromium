// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTHENTICATION_AGE_MISMATCH_SIGNOUT_COORDINATOR_AGE_MISMATCH_SIGNOUT_CONSTANTS_H_
#define IOS_CHROME_BROWSER_AUTHENTICATION_AGE_MISMATCH_SIGNOUT_COORDINATOR_AGE_MISMATCH_SIGNOUT_CONSTANTS_H_

// Enum for age mismatch sign-out actions.
// LINT.IfChange(AgeMismatchSignoutAction)
enum class AgeMismatchSignoutAction {
  kUseAnotherAccount = 0,
  kUseWithoutAccount = 1,
  kMaxValue = kUseWithoutAccount,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/signin/enums.xml:AgeMismatchSignoutAction)

// LINT.IfChange(AgeMismatchStaySignedOutButtonState)
enum class AgeMismatchStaySignedOutButtonState {
  kShown = 0,
  kHidden = 1,
  kMaxValue = kHidden,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/signin/enums.xml:AgeMismatchStaySignedOutButtonState)

// Histogram name for age mismatch sign-out actions.
extern const char kAgeMismatchSignoutActionHistogram[];

// Histogram name for age mismatch sign-out prompt mode.
extern const char kAgeMismatchSignoutPromptModeHistogram[];

// Histogram name for recording if 'Stay signed out' button is hidden or shown.
extern const char kAgeMismatchSignoutStaySignedOutButtonHistogram[];

// Action name for tapping 'Learn More'.
extern const char kAgeMismatchSignoutLearnMoreAction[];

#endif  // IOS_CHROME_BROWSER_AUTHENTICATION_AGE_MISMATCH_SIGNOUT_COORDINATOR_AGE_MISMATCH_SIGNOUT_CONSTANTS_H_
