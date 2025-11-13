// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_BWG_COORDINATOR_BWG_COORDINATOR_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_BWG_COORDINATOR_BWG_COORDINATOR_H_

#import "base/ios/block_types.h"
#import "ios/chrome/browser/intelligence/bwg/utils/bwg_constants.h"
#import "ios/chrome/browser/promos_manager/ui_bundled/promos_manager_ui_handler.h"
#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"

// Coordinator that manages the first run and any BWG triggers.
@interface BWGCoordinator : ChromeCoordinator

// The image attachment to be passed to the Gemini overlay.
@property(nonatomic, strong) UIImage* imageAttachment;

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                            fromEntryPoint:(bwg::EntryPoint)entryPoint
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser NS_UNAVAILABLE;

// Dismisses the BWG flow with a completion block before stopping the
// coordinator.
- (void)stopWithCompletion:(ProceduralBlock)completion;

@end

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_BWG_COORDINATOR_BWG_COORDINATOR_H_
