// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTOFILL_MANUAL_FILL_CARD_COORDINATOR_H_
#define IOS_CHROME_BROWSER_UI_AUTOFILL_MANUAL_FILL_CARD_COORDINATOR_H_

#import "ios/chrome/browser/ui/autofill/manual_fill/fallback_coordinator.h"

@protocol BrowserCoordinatorCommands;
class WebStateList;

// Delegate for the coordinator actions.
// TODO(crbug.com/845472): revise delegate method names.
@protocol CardCoordinatorDelegate<FallbackCoordinatorDelegate>

// Opens the cards settings.
- (void)openCardSettings;

@end

// Creates and manages a view controller to present cards to the user.
// Any selected card will be sent to the current field in the active web
// state.
@interface CardCoordinator : FallbackCoordinator

// The delegate for this coordinator. Delegate class extends
// FallbackCoordinatorDelegate, and replaces super class delegate.
@property(nonatomic, weak) id<CardCoordinatorDelegate> delegate;

// Creates a coordinator that uses a |viewController|, |browserState|,
// |webStateList| and |injectionHandler|.
- (instancetype)
    initWithBaseViewController:(UIViewController*)viewController
                  browserState:(ios::ChromeBrowserState*)browserState
                  webStateList:(WebStateList*)webStateList
              injectionHandler:(ManualFillInjectionHandler*)injectionHandler
                    dispatcher:(id<BrowserCoordinatorCommands>)dispatcher
    NS_DESIGNATED_INITIALIZER;

- (instancetype)
initWithBaseViewController:(UIViewController*)viewController
              browserState:(ios::ChromeBrowserState*)browserState
          injectionHandler:(ManualFillInjectionHandler*)injectionHandler
    NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_UI_AUTOFILL_MANUAL_FILL_CARD_COORDINATOR_H_
