// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTOFILL_MANUAL_FILL_MANUAL_FILL_CARD_MEDIATOR_H_
#define IOS_CHROME_BROWSER_UI_AUTOFILL_MANUAL_FILL_MANUAL_FILL_CARD_MEDIATOR_H_

#import <UIKit/UIKit.h>

#include "base/memory/ref_counted.h"
#import "ios/chrome/browser/ui/autofill/manual_fill/full_card_request_result_delegate_bridge.h"

@protocol BrowserCoordinatorCommands;
namespace autofill {
class CreditCard;
}  // namespace autofill

@protocol ManualFillContentInjector;
@protocol ManualFillCardConsumer;
@protocol CardListDelegate;

namespace manual_fill {
extern NSString* const ManageCardsAccessibilityIdentifier;
extern NSString* const kAddCreditCardsAccessibilityIdentifier;
}  // namespace manual_fill

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

// The designated initializer. |cards| must not be nil.
- (instancetype)initWithCards:(std::vector<autofill::CreditCard*>)cards
                      handler:(id<BrowserCoordinatorCommands>)handler
    NS_DESIGNATED_INITIALIZER;

// Unavailable. Use |initWithCards:|.
- (instancetype)init NS_UNAVAILABLE;

// Finds the original autofill::CreditCard from given |GUID|.
- (const autofill::CreditCard*)findCreditCardfromGUID:(NSString*)GUID;

// Updates the |cards| being presented.
- (void)reloadWithCards:(std::vector<autofill::CreditCard*>)cards;

@end

#endif  // IOS_CHROME_BROWSER_UI_AUTOFILL_MANUAL_FILL_MANUAL_FILL_CARD_MEDIATOR_H_
