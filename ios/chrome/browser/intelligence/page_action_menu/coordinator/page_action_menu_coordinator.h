// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_PAGE_ACTION_MENU_COORDINATOR_PAGE_ACTION_MENU_COORDINATOR_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_PAGE_ACTION_MENU_COORDINATOR_PAGE_ACTION_MENU_COORDINATOR_H_

#import <UIKit/UIKit.h>

#import "base/ios/block_types.h"
#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"

@protocol PageActionMenuCommands;

// Coordinator for the page action menu.
@interface PageActionMenuCoordinator : ChromeCoordinator

// The dispatcher for commands.
@property(nonatomic, weak) id<PageActionMenuCommands> pageActionMenuHandler;

// Dismisses the menu with a completion block before stopping the coordinator.
- (void)stopWithCompletion:(ProceduralBlock)completion;

@end

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_PAGE_ACTION_MENU_COORDINATOR_PAGE_ACTION_MENU_COORDINATOR_H_
