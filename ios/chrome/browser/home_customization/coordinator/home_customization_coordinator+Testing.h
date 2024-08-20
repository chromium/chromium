// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_COORDINATOR_HOME_CUSTOMIZATION_COORDINATOR_TESTING_H_
#define IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_COORDINATOR_HOME_CUSTOMIZATION_COORDINATOR_TESTING_H_

#import "ios/chrome/browser/home_customization/coordinator/home_customization_coordinator.h"

@class HomeCustomizationMagicStackViewController;
@class HomeCustomizationMainViewController;
@class HomeCustomizationMediator;

// Testing category that is intended to only be imported in tests.
@interface HomeCustomizationCoordinator (Testing) <
    UISheetPresentationControllerDelegate>

// Exposing as readonly for testing.
@property(nonatomic, strong, readonly)
    HomeCustomizationMainViewController* mainViewController;

// Exposing as readonly for testing.
@property(nonatomic, strong, readonly)
    HomeCustomizationMagicStackViewController* magicStackViewController;

// Exposing as readonly for testing.
@property(nonatomic, strong, readonly) HomeCustomizationMediator* mediator;

// Exposing as readonly for testing.
@property(nonatomic, strong, readonly)
    UINavigationController* navigationController;

@end

#endif  // IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_COORDINATOR_HOME_CUSTOMIZATION_COORDINATOR_TESTING_H_
