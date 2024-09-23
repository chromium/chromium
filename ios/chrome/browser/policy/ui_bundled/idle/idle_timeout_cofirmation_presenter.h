// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_POLICY_UI_BUNDLED_IDLE_IDLE_TIMEOUT_COFIRMATION_PRESENTER_H_
#define IOS_CHROME_BROWSER_POLICY_UI_BUNDLED_IDLE_IDLE_TIMEOUT_COFIRMATION_PRESENTER_H_

@protocol IdleTimeoutConfirmationPresenter

// Dismisses the dialog and does nothing after user clicks `Continue using
// Chrome`.
- (void)stopPresentingAfterUserClickedContinue;

// Dismisses the dialog and runs actions after dialog has displayed until it
// expired.
- (void)stopPresentingAfterDialogExpired;

@end

#endif  // IOS_CHROME_BROWSER_POLICY_UI_BUNDLED_IDLE_IDLE_TIMEOUT_COFIRMATION_PRESENTER_H_
