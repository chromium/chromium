// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_PUBLIC_PROVIDER_CHROME_BROWSER_SIGNIN_FAKE_CHROME_IDENTITY_INTERACTION_MANAGER_H_
#define IOS_PUBLIC_PROVIDER_CHROME_BROWSER_SIGNIN_FAKE_CHROME_IDENTITY_INTERACTION_MANAGER_H_

#import "ios/public/provider/chrome/browser/signin/chrome_identity_interaction_manager.h"

// Accessibility identifier for FakeChromeIdentityInteractionManager view.
extern NSString* kFakeAddAccountViewIdentifier;

// A fake ChromeIdentityInteractionManager to use in integration tests.
@interface FakeChromeIdentityInteractionManager
    : ChromeIdentityInteractionManager

// Fake identity that will be returned by the add account method.
@property(nonatomic, weak) ChromeIdentity* fakeIdentity;

@end

#endif  // IOS_PUBLIC_PROVIDER_CHROME_BROWSER_SIGNIN_FAKE_CHROME_IDENTITY_INTERACTION_MANAGER_H_
