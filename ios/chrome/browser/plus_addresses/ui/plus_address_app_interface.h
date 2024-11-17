// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PLUS_ADDRESSES_UI_PLUS_ADDRESS_APP_INTERFACE_H_
#define IOS_CHROME_BROWSER_PLUS_ADDRESSES_UI_PLUS_ADDRESS_APP_INTERFACE_H_

#import <Foundation/Foundation.h>

// PlusAddressAppInterface contains the app-side implementation for helpers.
// These helpers are compiled into the app binary and can be called from either
// app or test code.
@interface PlusAddressAppInterface : NSObject

// Setter to enable plus address creation in `FakePlusAddressService` in tests.
+ (void)setShouldOfferPlusAddressCreation:(BOOL)shouldOfferPlusAddressCreation;

// Setter to return no affiliated plus profiles in call to
// `FakePlusAddressService::GetAffiliatedPlusProfiles`.
+ (void)setShouldReturnNoAffiliatedPlusProfiles:
    (BOOL)shouldReturnNoAffiliatedPlusProfiles;

// Setter to enable plus address filling in `FakePlusAddressService` in tests.
+ (void)setPlusAddressFillingEnabled:(BOOL)plusAddressFillingEnabled;

// Adds a plus address profile in `FakePlusAddressService`.
+ (void)addPlusAddressProfile;

// Setter to show reserve error states in `FakePlusAddressService` in tests.
+ (void)setShouldFailToReserve:(BOOL)shouldFailToReserve;

// Setter to show confirm error states in `FakePlusAddressService` in tests.
+ (void)setShouldFailToConfirm:(BOOL)shouldFailToConfirm;

// Setter to return an affiliated plus profile on confirm.
+ (void)setShouldReturnAffiliatedPlusProfileOnConfirm:
    (BOOL)shouldReturnAffiliatedPlusProfileOnConfirm;

// Setter to return quota error on reserve/confirm.
+ (void)setShouldReturnQuotaError:(BOOL)shouldReturnQuotaError;

// Setter to return timeout error on reserve/confirm.
+ (void)setShouldReturnTimeoutError:(BOOL)shouldReturnTimeoutError;

// Resets the state of `FakePlusAddressService`.
+ (void)clearState;

// Returns the primary email from `FakePlusAddressService`.
+ (NSString*)primaryEmail;

@end

#endif  // IOS_CHROME_BROWSER_PLUS_ADDRESSES_UI_PLUS_ADDRESS_APP_INTERFACE_H_
