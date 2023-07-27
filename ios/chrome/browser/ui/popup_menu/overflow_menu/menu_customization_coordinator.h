// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_POPUP_MENU_OVERFLOW_MENU_MENU_CUSTOMIZATION_COORDINATOR_H_
#define IOS_CHROME_BROWSER_UI_POPUP_MENU_OVERFLOW_MENU_MENU_CUSTOMIZATION_COORDINATOR_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"

@class OverflowMenuOrderer;

// Coordinator for the overflow menu customization screen.
@interface MenuCustomizationCoordinator : ChromeCoordinator

// Orderer to provide the actions to customize in the correct order.
@property(nonatomic, strong) OverflowMenuOrderer* menuOrderer;

@end

#endif  // IOS_CHROME_BROWSER_UI_POPUP_MENU_OVERFLOW_MENU_MENU_CUSTOMIZATION_COORDINATOR_H_
