// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_UTILS_PASSWORD_AUTO_FILL_STATUS_MANAGER_H_
#define IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_UTILS_PASSWORD_AUTO_FILL_STATUS_MANAGER_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/settings/ui_bundled/utils/password_auto_fill_status_observer.h"

// Singleton that listens to password autofill status changes and notifies
// observers on change.
@interface PasswordAutoFillStatusManager : NSObject

// The shared instance PasswordAutofillStatusManager.
+ (PasswordAutoFillStatusManager*)sharedManager;

// Adds observer that registers AutoFill status updates.
- (void)addObserver:(id<PasswordAutoFillStatusObserver>)observer;

// Removes observer that registers AutoFill status updates.
- (void)removeObserver:(id<PasswordAutoFillStatusObserver>)observer;

// Checks the AutoFill status and updates the observers with the status if
// needed.
- (void)checkAndUpdatePasswordAutoFillStatus;

// Whether the observer has finished initialization.
@property(nonatomic, assign, readonly) BOOL ready;

// Whether password auto-fill for Chrome is turned on.
@property(nonatomic, assign, readonly) BOOL autoFillEnabled;

@end

#endif  // IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_UTILS_PASSWORD_AUTO_FILL_STATUS_MANAGER_H_
