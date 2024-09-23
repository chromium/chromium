// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_WHATS_NEW_WHATS_NEW_INSTRUCTIONS_COORDINATOR_H_
#define IOS_CHROME_BROWSER_UI_WHATS_NEW_WHATS_NEW_INSTRUCTIONS_COORDINATOR_H_

#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"

@protocol WhatsNewDetailViewActionHandler;
@protocol WhatsNewInstructionsViewDelegate;
@protocol WhatsNewCommands;

@class WhatsNewItem;

// Coordinator to present the half screen instruction for What's New feature and
// chrome tip.
@interface WhatsNewInstructionsCoordinator : ChromeCoordinator

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                                      item:(WhatsNewItem*)item
                             actionHandler:(id<WhatsNewDetailViewActionHandler>)
                                               actionHandler
                           whatsNewHandler:(id<WhatsNewCommands>)whatsNewHandler
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser NS_UNAVAILABLE;

// The delegate object to the main coordinator.
@property(nonatomic, weak) id<WhatsNewInstructionsViewDelegate> delegate;

@end

#endif  // IOS_CHROME_BROWSER_UI_WHATS_NEW_WHATS_NEW_INSTRUCTIONS_COORDINATOR_H_
