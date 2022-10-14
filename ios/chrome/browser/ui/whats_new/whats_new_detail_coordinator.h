// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_WHATS_NEW_WHATS_NEW_DETAIL_COORDINATOR_H_
#define IOS_CHROME_BROWSER_UI_WHATS_NEW_WHATS_NEW_DETAIL_COORDINATOR_H_

#import "ios/chrome/browser/ui/coordinators/chrome_coordinator.h"
#import "ios/chrome/browser/ui/whats_new/whats_new_detail_view_delegate.h"

@protocol WhatsNewDetailViewActionHandler;

@class WhatsNewItem;

@interface WhatsNewDetailCoordinator
    : ChromeCoordinator <WhatsNewDetailViewDelegate>

// `navigationController`: Handles user movement to check subpages.
// `browser`: browser state for preferences and password check.
- (instancetype)initWithBaseNavigationController:
                    (UINavigationController*)navigationController
                                         browser:(Browser*)browser
                                            item:(WhatsNewItem*)item
                                   actionHandler:
                                       (id<WhatsNewDetailViewActionHandler>)
                                           actionHandler
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_UI_WHATS_NEW_WHATS_NEW_DETAIL_COORDINATOR_H_
