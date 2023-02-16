// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_AUTOFILL_AUTOFILL_PROFILE_EDIT_MEDIATOR_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_AUTOFILL_AUTOFILL_PROFILE_EDIT_MEDIATOR_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/ui/settings/autofill/autofill_profile_edit_table_view_controller_delegate.h"

namespace autofill {
class PersonalDataManager;
}

@protocol AutofillProfileEditConsumer;
@protocol AutofillProfileEditMediatorDelegate;
@class CountryItem;

// The Mediator for viewing and editing the profile.
@interface AutofillProfileEditMediator
    : NSObject <AutofillProfileEditTableViewControllerDelegate>

// Designated initializer. `AutofillProfileEditMediatorDelegate` and
// `dataManager` should not be nil.
- (instancetype)initWithDelegate:
                    (id<AutofillProfileEditMediatorDelegate>)delegate
             personalDataManager:(autofill::PersonalDataManager*)dataManager
                     countryCode:(NSString*)countryCode
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// Consumer of this mediator.
@property(nonatomic, weak) id<AutofillProfileEditConsumer> consumer;

// Called when the country is selected from a dropdown.
- (void)didSelectCountry:(CountryItem*)countryItem;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_AUTOFILL_AUTOFILL_PROFILE_EDIT_MEDIATOR_H_
