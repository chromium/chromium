// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_FIRST_RUN_OMNIBOX_POSITION_OMNIBOX_POSITION_CHOICE_COORDINATOR_H_
#define IOS_CHROME_BROWSER_UI_FIRST_RUN_OMNIBOX_POSITION_OMNIBOX_POSITION_CHOICE_COORDINATOR_H_

#import "ios/chrome/browser/ui/first_run/interruptible_chrome_coordinator.h"

@protocol FirstRunScreenDelegate;

/// Coordinator for the omnibox position choice screen.
@interface OmniboxPositionChoiceCoordinator : InterruptibleChromeCoordinator

/// Initiates a OmniboxPositionChoiceCoordinator with a first run delegate.
- (instancetype)initWithBaseNavigationController:
                    (UINavigationController*)navigationController
                                         browser:(Browser*)browser
                                        delegate:(id<FirstRunScreenDelegate>)
                                                     delegate;

@end

#endif  // IOS_CHROME_BROWSER_UI_FIRST_RUN_OMNIBOX_POSITION_OMNIBOX_POSITION_CHOICE_COORDINATOR_H_
