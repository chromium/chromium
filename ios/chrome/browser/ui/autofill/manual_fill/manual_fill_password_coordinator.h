// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTOFILL_MANUAL_FILL_MANUAL_FILL_PASSWORD_COORDINATOR_H_
#define IOS_CHROME_BROWSER_UI_AUTOFILL_MANUAL_FILL_MANUAL_FILL_PASSWORD_COORDINATOR_H_

#import "ios/chrome/browser/ui/autofill/manual_fill/fallback_coordinator.h"

class GURL;

// Delegate for the coordinator actions.
@protocol PasswordCoordinatorDelegate <FallbackCoordinatorDelegate>

// Opens the passwords settings.
- (void)openPasswordSettings;

// Opens the all passwords picker, used for manual fallback.
- (void)openAllPasswordsPicker;

@end

// Creates and manages a view controller to present passwords to the user. It
// will filter the passwords based on the passed URL when instantiating it. Any
// selected password will be sent to the current field in the active web state.
@interface ManualFillPasswordCoordinator : FallbackCoordinator

// The delegate for this coordinator. Delegate protocol conforms to
// FallbackCoordinatorDelegate, and replaces the superclass delegate.
@property(nonatomic, weak) id<PasswordCoordinatorDelegate> delegate;

// Creates a coordinator that uses a |viewController|, |browser|,
// |URL| and an |injectionHandler|.
- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                                       URL:(const GURL&)URL
                          injectionHandler:
                              (ManualFillInjectionHandler*)injectionHandler
    NS_DESIGNATED_INITIALIZER;

// Unavailable, use
// -initWithBaseViewController:browser:URL:injectionHandler:.
- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                          injectionHandler:
                              (ManualFillInjectionHandler*)injectionHandler
    NS_UNAVAILABLE;

// Presents the password view controller as a popover from the passed button.
- (void)presentFromButton:(UIButton*)button;

@end

#endif  // IOS_CHROME_BROWSER_UI_AUTOFILL_MANUAL_FILL_MANUAL_FILL_PASSWORD_COORDINATOR_H_
