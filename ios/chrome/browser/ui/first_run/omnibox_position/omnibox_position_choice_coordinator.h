// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_FIRST_RUN_OMNIBOX_POSITION_OMNIBOX_POSITION_CHOICE_COORDINATOR_H_
#define IOS_CHROME_BROWSER_UI_FIRST_RUN_OMNIBOX_POSITION_OMNIBOX_POSITION_CHOICE_COORDINATOR_H_

#import "ios/chrome/browser/ui/first_run/interruptible_chrome_coordinator.h"

@protocol FirstRunScreenDelegate;
@protocol PromosManagerUIHandler;

/// Coordinator for the omnibox position choice screen.
@interface OmniboxPositionChoiceCoordinator : InterruptibleChromeCoordinator

/// Initiates a OmniboxPositionChoiceCoordinator. Used for app-launch promo with
/// the promos manager.
- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
    NS_DESIGNATED_INITIALIZER;

/// Initiates a OmniboxPositionChoiceCoordinator with a first run delegate.
- (instancetype)initWithBaseNavigationController:
                    (UINavigationController*)navigationController
                                         browser:(Browser*)browser
                                        delegate:(id<FirstRunScreenDelegate>)
                                                     delegate;

/// The promos manager ui handler to alert for promo UI changes. Should only be
/// set if this coordinator was a promo presented by the PromosManager.
@property(nonatomic, weak) id<PromosManagerUIHandler> promosUIHandler;

@end

#endif  // IOS_CHROME_BROWSER_UI_FIRST_RUN_OMNIBOX_POSITION_OMNIBOX_POSITION_CHOICE_COORDINATOR_H_
