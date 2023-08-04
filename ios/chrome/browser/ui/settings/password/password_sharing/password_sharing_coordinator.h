// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_SHARING_PASSWORD_SHARING_COORDINATOR_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_SHARING_PASSWORD_SHARING_COORDINATOR_H_

#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"

// This is the main coordinator for the password sharing flow initiated from a
// password details view. It coordinates the whole flow including fetching
// recipient candidates, dispatching child coordinators (e.g. family picker,
// password picker) and sending sharing invitations.
@interface PasswordSharingCoordinator : ChromeCoordinator

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
    NS_DESIGNATED_INITIALIZER;
@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_SHARING_PASSWORD_SHARING_COORDINATOR_H_
