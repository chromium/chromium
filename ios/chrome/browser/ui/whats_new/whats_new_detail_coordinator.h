// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_WHATS_NEW_WHATS_NEW_DETAIL_COORDINATOR_H_
#define IOS_CHROME_BROWSER_UI_WHATS_NEW_WHATS_NEW_DETAIL_COORDINATOR_H_

#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"
#import "ios/chrome/browser/ui/whats_new/whats_new_instructions_coordinator_delegate.h"

@protocol WhatsNewDetailViewActionHandler;
@protocol WhatsNewCommands;

@class WhatsNewItem;

@interface WhatsNewDetailCoordinator
    : ChromeCoordinator <WhatsNewInstructionsViewDelegate>

// `navigationController`: Handles user movement to check subpages.
// `browser`: profile for preferences and password check.
- (instancetype)
    initWithBaseNavigationController:
        (UINavigationController*)navigationController
                             browser:(Browser*)browser
                                item:(WhatsNewItem*)item
                       actionHandler:
                           (id<WhatsNewDetailViewActionHandler>)actionHandler
                     whatsNewHandler:(id<WhatsNewCommands>)whatsNewHandler
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_UI_WHATS_NEW_WHATS_NEW_DETAIL_COORDINATOR_H_
