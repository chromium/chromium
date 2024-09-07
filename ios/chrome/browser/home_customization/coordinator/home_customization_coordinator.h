// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_COORDINATOR_HOME_CUSTOMIZATION_COORDINATOR_H_
#define IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_COORDINATOR_HOME_CUSTOMIZATION_COORDINATOR_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/home_customization/coordinator/home_customization_navigation_delegate.h"
#import "ios/chrome/browser/home_customization/utils/home_customization_constants.h"
#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"

@protocol HomeCustomizationDelegate;

// The coordinator for the Home surface's customization menu.
@interface HomeCustomizationCoordinator
    : ChromeCoordinator <HomeCustomizationNavigationDelegate>

// Delegate for communicating back to the Home surface.
@property(nonatomic, weak) id<HomeCustomizationDelegate> delegate;

// Updates the data for all existing menu pages.
- (void)updateMenuData;

@end

#endif  // IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_COORDINATOR_HOME_CUSTOMIZATION_COORDINATOR_H_
