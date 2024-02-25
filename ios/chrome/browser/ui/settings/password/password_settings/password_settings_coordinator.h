// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_SETTINGS_PASSWORD_SETTINGS_COORDINATOR_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_SETTINGS_PASSWORD_SETTINGS_COORDINATOR_H_

#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"

@protocol PasswordSettingsCoordinatorDelegate;

// This coordinator presents settings related to the Password Manager.
@interface PasswordSettingsCoordinator : ChromeCoordinator

// Delegate.
@property(nonatomic, weak) id<PasswordSettingsCoordinatorDelegate> delegate;

// Whether Local Authentication should be skipped when the coordinator is
// started. Defaults to NO. Authentication should be required when starting the
// coordinator unless it was already required by the starting coordinator or
// another ancestor higher in the ancestor chain. This property is most likely
// used only by coordinators for other password manager subpages as the password
// manager requires authentication upon entry.
@property(nonatomic) BOOL skipAuthenticationOnStart;

// Stops the coordinator.
// - shouldDismissUI: Whether stopping also dismisses the presented
// UIViewController. Use NO when dismissing the whole Password Manager UI in one
// animation instead of a cascade of animations (i.e. Password Settings is
// dismissed and then the Password Manager).
- (void)stopWithUIDismissal:(BOOL)shouldDismissUI;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_SETTINGS_PASSWORD_SETTINGS_COORDINATOR_H_
