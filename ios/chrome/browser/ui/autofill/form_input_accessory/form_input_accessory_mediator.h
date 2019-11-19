// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTOFILL_FORM_INPUT_ACCESSORY_FORM_INPUT_ACCESSORY_MEDIATOR_H_
#define IOS_CHROME_BROWSER_UI_AUTOFILL_FORM_INPUT_ACCESSORY_FORM_INPUT_ACCESSORY_MEDIATOR_H_

#import <Foundation/Foundation.h>

#include "components/password_manager/core/browser/password_store.h"
#import "ios/chrome/browser/autofill/form_input_navigator.h"
#import "ios/chrome/browser/web_state_list/web_state_list_observer_bridge.h"
#import "ios/web/public/web_state_observer_bridge.h"

@class ChromeCoordinator;
@protocol FormInputAccessoryConsumer;
@class FormInputAccessoryMediator;
@protocol FormInputSuggestionsProvider;
@class JsSuggestionManager;

namespace autofill {
class PersonalDataManager;
}

namespace web {
class WebState;
}

class WebStateList;

// Delegate in charge of reacting to accessory mediator events.
@protocol FormInputAccessoryMediatorDelegate

// The mediator detected that the keyboard was hidden and it is no longer
// present on the screen.
- (void)mediatorDidDetectKeyboardHide:(FormInputAccessoryMediator*)mediator;

// The mediator detected that the keyboard was hidden and it is no longer
// present on the screen.
- (void)mediatorDidDetectMovingToBackground:
    (FormInputAccessoryMediator*)mediator;

@end

// This class contains all the logic to get and provide keyboard input accessory
// views to its consumer. As well as telling the consumer when the default
// accessory view shoeuld be restored to the system default.
@interface FormInputAccessoryMediator : NSObject

// Returns a mediator observing the passed `WebStateList` and associated with
// the passed consumer. `webSateList` can be nullptr and `consumer` can be nil.
- (instancetype)
       initWithConsumer:(id<FormInputAccessoryConsumer>)consumer
               delegate:(id<FormInputAccessoryMediatorDelegate>)delegate
           webStateList:(WebStateList*)webStateList
    personalDataManager:(autofill::PersonalDataManager*)personalDataManager
          passwordStore:
              (scoped_refptr<password_manager::PasswordStore>)passwordStore;

// Unavailable, use initWithConsumer:webStateList: instead.
- (instancetype)init NS_UNAVAILABLE;

// Disables suggestions updates and asks the consumer to remove the current
// ones.
- (void)disableSuggestions;

// Enables suggestions updates and sends the current ones, if any, to the
// consumer.
- (void)enableSuggestions;

// Stops observing all objects.
- (void)disconnect;

@end

// Methods to allow injection in tests.
@interface FormInputAccessoryMediator (Tests)

// The WebState this instance is observing. Can be null.
- (void)injectWebState:(web::WebState*)webState;

// The JS manager for interacting with the underlying form.
- (void)injectSuggestionManager:(JsSuggestionManager*)JSSuggestionManager;

// Replaces the object in charge of providing suggestions.
- (void)injectProvider:(id<FormInputSuggestionsProvider>)provider;

@end

#endif  // IOS_CHROME_BROWSER_UI_AUTOFILL_FORM_INPUT_ACCESSORY_FORM_INPUT_ACCESSORY_MEDIATOR_H_
