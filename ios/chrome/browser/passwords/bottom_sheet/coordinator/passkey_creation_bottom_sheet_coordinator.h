// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PASSWORDS_BOTTOM_SHEET_COORDINATOR_PASSKEY_CREATION_BOTTOM_SHEET_COORDINATOR_H_
#define IOS_CHROME_BROWSER_PASSWORDS_BOTTOM_SHEET_COORDINATOR_PASSKEY_CREATION_BOTTOM_SHEET_COORDINATOR_H_

#import <string>

#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"

@protocol BrowserCoordinatorCommands;

// Coordinator for the passkey creation bottom sheet.
@interface PasskeyCreationBottomSheetCoordinator : ChromeCoordinator

// `viewController` is the view controller used to present the bottom sheet.
// `requestID` comes from the PasskeyTabHelper and identifies the passkey
// request which triggered this bottom sheet.
- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                                 requestID:(std::string)requestID;

// Handler for Browser Coordinator Commands.
@property(nonatomic, weak) id<BrowserCoordinatorCommands>
    browserCoordinatorCommandsHandler;

@end

#endif  // IOS_CHROME_BROWSER_PASSWORDS_BOTTOM_SHEET_COORDINATOR_PASSKEY_CREATION_BOTTOM_SHEET_COORDINATOR_H_
