// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_POST_RESTORE_SIGNIN_METRICS_H_
#define IOS_CHROME_BROWSER_UI_POST_RESTORE_SIGNIN_METRICS_H_

// Histogram to record when the post restore signin promo is displayed.
extern const char kIOSPostRestoreSigninDisplayedHistogram[];

// Histogram to record which choice the user made when the promo was presented.
extern const char kIOSPostRestoreSigninChoiceHistogram[];

// Enum containing bucket values for kIOSPostRestoreSigninChoiceHistogram.
enum class IOSPostRestoreSigninChoice {
  Continue = 0,
  Dismiss = 1,
  kMaxValue = Dismiss,
};

#endif  // IOS_CHROME_BROWSER_UI_POST_RESTORE_SIGNIN_METRICS_H_
