// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_POLICY_UI_BUNDLED_IDLE_IDLE_TIMEOUT_CONFIRMATION_COORDINATOR_DELEGATE_H_
#define IOS_CHROME_BROWSER_POLICY_UI_BUNDLED_IDLE_IDLE_TIMEOUT_CONFIRMATION_COORDINATOR_DELEGATE_H_

@protocol IdleTimeoutConfirmationCoordinatorDelegate <NSObject>

// Called when the presentation did complete and should be dismissed. Usually
// called when the user clicks the  `Continue Using Chrome` button or when the
// dialog expires.
- (void)stopPresentingAndRunActionsAfterwards:(BOOL)doRunActions;

@end

#endif  // IOS_CHROME_BROWSER_POLICY_UI_BUNDLED_IDLE_IDLE_TIMEOUT_CONFIRMATION_COORDINATOR_DELEGATE_H_
