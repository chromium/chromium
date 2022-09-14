// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_TEST_FAKES_FAKE_PASSWORD_AUTO_FILL_STATUS_MANAGER_H_
#define IOS_CHROME_TEST_FAKES_FAKE_PASSWORD_AUTO_FILL_STATUS_MANAGER_H_

#import "ios/chrome/browser/ui/settings/utils/password_auto_fill_status_manager.h"

// Substitute of the real PasswordAutoFillStatusManager that could update the UI
// of observers accordingly without interacting with iOS API.
@interface FakePasswordAutoFillStatusManager : PasswordAutoFillStatusManager

// The shared instance MockPasswordAutofillStatusManager.
+ (FakePasswordAutoFillStatusManager*)sharedFakeManager;

// Mocks the scenario that the app has retrieved the current state of device
// auto-fill status.
// `autoFillEnabled`: whether auto-fill with Chrome is enabled or not.
- (void)startFakeManagerWithAutoFillStatus:(BOOL)autoFillEnabled;

// Explicitly sets auto-fill status.
// `autoFillEnabled`: whether auto-fill with Chrome should be.
- (void)setAutoFillStatus:(BOOL)autoFillEnabled;

// Resets the manager.
- (void)reset;

@end

#endif  // IOS_CHROME_TEST_FAKES_FAKE_PASSWORD_AUTO_FILL_STATUS_MANAGER_H_
