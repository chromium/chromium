// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_MANUAL_FILL_MANUAL_FILL_CARD_MEDIATOR_H_
#define IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_MANUAL_FILL_MANUAL_FILL_CARD_MEDIATOR_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/autofill/ui_bundled/manual_fill/full_card_request_result_delegate_bridge.h"

@protocol BrowserCoordinatorCommands;
namespace autofill {
class CreditCard;
class PersonalDataManager;
}  // namespace autofill

@protocol ManualFillContentInjector;
@protocol ManualFillCardConsumer;
@protocol CardListDelegate;
@class ReauthenticationModule;

// Object in charge of getting the cards relevant for the manual fill
// cards UI.
@interface ManualFillCardMediator
    : NSObject <FullCardRequestResultDelegateObserving>

// The consumer for cards updates. Setting it will trigger the consumer
// methods with the current data.
@property(nonatomic, weak) id<ManualFillCardConsumer> consumer;

// The delegate in charge of using the content selected by the user.
@property(nonatomic, weak) id<ManualFillContentInjector> contentInjector;

// The delegate in charge of navigation.
@property(nonatomic, weak) id<CardListDelegate> navigationDelegate;

// The designated initializer. `personalDataManager` must not be nil.
- (instancetype)initWithPersonalDataManager:
                    (autofill::PersonalDataManager*)personalDataManager
                     reauthenticationModule:
                         (ReauthenticationModule*)reauthenticationModule
                     showAutofillFormButton:(BOOL)showAutofillFormButton
    NS_DESIGNATED_INITIALIZER;

// Unavailable. Use `initWithCards:`.
- (instancetype)init NS_UNAVAILABLE;

// Finds the original autofill::CreditCard from given `GUID`. Returns an
// optional in case `GUID` can't be mapped to a card.
- (std::optional<const autofill::CreditCard>)findCreditCardfromGUID:
    (NSString*)GUID;

// Disconnects the mediator.
- (void)disconnect;

@end

#endif  // IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_MANUAL_FILL_MANUAL_FILL_CARD_MEDIATOR_H_
