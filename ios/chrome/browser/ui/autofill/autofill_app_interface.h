// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_APP_INTERFACE_H_
#define IOS_CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_APP_INTERFACE_H_

#import <Foundation/Foundation.h>

// AutofillAppInterface contains the app-side
// implementation for helpers. These helpers are compiled into
// the app binary and can be called from either app or test code.
@interface AutofillAppInterface : NSObject

// Removes all credentials stored.
+ (void)clearPasswordStore;

// Saves an example form in the store.
+ (void)saveExamplePasswordForm;

// Saves an example form in the store for the passed URL spec.
+ (void)savePasswordFormForURLSpec:(NSString*)URLSpec;

// Returns the number of profiles (addresses) in the data manager.
+ (NSInteger)profilesCount;

// Clears the profiles (addresses) in the data manager.
+ (void)clearProfilesStore;

// Saves a sample profile (address) in the data manager.
+ (void)saveExampleProfile;

// Returns the name of the sample profile.
+ (NSString*)exampleProfileName;

// Removes the stored credit cards.
+ (void)clearCreditCardStore;

// Saves a local credit card that doesn't require CVC to be used.
// Returns the |card.NetworkAndLastFourDigits| of the card used in the UIs.
+ (NSString*)saveLocalCreditCard;

// Saves a masked credit card that requires CVC to be used.
+ (void)saveMaskedCreditCard;

@end

#endif  // IOS_CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_APP_INTERFACE_H_
