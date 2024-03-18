// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTOFILL_MANUAL_FILL_EXPANDED_MANUAL_FILL_COORDINATOR_H_
#define IOS_CHROME_BROWSER_UI_AUTOFILL_MANUAL_FILL_EXPANDED_MANUAL_FILL_COORDINATOR_H_

#import <string>

#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"

namespace autofill {
class FormRendererId;
}

namespace manual_fill {
enum class ManualFillDataType;
}

@class ManualFillInjectionHandler;
@class ExpandedManualFillCoordinator;

@protocol PasswordCoordinatorDelegate;

// Delegate for the ExpandedManualFillCoordinator.
@protocol ExpandedManualFillCoordinatorDelegate

// Called when the ExpandedManaualFillCoordinator needs to be stopped.
- (void)stopExpandedManualFillCoordinator:
    (ExpandedManualFillCoordinator*)coordinator;

@end

// The coordinator responsible for presenting the expanded manual fill view.
@interface ExpandedManualFillCoordinator : ChromeCoordinator

// The object in charge of interacting with the web view. Used to fill the data
// in the forms.
@property(nonatomic, weak) ManualFillInjectionHandler* injectionHandler;

// Whether or not the last focused field was obfuscated. Needed to instantiate
// the ManualFillPasswordCoordinator.
@property(nonatomic, assign) BOOL invokedOnObfuscatedField;

// The form ID associated with the field that was last focused. Needed to
// instantiate the ManualFillPasswordCoordinator.
@property(nonatomic, assign) autofill::FormRendererId formID;

// The frame ID associated with the field that was last focused. Needed to
// instantiate the ManualFillPasswordCoordinator.
@property(nonatomic, assign) std::string frameID;

// The delegate to communicate with the FormInputAccessoryCoordinator.
@property(nonatomic, weak)
    id<ExpandedManualFillCoordinatorDelegate, PasswordCoordinatorDelegate>
        delegate;

// Designated initializer. `dataType` represents the type of manual filling
// options to show in the expanded manual fill view.
- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                               forDataType:
                                   (manual_fill::ManualFillDataType)dataType
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser NS_UNAVAILABLE;

// Returns the coordinator's view controller.
- (UIViewController*)viewController;

@end

#endif  // IOS_CHROME_BROWSER_UI_AUTOFILL_MANUAL_FILL_EXPANDED_MANUAL_FILL_COORDINATOR_H_
