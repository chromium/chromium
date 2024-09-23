// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_FORM_INPUT_ACCESSORY_FORM_INPUT_ACCESSORY_COORDINATOR_H_
#define IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_FORM_INPUT_ACCESSORY_FORM_INPUT_ACCESSORY_COORDINATOR_H_

#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"
#import "ios/web/common/crw_input_view_provider.h"

@class ManualFillInjectionHandler;
@class ReauthenticationModule;

// Delegate for the coordinator actions.
@protocol FormInputAccessoryCoordinatorNavigator <NSObject>

// Opens the Password Manager screen.
- (void)openPasswordManager;

// Opens the Password Settings screen.
- (void)openPasswordSettings;

// Opens the addresses settings.
- (void)openAddressSettings;

// Opens the credit cards settings.
- (void)openCreditCardSettings;

@end

// Creates and manages a custom input accessory view while the user is
// interacting with a form. Also handles hiding and showing the default
// accessory view elements.
@interface FormInputAccessoryCoordinator
    : ChromeCoordinator <CRWResponderInputView>

// The delegate for the coordinator. Must be set before it starts.
@property(nonatomic, weak) id<FormInputAccessoryCoordinatorNavigator> navigator;

// Stops child coordinators presenting UI.
- (void)clearPresentedState;

@end

#endif  // IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_FORM_INPUT_ACCESSORY_FORM_INPUT_ACCESSORY_COORDINATOR_H_
