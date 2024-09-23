// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PHONE_NUMBER_UI_BUNDLED_ADD_CONTACTS_COORDINATOR_H_
#define IOS_CHROME_BROWSER_PHONE_NUMBER_UI_BUNDLED_ADD_CONTACTS_COORDINATOR_H_

#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"

// This coordinator presents a `CNContactViewController`.
@interface AddContactsCoordinator : ChromeCoordinator

// Init AddContactsCoordinator with the detected `phoneNumber`.
- (instancetype)initWithBaseViewController:(UIViewController*)baseViewController
                                   browser:(Browser*)browser
                               phoneNumber:(NSString*)phoneNumber
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_PHONE_NUMBER_UI_BUNDLED_ADD_CONTACTS_COORDINATOR_H_
