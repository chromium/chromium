// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTOFILL_FORM_INPUT_ACCESSORY_FORM_INPUT_ACCESSORY_MEDIATOR_H_
#define IOS_CHROME_BROWSER_UI_AUTOFILL_FORM_INPUT_ACCESSORY_FORM_INPUT_ACCESSORY_MEDIATOR_H_

#import <Foundation/Foundation.h>

#import "components/password_manager/core/browser/password_store_interface.h"
#import "ios/chrome/browser/autofill/form_input_navigator.h"
#import "ios/chrome/browser/autofill/form_suggestion_client.h"
#import "ios/chrome/browser/ui/autofill/form_input_accessory/branding_view_controller_delegate.h"
#import "ios/chrome/browser/web_state_list/web_state_list_observer_bridge.h"
#import "ios/web/public/web_state_observer_bridge.h"

@class ChromeCoordinator;
@protocol FormInputAccessoryConsumer;
@protocol FormInputSuggestionsProvider;
@class ReauthenticationModule;
@protocol SecurityAlertCommands;

namespace autofill {
class PersonalDataManager;
}

namespace web {
class WebState;
}

class WebStateList;

// Handler in charge of accessory mediator events.
@protocol FormInputAccessoryMediatorHandler

// The mediator detected that the keyboard input view should be reset.
- (void)resetFormInputView;

// The mediator notifies that password suggestions have been shown.
- (void)notifyPasswordSuggestionsShown;

// The mediator notifies that a password suggestion has been selected.
- (void)notifyPasswordSuggestionSelected;

// The mediator shows password suggestion tip if needed.
- (void)showPasswordSuggestionIPHIfNeeded;

@end

// This class contains all the logic to get and provide keyboard input accessory
// views to its consumer. As well as telling the consumer when the default
// accessory view should be restored to the system default.
@interface FormInputAccessoryMediator
    : NSObject <BrandingViewControllerDelegate, FormSuggestionClient>

// Returns a mediator observing the passed `WebStateList` and associated with
// the passed consumer. `webSateList` can be nullptr and `consumer` can be nil.
- (instancetype)
          initWithConsumer:(id<FormInputAccessoryConsumer>)consumer
                   handler:(id<FormInputAccessoryMediatorHandler>)handler
              webStateList:(WebStateList*)webStateList
       personalDataManager:(autofill::PersonalDataManager*)personalDataManager
             passwordStore:
                 (scoped_refptr<password_manager::PasswordStoreInterface>)
                     passwordStore
      securityAlertHandler:(id<SecurityAlertCommands>)securityAlertHandler
    reauthenticationModule:(ReauthenticationModule*)reauthenticationModule;

// Unavailable, use initWithConsumer:webStateList: instead.
- (instancetype)init NS_UNAVAILABLE;

@property(nonatomic, readonly, getter=isInputAccessoryViewActive)
    BOOL inputAccessoryViewActive;

// Disables suggestions updates and asks the consumer to remove the current
// ones.
- (void)disableSuggestions;

// Enables suggestions updates and sends the current ones, if any, to the
// consumer.
- (void)enableSuggestions;

// Stops observing all objects.
- (void)disconnect;

// Returns YES if the last focused field is of type 'password'.
- (BOOL)lastFocusedFieldWasPassword;

@end

// Methods to allow injection in tests.
@interface FormInputAccessoryMediator (Tests)

// The WebState this instance is observing. Can be null.
- (void)injectWebState:(web::WebState*)webState;

// Replaces the object in charge of providing suggestions.
- (void)injectProvider:(id<FormInputSuggestionsProvider>)provider;

@end

#endif  // IOS_CHROME_BROWSER_UI_AUTOFILL_FORM_INPUT_ACCESSORY_FORM_INPUT_ACCESSORY_MEDIATOR_H_
