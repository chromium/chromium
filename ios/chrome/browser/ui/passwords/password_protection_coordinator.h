// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_PASSWORDS_PASSWORD_PROTECTION_COORDINATOR_H_
#define IOS_CHROME_BROWSER_UI_PASSWORDS_PASSWORD_PROTECTION_COORDINATOR_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/ui/coordinators/chrome_coordinator.h"

// Presents and stops the Password Protection feature.
@interface PasswordProtectionCoordinator : ChromeCoordinator

- (instancetype)initWithBaseViewController:(UIViewController*)baseViewController
                                   browser:(Browser*)browser
                               warningText:(NSString*)warningText
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_UI_PASSWORDS_PASSWORD_PROTECTION_COORDINATOR_H_
