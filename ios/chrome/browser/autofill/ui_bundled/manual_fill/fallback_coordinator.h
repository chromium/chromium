// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_MANUAL_FILL_FALLBACK_COORDINATOR_H_
#define IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_MANUAL_FILL_FALLBACK_COORDINATOR_H_

#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"

@class LegacyChromeTableViewController;
@class FallbackCoordinator;
@class FallbackViewController;
@class ManualFillInjectionHandler;

// Delegate for the coordinator actions.
@protocol FallbackCoordinatorDelegate<NSObject>

// Called when the user has taken action to dismiss a popover.
- (void)fallbackCoordinatorDidDismissPopover:
    (FallbackCoordinator*)fallbackCoordinator;

@end

// Creates and manages a view controller to present some fallbacks (passwords,
// cards or addresses) to the user. Any selected fallback item will be sent to
// the current field in the active web state.
@interface FallbackCoordinator : ChromeCoordinator

// The view controller of this coordinator.
@property(nonatomic, readonly) FallbackViewController* viewController;

// The delegate for this coordinator.
@property(nonatomic, weak) id<FallbackCoordinatorDelegate> delegate;

// The object in charge of interacting with the web view. Used to fill the data
// in the forms.
@property(nonatomic, strong) ManualFillInjectionHandler* injectionHandler;

// Creates a coordinator that uses a `viewController`, `browser` and an
// `injectionHandler`.
- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                          injectionHandler:
                              (ManualFillInjectionHandler*)injectionHandler
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser NS_UNAVAILABLE;

// Presents the view controller as a popover from the passed button.
- (void)presentFromButton:(UIButton*)button;

// Dismisses the view controller, if needed, and according to the platform. It
// then calls the completer either way. Returns true if dismissing was
// necessary.
- (BOOL)dismissIfNecessaryThenDoCompletion:(void (^)(void))completion;

@end

#endif  // IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_MANUAL_FILL_FALLBACK_COORDINATOR_H_
