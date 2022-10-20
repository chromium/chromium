// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_PUBLIC_PROVIDER_CHROME_BROWSER_SIGNIN_FAKE_CHROME_IDENTITY_INTERACTION_MANAGER_H_
#define IOS_PUBLIC_PROVIDER_CHROME_BROWSER_SIGNIN_FAKE_CHROME_IDENTITY_INTERACTION_MANAGER_H_

#import "ios/public/provider/chrome/browser/signin/chrome_identity_interaction_manager.h"

// A fake ChromeIdentityInteractionManager to use in integration tests.
@interface FakeChromeIdentityInteractionManager
    : ChromeIdentityInteractionManager

// Identity that will be returned by the add account method if the dialog is
// closed successfully.
@property(nonatomic, strong, class) id<SystemIdentity> identity;

// YES if the fake add account view is presented.
@property(nonatomic, assign, readonly) BOOL viewControllerPresented;

// Simulates a user tapping the sign-in button.
- (void)addAccountViewControllerDidTapSignIn;

// Simulates a user tapping the cancel button.
- (void)addAccountViewControllerDidTapCancel;

// Simulates the user encountering an error not handled by SystemIdentity.
- (void)addAccountViewControllerDidThrowUnhandledError;

// Simulates the add account view being interrupted.
- (void)addAccountViewControllerDidInterrupt;

@end

#endif  // IOS_PUBLIC_PROVIDER_CHROME_BROWSER_SIGNIN_FAKE_CHROME_IDENTITY_INTERACTION_MANAGER_H_
