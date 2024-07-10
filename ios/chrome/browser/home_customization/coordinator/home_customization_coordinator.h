// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_COORDINATOR_HOME_CUSTOMIZATION_COORDINATOR_H_
#define IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_COORDINATOR_HOME_CUSTOMIZATION_COORDINATOR_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/home_customization/utils/home_customization_constants.h"
#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"

@protocol HomeCustomizationDelegate;

// The coordinator for the Home surface's customization menu.
@interface HomeCustomizationCoordinator : ChromeCoordinator

// Delegate for communicating back to the Home surface.
@property(nonatomic, weak) id<HomeCustomizationDelegate> delegate;

// Opens the customization menu as a sheet.
- (void)presentCustomizationMenuAtPage:(CustomizationMenuPage)page;

@end

#endif  // IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_COORDINATOR_HOME_CUSTOMIZATION_COORDINATOR_H_
