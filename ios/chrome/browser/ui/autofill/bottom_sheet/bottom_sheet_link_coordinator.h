// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTOFILL_BOTTOM_SHEET_BOTTOM_SHEET_LINK_COORDINATOR_H_
#define IOS_CHROME_BROWSER_UI_AUTOFILL_BOTTOM_SHEET_BOTTOM_SHEET_LINK_COORDINATOR_H_

#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"

@class CrURL;
@protocol BottomSheetLinkCoordinatorDelegate;

@interface BottomSheetLinkCoordinator : ChromeCoordinator

// Initialize this coordinator for the given url.
- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                                       url:(CrURL*)url
                                     title:(NSString*)title;

// This delegate is responsible for dismissing this coordinator.
@property(weak, nonatomic) id<BottomSheetLinkCoordinatorDelegate> delegate;

@end

#endif  // IOS_CHROME_BROWSER_UI_AUTOFILL_BOTTOM_SHEET_BOTTOM_SHEET_LINK_COORDINATOR_H_
