// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORDS_IN_OTHER_APPS_PASSWORDS_IN_OTHER_APPS_APP_INTERFACE_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORDS_IN_OTHER_APPS_PASSWORDS_IN_OTHER_APPS_APP_INTERFACE_H_

#import <Foundation/Foundation.h>

// Bridges passwords_in_other_apps_egtest.mm to
// FakePasswordAutoFillStatusManager.
@interface PasswordsInOtherAppsAppInterface : NSObject

#pragma mark - Swizzling

// Returns the block to use for swizzling the PasswordAutoFillStatusManager.
// This block is only used for swizzling, which is why its type is opaque. It
// should not be called directly.
+ (id)swizzlePasswordAutoFillStatusManagerWithFake;

#pragma mark - Mocking and Expectations

// Mocks the scenario that the app has retrieved the current state of device
// auto-fill status.
// `isEnabled`: whether auto-fill with Chrome is enabled or not.
+ (void)startFakeManagerWithAutoFillStatus:(BOOL)autoFillEnabled;

// Explicitly sets auto-fill status.
// `autoFillEnabled`: whether auto-fill with Chrome should be.
+ (void)setAutoFillStatus:(BOOL)autoFillEnabled;

// Resets the manager.
+ (void)resetManager;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORDS_IN_OTHER_APPS_PASSWORDS_IN_OTHER_APPS_APP_INTERFACE_H_
