// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_FORM_INPUT_ACCESSORY_FORM_INPUT_ACCESSORY_MEDIATOR_H_
#define IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_FORM_INPUT_ACCESSORY_FORM_INPUT_ACCESSORY_MEDIATOR_H_

#import <Foundation/Foundation.h>

#import "components/password_manager/core/browser/password_store/password_store_interface.h"
#import "ios/chrome/browser/autofill/model/form_suggestion_client.h"
#import "ios/chrome/browser/autofill/ui_bundled/manual_fill/form_input_interaction_delegate.h"

@protocol FormInputAccessoryConsumer;
@protocol FormInputAccessoryMediatorHandler;
@protocol FormInputSuggestionsProvider;
class PrefService;
@class ReauthenticationModule;
@protocol SecurityAlertCommands;

namespace autofill {
class PersonalDataManager;
}

namespace feature_engagement {
class Tracker;
}

namespace web {
class WebState;
}

class WebStateList;

// This class contains all the logic to get and provide keyboard input accessory
// views to its consumer. As well as telling the consumer when the default
// accessory view should be restored to the system default.
@interface FormInputAccessoryMediator : NSObject <FormSuggestionClient>

// Returns a mediator observing the passed `WebStateList` and associated with
// the passed consumer. `webSateList` can be nullptr and `consumer` can be nil.
- (instancetype)
          initWithConsumer:(id<FormInputAccessoryConsumer>)consumer
                   handler:(id<FormInputAccessoryMediatorHandler>)handler
              webStateList:(WebStateList*)webStateList
       personalDataManager:(autofill::PersonalDataManager*)personalDataManager
      profilePasswordStore:
          (scoped_refptr<password_manager::PasswordStoreInterface>)
              profilePasswordStore
      accountPasswordStore:
          (scoped_refptr<password_manager::PasswordStoreInterface>)
              accountPasswordStore
      securityAlertHandler:(id<SecurityAlertCommands>)securityAlertHandler
    reauthenticationModule:(ReauthenticationModule*)reauthenticationModule
         engagementTracker:(feature_engagement::Tracker*)engagementTracker;

// Unavailable, use initWithConsumer:webStateList: instead.
- (instancetype)init NS_UNAVAILABLE;

@property(nonatomic, readonly, getter=isInputAccessoryViewActive)
    BOOL inputAccessoryViewActive;

// Whether suggestions updates are enabled. The setter updates the consumer.
@property(nonatomic, assign) BOOL suggestionsEnabled;

// Informs via the delegate about form interactions by the user.
@property(nonatomic, weak) id<FormInputInteractionDelegate>
    formInputInteractionDelegate;

// Stops observing all objects.
- (void)disconnect;

// Returns YES if the last focused field is of type 'password'.
- (BOOL)lastFocusedFieldWasObfuscated;

// Returns the main filling product of the current FormInputSuggestionsProvider.
- (autofill::FillingProduct)currentProviderMainFillingProduct;

@end

// Methods to allow injection in tests.
@interface FormInputAccessoryMediator (Tests)

// The WebState this instance is observing. Can be null.
- (void)injectWebState:(web::WebState*)webState;

// Replaces the object in charge of providing suggestions.
- (void)injectProvider:(id<FormInputSuggestionsProvider>)provider;

@end

#endif  // IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_FORM_INPUT_ACCESSORY_FORM_INPUT_ACCESSORY_MEDIATOR_H_
