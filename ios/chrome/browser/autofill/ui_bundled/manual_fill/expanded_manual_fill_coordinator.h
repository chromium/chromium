// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_MANUAL_FILL_EXPANDED_MANUAL_FILL_COORDINATOR_H_
#define IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_MANUAL_FILL_EXPANDED_MANUAL_FILL_COORDINATOR_H_

#import "ios/chrome/browser/autofill/ui_bundled/manual_fill/form_input_interaction_delegate.h"
#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"

namespace manual_fill {
enum class ManualFillDataType;
}

@class ManualFillInjectionHandler;
@class ExpandedManualFillCoordinator;
@class ReauthenticationModule;

@protocol AddressCoordinatorDelegate;
@protocol CardCoordinatorDelegate;
@protocol PasswordCoordinatorDelegate;

// Delegate for the ExpandedManualFillCoordinator.
@protocol ExpandedManualFillCoordinatorDelegate

// Called when the ExpandedManaualFillCoordinator needs to be stopped.
- (void)stopExpandedManualFillCoordinator:
    (ExpandedManualFillCoordinator*)coordinator;

@end

// The coordinator responsible for presenting the expanded manual fill view.
@interface ExpandedManualFillCoordinator
    : ChromeCoordinator <FormInputInteractionDelegate>

// The object in charge of interacting with the web view. Used to fill the data
// in the forms.
@property(nonatomic, weak) ManualFillInjectionHandler* injectionHandler;

// Whether or not the last focused field was obfuscated. Needed to instantiate
// the ManualFillPasswordCoordinator.
@property(nonatomic, assign) BOOL invokedOnObfuscatedField;

// The delegate to communicate with the FormInputAccessoryCoordinator.
@property(nonatomic, weak) id<ExpandedManualFillCoordinatorDelegate,
                              AddressCoordinatorDelegate,
                              CardCoordinatorDelegate,
                              PasswordCoordinatorDelegate>
    delegate;

// Designated initializer. `dataType` represents the type of manual filling
// options to show in the expanded manual fill view. `focusedFieldDataType`
// represents the manual fill data type associated with the currently focused
// field. `dataType` and `focusedFieldDataType` can differ when the type of
// manual filling options to show was selected by the user (by tapping the
// password, card or address icon in the keyboard accessory).
- (instancetype)
    initWithBaseViewController:(UIViewController*)viewController
                       browser:(Browser*)browser
                   forDataType:(manual_fill::ManualFillDataType)dataType
          focusedFieldDataType:
              (manual_fill::ManualFillDataType)focusedFieldDataType
        reauthenticationModule:(ReauthenticationModule*)reauthenticationModule
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser NS_UNAVAILABLE;

// Returns the coordinator's view controller.
- (UIViewController*)viewController;

@end

#endif  // IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_MANUAL_FILL_EXPANDED_MANUAL_FILL_COORDINATOR_H_
