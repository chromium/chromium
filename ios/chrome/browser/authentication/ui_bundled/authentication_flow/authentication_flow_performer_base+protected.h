// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTHENTICATION_UI_BUNDLED_AUTHENTICATION_FLOW_AUTHENTICATION_FLOW_PERFORMER_BASE_PROTECTED_H_
#define IOS_CHROME_BROWSER_AUTHENTICATION_UI_BUNDLED_AUTHENTICATION_FLOW_AUTHENTICATION_FLOW_PERFORMER_BASE_PROTECTED_H_

@interface AuthenticationFlowPerformerBase (Protected)

// Starts a Watchdog Timer that calls `timeoutBlock` on time out.
- (void)startWatchdogTimerWithTimeoutBlock:(ProceduralBlock)timeoutBlock;

// Check fail if a dialog is displayed on top of the authentication flow
// performer.
- (void)checkNoDialog;

// Stops the watchdog timer, and doesn't call the `timeoutDelegateSelector`.
// Returns whether the watchdog was actually running.
- (BOOL)stopWatchdogTimer;

@end

#endif  // IOS_CHROME_BROWSER_AUTHENTICATION_UI_BUNDLED_AUTHENTICATION_FLOW_AUTHENTICATION_FLOW_PERFORMER_BASE_PROTECTED_H_
