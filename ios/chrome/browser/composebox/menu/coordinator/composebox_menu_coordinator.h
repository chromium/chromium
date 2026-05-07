// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_COMPOSEBOX_MENU_COORDINATOR_COMPOSEBOX_MENU_COORDINATOR_H_
#define IOS_CHROME_BROWSER_COMPOSEBOX_MENU_COORDINATOR_COMPOSEBOX_MENU_COORDINATOR_H_

#import "ios/chrome/browser/composebox/public/composebox_entrypoint.h"
#import "ios/chrome/browser/composebox/public/composebox_mode.h"
#import "ios/chrome/browser/composebox/public/composebox_model_option.h"
#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"

@class ComposeboxAttachmentSelection;
@class ComposeboxMenuCoordinator;
@class ComposeboxUIInputState;
@class ComposeboxMetricsRecorder;

// Delegate for events of `ComposeboxMenuCoordinator`.
@protocol ComposeboxMenuCoordinatorDelegate <NSObject>

// Called when the menu presentation finishes.
- (void)composeboxMenuCoordinatorDidDismissMenu:
    (ComposeboxMenuCoordinator*)composeboxMenuCoordinator;

@end

// Delegate for events specific to the input plate.
@protocol ComposeboxMenuCoordinatorInputPlateDelegate <NSObject>

// Called when the user taps a tool in embedded mode.
- (void)composeboxMenuCoordinator:(ComposeboxMenuCoordinator*)coordinator
                       didTapTool:(ComposeboxMode)toolMode;

// Called when the user taps a model option in embedded mode.
- (void)composeboxMenuCoordinator:(ComposeboxMenuCoordinator*)coordinator
                      didTapModel:(ComposeboxModelOption)modelMode;

// Called when the user picks or removes attachments in embedded mode.
- (void)composeboxMenuCoordinator:(ComposeboxMenuCoordinator*)coordinator
             didUpdateAttachments:(ComposeboxAttachmentSelection*)attachments;

@end

// Coordinator for the composebox menu on the New Tab Page.
@interface ComposeboxMenuCoordinator : ChromeCoordinator

// The delegate for this coordinator.
@property(nonatomic, weak) id<ComposeboxMenuCoordinatorDelegate> delegate;

// The input plate delegate for this coordinator.
@property(nonatomic, weak) id<ComposeboxMenuCoordinatorInputPlateDelegate>
    inputPlateDelegate;

// Creates a coordinator for embedded mode with the given preselected
// attachments and initial input state.
- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                    preselectedAttachments:
                        (ComposeboxAttachmentSelection*)preselectedAttachments
                                inputState:(ComposeboxUIInputState*)inputState
                           metricsRecorder:
                               (ComposeboxMetricsRecorder*)metricsRecorder
                                entrypoint:(ComposeboxEntrypoint)entrypoint
    NS_DESIGNATED_INITIALIZER;

// Creates a coordinator for standalone mode with the given entrypoint.
- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                                entrypoint:(ComposeboxEntrypoint)entrypoint;

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser NS_UNAVAILABLE;

- (instancetype)init NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_COMPOSEBOX_MENU_COORDINATOR_COMPOSEBOX_MENU_COORDINATOR_H_
