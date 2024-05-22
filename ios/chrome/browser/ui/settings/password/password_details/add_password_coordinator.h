// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_DETAILS_ADD_PASSWORD_COORDINATOR_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_DETAILS_ADD_PASSWORD_COORDINATOR_H_

#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"

@protocol AddPasswordCoordinatorDelegate;
@protocol ReauthenticationProtocol;

// This coordinator presents add password sheet for the user.
@interface AddPasswordCoordinator : ChromeCoordinator

// Delegate.
@property(nonatomic, weak) id<AddPasswordCoordinatorDelegate> delegate;

// Stops the coordinator.
// - shouldDismissUI: Whether stopping also dismisses the presented
// UIViewController. Use NO when dismissing the whole Password Manager UI in one
// animation instead of a cascade of animations (i.e. Add Password is dismissed
// and then the Password Manager).
- (void)stopWithUIDismissal:(BOOL)shouldDismissUI;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_DETAILS_ADD_PASSWORD_COORDINATOR_H_
