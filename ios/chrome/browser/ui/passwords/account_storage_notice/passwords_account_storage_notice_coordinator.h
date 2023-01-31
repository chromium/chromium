// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_PASSWORDS_ACCOUNT_STORAGE_NOTICE_PASSWORDS_ACCOUNT_STORAGE_NOTICE_COORDINATOR_H_
#define IOS_CHROME_BROWSER_UI_PASSWORDS_ACCOUNT_STORAGE_NOTICE_PASSWORDS_ACCOUNT_STORAGE_NOTICE_COORDINATOR_H_

#import "ios/chrome/browser/ui/coordinators/chrome_coordinator.h"

// See PasswordsAccountStorageNoticeCommands.
@interface PasswordsAccountStorageNoticeCoordinator : ChromeCoordinator

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                          dismissalHandler:(void (^)())dismissalHandler
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_UI_PASSWORDS_ACCOUNT_STORAGE_NOTICE_PASSWORDS_ACCOUNT_STORAGE_NOTICE_COORDINATOR_H_
