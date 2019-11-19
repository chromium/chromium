// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTOFILL_FORM_INPUT_ACCESSORY_FORM_INPUT_ACCESSORY_COORDINATOR_H_
#define IOS_CHROME_BROWSER_UI_AUTOFILL_FORM_INPUT_ACCESSORY_FORM_INPUT_ACCESSORY_COORDINATOR_H_

#import "ios/chrome/browser/ui/coordinators/chrome_coordinator.h"

#import "ios/web/public/web_state_observer_bridge.h"

@protocol BrowserCoordinatorCommands;
@class ManualFillInjectionHandler;
class WebStateList;

// Delegate for the coordinator actions.
@protocol FormInputAccessoryCoordinatorNavigator <NSObject>

// Opens the passwords settings.
- (void)openPasswordSettings;

// Opens the addresses settings.
- (void)openAddressSettings;

// Opens the credit cards settings.
- (void)openCreditCardSettings;

// Opens the all passwords picker, used for manual fallback.
- (void)openAllPasswordsPicker;

@end

// Creates and manages a custom input accessory view while the user is
// interacting with a form. Also handles hiding and showing the default
// accessory view elements.
@interface FormInputAccessoryCoordinator : ChromeCoordinator

// The delegate for the coordinator. Must be set before it starts.
@property(nonatomic, weak) id<FormInputAccessoryCoordinatorNavigator> navigator;

// Creates a coordinator that uses a |viewController| a |browserState| and
// a |webStateList|.
- (instancetype)
    initWithBaseViewController:(UIViewController*)viewController
                  browserState:(ios::ChromeBrowserState*)browserState
                  webStateList:(WebStateList*)webStateList
              injectionHandler:(ManualFillInjectionHandler*)injectionHandler
                    dispatcher:(id<BrowserCoordinatorCommands>)dispatcher
    NS_DESIGNATED_INITIALIZER;

// Unavailable, use -initWithBaseViewController:browserState:webStateList:.
- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                              browserState:
                                  (ios::ChromeBrowserState*)browserState
    NS_UNAVAILABLE;

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser NS_UNAVAILABLE;

// This resets the input accessory to a clean state.
- (void)reset;

@end

#endif  // IOS_CHROME_BROWSER_UI_AUTOFILL_FORM_INPUT_ACCESSORY_FORM_INPUT_ACCESSORY_COORDINATOR_H_
