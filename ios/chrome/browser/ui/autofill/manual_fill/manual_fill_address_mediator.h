// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTOFILL_MANUAL_FILL_MANUAL_FILL_ADDRESS_MEDIATOR_H_
#define IOS_CHROME_BROWSER_UI_AUTOFILL_MANUAL_FILL_MANUAL_FILL_ADDRESS_MEDIATOR_H_

#import <UIKit/UIKit.h>
#include <memory>

namespace autofill {
class AutofillProfile;
}  // namespace autofill

@protocol ManualFillContentInjector;
@protocol ManualFillAddressConsumer;
@protocol AddressListDelegate;

namespace manual_fill {
extern NSString* const ManageAddressAccessibilityIdentifier;
}  // namespace manual_fill

// Object in charge of getting the addresses relevant for the manual fill UI.
@interface ManualFillAddressMediator : NSObject

// The consumer for addresses updates. Setting it will trigger the consumer
// methods with the current data.
@property(nonatomic, weak) id<ManualFillAddressConsumer> consumer;

// The delegate in charge of using the content selected by the user.
@property(nonatomic, weak) id<ManualFillContentInjector> contentInjector;

// The delegate in charge of navigation.
@property(nonatomic, weak) id<AddressListDelegate> navigationDelegate;

// The designated initializer.
- (instancetype)initWithProfiles:
    (std::vector<autofill::AutofillProfile*>)profiles NS_DESIGNATED_INITIALIZER;

// Unavailable. Use `initWithProfiles:`.
- (instancetype)init NS_UNAVAILABLE;

// Updates the `profiles` being presented.
- (void)reloadWithProfiles:(std::vector<autofill::AutofillProfile*>)profiles;

@end

#endif  // IOS_CHROME_BROWSER_UI_AUTOFILL_MANUAL_FILL_MANUAL_FILL_ADDRESS_MEDIATOR_H_
