// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_SAFETY_CHECK_SAFETY_CHECK_MEDIATOR_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_SAFETY_CHECK_SAFETY_CHECK_MEDIATOR_DELEGATE_H_

// Delegate protocol for handling user interactions related to Safety Check
// push notifications. The delegate is responsible for coordinating the enabling
// and disabling of notifications, potentially involving user interface
// presentation.
@protocol SafetyCheckMediatorDelegate <NSObject>

// Toggles Safety Check notifications on/off. The coordinator should present
// appropriate UI (e.g., notification opt-in/opt-out) to the user
// based on the current notification state.
- (void)toggleSafetyCheckNotifications;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_SAFETY_CHECK_SAFETY_CHECK_MEDIATOR_DELEGATE_H_
