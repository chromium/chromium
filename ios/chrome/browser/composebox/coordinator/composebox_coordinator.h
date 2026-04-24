// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_COMPOSEBOX_COORDINATOR_COMPOSEBOX_COORDINATOR_H_
#define IOS_CHROME_BROWSER_COMPOSEBOX_COORDINATOR_COMPOSEBOX_COORDINATOR_H_

#import "base/ios/block_types.h"
#import "ios/chrome/browser/omnibox/model/omnibox_position/omnibox_state_provider.h"
#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"

@protocol ComposeboxAnimationBase;
@class ComposeboxFocusParams;

// Coordinator that contains the composebox, presenting it modally.
@interface ComposeboxCoordinator : ChromeCoordinator <OmniboxStateProvider>

/// Whether the composebox is presented.
@property(nonatomic, assign, getter=isPresented, readonly) BOOL presented;

/// Initializes the coordinator with the `baseViewController`, `browser`,
/// and `focusParams`.
- (instancetype)initWithBaseViewController:(UIViewController*)baseViewController
                                   browser:(Browser*)browser
                               focusParams:(ComposeboxFocusParams*)focusParams
                   composeboxAnimationBase:
                       (id<ComposeboxAnimationBase>)composeboxAnimationBase
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser NS_UNAVAILABLE;

/// Gracefully dismisses the coordinator before completing the cleanup.
- (void)stopAnimatedWithCompletion:(ProceduralBlock)completion;

@end

#endif  // IOS_CHROME_BROWSER_COMPOSEBOX_COORDINATOR_COMPOSEBOX_COORDINATOR_H_
