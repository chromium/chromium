// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_FORM_INPUT_ACCESSORY_COORDINATOR_FORM_INPUT_ACCESSORY_COORDINATOR_H_
#define IOS_CHROME_BROWSER_AUTOFILL_FORM_INPUT_ACCESSORY_COORDINATOR_FORM_INPUT_ACCESSORY_COORDINATOR_H_

#import "ios/chrome/browser/autofill/public/autofill_settings_navigator.h"
#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"
#import "ios/web/common/crw_input_view_provider.h"

@class ManualFillInjectionHandler;

// Creates and manages a custom input accessory view while the user is
// interacting with a form. Also handles hiding and showing the default
// accessory view elements.
@interface FormInputAccessoryCoordinator
    : ChromeCoordinator <CRWResponderInputView>

// The delegate for the coordinator. Must be set before it starts.
@property(nonatomic, weak) id<AutofillSettingsNavigator> navigator;

// Stops child coordinators presenting UI.
- (void)clearPresentedState;

// Resets the autofill suggestions loading states.
- (void)resetLoadingStates;

@end

#endif  // IOS_CHROME_BROWSER_AUTOFILL_FORM_INPUT_ACCESSORY_COORDINATOR_FORM_INPUT_ACCESSORY_COORDINATOR_H_
