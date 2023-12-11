// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_POLICY_IDLE_IDLE_TIMEOUT_CONFIRMATION_COORDINATOR_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_POLICY_IDLE_IDLE_TIMEOUT_CONFIRMATION_COORDINATOR_DELEGATE_H_

@protocol IdleTimeoutConfirmationCoordinatorDelegate <NSObject>

// Called when the presentation did complete. Usually called when the action the
// user did on the prompt is completed (e.g. after tapping on Continue). Will
// load enterprise learn more page if `showLearnMoreAfterward` is YES.
- (void)stopPresentingAndRunActionsAfterwards:(BOOL)doRunActions;

@end

#endif  // IOS_CHROME_BROWSER_UI_POLICY_IDLE_IDLE_TIMEOUT_CONFIRMATION_COORDINATOR_DELEGATE_H_
